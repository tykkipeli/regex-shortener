#pragma once
#include "automata.hpp"
#include <tuple>

namespace rx {
// Membership on a substring-closed finite set is a congruence for union,
// concatenation and repetition. This is an abstraction, never an equivalence
// test: exhaustive absence in the abstraction is a sound lower bound.
struct FiniteObservation {
    std::vector<std::string> words;
    std::vector<std::vector<std::pair<int, int>>> splits;
    int maximum = 0;
    explicit FiniteObservation(const std::set<std::string> &samples) {
        words.assign(samples.begin(), samples.end());
        std::sort(words.begin(), words.end(), [](const auto &a, const auto &b) {
            return a.size() != b.size() ? a.size() < b.size() : a < b;
        });
        if (words.empty() || words.front() != "" || words.size() > 63)
            throw std::runtime_error("Invalid finite observation");
        std::map<std::string, int> index;
        for (size_t i = 0; i < words.size(); ++i)
            index[words[i]] = int(i);
        for (const auto &word : words) {
            maximum = std::max(maximum, int(word.size()));
            std::vector<std::pair<int, int>> cuts;
            for (size_t p = 0; p <= word.size(); ++p) {
                auto a = index.find(word.substr(0, p)), b = index.find(word.substr(p));
                if (a == index.end() || b == index.end())
                    throw std::runtime_error("Observation is not substring closed");
                cuts.push_back({a->second, b->second});
            }
            splits.push_back(std::move(cuts));
        }
    }
    uint64_t product(uint64_t a, uint64_t b) const {
        if (!a || !b) return 0;
        if (a == 1) return b;
        if (b == 1) return a;
        uint64_t result = 0;
        for (size_t i = 0; i < splits.size(); ++i)
            for (auto cut : splits[i])
                if ((a & (1ULL << cut.first)) && (b & (1ULL << cut.second))) {
                    result |= 1ULL << i;
                    break;
                }
        return result;
    }
    uint64_t repeat(uint64_t a, I lo, I hi) const {
        int cap = maximum + 1;
        int lower = int(std::min(lo, I(cap)));
        int upper = hi == INF ? cap : int(std::min(hi, I(cap)));
        uint64_t p = 1, result = 0;
        for (int n = 0; n <= upper; ++n) {
            if (n >= lower) result |= p;
            p = product(p, a);
        }
        return result;
    }
    uint64_t evaluate(R r) const {
        switch (r->k) {
        case EMPTY: return 0;
        case EPS: return 1;
        case CHAR: {
            uint64_t result = 0;
            for (size_t i = 1; i < words.size(); ++i)
                if (words[i].size() == 1 && (r->mask & (1 << (words[i][0]-'0'))))
                    result |= 1ULL << i;
            return result;
        }
        case ALT: {
            uint64_t result = 0;
            for (R c : r->sub) result |= evaluate(c);
            return result;
        }
        case CAT: {
            uint64_t result = 1;
            for (R c : r->sub) result = product(result, evaluate(c));
            return result;
        }
        case REP: return repeat(evaluate(r->sub[0]), r->lo, r->hi);
        }
        return 0;
    }
};

struct OptimalityResult {
    bool proven = false;
    size_t lowerBound = 0, profiles = 0, refinements = 0;
    std::string expression, reason = "disabled", method = "none";
    double seconds = 0;
};

struct OptimalityCheck {
    const DFA &target;
    Limits limits;
    size_t profileLimit = 250000;
    explicit OptimalityCheck(const DFA &d, Limits l = {}) : target(d), limits(l) {}
    // Live support, plus productive-cycle detection. Digits in a zero-count
    // operand are irrelevant: removing that operand never lengthens a regex.
    std::pair<int, bool> invariants(const Budget &budget) const {
        size_t n = target.t.size();
        std::vector<std::vector<int>> pred(n);
        std::vector<bool> live(n), reached(n);
        std::queue<int> q;
        q.push(target.start); reached[target.start] = true;
        while (!q.empty()) {
            budget.check();
            int p = q.front(); q.pop();
            for (int v : target.t[p]) {
                pred[v].push_back(p);
                if (!reached[v]) { reached[v] = true; q.push(v); }
            }
        }
        for (size_t p = 0; p < n; ++p)
            if (reached[p] && target.accept[p]) { live[p] = true; q.push(int(p)); }
        while (!q.empty()) {
            int v = q.front(); q.pop();
            for (int p : pred[v])
                if (!live[p]) { live[p] = true; q.push(p); }
        }
        int mask = 0, productive = 0;
        std::vector<int> indegree(n);
        for (size_t p = 0; p < n; ++p)
            if (reached[p] && live[p]) {
                ++productive;
                for (int c = 0; c < 10; ++c)
                    if (live[target.t[p][c]]) { mask |= 1 << c; ++indegree[target.t[p][c]]; }
            }
        for (size_t p = 0; p < n; ++p)
            if (reached[p] && live[p] && !indegree[p]) q.push(int(p));
        while (!q.empty()) {
            int p = q.front(); q.pop(); --productive;
            for (int v : target.t[p])
                if (live[v] && !--indegree[v]) q.push(v);
        }
        return {mask, productive > 0};
    }
    // A relaxation: cover every productive digit by literal/class occurrences,
    // ignoring every other syntax character. Infinity needs an additional
    // unbounded-quantifier character, which cannot overlap a digit occurrence.
    static size_t supportBound(int mask, bool infinite, const Budget *budget = nullptr) {
        std::vector<int> cost(1024, 1000000);
        std::array<int,1024> atom{};
        for (int part=mask;part;part=(part-1)&mask)
            atom[part]=std::min(__builtin_popcount(unsigned(part)),int(charset(part).size()));
        cost[0] = 0;
        for (int covered = 0; covered < 1024; ++covered)
            if (!(covered & ~mask) && cost[covered] < 1000000) {
                if (budget) budget->check();
                for (int part = mask; part; part = (part - 1) & mask) {
                    int next = covered | part;
                    cost[next] = std::min(cost[next], cost[covered] + atom[part]);
                }
            }
        return size_t(cost[mask] + int(infinite));
    }
    struct Hit { bool found = false; int cost = 0; std::string text; };
    Hit enumerate(const FiniteObservation &obs, uint64_t wanted, int cutoff, int mask,
                  const Budget &budget, OptimalityResult &result) const {
        enum Category { EXPR, SEQ, ITEM, ATOM };
        struct State { uint64_t value; int cost, category; std::string text; };
        std::vector<State> states;
        std::array<std::unordered_map<uint64_t, size_t>, 4> best;
        std::array<std::vector<size_t>, 4> settled;
        using Pending = std::pair<int, size_t>;
        std::priority_queue<Pending, std::vector<Pending>, std::greater<Pending>> queue;
        size_t operations = 0;
        auto add = [&](int category, uint64_t value, std::string text) {
            if ((++operations & 255) == 0) budget.check();
            int cost = int(text.size());
            if (cost >= cutoff) return;
            auto old = best[category].find(value);
            if (old != best[category].end() && states[old->second].cost <= cost) return;
            if (states.size() >= profileLimit) throw Limit("Optimality profile limit");
            size_t index = states.size();
            states.push_back({value, cost, category, std::move(text)});
            best[category][value] = index;
            queue.push({cost, index});
            ++result.profiles;
        };
        add(EXPR, 1, ""); add(SEQ, 1, "");
        for (int part = mask; part; part = (part - 1) & mask)
            add(ATOM, obs.evaluate(chars(part)), charset(part));
        while (!queue.empty()) {
            budget.check();
            size_t index = queue.top().second; queue.pop();
            State s = states[index]; // Adding states may reallocate the arena.
            if (best[s.category][s.value] != index) continue;
            if (s.category == EXPR && s.value == wanted) return {true, s.cost, s.text};
            // Both operands of a binary grammar production are combined when
            // its later-settled operand is visited. Zero-cost promotions retain
            // the nonterminal, so no parenthesis cost is lost.
            if (s.category == EXPR) {
                add(ATOM, s.value, "(" + s.text + ")");
                auto partners = settled[SEQ];
                for (size_t j : partners) {
                    const State t = states[j];
                    if (s.cost + t.cost + 1 < cutoff)
                        add(EXPR, s.value | t.value, s.text + "|" + t.text);
                }
            } else if (s.category == SEQ) {
                add(EXPR, s.value, s.text);
                auto partners = settled[EXPR];
                for (size_t j : partners) {
                    const State t = states[j];
                    if (s.cost + t.cost + 1 < cutoff)
                        add(EXPR, s.value | t.value, t.text + "|" + s.text);
                }
                partners = settled[ITEM];
                for (size_t j : partners) {
                    const State t = states[j];
                    if (s.cost + t.cost < cutoff)
                        add(SEQ, obs.product(s.value, t.value), s.text + t.text);
                }
            } else if (s.category == ITEM) {
                add(SEQ, s.value, s.text);
                auto partners = settled[SEQ];
                for (size_t j : partners) {
                    const State t = states[j];
                    if (s.cost + t.cost < cutoff)
                        add(SEQ, obs.product(t.value, s.value), t.text + s.text);
                }
            } else {
                add(ITEM, s.value, s.text);
                add(ITEM, obs.repeat(s.value, 0, INF), s.text + "*");
                add(ITEM, obs.repeat(s.value, 1, INF), s.text + "+");
                add(ITEM, obs.repeat(s.value, 0, 1), s.text + "?");
                int cap = obs.maximum + 1;
                std::vector<uint64_t> powers(cap + 1, 1);
                for (int n = 1; n <= cap; ++n) powers[n] = obs.product(powers[n-1], s.value);
                for (int lo = 0; lo <= cap; ++lo) {
                    uint64_t range = 0;
                    for (int hi = lo; hi <= cap; ++hi) {
                        range |= powers[hi];
                        std::string suffix = lo == hi ? "{" + std::to_string(lo) + "}" :
                            "{" + std::to_string(lo) + "," + std::to_string(hi) + "}";
                        add(ITEM, range, s.text + suffix);
                    }
                    uint64_t tail = range;
                    add(ITEM, tail, s.text + "{" + std::to_string(lo) + ",}");
                }
            }
            settled[s.category].push_back(index);
        }
        return {false, cutoff, ""};
    }
    OptimalityResult run(const std::string &expression, double seconds = 0.25,
                         int maximumLength = 8) const {
        OptimalityResult result;
        result.expression = expression;
        if (seconds <= 0) return result;
        auto started = Budget::Clock::now();
        Budget budget(seconds);
        result.method = "finite_observation_complete_grammar";
        try {
            if (!compare(target,compileRaw(Parser(expression,false).parse(),limits,&budget),limits,&budget).equal) {
                result.reason="invalid_upper_bound";
                result.seconds=std::chrono::duration<double>(Budget::Clock::now()-started).count();
                return result;
            }
            auto properties = invariants(budget);
            int mask = properties.first;
            result.lowerBound = supportBound(mask, properties.second, &budget);
            if (result.lowerBound >= expression.size()) {
                result.proven = true; result.reason = "support_and_infinity_bound";
                result.method = "support_and_infinity_bound";
            } else {
                std::set<std::string> samples{""};
                for (int c = 0; c < 10; ++c) if (mask & (1 << c)) samples.insert(std::string(1,char('0'+c)));
                std::vector<std::string> frontier{""};
                for (int depth = 1; depth <= 5; ++depth) {
                    std::vector<std::string> next;
                    for (const auto &w : frontier)
                        for (int c = 0; c < 10; ++c) if (mask & (1 << c)) next.push_back(w+char('0'+c));
                    if (samples.size()+next.size()>63) break;
                    samples.insert(next.begin(),next.end()); frontier=std::move(next);
                }
                int cutoff = std::min(int(expression.size()), maximumLength);
                for (;;) {
                    budget.check();
                    FiniteObservation obs(samples);
                    uint64_t wanted = 0;
                    for (size_t i=0;i<obs.words.size();++i)
                        if (matches(target,obs.words[i])) wanted |= 1ULL<<i;
                    Hit hit = enumerate(obs,wanted,cutoff,mask,budget,result);
                    result.lowerBound = std::max(result.lowerBound,size_t(hit.cost));
                    if (!hit.found) {
                        result.proven = cutoff >= int(result.expression.size());
                        result.reason = result.proven ? "all_shorter_excluded" : "length_limit";
                        break;
                    }
                    // An abstract match only becomes an upper bound after an
                    // independent parse and full automaton-product proof.
                    auto proof=compare(target,compileRaw(Parser(hit.text,false).parse(),limits,&budget),limits,&budget);
                    if (proof.equal) {
                        result.expression=hit.text; result.proven=true;
                        result.reason="exact_match_at_proved_lower_bound"; break;
                    }
                    if (proof.witness.size()>62) { result.reason="witness_limit"; break; }
                    auto expanded=samples;
                    for (size_t a=0;a<=proof.witness.size();++a)
                        for (size_t b=a;b<=proof.witness.size();++b) expanded.insert(proof.witness.substr(a,b-a));
                    if (expanded.size()>63) { result.reason="witness_limit"; break; }
                    samples=std::move(expanded); ++result.refinements;
                }
            }
        } catch (const Limit &) { result.reason=interrupted.load() ? "interrupted" : "resource_limit"; }
        result.seconds=std::chrono::duration<double>(Budget::Clock::now()-started).count();
        return result;
    }
};
} // namespace rx
