#include "../src/unary_evolution.hpp"
#include "../src/optimize.hpp"
#include <cassert>
namespace rx {
std::atomic<bool> interrupted{false};
}
using namespace rx;
static UnaryEvolution::Value oracle(const DFA &dfa) {
    UnaryEvolution::Value result;
    std::vector<int> seen(dfa.t.size(), -1), path;
    int p = dfa.start;
    while (seen[p] < 0) {
        seen[p] = int(path.size());
        int length = int(path.size());
        path.push_back(p);
        if (dfa.accept[p]) {
            if (length < 64)
                result.lo |= 1ULL << length;
            else if (length < 128)
                result.hi |= 1ULL << (length - 64);
            else
                result.overflow = true;
        }
        p = dfa.t[p][0];
    }
    for (size_t i = size_t(seen[p]); i < path.size(); ++i)
        assert(!dfa.accept[path[i]]); // All generated test languages are finite.
    return result;
}
int main() {
    Budget budget(30);
    DFA empty = compile(eps());
    UnaryEvolution search(
        empty, budget, {}, [](R r) { return structural(r); }, [](R, const std::string &) {});
    search.digit = 0;
    std::mt19937 rng(972331);
    int checks = 0;
    std::function<R(int)> generate;
    // Use exact powers at leaves, including long atoms which cross the
    // 64-bit and 128-bit boundaries after concatenation/repetition.
    generate = [&](int depth) -> R {
        if (!depth || rng() % 4 == 0) {
            int length = int(rng() % 19);
            return rawRep(chars(1), length, length);
        }
        int operation = int(rng() % 3);
        if (!operation)
            return cat({generate(depth - 1), generate(depth - 1)});
        if (operation == 1)
            return alt({generate(depth - 1), generate(depth - 1)});
        int lo = int(rng() % 3), hi = lo + int(rng() % 3);
        return rawRep(generate(depth - 1), lo, hi);
    };
    for (int boundary : {63, 64, 65, 127, 128, 129, 255}) {
        R r = rawRep(chars(1), boundary, boundary);
        assert(search.evaluate(r) == oracle(compileRaw(Parser(r->s, false).parse())));
        ++checks;
    }
    for (int trial = 0; trial < 160; ++trial) {
        R r = generate(2);
        assert(search.evaluate(r) == oracle(compileRaw(Parser(r->s, false).parse())));
        ++checks;
    }
    for (int trial = 0; trial < 8; ++trial) {
        std::set<int> points;
        for (int j = 0; j < 8; ++j)
            points.insert(int(rng() % 30));
        std::vector<R> words;
        for (int point : points)
            words.push_back(rawRep(chars(1), point, point));
        R seed = alt(words);
        DFA target = compile(seed);
        UnaryEvolution evolution(
            target, budget, {}, [](R r) { return structural(r); },
            [&](R r, const std::string &) {
                assert(compare(target, compileRaw(Parser(r->s, false).parse())).equal);
                ++checks;
            },
            uint64_t(trial + 1));
        evolution.run(seed, .04);
        assert(evolution.best &&
               compare(target, compileRaw(Parser(evolution.best->s, false).parse())).equal);
        ++checks;
    }
    std::cout << "OK: " << checks << " independent finite-polynomial/evolution checks.\n";
}
