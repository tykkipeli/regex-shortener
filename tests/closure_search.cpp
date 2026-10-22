#include "../src/closure_search.hpp"
#include "../src/optimize.hpp"
#include <cassert>
namespace rx {
std::atomic<bool> interrupted{false};
}
using namespace rx;

// Independent product traversal records all target states at which the
// expression can finish, starting at an arbitrary target state.
static uint64_t referenceRelation(const DFA &target, const DFA &expression, int start) {
    std::queue<std::pair<int, int>> todo;
    std::set<std::pair<int, int>> seen;
    todo.push({start, expression.start});
    seen.insert(todo.front());
    uint64_t answer = 0;
    while (!todo.empty()) {
        auto [p, q] = todo.front();
        todo.pop();
        if (expression.accept[q])
            answer |= uint64_t(1) << p;
        for (int c = 0; c < 10; ++c) {
            auto next = std::make_pair(target.t[p][c], expression.t[q][c]);
            if (seen.insert(next).second)
                todo.push(next);
        }
    }
    return answer;
}

int main() {
    std::mt19937 rng(3218981);
    Budget budget(120);
    int checks = 0;
    std::function<R(int)> expression = [&](int depth) -> R {
        if (!depth)
            return rng() % 5 == 0 ? eps() : chars(1 << (rng() % 3));
        switch (rng() % 6) {
        case 0:
            return alt({expression(depth - 1), expression(depth - 1)});
        case 1:
        case 2:
            return cat({expression(depth - 1), expression(depth - 1)});
        case 3:
            return rep(expression(depth - 1), 0, INF);
        default: {
            I lo = rng() % 4, hi = lo + rng() % 5;
            return rep(expression(depth - 1), lo, hi);
        }
        }
    };
    for (int trial = 0; trial < 200; ++trial) {
        DFA d;
        int n = 2 + int(rng() % 8);
        d.t.resize(n);
        d.accept.resize(n);
        for (int p = 0; p < n; ++p) {
            d.accept[p] = rng() % 2;
            for (int c = 0; c < 10; ++c)
                d.t[p][c] = int(rng() % n);
        }
        R r = expression(3);
        ClosureSearch search(
            d, budget, {}, [](R x) { return structural(x); }, [](R, const std::string &) {});
        auto effect = search.action(r);
        DFA actual = compileRaw(Parser(r->s, false).parse());
        for (int p = 0; p < n; ++p) {
            assert(effect[p] == referenceRelation(d, actual, p));
            ++checks;
        }
    }
    for (int trial = 0; trial < 16; ++trial) {
        R seed = rep(expression(3), 0, INF);
        DFA target = compile(seed);
        Budget local(0.1);
        ClosureSearch search(
            target, local, {}, [](R r) { return structural(r); },
            [&](R result, const std::string &) {
                assert(compare(target, compileRaw(Parser(result->s, false).parse())).equal);
                ++checks;
            });
        assert(search.monoid());
        ++checks;
        try {
            search.run(seed);
        } catch (const Limit &) {
            // A timed-out discovery is permitted; no unproved output is.
        }
    }
    // Non-monoids must not be treated as closures merely because epsilon is
    // accepted. A positive word whose square is forbidden is a witness.
    for (const std::string text : {"|0", "0{0,4}", "|01|0101", "[01]{0,3}"}) {
        DFA target = compile(Parser(text, false).parse());
        ClosureSearch search(
            target, budget, {}, [](R r) { return structural(r); },
            [](R, const std::string &) { std::abort(); });
        assert(!search.monoid());
        search.run(Parser(text, false).parse());
        ++checks;
    }
    std::cout << "OK: " << checks << " independent closure-action checks.\n";
}
