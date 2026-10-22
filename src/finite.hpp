#pragma once
#include "automata.hpp"

namespace rx {
// Portable 128-bit word-set mask. Neighbor alphabets still use uint64_t.
struct FiniteMask {
    uint64_t low = 0, high = 0;
    FiniteMask(uint64_t a = 0, uint64_t b = 0) : low(a), high(b) {}
    static FiniteMask bit(int i) {
        return i < 64 ? FiniteMask(1ULL << i) : FiniteMask(0, 1ULL << (i - 64));
    }
    static FiniteMask full(int n) {
        if (n < 64)
            return FiniteMask((1ULL << n) - 1);
        if (n == 64)
            return FiniteMask(~0ULL);
        return FiniteMask(~0ULL, n == 128 ? ~0ULL : (1ULL << (n - 64)) - 1);
    }
    explicit operator bool() const {
        return low || high;
    }
    int count() const {
        return __builtin_popcountll(low) + __builtin_popcountll(high);
    }
    int first() const {
        return low ? __builtin_ctzll(low) : high ? 64 + __builtin_ctzll(high) : 128;
    }
    void clearFirst() {
        if (low)
            low &= low - 1;
        else
            high &= high - 1;
    }
    FiniteMask &operator|=(FiniteMask b) {
        low |= b.low;
        high |= b.high;
        return *this;
    }
    FiniteMask &operator&=(FiniteMask b) {
        low &= b.low;
        high &= b.high;
        return *this;
    }
    friend FiniteMask operator|(FiniteMask a, FiniteMask b) {
        return FiniteMask(a.low | b.low, a.high | b.high);
    }
    friend FiniteMask operator&(FiniteMask a, FiniteMask b) {
        return FiniteMask(a.low & b.low, a.high & b.high);
    }
    friend FiniteMask operator~(FiniteMask a) {
        return FiniteMask(~a.low, ~a.high);
    }
    friend bool operator==(FiniteMask a, FiniteMask b) {
        return a.low == b.low && a.high == b.high;
    }
    friend bool operator!=(FiniteMask a, FiniteMask b) {
        return !(a == b);
    }
    struct Hash {
        size_t operator()(FiniteMask a) const {
            return std::hash<uint64_t>{}(
                a.low ^ (a.high + 0x9e3779b97f4a7c15ULL + (a.low << 6) + (a.low >> 2)));
        }
    };
};
// A finite language is a Boolean relation at every cut between its symbols.
// Rectangles of that relation give safe concatenations A B; unlike a trie,
// overlapping rectangles can share both prefixes and suffixes simultaneously.
struct FiniteSearch {
    const DFA &target;
    const Budget &budget;
    Limits limits;
    std::function<void(R, const std::string &)> offer;
    std::function<R(R)> refine;
    std::map<std::vector<std::string>, R> representations;
    struct Term {
        FiniteMask cover;
        R r;
        int cost;
    };
    std::vector<std::string> words;
    std::vector<std::string> required;
    std::map<std::string, int> requiredIndex;
    std::unordered_map<FiniteMask, R, FiniteMask::Hash> rectangles;
    size_t operations = 0;
    size_t cutQuota = 20000;
    explicit FiniteSearch(const DFA &d, const Budget &b, Limits l,
                          std::function<void(R, const std::string &)> o, std::function<R(R)> f)
        : target(d), budget(b), limits(l), offer(std::move(o)), refine(std::move(f)) {}

    bool extract(size_t cap = 128) {
        words.clear();
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
        // First prove acyclicity of the productive part. Enumerating an
        // infinite language until a cap would bias which words we optimize.
        std::vector<int> color(n), longest(n);
        std::function<bool(int)> acyclic = [&](int p) {
            budget.check();
            if (!live[p])
                return true;
            if (color[p])
                return color[p] == 2;
            color[p] = 1;
            for (int q : target.t[p])
                if (live[q]) {
                    if (!acyclic(q))
                        return false;
                    longest[p] = std::max(longest[p], 1 + longest[q]);
                }
            color[p] = 2;
            return true;
        };
        if (!acyclic(target.start) || longest[target.start] > 32)
            return false;
        std::string word;
        bool capped = false;
        std::function<void(int)> visit = [&](int p) {
            budget.check();
            if (capped || !live[p])
                return;
            if (target.accept[p]) {
                words.push_back(word);
                if (words.size() > cap) {
                    capped = true;
                    return;
                }
            }
            for (int c = 0; c < 10; ++c)
                if (live[target.t[p][c]]) {
                    word += char('0' + c);
                    visit(target.t[p][c]);
                    word.pop_back();
                }
        };
        visit(target.start);
        return !capped;
    }

