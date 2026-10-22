#pragma once
#include "automata.hpp"
namespace rx {
struct UnarySearch {
    const DFA &target;
    const Budget &budget;
    int digit;
    std::function<R(R)> refine;
    std::function<void(R, const std::string &)> offer;
    std::map<std::vector<int>, R> finiteCache, expandedCache;
    UnarySearch(const DFA &d, const Budget &b, int c, std::function<R(R)> f,
                std::function<void(R, const std::string &)> o)
        : target(d), budget(b), digit(c), refine(std::move(f)), offer(std::move(o)) {}
    R power(int n) {
        return rep(chars(1 << digit), n, n);
    }
    R finite(const std::vector<int> &lengths) {
        if (lengths.empty())
            return zero();
        auto cached = finiteCache.find(lengths);
        if (cached != finiteCache.end())
            return cached->second;
        int maximum = lengths.back(), n = int(lengths.size());
        std::vector<bool> present(maximum + 1);
        for (int x : lengths)
            present[x] = true;
        if (n > 40 || maximum > 160) {
            std::vector<R> v;
            for (int x : lengths)
                v.push_back(power(x));
            return refine(alt(v));
        }
        std::unordered_map<uint64_t, R> terms;
        auto add = [&](const std::vector<int> &set, R expr) {
            uint64_t mask = 0;
            for (int x : set) {
                auto i = std::lower_bound(lengths.begin(), lengths.end(), x);
                if (i == lengths.end() || *i != x)
                    return;
                mask |= 1ULL << (i - lengths.begin());
            }
            auto it = terms.find(mask);
            if (it == terms.end() || better(expr, it->second))
                terms[mask] = expr;
        };
        for (int x : lengths)
            add({x}, power(x));
        for (int c : lengths)
            for (int step = 1; c + step <= maximum; ++step) {
                std::vector<int> set{c};
                for (int k = 1; c + k * step <= maximum && present[c + k * step]; ++k) {
                    set.push_back(c + k * step);
                    int offset = c % step, lo = c / step;
                    add(set, cat({power(offset), rep(power(step), lo, lo + k)}));
                }
            }
        // Bounded sums of two generators plus epsilon. This discovers compact
        // polynomial factorizations of finite length sets, not just intervals.
        if (maximum <= 96)
            for (int a = 1; a * 2 <= maximum; ++a)
                for (int b = a + 1; b * 2 <= maximum; ++b) {
                    budget.check();
                    std::vector<int> set{0};
                    for (int k = 1; k <= 6 && k * b <= maximum; ++k) {
                        std::set<int> next;
                        for (int x : set) {
                            next.insert(x);
                            next.insert(x + a);
                            next.insert(x + b);
                        }
                        set.assign(next.begin(), next.end());
                        if (k < 2)
                            continue;
                        for (int c : lengths) {
                            if (c + k * b > maximum)
                                break;
                            bool fits = true;
                            std::vector<int> shift;
                            for (int x : set) {
                                if (!present[c + x]) {
                                    fits = false;
                                    break;
                                }
                                shift.push_back(c + x);
                            }
                            if (fits)
                                add(shift,
                                    cat({power(c), rep(alt({eps(), power(a), power(b)}), k, k)}));
                        }
                    }
                }
        std::vector<std::pair<uint64_t, R>> choices(terms.begin(), terms.end());
        R result;
        if (n <= 18) {
            size_t count = size_t(1) << n;
            const int BIG = 1000000000;
            std::vector<int> cost(count, BIG), parent(count, -1), which(count, -1);
            cost[0] = 0;
            for (size_t mask = 0; mask < count; ++mask) {
                if ((mask & 1023) == 0)
                    budget.check();
                if (cost[mask] == BIG)
                    continue;
                for (size_t j = 0; j < choices.size(); ++j) {
                    size_t to = mask | choices[j].first;
                    if (to == mask)
                        continue;
                    int c = cost[mask] + int(choices[j].second->s.size()) + 1;
                    if (c < cost[to]) {
                        cost[to] = c;
                        parent[to] = int(mask);
                        which[to] = int(j);
                    }
                }
            }
            std::vector<R> v;
            for (size_t x = count - 1; x; x = size_t(parent[x]))
                v.push_back(choices[which[x]].second);
            expandedCache[lengths] = alt(v);
            result = refine(expandedCache[lengths]);
        } else {
            uint64_t missing = (1ULL << n) - 1;
            std::vector<R> v;
            while (missing) {
                budget.check();
                size_t best = 0;
                double score = -1;
                for (size_t j = 0; j < choices.size(); ++j) {
                    double s = double(__builtin_popcountll(choices[j].first & missing)) /
                               (choices[j].second->s.size() + 1);
                    if (s > score) {
                        score = s;
                        best = j;
                    }
                }
                v.push_back(choices[best].second);
                missing &= ~choices[best].first;
            }
            expandedCache[lengths] = alt(v);
            result = refine(expandedCache[lengths]);
        }
        finiteCache[lengths] = result;
        return result;
    }
    void run() {
        int n = int(target.t.size());
        std::vector<int> first(n, -1), path;
        int q = target.start;
        while (first[q] < 0) {
            first[q] = int(path.size());
            path.push_back(q);
            q = target.t[q][digit];
        }
        int mu = first[q], period = int(path.size()) - mu;
        if (period > 80 || mu > 128)
            return;
        auto accepts = [&](int length) {
            int p = length < int(path.size()) ? length : mu + (length - mu) % period;
            return bool(target.accept[path[p]]);
        };
        std::vector<int> prefix, residues;
        for (int i = 0; i < mu; ++i)
            if (accepts(i))
                prefix.push_back(i);
        for (int i = mu; i < mu + period; ++i)
            if (accepts(i))
                residues.push_back(i);
        R cycle = rep(power(period), 0, INF);
        offer(refine(alt({finite(prefix), cat({cycle, finite(residues)})})),
              "Unaere Laengenmengen");
        if (residues.empty() || period < 4)
            return;
        std::vector<int> steps;
        for (int a = 1; a < period; ++a)
            if (period % a == 0)
                steps.push_back(a);
        for (int a : steps)
            for (int b : steps)
                if (a < b && std::gcd(a, b) == 1)
                    for (int c = mu; c < mu + period; ++c) {
                        budget.check();
                        int ka = period / std::gcd(a, period), kb = period / std::gcd(b, period),
                            limit = kb - 1;
                        for (int rows = 0; rows < ka; ++rows) {
                            int row = 0;
                            while (row < kb && accepts(c + rows * a + row * b))
                                ++row;
                            limit = std::min(limit, row - 1);
                            if (limit < 0)
                                break;
                            if ((rows + 1) * (limit + 1) < int(residues.size()))
                                continue;
                            std::set<int> points, mods;
                            for (int i = 0; i <= rows; ++i)
                                for (int j = 0; j <= limit; ++j) {
                                    int value = c + i * a + j * b;
                                    points.insert(value);
                                    mods.insert(value % period);
                                }
                            if (mods.size() != residues.size() || *points.rbegin() > 160)
                                continue;
                            std::vector<int> holes;
                            for (int length = 0; length < *points.rbegin(); ++length)
                                if (accepts(length)) {
                                    bool covered = false;
                                    for (int p : points)
                                        if (p <= length && (length - p) % period == 0) {
                                            covered = true;
                                            break;
                                        }
                                    if (!covered)
                                        holes.push_back(length);
                                }
                            R h = finite(holes);
                            for (int la = 0; la * a <= c; ++la)
                                for (int lb = 0; la * a + lb * b <= c; ++lb) {
                                    budget.check();
                                    int offset = c - la * a - lb * b;
                                    std::vector<R> f{power(offset), rep(power(a), la, la + rows),
                                                     rep(power(b), lb, lb + limit), cycle};
                                    std::vector<int> order{0, 1, 2, 3};
                                    do {
                                        std::vector<R> v;
                                        for (int i : order)
                                            v.push_back(f[i]);
                                        offer(refine(alt({h, cat(v)})), "Modulare Gitter");
                                        auto expanded = expandedCache.find(holes);
                                        if (expanded != expandedCache.end())
                                            offer(refine(alt({expanded->second, cat(v)})),
                                                  "Modulare Gitter + Faktorisierung");
                                    } while (std::next_permutation(order.begin(), order.end()));
                                }
                        }
                    }
    }
};
} // namespace rx
