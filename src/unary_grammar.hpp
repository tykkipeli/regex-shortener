#pragma once
#include "unary_evolution.hpp"
namespace rx {
// Enumerate short Boolean length polynomials, retaining only languages that
// can occur inside the target in some concatenation context. No numerical
// predicate, task identifier, target score or external dictionary is used.
// The bank is a heuristic grammar search; it is NOT an optimality proof.
struct UnaryGrammarSearch {
    using Polynomial = __uint128_t;
    struct Hash {
        size_t operator()(Polynomial v) const {
            return uint64_t(v) ^ (uint64_t(v >> 64) * 0x9e3779b97f4a7c15ULL);
        }
    };
    struct Text {
        Polynomial value;
        std::string s;
        int precedence;
    };
    const DFA &target;
    const Budget &budget;
    Limits limits;
    std::function<R(R)> refine;
    std::function<void(R, const std::string &)> offer;
    uint64_t seed;
    int digit = -1, maximum = 0;
    Polynomial wanted = 0;
    R best;
    size_t attempts = 0, generated = 0, proofs = 0;
    size_t bankLimit = 600000, termLimit = 250000;
    std::vector<Text> bank;
    std::unordered_map<Polynomial, int, Hash> known;
    std::vector<std::vector<int>> buckets;
    std::unordered_map<Polynomial, Text, Hash> terms;

