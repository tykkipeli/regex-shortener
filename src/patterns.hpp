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
    // Every word avoiding w as a subsequence is a safe candidate for a
    // language with finite exceptions. Derive the expression from w, rather
    // than guessing a specific digit pattern or copying a known solution.
    R avoidSubsequence(const std::string &w, int mask) {
        if (w.empty())
            return zero();
        std::vector<R> parts;
        for (size_t i = 0; i < w.size(); ++i) {
            int c = w[i] - '0', other = mask & ~(1 << c);
            R loop = other ? rep(chars(other), 0, INF) : eps();
            if (i && w[i - 1] == w[i])
                loop = rep(cat({chars(1 << c), loop}), 0, 1);
            parts.push_back(loop);
        }
        return refine(cat(parts));
    }
    std::vector<R> exceptionCandidates(int threshold, int mask) {
        // Enumerate only paths which can still reach a rejection. Accepted
        // universal sinks are pruned, so a lone long exception costs O(n*k).
        int n = int(target.t.size());
        if (threshold <= 0 || threshold > 96)
            return {};
        std::vector<std::vector<uint8_t>> bad(threshold, std::vector<uint8_t>(n));
        for (int p = 0; p < n; ++p)
            bad[0][p] = !target.accept[p];
        for (int len = 1; len < threshold; ++len)
            for (int p = 0; p < n; ++p) {
                bad[len][p] = bad[0][p];
                for (int c = 0; c < 10; ++c)
                    if (mask & (1 << c))
                        bad[len][p] |= bad[len - 1][target.t[p][c]];
            }
        std::vector<R> out;
        std::string word;
        int visits = 0, exceptions = 0;
        std::function<void(int)> visit = [&](int p) {
            budget.check();
            if (++visits > 20000 || exceptions >= 64 || int(word.size()) >= threshold ||
                !bad[threshold - 1 - word.size()][p])
                return;
            if (!target.accept[p]) {
                ++exceptions;
                R r = avoidSubsequence(word, mask);
                if (r->k != EMPTY &&
                    compare(compile(r, limits, &budget), target, limits, &budget, true).equal)
                    out.push_back(r);
            }
            for (int c = 0; c < 10; ++c)
                if (mask & (1 << c)) {
                    word += char('0' + c);
                    visit(target.t[p][c]);
                    word.pop_back();
                }
        };
        if (threshold)
            visit(target.start);
        return out;
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
        auto exceptions = exceptionCandidates(threshold, mask);
        for (R r : exceptions) {
            R candidate = refine(alt({tail, r}));
            if (exact(candidate))
                offer(candidate, "Ausnahmen und Teilfolgen");
        }
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
        cover.insert(cover.end(), exceptions.begin(), exceptions.end());
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
    void semanticCovers(int mask) {
        if (!mask || target.t.size() > 40)
            return;
        double remain = std::chrono::duration<double>(budget.end - Budget::Clock::now()).count();
        Budget local(std::min(0.6, remain * 0.25));
        auto check = [&]() {
            budget.check();
            local.check();
        };
        struct Term {
            R r;
            DFA d;
        };
        std::vector<Term> terms;
        std::unordered_set<std::string> texts, languages;
        auto add = [&](R r) {
            check();
            if (r->k == EMPTY || r->s.size() > 80 || !texts.insert(r->s).second)
                return;
            try {
                DFA d = compile(r, limits, &local);
                if (!compare(d, target, limits, &local, true).equal)
                    return;
                if (languages.insert(key(d)).second)
                    terms.push_back({r, std::move(d)});
            } catch (const Limit &) {
                check();
            }
        };
        try {
            // Classes come from transition behavior, with no distinguished
            // alphabet or decimal divisibility rule in the search engine.
            std::set<int> masks{mask};
            for (int c = 0; c < 10; ++c)
                if (mask & (1 << c))
                    masks.insert(1 << c);
            for (const auto &row : target.t) {
                std::map<int, int> grouped;
                for (int c = 0; c < 10; ++c)
                    if (mask & (1 << c))
                        grouped[row[c]] |= 1 << c;
                for (auto [q, m] : grouped) {
                    (void)q;
                    masks.insert(m);
                    if (mask & ~m)
                        masks.insert(mask & ~m);
                }
            }
            std::vector<int> classes(masks.begin(), masks.end());
            std::sort(classes.begin(), classes.end(),
                      [](int a, int b) { return better(chars(a), chars(b)); });
            if (classes.size() > 24)
                classes.resize(24);
            R all = rep(chars(mask), 0, INF);
            add(eps());
            for (int m : classes) {
                R c = chars(m);
                for (int k = 1; k <= 4; ++k) {
                    R word = rep(c, k, k);
                    add(word);
                    add(cat({all, word, all}));
                    add(cat({word, all}));
                    add(cat({all, word}));
                }
            }
            for (int a : classes)
                for (int b : classes) {
                    add(cat({chars(a), all, chars(b)}));
                    add(cat({rep(cat({chars(a), chars(b)}), 0, INF), rep(chars(a), 0, 1)}));
                }
            for (int m : classes) {
                int other = mask & ~m;
                if (!other)
                    continue;
                R rest = rep(chars(other), 0, INF);
                R block = cat({chars(m), rest});
                for (int k = 1; k <= 6; ++k) {
                    add(cat({rest, rep(block, k, k)}));
                    add(cat({rest, rep(block, k, INF)}));
                    R multiple = rep(rep(block, k, k), 0, INF);
                    add(cat({rest, multiple}));
                    if (k <= 3)
                        for (int residue = 1; residue < k; ++residue)
                            add(cat({rest, multiple, rep(block, residue, residue)}));
                }
            }
            // Short word factors on both sides of an unrestricted middle.
            // This explores position constraints beyond a single edge digit;
            // all proposed sublanguages are independently inclusion-checked.
            std::vector<R> boundaries;
            for (int a : classes) {
                boundaries.push_back(chars(a));
                for (int b : classes) {
                    R r = cat({chars(a), chars(b)});
                    if (r->s.size() <= 8)
                        boundaries.push_back(r);
                }
            }
            std::sort(boundaries.begin(), boundaries.end(), better);
            for (R a : boundaries)
                for (R b : boundaries)
                    add(cat({a, all, b}));
        } catch (const Limit &) {
            // Keep the proved safe terms already collected when this small
            // discovery budget expires. Global GNFA search still gets time.
            budget.check();
        }
        if (terms.empty())
            return;
        // Counterexample-guided set cover. Sampling chooses promising terms;
        // a DFA product supplies missing words until coverage is proved.
        std::vector<std::string> samples;
        std::vector<std::string> frontier{std::string()};
        for (int depth = 0; depth <= 6 && samples.size() < 256; ++depth) {
            std::vector<std::string> next;
            for (const auto &w : frontier) {
                if (matches(target, w))
                    samples.push_back(w);
                if (next.size() < 512)
                    for (int c = 0; c < 10; ++c)
                        if (mask & (1 << c))
                            next.push_back(w + char('0' + c));
            }
            frontier = std::move(next);
        }
        std::vector<R> chosen;
        std::vector<bool> used(terms.size());
        // Cover all sample words that this candidate bank can express.
        for (;;) {
            budget.check();
            int best = -1;
            double score = 0;
            for (size_t i = 0; i < terms.size(); ++i)
                if (!used[i]) {
                    int gain = 0;
                    for (const auto &w : samples)
                        gain += matches(terms[i].d, w);
                    double s = double(gain) / (terms[i].r->s.size() + 1);
                    if (s > score) {
                        score = s;
                        best = int(i);
                    }
                }
            if (best < 0)
                break;
            chosen.push_back(terms[best].r);
            used[best] = true;
            samples.erase(
                std::remove_if(samples.begin(), samples.end(),
                               [&](const std::string &w) { return matches(terms[best].d, w); }),
                samples.end());
        }
        for (size_t iteration = 0; iteration <= terms.size(); ++iteration) {
            budget.check();
            R r = refine(alt(chosen));
            Comparison proof;
            try {
                proof = compare(target, compile(r, limits, &budget), limits, &budget, true);
            } catch (const Limit &) {
                budget.check();
                return;
            }
            if (proof.equal) {
                offer(r, "Automatengesteuerte Ueberdeckung");
                return;
            }
            int best = -1;
            for (size_t i = 0; i < terms.size(); ++i)
                if (!used[i] && matches(terms[i].d, proof.witness) &&
                    (best < 0 || better(terms[i].r, terms[best].r)))
                    best = int(i);
            if (best < 0)
                return;
            chosen.push_back(terms[best].r);
            used[best] = true;
        }
    }
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
        if (__builtin_popcount(unsigned(mask)) > 1) {
            int digit = __builtin_ctz(unsigned(mask));
            bool lengthOnly = true;
            for (const auto &row : target.t)
                for (int c = 0; c < 10; ++c)
                    if ((mask & (1 << c)) && row[c] != row[digit])
                        lengthOnly = false;
            if (lengthOnly) {
                // Quotient by the indistinguishable alphabet, optimize unary
                // length arithmetic, then substitute the entire class back.
                DFA unary = target;
                int dead = int(unary.t.size());
                unary.t.push_back({});
                unary.accept.push_back(0);
                for (auto &row : unary.t)
                    for (int c = 0; c < 10; ++c)
                        if (c != digit)
                            row[c] = dead;
                unary.t[dead].fill(dead);
                unary = minimize(unary, &budget);
                std::function<R(R)> expand = [&](R r) -> R {
                    if (r->k == CHAR)
                        return chars(mask);
                    if (r->k == REP)
                        return rep(expand(r->sub[0]), r->lo, r->hi);
                    if (r->k == CAT || r->k == ALT) {
                        std::vector<R> v;
                        for (R child : r->sub)
                            v.push_back(expand(child));
                        return r->k == CAT ? cat(v) : alt(v);
                    }
                    return r;
                };
                UnarySearch search(unary, budget, digit, refine, [&](R r, const std::string &) {
                    offer(refine(expand(r)), "Alphabetquotient und Laengen");
                });
                search.run();
                return;
            }
        }
        if (__builtin_popcount(unsigned(mask)) == 1) {
            UnarySearch u{target, budget, __builtin_ctz(unsigned(mask)), refine, offer};
            u.run();
            return;
        }
        noEqualNeighbors(mask);
        cofinite(original, mask);
        semanticCovers(mask);
    }
};
} // namespace rx
