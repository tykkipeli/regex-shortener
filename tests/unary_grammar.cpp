#include "../src/optimize.hpp"
#include <cassert>
namespace rx { std::atomic<bool> interrupted{false}; }
using namespace rx;
using P = UnaryGrammarSearch::Polynomial;
static DFA lengthsDFA(const std::set<int> &values, int digit) {
    int dead = *values.rbegin() + 1;
    DFA d;
    d.t.resize(size_t(dead + 1));
    d.accept.resize(size_t(dead + 1));
    for (int n = 0; n <= dead; ++n) {
        d.t[n].fill(dead);
        d.t[n][digit] = std::min(n + 1, dead);
        d.accept[n] = values.count(n);
    }
    return d;
}
static std::set<int> values(P bits) {
    std::set<int> result;
    for (int n = 0; n < 128; ++n)
        if ((bits >> n) & 1) result.insert(n);
    return result;
}
static P bits(const std::set<int> &points) {
    P result = 0;
    for (int n : points) result |= P(1) << n;
    return result;
}
int main() {
    Budget all(150);
    std::mt19937 random(831961);
    int checks = 0, improvements = 0;
    for (int trial = 0; trial < 18; ++trial) {
        int digit = trial % 10;
        std::set<int> wanted;
        for (int n = 0; n < 10; ++n) wanted.insert(int(random() % 45));
        DFA d = lengthsDFA(wanted, digit);
        std::vector<R> words;
        for (int n : wanted) words.push_back(rep(chars(1 << digit), n, n));
        R seed = alt(words);
        UnaryGrammarSearch search(d, all, {}, [](R r) { return structural(r); },
            [&](R r, const std::string &) {
                DFA printed = compileRaw(Parser(r->s, false).parse());
                assert(compare(d, printed).equal);
                for (int n = 0; n <= 140; ++n)
                    assert(matches(printed, std::string(size_t(n), char('0' + digit))) ==
                           bool(wanted.count(n)));
                ++checks;
                ++improvements;
            });
        assert(search.deriveTarget());
        for (int sample = 0; sample < 100; ++sample) {
            std::set<int> a, b;
            for (int n = 0; n < 6; ++n) {
                a.insert(int(random() % 35));
                b.insert(int(random() % 35));
            }
            std::set<int> sums;
            for (int x : a) for (int y : b) sums.insert(x + y);
            P actual = search.product(bits(a), bits(b));
            if (*sums.rbegin() > search.maximum) assert(actual == 0);
            else assert(values(actual) == sums);
            std::set<int> offsets;
            for (int q = 0; q <= search.maximum; ++q) {
                bool fits = true;
                for (int x : a) if (!wanted.count(q + x)) fits = false;
                if (fits) offsets.insert(q);
            }
            assert(values(search.positions(bits(a))) == offsets);
            checks += 2;
        }
        assert(search.run(seed, .07, 15));
        int sampled = 0;
        for (const auto &[support, id] : search.known) {
            if (++sampled % 37) continue;
            const auto &text = search.bank[id];
            DFA printed = compileRaw(Parser(text.s, false).parse());
            for (int n = 0; n < 128; ++n)
                assert(matches(printed, std::string(size_t(n), char('0' + digit))) ==
                       bool((support >> n) & 1));
            ++checks;
        }
        assert(compare(d, compileRaw(Parser(search.best->s, false).parse())).equal);
        ++checks;
    }
    // Independent predicate -> independent chain automaton -> ordinary long
    // union input. References and numeric predicates belong only in tests.
    std::set<int> prime;
    for (int n = 2; n < 100; ++n) {
        bool isPrime = true;
        for (int divisor = 2; divisor * divisor <= n; ++divisor)
            if (n % divisor == 0) isPrime = false;
        if (isPrime) prime.insert(n);
    }
    DFA target = lengthsDFA(prime, 0);
    std::vector<R> words;
    for (int n : prime) words.push_back(rep(chars(1), n, n));
    UnaryGrammarSearch regression(target, all, {}, [](R r) { return structural(r); },
        [&](R r, const std::string &) {
            assert(compare(target, compileRaw(Parser(r->s, false).parse())).equal);
            ++checks;
            if (r->s.size() <= 71) throw Limit("Regression complete");
        });
    regression.run(alt(words), 90);
    assert(regression.best && regression.best->s.size() <= 71);
    DFA result = compileRaw(Parser(regression.best->s, false).parse());
    for (int n = 0; n < 200; ++n) {
        assert(matches(result, std::string(size_t(n), '0')) == bool(prime.count(n)));
        ++checks;
    }
    for (std::string input : {"[01]*", "(000)*", "0?", "0{121}"}) {
        DFA d = compileRaw(Parser(input, false).parse());
        UnaryGrammarSearch rejected(d, all, {}, [](R r) { return r; }, [](R, const std::string &) {});
        assert(!rejected.deriveTarget());
        ++checks;
    }
    interrupted.store(true);
    bool stopped = false;
    try { regression.run(regression.best, 10); }
    catch (const Limit &) { stopped = true; }
    interrupted.store(false);
    assert(stopped);
    ++checks;
    std::cout << "OK: " << checks << " independent unary grammar checks; " << improvements
              << " verified holdout improvements; cold prime result "
              << regression.best->s.size() << ".\n";
}