    R literal(const std::string &word) {
        std::vector<R> v;
        for (char c : word)
            v.push_back(chars(1 << (c - '0')));
        return cat(v);
    }

    R represent(std::vector<std::string> set) {
        budget.check();
        std::sort(set.begin(), set.end());
        set.erase(std::unique(set.begin(), set.end()), set.end());
        auto old = representations.find(set);
        if (old != representations.end())
            return old->second;
        if (set.empty())
            return zero();
        if (set.size() == 1)
            return representations[set] = literal(set[0]);
        R best;
        for (bool suffix : {false, true}) {
            std::map<char, std::vector<std::string>> groups;
            std::vector<R> terms;
            for (const auto &word : set) {
                if (word.empty()) {
                    terms.push_back(eps());
                    continue;
                }
                char c = suffix ? word.back() : word.front();
                groups[c].push_back(suffix ? word.substr(0, word.size() - 1) : word.substr(1));
            }
            std::map<std::string, std::pair<int, R>> shared;
            for (auto &[c, rest] : groups) {
                R child = represent(rest);
                auto &entry = shared[child->s];
                entry.first |= 1 << (c - '0');
                entry.second = child;
            }
            for (auto &[text, entry] : shared) {
                (void)text;
                auto [mask, child] = entry;
                terms.push_back(suffix ? cat({child, chars(mask)}) : cat({chars(mask), child}));
            }
            R candidate = refine(alt(terms));
            if (!best || better(candidate, best))
                best = candidate;
        }
        representations[set] = best;
        return best;
    }

    void add(FiniteMask cover, R r) {
        if (!cover)
            return;
        auto it = rectangles.find(cover);
        auto cost = [](R x) { return wrap(x, 1).size(); };
        if (it == rectangles.end() || cost(r) < cost(it->second) ||
            (cost(r) == cost(it->second) && better(r, it->second)))
            rectangles[cover] = r;
    }

