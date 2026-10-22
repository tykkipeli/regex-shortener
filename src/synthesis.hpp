#pragma once
#include "automata.hpp"
#include <random>

namespace rx {

// A deliberately language-agnostic search. Unlike equivalence-preserving
// rewrites, its population may contain incorrect intermediate expressions.
// Tests guide exploration; only a complete DFA product can produce an offer.
struct SemanticSynthesis {
    struct Statistics {
        size_t evaluated = 0, proofs = 0, counterexamples = 0, improvements = 0;
    } stats;
    const DFA &target;
    const Budget &global;
    Limits limits;
    std::function<R(R)> refine;
    std::function<void(R, const std::string &)> offer;
    std::mt19937_64 rng;
    // Optional input-derived alternative forms supplied by the caller. They
    // are treated as search individuals, never as trusted answers.
    std::vector<R> extraSeeds;

    SemanticSynthesis(const DFA &d, const Budget &b, Limits l, std::function<R(R)> f,
                      std::function<void(R, const std::string &)> o, uint64_t seed = 1)
        : target(d), global(b), limits(l), refine(std::move(f)), offer(std::move(o)), rng(seed) {}

  private:
    struct Sample {
        std::string word;
        bool accept;
    };
    struct Entry {
        R r;
        DFA d;
        size_t tested = 0, fp = 0, fn = 0;
    };
    using E = std::shared_ptr<Entry>;
    std::vector<Sample> samples;
    std::unordered_set<std::string> sampleWords;
    size_t positives = 0, negatives = 0;
    std::vector<R> ingredients;
    std::vector<E> population;
    std::unordered_map<std::string, E> cache;
    std::unordered_set<std::string> failed;
    std::vector<int> digits;
    int mask = 0;
    R best;
    bool diverse = false;
    Budget *local = nullptr;

