#pragma once
#include "automata.hpp"

namespace rx {
// Search for small generators of a regular submonoid. All action signatures
// are computed on the supplied DFA; no counter, word, alphabet or task is
// distinguished. Exact equivalence is still required for each offered result.
struct ClosureSearch {
    using Relation = std::vector<uint64_t>;
    struct Macro {
        R r;
        Relation action;
        std::shared_ptr<DFA> dfa;
        std::vector<std::vector<uint64_t>> edges;
        int coverage = 0, order = 0;
    };
    const DFA &target;
    const Budget &budget;
    Limits limits;
    std::function<R(R)> refine;
    std::function<void(R, const std::string &)> offer;
    int n;
    uint64_t accepting = 0;
    std::unordered_map<std::string, Relation> relations;
    std::vector<Macro> macros;
    std::unordered_set<std::string> texts;
    std::vector<std::string> samples;
    std::vector<std::string> probes;
    size_t tested = 0;

    ClosureSearch(const DFA &d, const Budget &b, Limits l, std::function<R(R)> f,
                  std::function<void(R, const std::string &)> o)
        : target(d), budget(b), limits(l), refine(std::move(f)), offer(std::move(o)),
          n(int(d.t.size())) {
        if (n <= 63)
            for (int p = 0; p < n; ++p)
                if (target.accept[p])
                    accepting |= uint64_t(1) << p;
    }
    Relation identity() const {
        Relation r(n);
        for (int p = 0; p < n; ++p)
            r[p] = uint64_t(1) << p;
        return r;
    }
    Relation unite(Relation a, const Relation &b) const {
        for (int p = 0; p < n; ++p)
            a[p] |= b[p];
        return a;
    }
    Relation compose(const Relation &a, const Relation &b) const {
        Relation c(n);
        for (int p = 0; p < n; ++p) {
            uint64_t row = a[p];
            while (row) {
                int q = __builtin_ctzll(row);
                row &= row - 1;
                c[p] |= b[q];
            }
        }
        return c;
    }
    Relation power(Relation r, I k) const {
        Relation result = identity();
        while (k > 0) {
            if (k & 1)
                result = compose(result, r);
            k >>= 1;
            if (k)
                r = compose(r, r);
        }
        return result;
    }
    Relation star(Relation r) const {
        for (int p = 0; p < n; ++p)
            r[p] |= uint64_t(1) << p;
        for (int q = 0; q < n; ++q)
            for (int p = 0; p < n; ++p)
                if (r[p] & (uint64_t(1) << q))
                    r[p] |= r[q];
        return r;
    }
    Relation sumPowers(const Relation &r, I count) const {
        // Pair monoid: (R^a, sum_{i<a}R^i)*(R^b,sum_{i<b}R^i).
        Relation accumulatedPower = identity(), sum(n), blockPower = r, blockSum = identity();
        while (count > 0) {
            if (count & 1) {
                sum = unite(sum, compose(accumulatedPower, blockSum));
                accumulatedPower = compose(accumulatedPower, blockPower);
            }
            count >>= 1;
            if (count) {
                blockSum = unite(blockSum, compose(blockPower, blockSum));
                blockPower = compose(blockPower, blockPower);
            }
        }
        return sum;
    }
    Relation action(R r) {
        budget.check();
        auto found = relations.find(r->s);
        if (found != relations.end())
            return found->second;
        Relation result(n);
        switch (r->k) {
        case EMPTY:
            break;
        case EPS:
            result = identity();
            break;
        case CHAR:
            for (int p = 0; p < n; ++p)
                for (int c = 0; c < 10; ++c)
                    if (r->mask & (1 << c))
                        result[p] |= uint64_t(1) << target.t[p][c];
            break;
        case ALT:
            for (R child : r->sub)
                result = unite(result, action(child));
            break;
        case CAT:
            result = identity();
            for (R child : r->sub)
                result = compose(result, action(child));
            break;
        case REP: {
            Relation base = action(r->sub[0]);
            result = compose(power(base, r->lo),
                             r->hi == INF ? star(base) : sumPowers(base, r->hi - r->lo + 1));
            break;
        }
        }
        if (relations.size() < 20000)
            relations.emplace(r->s, result);
        return result;
    }
    bool safe(const Relation &r) const {
        return !(r[target.start] & ~accepting);
    }
    bool monoid() {
        if (!target.accept[target.start])
            return false;
        for (int p = 0; p < n; ++p)
            if (target.accept[p]) {
                DFA residual = target;
                residual.start = p;
                if (!compare(target, residual, limits, &budget, true).equal)
                    return false;
            }
        return true;
    }
    std::string signature(const Relation &r) const {
        std::string key;
        for (uint64_t row : r)
            for (int byte = 0; byte < 8; ++byte)
                key += char((row >> (byte * 8)) & 255);
        return key;
    }
    void add(R r, size_t cap = 5000) {
        budget.check();
        if (r->s.empty() || r->s.size() > 48 || r->k == EMPTY || macros.size() >= cap ||
            !texts.insert(r->s).second)
            return;
        Macro m;
        m.r = r;
        m.action = action(r);
        Relation effect = identity();
        for (int k = 1; k <= 2 * n; ++k) {
            effect = compose(effect, m.action);
            if (safe(effect)) {
                m.order = k;
                break;
            }
        }
        macros.push_back(std::move(m));
    }
    void windows(const std::vector<R> &parts) {
        for (size_t first = 0; first < parts.size(); ++first)
            for (size_t end = first + 1; end <= parts.size() && end <= first + 6; ++end) {
                std::vector<R> segment(parts.begin() + first, parts.begin() + end);
                auto variants = [&](std::vector<R> v) {
                    add(cat(v));
                    if (v.size() <= 5)
                        for (size_t shift = 1; shift < v.size(); ++shift) {
                            std::rotate(v.begin(), v.begin() + 1, v.end());
                            add(cat(v));
                        }
                };
                variants(segment);
                if (segment.size() >= 3)
                    for (size_t erased = 0; erased < segment.size(); ++erased) {
                        auto v = segment;
                        v.erase(v.begin() + erased);
                        variants(v);
                    }
            }
    }
    void collect(R r) {
        add(r);
        if (r->k == CAT) {
            windows(r->sub);
            // Expose windows crossing one distributed union. The original
            // expression is retained; these are only candidate generators.
            for (size_t i = 0; i < r->sub.size(); ++i)
                if (r->sub[i]->k == ALT && r->sub[i]->sub.size() <= 8)
                    for (R term : r->sub[i]->sub) {
                        auto parts = r->sub;
                        parts.erase(parts.begin() + i);
                        if (term->k == CAT)
                            parts.insert(parts.begin() + i, term->sub.begin(), term->sub.end());
                        else
                            parts.insert(parts.begin() + i, term);
                        windows(parts);
                    }
        }
        for (R child : r->sub)
            collect(child);
    }
    void makeSamples(int mask) {
        std::vector<std::string> frontier{std::string()};
        for (int depth = 0; depth <= std::min(10, n + 3) && samples.size() < 192; ++depth) {
            std::vector<std::string> next;
            for (const std::string &w : frontier) {
                if (probes.size() < 384)
                    probes.push_back(w);
                if (matches(target, w))
                    samples.push_back(w);
                if (samples.size() >= 192)
                    break;
                if (next.size() < 1600)
                    for (int c = 0; c < 10; ++c)
                        if (mask & (1 << c))
                            next.push_back(w + char('0' + c));
            }
            frontier = std::move(next);
        }
    }
    bool compileMacro(Macro &m) {
        budget.check();
        if (m.dfa)
            return true;
        try {
            Limits local = limits;
            local.dfa = std::min(local.dfa, 160);
            local.nfa = std::min(local.nfa, 2400);
            m.dfa = std::make_shared<DFA>(compileRaw(m.r, local, &budget));
            m.edges.reserve(samples.size());
            for (const auto &w : samples) {
                std::vector<uint64_t> rows(w.size() + 1);
                for (size_t start = 0; start <= w.size(); ++start) {
                    int q = m.dfa->start;
                    if (m.dfa->accept[q])
                        rows[start] |= uint64_t(1) << start;
                    for (size_t end = start; end < w.size(); ++end) {
                        q = m.dfa->t[q][w[end] - '0'];
                        if (m.dfa->accept[q])
                            rows[start] |= uint64_t(1) << (end + 1);
                    }
                }
                m.edges.push_back(std::move(rows));
            }
            for (const std::string &w : probes)
                m.coverage += matches(*m.dfa, w);
            return true;
        } catch (const Limit &) {
            budget.check();
            return false;
        }
    }
    bool sampleLift(const Macro &s, const Macro &r, int order, size_t index) const {
        size_t length = samples[index].size();
        std::vector<uint64_t> reachable(length + 1);
        reachable[0] = 1;
        for (size_t pos = 0; pos < length; ++pos) {
            uint64_t phases = reachable[pos];
            while (phases) {
                int phase = __builtin_ctzll(phases);
                phases &= phases - 1;
                uint64_t ends = s.edges[index][pos] & ~((uint64_t(1) << (pos + 1)) - 1);
                while (ends) {
                    int end = __builtin_ctzll(ends);
                    ends &= ends - 1;
                    reachable[end] |= uint64_t(1) << phase;
                }
                ends = r.edges[index][pos] & ~((uint64_t(1) << (pos + 1)) - 1);
                while (ends) {
                    int end = __builtin_ctzll(ends);
                    ends &= ends - 1;
                    reachable[end] |= uint64_t(1) << ((phase + 1) % order);
                }
            }
        }
        return reachable[length] & 1;
    }
    bool exact(R r) {
        ++tested;
        try {
            return compare(target, compileRaw(r, limits, &budget), limits, &budget).equal;
        } catch (const Limit &) {
            budget.check();
            return false;
        }
    }
    void run(R seed) {
        if (n > 24 || !monoid())
            return;
        collect(seed);
        int mask = alphabet(seed);
        for (int c = 0; c < 10; ++c)
            if (mask & (1 << c))
                add(chars(1 << c));
        // Neutral loops can be inserted at any intermediate target state.
        // This is established by the entire transition relation, not merely
        // by accepting the loop word at the initial state.
        std::vector<R> loops;
        Relation neutral = identity();
        for (const auto &m : macros)
            if (m.action == neutral)
                loops.push_back(rep(m.r, 0, INF));
        std::sort(loops.begin(), loops.end(), better);
        loops.erase(std::unique(loops.begin(), loops.end(), eq), loops.end());
        if (loops.size() > 3)
            loops.resize(3);
        size_t initial = macros.size();
        for (size_t i = 0; i < initial; ++i) {
            R r = macros[i].r;
            if (r->s.size() > 24)
                continue;
            std::vector<R> parts = r->k == CAT ? r->sub : std::vector<R>{r};
            for (R loop : loops) {
                if (parts.size() == 1) {
                    add(cat({loop, r}));
                    add(cat({r, loop}));
                } else if (parts.size() <= 6)
                    for (size_t gap = 1; gap < parts.size(); ++gap) {
                        auto v = parts;
                        v.insert(v.begin() + gap, loop);
                        add(cat(v));
                    }
            }
        }
        makeSamples(mask);
        std::map<std::string, std::vector<size_t>> grouped;
        for (size_t i = 0; i < macros.size(); ++i)
            if (macros[i].order)
                grouped[signature(macros[i].action)].push_back(i);
        std::vector<Macro> selected;
        std::unordered_set<std::string> selectedTexts;
        for (auto &[signature, ids] : grouped) {
            (void)signature;
            std::sort(ids.begin(), ids.end(),
                      [&](size_t a, size_t b) { return better(macros[a].r, macros[b].r); });
            if (ids.size() > 600)
                ids.resize(600);
            std::vector<size_t> ready;
            for (size_t id : ids)
                if (compileMacro(macros[id]))
                    ready.push_back(id);
            std::stable_sort(ready.begin(), ready.end(), [&](size_t a, size_t b) {
                double ac = double(macros[a].coverage) / (macros[a].r->s.size() + 1);
                double bc = double(macros[b].coverage) / (macros[b].r->s.size() + 1);
                return ac == bc ? better(macros[a].r, macros[b].r) : ac > bc;
            });
            if (ready.size() > 20)
                ready.resize(20);
            for (size_t id : ready)
                if (selectedTexts.insert(macros[id].r->s).second)
                    selected.push_back(macros[id]);
            // Equal actions permit useful union macros without a grammar
            // enumeration over arbitrary combinations of unrelated effects.
            for (size_t i = 0; i < ready.size(); ++i)
                for (size_t j = i + 1; j < ready.size(); ++j) {
                    Macro combined;
                    combined.r = alt({macros[ready[i]].r, macros[ready[j]].r});
                    if (combined.r->s.size() > 36)
                        combined.r = refine(combined.r);
                    if (combined.r->s.size() > 36)
                        continue;
                    if (!selectedTexts.insert(combined.r->s).second)
                        continue;
                    combined.action = macros[ready[i]].action;
                    combined.order = macros[ready[i]].order;
                    if (compileMacro(combined)) {
                        // Retain a short action representative alongside a
                        // broader union. This is still a same-action union,
                        // so its safety power is computed rather than guessed.
                        Macro augmented;
                        augmented.r = refine(alt({combined.r, macros[ids.front()].r}));
                        augmented.action = combined.action;
                        augmented.order = combined.order;
                        if (augmented.r->s.size() <= 36 && !eq(augmented.r, combined.r) &&
                            selectedTexts.insert(augmented.r->s).second && compileMacro(augmented))
                            selected.push_back(std::move(augmented));
                        selected.push_back(std::move(combined));
                    }
                }
        }
        std::vector<Macro> safeGenerators;
        std::unordered_set<std::string> safeTexts;
        auto addPower = [&](const Macro &m) {
            if (!m.order)
                return;
            Macro s;
            s.r = rep(m.r, m.order, m.order);
            if (s.r->s.size() > 32 || !safeTexts.insert(s.r->s).second)
                return;
            s.action = power(m.action, m.order);
            s.order = 1;
            if (compileMacro(s))
                safeGenerators.push_back(std::move(s));
        };
        for (const Macro &m : macros)
            addPower(m);
        for (const Macro &m : selected)
            addPower(m);
        // Short candidates are tested first. Sampling is only a rejection
        // filter; accepting a new output always requires an exact DFA proof.
        std::sort(safeGenerators.begin(), safeGenerators.end(),
                  [](const Macro &a, const Macro &b) { return better(a.r, b.r); });
        safeGenerators.erase(
            std::unique(safeGenerators.begin(), safeGenerators.end(),
                        [](const Macro &a, const Macro &b) { return eq(a.r, b.r); }),
            safeGenerators.end());
        std::sort(selected.begin(), selected.end(),
                  [](const Macro &a, const Macro &b) { return better(a.r, b.r); });
        size_t incumbentLength = seed->s.size();
        for (const Macro &s : safeGenerators) {
            budget.check();
            R closure = rep(s.r, 0, INF);
            if (closure->s.size() < incumbentLength && exact(closure)) {
                R result = refine(closure);
                incumbentLength = std::min(incumbentLength, result->s.size());
                offer(result, "Erzeuger einer Sternsprache");
            }
            for (const Macro &r : selected) {
                if (r.order <= 1 || r.order > 2 * n)
                    continue;
                R lifted = rep(cat({r.r, rep(s.r, 0, INF)}), r.order, r.order);
                R result = rep(alt({s.r, lifted}), 0, INF);
                if (result->s.size() >= incumbentLength || !safe(action(result)))
                    continue;
                bool covered = true;
                for (size_t i = 0; i < samples.size(); ++i)
                    if (!sampleLift(s, r, r.order, i)) {
                        covered = false;
                        break;
                    }
                if (covered && exact(result)) {
                    result = refine(result);
                    incumbentLength = std::min(incumbentLength, result->s.size());
                    offer(result, "Endliche Makroaktionen und Sternbasen");
                }
            }
        }
    }
};
} // namespace rx