    void cut(int position, bool fromEnd) {
        if (rectangles.size() >= 20000 || operations > 150000)
            return;
        size_t stopSize = std::min(size_t(20000), rectangles.size() + cutQuota);
        std::vector<std::pair<std::string, std::string>> pairs;
        std::set<std::string> a, b;
        for (const auto &word : words) {
            size_t p = fromEnd ? word.size() - std::min(size_t(position), word.size())
                               : std::min(size_t(position), word.size());
            pairs.emplace_back(word.substr(0, p), word.substr(p));
            a.insert(pairs.back().first);
            b.insert(pairs.back().second);
        }
        bool transpose = a.size() > b.size();
        std::vector<std::string> small, large;
        const auto &smallSet = transpose ? b : a;
        const auto &largeSet = transpose ? a : b;
        small.assign(smallSet.begin(), smallSet.end());
        large.assign(largeSet.begin(), largeSet.end());
        if (small.size() > 12 || large.size() > 64)
            return;
        std::vector<uint64_t> neighbors(small.size());
        std::vector<std::vector<FiniteMask>> edge(small.size(),
                                                  std::vector<FiniteMask>(large.size()));
        for (size_t i = 0; i < pairs.size(); ++i) {
            auto [left, right] = pairs[i];
            if (transpose)
                std::swap(left, right);
            size_t x = std::lower_bound(small.begin(), small.end(), left) - small.begin();
            size_t y = std::lower_bound(large.begin(), large.end(), right) - large.begin();
            neighbors[x] |= 1ULL << y;
            auto requiredWord = requiredIndex.find(words[i]);
            if (requiredWord != requiredIndex.end())
                edge[x][y] |= FiniteMask::bit(requiredWord->second);
        }
        uint64_t allLarge = large.size() == 64 ? ~0ULL : (1ULL << large.size()) - 1;
        struct Selection {
            unsigned subset;
            uint64_t common;
            R expression;
            double priority;
        };
        std::vector<Selection> selections;
        for (unsigned subset = 1; subset < (1U << small.size()); ++subset) {
            budget.check();
            uint64_t common = allLarge;
            std::vector<std::string> x;
            for (size_t i = 0; i < small.size(); ++i)
                if (subset & (1U << i)) {
                    common &= neighbors[i];
                    x.push_back(small[i]);
                }
            if (!common)
                continue;
            FiniteMask potential;
            for (size_t i = 0; i < small.size(); ++i)
                if (subset & (1U << i))
                    for (uint64_t bits = common; bits; bits &= bits - 1)
                        potential |= edge[i][__builtin_ctzll(bits)];
            if (!potential)
                continue;
            R xr = represent(x);
            double priority = double(potential.count()) / (wrap(xr, 2).size() + 2);
            selections.push_back({subset, common, xr, priority});
        }
        std::sort(selections.begin(), selections.end(), [](const Selection &a, const Selection &b) {
            return a.priority != b.priority ? a.priority > b.priority : a.subset < b.subset;
        });
        for (const auto &selection : selections) {
            auto [subset, common, xr, priority] = selection;
            (void)priority;
            auto rectangle = [&](uint64_t other) {
                if ((++operations & 127) == 0)
                    budget.check();
                if (rectangles.size() >= stopSize || operations > 150000)
                    return;
                FiniteMask cover;
                std::vector<std::string> y;
                for (uint64_t bits = other; bits; bits &= bits - 1) {
                    int j = __builtin_ctzll(bits);
                    y.push_back(large[j]);
                    for (size_t i = 0; i < small.size(); ++i)
                        if (subset & (1U << i))
                            cover |= edge[i][j];
                }
                R yr = represent(y);
                add(cover, transpose ? cat({yr, xr}) : cat({xr, yr}));
            };
            // Maximal bicliques alone are insufficient: a smaller character
            // class can be cheaper, even if it covers fewer accepted words.
            if (__builtin_popcountll(common) <= 12) {
                for (uint64_t other = common; other; other = (other - 1) & common) {
                    rectangle(other);
                    if (rectangles.size() >= stopSize || operations > 150000)
                        break;
                }
            } else {
                rectangle(common);
                for (uint64_t bits = common; bits; bits &= bits - 1) {
                    uint64_t one = bits & -bits;
                    rectangle(one);
                    rectangle(common & ~one);
                }
            }
            if (rectangles.size() >= stopSize || operations > 150000)
                break;
        }
    }

    void run() {
        if (!extract() || words.empty())
            return;
        optimize(words);
    }

    // For use inside a proved union context: required is mandatory, while
    // extra words of this finite target are don't-cares already accepted by
    // another alternative. Offers satisfy required <= offer <= target rather
    // than equality. A caller must preserve that context and prove the union.
    void runRequired(std::vector<std::string> mandatory) {
        if (mandatory.empty() || mandatory.size() > 128 || !extract(512))
            return;
        for (const auto &word : mandatory)
            if (!matches(target, word))
                return;
        optimize(std::move(mandatory));
    }