    size_t pick(size_t n) {
        return size_t(rng() % n);
    }
    void check() const {
        global.check();
        local->check();
    }
    void addSample(const std::string &w) {
        if (samples.size() >= 1536 || !sampleWords.insert(w).second)
            return;
        bool accept = matches(target, w);
        samples.push_back({w, accept});
        accept ? ++positives : ++negatives;
    }
    void initializeSamples() {
        int n = int(target.t.size());
        std::vector<std::string> access(n);
        std::vector<bool> reached(n);
        std::vector<int> queue{target.start};
        reached[target.start] = true;
        for (size_t i = 0; i < queue.size(); ++i)
            for (int c = 0; c < 10; ++c) {
                int q = target.t[queue[i]][c];
                if (!reached[q]) {
                    reached[q] = true;
                    access[q] = access[queue[i]] + char('0' + c);
                    queue.push_back(q);
                }
            }
        // Short distinguishing suffixes, obtained from the supplied automaton
        // alone. No expected expression or external answer enters this step.
        std::vector<std::vector<std::string>> suffix(n, std::vector<std::string>(n));
        std::vector<std::vector<bool>> known(n, std::vector<bool>(n));
        for (int a = 0; a < n; ++a)
            for (int b = 0; b < n; ++b)
                known[a][b] = target.accept[a] != target.accept[b];
        for (int round = 0; round < n; ++round) {
            check();
            bool changed = false;
            auto next = known;
            auto text = suffix;
            for (int a = 0; a < n; ++a)
                for (int b = 0; b < a; ++b)
                    if (!known[a][b])
                        for (int c = 0; c < 10; ++c) {
                            int u = target.t[a][c], v = target.t[b][c];
                            if (known[u][v]) {
                                next[a][b] = next[b][a] = true;
                                text[a][b] = text[b][a] = char('0' + c) + suffix[u][v];
                                changed = true;
                                break;
                            }
                        }
            known = std::move(next);
            suffix = std::move(text);
            if (!changed)
                break;
        }
        std::set<std::string> suffixes{std::string()};
        for (int a = 0; a < n; ++a)
            for (int b = 0; b < a; ++b)
                if (known[a][b])
                    suffixes.insert(suffix[a][b]);
        std::vector<std::string> endings(suffixes.begin(), suffixes.end());
        std::sort(endings.begin(), endings.end(), [](const std::string &a, const std::string &b) {
            return a.size() != b.size() ? a.size() < b.size() : a < b;
        });
        if (endings.size() > 32)
            endings.resize(32);
        std::unordered_set<std::string> words;
        for (int q : queue) {
            words.insert(access[q]);
            for (const auto &tail : endings) {
                words.insert(access[q] + tail);
                for (int c : digits)
                    words.insert(access[q] + char('0' + c) + tail);
            }
        }
        std::vector<std::string> frontier{std::string()};
        for (int len = 0; len <= (digits.size() <= 4 ? 4 : 2); ++len) {
            std::vector<std::string> next;
            for (const auto &w : frontier) {
                words.insert(w);
                for (int c : digits)
                    next.push_back(w + char('0' + c));
            }
            frontier = std::move(next);
        }
        for (int trial = 0; trial < 384; ++trial) {
            std::string w;
            for (size_t j = 0, len = pick(size_t(std::min(64, 2 * n + 8))); j < len; ++j)
                w += char('0' + digits[pick(digits.size())]);
            words.insert(w);
        }
        // Balance positive and negative observations so sparse languages do
        // not make the empty language artificially attractive.
        std::vector<std::string> ordered(words.begin(), words.end());
        std::sort(ordered.begin(), ordered.end());
        std::shuffle(ordered.begin(), ordered.end(), rng);
        addSample("");
        for (const auto &w : ordered) {
            bool a = matches(target, w);
            if ((a ? positives : negatives) < 384)
                addSample(w);
        }
    }
    void score(const E &e) {
        for (; e->tested < samples.size(); ++e->tested) {
            const auto &s = samples[e->tested];
            bool actual = matches(e->d, s.word);
            e->fp += actual && !s.accept;
            e->fn += !actual && s.accept;
        }
    }
    double objective(const E &e, double weight) {
        score(e);
        double error = double(e->fp) / std::max(size_t(1), negatives) +
                       double(e->fn) / std::max(size_t(1), positives);
        return weight * error + double(e->r->s.size()) / std::max(size_t(1), best->s.size());
    }
    E evaluate(R r) {
        check();
        size_t maximum = std::min(size_t(256), best->s.size() + 24);
        if (r->s.size() > maximum || failed.count(r->s))
            return {};
        auto old = cache.find(r->s);
        if (old != cache.end()) {
            score(old->second);
            return old->second;
        }
        if (cache.size() >= 50000) {
            // Retain the live search frontier, rather than freezing discovery
            // when a long run fills the memoization budget.
            cache.clear();
            for (const E &live : population)
                cache.emplace(live->r->s, live);
            failed.clear();
        }
        try {
            Limits cap = limits;
            cap.nfa = std::min(cap.nfa, 1200);
            cap.dfa = std::min(cap.dfa, 256);
            DFA d = compileRaw(r, cap, local);
            E e = std::make_shared<Entry>();
            e->r = r;
            e->d = std::move(d);
            cache.emplace(r->s, e);
            ++stats.evaluated;
            score(e);
            if (!e->fp && !e->fn && r->s.size() < best->s.size()) {
                ++stats.proofs;
                auto proof = compare(target, e->d, limits, local);
                if (proof.equal) {
                    R shortened = refine(r);
                    if (better(r, shortened))
                        shortened = r;
                    // Verify printed syntax too: samples and internal AST
                    // agreement never substitute for this exact proof.
                    DFA printed = compileRaw(Parser(shortened->s, false).parse(), limits, local);
                    if (compare(target, printed, limits, local).equal) {
                        best = shortened;
                        ++stats.improvements;
                        offer(shortened, "Gegenbeispielgesteuerte Synthese");
                        // Refinement may expose a much better mutation basis
                        // than the raw successful individual. Keep it alive.
                        if (!eq(shortened, r))
                            if (E normalized = evaluate(shortened))
                                population.push_back(normalized);
                    }
                } else {
                    addSample(proof.witness);
                    ++stats.counterexamples;
                    score(e);
                }
            }
            return e;
        } catch (const Limit &) {
            check();
            failed.insert(r->s);
            return {};
        }
    }
    void collect(R r, std::vector<R> &out, int depth = 0) {
        if (depth > 24 || out.size() >= 512)
            return;
        out.push_back(r);
        for (R child : r->sub)
            collect(child, out, depth + 1);
    }
    std::string profile(R r) const {
        std::array<int, 6> counts{};
        int height = 0, first = 0, last = 0;
        std::function<void(R, int)> visit = [&](R node, int depth) {
            ++counts[node->k];
            height = std::max(height, depth);
            if (node->k == CHAR) {
                if (!first)
                    first = node->mask;
                last = node->mask;
            }
            for (R child : node->sub)
                visit(child, depth + 1);
        };
        visit(r, 0);
        std::string result = std::to_string(r->k) + ":" + std::to_string(height) + ":" +
                             std::to_string(first) + ":" + std::to_string(last);
        for (int count : counts)
            result += ":" + std::to_string(count);
        return result;
    }
    void trim() {
        if (population.size() <= 128)
            return;
        std::vector<E> keep;
        std::unordered_set<std::string> seen;
        auto add = [&](const E &e) {
            if (seen.insert(e->r->s).second)
                keep.push_back(e);
        };
        if (!extraSeeds.empty()) {
            auto order = population;
            std::sort(order.begin(), order.end(),
                      [](const E &a, const E &b) { return better(a->r, b->r); });
            std::set<std::string> shapes;
            for (const E &entry : order) {
                score(entry);
                if (!entry->fp && !entry->fn && entry->r->s.size() <= best->s.size() + 8 &&
                    shapes.insert(profile(entry->r)).second) {
                    add(entry);
                    if (shapes.size() >= 24)
                        break;
                }
            }
        }
        for (double weight : {0.25, 1.0, 4.0, 16.0, 64.0, 256.0}) {
            auto order = population;
            std::sort(order.begin(), order.end(), [&](const E &a, const E &b) {
                double x = objective(a, weight), y = objective(b, weight);
                return x != y ? x < y : better(a->r, b->r);
            });
            for (size_t i = 0; i < std::min(size_t(18), order.size()); ++i)
                add(order[i]);
        }
        if (diverse) {
            // Weighted sums miss unsupported points on a non-convex Pareto
            // frontier. Such individuals often need another repair before
            // becoming correct, so preserve them explicitly in this island.
            auto order = population;
            auto error = [&](const E &e) {
                score(e);
                return double(e->fp) / std::max(size_t(1), negatives) +
                       double(e->fn) / std::max(size_t(1), positives);
            };
            std::sort(order.begin(), order.end(), [&](const E &a, const E &b) {
                if (a->r->s.size() != b->r->s.size())
                    return a->r->s.size() < b->r->s.size();
                double x = error(a), y = error(b);
                return x != y ? x < y : better(a->r, b->r);
            });
            std::vector<E> frontier;
            double previous = std::numeric_limits<double>::infinity();
            for (const E &entry : order)
                if (double current = error(entry); current < previous) {
                    frontier.push_back(entry);
                    previous = current;
                }
            for (size_t i = 0; i < frontier.size(); ++i)
                if (frontier.size() <= 48 || i % (frontier.size() / 48 + 1) == 0)
                    add(frontier[i]);
            if (!frontier.empty())
                add(frontier.back());
        }
        for (int i = 0; i < 24; ++i)
            add(population[pick(population.size())]);
        population = std::move(keep);
    }
    E parent() {
        if (diverse && pick(4) == 0)
            return population[pick(population.size())];
        double weight = std::pow(4.0, int(pick(6)) - 1);
        E result = population[pick(population.size())];
        for (int trial = 0; trial < 4; ++trial) {
            E candidate = population[pick(population.size())];
            if (objective(candidate, weight) < objective(result, weight))
                result = candidate;
        }
        return result;
    }
    R donor() {
        if (population.empty() || pick(3))
            return ingredients[pick(ingredients.size())];
        std::vector<R> nodes;
        collect(population[pick(population.size())]->r, nodes);
        return nodes[pick(nodes.size())];
    }
    R edit(R r, int operation) {
        switch (operation) {
        case 0:
            return donor();
        case 1:
            if (r->k == REP)
                return r->sub[0];
            if ((r->k == CAT || r->k == ALT) && !r->sub.empty()) {
                auto v = r->sub;
                v.erase(v.begin() + pick(v.size()));
                return rebuild(r, v);
            }
            return eps();
        case 2: {
            R d = donor();
            return pick(2) ? cat({r, d}) : cat({d, r});
        }
        case 3:
            return alt({r, donor()});
        case 4: {
            static const std::pair<I, I> bounds[] = {{0, INF}, {1, INF}, {0, 1}, {2, 2},
                                                     {3, 3},   {0, 2},   {1, 2}, {2, INF}};
            auto bound = bounds[pick(sizeof(bounds) / sizeof(bounds[0]))];
            return rep(r, bound.first, bound.second);
        }
        case 5:
            if (r->k == REP) {
                I lo = r->lo, hi = r->hi;
                if (pick(2))
                    lo = std::max(I(0), std::min(I(16), lo + (pick(2) ? 1 : -1)));
                else if (hi == INF)
                    hi = std::max(lo, I(1 + pick(6)));
                else
                    hi = std::max(lo, std::min(I(16), hi + (pick(2) ? 1 : -1)));
                if (hi != INF && lo > hi)
                    hi = lo;
                return rep(r->sub[0], lo, hi);
            }
            return rep(r, I(pick(4)), INF);
        case 6:
            if (r->k == CHAR)
                return chars(r->mask ^ (1 << digits[pick(digits.size())]));
            return chars(1 << digits[pick(digits.size())]);
        case 7:
            if (r->sub.size() >= 2) {
                auto v = r->sub;
                std::swap(v[pick(v.size())], v[pick(v.size())]);
                return rebuild(r, v);
            }
            return r;
        case 8:
            return cat({r, r});
        case 9: {
            R d = donor();
            return pick(2) ? rep(cat({r, d}), 0, INF) : rep(alt({r, d}), 0, INF);
        }
        case 10:
            if (r->k == CAT)
                return alt(r->sub);
            if (r->k == ALT)
                return cat(r->sub);
            return r;
        default:
            if (r->k == CAT && r->sub.size() >= 2) {
                auto v = r->sub;
                size_t a = pick(v.size()), b = pick(v.size());
                if (a > b)
                    std::swap(a, b);
                std::vector<R> middle(v.begin() + a, v.begin() + b + 1);
                v.erase(v.begin() + a, v.begin() + b + 1);
                v.insert(v.begin() + a, rep(cat(middle), 0, pick(2) ? INF : I(1)));
                return cat(v);
            }
            return r;
        }
    }
    R mutate(R r) {
        std::vector<R> nodes;
        collect(r, nodes);
        size_t index = pick(nodes.size());
        R replacement = edit(nodes[index], int(pick(12)));
        size_t current = 0;
        std::function<R(R, int)> substitute = [&](R node, int depth) -> R {
            if (depth > 24)
                return node;
            if (current++ == index)
                return replacement;
            std::vector<R> children;
            for (R child : node->sub)
                children.push_back(substitute(child, depth + 1));
            return rebuild(node, children);
        };
        R out = substitute(r, 0);
        if (pick(12) == 0)
            out = refine(out);
        return out;
    }

