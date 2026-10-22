#include "../src/optimize.hpp"
#include <cassert>
namespace rx {
std::atomic<bool> interrupted{false};
}
using namespace rx;
static void check(const std::string &s) {
    Budget b(10);
    R raw = Parser(s, false).parse(), n = structural(normalize(raw));
    DFA a = compileRaw(raw, {}, &b), c = compileRaw(Parser(n->s, false).parse(), {}, &b);
    auto cmp = compare(a, c, {}, &b);
    if (!cmp.equal) {
        std::cerr << "Unsichere Umformung: " << s << " -> " << n->s << "; Wort: " << cmp.witness
                  << '\n';
        std::abort();
    }
    DFA m = minimize(a, &b);
    assert(compare(a, m, {}, &b).equal);
}
static int checkRewrites(const std::string &s) {
    R raw = Parser(s, false).parse(), normal = normalize(raw);
    DFA source = compileRaw(raw);
    std::vector<R> variants = boundaryVariants(normal);
    auto hints = classHints(normal);
    if (normal->k == REP) {
        auto extra = localVariants(normal, hints);
        variants.insert(variants.end(), extra.begin(), extra.end());
    }
    for (auto candidate : variants) {
        R printed = Parser(candidate->s, false).parse();
        if (!compare(source, compileRaw(printed)).equal) {
            std::cerr << "Falsche neue Identitaet: " << s << " -> " << candidate->s << '\n';
            std::abort();
        }
    }
    check(s);
    return 1 + int(variants.size());
}
static int referenceSize(DFA d) {
    d = canonical(d);
    int n = int(d.t.size());
    std::vector<std::vector<bool>> diff(n, std::vector<bool>(n));
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < i; ++j)
            diff[i][j] = d.accept[i] != d.accept[j];
    bool change = true;
    while (change) {
        change = false;
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < i; ++j)
                if (!diff[i][j])
                    for (int c = 0; c < 10; ++c) {
                        int a = d.t[i][c], b = d.t[j][c];
                        if (a < b)
                            std::swap(a, b);
                        if (diff[a][b]) {
                            diff[i][j] = true;
                            change = true;
                            break;
                        }
                    }
    }
    int size = 0;
    for (int i = 0; i < n; ++i) {
        bool first = true;
        for (int j = 0; j < i; ++j)
            if (!diff[i][j]) {
                first = false;
                break;
            }
        size += first;
    }
    return size;
}
int main() {
    int tests = 0;
    for (int a = 0; a <= 4; ++a)
        for (int b = a; b <= 5; ++b)
            for (int c = 0; c <= 4; ++c)
                for (int d = c; d <= 5; ++d) {
                    std::string s = "(0{" + std::to_string(a) + "," + std::to_string(b) + "}){" +
                                    std::to_string(c) + "," + std::to_string(d) + "}";
                    check(s);
                    ++tests;
                }
    for (int a = 0; a <= 4; ++a)
        for (int c = 0; c <= 4; ++c)
            for (int d = c; d <= 5; ++d) {
                check("(0{" + std::to_string(a) + ",}){" + std::to_string(c) + "," +
                      std::to_string(d) + "}");
                ++tests;
            }
    for (auto s : {"0*(10*)*", "(0*1)*0*", "(0?1?)*", "(0*1?)*", "(00|000)*", "(0|00|000)*",
                   "(01|0101|010101)*", "0{5}1|0{6}2", "0|00|000|0000", "01(01)*", "(01)*(01)?",
                   "(0{0}|1){3}", "[01]{2,3}|[01]{4}", "(0*1)*0*2", "2(0*1)*0*", "0*(10*)*2"}) {
        check(s);
        ++tests;
    }
    // All nonempty subset relationships over four symbols, in both
    // directions, with zero/mandatory repetitions and bounded edge powers.
    for (int universe = 1; universe < 16; ++universe)
        for (int part = universe; part; part = (part - 1) & universe)
            for (auto loop : {"*", "+", "{2,}"})
                for (auto count : {"", "*", "+", "{0,3}", "{2,4}", "{2}"}) {
                    std::string a = charset(part) + count, u = charset(universe) + loop;
                    tests += checkRewrites(a + u);
                    tests += checkRewrites(u + a);
                }
    for (auto s : {"5|[1-9][0-9]*[05]",
                   "[1-9][0-9]*",
                   "[0-9]*[1-9]",
                   "[1-9]{1,4}[0-9]*",
                   "[0-9]*[1-9]{1,4}",
                   "01(01|2)*",
                   "(01|2)*01",
                   "(01|23)(01|23|4)*",
                   "(01|23|4)*(01|23)",
                   "(01|2)(01|2|3)*",
                   "[12][23]*",
                   "[23]*[12]",
                   "0[12]*",
                   "[12]*0",
                   "01[01]*",
                   "[01]*01",
                   "[0-9]*",
                   "[012]*",
                   "1234567(1234567|8)*",
                   "(1234567|8)*1234567"})
        tests += checkRewrites(s);
    assert(!unionRemainder(chars(6), chars(3)));
    assert(!unionRemainder(Parser("[01]").parse(), Parser("01").parse()));
    // Polynomial finite-language synthesis: compare to the explicit finite
    // union, not to another invocation of the same length-set algorithm.
    std::mt19937 lengthsRng(92713);
    Budget advancedBudget(60);
    for (int trial = 0; trial < 24; ++trial) {
        std::set<int> set;
        for (int j = 0; j < 7; ++j)
            set.insert(int(lengthsRng() % 25));
        std::vector<int> lengths(set.begin(), set.end());
        std::vector<R> literal;
        for (int n : lengths)
            literal.push_back(rawRep(chars(1), n, n));
        DFA target = compile(alt(literal));
        UnarySearch search(
            target, advancedBudget, 0, [](R r) { return structural(r); },
            [](R, const std::string &) {});
        R result = search.finite(lengths);
        assert(
            compare(compileRaw(alt(literal)), compileRaw(Parser(result->s, false).parse())).equal);
        ++tests;
    }
    // Recognize the same adjacency language after changing its alphabet.
    for (int mask : {3, 42, 85, 340, 341}) {
        std::vector<int> digits;
        for (int c = 0; c < 10; ++c)
            if (mask & (1 << c))
                digits.push_back(c);
        int k = int(digits.size()), dead = k + 1;
        DFA d;
        d.t.resize(k + 2);
        d.accept.assign(k + 2, 1);
        d.accept[dead] = 0;
        for (int q = 0; q < k + 2; ++q) {
            d.t[q].fill(dead);
            if (q == dead)
                continue;
            for (int i = 0; i < k; ++i)
                d.t[q][digits[i]] = q == i + 1 ? dead : i + 1;
        }
        d = minimize(d);
        bool offered = false;
        PatternSearch search(
            d, advancedBudget, {},
            [&](R r, const std::string &) {
                assert(compare(d, compileRaw(Parser(r->s, false).parse())).equal);
                offered = true;
            },
            [](R r) { return structural(r); });
        search.noEqualNeighbors(mask);
        assert(offered);
        ++tests;
    }
    std::mt19937 rng(199937);
    // Independent greedy subsequence automata, including repeated adjacent
    // letters, larger alphabets and a one-letter alphabet.
    for (int trial = 0; trial < 240; ++trial) {
        int k = 1 + int(rng() % 4), mask = (1 << k) - 1;
        std::string word;
        for (int i = 0, len = 1 + int(rng() % 12); i < len; ++i)
            word += char('0' + rng() % k);
        int m = int(word.size()), dead = m;
        DFA d;
        d.t.resize(m + 1);
        d.accept.assign(m + 1, 1);
        d.accept[dead] = 0;
        for (int q = 0; q <= m; ++q) {
            d.t[q].fill(dead);
            if (q == dead)
                continue;
            for (int c = 0; c < k; ++c)
                d.t[q][c] = q + (word[q] == char('0' + c));
        }
        PatternSearch search(
            d, advancedBudget, {}, [](R, const std::string &) {},
            [](R r) { return structural(r); });
        R r = search.avoidSubsequence(word, mask);
        assert(compare(d, compileRaw(Parser(r->s, false).parse())).equal);
        ++tests;
    }
    for (auto operand : {"0*1*", "0?1?", "(01)*", "(|00|111)", "(0?1){0,3}"})
        for (int lo = 0; lo <= 4; ++lo)
            for (int hi = std::max(1, lo); hi <= 6; ++hi) {
                R base = Parser(operand, false).parse();
                R r = rawRep(base, lo, hi);
                std::string literal = "(" + std::string(operand) + "){" + std::to_string(lo) + "," +
                                      std::to_string(hi) + "}";
                assert(compare(compileRaw(Parser(literal, false).parse()),
                               compileRaw(Parser(r->s, false).parse()))
                           .equal);
                ++tests;
            }
    for (int it = 0; it < 2000; ++it) {
        int n = 1 + int(rng() % 18);
        DFA d;
        d.t.resize(n);
        d.accept.resize(n);
        d.start = int(rng() % n);
        for (int i = 0; i < n; ++i) {
            d.accept[i] = rng() % 2;
            for (int c = 0; c < 10; ++c)
                d.t[i][c] = int(rng() % n);
        }
        DFA m = minimize(d);
        assert(int(m.t.size()) == referenceSize(d));
        assert(compare(d, m).equal);
        assert(key(m) == key(minimize(m)));
        ++tests;
    }
    std::cout << "OK: " << tests << " interne Pruefungen (inkl. unabh. DFA-Minimalitaetstest).\n";
}
