#include "../src/synthesis.hpp"
#include <cassert>

namespace rx {
std::atomic<bool> interrupted{false};
}

using namespace rx;

static void prove(const DFA &expected, R expression) {
    // Reparse the emitted syntax. An agreement of cached sample signatures or
    // internal ASTs is deliberately insufficient for this test.
    DFA printed = compileRaw(Parser(expression->s, false).parse());
    assert(compare(expected, printed).equal);
}

int main() {
    std::mt19937 random(9047);
    std::function<R(int)> expression = [&](int depth) -> R {
        if (!depth || random() % 4 == 0)
            return chars(1 << (random() % 4));
        R a = expression(depth - 1);
        switch (random() % 6) {
        case 0:
            return cat({a, expression(depth - 1)});
        case 1:
            return alt({a, expression(depth - 1)});
        case 2:
            return rep(a, 0, INF);
        case 3:
            return rep(a, 0, 1);
        case 4:
            return rep(a, 1, 3);
        default:
            return rep(a, 2, 2);
        }
    };

    size_t evaluations = 0, learned = 0, archived = 0;
    for (int trial = 0; trial < 40; ++trial) {
        R seed = expression(3);
        DFA target = compileRaw(seed);
        Budget global(0.3);
        SemanticSynthesis search(
            target, global, {}, [](R r) { return normalize(r); },
            [&](R r, const std::string &) { prove(target, r); }, uint64_t(trial + 1));
        for (bool pareto : {false, true}) {
            // The second form is intentionally wrong: input-derived seed
            // suggestions must never bypass the normal language proof.
            search.extraSeeds = {seed, cat({seed, chars(1 << 9)})};
            R result = search.run(seed, 0.012, 1000, pareto);
            assert(result->s.size() <= seed->s.size());
            prove(target, result);
            evaluations += search.stats.evaluated;

            std::set<std::string> seen;
            for (R alternative : search.equivalentSeeds(8)) {
                assert(seen.insert(alternative->s).second);
                prove(target, alternative);
                ++archived;
            }
            for (const auto &entry : search.learnedEntries(32)) {
                R printed = Parser(entry.second->s, false).parse();
                assert(entry.first == key(compile(printed)));
                ++learned;
            }
        }
    }
    assert(evaluations > 0);

    // Both the empty alphabet and an already exhausted outer budget are
    // ordinary stopping conditions. Reusing an instance is tested above.
    DFA epsilon = compileRaw(eps());
    Budget available(1), exhausted(0);
    SemanticSynthesis empty(
        epsilon, available, {}, [](R r) { return r; },
        [](R, const std::string &) { std::abort(); });
    assert(empty.run(eps())->k == EPS);
    SemanticSynthesis stopped(
        epsilon, exhausted, {}, [](R r) { return r; },
        [](R, const std::string &) { std::abort(); });
    assert(stopped.run(eps())->k == EPS && !stopped.stats.evaluated);

    std::cout << "OK: semantic synthesis, 40 random languages, both search modes, "
                 "untrusted seeds, reuse, "
              << archived << " proved archive entries and " << learned
              << " proved language keys.\n";
}
