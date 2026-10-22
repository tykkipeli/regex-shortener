#pragma once
#include "automata.hpp"
#include "unary.hpp"
namespace rx {
inline int support(const DFA &d) {
    int n = int(d.t.size()), mask = 0;
    std::vector<std::vector<int>> pred(n);
    for (int p = 0; p < n; ++p)
        for (int q : d.t[p])
            pred[q].push_back(p);
    std::queue<int> todo;
    std::vector<bool> live(n);
    for (int p = 0; p < n; ++p)
        if (d.accept[p]) {
            todo.push(p);
            live[p] = true;
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
    for (int p = 0; p < n; ++p)
        if (live[p])
            for (int c = 0; c < 10; ++c)
                if (live[d.t[p][c]])
                    mask |= 1 << c;
    return mask;
}
struct PatternSearch {
    const DFA &target;
    const Budget &budget;
    Limits limits;
    std::string targetKey;
    std::function<void(R, const std::string &)> offer;
    std::function<R(R)> refine;
    PatternSearch(const DFA &d, const Budget &b, Limits l,
                  std::function<void(R, const std::string &)> o, std::function<R(R)> f)
        : target(d), budget(b), limits(l), targetKey(key(d)), offer(std::move(o)),
          refine(std::move(f)) {}
    bool exact(R r) {
        return key(compile(r, limits, &budget)) == targetKey;
    }
    void cofinite(R original, int mask) {
        int n = int(target.t.size());
        if (n > 2000 || !mask)
            return;
        // States from which every continuation over the live alphabet is
        // accepted. Reachability of a rejecting state computes the complement.
        std::vector<std::vector<int>> pred(n);
        std::vector<bool> bad(n);
        for (int p = 0; p < n; ++p)
            for (int c = 0; c < 10; ++c)
                if (mask & (1 << c))
                    pred[target.t[p][c]].push_back(p);
        std::queue<int> todo;
        for (int p = 0; p < n; ++p)
            if (!target.accept[p]) {
                bad[p] = true;
                todo.push(p);
            }
        while (!todo.empty()) {
            int p = todo.front();
            todo.pop();
            for (int q : pred[p])
                if (!bad[q]) {
                    bad[q] = true;
                    todo.push(q);
                }
        }
        std::set<int> reach{target.start};
        int threshold = -1;
        for (int length = 0; length <= std::min(512, n * 2); ++length) {
            budget.check();
            bool universal = true;
            for (int p : reach)
                if (bad[p])
                    universal = false;
            if (universal) {
                threshold = length;
                break;
            }
            std::set<int> next;
            for (int p : reach)
                for (int c = 0; c < 10; ++c)
                    if (mask & (1 << c))
                        next.insert(target.t[p][c]);
            if (next == reach)
                break;
            reach = std::move(next);
        }
        if (threshold < 0)
            return;
        R tail = rep(chars(mask), threshold, INF);
        std::vector<int> accepted;
        for (int sub = mask; sub; sub = (sub - 1) & mask) {
            budget.check();
            std::vector<bool> seen(n);
            std::vector<int> q{target.start};
            seen[target.start] = true;
            bool ok = true;
            for (size_t i = 0; i < q.size() && ok; ++i) {
                int p = q[i];
                if (!target.accept[p]) {
                    ok = false;
                    break;
                }
                for (int c = 0; c < 10; ++c)
                    if (sub & (1 << c)) {
                        int x = target.t[p][c];
                        if (!seen[x]) {
                            seen[x] = true;
                            q.push_back(x);
                        }
                    }
            }
            if (ok)
                accepted.push_back(sub);
        }
        std::sort(accepted.begin(), accepted.end(), [](int a, int b) {
            return __builtin_popcount(unsigned(a)) > __builtin_popcount(unsigned(b));
        });
        std::vector<int> maximal;
        std::vector<R> cover{tail};
        for (int sub : accepted) {
            bool redundant = false;
            for (int old : maximal)
                if (!(sub & ~old))
                    redundant = true;
            if (!redundant) {
                maximal.push_back(sub);
                cover.push_back(rep(chars(sub), 0, INF));
            }
        }
        R simple = refine(alt(cover));
        if (exact(simple)) {
            offer(simple, "Endliche Ausnahmen");
            return;
        }
        if (original->k == ALT)
            cover.insert(cover.end(), original->sub.begin(), original->sub.end());
        else
            cover.push_back(original);
        std::sort(cover.begin(), cover.end(), [](R a, R b) { return a->s.size() > b->s.size(); });
        for (size_t i = 0; i < cover.size();) {
            budget.check();
            auto v = cover;
            v.erase(v.begin() + i);
            R r = refine(alt(v));
            if (exact(r))
                cover = std::move(v);
            else
                ++i;
        }
        offer(refine(alt(cover)), "Semantische Ueberdeckung");
    }
    struct Pair {
        R all, nonempty;
    };
    void noEqualNeighbors(int mask) {
        int k = __builtin_popcount(unsigned(mask));
        if (k < 2 || k > 6)
            return;
        std::vector<int> digits;
        for (int c = 0; c < 10; ++c)
            if (mask & (1 << c))
                digits.push_back(c);
        DFA expected;
        expected.t.resize(k + 2);
        expected.accept.assign(k + 2, 1);
        int dead = k + 1;
        expected.accept[dead] = 0;
        for (int p = 0; p < k + 2; ++p) {
            expected.t[p].fill(dead);
            if (p == dead)
                continue;
            for (int j = 0; j < k; ++j)
                expected.t[p][digits[j]] = p == j + 1 ? dead : j + 1;
        }
        if (key(minimize(expected, &budget)) != targetKey)
            return;
        std::map<int, Pair> memo;
        std::function<Pair(int)> solve = [&](int set) -> Pair {
            budget.check();
            auto it = memo.find(set);
            if (it != memo.end())
                return it->second;
            std::vector<int> v;
            for (int c = 0; c < 10; ++c)
                if (set & (1 << c))
                    v.push_back(c);
            Pair best;
            auto consider = [&](R all, R nonempty) {
                all = refine(all);
                nonempty = refine(nonempty);
                if (!best.all || better(all, best.all))
                    best.all = all;
                if (!best.nonempty || better(nonempty, best.nonempty))
                    best.nonempty = nonempty;
            };
            if (v.size() == 1) {
                R a = chars(set);
                best = {rep(a, 0, 1), a};
            } else if (v.size() == 2) {
                for (int orientation = 0; orientation < 2; ++orientation) {
                    R a = chars(1 << v[orientation]), b = chars(1 << v[1 - orientation]);
                    R middle = rep(cat({b, a}), 0, INF);
                    consider(cat({rep(a, 0, 1), middle, rep(b, 0, 1)}),
                             alt({a, cat({rep(a, 0, 1), middle, b, rep(a, 0, 1)})}));
                }
            } else
                for (int left = (set - 1) & set; left; left = (left - 1) & set) {
                    int right = set & ~left;
                    if (!right)
                        continue;
                    Pair a = solve(left), b = solve(right);
                    R all = cat({b.all, rep(cat({a.nonempty, b.nonempty}), 0, INF), a.all});
                    R nonempty =
                        alt({b.nonempty, cat({b.all, a.nonempty,
                                              rep(cat({b.nonempty, a.nonempty}), 0, INF), b.all})});
                    consider(all, nonempty);
                }
            return memo[set] = best;
        };
        offer(solve(mask).all, "Nachbarschaftsstruktur");
    }
    void run(R original) {
        int mask = support(target);
        if (__builtin_popcount(unsigned(mask)) == 1) {
            UnarySearch u{target, budget, __builtin_ctz(unsigned(mask)), refine, offer};
            u.run();
            return;
        }
        noEqualNeighbors(mask);
        cofinite(original, mask);
    }
};
} // namespace rx