    void optimize(std::vector<std::string> mandatory) {
        required = std::move(mandatory);
        std::sort(required.begin(), required.end());
        required.erase(std::unique(required.begin(), required.end()), required.end());
        if (required.empty() || required.size() > 128)
            return;
        requiredIndex.clear();
        rectangles.clear();
        operations = 0;
        for (size_t i = 0; i < required.size(); ++i)
            requiredIndex[required[i]] = int(i);
        R baseline = represent(required);
        offer(baseline, "Endliche Sprache: Praefixe und Suffixe");
        FiniteMask full = FiniteMask::full(int(required.size()));
        // A complete known representation is a legitimate cover term too.
        // It supplies a finite incumbent for bounded improvement search.
        add(full, baseline);
        int maximum = 0, alphabet = 0;
        for (size_t i = 0; i < required.size(); ++i)
            add(FiniteMask::bit(int(i)), literal(required[i]));
        for (const auto &word : words) {
            maximum = std::max(maximum, int(word.size()));
            for (char c : word)
                alphabet |= 1 << (c - '0');
        }
        if (__builtin_popcount(unsigned(alphabet)) <= 1)
            return;
        // Share the finite candidate budget across both orientations and all
        // cuts; one dense relation must not prevent every later cut entirely.
        cutQuota = std::max(size_t(512), size_t(20000) / std::max(2, 2 * (maximum - 1)));
        for (int position = 1; position < maximum; ++position)
            for (bool suffix : {false, true})
                cut(position, suffix);
        if (maximum == 1)
            return;
        std::vector<Term> terms;
        for (const auto &[cover, r] : rectangles)
            terms.push_back({cover, r, int(wrap(r, 1).size()) + 1});
        std::sort(terms.begin(), terms.end(), [](const Term &a, const Term &b) {
            if (a.cost != b.cost)
                return a.cost < b.cost;
            int ca = a.cover.count(), cb = b.cover.count();
            return ca != cb ? ca > cb : a.r->s < b.r->s;
        });
        std::vector<Term> undominated;
        for (const auto &term : terms) {
            bool dominated = false;
            for (const auto &old : undominated)
                if (!(term.cover & ~old.cover)) {
                    dominated = true;
                    break;
                }
            if (!dominated)
                undominated.push_back(term);
        }
        terms = std::move(undominated);
        std::vector<std::vector<int>> choices(required.size());
        for (size_t i = 0; i < terms.size(); ++i)
            for (FiniteMask bits = terms[i].cover; bits; bits.clearFirst())
                choices[bits.first()].push_back(int(i));
        // Always offer a complete cover before starting the more expensive
        // exact additive-cost search. An interrupted search retains this result.
        FiniteMask missing = full;
        std::vector<R> greedy;
        int greedyCost = 0;
        while (missing) {
            budget.check();
            int best = -1;
            double score = -1;
            for (size_t i = 0; i < terms.size(); ++i) {
                int gain = (terms[i].cover & missing).count();
                double s = double(gain) / terms[i].cost;
                if (s > score) {
                    score = s;
                    best = int(i);
                }
            }
            greedy.push_back(terms[best].r);
            greedyCost += terms[best].cost;
            missing &= ~terms[best].cover;
        }
        offer(refine(alt(greedy)), "Endliche Sprache: Rechteckueberdeckung");
        if ((words.size() != required.size() || required.size() > 32) && terms.size() <= 5000) {
            // Don't-care relations can have many symmetric covers. Search
            // directly below a known incumbent instead of filling an entire
            // unrestricted subset-DP table before finding an improvement.
            int incumbent = std::min(greedyCost, int(wrap(baseline, 1).size()) + 1);
            int n = int(required.size());
            std::vector<std::vector<int>> prices;
            std::mt19937 random(unsigned(words.size() * 311 + required.size() * 73));
            std::vector<int> order(n);
            std::iota(order.begin(), order.end(), 0);
            for (int variant = 0; variant < 12; ++variant) {
                if (variant == 0)
                    std::sort(order.begin(), order.end(),
                              [&](int a, int b) { return choices[a].size() < choices[b].size(); });
                else
                    std::shuffle(order.begin(), order.end(), random);
                std::vector<int> slack, price(n);
                for (const auto &term : terms)
                    slack.push_back(term.cost);
                for (int p : order) {
                    int value = 1000000000;
                    for (int i : choices[p])
                        value = std::min(value, slack[i]);
                    price[p] = value;
                    for (int i : choices[p])
                        slack[i] -= value;
                }
                // These prices are a feasible dual solution: every term's
                // covered word prices sum to at most its additive cost.
                prices.push_back(std::move(price));
            }
            size_t nodes = 0;
            std::unordered_map<FiniteMask, int, FiniteMask::Hash> cheapestPrefix;
            std::vector<int> path;
            std::function<void(FiniteMask, int)> improve = [&](FiniteMask remaining, int spent) {
                if ((++nodes & 255) == 0)
                    budget.check();
                if (nodes > 250000)
                    throw Limit("Beschraenkte Mengenueberdeckung erschoepft");
                if (!remaining) {
                    if (spent < incumbent) {
                        incumbent = spent;
                        std::vector<R> cover;
                        for (int i : path)
                            cover.push_back(terms[i].r);
                        offer(refine(alt(cover)), "Endlicher Kontext: verbesserte Ueberdeckung");
                    }
                    return;
                }
                int lower = 0;
                for (const auto &price : prices) {
                    int value = 0;
                    for (FiniteMask bits = remaining; bits; bits.clearFirst())
                        value += price[bits.first()];
                    lower = std::max(lower, value);
                }
                if (spent + lower >= incumbent)
                    return;
                auto seen = cheapestPrefix.find(remaining);
                if (seen != cheapestPrefix.end() && seen->second <= spent)
                    return;
                cheapestPrefix[remaining] = spent;
                int pivot = -1, fewest = 1000000000;
                for (FiniteMask bits = remaining; bits; bits.clearFirst()) {
                    int p = bits.first(), count = 0;
                    for (int i : choices[p])
                        count += spent + terms[i].cost < incumbent;
                    if (count < fewest) {
                        fewest = count;
                        pivot = p;
                    }
                }
                if (!fewest)
                    return;
                std::vector<int> branches;
                double bestRatio = 0;
                for (size_t i = 0; i < terms.size(); ++i)
                    bestRatio = std::max(bestRatio, double((terms[i].cover & remaining).count()) /
                                                        terms[i].cost);
                if (bestRatio > 0 &&
                    spent + int(std::ceil(remaining.count() / bestRatio - 1e-9)) >= incumbent)
                    return;
                for (int i : choices[pivot])
                    if (spent + terms[i].cost < incumbent)
                        branches.push_back(i);
                std::sort(branches.begin(), branches.end(), [&](int a, int b) {
                    int ga = (terms[a].cover & remaining).count();
                    int gb = (terms[b].cover & remaining).count();
                    int left = ga * terms[b].cost, right = gb * terms[a].cost;
                    return left != right ? left > right : a < b;
                });
                for (int i : branches) {
                    path.push_back(i);
                    improve(remaining & ~terms[i].cover, spent + terms[i].cost);
                    path.pop_back();
                }
            };
            try {
                improve(full, 0);
            } catch (const Limit &) {
                // Complete improvements were published immediately.
            }
            return;
        }
        if (required.size() > 32 || terms.size() > 5000)
            return;
        struct Entry {
            int cost, choice;
        };
        std::unordered_map<FiniteMask, Entry, FiniteMask::Hash> memo;
        memo.emplace(0, Entry{0, -1});
        std::function<int(FiniteMask)> solve = [&](FiniteMask remaining) {
            auto found = memo.find(remaining);
            if (found != memo.end())
                return found->second.cost;
            if ((memo.size() & 255) == 0)
                budget.check();
            if (memo.size() >= 500000)
                throw Limit("Endlicher Mengenueberdeckungs-Vorrat erschoepft");
            int selected = -1;
            for (FiniteMask bits = remaining; bits; bits.clearFirst()) {
                int i = bits.first();
                if (selected < 0 || choices[i].size() < choices[selected].size())
                    selected = i;
            }
            Entry best{1000000000, -1};
            for (int i : choices[selected]) {
                int cost = terms[i].cost + solve(remaining & ~terms[i].cover);
                if (cost < best.cost)
                    best = {cost, i};
            }
            memo.emplace(remaining, best);
            return best.cost;
        };
        try {
            solve(full);
        } catch (const Limit &) {
            budget.check();
            return;
        }
        std::vector<R> exact;
        for (FiniteMask remaining = full; remaining;) {
            int i = memo.at(remaining).choice;
            exact.push_back(terms[i].r);
            remaining &= ~terms[i].cover;
        }
        offer(refine(alt(exact)), "Endliche Sprache: minimale Rechteckkosten");
    }
};

// Semantic finite windows inside an otherwise infinite expression. Every
// replacement is proved locally; no Optimizer or structural.hpp dependency is
// needed, so this can also run on candidates produced by other search methods.
struct FiniteLocalSearch {
    const Budget &budget;
    Limits limits;
    std::function<R(R)> refine;
    int fuel;
    std::map<std::pair<std::string, int>, R> memo;
    FiniteLocalSearch(const Budget &b, Limits l, std::function<R(R)> f, int attempts = 12)
        : budget(b), limits(l), refine(std::move(f)), fuel(attempts) {}

