#include "../src/automata_search.hpp"
#include "../src/optimize.hpp"
#include <cassert>
namespace rx {
std::atomic<bool> interrupted{false};
}
using namespace rx;

int main() {
    std::mt19937 rng(923817);
    Budget budget(120);
    int checks = 0;
    // Random transition systems are independent of any expression grammar.
    for (int trial = 0; trial < 120; ++trial) {
        int n = 2 + int(rng() % 10), alphabet = 1 + int(rng() % 4);
        DFA d;
        d.t.resize(n);
        d.accept.resize(n);
        for (int p = 0; p < n; ++p) {
            d.accept[p] = rng() % 2;
            d.t[p].fill(n - 1);
            for (int c = 0; c < alphabet; ++c)
                d.t[p][c] = int(rng() % n);
        }
        d = minimize(d);
        AutomataSearch search(
            d, budget, {}, [](R r) { return structural(r); }, [](R, const std::string &) {});
        for (bool maximal : {false, true})
            for (bool epsilon : {false, true}) {
                auto g = search.residualGraph(maximal, epsilon);
                assert(compare(d, search.graphAutomaton(g)).equal);
                ++checks;
                if (trial < 16) {
                    g = search.prune(g, trial + 1);
                    assert(compare(d, search.graphAutomaton(g)).equal);
                    ++checks;
                }
            }
    }
    std::function<R(int)> expression = [&](int depth) -> R {
        if (depth <= 0)
            return rng() % 5 == 0 ? eps() : chars(1 << (rng() % 4));
        switch (rng() % 5) {
        case 0:
            return alt({expression(depth - 1), expression(depth - 1)});
        case 1:
        case 2:
            return cat({expression(depth - 1), expression(depth - 1)});
        case 3:
            return rep(expression(depth - 1), 0, rng() % 2 ? INF : 3);
        default:
            return rep(expression(depth - 1), 1, 3);
        }
    };
    for (int trial = 0; trial < 120; ++trial) {
        R r = expression(3);
        DFA d = compileRaw(Parser(r->s, false).parse());
        AutomataSearch search(
            d, budget, {}, [](R x) { return structural(x); },
            [&](R x, const std::string &) {
                assert(compare(d, compileRaw(Parser(x->s, false).parse())).equal);
                ++checks;
            });
        try {
            auto g = search.derivativeGraph(r, 64);
            assert(compare(d, search.graphAutomaton(g)).equal);
            ++checks;
            if (g.remaining <= 6)
                search.beam(g, 24);
        } catch (const Limit &) {
            budget.check();
        }
    }
    std::cout << "OK: " << checks << " independent automata portfolio checks.\n";
}
