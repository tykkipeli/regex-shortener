#pragma once
#include "finite.hpp"

namespace rx {

// Mines right-ideal sublanguages from residual continuation profiles. A state
// has profile n when every continuation of length at least n is accepted.
// Short prefix languages leading to such states give candidates P Sigma{n,}.
// The remaining finite language is optimized without knowing a task identity,
// numeric interpretation, comparison length, or any reference expression.
struct ProfileSearch {
    const DFA &target;
    const Budget &budget;
    Limits limits;
    std::function<void(R, const std::string &)> offer;
    std::function<R(R)> refine;
    std::vector<int> ranks;
    int mask = 0;
    size_t tails = 0, finiteRemainders = 0;

    ProfileSearch(const DFA &d, const Budget &b, Limits l,
                  std::function<void(R, const std::string &)> o, std::function<R(R)> f)
        : target(d), budget(b), limits(l), offer(std::move(o)), refine(std::move(f)) {}

    bool prepare() {
        int n = int(target.t.size());
        if (n > 512)
            return false;
        std::vector<std::vector<int>> pred(n);
        for (int p = 0; p < n; ++p)
            for (int q : target.t[p])
                pred[q].push_back(p);
        std::vector<bool> live(n);
        std::queue<int> todo;
        for (int p = 0; p < n; ++p)
            if (target.accept[p]) {
                live[p] = true;
                todo.push(p);
            }
        while (!todo.empty()) {
            int p = todo.front();
            todo.pop();
            for (int q : pred[p])
                if (!live[q]) {
                    live[q] = true;
                    todo.push(q);
                }
        }
        mask = 0;
        for (int p = 0; p < n; ++p)
            if (live[p])
                for (int c = 0; c < 10; ++c)
                    if (live[target.t[p][c]])
                        mask |= 1 << c;
        if (!mask)
            return false;
        // Greatest fixed point of accepting states closed under the actual
        // alphabet, followed by backwards uniform-length attraction.
        std::vector<bool> universal(target.accept.begin(), target.accept.end());
        bool changed = true;
        while (changed) {
            budget.check();
            changed = false;
            for (int p = 0; p < n; ++p)
                if (universal[p])
                    for (int c = 0; c < 10; ++c)
                        if ((mask & (1 << c)) && !universal[target.t[p][c]]) {
                            universal[p] = false;
                            changed = true;
                            break;
                        }
        }
        ranks.assign(n, -1);
        bool any = false;
        for (int p = 0; p < n; ++p)
            if (universal[p]) {
                ranks[p] = 0;
                any = true;
            }
        if (!any)
            return false;
        for (int depth = 1; depth <= std::min(32, n); ++depth) {
            budget.check();
            std::vector<int> next = ranks;
            bool advanced = false;
            for (int p = 0; p < n; ++p)
                if (ranks[p] < 0) {
                    bool all = true;
                    for (int c = 0; c < 10; ++c)
                        if ((mask & (1 << c)) &&
                            (ranks[target.t[p][c]] < 0 || ranks[target.t[p][c]] >= depth))
                            all = false;
                    if (all) {
                        next[p] = depth;
                        advanced = true;
                    }
                }
            ranks = std::move(next);
            if (!advanced)
                break;
        }
        return true;
    }

    R prefixes(int length, int continuation) {
        std::map<std::pair<int, int>, R> memo;
        std::function<R(int, int)> solve = [&](int p, int left) -> R {
            budget.check();
            if (!left)
                return ranks[p] >= 0 && ranks[p] <= continuation ? eps() : zero();
            auto key = std::make_pair(p, left);
            auto old = memo.find(key);
            if (old != memo.end())
                return old->second;
            std::map<std::string, std::pair<int, R>> groups;
            for (int c = 0; c < 10; ++c)
                if (mask & (1 << c)) {
                    R suffix = solve(target.t[p][c], left - 1);
                    if (suffix->k == EMPTY)
                        continue;
                    auto &entry = groups[suffix->s];
                    entry.first |= 1 << c;
                    entry.second = suffix;
                }
            std::vector<R> alternatives;
            for (const auto &entry : groups)
                alternatives.push_back(cat({chars(entry.second.first), entry.second.second}));
            return memo[key] = refine(alt(alternatives));
        };
        return solve(target.start, length);
    }