    static bool syntacticallyFinite(R r) {
        if (r->k == REP && r->hi == INF)
            return false;
        for (R child : r->sub)
            if (!syntacticallyFinite(child))
                return false;
        return true;
    }
    static bool shorterIn(R a, R b, int precedence) {
        size_t ca = wrap(a, precedence).size(), cb = wrap(b, precedence).size();
        return ca != cb ? ca < cb : better(a, b);
    }
    R finite(R r, int precedence) {
        if (fuel <= 0 || budget.done() || r->s.size() < 8 || r->s.size() > 700 ||
            !syntacticallyFinite(r))
            return r;
        auto k = std::make_pair(r->s, precedence);
        auto found = memo.find(k);
        if (found != memo.end())
            return found->second;
        --fuel;
        R best = r;
        try {
            DFA d = compile(r, limits, &budget);
            FiniteSearch search(
                d, budget, limits,
                [&](R candidate, const std::string &) {
                    if (shorterIn(candidate, best, precedence) &&
                        compare(d, compileRaw(Parser(candidate->s, false).parse(), limits, &budget),
                                limits, &budget)
                            .equal)
                        best = candidate;
                },
                refine);
            search.run();
        } catch (const Limit &) {
            // Keep the last individually proved result on exhaustion.
        }
        memo[k] = best;
        return best;
    }
    DFA bounded(const DFA &d, int length) {
        if (length > 32 || d.t.size() * size_t(length + 2) > 4096)
            throw Limit("Endlicher Kontext zu gross");
        int width = length + 2, n = int(d.t.size()) * width;
        DFA out;
        out.start = d.start * width;
        out.t.resize(n);
        out.accept.resize(n);
        for (size_t p = 0; p < d.t.size(); ++p)
            for (int k = 0; k < width; ++k) {
                int q = int(p) * width + k;
                out.accept[q] = k <= length && d.accept[p];
                for (int c = 0; c < 10; ++c)
                    out.t[q][c] = d.t[p][c] * width + std::min(width - 1, k + 1);
            }
        return minimize(out, &budget);
    }
    R alternatives(R r, int precedence) {
        if (r->k != ALT || fuel <= 0 || budget.done() || r->s.size() > 1000)
            return r;
        std::vector<R> mandatory, context;
        for (R child : r->sub)
            (syntacticallyFinite(child) ? mandatory : context).push_back(child);
        if (mandatory.size() < 2 || context.empty())
            return r;
        R finitePart = alt(mandatory), best = r;
        if (finitePart->s.size() < 8)
            return r;
        --fuel;
        try {
            DFA requiredDFA = compile(finitePart, limits, &budget);
            FiniteSearch extractor(
                requiredDFA, budget, limits, [](R, const std::string &) {}, refine);
            if (!extractor.extract() || extractor.words.empty())
                return r;
            int maximum = 0;
            for (const auto &word : extractor.words)
                maximum = std::max(maximum, int(word.size()));
            DFA whole = compile(r, limits, &budget);
            DFA allowed = bounded(whole, maximum);
            FiniteSearch cover(
                allowed, budget, limits,
                [&](R replacement, const std::string &) {
                    auto v = context;
                    v.push_back(replacement);
                    R candidate = refine(alt(v));
                    if (shorterIn(candidate, best, precedence) &&
                        compare(whole,
                                compileRaw(Parser(candidate->s, false).parse(), limits, &budget),
                                limits, &budget)
                            .equal)
                        best = candidate;
                },
                refine);
            cover.runRequired(extractor.words);
        } catch (const Limit &) {
        }
        return best;
    }
    R run(R r, int precedence = 1, int depth = 0) {
        if (budget.done() || fuel <= 0 || depth > 24)
            return r;
        int childPrecedence = r->k == CAT ? 2 : r->k == REP ? 4 : 1;
        std::vector<R> children;
        for (R child : r->sub)
            children.push_back(run(child, childPrecedence, depth + 1));
        r = rebuild(r, children);
        if (r->k == CAT && fuel > 0) {
            std::vector<R> v;
            for (size_t i = 0; i < r->sub.size();) {
                if (!syntacticallyFinite(r->sub[i])) {
                    v.push_back(r->sub[i++]);
                    continue;
                }
                size_t end = i;
                std::vector<R> run;
                while (end < r->sub.size() && syntacticallyFinite(r->sub[end]))
                    run.push_back(r->sub[end++]);
                v.push_back(finite(cat(run), 2));
                i = end;
            }
            r = cat(v);
        }
        r = alternatives(r, precedence);
        return finite(r, precedence);
    }
};
} // namespace rx
