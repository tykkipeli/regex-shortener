// Search-engine audit using unrelated languages, arbitrary automata, alphabet
// permutations and reversal. No task identifiers or record answers occur here.
#include "../src/finite.hpp"
#include "../src/optimize.hpp"
#include <cassert>
namespace rx {
std::atomic<bool> interrupted{false};
}
using namespace rx;

static R unionOfWords(const std::vector<std::string> &words) {
    std::vector<R> choices;
    for (const auto &word : words) {
        std::vector<R> symbols;
        for (char c : word)
            symbols.push_back(chars(1 << (c - '0')));
        choices.push_back(cat(symbols));
    }
    return alt(choices);
}
int main() {
    for (int n = 0; n <= 128; ++n) {
        FiniteMask mask = FiniteMask::full(n);
        assert(mask.count() == n && (~mask).count() == 128 - n);
        int next = 0;
        for (; mask; mask.clearFirst())
            assert(mask.first() == next++);
        assert(next == n);
    }
    for (int bit = 0; bit < 128; ++bit) {
        FiniteMask mask = FiniteMask::bit(bit);
        assert(mask.count() == 1 && mask.first() == bit);
    }
    std::mt19937 rng(1901347);
    int languages = 0, offers = 0, partialOffers = 0, contexts = 0;
    for (int trial = 0; trial < 30; ++trial) {
        std::set<std::string> original;
        for (int j = 0, count = 2 + int(rng() % 15); j < count; ++j) {
            std::string word;
            for (int i = 0, length = int(rng() % 5); i < length; ++i)
                word += char('0' + rng() % 5);
            original.insert(word);
        }
        std::array<int, 10> permutation;
        std::iota(permutation.begin(), permutation.end(), 0);
        std::shuffle(permutation.begin(), permutation.end(), rng);
        for (int variant = 0; variant < 4; ++variant) {
            std::vector<std::string> words;
            for (std::string word : original) {
                if (variant & 1)
                    for (char &c : word)
                        c = char('0' + permutation[c - '0']);
                if (variant & 2)
                    std::reverse(word.begin(), word.end());
                words.push_back(word);
            }
            DFA target = compileRaw(unionOfWords(words));
            Budget budget(1);
            FiniteSearch search(
                target, budget, {},
                [&](R candidate, const std::string &) {
                    assert(compare(target, compileRaw(Parser(candidate->s, false).parse())).equal);
                    ++offers;
                },
                [](R r) { return structural(r); });
            try {
                search.run();
                if (trial == 0 && variant == 0) {
                    size_t count = search.words.size();
                    search.run();
                    assert(search.words.size() == count);
                    assert(search.extract() && search.words.size() == count);
                }
            } catch (const Limit &) {
            }
            ++languages;
            if (variant == 0) {
                std::vector<std::string> required;
                for (size_t i = 0; i < words.size(); i += 2)
                    required.push_back(words[i]);
                DFA mandatory = compileRaw(unionOfWords(required));
                Budget partialBudget(1);
                int bestPublished = 1000000000;
                FiniteSearch partial(
                    target, partialBudget, {},
                    [&](R candidate, const std::string &) {
                        DFA actual = compileRaw(Parser(candidate->s, false).parse());
                        assert(compare(mandatory, actual, {}, nullptr, true).equal);
                        assert(compare(actual, target, {}, nullptr, true).equal);
                        bestPublished = std::min(bestPublished, int(wrap(candidate, 1).size()) + 1);
                        ++partialOffers;
                    },
                    [](R r) { return structural(r); });
                bool completed = true;
                try {
                    partial.runRequired(required);
                } catch (const Limit &) {
                    completed = false;
                }
                if (completed && !partialBudget.done()) {
                    // Independently enumerate every subset cost, without the
                    // branch-and-bound solver's dual or dominance pruning.
                    size_t states = size_t(1) << required.size();
                    std::vector<int> cost(states, 1000000000);
                    cost[0] = 0;
                    for (size_t missing = 1; missing < states; ++missing) {
                        uint64_t pivot = missing & -missing;
                        for (const auto &[cover, expression] : partial.rectangles)
                            if (cover.low & pivot)
                                cost[missing] = std::min(cost[missing],
                                                         cost[missing & ~cover.low] +
                                                             int(wrap(expression, 1).size()) + 1);
                    }
                    assert(bestPublished <= cost.back());
                }
            }
            if (variant == 1) {
                // A finite language occurs under both concatenation and a
                // repetition. The context must survive every local rewrite.
                R finite = unionOfWords(words);
                R source = alt({cat({chars(1 << 8), finite, rep(chars(1 << 9), 0, INF)}),
                                cat({rep(finite, 0, INF), chars(1 << 6)})});
                Budget localBudget(0.2);
                FiniteLocalSearch local(localBudget, {}, [](R r) { return structural(r); }, 6);
                R result = local.run(source);
                assert(compare(compileRaw(source), compileRaw(Parser(result->s, false).parse()))
                           .equal);
                ++contexts;
            }
        }
    }
    // This set has no connection to the finite-word construction above.
    // Productive cycles must be detected; any offered finite language still
    // has to be exactly the entire randomly generated DFA language.
    for (int trial = 0; trial < 80; ++trial) {
        int n = 1 + int(rng() % 12);
        DFA d;
        d.start = int(rng() % n);
        d.t.resize(n);
        d.accept.resize(n);
        for (int p = 0; p < n; ++p) {
            d.accept[p] = rng() % 2;
            for (int &q : d.t[p])
                q = int(rng() % n);
        }
        Budget budget(0.2);
        FiniteSearch search(
            d, budget, {},
            [&](R candidate, const std::string &) {
                assert(compare(d, compileRaw(Parser(candidate->s, false).parse())).equal);
                ++offers;
            },
            [](R r) { return structural(r); });
        try {
            search.run();
        } catch (const Limit &) {
        }
        ++languages;
    }
    // Exercise both mask limbs and the larger allowed-language cap of
    // partial covers, followed by a rejection beyond the 128-word limit.
    std::vector<std::string> boundary;
    for (unsigned value = 0; value < 64; ++value) {
        std::string word;
        for (int bit = 0; bit < 6; ++bit)
            word += char('0' + ((value >> bit) & 1));
        boundary.push_back(word);
    }
    DFA required64 = compileRaw(unionOfWords(boundary));
    auto allowedWords = boundary;
    allowedWords.push_back("2");
    DFA allowed65 = compileRaw(unionOfWords(allowedWords));
    Budget boundaryBudget(2);
    FiniteSearch boundarySearch(
        allowed65, boundaryBudget, {},
        [&](R candidate, const std::string &) {
            DFA actual = compileRaw(Parser(candidate->s, false).parse());
            assert(compare(required64, actual, {}, nullptr, true).equal);
            assert(compare(actual, allowed65, {}, nullptr, true).equal);
            ++partialOffers;
        },
        [](R r) { return structural(r); });
    try {
        boundarySearch.runRequired(boundary);
    } catch (const Limit &) {
    }
    assert(boundarySearch.words.size() == 65);
    ++languages;
    std::vector<std::string> boundary128;
    for (unsigned value = 0; value < 128; ++value) {
        std::string word;
        for (int bit = 0; bit < 7; ++bit)
            word += char('0' + ((value >> bit) & 1));
        boundary128.push_back(word);
    }
    DFA required128 = compileRaw(unionOfWords(boundary128));
    auto allowed129Words = boundary128;
    allowed129Words.push_back("2");
    DFA allowed129 = compileRaw(unionOfWords(allowed129Words));
    Budget largeBoundaryBudget(2);
    FiniteSearch largeBoundarySearch(
        allowed129, largeBoundaryBudget, {},
        [&](R candidate, const std::string &) {
            DFA actual = compileRaw(Parser(candidate->s, false).parse());
            assert(compare(required128, actual, {}, nullptr, true).equal);
            assert(compare(actual, allowed129, {}, nullptr, true).equal);
            ++partialOffers;
        },
        [](R r) { return structural(r); });
    try {
        largeBoundarySearch.runRequired(boundary128);
    } catch (const Limit &) {
    }
    assert(largeBoundarySearch.words.size() == 129);
    ++languages;
    Budget oversizedBudget(1);
    FiniteSearch oversized(
        allowed129, oversizedBudget, {}, [](R, const std::string &) { std::abort(); },
        [](R r) { return r; });
    oversized.runRequired(allowed129Words);
    assert(oversized.words.empty());
    assert(offers >= 120 && partialOffers >= 30 && contexts == 30);
    std::cout << "OK: " << languages << " unabhaengige Sprachen, " << offers << " Kandidaten, "
              << partialOffers << " Teilueberdeckungen und " << contexts
              << " unendliche Kontexte; Ziffernpermutation und Umkehrung geprueft.\n";
}
