#pragma once
#include "automata.hpp"

namespace rx {

// A bounded superoptimizer over complete expression contexts. In contrast to
// an equality-only rewrite system, mutations may enlarge or shrink a subtree:
// surrounding factors can make that difference irrelevant. Only a complete
// DFA product proof permits a candidate to be offered to the caller.
struct AlgebraSearch {
    const DFA &target;
    const Budget &budget;
    Limits limits;
    std::function<void(R, const std::string &)> offer;
    std::function<R(R)> refine;
    std::mt19937_64 rng;
    struct Candidate {
        R r;
        std::shared_ptr<DFA> d;
        bool exact = false;
        int mistakes = 0;
    };
    std::vector<std::pair<std::string, bool>> examples;
    std::unordered_set<std::string> exampleWords;
    std::unordered_set<std::string> seen;
    std::vector<R> phrasePowers;
    R best;
    size_t evaluated = 0, proved = 0;

    AlgebraSearch(const DFA &d, const Budget &b, Limits l,
                  std::function<void(R, const std::string &)> o, std::function<R(R)> f,
                  uint64_t seed = 1)
        : target(d), budget(b), limits(l), offer(std::move(o)), refine(std::move(f)), rng(seed) {}

    static R rebuildNode(R r, const std::vector<R> &v) {
        if (r->k == ALT)
            return alt(v);
        if (r->k == CAT)
            return cat(v);
        if (r->k == REP)
            return rep(v[0], r->lo, r->hi);
        return r;
    }
    static R segment(const std::vector<R> &v, size_t first, size_t last) {
        return cat(std::vector<R>(v.begin() + first, v.begin() + last));
    }
    static bool star(R r) {
        return r->k == REP && r->lo == 0 && r->hi == INF;
    }
    void addExample(const std::string &w) {
        if (examples.size() < 640 && exampleWords.insert(w).second)
            examples.push_back({w, matches(target, w)});
    }
    void prepareExamples(R input, const Budget *local = nullptr) {
        // State-reaching prefixes make the test set follow the supplied
        // language, including long corridors missed by tiny random strings.
        std::vector<std::string> prefix(target.t.size());
        std::vector<bool> visited(target.t.size());
        std::vector<int> q{target.start};
        visited[target.start] = true;
        int mask = 0;
        std::function<void(R)> collect = [&](R r) {
            if (r->k == CHAR)
                mask |= r->mask;
            for (R x : r->sub)
                collect(x);
        };
        collect(input);
        std::vector<char> digits;
        for (int c = 0; c < 10; ++c)
            if (mask & (1 << c))
                digits.push_back(char('0' + c));
        addExample("");
        for (size_t i = 0; i < q.size(); ++i) {
            budget.check();
            if (local)
                local->check();
            int p = q[i];
            addExample(prefix[p]);
            for (char c : digits) {
                int next = target.t[p][c - '0'];
                addExample(prefix[p] + c);
                if (!visited[next]) {
                    visited[next] = true;
                    prefix[next] = prefix[p] + c;
                    q.push_back(next);
                }
            }
        }
        if (digits.empty())
            return;
        // The same unseeded grammar is exercised on arbitrary input alphabets.
        for (int i = 0; i < 180; ++i) {
            if (local && (i & 15) == 0)
                local->check();
            std::string w;
            int length = int(rng() % 25);
            for (int j = 0; j < length; ++j)
                w += digits[rng() % digits.size()];
            addExample(w);
        }
    }
    int mistakes(const DFA &d) const {
        int count = 0;
        for (const auto &example : examples)
            count += matches(d, example.first) != example.second;
        return count;
    }
    static std::string shapeKey(R r) {
        std::map<std::pair<I, I>, int> quantifiers;
        int sum = 0, product = 0;
        std::function<void(R)> visit = [&](R n) {
            if (n->k == REP)
                ++quantifiers[{n->lo, n->hi}];
            if (n->k == ALT)
                ++sum;
            if (n->k == CAT)
                ++product;
            for (R child : n->sub)
                visit(child);
        };
        visit(r);
        std::string key = std::to_string(sum) + ":" + std::to_string(product);
        for (const auto &item : quantifiers)
            key += ";" + std::to_string(item.first.first) + "," +
                   std::to_string(item.first.second) + ":" + std::to_string(item.second);
        return key;
    }
    void preparePhrases(R root, const Budget *local = nullptr) {
        phrasePowers.clear();
        // Phrase recombination is useful for larger trees; on tiny trees it
        // would crowd out the cheap local edits with duplicated proposals.
        if (root->s.size() < 60)
            return;
        std::map<std::string, std::pair<int, R>> phrases;
        auto add = [&](R r) {
            if (r->s.size() >= 2 && r->s.size() <= 32) {
                auto &item = phrases[r->s];
                ++item.first;
                item.second = r;
            }
        };
        std::function<void(R)> visit = [&](R r) {
            budget.check();
            if (local)
                local->check();
            add(r);
            if (r->k == CAT)
                for (size_t first = 0; first < r->sub.size(); ++first)
                    for (size_t length = 2; length <= 5 && first + length <= r->sub.size();
                         ++length)
                        add(segment(r->sub, first, first + length));
            for (R child : r->sub)
                visit(child);
        };
        visit(root);
        std::vector<std::pair<int, R>> ranked;
        for (const auto &item : phrases) {
            auto p = item.second;
            int length = int(p.second->s.size());
            ranked.push_back({(p.first - 1) * length + p.first, p.second});
        }
        std::sort(ranked.begin(), ranked.end(), [](const auto &a, const auto &b) {
            return a.first != b.first ? a.first > b.first : better(a.second, b.second);
        });
        for (size_t i = 0; i < std::min(size_t(12), ranked.size()); ++i)
            for (int count = 2; count <= 4; ++count)
                phrasePowers.push_back(rep(ranked[i].second, count, count));
    }
    // These are proposals, not asserted identities. Context equivalence is
    // checked at the complete root, never inferred from the test examples.
    std::vector<R> local(R r) {
        std::vector<R> out;
        if (r->k == EMPTY || r->k == EPS)
            return out;
        if (r->k == CHAR) {
            out.push_back(eps());
            out.push_back(rep(r, 0, 1));
            out.push_back(rep(r, 0, INF));
            out.push_back(rep(r, 1, INF));
        }
        if (r->k == REP) {
            R operand = r->sub[0];
            out.push_back(operand);
            out.push_back(eps());
            for (auto interval : {std::pair<I, I>{0, 1}, {0, INF}, {1, INF}, {1, 1}})
                out.push_back(rep(operand, interval.first, interval.second));
            if (r->lo > 0 && r->lo <= 12)
                out.push_back(rep(operand, r->lo - 1, r->hi));
            if (r->hi != INF && r->hi <= 12) {
                out.push_back(rep(operand, r->lo, r->hi + 1));
                if (r->hi > r->lo)
                    out.push_back(rep(operand, r->lo, r->hi - 1));
            }
            for (int count = 2; count <= 4; ++count) {
                out.push_back(rep(operand, count, count));
                out.push_back(rep(operand, 0, count));
                out.push_back(rep(operand, count, INF));
            }
            if (r->lo == 0 && r->hi == 1 && operand->k == CAT) {
                std::vector<R> padded;
                for (R x : operand->sub)
                    padded.push_back(nullable(x) ? x : rep(x, 0, 1));
                out.push_back(cat(padded));
            }
            if (star(r) && operand->k == CAT && operand->sub.size() <= 10)
                for (size_t i = 1; i < operand->sub.size(); ++i) {
                    R a = segment(operand->sub, 0, i);
                    R b = segment(operand->sub, i, operand->sub.size());
                    out.push_back(alt({eps(), cat({a, rep(cat({b, a}), 0, INF), b})}));
                    // Change the phase of a repeated block, allowing boundary
                    // pieces to be supplied or absorbed by the full context.
                    R rotated = rep(cat({b, a}), 0, INF);
                    out.push_back(cat({rep(a, 0, 1), rotated, rep(b, 0, 1)}));
                    out.push_back(cat({rep(a, 0, 1), rotated}));
                    out.push_back(cat({rotated, rep(b, 0, 1)}));
                }
            if (star(r) && operand->k == ALT && operand->sub.size() <= 6)
                for (size_t i = 0; i < operand->sub.size(); ++i) {
                    auto others = operand->sub;
                    R a = others[i];
                    others.erase(others.begin() + i);
                    R b = alt(others), loop = rep(a, 0, INF);
                    out.push_back(cat({loop, rep(cat({b, loop}), 0, INF)}));
                    out.push_back(cat({rep(cat({loop, b}), 0, INF), loop}));
                }
            if (star(r) && operand->sub.size() <= 8) {
                std::vector<R> generators =
                    operand->k == ALT ? operand->sub : std::vector<R>{operand};
                auto powers = phrasePowers;
                std::shuffle(powers.begin(), powers.end(), rng);
                if (powers.size() > 12)
                    powers.resize(12);
                for (R power : powers) {
                    auto padded = generators;
                    padded.push_back(power);
                    out.push_back(rep(alt(padded), 0, INF));
                    for (size_t i = 0; i < generators.size(); ++i) {
                        auto changed = generators;
                        changed[i] = power;
                        out.push_back(rep(alt(changed), 0, INF));
                    }
                }
            }
        }
        if (r->k == CAT) {
            const auto &v = r->sub;
            for (size_t i = 0; i < v.size(); ++i) {
                auto w = v;
                w.erase(w.begin() + i);
                out.push_back(cat(w));
                if (i + 1 < v.size()) {
                    // Group-wide optionality/closure often exposes a concise
                    // context-equivalent factor that nodewise edits miss.
                    for (int mode = 0; mode < 3; ++mode) {
                        auto grouped = v;
                        grouped[i] = rep(cat({v[i], v[i + 1]}), 0, mode ? INF : 1);
                        if (mode == 2)
                            grouped[i] = rep(cat({v[i], v[i + 1]}), 1, INF);
                        grouped.erase(grouped.begin() + i + 1);
                        out.push_back(cat(grouped));
                    }
                    for (int count = 2; count <= 4; ++count) {
                        auto grouped = v;
                        grouped[i] = rep(cat({v[i], v[i + 1]}), count, count);
                        grouped.erase(grouped.begin() + i + 1);
                        out.push_back(cat(grouped));
                    }
                    if (star(v[i]) && star(v[i + 1])) {
                        auto merged = v;
                        merged[i] = rep(alt({v[i]->sub[0], v[i + 1]->sub[0]}), 0, INF);
                        merged.erase(merged.begin() + i + 1);
                        out.push_back(cat(merged));
                    }
                    if (i + 2 < v.size() && star(v[i]) && star(v[i + 1]) && eq(v[i], v[i + 2])) {
                        auto merged = v;
                        merged[i] = rep(alt({v[i]->sub[0], v[i + 1]->sub[0]}), 0, INF);
                        merged.erase(merged.begin() + i + 1, merged.begin() + i + 3);
                        out.push_back(cat(merged));
                    }
                }
                if (star(v[i]))
                    for (size_t j = i > 2 ? i - 2 : 0; j < v.size() && j <= i + 2; ++j) {
                        if (i == j || v[j]->k != REP)
                            continue;
                        R body = v[j]->sub[0];
                        for (int mode = 0; mode < 3; ++mode) {
                            R padded = mode == 0   ? cat({v[i], body})
                                       : mode == 1 ? cat({body, v[i]})
                                                   : alt({v[i]->sub[0], body});
                            auto moved = v;
                            moved[j] = rep(padded, v[j]->lo, v[j]->hi);
                            out.push_back(cat(moved));
                        }
                    }
                if (v[i]->k == ALT && v[i]->sub.size() <= 5) {
                    std::vector<R> expanded;
                    for (R x : v[i]->sub) {
                        auto w = v;
                        w[i] = x;
                        expanded.push_back(cat(w));
                    }
                    out.push_back(alt(expanded));
                }
            }
        }
        if (r->k == ALT) {
            const auto &v = r->sub;
            for (size_t i = 0; i < v.size(); ++i) {
                auto w = v;
                w.erase(w.begin() + i);
                out.push_back(alt(w));
            }
            // Dropping only epsilon at the wrapper can be compensated by
            // optional factors inside it. This is deliberately contextual.
            std::vector<R> nonempty;
            for (R x : v)
                if (x->k != EPS)
                    nonempty.push_back(x);
            if (nonempty.size() < v.size()) {
                R body = alt(nonempty);
                out.push_back(body);
                if (body->k == CAT) {
                    std::vector<R> padded;
                    for (R x : body->sub)
                        padded.push_back(nullable(x) ? x : rep(x, 0, 1));
                    out.push_back(cat(padded));
                }
            }
        }
        return out;
    }
    void mutations(R root, std::vector<R> &out, size_t cap, int depth = 0) {
        struct Site {
            std::vector<size_t> path;
            R r;
            std::vector<R> edits;
        };
        std::vector<Site> sites;
        std::vector<size_t> path;
        std::function<void(R, int)> visit = [&](R r, int level) {
            if (level > 16 || sites.size() >= 160)
                return;
            auto edits = local(r);
            std::shuffle(edits.begin(), edits.end(), rng);
            sites.push_back({path, r, std::move(edits)});
            for (size_t i = 0; i < r->sub.size(); ++i) {
                path.push_back(i);
                visit(r->sub[i], level + 1);
                path.pop_back();
            }
        };
        visit(root, depth);
        std::shuffle(sites.begin(), sites.end(), rng);
        std::function<R(R, const std::vector<size_t> &, size_t, R)> replace =
            [&](R r, const std::vector<size_t> &p, size_t i, R child) -> R {
            if (i == p.size())
                return child;
            auto v = r->sub;
            v[p[i]] = replace(v[p[i]], p, i + 1, child);
            return rebuildNode(r, v);
        };
        for (size_t edit = 0; out.size() < cap; ++edit) {
            bool any = false;
            for (const auto &site : sites)
                if (edit < site.edits.size()) {
                    any = true;
                    if (!eq(site.r, site.edits[edit]))
                        out.push_back(replace(root, site.path, 0, site.edits[edit]));
                    if (out.size() >= cap)
                        break;
                }
            if (!any)
                break;
        }
    }
    R run(R original, double seconds = 1.0, size_t maximumCandidates = 10000) {
        best = original;
        examples.clear();
        exampleWords.clear();
        seen.clear();
        evaluated = proved = 0;
        Budget localBudget(seconds);
        try {
            prepareExamples(original, &localBudget);
            preparePhrases(original, &localBudget);
            std::vector<Candidate> beam;
            size_t retainedStates = 0;
            auto evaluate = [&](R r) {
                budget.check();
                localBudget.check();
                if (evaluated >= maximumCandidates || r->k == EMPTY ||
                    r->s.size() > best->s.size() + 24 || !seen.insert(r->s).second)
                    return;
                ++evaluated;
                try {
                    auto d = std::make_shared<DFA>(compile(r, limits, &localBudget));
                    Comparison proof = compare(target, *d, limits, &localBudget);
                    bool exact = proof.equal;
                    if (exact) {
                        ++proved;
                        if (better(r, best)) {
                            best = r;
                            offer(r, "Kontextuelle Superoptimierung");
                        }
                    } else
                        addExample(proof.witness);
                    if (retainedStates + d->t.size() <= 200000) {
                        retainedStates += d->t.size();
                        beam.push_back({r, d, exact, mistakes(*d)});
                    }
                } catch (const Limit &) {
                    localBudget.check();
                    budget.check();
                }
            };
            evaluate(original);
            if (R normalized = refine(original); !eq(normalized, original))
                evaluate(normalized);
            std::unordered_set<std::string> expanded;
            for (int round = 0; !beam.empty() && evaluated < maximumCandidates; ++round) {
                budget.check();
                localBudget.check();
                for (auto &candidate : beam)
                    candidate.mistakes = mistakes(*candidate.d);
                auto score = [](const Candidate &a) {
                    return double(a.r->s.size()) + 5.0 * a.mistakes;
                };
                std::stable_sort(beam.begin(), beam.end(),
                                 [&](const Candidate &a, const Candidate &b) {
                                     if (a.exact != b.exact)
                                         return a.exact;
                                     double x = score(a), y = score(b);
                                     return x != y ? x < y : better(a.r, b.r);
                                 });
                // Reserve slots for approximate candidates, so one longer
                // equality path cannot prevent a useful two-edit repair.
                std::vector<Candidate> chosen;
                size_t equal = 0, approximate = 0;
                std::unordered_set<std::string> shapes;
                for (auto &candidate : beam) {
                    if (expanded.count(candidate.r->s))
                        continue;
                    bool take = false;
                    if (candidate.exact) {
                        std::string shape = shapeKey(candidate.r);
                        bool novel = shapes.insert(shape).second;
                        take = equal < 6 || (equal < 12 && novel);
                        if (take)
                            ++equal;
                    } else
                        take = approximate++ < 16;
                    if (take)
                        chosen.push_back(candidate);
                }
                beam = chosen;
                retainedStates = 0;
                for (const auto &candidate : beam)
                    retainedStates += candidate.d->t.size();
                bool any = false;
                for (auto &candidate : chosen) {
                    if (!expanded.insert(candidate.r->s).second)
                        continue;
                    any = true;
                    std::vector<R> variants;
                    mutations(candidate.r, variants, 192);
                    // Cheapest edits first, but reserve some increasing moves.
                    std::stable_sort(variants.begin(), variants.end(), better);
                    for (R variant : variants) {
                        evaluate(variant);
                        R normalized = refine(variant);
                        if (!eq(normalized, variant))
                            evaluate(normalized);
                        if (evaluated >= maximumCandidates)
                            break;
                    }
                    if (evaluated >= maximumCandidates)
                        break;
                }
                if (!any)
                    break;
            }
        } catch (const Limit &) {
            budget.check();
        }
        return best;
    }
};
} // namespace rx
