#pragma once
#include "automata.hpp"
namespace rx {
// A finite unary regex is a Boolean length polynomial. This optimizer works
// on those polynomials, deliberately allowing incorrect intermediate
// languages. Exact DFA equivalence remains the gate for publishing a result.
struct UnaryEvolution {
    struct Value {
        uint64_t lo = 0, hi = 0;
        bool overflow = false;
        bool empty() const {
            return !lo && !hi && !overflow;
        }
        bool operator==(const Value &b) const {
            return lo == b.lo && hi == b.hi && overflow == b.overflow;
        }
    };
    const DFA &target;
    const Budget &budget;
    Limits limits;
    std::function<R(R)> refine;
    std::function<void(R, const std::string &)> offer;
    std::mt19937_64 random;
    int digit = -1, maximum = 0;
    Value wanted;
    R best;
    size_t evaluations = 0;
    std::unordered_map<std::string, Value> cache;
    struct Individual {
        R expression;
        Value value;
        int error;
    };
    std::vector<std::vector<Individual>> islands;
    std::vector<Individual> dictionary;
    std::vector<Individual> walkers;
    std::unordered_set<std::string> texts;
    UnaryEvolution(const DFA &d, const Budget &b, Limits l, std::function<R(R)> f,
                   std::function<void(R, const std::string &)> o, uint64_t seed = 1)
        : target(d), budget(b), limits(l), refine(std::move(f)), offer(std::move(o)), random(seed) {
    }
    static int count(Value a) {
        return __builtin_popcountll(a.lo) + __builtin_popcountll(a.hi);
    }
    static Value either(Value a, Value b) {
        return {a.lo | b.lo, a.hi | b.hi, a.overflow || b.overflow};
    }
    static Value shift(Value a, unsigned amount) {
        if (!amount)
            return a;
        if (amount >= 128)
            return {0, 0, !a.empty()};
        Value result;
        result.overflow = a.overflow;
        if (amount < 64) {
            result.lo = a.lo << amount;
            result.hi = (a.hi << amount) | (a.lo >> (64 - amount));
            result.overflow |= (a.hi >> (64 - amount)) != 0;
        } else {
            result.hi = a.lo << (amount - 64);
            result.overflow |= a.hi != 0;
            if (amount > 64)
                result.overflow |= (a.lo >> (128 - amount)) != 0;
        }
        return result;
    }
    static Value product(Value a, Value b) {
        if (a.empty() || b.empty())
            return {};
        if (count(a) > count(b))
            std::swap(a, b);
        Value result{0, 0, a.overflow || b.overflow};
        for (uint64_t bits = a.lo; bits; bits &= bits - 1)
            result = either(result, shift(b, unsigned(__builtin_ctzll(bits))));
        for (uint64_t bits = a.hi; bits; bits &= bits - 1)
            result = either(result, shift(b, 64u + unsigned(__builtin_ctzll(bits))));
        return result;
    }
    static Value power(Value a, I n) {
        Value result{1, 0, false};
        while (n) {
            if (n & 1)
                result = product(result, a);
            n >>= 1;
            if (n)
                a = product(a, a);
        }
        return result;
    }
    // Return (a^n, 1|a|...|a^(n-1)) using logarithmically many convolutions.
    static std::pair<Value, Value> powerSum(Value a, I n) {
        Value p{1, 0, false}, sum, basePower = a, baseSum{1, 0, false};
        while (n) {
            if (n & 1) {
                sum = either(sum, product(p, baseSum));
                p = product(p, basePower);
            }
            n >>= 1;
            if (n) {
                baseSum = either(baseSum, product(basePower, baseSum));
                basePower = product(basePower, basePower);
            }
        }
        return {p, sum};
    }
    Value evaluate(R r) {
        auto known = cache.find(r->s);
        if (known != cache.end())
            return known->second;
        Value value;
        switch (r->k) {
        case EMPTY:
            break;
        case EPS:
            value.lo = 1;
            break;
        case CHAR:
            if (r->mask == (1 << digit))
                value.lo = 2;
            else
                value.overflow = true;
            break;
        case ALT:
            for (R child : r->sub)
                value = either(value, evaluate(child));
            break;
        case CAT:
            value.lo = 1;
            for (R child : r->sub)
                value = product(value, evaluate(child));
            break;
        case REP: {
            Value a = evaluate(r->sub[0]);
            if (r->hi == INF) {
                if ((a.lo & ~1ULL) || a.hi || a.overflow)
                    value = {0, 0, true};
                else
                    value = r->lo == 0 ? Value{1, 0, false} : a;
            } else {
                value = product(power(a, r->lo), powerSum(a, r->hi - r->lo + 1).second);
            }
            break;
        }
        }
        if (cache.size() < 100000)
            cache.emplace(r->s, value);
        ++evaluations;
        return value;
    }
    int distance(Value a) const {
        return __builtin_popcountll(a.lo ^ wanted.lo) + __builtin_popcountll(a.hi ^ wanted.hi) +
               (a.overflow ? 12 : 0);
    }
    bool deriveTarget() {
        int n = int(target.t.size());
        std::vector<std::vector<int>> predecessors(n);
        std::vector<bool> live(n), reached(n);
        std::queue<int> work;
        work.push(target.start);
        reached[target.start] = true;
        while (!work.empty()) {
            int p = work.front();
            work.pop();
            for (int q : target.t[p]) {
                predecessors[q].push_back(p);
                if (!reached[q]) {
                    reached[q] = true;
                    work.push(q);
                }
            }
        }
        for (int p = 0; p < n; ++p)
            if (reached[p] && target.accept[p]) {
                live[p] = true;
                work.push(p);
            }
        while (!work.empty()) {
            int q = work.front();
            work.pop();
            for (int p : predecessors[q])
                if (!live[p]) {
                    live[p] = true;
                    work.push(p);
                }
        }
        int mask = 0;
        for (int p = 0; p < n; ++p)
            if (reached[p] && live[p])
                for (int c = 0; c < 10; ++c)
                    if (live[target.t[p][c]])
                        mask |= 1 << c;
        if (__builtin_popcount(unsigned(mask)) != 1)
            return false;
        digit = __builtin_ctz(unsigned(mask));
        std::vector<int> first(n, -1), path;
        int p = target.start;
        while (first[p] < 0) {
            first[p] = int(path.size());
            path.push_back(p);
            if (target.accept[p]) {
                int length = int(path.size()) - 1;
                if (length > 120)
                    return false;
                wanted = either(wanted, shift(Value{1, 0, false}, unsigned(length)));
                maximum = length;
            }
            p = target.t[p][digit];
        }
        for (size_t i = size_t(first[p]); i < path.size(); ++i)
            if (target.accept[path[i]])
                return false;
        return count(wanted) >= 3;
    }
    R literal(int n) const {
        return rep(chars(1 << digit), n, n);
    }
    R simple(Value values) const {
        std::vector<int> lengths;
        for (int i = 0; i < 128; ++i)
            if ((i < 64 ? values.lo >> i : values.hi >> (i - 64)) & 1)
                lengths.push_back(i);
        if (lengths.empty())
            return zero();
        std::vector<R> terms;
        while (!lengths.empty()) {
            size_t chosen = 1;
            int step = 1;
            for (size_t j = 1; j < lengths.size(); ++j) {
                int d = lengths[j] - lengths[0];
                size_t k = 1;
                while (std::binary_search(lengths.begin(), lengths.end(), lengths[0] + int(k) * d))
                    ++k;
                if (k > chosen) {
                    chosen = k;
                    step = d;
                }
            }
            int start = lengths.front();
            if (chosen == 1)
                terms.push_back(literal(start));
            else
                terms.push_back(cat({literal(start % step), rep(literal(step), start / step,
                                                                start / step + int(chosen) - 1)}));
            lengths.erase(std::remove_if(lengths.begin(), lengths.end(),
                                         [&](int n) {
                                             return n >= start && (n - start) % step == 0 &&
                                                    (n - start) / step < int(chosen);
                                         }),
                          lengths.end());
        }
        return alt(terms);
    }
    void prove(R candidate) {
        candidate = refine(candidate);
        if (!better(candidate, best))
            return;
        if (!(evaluate(candidate) == wanted))
            return;
        try {
            R printed = Parser(candidate->s, false).parse();
            if (!compare(target, compileRaw(printed, limits, &budget), limits, &budget).equal)
                return;
        } catch (const Limit &) {
            budget.check();
            return;
        }
        best = candidate;
        offer(candidate, "Evolution endlicher Laengenpolynome");
    }
    void insert(R r, bool addParts = false) {
        if (!r || r->s.size() > std::max(size_t(180), best->s.size() * 2 + 16))
            return;
        if (!texts.insert(r->s).second)
            return;
        Value value = evaluate(r);
        int error = distance(value);
        Individual individual{r, value, error};
        int bucket = std::min(63, error);
        auto &island = islands[bucket];
        bool duplicate = false;
        for (const auto &old : island)
            if (old.expression->s == r->s)
                duplicate = true;
        if (!duplicate) {
            island.push_back(individual);
            std::sort(island.begin(), island.end(), [](const auto &a, const auto &b) {
                return better(a.expression, b.expression);
            });
            size_t keep = bucket == 0 ? 20 : 8;
            if (island.size() > keep)
                island.resize(keep);
        }
        if (dictionary.size() < 12000 &&
            (addParts ||
             (!value.overflow && (count(value) <= 10 || error <= 16) && (random() & 7) == 0)))
            dictionary.push_back(individual);
        if (value == wanted)
            prove(r);
        else if (!value.overflow && !(value.lo & ~wanted.lo) && !(value.hi & ~wanted.hi)) {
            Value missing{wanted.lo & ~value.lo, wanted.hi & ~value.hi, false};
            if (count(missing) <= 7 || r->s.size() + count(missing) * 3 < best->s.size()) {
                R repaired = alt({r, simple(missing)});
                if (repaired->s.size() <= best->s.size() + 16)
                    prove(repaired);
            }
        }
        if (addParts)
            for (R child : r->sub)
                insert(child, true);
    }
    static int nodes(R r) {
        int result = 1;
        for (R child : r->sub)
            result += nodes(child);
        return result;
    }
    R replace(R r, int &position, const std::function<R(R)> &mutation) {
        if (position-- == 0)
            return mutation(r);
        if (r->sub.empty())
            return r;
        std::vector<R> children;
        for (R child : r->sub)
            children.push_back(position >= 0 ? replace(child, position, mutation) : child);
        if (r->k == CAT)
            return cat(children);
        if (r->k == ALT)
            return alt(children);
        return rep(children[0], r->lo, r->hi);
    }
    R donor() {
        if (dictionary.empty() || random() % 5 == 0)
            return literal(int(random() % unsigned(maximum + 1)));
        return dictionary[random() % dictionary.size()].expression;
    }
    R mutate(R r) {
        switch (random() % 11) {
        case 0:
            return donor();
        case 1:
        case 2: {
            if (r->k == REP && r->hi != INF) {
                I lo = r->lo, hi = r->hi;
                int delta = int(random() % 7) - 3;
                if (random() % 3 == 0)
                    delta = int(random() % unsigned(maximum + 1)) - int(hi);
                if (random() % 2)
                    lo = std::max(I(0), lo + delta);
                else
                    hi = std::max(I(0), hi + delta);
                if (lo > hi)
                    std::swap(lo, hi);
                return rep(r->sub[0], lo, hi);
            }
            int count = int(random() % 4);
            return rep(r, count, count);
        }
        case 3:
            return random() % 2 ? cat({r, donor()}) : cat({donor(), r});
        case 4:
            return alt({r, donor()});
        case 5:
            if ((r->k == ALT || r->k == CAT) && r->sub.size() > 1) {
                auto sub = r->sub;
                sub.erase(sub.begin() + random() % sub.size());
                return r->k == ALT ? alt(sub) : cat(sub);
            }
            return eps();
        case 6:
            if (!r->sub.empty())
                return r->sub[random() % r->sub.size()];
            return rep(r, 0, 1);
        case 7:
            if (r->k == CAT && r->sub.size() >= 2) {
                auto sub = r->sub;
                std::shuffle(sub.begin(), sub.end(), random);
                return cat(sub);
            }
            return rep(r, 0, int(random() % 4));
        case 8: {
            Value a = evaluate(r);
            R chosen;
            int closest = 100000;
            for (int trial = 0; trial < 12 && !dictionary.empty(); ++trial) {
                const auto &item = dictionary[random() % dictionary.size()];
                int score = __builtin_popcountll(a.lo ^ item.value.lo) +
                            __builtin_popcountll(a.hi ^ item.value.hi) +
                            (a.overflow != item.value.overflow ? 16 : 0);
                if (score < closest) {
                    closest = score;
                    chosen = item.expression;
                }
            }
            return chosen ? chosen : donor();
        }
        case 9:
            if (r->k == CAT && r->sub.size() <= 8) {
                for (size_t i = 0; i < r->sub.size(); ++i)
                    if (r->sub[i]->k == ALT && r->sub[i]->sub.size() <= 6) {
                        std::vector<R> alternatives;
                        for (R term : r->sub[i]->sub) {
                            auto factors = r->sub;
                            factors[i] = term;
                            alternatives.push_back(cat(factors));
                        }
                        return alt(alternatives);
                    }
            }
            return refine(r);
        default:
            return literal(int(random() % unsigned(maximum + 1)));
        }
    }
    void run(R seed, double seconds) {
        if (!deriveTarget() || seconds <= 0)
            return;
        best = seed;
        islands.resize(64);
        Budget local(seconds);
        try {
            insert(seed, true);
            for (int length = 0; length <= maximum; ++length)
                insert(literal(length), true);
            insert(simple(wanted), true);
            for (int i = 0; i < 24; ++i)
                walkers.push_back({seed, wanted, 0});
            for (size_t iteration = 0; iteration < 2000000; ++iteration) {
                if ((iteration & 63) == 0) {
                    budget.check();
                    local.check();
                }
                R parent = best;
                int walker = random() % 3 == 0 ? int(random() % walkers.size()) : -1;
                if (walker >= 0)
                    parent = walkers[walker].expression;
                else if (random() % 6) {
                    int bucket = int(random() % 64);
                    if (random() % 2 == 0)
                        bucket = int(random() % 12);
                    for (int tries = 0; tries < 64 && islands[bucket].empty(); ++tries)
                        bucket = (bucket + 1) % 64;
                    if (!islands[bucket].empty()) {
                        const auto &pool = islands[bucket];
                        parent = pool[random() % pool.size()].expression;
                    }
                }
                int size = nodes(parent);
                if (size > 160)
                    continue;
                int position = int(random() % unsigned(size));
                R child = replace(parent, position, [&](R r) { return mutate(r); });
                insert(child);
                if (walker >= 0 &&
                    child->s.size() <= std::max(size_t(180), best->s.size() * 2 + 16)) {
                    Value value = evaluate(child);
                    int error = distance(value);
                    int penalty = 2 << (walker % 6);
                    double before =
                        walkers[walker].expression->s.size() + penalty * walkers[walker].error;
                    double after = child->s.size() + penalty * error;
                    double temperature = 3.0 + (walker / 6) * 5.0;
                    double draw = std::uniform_real_distribution<double>(0, 1)(random);
                    if (after <= before || draw < std::exp((before - after) / temperature))
                        walkers[walker] = {child, value, error};
                }
                if (iteration % 4093 == 0)
                    walkers[random() % walkers.size()] = {best, wanted, 0};
                if (iteration % 257 == 0 && !dictionary.empty()) {
                    R a = donor(), b = donor();
                    insert(random() % 2 ? alt({a, b}) : cat({a, b}));
                }
                if (texts.size() > 180000) {
                    texts.clear();
                    for (const auto &pool : islands)
                        for (const auto &individual : pool)
                            texts.insert(individual.expression->s);
                }
            }
        } catch (const Limit &) {
            budget.check();
        }
    }
};
} // namespace rx
