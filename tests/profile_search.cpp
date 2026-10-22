#include "../src/profile_search.hpp"
#include "../src/optimize.hpp"
#include <cassert>
namespace rx {
std::atomic<bool> interrupted{false};
}
using namespace rx;

int main() {
    std::mt19937 rng(6718331);
    int tested = 0, offers = 0, prepared = 0;
    for (int mask : {3, 42, 341, 1023}) {
        std::vector<int> digits;
        for (int c = 0; c < 10; ++c)
            if (mask & (1 << c))
                digits.push_back(c);
        for (int trial = 0; trial < 8; ++trial) {
            std::vector<R> terms;
            for (int j = 0; j < 2; ++j) {
                int prefixMask = 0;
                for (int c : digits)
                    if (rng() % 2)
                        prefixMask |= 1 << c;
                if (!prefixMask)
                    prefixMask = 1 << digits[rng() % digits.size()];
                terms.push_back(cat({chars(prefixMask), rep(chars(mask), rng() % 4, INF)}));
            }
            for (int j = 0; j < 5; ++j) {
                std::vector<R> word;
                for (int k = int(rng() % 4); k > 0; --k)
                    word.push_back(chars(1 << digits[rng() % digits.size()]));
                terms.push_back(cat(word));
            }
            R original = alt(terms), best = original;
            Budget preparation(10);
            DFA target = compileRaw(Parser(original->s, false).parse(), {}, &preparation);
            DFA minimal = minimize(target, &preparation);
            Budget phase(.06);
            ProfileSearch search(
                minimal, phase, {},
                [&](R candidate, const std::string &) {
                    assert(
                        compare(target,
                                compileRaw(Parser(candidate->s, false).parse(), {}, &preparation),
                                {}, &preparation)
                            .equal);
                    assert(candidate->s.size() <= original->s.size());
                    ++offers;
                    if (better(candidate, best))
                        best = candidate;
                },
                [](R r) { return structural(r); });
            try {
                search.run(original);
            } catch (const Limit &) {
                assert(phase.done());
            }
            prepared += !search.ranks.empty();
            assert(compare(target, compileRaw(Parser(best->s, false).parse()), {}, &preparation)
                       .equal);
            ++tested;
        }
    }
    // Languages without any universally accepting residual do not admit a
    // nonempty ideal tail. Rejecting that hypothesis must preserve the input.
    for (const std::string expression : {"(01)*", "0*1*", "[23]{3}", "0{0}"}) {
        R original = Parser(expression, false).parse();
        DFA target = compile(original);
        Budget phase(1);
        ProfileSearch search(
            target, phase, {}, [](R, const std::string &) { std::abort(); },
            [](R r) { return structural(r); });
        search.run(original);
        ++tested;
    }
    assert(prepared == 32);
    // Verify that the independently constructed product difference is exact
    // on all short words, with digits outside either generating alphabet.
    Budget differenceBudget(10);
    R a = Parser("[01]*|22", false).parse(), b = Parser("0[01]*|22", false).parse();
    DFA left = compile(a), right = compile(b);
    ProfileSearch difference(
        left, differenceBudget, {}, [](R, const std::string &) {}, [](R r) { return r; });
    DFA d = difference.difference(left, right);
    std::vector<std::string> words{std::string()};
    for (int depth = 0; depth <= 6; ++depth) {
        std::vector<std::string> next;
        for (const auto &word : words) {
            assert(matches(d, word) == (matches(left, word) && !matches(right, word)));
            for (char c : std::string("0123"))
                next.push_back(word + c);
        }
        words = std::move(next);
    }
    std::cout << "PASS " << tested << " continuation-profile cases; " << offers
              << " exact freshly reparsed offers; product difference checked independently.\n";
}