    DFA difference(const DFA &a, const DFA &b) {
        std::map<std::pair<int, int>, int> ids;
        std::vector<std::pair<int, int>> states;
        DFA out;
        auto add = [&](int p, int q) {
            auto key = std::make_pair(p, q);
            auto old = ids.find(key);
            if (old != ids.end())
                return old->second;
            if (states.size() >= size_t(limits.dfa) || states.size() >= limits.products)
                throw Limit("Profil-Differenzautomat zu gross");
            int id = int(states.size());
            ids[key] = id;
            states.push_back(key);
            out.t.push_back({});
            out.accept.push_back(a.accept[p] && !b.accept[q]);
            return id;
        };
        add(a.start, b.start);
        for (size_t i = 0; i < states.size(); ++i) {
            budget.check();
            auto [p, q] = states[i];
            for (int c = 0; c < 10; ++c) {
                int next = add(a.t[p][c], b.t[q][c]);
                out.t[i][c] = next;
            }
        }
        return minimize(out, &budget);
    }

    void run(R original) {
        if (!prepare())
            return;
        std::vector<int> depths;
        for (int rank : ranks)
            if (rank >= 0 && std::find(depths.begin(), depths.end(), rank) == depths.end())
                depths.push_back(rank);
        std::sort(depths.begin(), depths.end());
        std::map<std::string, R> candidates;
        int maximum = std::min(4, int(target.t.size()));
        for (int length = 0; length <= maximum; ++length)
            for (int continuation : depths) {
                budget.check();
                R prefix = prefixes(length, continuation);
                if (prefix->k == EMPTY)
                    continue;
                R tail = refine(cat({prefix, rep(chars(mask), continuation, INF)}));
                if (tail->s.size() < original->s.size())
                    candidates[tail->s] = tail;
            }
        std::vector<R> ordered;
        for (const auto &entry : candidates)
            ordered.push_back(entry.second);
        std::sort(ordered.begin(), ordered.end(), better);
        // Short tails are tried first. Composite profiles may make the finite
        // remainder cheaper, even when their own text is slightly longer.
        std::vector<R> singles = ordered;
        if (singles.size() > 12)
            singles.resize(12);
        for (size_t i = 0; i < singles.size(); ++i)
            for (size_t j = i + 1; j < singles.size(); ++j) {
                R joined = refine(alt({singles[i], singles[j]}));
                if (joined->s.size() < original->s.size())
                    ordered.push_back(joined);
            }
        std::sort(ordered.begin(), ordered.end(), better);
        std::unordered_set<std::string> tested;
        for (R tail : ordered) {
            budget.check();
            if (!tested.insert(tail->s).second)
                continue;
            ++tails;
            try {
                DFA tailDFA = compile(tail, limits, &budget);
                if (!compare(tailDFA, target, limits, &budget, true).equal)
                    continue;
                DFA remainder = difference(target, tailDFA);
                FiniteSearch extractor(
                    remainder, budget, limits, [](R, const std::string &) {}, refine);
                if (!extractor.extract(128))
                    continue;
                ++finiteRemainders;
                R baseline = extractor.represent(extractor.words);
                auto emit = [&](R finite, const std::string &) {
                    R candidate = refine(alt({tail, finite}));
                    if (better(candidate, original) &&
                        compare(target,
                                compileRaw(Parser(candidate->s, false).parse(), limits, &budget),
                                limits, &budget)
                            .equal)
                        offer(candidate, "Fortsetzungsprofile und endlicher Rest");
                };
                emit(baseline, "");
                if (extractor.words.empty())
                    continue;
                FiniteSearch search(remainder, budget, limits, emit, refine);
                search.run();
            } catch (const Limit &) {
                budget.check();
            }
        }
    }
};
} // namespace rx
