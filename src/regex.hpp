#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rx {
using I = int64_t;
constexpr I INF = -1, MAX_COUNT = 1000000000;
enum Kind { EMPTY, EPS, CHAR, ALT, CAT, REP };
struct Node;
using R = std::shared_ptr<const Node>;
struct Node {
    Kind k;
    int mask = 0;
    I lo = 0, hi = 0;
    std::vector<R> sub;
    std::string s;
    int prec = 4;
};
inline bool better(const R &a, const R &b) {
    return a->s.size() != b->s.size() ? a->s.size() < b->s.size() : a->s < b->s;
}
inline bool eq(const R &a, const R &b) {
    return a->s == b->s;
}
inline R zero() {
    static R r = std::make_shared<Node>(Node{EMPTY, 0, 0, 0, {}, "#", 4});
    return r;
}
inline R eps() {
    static R r = std::make_shared<Node>(Node{EPS, 0, 0, 0, {}, "", 4});
    return r;
}
inline std::string wrap(R r, int p) {
    if (r->k == CHAR && p <= 1 && __builtin_popcount(unsigned(r->mask)) == 2) {
        std::string t;
        for (int c = 0; c < 10; ++c)
            if (r->mask & (1 << c)) {
                if (!t.empty())
                    t += '|';
                t += char('0' + c);
            }
        return t;
    }
    return r->prec < p ? "(" + r->s + ")" : r->s;
}
inline std::string charset(int mask) {
    std::string t;
    for (int i = 0; i < 10;) {
        if (!(mask & (1 << i))) {
            ++i;
            continue;
        }
        int j = i;
        while (j < 9 && (mask & (1 << (j + 1))))
            ++j;
        if (j - i >= 3)
            t += std::string(1, char('0' + i)) + "-" + char('0' + j);
        else
            for (int x = i; x <= j; ++x)
                t += char('0' + x);
        i = j + 1;
    }
    return __builtin_popcount(unsigned(mask)) == 1 ? t : "[" + t + "]";
}
inline R chars(int mask) {
    if (!mask)
        return zero();
    return std::make_shared<Node>(Node{CHAR, mask, 0, 0, {}, charset(mask), 4});
}
inline bool nullable(R r) {
    switch (r->k) {
    case EMPTY:
    case CHAR:
        return false;
    case EPS:
        return true;
    case REP:
        return r->lo == 0 || nullable(r->sub[0]);
    case ALT:
        for (auto x : r->sub)
            if (nullable(x))
                return true;
        return false;
    case CAT:
        for (auto x : r->sub)
            if (!nullable(x))
                return false;
        return true;
    }
    return false;
}
inline R cat(std::vector<R> v);
inline R alt(std::vector<R> v);
inline R rep(R r, I lo, I hi);
inline R rawRep(R r, I lo, I hi) {
    if (r->k == EPS)
        return eps();
    std::string suffix;
    if (lo == 0 && hi == INF)
        suffix = "*";
    else if (lo == 1 && hi == INF)
        suffix = "+";
    else if (lo == 0 && hi == 1)
        suffix = "?";
    else if (lo == hi)
        suffix = "{" + std::to_string(lo) + "}";
    else
        suffix = "{" + std::to_string(lo) + "," + (hi == INF ? "" : std::to_string(hi)) + "}";
    std::string s = wrap(r, 4) + suffix;
    // A nullable operand supplies its own padding: R{0,n} = R{n}.
    // Keep the interval in the AST; only shorten the printed spelling.
    if (hi > 1 && nullable(r)) {
        std::string padded = wrap(r, 4) + "{" + std::to_string(hi) + "}";
        if (padded.size() < s.size())
            s = padded;
    }
    if (lo == 0 && hi > 1) {
        // Padding with epsilon can save braces and an extra group around an
        // optional operand: (|X){n} denotes X{0,n}.
        std::string padded = "(|" + wrap(r, 1) + "){" + std::to_string(hi) + "}";
        if (padded.size() < s.size())
            s = padded;
    }
    return std::make_shared<Node>(Node{REP, 0, lo, hi, {r}, s, 3});
}
inline R rep(R r, I lo, I hi) {
    if (hi == 0 || r->k == EPS)
        return eps();
    if (r->k == EMPTY)
        return lo == 0 ? eps() : zero();
    if (lo == 1 && hi == 1)
        return r;
    // A nullable language can pad any smaller number of factors with epsilon.
    if (nullable(r))
        lo = 0;
    if (r->k == REP) {
        I a = r->lo, b = r->hi;
        bool contiguous = lo == hi || b == INF || (b - a) * lo + 1 >= a;
        if (b == INF && lo == 0 && a > 1)
            contiguous = false;
        I newlo = a * lo, newhi = (hi == INF || b == INF) ? INF : b * hi;
        if (contiguous && newlo <= MAX_COUNT && (newhi == INF || newhi <= MAX_COUNT))
            return rep(r->sub[0], newlo, newhi);
    }
    R best = rawRep(r, lo, hi);
    // Braces can be more expensive than spelling a few copies explicitly.
    if (hi != INF && hi <= 6) {
        std::string s;
        std::vector<R> parts;
        for (I i = 0; i < hi; ++i) {
            R x = i < lo ? r : rawRep(r, 0, 1);
            s += wrap(x, 2);
            parts.push_back(x);
        }
        if (!parts.empty() && s.size() < best->s.size())
            best = std::make_shared<Node>(Node{CAT, 0, 0, 0, parts, s, 2});
    }
    if (hi == INF && lo >= 2 && lo <= 6) {
        std::vector<R> parts(size_t(lo - 1), r);
        parts.push_back(rawRep(r, 1, INF));
        std::string s;
        for (auto x : parts)
            s += wrap(x, 2);
        if (s.size() < best->s.size())
            best = std::make_shared<Node>(Node{CAT, 0, 0, 0, parts, s, 2});
    }
    return best;
}
inline R cat(std::vector<R> v) {
    std::vector<R> w;
    for (auto x : v) {
        if (x->k == EMPTY)
            return zero();
        if (x->k == EPS)
            continue;
        if (x->k == CAT)
            w.insert(w.end(), x->sub.begin(), x->sub.end());
        else
            w.push_back(x);
    }
    if (w.empty())
        return eps();
    if (w.size() == 1)
        return w[0];
    // Merge runs of equal factors, including adjacent intervals of repetition.
    std::vector<R> out;
    for (size_t i = 0; i < w.size();) {
        R base = w[i]->k == REP ? w[i]->sub[0] : w[i];
        I lo = 0, hi = 0;
        size_t j = i;
        while (j < w.size()) {
            R b = w[j]->k == REP ? w[j]->sub[0] : w[j];
            if (!eq(base, b))
                break;
            I a = w[j]->k == REP ? w[j]->lo : 1, c = w[j]->k == REP ? w[j]->hi : 1;
            if (lo + a > MAX_COUNT || (hi != INF && c != INF && hi + c > MAX_COUNT))
                break;
            lo += a;
            hi = (hi == INF || c == INF) ? INF : hi + c;
            ++j;
        }
        if (j == i) {
            out.push_back(w[i++]);
            continue;
        }
        R merged = rep(base, lo, hi);
        size_t old = 0;
        for (size_t k = i; k < j; ++k)
            old += wrap(w[k], 2).size();
        if (wrap(merged, 2).size() <= old) {
            if (merged->k == CAT)
                out.insert(out.end(), merged->sub.begin(), merged->sub.end());
            else
                out.push_back(merged);
        } else
            for (size_t k = i; k < j; ++k)
                out.push_back(w[k]);
        i = j;
    }
    if (out.size() == 1)
        return out[0];
    std::string s;
    for (auto x : out)
        s += wrap(x, 2);
    return std::make_shared<Node>(Node{CAT, 0, 0, 0, out, s, 2});
}
inline R alt(std::vector<R> v) {
    std::vector<R> w;
    int mask = 0;
    bool hasEps = false;
    for (auto x : v) {
        if (x->k == ALT)
            w.insert(w.end(), x->sub.begin(), x->sub.end());
        else
            w.push_back(x);
    }
    std::vector<R> u;
    for (auto x : w) {
        if (x->k == EMPTY)
            continue;
        if (x->k == EPS) {
            hasEps = true;
            continue;
        }
        if (x->k == CHAR)
            mask |= x->mask;
        else
            u.push_back(x);
    }
    if (mask)
        u.push_back(chars(mask));
    std::sort(u.begin(), u.end(), [](R a, R b) { return a->s < b->s; });
    u.erase(std::unique(u.begin(), u.end(), eq), u.end());
    if (u.empty())
        return hasEps ? eps() : zero();
    R r;
    if (u.size() == 1)
        r = u[0];
    else {
        std::string s;
        for (auto x : u) {
            if (!s.empty())
                s += '|';
            s += wrap(x, 1);
        }
        r = std::make_shared<Node>(Node{ALT, 0, 0, 0, u, s, 1});
    }
    if (hasEps && !nullable(r)) {
        R optional = rep(r, 0, 1);
        std::string text = "|" + wrap(r, 1);
        if (text.size() < optional->s.size()) {
            u.insert(u.begin(), eps());
            return std::make_shared<Node>(Node{ALT, 0, 0, 0, u, text, 1});
        }
        return optional;
    }
    return r;
}
struct ParseError : std::runtime_error {
    using std::runtime_error::runtime_error;
};
struct Parser {
    std::string s;
    size_t pos = 0;
    int depth = 0;
    bool normalize;
    explicit Parser(std::string text, bool norm = true) : s(std::move(text)), normalize(norm) {}
    [[noreturn]] void fail(std::string m) {
        throw ParseError(m + " (Position " + std::to_string(pos + 1) + ")");
    }
    char peek() const {
        return pos < s.size() ? s[pos] : '\0';
    }
    I number(std::string *digits = nullptr) {
        if (peek() < '0' || peek() > '9')
            fail("Zahl erwartet");
        I n = 0;
        size_t start = pos;
        while (peek() >= '0' && peek() <= '9') {
            n = std::min(MAX_COUNT + 1, n * 10 + s[pos++] - '0');
        }
        if (digits) {
            *digits = s.substr(start, pos - start);
            size_t first = digits->find_first_not_of('0');
            *digits = first == std::string::npos ? "0" : digits->substr(first);
        }
        return n;
    }
    R atom() {
        if (++depth > 256)
            fail("Zu tiefe Verschachtelung");
        R r;
        char c = peek();
        if (c >= '0' && c <= '9') {
            ++pos;
            r = chars(1 << (c - '0'));
        } else if (c == '(') {
            ++pos;
            r = expression();
            if (peek() != ')')
                fail("Schliessende Klammer erwartet");
            ++pos;
        } else if (c == '[') {
            ++pos;
            int mask = 0;
            if (peek() == ']')
                fail("Leere Zeichengruppe");
            while (peek() != ']') {
                char a = peek();
                if (a < '0' || a > '9')
                    fail("Ziffer in Zeichengruppe erwartet");
                ++pos;
                char b = a;
                if (peek() == '-') {
                    ++pos;
                    b = peek();
                    if (b < a || b > '9')
                        fail("Ungueltiger Zeichenbereich");
                    ++pos;
                }
                for (char x = a; x <= b; ++x)
                    mask |= 1 << (x - '0');
            }
            ++pos;
            r = chars(mask);
        } else
            fail("Ziffer, Gruppe oder Zeichengruppe erwartet");
        --depth;
        return r;
    }
    R piece() {
        R r = atom();
        char c = peek();
        I a, b;
        size_t quantStart = pos;
        if (c == '*' || c == '+' || c == '?' || c == '{') {
            ++pos;
            if (c == '*') {
                a = 0;
                b = INF;
            } else if (c == '+') {
                a = 1;
                b = INF;
            } else if (c == '?') {
                a = 0;
                b = 1;
            } else {
                std::string lower, upper;
                a = number(&lower);
                b = a;
                upper = lower;
                if (peek() == ',') {
                    ++pos;
                    b = peek() == '}' ? INF : number(&upper);
                }
                if (peek() != '}')
                    fail("Schliessende geschweifte Klammer erwartet");
                ++pos;
                if (b != INF && (lower.size() > upper.size() ||
                                 (lower.size() == upper.size() && lower > upper)))
                    fail("Obere Grenze kleiner als untere Grenze");
            }
            if (a > MAX_COUNT || b > MAX_COUNT) {
                // Keep arbitrary-size decimal counts losslessly. Their numeric
                // sentinel exceeds all supported NFA limits, so compilation
                // fails closed instead of approximating the count.
                std::string text = wrap(r, 4) + s.substr(quantStart, pos - quantStart);
                r = std::make_shared<Node>(Node{REP, 0, a, b, {r}, text, 3});
            } else
                r = normalize ? rep(r, a, b) : rawRep(r, a, b);
            if (peek() == '*' || peek() == '+' || peek() == '?' || peek() == '{')
                fail("Aufeinanderfolgende Quantoren brauchen Klammern");
        }
        return r;
    }
    R sequence() {
        std::vector<R> v;
        while (peek() && peek() != ')' && peek() != '|')
            v.push_back(piece());
        if (normalize)
            return cat(v);
        if (v.empty())
            return eps();
        if (v.size() == 1)
            return v[0];
        std::string t;
        for (auto x : v)
            t += wrap(x, 2);
        return std::make_shared<Node>(Node{CAT, 0, 0, 0, v, t, 2});
    }
    R expression() {
        std::vector<R> v{sequence()};
        while (peek() == '|') {
            ++pos;
            v.push_back(sequence());
        }
        if (normalize)
            return alt(v);
        if (v.size() == 1)
            return v[0];
        std::string t;
        for (size_t i = 0; i < v.size(); ++i) {
            if (i)
                t += '|';
            t += v[i]->s;
        }
        return std::make_shared<Node>(Node{ALT, 0, 0, 0, v, t, 1});
    }
    R parse() {
        if (s.size() > 10000)
            fail("Eingabe zu lang (max. 10000 Zeichen)");
        R r = expression();
        if (pos != s.size())
            fail("Unerwartetes Zeichen");
        return r;
    }
};
inline R rebuild(R r, std::vector<R> v) {
    switch (r->k) {
    case ALT:
        return alt(v);
    case CAT:
        return cat(v);
    case REP:
        return rep(v[0], r->lo, r->hi);
    default:
        return r;
    }
}
inline R normalize(R r) {
    std::vector<R> v;
    for (auto x : r->sub)
        v.push_back(normalize(x));
    return rebuild(r, v);
}
inline int alphabet(R r) {
    int m = r->mask;
    for (auto x : r->sub)
        m |= alphabet(x);
    return m;
}
struct Limit : std::runtime_error {
    using std::runtime_error::runtime_error;
};
extern std::atomic<bool> interrupted;
static_assert(std::atomic<bool>::is_always_lock_free, "Signal handling needs lock-free atomics");
struct Budget {
    using Clock = std::chrono::steady_clock;
    Clock::time_point end;
    bool timed = true, honorStop = true;
    explicit Budget(double sec)
        : end(Clock::now() +
              std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(sec))) {}
    bool done() const {
        return (honorStop && interrupted.load(std::memory_order_relaxed)) ||
               (timed && Clock::now() >= end);
    }
    void check() const {
        if (done())
            throw Limit("Zeitbudget erreicht");
    }
};
} // namespace rx