    UnaryGrammarSearch(const DFA &d, const Budget &b, Limits l, std::function<R(R)> f,
                       std::function<void(R, const std::string &)> o, uint64_t start = 1)
        : target(d), budget(b), limits(l), refine(std::move(f)), offer(std::move(o)), seed(start) {}
    static int low(Polynomial v) {
        return uint64_t(v) ? __builtin_ctzll(uint64_t(v))
                           : 64 + __builtin_ctzll(uint64_t(v >> 64));
    }
    static int high(Polynomial v) {
        return uint64_t(v >> 64) ? 127 - __builtin_clzll(uint64_t(v >> 64))
                                 : 63 - __builtin_clzll(uint64_t(v));
    }
    static int count(Polynomial v) {
        return __builtin_popcountll(uint64_t(v)) + __builtin_popcountll(uint64_t(v >> 64));
    }
    static bool shorter(const Text &a, const Text &b) {
        return a.s.size() != b.s.size() ? a.s.size() < b.s.size() : a.s < b.s;
    }
    static std::string wrapText(const Text &a, int precedence) {
        return a.precedence < precedence ? "(" + a.s + ")" : a.s;
    }
    static Text concatenate(const Text &a, const Text &b, Polynomial value) {
        if (a.value == 1)
            return {value, b.s, b.precedence};
        if (b.value == 1)
            return {value, a.s, a.precedence};
        return {value, wrapText(a, 2) + wrapText(b, 2), 2};
    }
    Text literal(int n) const {
        std::string s;
        if (n <= 4)
            s.assign(size_t(n), char('0' + digit));
        else
            s = std::string(1, char('0' + digit)) + "{" + std::to_string(n) + "}";
        return {Polynomial(1) << n, s, n <= 1 ? 4 : n <= 4 ? 2 : 3};
    }
    bool deriveTarget() {
        UnaryEvolution arithmetic(target, budget, limits, refine, offer);
        if (!arithmetic.deriveTarget())
            return false;
        digit = arithmetic.digit;
        maximum = arithmetic.maximum;
        wanted = (Polynomial(arithmetic.wanted.hi) << 64) | arithmetic.wanted.lo;
        // Resource guards for this particular search, not restrictions on input.
        return maximum >= 2 && count(wanted) <= 40;
    }
    Polynomial positions(Polynomial value) const {
        if (!value || high(value) > maximum)
            return 0;
        Polynomial result = ~Polynomial(0);
        while (value) {
            result &= wanted >> low(value);
            if (!result)
                return 0;
            value &= value - 1;
        }
        return result;
    }
    Polynomial product(Polynomial a, Polynomial b) const {
        if (!a || !b || high(a) + high(b) > maximum)
            return 0;
        if (count(a) > count(b))
            std::swap(a, b);
        Polynomial result = 0;
        while (a) {
            result |= b << low(a);
            a &= a - 1;
        }
        return result;
    }
    void add(Polynomial value, std::string text, int precedence) {
        if (!value || text.size() >= buckets.size() || !positions(value))
            return;
        auto it = known.find(value);
        if (it != known.end()) {
            const Text &old = bank[it->second];
            if (old.s.size() < text.size() ||
                (old.s.size() == text.size() && old.precedence >= precedence))
                return;
        }
        if (bank.size() >= bankLimit)
            throw Limit("Speichergrenze der Laengengrammatik erreicht");
        int id = int(bank.size());
        known[value] = id;
        buckets[text.size()].push_back(id);
        bank.push_back({value, std::move(text), precedence});
        ++generated;
    }
    void generate(int maximumCost, const Budget &phase) {
        auto check = [&]() { budget.check(); phase.check(); };
        buckets.resize(size_t(maximumCost + 6));
        add(1, "", 4);
        add(2, std::string(1, char('0' + digit)), 4);
        for (int n = 2; n <= maximum; ++n) {
            Text text = literal(n);
            add(text.value, text.s, text.precedence);
        }
        for (int cost = 2; cost <= maximumCost; ++cost) {
            check();
            size_t end = bank.size();
            for (size_t id = 1; id < end; ++id) {
                if ((++attempts & 1023) == 0)
                    check();
                Text a = bank[id]; // add() can reallocate the bank.
                if (a.s.empty() || a.s.size() + 1 > size_t(cost) || high(a.value) == 0)
                    continue;
                std::string atom = wrapText(a, 4);
                Polynomial power = 1;
                for (int n = 0; n * high(a.value) <= maximum; ++n) {
                    if (n > 1) {
                        std::string text = atom + "{" + std::to_string(n) + "}";
                        if (text.size() == size_t(cost))
                            add(power, std::move(text), 3);
                    }
                    Polynomial sum = 0, next = power;
                    for (int m = n; m * high(a.value) <= maximum; ++m) {
                        sum |= next;
                        if (m > n) {
                            std::string suffix = n == 0 && m == 1 ? "?" :
                                "{" + std::to_string(n) + "," + std::to_string(m) + "}";
                            std::string text = atom + suffix;
                            if (text.size() == size_t(cost))
                                add(sum, std::move(text), 3);
                        }
                        next = product(next, a.value);
                        if (!next)
                            break;
                    }
                    power = product(power, a.value);
                    if (!power)
                        break;
                }
            }
            for (int ca = 0; ca < cost; ++ca)
                for (int cb = ca; cb < cost; ++cb) {
                    if (ca + cb > cost || ca + cb + 5 < cost)
                        continue;
                    // Lower cost buckets cannot change while generating this cost.
                    const auto &aa = buckets[ca];
                    const auto &bb = buckets[cb];
                    for (int ia : aa)
                        for (int ib : bb) {
                            if (ca == cb && ib < ia)
                                continue;
                            if ((++attempts & 8191) == 0)
                                check();
                            Text a = bank[ia], b = bank[ib];
                            int catCost = ca + cb + (a.precedence < 2 ? 2 : 0) +
                                          (b.precedence < 2 ? 2 : 0);
                            if (ca + cb + 1 == cost) {
                                Polynomial value = a.value | b.value;
                                if (value != a.value && value != b.value && positions(value))
                                    add(value, a.s + "|" + b.s, 1);
                            }
                            if (ca && cb && catCost == cost) {
                                Polynomial value = product(a.value, b.value);
                                if (value && positions(value))
                                    add(value, wrapText(a, 2) + wrapText(b, 2), 2);
                            }
                        }
                }
        }
    }
    void addTerm(Text text) {
        if (!text.value || (text.value & ~wanted))
            return;
        auto it = terms.find(text.value);
        if (it != terms.end()) {
            if (shorter(text, it->second))
                it->second = std::move(text);
        } else if (terms.size() < termLimit)
            terms.emplace(text.value, std::move(text));
    }
    void buildTerms(const Budget &phase) {
        size_t iteration = 0;
        for (const auto &[value, id] : known) {
            if ((++iteration & 255) == 0) { budget.check(); phase.check(); }
            const Text &text = bank[id];
            Polynomial shifts = positions(value);
            while (shifts) {
                int shift = low(shifts);
                addTerm(concatenate(literal(shift), text, value << shift));
                shifts &= shifts - 1;
            }
            if (!(value & 1))
                continue;
            // Maximal compatible quotients give larger product terms whose
            // factors are themselves synthesized by the grammar, not greedy
            // arithmetic progressions. They need not be disjoint in a cover.
            Polynomial quotient = positions(value);
            if (count(quotient) < 2)
                continue;
            auto found = known.find(quotient);
            if (found == known.end())
                continue;
            Polynomial sums = product(value, quotient);
            if (sums) {
                const Text &other = bank[found->second];
                addTerm(concatenate(text, other, sums));
                addTerm(concatenate(other, text, sums));
            }
        }
    }
    void prove(const std::string &text) {
        if (text.size() > best->s.size() + 8)
            return;
        budget.check();
        R candidate = refine(Parser(text, false).parse());
        if (!better(candidate, best))
            return;
        ++proofs;
        // Reparse the actual emitted spelling and perform full equivalence.
        DFA emitted = compileRaw(Parser(candidate->s, false).parse(), limits, &budget);
        if (!compare(target, emitted, limits, &budget).equal)
            return;
        best = candidate;
        offer(candidate, "Grammatik endlicher Laengenpolynome");
    }
    void cover(const Budget &phase) {
        if (terms.empty())
            return;
        auto check = [&]() { budget.check(); phase.check(); };
        auto full = terms.find(wanted);
        if (full != terms.end())
            prove(full->second.s);
        // Complement joins are exact covers and are particularly cheap. Keep
        // the unpruned bank here: pruning by coverage can lose a useful shape.
        size_t iteration = 0;
        for (const auto &[mask, text] : terms) {
            if ((++iteration & 255) == 0)
                check();
            auto other = terms.find(wanted & ~mask);
            if (other != terms.end() &&
                text.s.size() + other->second.s.size() + 1 <= best->s.size() + 2)
                prove(text.s + "|" + other->second.s);
        }
        // General overlapping weighted set covers; bounded branch-and-bound.
        // Dominance below is a heuristic for printed/normalized cost only.
        struct Piece { Polynomial mask; std::string s; int cost; };
        std::vector<Piece> sorted, pieces;
        for (const auto &[mask, text] : terms)
            sorted.push_back({mask, text.s, int(text.s.size()) + 1});
        std::sort(sorted.begin(), sorted.end(), [](const Piece &a, const Piece &b) {
            return a.cost != b.cost ? a.cost < b.cost :
                   count(a.mask) != count(b.mask) ? count(a.mask) > count(b.mask) : a.s < b.s;
        });
        std::array<std::vector<int>, 128> containing;
        for (const Piece &piece : sorted) {
            if ((++iteration & 255) == 0) check();
            int pivot = low(piece.mask);
            for (Polynomial bits = piece.mask; bits; bits &= bits - 1)
                if (containing[low(bits)].size() < containing[pivot].size()) pivot = low(bits);
            bool redundant = false;
            for (int id : containing[pivot])
                if ((pieces[id].mask & piece.mask) == piece.mask) { redundant = true; break; }
            if (redundant) continue;
            for (Polynomial bits = piece.mask; bits; bits &= bits - 1)
                containing[low(bits)].push_back(int(pieces.size()));
            pieces.push_back(piece);
        }
        std::vector<int> selected;
        auto publish = [&]() {
            std::string text;
            bool first = true;
            for (int id : selected) {
                if (!first) text += '|';
                first = false;
                text += pieces[id].s;
            }
            prove(text);
        };
        Polynomial missing = wanted;
        int upper = 0;
        while (missing) {
            int choice = -1;
            double score = 0;
            for (size_t j = 0; j < pieces.size(); ++j) {
                double ratio = double(count(missing & pieces[j].mask)) / pieces[j].cost;
                if (ratio > score) { score = ratio; choice = int(j); }
            }
            if (choice < 0) return;
            selected.push_back(choice);
            upper += pieces[choice].cost;
            missing &= ~pieces[choice].mask;
        }
        publish();
        selected.clear();
        // best may have improved via a complementary pair before pruning.
        upper = std::min(upper, int(best->s.size()) + 1);
        std::unordered_map<Polynomial, int, Hash> seen;
        size_t nodes = 0;
        std::mt19937_64 random(seed);
        std::function<void(Polynomial, int)> solve = [&](Polynomial need, int cost) {
            if ((++nodes & 63) == 0) check();
            if (!need) { upper = std::min(upper, cost); publish(); return; }
            if (cost >= upper + 2) return;
            auto old = seen.find(need);
            if (old != seen.end() && old->second < cost) return;
            seen[need] = cost;
            if (seen.size() > 250000) seen.clear();
            int pivot = low(need);
            for (Polynomial bits = need; bits; bits &= bits - 1)
                if (containing[low(bits)].size() < containing[pivot].size()) pivot = low(bits);
            double ratio = 0;
            for (const auto &piece : pieces)
                ratio = std::max(ratio, double(count(piece.mask & need)) / piece.cost);
            if (cost + int(std::ceil(count(need) / ratio - 1e-9)) >= upper + 2) return;
            auto branches = containing[pivot];
            if (seed != 1) std::shuffle(branches.begin(), branches.end(), random);
            std::stable_sort(branches.begin(), branches.end(), [&](int a, int b) {
                double x = double(count(pieces[a].mask & need)) / pieces[a].cost;
                double y = double(count(pieces[b].mask & need)) / pieces[b].cost;
                return x > y;
            });
            std::unordered_map<Polynomial, int, Hash> distinct;
            for (int id : branches) {
                const Piece &piece = pieces[id];
                if (cost + piece.cost >= upper + 2) continue;
                Polynomial remaining = need & ~piece.mask;
                auto duplicate = distinct.find(remaining);
                if (duplicate != distinct.end() && duplicate->second < piece.cost) continue;
                distinct[remaining] = piece.cost;
                selected.push_back(id);
                solve(remaining, cost + piece.cost);
                selected.pop_back();
            }
        };
        solve(wanted, 0);
    }
    bool run(R start, double seconds, int maximumCost = 25) {
        if (seconds <= 0 || !deriveTarget())
            return false;
        best = start;
        Budget phase(seconds);
        phase.end = std::min(phase.end, budget.end);
        Budget generation(seconds * .65);
        generation.end = std::min(generation.end, phase.end);
        bankLimit = maximumCost <= 25 ? 600000 : 1200000;
        try { generate(maximumCost, generation); }
        catch (const Limit &) { budget.check(); }
        Budget assembly(seconds * .2);
        assembly.end = std::min(assembly.end, phase.end);
        try { buildTerms(assembly); }
        catch (const Limit &) { budget.check(); }
        try { cover(phase); }
        catch (const Limit &) { budget.check(); }
        return true;
    }
};
} // namespace rx
