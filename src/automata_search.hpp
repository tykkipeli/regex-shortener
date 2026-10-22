#pragma once
#include "automata.hpp"
#include <tuple>

namespace rx {
// A second automata portfolio: labels are normalized while states are still
// present, and whole partial graphs compete instead of committing to one
// greedy elimination order. No alphabet, accepting condition or word is fixed.
struct AutomataSearch {
    struct Graph {
        int start = 0, finish = 0;
        std::vector<std::vector<R>> e;
        std::vector<uint8_t> alive;
        int remaining = 0;
    };
    const DFA &target;
    const Budget &budget;
    Limits limits;
    std::function<R(R)> refine;
    std::function<void(R, const std::string &)> offer;
    std::unordered_map<std::string, R> simplified;
    size_t expanded = 0;

    AutomataSearch(const DFA &d, const Budget &b, Limits l, std::function<R(R)> f,
                   std::function<void(R, const std::string &)> o)
        : target(d), budget(b), limits(l), refine(std::move(f)), offer(std::move(o)) {}

    R normalizeLabel(R r) {
        budget.check();
        auto found = simplified.find(r->s);
        if (found != simplified.end())
            return found->second;
        R result = refine(r);
        if (simplified.size() < 12000)
            simplified.emplace(r->s, result);
        return result;
    }
    Graph makeGraph(const DFA &d) {
        int n = int(d.t.size());
        std::vector<std::vector<int>> pred(n);
        for (int p = 0; p < n; ++p)
            for (int q : d.t[p])
                pred[q].push_back(p);
        std::vector<uint8_t> live(n);
        std::queue<int> todo;
        for (int p = 0; p < n; ++p)
            if (d.accept[p]) {
                live[p] = 1;
                todo.push(p);
            }
        while (!todo.empty()) {
            int p = todo.front();
            todo.pop();
            for (int q : pred[p])
                if (!live[q]) {
                    live[q] = 1;
                    todo.push(q);
                }
        }
        Graph g;
        g.start = n;
        g.finish = n + 1;
        g.e.assign(n + 2, std::vector<R>(n + 2, zero()));
        g.alive.assign(n + 2, 0);
        g.alive[n] = g.alive[n + 1] = 1;
        if (live[d.start])
            g.e[n][d.start] = eps();
        for (int p = 0; p < n; ++p)
            if (live[p]) {
                g.alive[p] = 1;
                ++g.remaining;
                std::map<int, int> destinations;
                for (int c = 0; c < 10; ++c)
                    if (live[d.t[p][c]])
                        destinations[d.t[p][c]] |= 1 << c;
                for (auto [q, mask] : destinations)
                    g.e[p][q] = chars(mask);
                if (d.accept[p])
                    g.e[p][g.finish] = eps();
            }
        return g;
    }
    Graph eliminate(Graph g, int pivot) {
        budget.check();
        const int n = int(g.e.size());
        R loop = normalizeLabel(rep(g.e[pivot][pivot], 0, INF));
        std::vector<int> in, out;
        for (int p = 0; p < n; ++p)
            if (g.alive[p] && p != pivot) {
                if (g.e[p][pivot]->k != EMPTY)
                    in.push_back(p);
                if (g.e[pivot][p]->k != EMPTY)
                    out.push_back(p);
            }
        for (int p : in)
            for (int q : out) {
                R candidate = alt({g.e[p][q], cat({g.e[p][pivot], loop, g.e[pivot][q]})});
                if (candidate->s.size() > 12000)
                    throw Limit("Partial-graph expression limit");
                g.e[p][q] = normalizeLabel(candidate);
            }
        for (int p = 0; p < n; ++p)
            g.e[p][pivot] = g.e[pivot][p] = zero();
        g.alive[pivot] = 0;
        --g.remaining;
        ++expanded;
        return g;
    }
    double cost(const Graph &g, int mode) {
        double size = 0, arcs = 0, maximum = 0;
        for (size_t p = 0; p < g.e.size(); ++p)
            if (g.alive[p])
                for (size_t q = 0; q < g.e.size(); ++q)
                    if (g.alive[q] && g.e[p][q]->k != EMPTY) {
                        double length = g.e[p][q]->s.size();
                        size += length;
                        ++arcs;
                        maximum = std::max(maximum, length);
                    }
        return size + (mode == 1 ? maximum : mode == 2 ? 2 * arcs : 0);
    }
    std::string graphKey(const Graph &g) {
        std::string result;
        for (size_t p = 0; p < g.e.size(); ++p) {
            result += char(g.alive[p]);
            if (g.alive[p])
                for (size_t q = 0; q < g.e.size(); ++q)
                    if (g.alive[q]) {
                        result += g.e[p][q]->s;
                        result += '\x1f';
                    }
        }
        return result;
    }
    void beam(Graph initial, size_t width, int mode = 0, bool reversed = false) {
        if (initial.remaining > 22)
            return;
        struct Entry {
            Graph g;
            double cost;
        };
        std::vector<Entry> beam{{initial, cost(initial, mode)}};
        while (!beam.empty()) {
            budget.check();
            if (beam.front().g.remaining == 0) {
                for (auto &entry : beam) {
                    R r = entry.g.e[entry.g.start][entry.g.finish];
                    offer(reversed ? reverseRegex(r) : r, "Normalisierte Automatenstrahlsuche");
                }
                return;
            }
            std::vector<Entry> next;
            std::unordered_set<std::string> seen;
            for (const auto &entry : beam) {
                const Graph &g = entry.g;
                for (size_t p = 0; p < g.e.size(); ++p)
                    if (g.alive[p] && int(p) != g.start && int(p) != g.finish) {
                        try {
                            Graph child = eliminate(g, int(p));
                            double score = cost(child, mode);
                            if (score > 24000)
                                continue;
                            if (seen.insert(graphKey(child)).second)
                                next.push_back({std::move(child), score});
                        } catch (const Limit &) {
                            budget.check();
                        }
                    }
            }
            std::stable_sort(next.begin(), next.end(),
                             [](const Entry &a, const Entry &b) { return a.cost < b.cost; });
            if (next.size() > width)
                next.resize(width);
            beam = std::move(next);
        }
    }
    // Antimirov partial derivatives give a second, epsilon-free NFA for the
    // input language. They need not reproduce its minimal deterministic graph.
    std::vector<R> partial(R r, int digit) {
        budget.check();
        std::vector<R> out;
        switch (r->k) {
        case EMPTY:
        case EPS:
            break;
        case CHAR:
            if (r->mask & (1 << digit))
                out.push_back(eps());
            break;
        case ALT:
            for (auto x : r->sub) {
                auto child = partial(x, digit);
                out.insert(out.end(), child.begin(), child.end());
            }
            break;
        case CAT:
            for (size_t i = 0; i < r->sub.size(); ++i) {
                std::vector<R> suffix(r->sub.begin() + i + 1, r->sub.end());
                for (R d : partial(r->sub[i], digit)) {
                    auto parts = suffix;
                    parts.insert(parts.begin(), d);
                    out.push_back(normalizeLabel(cat(parts)));
                }
                if (!nullable(r->sub[i]))
                    break;
            }
            break;
        case REP:
            if (r->hi != 0) {
                I lo = std::max<I>(0, r->lo - 1), hi = r->hi == INF ? INF : r->hi - 1;
                R tail = rep(r->sub[0], lo, hi);
                for (R d : partial(r->sub[0], digit))
                    out.push_back(normalizeLabel(cat({d, tail})));
            }
            break;
        }
        std::sort(out.begin(), out.end(), [](R a, R b) { return a->s < b->s; });
        out.erase(std::unique(out.begin(), out.end(), eq), out.end());
        return out;
    }
    Graph derivativeGraph(R seed, int cap = 64) {
        std::vector<R> residuals;
        std::map<std::string, int> ids;
        std::vector<std::tuple<int, int, int>> edges;
        auto add = [&](R r) {
            auto it = ids.find(r->s);
            if (it != ids.end())
                return it->second;
            if (int(ids.size()) >= cap)
                throw Limit("Partial derivative state limit");
            int index = int(ids.size());
            ids.emplace(r->s, index);
            residuals.push_back(r);
            return index;
        };
        add(normalizeLabel(seed));
        for (size_t p = 0; p < residuals.size(); ++p) {
            R r = residuals[p];
            for (int c = 0; c < 10; ++c)
                for (R next : partial(r, c))
                    edges.emplace_back(int(p), add(next), c);
        }
        int n = int(residuals.size());
        Graph g;
        g.start = n;
        g.finish = n + 1;
        g.remaining = n;
        g.alive.assign(n + 2, 1);
        g.e.assign(n + 2, std::vector<R>(n + 2, zero()));
        g.e[g.start][0] = eps();
        for (const auto &[p, q, c] : edges)
            g.e[p][q] = alt({g.e[p][q], chars(1 << c)});
        for (int p = 0; p < n; ++p)
            if (nullable(residuals[p]))
                g.e[p][g.finish] = eps();
        return g;
    }
    Graph residualGraph(bool maximalTargets = true, bool inclusionEpsilon = false) {
        // Canonical residual finite-state automaton: a residual is composite
        // if it is exactly the union of all its strictly smaller residuals.
        // This construction uses only language inclusion, not DFA topology or
        // a family-specific recognition rule.
        int n = int(target.t.size());
        std::vector<std::vector<uint8_t>> included(n, std::vector<uint8_t>(n));
        for (int p = 0; p < n; ++p)
            for (int q = 0; q < n; ++q) {
                budget.check();
                DFA left = target, right = target;
                left.start = p;
                right.start = q;
                included[p][q] = compare(left, right, limits, &budget, true).equal;
            }
        auto unionResiduals = [&](const std::vector<int> &states) {
            NFA automaton(&budget, limits.nfa);
            for (int p = 0; p < n + 2; ++p)
                automaton.state();
            automaton.start = n;
            automaton.finish = n + 1;
            for (int p : states)
                automaton.edge(n, p);
            for (int p = 0; p < n; ++p) {
                if (target.accept[p])
                    automaton.edge(p, n + 1);
                for (int c = 0; c < 10; ++c)
                    automaton.edge(p, target.t[p][c], 1 << c);
            }
            return determinize(automaton, limits, &budget);
        };
        std::vector<int> basis;
        for (int q = 0; q < n; ++q) {
            bool duplicate = false;
            std::vector<int> smaller;
            for (int p = 0; p < n; ++p) {
                if (p < q && included[p][q] && included[q][p])
                    duplicate = true;
                if (included[p][q] && !included[q][p])
                    smaller.push_back(p);
            }
            if (duplicate)
                continue;
            DFA residual = target;
            residual.start = q;
            if (!compare(residual, unionResiduals(smaller), limits, &budget).equal)
                basis.push_back(q);
        }
        int count = int(basis.size());
        Graph g;
        g.start = count;
        g.finish = count + 1;
        g.remaining = count;
        g.alive.assign(count + 2, 1);
        g.e.assign(count + 2, std::vector<R>(count + 2, zero()));
        auto destinations = [&](int residual) {
            std::vector<int> result;
            for (int p = 0; p < count; ++p)
                if (included[basis[p]][residual]) {
                    bool dominated = false;
                    if (maximalTargets)
                        for (int q = 0; q < count; ++q)
                            if (included[basis[q]][residual] && included[basis[p]][basis[q]] &&
                                !included[basis[q]][basis[p]])
                                dominated = true;
                    if (!dominated)
                        result.push_back(p);
                }
            return result;
        };
        for (int p : destinations(target.start))
            g.e[g.start][p] = eps();
        for (int p = 0; p < count; ++p) {
            if (target.accept[basis[p]])
                g.e[p][g.finish] = eps();
            for (int c = 0; c < 10; ++c)
                for (int q : destinations(target.t[basis[p]][c]))
                    g.e[p][q] = alt({g.e[p][q], chars(1 << c)});
        }
        if (inclusionEpsilon)
            for (int p = 0; p < count; ++p)
                for (int q = 0; q < count; ++q)
                    if (included[basis[q]][basis[p]] && !included[basis[p]][basis[q]]) {
                        bool covered = false;
                        for (int mid = 0; mid < count; ++mid)
                            if (mid != p && mid != q && included[basis[q]][basis[mid]] &&
                                included[basis[mid]][basis[p]])
                                covered = true;
                        if (!covered)
                            g.e[p][q] = alt({g.e[p][q], eps()});
                    }
        return g;
    }
    DFA graphAutomaton(const Graph &g) {
        NFA automaton(&budget, limits.nfa);
        for (size_t p = 0; p < g.e.size(); ++p)
            automaton.state();
        automaton.start = g.start;
        automaton.finish = g.finish;
        for (size_t p = 0; p < g.e.size(); ++p)
            if (g.alive[p])
                for (size_t q = 0; q < g.e.size(); ++q)
                    if (g.alive[q] && g.e[p][q]->k != EMPTY) {
                        R label = g.e[p][q];
                        if (label->k == EPS)
                            automaton.edge(int(p), int(q));
                        else if (label->k == CHAR)
                            automaton.edge(int(p), int(q), label->mask);
                        else {
                            auto [from, to] = automaton.build(label);
                            automaton.edge(int(p), from);
                            automaton.edge(to, int(q));
                        }
                    }
        return determinize(automaton, limits, &budget);
    }
    Graph prune(Graph g, uint64_t seed = 1) {
        // A transition may be redundant in the whole NFA even when its label
        // is locally indispensable. Delete it only after a complete proof.
        std::mt19937_64 rng(seed);
        std::vector<std::pair<int, int>> arcs;
        for (size_t p = 0; p < g.e.size(); ++p)
            for (size_t q = 0; q < g.e.size(); ++q)
                if (g.alive[p] && g.alive[q] && g.e[p][q]->k != EMPTY)
                    arcs.emplace_back(int(p), int(q));
        if (seed)
            std::shuffle(arcs.begin(), arcs.end(), rng);
        for (auto [p, q] : arcs) {
            budget.check();
            R previous = g.e[p][q];
            g.e[p][q] = zero();
            try {
                if (!compare(target, graphAutomaton(g), limits, &budget).equal)
                    g.e[p][q] = previous;
            } catch (const Limit &) {
                g.e[p][q] = previous;
                budget.check();
            }
        }
        return g;
    }
    void run(R seed = {}) {
        if (target.t.size() > 24)
            return;
        Graph g = makeGraph(target);
        beam(g, 128, 0);
        budget.check();
        Graph residual = residualGraph(false, true);
        beam(prune(residual, 0), 128, 0);
        beam(prune(residual, 1), 128, 0);
        budget.check();
        try {
            DFA reversed = reverseDFA(target, limits, &budget);
            beam(makeGraph(reversed), 96, 1, true);
        } catch (const Limit &) {
            budget.check();
        }
        if (seed)
            try {
                beam(derivativeGraph(seed), 64, 2);
            } catch (const Limit &) {
                budget.check();
            }
    }
};
} // namespace rx