  public:
    std::vector<R> equivalentSeeds(size_t maximum = 24) {
        std::vector<R> result;
        if (global.done() || !maximum)
            return result;
        Budget slice(0.25);
        slice.end = std::min(slice.end, global.end);
        std::vector<E> candidates;
        for (const auto &entry : cache) {
            if (slice.done())
                break;
            const E &candidate = entry.second;
            if (candidate->r->k == EMPTY || candidate->r->s.size() > best->s.size() + 8)
                continue;
            score(candidate);
            if (!candidate->fp && !candidate->fn)
                candidates.push_back(candidate);
        }
        std::sort(candidates.begin(), candidates.end(),
                  [](const E &a, const E &b) { return better(a->r, b->r); });
        // Distinct structural profiles get their first chance before several
        // cosmetic variants of one basin consume the archive.
        std::set<std::string> shapes;
        std::vector<E> ordered, repeated;
        for (const E &candidate : candidates) {
            if (slice.done())
                break;
            (shapes.insert(profile(candidate->r)).second ? ordered : repeated).push_back(candidate);
        }
        ordered.insert(ordered.end(), repeated.begin(), repeated.end());
        for (size_t i = 0; i < ordered.size() && i < maximum * 16 && result.size() < maximum; ++i) {
            if (slice.done())
                break;
            try {
                R r = ordered[i]->r;
                DFA printed = compileRaw(Parser(r->s, false).parse(), limits, &slice);
                if (compare(target, printed, limits, &slice).equal)
                    result.push_back(r);
            } catch (const Limit &) {
                if (slice.done())
                    break;
            }
        }
        return result;
    }
    std::vector<std::pair<std::string, R>> learnedEntries(size_t maximum = 2000) {
        std::vector<E> candidates;
        for (const auto &entry : cache)
            if (entry.second->r->k != EMPTY && entry.second->r->s.size() <= 48 &&
                entry.second->d.t.size() <= 48)
                candidates.push_back(entry.second);
        std::sort(candidates.begin(), candidates.end(),
                  [](const E &a, const E &b) { return better(a->r, b->r); });
        std::map<std::string, R> languages;
        size_t visited = 0;
        for (const E &entry : candidates) {
            if (global.done() || visited++ >= maximum * 4)
                break;
            try {
                std::string signature = key(minimize(entry->d, &global));
                auto old = languages.find(signature);
                if (old != languages.end()) {
                    if (better(entry->r, old->second))
                        old->second = entry->r;
                } else if (languages.size() < maximum)
                    languages.emplace(std::move(signature), entry->r);
            } catch (const Limit &) {
                if (global.done())
                    break;
            }
        }
        return {languages.begin(), languages.end()};
    }
    R run(R seed, double seconds = 1.0, size_t maxEvaluations = 30000, bool diversify = false) {
        stats = {};
        samples.clear();
        sampleWords.clear();
        positives = negatives = 0;
        ingredients.clear();
        population.clear();
        cache.clear();
        failed.clear();
        digits.clear();
        mask = 0;
        diverse = diversify;
        local = nullptr;
        best = seed;
        if (target.t.size() > 64 || seconds <= 0 || global.done())
            return best;
        Budget slice(seconds);
        slice.end = std::min(slice.end, global.end);
        local = &slice;
        try {
            // Ignore alphabet symbols absent from the input language. They
            // cannot improve a correct regex without complement operations.
            mask = alphabet(seed);
            for (int c = 0; c < 10; ++c)
                if (mask & (1 << c))
                    digits.push_back(c);
            if (digits.empty()) {
                local = nullptr;
                return best;
            }
            initializeSamples();
            std::vector<R> seeds;
            collect(seed, seeds);
            for (size_t i = 0; i < extraSeeds.size() && i < 24; ++i)
                collect(extraSeeds[i], seeds);
            seeds.push_back(eps());
            seeds.push_back(zero());
            std::map<std::vector<int>, int> classes;
            for (int c : digits) {
                std::vector<int> signature;
                for (const auto &row : target.t)
                    signature.push_back(row[c]);
                classes[signature] |= 1 << c;
            }
            std::vector<int> combinations{0};
            for (const auto &entry : classes) {
                size_t count = combinations.size();
                for (size_t i = 0; i < count && combinations.size() < 128; ++i)
                    combinations.push_back(combinations[i] | entry.second);
            }
            for (int m : combinations)
                if (m)
                    seeds.push_back(chars(m));
            for (int c : digits)
                seeds.push_back(chars(1 << c));
            std::unordered_set<std::string> seen;
            for (R r : seeds)
                if (r->s.size() <= 64 && seen.insert(r->s).second)
                    ingredients.push_back(r);
            std::sort(ingredients.begin(), ingredients.end(), better);
            auto insert = [&](R r) {
                if (E e = evaluate(r))
                    population.push_back(e);
            };
            insert(seed);
            for (size_t i = 0; i < extraSeeds.size() && i < 24; ++i)
                insert(extraSeeds[i]);
            for (R r : ingredients) {
                insert(r);
                if (r->s.size() <= 12) {
                    insert(rep(r, 0, INF));
                    insert(rep(r, 0, 1));
                    insert(rep(r, 1, INF));
                }
            }
            trim();
            for (size_t iteration = 0; iteration < maxEvaluations * 4 &&
                                       stats.evaluated < maxEvaluations && !population.empty();
                 ++iteration) {
                check();
                R candidate = mutate(parent()->r);
                if (iteration % 8 == 7)
                    candidate = mutate(candidate);
                if (E e = evaluate(candidate))
                    population.push_back(e);
                if (population.size() > 192 || iteration % 64 == 63)
                    trim();
            }
        } catch (const Limit &) {
            // The caller retains the best fully proved expression obtained
            // before this phase's own time or resource limit was reached.
        }
        local = nullptr;
        return best;
    }
};
} // namespace rx
