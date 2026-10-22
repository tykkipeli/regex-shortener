#pragma once
#include "automata.hpp"
#include "catalog.hpp"
#include "patterns.hpp"
namespace rx {
inline std::vector<R> factors(R r) {
    if (r->k == EPS)
        return {};
    if (r->k == CAT)
        return r->sub;
    return {r};
}
inline R slice(const std::vector<R> &v, size_t a, size_t b) {
    return cat(std::vector<R>(v.begin() + a, v.begin() + b));
}
inline R factorOnce(R r, bool suffix) {
    if (r->k != ALT)
        return r;
    std::map<std::string, std::vector<size_t>> groups;
    std::vector<std::vector<R>> v;
    for (auto x : r->sub)
        v.push_back(factors(x));
    for (size_t i = 0; i < v.size(); ++i) {
        std::string key;
        for (size_t k = 0; k < std::min(size_t(16), v[i].size()); ++k) {
            key += (suffix ? v[i][v[i].size() - 1 - k] : v[i][k])->s;
            key += '\x1f';
            groups[key].push_back(i);
        }
    }
    R best = r;
    for (auto &[unused, ids] : groups)
        if (ids.size() >= 2) {
            size_t n = v[ids[0]].size();
            for (auto i : ids)
                n = std::min(n, v[i].size());
            size_t common = 0;
            for (; common < n; ++common) {
                R a = v[ids[0]][suffix ? v[ids[0]].size() - 1 - common : common];
                bool same = true;
                for (auto i : ids)
                    if (!eq(a, v[i][suffix ? v[i].size() - 1 - common : common])) {
                        same = false;
                        break;
                    }
                if (!same)
                    break;
            }
            std::vector<R> rest, outer;
            for (auto i : ids)
                rest.push_back(suffix ? slice(v[i], 0, v[i].size() - common)
                                      : slice(v[i], common, v[i].size()));
            R edge = suffix ? slice(v[ids[0]], v[ids[0]].size() - common, v[ids[0]].size())
                            : slice(v[ids[0]], 0, common);
            R merged = suffix ? cat({alt(rest), edge}) : cat({edge, alt(rest)});
            for (size_t i = 0; i < v.size(); ++i)
                if (std::find(ids.begin(), ids.end(), i) == ids.end())
                    outer.push_back(r->sub[i]);
            outer.push_back(merged);
            R candidate = alt(outer);
            if (better(candidate, best))
                best = candidate;
        }
    return best;
}
inline R factorPowers(R r, bool suffix) {
    if (r->k != ALT)
        return r;
    struct Term {
        std::vector<R> v;
        R base;
        I lo, hi;
    };
    std::vector<Term> terms;
    std::map<std::string, std::vector<size_t>> groups;
    for (auto x : r->sub) {
        auto v = factors(x);
        if (v.empty()) {
            terms.push_back({v, eps(), 0, 0});
            continue;
        }
        R f = suffix ? v.back() : v.front(), base = f->k == REP ? f->sub[0] : f;
        I lo = f->k == REP ? f->lo : 1, hi = f->k == REP ? f->hi : 1;
        if (lo > 0)
            groups[base->s].push_back(terms.size());
        terms.push_back({v, base, lo, hi});
    }
    R best = r;
    for (auto &[unused, ids] : groups)
        if (ids.size() > 1) {
            I common = MAX_COUNT;
            for (auto i : ids)
                common = std::min(common, terms[i].lo);
            std::vector<R> rest, outer;
            for (auto i : ids) {
                auto t = terms[i];
                R remain = rep(t.base, t.lo - common, t.hi == INF ? INF : t.hi - common);
                if (suffix)
                    t.v.back() = remain;
                else
                    t.v.front() = remain;
                rest.push_back(cat(t.v));
            }
            R edge = rep(terms[ids[0]].base, common, common),
              merged = suffix ? cat({alt(rest), edge}) : cat({edge, alt(rest)});
            for (size_t i = 0; i < terms.size(); ++i)
                if (std::find(ids.begin(), ids.end(), i) == ids.end())
                    outer.push_back(r->sub[i]);
            outer.push_back(merged);
            R c = alt(outer);
            if (better(c, best))
                best = c;
        }
    return best;
}
inline std::pair<R, I> primitivePower(R r) {
    if (r->k != CAT)
        return {r, 1};
    auto v = r->sub;
    size_t n = v.size();
    for (size_t period = 1; period * 2 <= n; ++period)
        if (n % period == 0) {
            bool ok = true;
            for (size_t i = period; i < n; ++i)
                if (!eq(v[i], v[i % period])) {
                    ok = false;
                    break;
                }
            if (ok)
                return {slice(v, 0, period), I(n / period)};
        }
    return {r, 1};
}
inline void starGenerators(R r, std::vector<R> &out) {
    if (r->k == EPS || r->k == EMPTY)
        return;
    if (r->k == ALT) {
        for (auto x : r->sub)
            starGenerators(x, out);
        return;
    }
    if (r->k == REP && r->lo <= 1 && (r->hi == INF || r->hi >= 1)) {
        starGenerators(r->sub[0], out);
        return;
    }
    if (r->k == CAT && nullable(r)) {
        for (auto x : r->sub)
            starGenerators(x, out);
        return;
    }
    out.push_back(r);
}
// A syntactic union decomposition U = A | B. Character classes are split
// by mask difference because the normal form deliberately merges 0|[1-9]
// into [0-9]. No approximation of a language difference is used here.
inline R unionRemainder(R universe, R part) {
    if (eq(universe, part))
        return zero();
    auto terms = [](R x) { return x->k == ALT ? x->sub : std::vector<R>{x}; };
    auto source = terms(universe), wanted = terms(part);
    int sourceMask = 0, wantedMask = 0;
    std::map<std::string, R> other;
    for (auto x : source) {
        if (x->k == CHAR)
            sourceMask |= x->mask;
        else
            other.emplace(x->s, x);
    }
    for (auto x : wanted) {
        if (x->k == CHAR)
            wantedMask |= x->mask;
        else {
            auto it = other.find(x->s);
            if (it == other.end())
                return {};
            other.erase(it);
        }
    }
    if (wantedMask & ~sourceMask)
        return {};
    std::vector<R> rest;
    if (int mask = sourceMask & ~wantedMask)
        rest.push_back(chars(mask));
    for (auto &[text, x] : other)
        rest.push_back(x);
    return alt(rest);
}
inline R replaceFactors(const std::vector<R> &v, size_t first, size_t last, R replacement) {
    return cat({slice(v, 0, first), replacement, slice(v, last, v.size())});
}
inline std::vector<R> boundaryVariants(R r) {
    std::vector<R> out;
    if (r->k != CAT)
        return out;
    const auto &v = r->sub;
    for (size_t i = 0; i < v.size(); ++i) {
        R loop = v[i];
        if (loop->k != REP || loop->hi != INF)
            continue;
        R universe = loop->sub[0];
        for (bool suffix : {false, true}) {
            size_t available = suffix ? v.size() - i - 1 : i;
            // Candidate widths come from union terms, rather than a fixed
            // maximum word length. This also handles long block alphabets.
            std::set<size_t> widths;
            if (available)
                widths.insert(1);
            auto terms = universe->k == ALT ? universe->sub : std::vector<R>{universe};
            for (auto term : terms) {
                size_t width = factors(term).size();
                if (width && width <= available)
                    widths.insert(width);
            }
            for (size_t width : widths) {
                size_t first = suffix ? i + 1 : i - width, last = suffix ? i + 1 + width : i;
                R edge = slice(v, first, last);
                R rest = unionRemainder(universe, edge);
                if (rest) {
                    // A(A|B)* = (AB*)+ and (A|B)*A = (B*A)+.
                    // For {m,}, retain m copies on the opposite side.
                    R block =
                        suffix ? cat({rep(rest, 0, INF), edge}) : cat({edge, rep(rest, 0, INF)});
                    R copies = rep(universe, loop->lo, loop->lo);
                    R replacement = suffix ? cat({copies, rep(block, 1, INF)})
                                           : cat({rep(block, 1, INF), copies});
                    out.push_back(
                        replaceFactors(v, suffix ? i : first, suffix ? last : i + 1, replacement));
                }
                // A{m,n}(A|B)* = A{m}(A|B)*. The analogous suffix
                // rule is valid too. Keep mandatory copies when m > 1.
                if (width == 1 && edge->k == REP && loop->lo == 0) {
                    R base = edge->sub[0], tail = unionRemainder(universe, base);
                    if (!tail)
                        continue;
                    R replacement;
                    if (edge->lo == 0)
                        replacement = loop;
                    else {
                        R block = suffix ? cat({rep(tail, 0, INF), base})
                                         : cat({base, rep(tail, 0, INF)});
                        R copies = rep(base, edge->lo - 1, edge->lo - 1);
                        replacement = suffix ? cat({rep(block, 1, INF), copies})
                                             : cat({copies, rep(block, 1, INF)});
                    }
                    out.push_back(
                        replaceFactors(v, suffix ? i : first, suffix ? last : i + 1, replacement));
                }
            }
        }
    }
    return out;
}
inline std::vector<int> classHints(R r) {
    std::set<int> masks;
    std::function<void(R)> visit = [&](R x) {
        if (x->k == CHAR)
            masks.insert(x->mask);
        for (auto y : x->sub)
            visit(y);
    };
    visit(r);
    std::vector<int> original(masks.begin(), masks.end());
    for (int a : original)
        for (int b : original)
            if (int m = a & ~b)
                masks.insert(m);
    int alphabetMask = alphabet(r);
    for (int c = 0; c < 10; ++c)
        if (alphabetMask & (1 << c))
            masks.insert(1 << c);
    std::vector<int> hints(masks.begin(), masks.end());
    std::sort(hints.begin(), hints.end(), [](int a, int b) {
        auto x = charset(a), y = charset(b);
        return x.size() != y.size() ? x.size() < y.size() : x < y;
    });
    return hints;
}
inline R foldWholePower(R r) {
    auto [base, count] = primitivePower(r);
    if (count > 1) {
        R c = rep(base, count, count);
        if (better(c, r))
            return c;
    }
    return r;
}
inline std::vector<R> closureVariants(R r) {
    std::vector<R> out;
    if (r->k == REP && r->lo == 0 && r->hi == INF && r->sub[0]->k == ALT) {
        const auto &terms = r->sub[0]->sub;
        if (terms.size() <= 8)
            for (size_t b = 0; b < terms.size(); ++b) {
                auto rest = terms;
                rest.erase(rest.begin() + b);
                R pad = rep(alt(rest), 0, INF);
                for (bool front : {false, true}) {
                    auto v = terms;
                    v[b] = foldWholePower(front ? cat({pad, terms[b]}) : cat({terms[b], pad}));
                    out.push_back(rep(alt(v), 0, INF));
                }
            }
        if (terms.size() <= 8)
            for (size_t a = 0; a < terms.size(); ++a)
                for (size_t b = 0; b < terms.size(); ++b)
                    if (a != b) {
                        R pad = rep(terms[a], 0, INF);
                        for (bool front : {false, true}) {
                            R x =
                                foldWholePower(front ? cat({pad, terms[b]}) : cat({terms[b], pad}));
                            auto v = terms;
                            v[b] = x;
                            out.push_back(rep(alt(v), 0, INF));
                        }
                    }
    }
    if (r->k == CAT) {
        auto v = r->sub;
        for (size_t i = 0; i < v.size(); ++i)
            if (v[i]->k == ALT && v[i]->sub.size() <= 8) {
                for (bool front : {false, true}) {
                    if (front ? i == 0 : i + 1 == v.size())
                        continue;
                    R pad = v[front ? i - 1 : i + 1];
                    if (pad->k != REP || pad->lo != 0 || pad->hi != INF)
                        continue;
                    for (size_t j = 0; j < v[i]->sub.size(); ++j) {
                        auto terms = v[i]->sub;
                        terms[j] =
                            foldWholePower(front ? cat({pad, terms[j]}) : cat({terms[j], pad}));
                        auto w = v;
                        w[i] = alt(terms);
                        out.push_back(cat(w));
                    }
                }
            }
    }
    return out;
}
inline R structural(R r, int depth = 0) {
    if (depth > 100)
        return r;
    std::vector<R> v;
    for (auto x : r->sub)
        v.push_back(structural(x, depth + 1));
    r = rebuild(r, v);
    if (r->k == ALT) {
        // Equal bases with adjacent exponent intervals: X|XX|XXX -> X{1,3}.
        v = r->sub;
        bool changed = true;
        while (changed) {
            changed = false;
            for (size_t i = 0; i < v.size() && !changed; ++i)
                for (size_t j = i + 1; j < v.size(); ++j) {
                    R a = v[i]->k == REP ? v[i]->sub[0] : v[i],
                      b = v[j]->k == REP ? v[j]->sub[0] : v[j];
                    auto [pa, na] = primitivePower(a);
                    auto [pb, nb] = primitivePower(b);
                    if (na > 1 && v[i]->k != REP)
                        a = pa;
                    if (nb > 1 && v[j]->k != REP)
                        b = pb;
                    if (!eq(a, b))
                        continue;
                    I al = v[i]->k == REP ? v[i]->lo : na, ah = v[i]->k == REP ? v[i]->hi : na,
                      bl = v[j]->k == REP ? v[j]->lo : nb, bh = v[j]->k == REP ? v[j]->hi : nb;
                    if ((ah == INF || bl <= ah + 1) && (bh == INF || al <= bh + 1)) {
                        R m = rep(a, std::min(al, bl),
                                  ah == INF || bh == INF ? INF : std::max(ah, bh));
                        if (m->s.size() <= v[i]->s.size() + v[j]->s.size() + 1) {
                            v[i] = m;
                            v.erase(v.begin() + j);
                            changed = true;
                            break;
                        }
                    }
                }
        }
        r = alt(v);
        for (int k = 0; k < 12; ++k) {
            R a = factorOnce(r, false), b = factorOnce(r, true), next = better(a, b) ? a : b;
            for (bool suffix : {false, true}) {
                R c = factorPowers(r, suffix);
                if (better(c, next))
                    next = c;
            }
            if (!better(next, r))
                break;
            r = structural(next, depth + 1);
        }
    }
    if (r->k == REP && r->lo == 0 && r->hi == INF) {
        std::vector<R> generators;
        starGenerators(r->sub[0], generators);
        R c = rep(alt(generators), 0, INF);
        if (better(c, r))
            r = c;
    }
    if (r->k == CAT) {
        v = r->sub;
        // Arden-style closure identities in both directions:
        // A*(BA*)* = (A*B)*A* = (A|B)*.
        for (size_t i = 0; i + 1 < v.size(); ++i) {
            auto star = [](R x) { return x->k == REP && x->lo == 0 && x->hi == INF; };
            R a = v[i], b = v[i + 1], candidate;
            if (star(a) && star(b) && b->sub[0]->k == CAT) {
                auto f = b->sub[0]->sub;
                if (eq(f.back(), a))
                    candidate = rep(alt({a->sub[0], slice(f, 0, f.size() - 1)}), 0, INF);
            }
            if (star(a) && star(b) && a->sub[0]->k == CAT) {
                auto f = a->sub[0]->sub;
                if (eq(f.front(), b))
                    candidate = rep(alt({b->sub[0], slice(f, 1, f.size())}), 0, INF);
            }
            if (candidate) {
                auto w = v;
                w[i] = candidate;
                w.erase(w.begin() + i + 1);
                R c = cat(w);
                if (better(c, r)) {
                    r = c;
                    v = factors(r);
                    i = 0;
                }
            }
        }
        size_t n = v.size();
        if (n <= 160) {
            // Dynamic programming discovers repeated blocks of any period.
            std::vector<R> dp(n + 1);
            dp[n] = eps();
            for (size_t ii = n; ii-- > 0;) {
                dp[ii] = cat({v[ii], dp[ii + 1]});
                for (size_t len = 1; ii + 2 * len <= n; ++len) {
                    size_t k = 1;
                    while (ii + (k + 1) * len <= n) {
                        bool same = true;
                        for (size_t j = 0; j < len; ++j)
                            if (!eq(v[ii + j], v[ii + k * len + j])) {
                                same = false;
                                break;
                            }
                        if (!same)
                            break;
                        ++k;
                        R c = cat({rep(slice(v, ii, ii + len), I(k), I(k)), dp[ii + k * len]});
                        if (better(c, dp[ii]))
                            dp[ii] = c;
                    }
                }
            }
            if (better(dp[0], r))
                r = dp[0];
        }
        // Apply cheap proven identities before the heuristic search. Accept
        // strictly improving forms only, so this cannot create rewrite cycles.
        for (int pass = 0; pass < 8 && r->k == CAT; ++pass) {
            R next = r;
            for (auto x : boundaryVariants(r))
                if (better(x, next))
                    next = x;
            if (!better(next, r))
                break;
            r = next;
        }
    }
    for (int pass = 0; pass < 4; ++pass) {
        R next = r;
        for (auto c : closureVariants(r))
            if (better(c, next))
                next = c;
        if (!better(next, r))
            break;
        r = next;
    }
    return r;
}
inline std::vector<R> localVariants(R r, const std::vector<int> &hints = {}) {
    std::vector<R> out = boundaryVariants(r);
    auto closure = closureVariants(r);
    out.insert(out.end(), closure.begin(), closure.end());
    if (r->k == REP && r->lo == 0 && r->hi == INF && r->sub[0]->k == ALT) {
        auto v = r->sub[0]->sub;
        for (size_t i = 0; i < v.size() && i < 6; ++i) {
            auto w = v;
            R a = w[i];
            w.erase(w.begin() + i);
            R b = alt(w), as = rep(a, 0, INF);
            out.push_back(cat({as, rep(cat({b, as}), 0, INF)}));
            out.push_back(cat({rep(cat({as, b}), 0, INF), as}));
        }
    }
    if (r->k == REP && r->lo == 0 && r->hi == INF && r->sub[0]->k == CHAR) {
        int universe = r->sub[0]->mask;
        for (int mask : hints) {
            if ((mask & ~universe) || mask == universe)
                continue;
            R a = rep(chars(mask), 0, INF), b = chars(universe & ~mask);
            out.push_back(cat({a, rep(cat({b, a}), 0, INF)}));
            out.push_back(cat({rep(cat({a, b}), 0, INF), a}));
        }
    }
    if (r->k == CAT) {
        auto v = r->sub;
        for (size_t i = 0; i < v.size(); ++i) {
            R x = v[i];
            if (x->k == ALT && x->sub.size() <= 5) {
                // A(B|C)D -> ABD|ACD; the expanded form may expose an
                // alternative factoring in the next search step.
                std::vector<R> sum;
                for (auto y : x->sub) {
                    auto w = v;
                    w[i] = y;
                    sum.push_back(cat(w));
                }
                out.push_back(alt(sum));
            }
            if (x->k == REP && x->lo == 0 && x->hi == INF && x->sub[0]->k == CAT) {
                auto f = x->sub[0]->sub;
                if (i + 1 < v.size() && eq(f.front(), v[i + 1])) {
                    auto w = v;
                    R a = f.front(), b = slice(f, 1, f.size());
                    w[i] = a;
                    w[i + 1] = rep(cat({b, a}), 0, INF);
                    out.push_back(cat(w));
                }
                if (i > 0 && eq(f.back(), v[i - 1])) {
                    auto w = v;
                    R a = f.back(), b = slice(f, 0, f.size() - 1);
                    w[i - 1] = rep(cat({a, b}), 0, INF);
                    w[i] = a;
                    out.push_back(cat(w));
                }
            }
        }
    }
    return out;
}
inline void contextualVariants(R r, std::vector<R> &out, size_t cap = 100, int depth = 0,
                               const std::vector<int> &hints = {}) {
    if (out.size() >= cap || depth > 12)
        return;
    for (auto c : localVariants(r, hints)) {
        if (out.size() >= cap)
            break;
        out.push_back(c);
    }
    for (size_t i = 0; i < r->sub.size() && out.size() < cap; ++i) {
        std::vector<R> inner;
        contextualVariants(r->sub[i], inner, std::min(size_t(15), cap - out.size()), depth + 1,
                           hints);
        for (auto x : inner) {
            auto v = r->sub;
            v[i] = x;
            out.push_back(rebuild(r, v));
            if (out.size() >= cap)
                break;
        }
    }
}
struct Options {
    double seconds = 15;
    uint64_t seed = 1;
    int beam = 12, synthesis = 20000;
    Limits limits;
    std::shared_ptr<const LanguageCatalog> catalog;
    bool quiet = false;
};
struct Stats {
    size_t verified = 0, rejected = 0, eliminations = 0, synthesized = 0, catalogHits = 0;
};
struct Optimizer {
    Options opt;
    Budget budget;
    std::mt19937_64 rng;
    R best;
    DFA target;
    std::string targetKey;
    std::vector<int> hints;
    Stats stats;
    bool verified = false;
    size_t cachedStates = 0;
    std::unordered_map<std::string, std::shared_ptr<DFA>> cache;
    std::unordered_set<std::string> failed;
    std::unordered_map<std::string, R> semanticBank;
    explicit Optimizer(Options o) : opt(o), budget(o.seconds), rng(o.seed) {}
    std::shared_ptr<DFA> automaton(R r) {
        auto i = cache.find(r->s);
        if (i != cache.end())
            return i->second;
        if (failed.count(r->s))
            return {};
        try {
            auto d = std::make_shared<DFA>(compile(r, opt.limits, &budget));
            if (cache.size() < 12000 && cachedStates + d->t.size() < 200000) {
                cache.emplace(r->s, d);
                cachedStates += d->t.size();
            }
            return d;
        } catch (const Limit &) {
            if (budget.done())
                throw;
            failed.insert(r->s);
            return {};
        }
    }
    void report(std::string source) {
        if (!opt.quiet)
            std::cerr << "[" << source << "] " << best->s.size() << " Zeichen: " << best->s << "\n";
    }
    bool consider(R r, const std::string &source) {
        budget.check();
        R normalized = structural(r);
        if (better(normalized, r))
            r = normalized;
        if (!better(r, best))
            return false;
        auto d = automaton(r);
        if (!d)
            return false;
        if (key(*d) != targetKey) {
            ++stats.rejected;
            return false;
        }
        ++stats.verified;
        best = r;
        report(source);
        return true;
    }
    R intern(R r) {
        if (r->s.size() > 700)
            return r;
        auto d = automaton(r);
        if (!d || d->t.size() > 128)
            return r;
        std::string k = key(*d);
        if (opt.catalog)
            if (R stored = opt.catalog->find(k))
                if (better(stored, r)) {
                    auto check = automaton(stored);
                    if (check && key(*check) == k) {
                        r = stored;
                        ++stats.catalogHits;
                    }
                }
        auto it = semanticBank.find(k);
        if (it != semanticBank.end()) {
            if (better(it->second, r))
                return it->second;
            it->second = r;
        } else if (semanticBank.size() < 50000)
            semanticBank.emplace(std::move(k), r);
        return r;
    }
    R catalogWindows(R r, int &fuel) {
        if (!opt.catalog || r->k != CAT || r->sub.size() > 80)
            return r;
        for (int pass = 0; pass < 2 && r->k == CAT && fuel > 0; ++pass) {
            auto v = r->sub;
            R bestHere = r;
            for (size_t length = 2; length <= std::min(size_t(8), v.size()) && fuel > 0; ++length)
                for (size_t first = 0; first + length <= v.size() && fuel > 0; ++first) {
                    R part = slice(v, first, first + length);
                    if (part->s.size() < 4 || part->s.size() > 64)
                        continue;
                    --fuel;
                    auto d = automaton(part);
                    if (!d)
                        continue;
                    std::string k = key(*d);
                    R stored = opt.catalog->find(k);
                    if (!stored || !better(stored, part))
                        continue;
                    auto proof = automaton(stored);
                    if (!proof || key(*proof) != k)
                        continue;
                    R candidate =
                        cat({slice(v, 0, first), stored, slice(v, first + length, v.size())});
                    if (better(candidate, bestHere)) {
                        bestHere = candidate;
                        ++stats.catalogHits;
                    }
                }
            if (!better(bestHere, r))
                break;
            r = bestHere;
        }
        return r;
    }
    R semantic(R r, int &fuel) {
        budget.check();
        if (fuel <= 0)
            return r;
        std::vector<R> v;
        for (auto x : r->sub)
            v.push_back(semantic(x, fuel));
        r = intern(rebuild(r, v));
        auto equivalent = [&](R a, R b) {
            if (eq(a, b))
                return true;
            if (--fuel < 0)
                return false;
            auto x = automaton(a), y = automaton(b);
            return x && y && key(*x) == key(*y);
        };
        auto subset = [&](R a, R b) {
            if (--fuel < 0)
                return false;
            auto x = automaton(a), y = automaton(b);
            if (!x || !y)
                return false;
            try {
                return compare(*x, *y, opt.limits, &budget, true).equal;
            } catch (const Limit &) {
                if (budget.done())
                    throw;
                return false;
            }
        };
        if (r->k == ALT) {
            v = r->sub;
            for (size_t i = 0; i < v.size() && fuel > 0;) {
                bool removed = false;
                for (size_t j = 0; j < v.size() && fuel > 0; ++j)
                    if (i != j && subset(v[i], v[j])) {
                        v.erase(v.begin() + i);
                        removed = true;
                        break;
                    }
                if (!removed)
                    ++i;
            }
            r = alt(v);
        }
        if (r->k == CAT) {
            v = r->sub;
            // Cancellation is checked for the whole context; nullability alone is not enough.
            for (size_t i = 0; i < v.size() && fuel > 0;) {
                auto w = v;
                w.erase(w.begin() + i);
                R c = cat(w);
                if (better(c, r) && equivalent(c, r)) {
                    v = w;
                    r = c;
                } else
                    ++i;
            }
            // XY*X -> X(YX)* and its reverse; useful when grouping gets cheaper.
            if (r->k == CAT) {
                v = r->sub;
                for (size_t i = 0; i + 1 < v.size() && fuel > 0; ++i) {
                    R a = v[i], b = v[i + 1];
                    if (b->k == REP && b->lo == 0 && b->hi == INF && b->sub[0]->k == CAT) {
                        auto f = b->sub[0]->sub;
                        if (eq(a, f.back())) {
                            R tail = slice(f, 0, f.size() - 1);
                            auto w = v;
                            w[i] = rep(cat({a, tail}), 0, INF);
                            w[i + 1] = a;
                            R c = cat(w);
                            if (better(c, r) && equivalent(c, r))
                                r = c;
                        }
                    }
                }
            }
        }
        // Regular-language closure candidates often replace complicated starred unions.
        int mask = alphabet(r);
        std::vector<R> candidates;
        if (mask)
            for (auto [a, b] : std::vector<std::pair<I, I>>{{0, INF}, {1, INF}, {0, 1}, {1, 1}})
                candidates.push_back(rep(chars(mask), a, b));
        if (r->k == REP) {
            R x = r->sub[0];
            candidates.push_back(rep(x, 0, INF));
            candidates.push_back(rep(x, 1, INF));
            candidates.push_back(rep(x, 0, 1));
            candidates.push_back(x);
            if (x->k == ALT)
                for (size_t i = 0; i < x->sub.size(); ++i) {
                    candidates.push_back(rep(x->sub[i], r->lo, r->hi));
                    auto w = x->sub;
                    w.erase(w.begin() + i);
                    candidates.push_back(rep(alt(w), r->lo, r->hi));
                }
        }
        for (auto c : candidates)
            if (fuel > 0 && better(c, r) && equivalent(c, r))
                r = c;
        r = catalogWindows(r, fuel);
        return intern(structural(r));
    }
    struct Graph {
        int start, finish;
        std::vector<std::vector<R>> e;
    };
    Graph graph(const DFA &d) {
        int n = int(d.t.size());
        Graph g{n, n + 1, std::vector<std::vector<R>>(n + 2, std::vector<R>(n + 2, zero()))};
        // Trim dead states before forming a GNFA.
        std::vector<std::vector<int>> pred(n);
        for (int p = 0; p < n; ++p)
            for (int q : d.t[p])
                pred[q].push_back(p);
        std::vector<bool> live(n);
        std::queue<int> q;
        for (int p = 0; p < n; ++p)
            if (d.accept[p]) {
                live[p] = true;
                q.push(p);
            }
        while (!q.empty()) {
            int p = q.front();
            q.pop();
            for (int x : pred[p])
                if (!live[x]) {
                    live[x] = true;
                    q.push(x);
                }
        }
        if (live[d.start])
            g.e[n][d.start] = eps();
        for (int p = 0; p < n; ++p)
            if (live[p]) {
                std::map<int, int> dest;
                for (int c = 0; c < 10; ++c)
                    if (live[d.t[p][c]])
                        dest[d.t[p][c]] |= 1 << c;
                for (auto [to, m] : dest)
                    g.e[p][to] = chars(m);
                if (d.accept[p])
                    g.e[p][n + 1] = eps();
            }
        return g;
    }
    Graph nfaGraph(R r, bool reversed) {
        NFA n(&budget, std::min(opt.limits.nfa, 160));
        auto [a, b] = n.build(r);
        int count = int(n.e.size());
        Graph g{reversed ? b : a, reversed ? a : b,
                std::vector<std::vector<R>>(count, std::vector<R>(count, zero()))};
        for (int p = 0; p < count; ++p)
            for (auto edge : n.e[p]) {
                int u = reversed ? edge.to : p, v = reversed ? p : edge.to;
                g.e[u][v] = alt({g.e[u][v], edge.mask ? chars(edge.mask) : eps()});
            }
        return g;
    }
    R eliminate(Graph g, int mode) {
        int n = int(g.e.size());
        std::vector<bool> alive(n, true);
        int remaining = n - 2;
        size_t total = 0;
        while (remaining--) {
            budget.check();
            std::vector<std::pair<double, int>> scores;
            for (int k = 0; k < n; ++k)
                if (alive[k] && k != g.start && k != g.finish) {
                    double in = 0, out = 0, ins = 0, outs = 0;
                    for (int i = 0; i < n; ++i)
                        if (alive[i] && i != k) {
                            if (g.e[i][k]->k != EMPTY) {
                                ++in;
                                ins += g.e[i][k]->s.size();
                            }
                            if (g.e[k][i]->k != EMPTY) {
                                ++out;
                                outs += g.e[k][i]->s.size();
                            }
                        }
                    double loop = g.e[k][k]->k == EMPTY ? 0 : g.e[k][k]->s.size();
                    double score;
                    switch (mode % 5) {
                    case 0:
                        score = (in - 1) * outs + (out - 1) * ins + in * out * loop;
                        break;
                    case 1:
                        score = in * out;
                        break;
                    case 2:
                        score = (ins + 1) * (outs + 1) * (loop + 1);
                        break;
                    case 3:
                        score = ins + outs + loop;
                        break;
                    default:
                        score = in * out * (loop + 1) + 0.1 * (ins + outs);
                    }
                    if (mode >= 5)
                        score = (std::max(0.0, score) + 1) *
                                std::exp(std::uniform_real_distribution<double>(-1.7, 1.7)(rng));
                    scores.push_back({score, k});
                }
            std::sort(scores.begin(), scores.end());
            int k = scores.front().second;
            R loop = rep(g.e[k][k], 0, INF);
            std::vector<int> in, out;
            for (int i = 0; i < n; ++i)
                if (alive[i] && i != k) {
                    if (g.e[i][k]->k != EMPTY)
                        in.push_back(i);
                    if (g.e[k][i]->k != EMPTY)
                        out.push_back(i);
                }
            for (int i : in)
                for (int j : out) {
                    R path = cat({g.e[i][k], loop, g.e[k][j]});
                    R r = alt({g.e[i][j], path});
                    if (r->s.size() > 16000)
                        throw Limit("Zwischenausdruck zu lang");
                    if (r->k == ALT)
                        r = factorOnce(factorOnce(r, false), true);
                    g.e[i][j] = r;
                }
            alive[k] = false;
            for (int i = 0; i < n; ++i)
                g.e[i][k] = g.e[k][i] = zero();
            total = 0;
            for (int i = 0; i < n; ++i)
                if (alive[i])
                    for (int j = 0; j < n; ++j)
                        if (alive[j])
                            total += g.e[i][j]->s.size();
            if (total > 2000000)
                throw Limit("GNFA-Speicherbudget erreicht");
        }
        ++stats.eliminations;
        return structural(g.e[g.start][g.finish]);
    }
    std::vector<R> subexpressions(R r) {
        std::map<std::string, R> all;
        std::function<void(R)> visit = [&](R x) {
            if (!all.emplace(x->s, x).second)
                return;
            for (auto y : x->sub)
                visit(y);
        };
        visit(r);
        std::vector<R> v;
        for (auto [s, x] : all)
            v.push_back(x);
        std::sort(v.begin(), v.end(), better);
        return v;
    }
    R replace(R r, const std::unordered_map<std::string, R> &bank) {
        budget.check();
        std::vector<R> v;
        for (auto x : r->sub)
            v.push_back(replace(x, bank));
        r = rebuild(r, v);
        auto d = automaton(r);
        if (!d)
            return r;
        auto i = bank.find(key(*d));
        if (i != bank.end() && better(i->second, r))
            return i->second;
        return r;
    }
    void synthesize(double fraction) {
        // Cost-ordered, language-deduplicated grammar search. Bounded explicitly;
        // no completeness/minimality claim is made for this heuristic bank.
        auto until = Budget::Clock::now() + std::chrono::duration_cast<Budget::Clock::duration>(
                                                std::chrono::duration<double>(fraction));
        auto &bank = semanticBank;
        std::unordered_set<std::string> texts;
        struct Entry {
            R r;
            std::string k;
        };
        std::vector<Entry> entries;
        std::vector<std::vector<size_t>> buckets(21);
        std::queue<R> pending;
        int attempts = 0;
        bool found = false;
        auto insert = [&](R r) {
            if (++attempts > opt.synthesis || budget.done() || Budget::Clock::now() >= until)
                return;
            if (r->s.size() > 100 || !texts.insert(r->s).second)
                return;
            auto d = automaton(r);
            if (!d || d->t.size() > 48)
                return;
            std::string k = key(*d);
            auto it = bank.find(k);
            if (it != bank.end() && !better(r, it->second))
                return;
            bank[k] = r;
            entries.push_back({r, k});
            if (r->s.size() < buckets.size())
                buckets[r->s.size()].push_back(entries.size() - 1);
            pending.push(r);
            ++stats.synthesized;
            if (k == targetKey && better(r, best)) {
                best = r;
                ++stats.verified;
                report("Sprachsuche");
                found = true;
            }
        };
        auto seeds = subexpressions(best);
        for (auto x : seeds)
            insert(x);
        int mask = alphabet(best);
        insert(eps());
        // All useful character classes: transition-equivalent digit blocks plus
        // their unions (at most 2^10-1), and classes already in the input.
        std::map<std::vector<int>, int> blocks;
        for (int c = 0; c < 10; ++c)
            if (mask & (1 << c)) {
                std::vector<int> sig;
                for (auto row : target.t)
                    sig.push_back(row[c]);
                blocks[sig] |= 1 << c;
            }
        std::vector<int> masks{0};
        for (auto &[sig, m] : blocks) {
            size_t n = masks.size();
            for (size_t i = 0; i < n; ++i)
                masks.push_back(masks[i] | m);
        }
        for (int m : masks)
            if (m)
                insert(chars(m));
        // Seed powers and simple closures even when their text exceeds a bucket.
        size_t initial = entries.size();
        for (size_t i = 0; i < initial && attempts < opt.synthesis; ++i)
            for (auto [a, b] : std::vector<std::pair<I, I>>{
                     {0, INF}, {1, INF}, {0, 1}, {2, 2}, {3, 3}, {2, INF}, {2, 3}})
                insert(rep(entries[i].r, a, b));
        // Each cost layer is capped per operand to stop one prolific language
        // family from consuming the whole search. Original subtrees remain seeds.
        for (int cost = 2; cost <= 18 && attempts < opt.synthesis && !budget.done() &&
                           Budget::Clock::now() < until;
             ++cost) {
            for (int a = 1; a < cost && attempts < opt.synthesis; ++a) {
                int b = cost - a;
                if (a >= int(buckets.size()) || b >= int(buckets.size()))
                    continue;
                auto left = buckets[a], right = buckets[b];
                if (left.size() > 140)
                    left.resize(140);
                if (right.size() > 140)
                    right.resize(140);
                for (auto i : left) {
                    for (auto j : right) {
                        if (attempts >= opt.synthesis || budget.done() ||
                            Budget::Clock::now() >= until)
                            break;
                        R x = entries[i].r, y = entries[j].r;
                        insert(cat({x, y}));
                        if (i <= j)
                            insert(alt({x, y}));
                    }
                    if (attempts >= opt.synthesis || budget.done() || Budget::Clock::now() >= until)
                        break;
                }
            }
            // Close every newly discovered language under unary operators.
            size_t count = pending.size();
            while (count-- && !pending.empty() && attempts < opt.synthesis &&
                   Budget::Clock::now() < until) {
                R x = pending.front();
                pending.pop();
                for (auto [a, b] :
                     std::vector<std::pair<I, I>>{{0, INF}, {1, INF}, {0, 1}, {2, 2}, {3, 3}})
                    insert(rep(x, a, b));
            }
        }
        (void)found;
        if (!budget.done()) {
            R c = replace(best, bank);
            consider(c, "Teilausdruck-Synthese");
        }
    }
    void explore(std::vector<R> &beam, std::set<std::string> &seen) {
        std::vector<R> sources = beam, newcomers;
        std::shuffle(sources.begin(), sources.end(), rng);
        if (sources.size() > 4)
            sources.resize(4);
        for (auto x : sources) {
            std::vector<R> variants;
            contextualVariants(x, variants, 60, 0, hints);
            std::shuffle(variants.begin(), variants.end(), rng);
            if (variants.size() > 25)
                variants.resize(25);
            for (auto c : variants) {
                budget.check();
                if (c->s.size() > best->s.size() + 24 || !seen.insert(c->s).second)
                    continue;
                auto d = automaton(c);
                if (!d || key(*d) != targetKey)
                    continue;
                newcomers.push_back(c);
                int fuel = 50;
                R shortened = semantic(c, fuel);
                consider(shortened, "Umformungssuche");
                if (!eq(c, shortened))
                    newcomers.push_back(shortened);
            }
        }
        beam.insert(beam.end(), newcomers.begin(), newcomers.end());
        std::sort(beam.begin(), beam.end(), better);
        if (int(beam.size()) > opt.beam) {
            // Preserve the shortest half and a diverse sample of longer
            // equivalent forms, so temporary expansion is not discarded.
            size_t keep = std::max(size_t(1), size_t(opt.beam / 2));
            std::shuffle(beam.begin() + keep, beam.end(), rng);
            beam.resize(opt.beam);
        }
    }
    R replaceExact(R r, const std::string &anchor, R replacement) {
        if (r->s == anchor)
            return replacement;
        bool changed = false;
        std::vector<R> v;
        for (auto x : r->sub) {
            R y = replaceExact(x, anchor, replacement);
            changed |= x != y;
            v.push_back(y);
        }
        return changed ? rebuild(r, v) : r;
    }
    R run(R raw) {
        best = raw;
        try {
            target = compile(raw, opt.limits, &budget);
            targetKey = key(target);
            hints = classHints(raw);
            verified = true;
            if (opt.catalog)
                if (R stored = opt.catalog->find(targetKey)) {
                    if (consider(stored, "Katalog"))
                        ++stats.catalogHits;
                }
            if (!opt.quiet)
                std::cerr << "Minimaler DFA: " << target.t.size() << " Zustaende. Suche startet.\n";
            consider(normalize(raw), "Normalisierung");
            PatternSearch patterns(
                target, budget, opt.limits, [&](R r, const std::string &why) { consider(r, why); },
                [&](R r) { return structural(r); });
            patterns.run(best);
            int fuel = 250;
            consider(semantic(best, fuel), "Semantische Kuerzung");
            if (best->s.size() <= 1)
                return best;
            // Spend at most a quarter of the remaining wall-clock budget here.
            double remain =
                std::chrono::duration<double>(budget.end - Budget::Clock::now()).count();
            if (opt.synthesis > 0 && remain > 0.05)
                synthesize(remain * 0.25);
            std::vector<Graph> graphs;
            std::vector<bool> reversed;
            if (target.t.size() <= 100) {
                graphs.push_back(graph(target));
                reversed.push_back(false);
            }
            if (target.t.size() <= 65 && !budget.done())
                try {
                    DFA rev = reverseDFA(target, opt.limits, &budget);
                    if (rev.t.size() <= 100) {
                        graphs.push_back(graph(rev));
                        reversed.push_back(true);
                    }
                } catch (const Limit &) {
                    if (budget.done())
                        throw;
                }
            for (bool rev : {false, true})
                try {
                    graphs.push_back(nfaGraph(best, rev));
                    reversed.push_back(rev);
                } catch (const Limit &) {
                    if (budget.done())
                        throw;
                }
            // A large global DFA can be difficult to eliminate. Rebuild smaller
            // sublanguages independently and substitute their improved regexes.
            std::vector<R> anchors(graphs.size());
            if (target.t.size() > 20 || best->s.size() > 100 || graphs.empty()) {
                auto nodes = subexpressions(best);
                std::reverse(nodes.begin(), nodes.end());
                int count = 0;
                for (auto x : nodes) {
                    budget.check();
                    if (eq(x, best) || x->s.size() < 8)
                        continue;
                    auto d = automaton(x);
                    if (!d || d->t.size() > 35)
                        continue;
                    graphs.push_back(graph(*d));
                    reversed.push_back(false);
                    anchors.push_back(x);
                    if (++count >= 10)
                        break;
                }
            }
            std::vector<R> beam{best};
            std::set<std::string> beamSeen{best->s};
            for (int iteration = 0; !budget.done() && !graphs.empty(); ++iteration) {
                int index = iteration % int(graphs.size()), mode = iteration / int(graphs.size());
                try {
                    R c = eliminate(graphs[index], mode);
                    if (reversed[index])
                        c = reverseRegex(c);
                    if (c->k == EMPTY)
                        continue;
                    c = structural(c);
                    if (anchors[index]) {
                        c = intern(c);
                        c = replaceExact(best, anchors[index]->s, c);
                    }
                    if (c->s.size() <= best->s.size() * 3 + 20 && beamSeen.insert(c->s).second) {
                        consider(c, "Automaten-Elimination");
                        beam.push_back(c);
                        std::sort(beam.begin(), beam.end(), better);
                        if (int(beam.size()) > opt.beam)
                            beam.resize(opt.beam);
                        if (c->s.size() <= best->s.size() + 30) {
                            int f = 70;
                            consider(semantic(c, f), "Elimination + Semantik");
                        }
                    }
                    if (iteration % 32 == 31)
                        explore(beam, beamSeen);
                    if (iteration % 16 == 15) {
                        for (auto x : beam) {
                            int f = 70;
                            consider(semantic(x, f), "Mehrstart-Suche");
                        }
                    }
                } catch (const Limit &) {
                    if (budget.done())
                        throw;
                }
            }
        } catch (const Limit &e) {
            if (!opt.quiet)
                std::cerr << e.what() << ". Bester bisheriger Ausdruck wird ausgegeben.\n";
        }
        return best;
    }
};
} // namespace rx
