#pragma once
#include "regex.hpp"
namespace rx {
struct Limits {
    int nfa = 12000, dfa = 4096;
    size_t products = 2000000;
};
struct Edge {
    int to, mask;
};
struct NFA {
    std::vector<std::vector<Edge>> e;
    int start = 0, finish = 0;
    const Budget *budget;
    int cap;
    NFA(const Budget *b, int c) : budget(b), cap(c) {}
    int state() {
        if (int(e.size()) >= cap)
            throw Limit("NFA-Zustandslimit erreicht");
        e.emplace_back();
        return int(e.size()) - 1;
    }
    void edge(int a, int b, int m = 0) {
        e[a].push_back({b, m});
    }
    std::pair<int, int> build(R r) {
        if (budget)
            budget->check();
        int a = state(), b = state();
        switch (r->k) {
        case EMPTY:
            break;
        case EPS:
            edge(a, b);
            break;
        case CHAR:
            edge(a, b, r->mask);
            break;
        case CAT: {
            int p = a;
            for (auto x : r->sub) {
                auto [u, v] = build(x);
                edge(p, u);
                p = v;
            }
            edge(p, b);
            break;
        }
        case ALT:
            for (auto x : r->sub) {
                auto [u, v] = build(x);
                edge(a, u);
                edge(v, b);
            }
            break;
        case REP: {
            if (r->lo > cap || (r->hi != INF && r->hi > cap))
                throw Limit("Wiederholungszahl ueberschreitet das NFA-Limit");
            int p = a;
            for (I i = 0; i < r->lo; ++i) {
                auto [u, v] = build(r->sub[0]);
                edge(p, u);
                p = v;
            }
            if (r->hi == INF) {
                auto [u, v] = build(r->sub[0]);
                edge(p, b);
                edge(p, u);
                edge(v, p);
            } else {
                for (I i = r->lo; i < r->hi; ++i) {
                    auto [u, v] = build(r->sub[0]);
                    edge(p, b);
                    edge(p, u);
                    p = v;
                }
                edge(p, b);
            }
            break;
        }
        }
        return {a, b};
    }
};
struct DFA {
    std::vector<std::array<int, 10>> t;
    std::vector<uint8_t> accept;
    int start = 0;
};
using Bits = std::vector<uint64_t>;
struct BitsHash {
    size_t operator()(const Bits &v) const {
        size_t h = 0xcbf29ce484222325ULL;
        for (auto x : v) {
            h ^= x;
            h *= 0x100000001b3ULL;
            h ^= h >> 32;
        }
        return h;
    }
};
template <class F> inline void bitsEach(const Bits &b, F f) {
    for (size_t i = 0; i < b.size(); ++i) {
        uint64_t x = b[i];
        while (x) {
            int k = __builtin_ctzll(x);
            x &= x - 1;
            f(int(i * 64 + k));
        }
    }
}
inline DFA determinize(const NFA &n, const Limits &lim, const Budget *budget) {
    int sz = int(n.e.size()), words = (sz + 63) / 64;
    std::vector<Bits> closures(sz);
    std::vector<int> stack;
    auto closure = [&](int q) -> const Bits & {
        Bits &b = closures[q];
        if (!b.empty())
            return b;
        b.assign(words, 0);
        stack.clear();
        stack.push_back(q);
        b[q / 64] |= 1ULL << (q % 64);
        while (!stack.empty()) {
            int p = stack.back();
            stack.pop_back();
            for (auto e : n.e[p])
                if (!e.mask && !(b[e.to / 64] & (1ULL << (e.to % 64)))) {
                    b[e.to / 64] |= 1ULL << (e.to % 64);
                    stack.push_back(e.to);
                }
        }
        return b;
    };
    std::unordered_map<Bits, int, BitsHash> ids;
    std::vector<Bits> states;
    DFA d;
    auto add = [&](const Bits &b) {
        auto it = ids.find(b);
        if (it != ids.end())
            return it->second;
        if (int(states.size()) >= lim.dfa)
            throw Limit("DFA-Zustandslimit erreicht");
        int i = int(states.size());
        ids.emplace(b, i);
        states.push_back(b);
        d.t.push_back({});
        d.accept.push_back(bool(b[n.finish / 64] & (1ULL << (n.finish % 64))));
        return i;
    };
    add(closure(n.start));
    for (size_t i = 0; i < states.size(); ++i) {
        if (budget)
            budget->check();
        std::array<Bits, 10> dest;
        for (auto &b : dest)
            b.assign(words, 0);
        bitsEach(states[i], [&](int p) {
            for (auto e : n.e[p])
                if (e.mask) {
                    const Bits &b = closure(e.to);
                    for (int c = 0; c < 10; ++c)
                        if (e.mask & (1 << c))
                            for (int k = 0; k < words; ++k)
                                dest[c][k] |= b[k];
                }
        });
        for (int c = 0; c < 10; ++c) {
            int to = add(dest[c]);
            d.t[i][c] = to;
        }
    }
    return d;
}
inline DFA canonical(const DFA &d) {
    DFA out;
    std::vector<int> id(d.t.size(), -1), todo{d.start};
    id[d.start] = 0;
    for (size_t i = 0; i < todo.size(); ++i) {
        int p = todo[i];
        std::array<int, 10> row{};
        for (int c = 0; c < 10; ++c) {
            int q = d.t[p][c];
            if (id[q] < 0) {
                id[q] = int(todo.size());
                todo.push_back(q);
            }
            row[c] = id[q];
        }
        out.t.push_back(row);
        out.accept.push_back(d.accept[p]);
    }
    return out;
}
inline DFA minimize(const DFA &in, const Budget *budget = nullptr) {
    DFA d = canonical(in);
    int n = int(d.t.size());
    std::vector<std::vector<int>> blocks(2);
    std::vector<int> cls(n);
    for (int q = 0; q < n; ++q) {
        cls[q] = d.accept[q] ? 1 : 0;
        blocks[cls[q]].push_back(q);
    }
    if (blocks[0].empty() || blocks[1].empty()) {
        DFA a;
        a.t.push_back({});
        a.accept.push_back(d.accept[0]);
        return a;
    }
    std::array<std::vector<std::vector<int>>, 10> pred;
    for (int c = 0; c < 10; ++c) {
        pred[c].resize(n);
        for (int q = 0; q < n; ++q)
            pred[c][d.t[q][c]].push_back(q);
    }
    std::vector<std::array<bool, 10>> pending(2);
    std::queue<std::pair<int, int>> work;
    auto push = [&](int b, int c) {
        if (!pending[b][c]) {
            pending[b][c] = true;
            work.push({b, c});
        }
    };
    int small = blocks[0].size() < blocks[1].size() ? 0 : 1;
    for (int c = 0; c < 10; ++c)
        push(small, c);
    std::vector<int> mark(n, 0), count(2, 0);
    int stamp = 0;
    while (!work.empty()) {
        if (budget)
            budget->check();
        auto [a, c] = work.front();
        work.pop();
        pending[a][c] = false;
        ++stamp;
        std::vector<int> touched;
        for (int q : blocks[a])
            for (int p : pred[c][q]) {
                mark[p] = stamp;
                int b = cls[p];
                if (count[b]++ == 0)
                    touched.push_back(b);
            }
        for (int b : touched) {
            int k = count[b];
            count[b] = 0;
            if (k == int(blocks[b].size()))
                continue;
            int nb = int(blocks.size());
            std::vector<int> yes, no;
            yes.reserve(k);
            no.reserve(blocks[b].size() - k);
            for (int q : blocks[b])
                (mark[q] == stamp ? yes : no).push_back(q);
            blocks[b] = std::move(no);
            blocks.push_back(std::move(yes));
            pending.push_back({});
            count.push_back(0);
            for (int q : blocks[nb])
                cls[q] = nb;
            for (int x = 0; x < 10; ++x) {
                if (pending[b][x])
                    push(nb, x);
                else
                    push(blocks[b].size() < blocks[nb].size() ? b : nb, x);
            }
        }
    }
    DFA out;
    out.t.resize(blocks.size());
    out.accept.resize(blocks.size());
    out.start = cls[d.start];
    for (size_t b = 0; b < blocks.size(); ++b) {
        int q = blocks[b][0];
        out.accept[b] = d.accept[q];
        for (int c = 0; c < 10; ++c)
            out.t[b][c] = cls[d.t[q][c]];
    }
    return canonical(out);
}
inline DFA compileRaw(R r, const Limits &lim = {}, const Budget *budget = nullptr) {
    NFA n(budget, lim.nfa);
    auto [a, b] = n.build(r);
    n.start = a;
    n.finish = b;
    return determinize(n, lim, budget);
}
inline DFA compile(R r, const Limits &lim = {}, const Budget *budget = nullptr) {
    return minimize(compileRaw(r, lim, budget), budget);
}

inline std::string key(const DFA &d) {
    std::string s;
    s.reserve(d.t.size() * 41);
    for (size_t i = 0; i < d.t.size(); ++i) {
        s += char(d.accept[i]);
        for (int q : d.t[i])
            for (int k = 0; k < 4; ++k)
                s += char((unsigned(q) >> (k * 8)) & 255);
    }
    return s;
}
struct Comparison {
    bool equal = true;
    std::string witness;
};
inline Comparison compare(const DFA &a, const DFA &b, const Limits &lim = {},
                          const Budget *budget = nullptr, bool inclusion = false) {
    struct Item {
        int a, b, parent;
        char ch;
    };
    std::vector<Item> todo;
    std::unordered_set<uint64_t> seen;
    auto add = [&](int p, int q, int parent, char ch) {
        uint64_t k = (uint64_t(unsigned(p)) << 32) | unsigned(q);
        if (seen.insert(k).second) {
            if (todo.size() >= lim.products)
                throw Limit("Produkt-Zustandslimit erreicht");
            todo.push_back({p, q, parent, ch});
        }
    };
    add(a.start, b.start, -1, 0);
    for (size_t i = 0; i < todo.size(); ++i) {
        if (budget && (i & 255) == 0)
            budget->check();
        auto x = todo[i];
        if (inclusion ? (a.accept[x.a] && !b.accept[x.b]) : (a.accept[x.a] != b.accept[x.b])) {
            std::string w;
            int j = int(i);
            while (todo[j].parent >= 0) {
                w += todo[j].ch;
                j = todo[j].parent;
            }
            std::reverse(w.begin(), w.end());
            return {false, w};
        }
        for (int c = 0; c < 10; ++c)
            add(a.t[x.a][c], b.t[x.b][c], int(i), char('0' + c));
    }
    return {};
}
inline bool matches(const DFA &d, const std::string &s) {
    int p = d.start;
    for (char c : s) {
        if (c < '0' || c > '9')
            return false;
        p = d.t[p][c - '0'];
    }
    return d.accept[p];
}
inline DFA reverseDFA(const DFA &d, const Limits &lim, const Budget *budget) {
    NFA n(budget, lim.nfa);
    for (size_t i = 0; i < d.t.size() + 2; ++i)
        n.state();
    n.start = int(d.t.size());
    n.finish = n.start + 1;
    n.edge(d.start, n.finish);
    for (size_t q = 0; q < d.t.size(); ++q) {
        if (d.accept[q])
            n.edge(n.start, int(q));
        for (int c = 0; c < 10; ++c)
            n.edge(d.t[q][c], int(q), 1 << c);
    }
    return minimize(determinize(n, lim, budget), budget);
}
inline R reverseRegex(R r) {
    std::vector<R> v;
    for (auto x : r->sub)
        v.push_back(reverseRegex(x));
    if (r->k == CAT)
        std::reverse(v.begin(), v.end());
    return rebuild(r, v);
}
} // namespace rx
