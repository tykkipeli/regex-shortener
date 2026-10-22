#pragma once
#include "automata.hpp"
namespace rx {
struct UnarySearch {
    const DFA &target;
    const Budget &budget;
    int digit;
    I stride = 1;
    std::function<R(R)> refine;
    std::function<void(R, const std::string &)> offer;
    std::map<std::vector<int>, R> finiteCache, expandedCache;
    UnarySearch(const DFA &d, const Budget &b, int c, std::function<R(R)> f,
                std::function<void(R, const std::string &)> o)
        : target(d), budget(b), digit(c), refine(std::move(f)), offer(std::move(o)) {}
    R power(int n) {
        return rep(chars(1 << digit), stride * n, stride * n);
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
        if (maximum <= 160)
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
    // Finite unary languages form a semiring: union is set union and
    // concatenation is a Minkowski sum. Search actual finite sets, so no
    // arithmetic characterization (prime numbers, divisibility, etc.) is
    // needed or assumed here.
    R finiteFactors(const std::vector<int> &lengths, double seconds) {
        if (lengths.size() < 3 || lengths.size() > 40 || lengths.back() > 192)
            return finite(lengths);
        Budget local(seconds * 0.72);
        auto check = [&]() {
            budget.check();
            local.check();
        };
        struct Candidate {
            std::vector<int> quotient, remainder;
            std::vector<int> divisorValues;
            std::vector<int> supportOffsets;
            R divisor, approximate;
        };
        std::map<std::vector<int>, R> memo;
        std::map<std::vector<int>, std::pair<int, std::vector<Candidate>>> plans;
        std::map<std::vector<int>, R> cheapCache;
        std::unordered_map<uint64_t, R> coverTerms;
        auto coverTerm = [&](const std::vector<int> &values, R expression) {
            uint64_t mask = 0;
            for (int v : values) {
                auto at = std::lower_bound(lengths.begin(), lengths.end(), v);
                if (at == lengths.end() || *at != v)
                    return;
                mask |= 1ULL << (at - lengths.begin());
            }
            auto old = coverTerms.find(mask);
            if (old == coverTerms.end() || better(expression, old->second))
                coverTerms[mask] = expression;
        };
        for (int value : lengths)
            coverTerm({value}, power(value));
        for (int start : lengths)
            for (int step = 1; start + step <= lengths.back(); ++step) {
                std::vector<int> values{start};
                for (int k = 1;
                     start + k * step <= lengths.back() &&
                     std::binary_search(lengths.begin(), lengths.end(), start + k * step);
                     ++k) {
                    values.push_back(start + k * step);
                    coverTerm(values, cat({power(start % step),
                                           rep(power(step), start / step, start / step + k)}));
                }
            }
        std::function<R(const std::vector<int> &)> cheap = [&](const std::vector<int> &set) -> R {
            if (set.empty())
                return zero();
            auto old = cheapCache.find(set);
            if (old != cheapCache.end())
                return old->second;
            struct Piece {
                uint64_t mask;
                R r;
            };
            std::vector<Piece> pieces;
            int top = set.back();
            std::vector<int> index(top + 1, -1);
            for (size_t i = 0; i < set.size(); ++i) {
                index[set[i]] = int(i);
                pieces.push_back({1ULL << i, power(set[i])});
            }
            for (int c : set)
                for (int step = 1; c + step <= top; ++step) {
                    uint64_t mask = 1ULL << index[c];
                    for (int k = 1; c + k * step <= top && index[c + k * step] >= 0; ++k) {
                        mask |= 1ULL << index[c + k * step];
                        int offset = c % step, lo = c / step;
                        pieces.push_back(
                            {mask, cat({power(offset), rep(power(step), lo, lo + k)})});
                    }
                }
            uint64_t missing = (1ULL << set.size()) - 1;
            std::vector<R> result;
            while (missing) {
                int chosen = 0;
                double bestScore = -1;
                for (size_t i = 0; i < pieces.size(); ++i) {
                    double score = double(__builtin_popcountll(pieces[i].mask & missing)) /
                                   (pieces[i].r->s.size() + 1);
                    if (score > bestScore) {
                        bestScore = score;
                        chosen = int(i);
                    }
                }
                result.push_back(pieces[chosen].r);
                missing &= ~pieces[chosen].mask;
            }
            R best = refine(alt(result));
            if (set.front() > 0 && set.size() > 1) {
                std::vector<int> shift;
                for (int n : set)
                    shift.push_back(n - set.front());
                R c = refine(cat({power(set.front()), cheap(shift)}));
                if (better(c, best))
                    best = c;
            }
            cheapCache[set] = best;
            return best;
        };
        R incumbent = finite(lengths);
        std::vector<std::function<R(R)>> contexts;
        auto publishContext = [&](R r, const std::string &reason) {
            for (auto frame = contexts.rbegin(); frame != contexts.rend(); ++frame)
                r = (*frame)(r);
            r = refine(r);
            if (better(r, incumbent)) {
                incumbent = r;
                offer(incumbent, reason);
            }
        };
        std::map<std::vector<int>, std::vector<std::vector<int>>> conceptCache;
        auto closedFactorsFor = [&](const std::vector<int> &set) {
            std::vector<int> shape;
            for (int value : set)
                shape.push_back(value - set.front());
            auto saved = conceptCache.find(shape);
            if (saved != conceptCache.end())
                return saved->second;
            using Bits = std::array<uint64_t, 4>;
            std::set<Bits> concepts;
            for (int start : shape) {
                check();
                Bits row{};
                for (int value : shape)
                    if (value >= start)
                        row[(value - start) / 64] |= 1ULL << ((value - start) % 64);
                std::vector<Bits> extra;
                for (const auto &old : concepts) {
                    Bits common{};
                    int count = 0;
                    for (int i = 0; i < 4; ++i) {
                        common[i] = old[i] & row[i];
                        count += __builtin_popcountll(common[i]);
                    }
                    if (count >= 2)
                        extra.push_back(common);
                }
                int count = 0;
                for (uint64_t bits : row)
                    count += __builtin_popcountll(bits);
                if (count >= 2)
                    concepts.insert(row);
                concepts.insert(extra.begin(), extra.end());
                if (concepts.size() > 6000)
                    break;
            }
            std::vector<std::vector<int>> result;
            for (const auto &bits : concepts) {
                std::vector<int> factor;
                for (int value = 0; value <= shape.back(); ++value)
                    if (bits[value / 64] & (1ULL << (value % 64)))
                        factor.push_back(value);
                if (factor.size() >= 2)
                    result.push_back(std::move(factor));
            }
            conceptCache.emplace(shape, result);
            return result;
        };
        // Cheap complete bicliques provide a useful incumbent before deeper
        // affine quotients and recursive searches consume their budgets.
        try {
            for (const auto &divisor : closedFactorsFor(lengths)) {
                check();
                std::vector<int> quotient;
                for (int value : lengths) {
                    bool fits = true;
                    for (int delta : divisor)
                        if (!std::binary_search(lengths.begin(), lengths.end(), value + delta)) {
                            fits = false;
                            break;
                        }
                    if (fits)
                        quotient.push_back(value);
                }
                if (quotient.size() < 2)
                    continue;
                std::set<int> covered;
                for (int value : quotient)
                    for (int delta : divisor)
                        covered.insert(value + delta);
                std::vector<int> outside;
                for (int value : lengths)
                    if (!covered.count(value))
                        outside.push_back(value);
                R a = cheap(quotient), b = cheap(divisor), rem = cheap(outside);
                publishContext(alt({cat({a, b}), rem}), "Geschlossene endliche Produkte");
                publishContext(alt({cat({b, a}), rem}), "Geschlossene endliche Produkte");
            }
        } catch (const Limit &) {
            budget.check();
        }
        // Separate a dominant congruence class from a few outliers, then
        // optimize its affine quotient with the true cost of the lifted atom.
        // This is independent of any numerical predicate defining the set.
        std::set<std::vector<int>> affineSeen;
        std::vector<std::vector<int>> affineCores;
        for (int modulus = 2; modulus <= 12; ++modulus) {
            std::vector<std::vector<int>> classes(modulus);
            for (int value : lengths)
                classes[value % modulus].push_back(value);
            for (const auto &core : classes)
                if (core.size() >= 4 && core.size() * 4 >= lengths.size() * 3 &&
                    affineSeen.insert(core).second)
                    affineCores.push_back(core);
        }
        std::sort(affineCores.begin(), affineCores.end(),
                  [](const auto &a, const auto &b) { return a.size() > b.size(); });
        for (const auto &core : affineCores) {
            int offset = core.front(), scale = 0;
            for (int value : core)
                scale = std::gcd(scale, value - offset);
            if (scale <= 1)
                continue;
            double remaining =
                std::chrono::duration<double>(local.end - Budget::Clock::now()).count();
            if (remaining <= 0.03)
                break;
            std::vector<int> reduced, outside;
            for (int value : core)
                reduced.push_back((value - offset) / scale);
            std::set_difference(lengths.begin(), lengths.end(), core.begin(), core.end(),
                                std::back_inserter(outside));
            R outliers = finite(outside);
            auto lift = [&](R r) {
                publishContext(alt({cat({power(offset), r}), outliers}),
                               "Affine endliche Summenmengen");
            };
            UnarySearch child(target, budget, digit, refine,
                              [&](R r, const std::string &) { lift(r); });
            child.stride = stride * scale;
            R result = child.finiteFactors(reduced, std::min(seconds * 0.15, remaining * 0.3));
            lift(result);
        }
        size_t visits = 0;
        std::function<R(const std::vector<int> &, int)> solve = [&](const std::vector<int> &set,
                                                                    int depth) -> R {
            check();
            auto cached = memo.find(set);
            if (cached != memo.end())
                return cached->second;
            R best = cheap(set);
            if (depth == 0 || set.size() < 3 || ++visits > 2500)
                return best;
            int lo = set.front(), hi = set.back(), span = hi - lo;
            auto accept = [&](R r) {
                r = refine(r);
                if (better(r, best))
                    best = r;
                publishContext(best, "Gemeinsame endliche Offsets");
            };
            if (lo > 0) {
                std::vector<int> normalized;
                for (int value : set)
                    normalized.push_back(value - lo);
                contexts.push_back([&, lo](R r) { return cat({power(lo), r}); });
                R normalizedResult = solve(normalized, depth - 1);
                contexts.pop_back();
                accept(cat({power(lo), normalizedResult}));
            }
            // A few small boundary outliers can hide a shared shift of a
            // large finite sublanguage. Split by sorted lengths, then
            // optimize the suffix as one algebraic object.
            for (size_t cut = 1; cut <= 4 && cut + 2 < set.size(); ++cut) {
                std::vector<int> prefix(set.begin(), set.begin() + cut);
                std::vector<int> suffix(set.begin() + cut, set.end());
                R outside = cheap(prefix);
                contexts.push_back([outside](R r) { return alt({outside, r}); });
                R suffixResult = solve(suffix, depth - 1);
                contexts.pop_back();
                accept(alt({outside, suffixResult}));
            }
            std::vector<Candidate> choices;
            std::vector<int> shape;
            for (int value : set)
                shape.push_back(value - lo);
            auto planned = plans.find(shape);
            if (planned != plans.end()) {
                choices = planned->second.second;
                int shift = lo - planned->second.first;
                if (shift)
                    for (auto &candidate : choices) {
                        for (int &value : candidate.quotient)
                            value += shift;
                        for (int &value : candidate.remainder)
                            value += shift;
                        candidate.approximate =
                            refine(alt({cat({cheap(candidate.quotient), candidate.divisor}),
                                        cheap(candidate.remainder)}));
                    }
                std::sort(choices.begin(), choices.end(),
                          [](const Candidate &a, const Candidate &b) {
                              return better(a.approximate, b.approximate);
                          });
            } else {
                std::vector<uint8_t> present(hi + 1);
                for (int n : set)
                    present[n] = true;
                std::map<std::pair<std::vector<int>, std::vector<int>>, Candidate> unique;
                auto factor = [&](const std::vector<int> &offsets, R divisor,
                                  bool recursiveDivisor = false) {
                    check();
                    std::vector<int> quotient;
                    for (int n : set) {
                        bool fits = true;
                        for (int d : offsets)
                            if (n + d > hi || !present[n + d]) {
                                fits = false;
                                break;
                            }
                        if (fits)
                            quotient.push_back(n);
                    }
                    if (quotient.size() < 2 || quotient.size() == set.size())
                        return;
                    std::vector<uint8_t> covered(hi + 1);
                    for (int n : quotient)
                        for (int d : offsets)
                            covered[n + d] = true;
                    std::vector<int> remainder;
                    for (int n : set)
                        if (!covered[n])
                            remainder.push_back(n);
                    R q = cheap(quotient), rem = cheap(remainder);
                    R product = cat({q, divisor}), reversed = cat({divisor, q});
                    if (better(reversed, product))
                        product = reversed;
                    if (set == lengths) {
                        std::vector<int> values;
                        for (int n : set)
                            if (covered[n])
                                values.push_back(n);
                        coverTerm(values, refine(product));
                        // A closed rectangle maximizes coverage, but a few
                        // expensive rows may destroy its compression. Explore
                        // irregular subquotients too, not only progressions.
                        if (quotient.size() >= 3 && quotient.size() <= 8) {
                            auto rows = [&](const std::vector<int>& subset) {
                                std::set<int> sums;
                                for (int x : subset)
                                    for (int d : offsets) sums.insert(x+d);
                                R a=cheap(subset);
                                R p=refine(cat({a,divisor}));
                                R reverse=refine(cat({divisor,a}));
                                if (better(reverse,p)) p=reverse;
                                coverTerm(std::vector<int>(sums.begin(),sums.end()),p);
                            };
                            for (size_t omit=0;omit<quotient.size();++omit) {
                                auto subset=quotient;
                                subset.erase(subset.begin()+omit);
                                rows(subset);
                                if (quotient.size()<=6 && subset.size()>=3)
                                    for (size_t second=omit;second<subset.size();++second) {
                                        auto smaller=subset;
                                        smaller.erase(smaller.begin()+second);
                                        rows(smaller);
                                    }
                            }
                        }
                        // A maximal quotient can contain costly outliers.
                        // Its short arithmetic subquotients remain useful
                        // independently in an overlapping cover.
                        for (int start : quotient)
                            for (int step = 1; start + step <= quotient.back(); ++step) {
                                std::vector<int> points{start};
                                for (int k = 1; start + k * step <= quotient.back() &&
                                                std::binary_search(quotient.begin(), quotient.end(),
                                                                   start + k * step);
                                     ++k) {
                                    points.push_back(start + k * step);
                                    std::set<int> sums;
                                    for (int x : points)
                                        for (int d : offsets)
                                            sums.insert(x + d);
                                    R range = rep(power(step), start / step, start / step + k);
                                    std::vector<R> factors{power(start % step), range, divisor};
                                    std::vector<int> order{0, 1, 2};
                                    R expression;
                                    do {
                                        R r = refine(cat({factors[order[0]], factors[order[1]],
                                                          factors[order[2]]}));
                                        if (!expression || better(r, expression))
                                            expression = r;
                                    } while (std::next_permutation(order.begin(), order.end()));
                                    coverTerm(std::vector<int>(sums.begin(), sums.end()),
                                              expression);
                                }
                            }
                    }
                    R candidate = refine(alt({product, rem}));
                    if (better(candidate, best)) {
                        best = candidate;
                        publishContext(best, "Endliche Summenmengen im Kontext");
                    }
                    auto key = std::make_pair(quotient, remainder);
                    auto old = unique.find(key);
                    if (old == unique.end() || better(candidate, old->second.approximate))
                        unique[key] = {
                            quotient, remainder, recursiveDivisor ? offsets : std::vector<int>(),
                            offsets,  divisor,   candidate};
                };
                // Closed bicliques of the additive Hankel relation admit
                // arbitrary irregular finite product factors.
                auto closedFactors = closedFactorsFor(set);
                std::set<std::vector<int>> usedFactors;
                for (const auto &offsets : closedFactors) {
                    factor(offsets, cheap(offsets), true);
                    usedFactors.insert(offsets);
                }
                // Closure maximizes coverage, not compression. Removing a
                // costly outlier from either factor can improve the printed
                // expression and still participate in an overlapping cover.
                for (const auto &closed : closedFactors)
                    if (closed.size() >= 3 && closed.size() <= 12)
                        for (size_t omit = 1; omit < closed.size(); ++omit) {
                            auto subset = closed;
                            subset.erase(subset.begin() + omit);
                            if (usedFactors.insert(subset).second)
                                factor(subset, cheap(subset), true);
                            if (closed.size() <= 8 && subset.size() >= 3)
                                for (size_t second = 1; second < subset.size(); ++second) {
                                    auto smaller = subset;
                                    smaller.erase(smaller.begin() + second);
                                    if (usedFactors.insert(smaller).second)
                                        factor(smaller, cheap(smaller), true);
                                }
                        }
                // Every interval of repetitions is a potential factor.
                // Maximal quotients are derived by exact set erosion.
                for (int step = 1; step <= span; ++step) {
                    std::vector<int> offsets{0};
                    for (int k = 1; k <= 5 && k * step <= span; ++k) {
                        offsets.push_back(k * step);
                        factor(offsets, rep(power(step), 0, k));
                    }
                }
                // Irregular three-point factors discover sums that are not
                // arithmetic grids. Only differences occurring in the input
                // need be considered; every useful factor has two translates.
                std::set<int> differences;
                for (size_t i = 0; i < set.size(); ++i)
                    for (size_t j = i + 1; j < set.size(); ++j)
                        differences.insert(set[j] - set[i]);
                for (int a : differences)
                    for (int b : differences)
                        if (a < b && b <= span) {
                            factor({0, a, b}, alt({eps(), power(a), power(b)}));
                            for (int repeat = 2; repeat <= 3 && repeat * b <= span; ++repeat) {
                                std::set<int> sums;
                                for (int ia = 0; ia <= repeat; ++ia)
                                    for (int ib = 0; ia + ib <= repeat; ++ib)
                                        sums.insert(ia * a + ib * b);
                                factor(std::vector<int>(sums.begin(), sums.end()),
                                       rep(alt({eps(), power(a), power(b)}), repeat, repeat));
                            }
                            for (int ka = 1; ka <= 2; ++ka)
                                for (int kb = 1; kb <= 2; ++kb)
                                    if (ka * a + kb * b <= span) {
                                        std::set<int> sums;
                                        for (int ia = 0; ia <= ka; ++ia)
                                            for (int ib = 0; ib <= kb; ++ib)
                                                sums.insert(ia * a + ib * b);
                                        factor(std::vector<int>(sums.begin(), sums.end()),
                                               cat({rep(power(a), 0, ka), rep(power(b), 0, kb)}));
                                    }
                        }
                for (auto &entry : unique)
                    choices.push_back(std::move(entry.second));
                std::sort(choices.begin(), choices.end(),
                          [](const Candidate &a, const Candidate &b) {
                              return better(a.approximate, b.approximate);
                          });
                // Retain some temporarily longer decompositions: their
                // quotients can expose a second, much cheaper factor.
                if (choices.size() > 80)
                    choices.resize(80);
                plans.emplace(shape, std::make_pair(lo, choices));
            }
            for (const auto &candidate : choices) {
                if (better(candidate.approximate, best))
                    best = candidate.approximate;
                R outside = cheap(candidate.remainder), basicDivisor = candidate.divisor;
                contexts.push_back([outside, basicDivisor](R r) {
                    return alt({cat({r, basicDivisor}), outside});
                });
                R q = solve(candidate.quotient, depth - 1);
                contexts.pop_back();
                R rem = zero();
                if (!candidate.remainder.empty()) {
                    contexts.push_back(
                        [q, basicDivisor](R r) { return alt({cat({q, basicDivisor}), r}); });
                    rem = solve(candidate.remainder, depth - 1);
                    contexts.pop_back();
                }
                R divisor = basicDivisor;
                if (!candidate.divisorValues.empty()) {
                    contexts.push_back([q, rem](R r) { return alt({cat({q, r}), rem}); });
                    divisor = solve(candidate.divisorValues, depth - 1);
                    contexts.pop_back();
                }
                for (bool reverse : {false, true}) {
                    R product = reverse ? cat({divisor, q}) : cat({q, divisor});
                    if (set == lengths) {
                        std::set<int> values;
                        for (int a : candidate.quotient)
                            for (int b : candidate.supportOffsets)
                                values.insert(a + b);
                        coverTerm(std::vector<int>(values.begin(), values.end()), refine(product));
                    }
                    R result = refine(alt({product, rem}));
                    if (better(result, best))
                        best = result;
                    publishContext(best, "Endliche Summenmengen im Kontext");
                }
            }
            memo[set] = best;
            return best;
        };
        R best = incumbent;
        try {
            // Iterative deepening preserves concrete improvements if a later
            // decomposition exhausts the independent arithmetic budget.
            for (int depth = 1; depth <= 7; ++depth) {
                memo.clear();
                visits = 0;
                R next = solve(lengths, depth);
                if (better(next, best)) {
                    best = next;
                    offer(best, "Endliche Summenmengen");
                }
            }
        } catch (const Limit &) {
            budget.check();
        }
        // Different factorizations may overlap. Choose their union jointly
        // rather than forcing the remainder of one decomposition to stand
        // alone. This bounded set-cover search uses real printed costs.
        struct Cover {
            uint64_t mask;
            R expression;
            int cost;
        };
        std::vector<Cover> choices;
        for (const auto &[mask, expression] : coverTerms)
            choices.push_back({mask, expression, int(expression->s.size()) + 1});
        std::sort(choices.begin(), choices.end(), [](const Cover &a, const Cover &b) {
            return a.cost != b.cost ? a.cost < b.cost
                                    : __builtin_popcountll(a.mask) > __builtin_popcountll(b.mask);
        });
        std::vector<Cover> nondominated;
        std::vector<std::vector<int>> supersets(lengths.size());
        for (const auto &piece : choices) {
            int pivot = -1;
            for (size_t bit = 0; bit < lengths.size(); ++bit)
                if ((piece.mask & (1ULL << bit)) &&
                    (pivot < 0 || supersets[bit].size() < supersets[pivot].size()))
                    pivot = int(bit);
            bool redundant = false;
            for (int index : supersets[pivot])
                if ((nondominated[index].mask & piece.mask) == piece.mask) {
                    redundant = true;
                    break;
                }
            if (!redundant) {
                for (size_t bit = 0; bit < lengths.size(); ++bit)
                    if (piece.mask & (1ULL << bit))
                        supersets[bit].push_back(int(nondominated.size()));
                nondominated.push_back(piece);
            }
        }
        choices = std::move(nondominated);
        std::sort(choices.begin(), choices.end(), [](const Cover &a, const Cover &b) {
            double x = double(__builtin_popcountll(a.mask)) / a.cost;
            double y = double(__builtin_popcountll(b.mask)) / b.cost;
            return x != y ? x > y : a.cost < b.cost;
        });
        uint64_t all = (1ULL << lengths.size()) - 1;
        std::vector<std::vector<int>> containing(lengths.size());
        for (size_t i = 0; i < choices.size(); ++i)
            for (size_t bit = 0; bit < lengths.size(); ++bit)
                if (choices[i].mask & (1ULL << bit))
                    containing[bit].push_back(int(i));
        int upper = 1000000000;
        std::vector<int> selected;
        std::unordered_map<uint64_t, std::vector<std::pair<int, uint64_t>>> seen;
        Budget covering(seconds * 0.28);
        auto publish = [&]() {
            std::vector<R> expressions;
            for (int index : selected)
                expressions.push_back(choices[index].expression);
            R r = refine(alt(expressions));
            if (better(r, incumbent)) {
                incumbent = r;
                offer(incumbent, "Ueberlappende endliche Summenmengen");
            }
        };
        {
            uint64_t missing = all;
            int cost = 0;
            while (missing) {
                int chosen = 0;
                double ratio = -1;
                for (size_t i = 0; i < choices.size(); ++i) {
                    double r =
                        double(__builtin_popcountll(missing & choices[i].mask)) / choices[i].cost;
                    if (r > ratio) {
                        ratio = r;
                        chosen = int(i);
                    }
                }
                selected.push_back(chosen);
                cost += choices[chosen].cost;
                missing &= ~choices[chosen].mask;
            }
            upper = cost;
            publish();
            selected.clear();
        }
        std::function<void(uint64_t, int)> cover = [&](uint64_t missing, int cost) {
            budget.check();
            covering.check();
            if (!missing) {
                upper = std::min(upper, cost);
                publish();
                return;
            }
            constexpr int contextSlack = 12;
            if (cost >= upper + contextSlack)
                return;
            uint64_t signature = 0;
            for (int index : selected) {
                uint64_t x = uint64_t(index + 1) * 0x9e3779b97f4a7c15ULL;
                x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
                signature ^= x ^ (x >> 27);
            }
            auto &history = seen[missing];
            for (auto entry : history)
                if (entry.second == signature && entry.first <= cost)
                    return;
            if (history.size() >= 6 && history.back().first < cost)
                return;
            history.push_back({cost, signature});
            std::sort(history.begin(), history.end());
            if (history.size() > 6)
                history.resize(6);
            if (seen.size() > 400000)
                throw Limit("Endlicher Ueberdeckungsspeicher erreicht");
            int pivot = -1;
            for (size_t bit = 0; bit < lengths.size(); ++bit)
                if ((missing & (1ULL << bit)) &&
                    (pivot < 0 || containing[bit].size() < containing[pivot].size()))
                    pivot = int(bit);
            double bestRatio = 0;
            for (const auto &piece : choices)
                bestRatio = std::max(bestRatio, double(__builtin_popcountll(missing & piece.mask)) /
                                                    piece.cost);
            if (cost + int(std::ceil(__builtin_popcountll(missing) / bestRatio - 1e-9)) >=
                upper + contextSlack)
                return;
            std::vector<int> branches = containing[pivot];
            std::sort(branches.begin(), branches.end(), [&](int a, int b) {
                double x =
                    double(__builtin_popcountll(missing & choices[a].mask)) / choices[a].cost;
                double y =
                    double(__builtin_popcountll(missing & choices[b].mask)) / choices[b].cost;
                return x != y ? x > y : choices[a].cost < choices[b].cost;
            });
            std::unordered_map<uint64_t, int> distinct;
            for (int index : branches) {
                const auto &piece = choices[index];
                uint64_t remains = missing & ~piece.mask;
                auto duplicate = distinct.find(remains);
                if (duplicate != distinct.end() && duplicate->second <= piece.cost)
                    continue;
                distinct[remains] = piece.cost;
                selected.push_back(index);
                cover(remains, cost + piece.cost);
                selected.pop_back();
            }
        };
        try {
            cover(all, 0);
        } catch (const Limit &) {
            budget.check();
        }
        return better(incumbent, best) ? incumbent : best;
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
        // Recover additive generators from the unary automaton. This is a
        // hypothesis, not an assumption that every unary language is a monoid;
        // the caller proves the resulting starred expression before using it.
        if (accepts(0)) {
            int bound = std::min(160, 2 * (mu + period) + 2);
            std::vector<bool> generated(bound + 1);
            generated[0] = true;
            std::vector<int> generators;
            for (int length = 1; length <= bound; ++length) {
                for (int g : generators)
                    if (g <= length && generated[length - g])
                        generated[length] = true;
                if (accepts(length) && !generated[length]) {
                    generators.push_back(length);
                    generated[length] = true;
                }
            }
            if (!generators.empty() && generators.size() <= 12)
                offer(refine(rep(finite(generators), 0, INF)), "Additive Erzeuger");
        }
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
        if (residues.empty() && prefix.size() >= 3 && prefix.size() <= 40) {
            double remaining =
                std::chrono::duration<double>(budget.end - Budget::Clock::now()).count();
            if (remaining > 0.02)
                finiteFactors(prefix, std::min(40.0, remaining * 0.45));
        }
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
