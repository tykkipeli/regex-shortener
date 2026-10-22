// Reproducible context-superoptimizer checks, independent of record answers.
#include "../src/algebra_search.hpp"
#include "../src/optimize.hpp"
#include <cassert>
namespace rx {
std::atomic<bool> interrupted{false};
}
using namespace rx;
static std::mt19937_64 randomizer(82947927);
static int offered = 0;
static std::string generate(int depth, int mask) {
    if (depth <= 0 || randomizer() % 4 == 0) {
        if (randomizer() % 8 == 0)
            return "";
        int subset = 0;
        for (int c = 0; c < 10; ++c)
            if (mask & (1 << c))
                if (randomizer() % 2)
                    subset |= 1 << c;
        if (!subset)
            subset = mask & -mask;
        return charset(subset);
    }
    int operation = randomizer() % 5;
    auto a = generate(depth - 1, mask);
    if (operation == 0)
        return "(" + a + ")*";
    if (operation == 1)
        return "(" + a + ")?";
    if (operation == 2)
        return "(" + a + "){1,3}";
    auto b = generate(depth - 1, mask);
    return operation == 3 ? "(" + a + "|" + b + ")" : "(" + a + ")(" + b + ")";
}
static int longCases = 0;
static void test(const std::string &expr, Limits limits = {}, double seconds = .04) {
    R input = Parser(expr, false).parse();
    Budget budget(20);
    auto source = compileRaw(input, {}, &budget);
    auto target = minimize(source, &budget);
    AlgebraSearch search(
        target, budget, limits,
        [&](R candidate, const std::string &) {
            R reparsed = Parser(candidate->s, false).parse();
            auto d = compileRaw(reparsed, {}, &budget);
            auto proof = compare(source, d, {}, &budget);
            if (!proof.equal) {
                std::cerr << "WRONG OFFER " << expr << " -> " << candidate->s << " witness "
                          << proof.witness << "\n";
                std::abort();
            }
            ++offered;
        },
        [](R r) { return structural(r); }, randomizer());
    R result = search.run(input, seconds, 400);
    if (input->s.size() >= 60 && limits.nfa > 2) {
        assert(!search.phrasePowers.empty());
        ++longCases;
    }
    assert(result->s.size() <= input->s.size());
    assert(compare(source, compileRaw(Parser(result->s, false).parse(), {}, &budget), {}, &budget)
               .equal);
}
int main() {
    int completed = 0;
    for (int mask : {3, 192, 276, 1023, 1})
        for (int trial = 0; trial < 12; ++trial) {
            test(generate(3, mask));
            ++completed;
        }
    for (auto expression :
         {"", "0{0}", "1*(|0(11)*0?)1*", "(01)*", "[0-9]*[1-9]", "(0{2,4}){3,5}"}) {
        test(expression);
        ++completed;
    }
    Limits tiny;
    tiny.nfa = 2;
    tiny.dfa = 2;
    tiny.products = 1;
    for (auto expression : {"(01)*", "0{40}", "[01]*0[01]*"}) {
        test(expression, tiny);
        ++completed;
    }
    for (int trial = 0; trial < 10; ++trial) {
        std::string longExpression;
        int mask = trial % 2 ? 3 : 192;
        while (Parser(longExpression, false).parse()->s.size() < 90)
            longExpression += "(" + generate(2, mask) + ")";
        test(longExpression, {}, .08);
        ++completed;
    }
    std::string saved;
    while (Parser(saved, false).parse()->s.size() < 100)
        saved += "(" + generate(2, 276) + ")";
    test(saved, {}, .15);
    ++completed;
    assert(longCases >= 11);
    Budget budget(10);
    auto r = Parser("(01)*", false).parse();
    auto d = compile(r);
    int hits = 0;
    AlgebraSearch interruptedSearch(
        d, budget, {}, [&](R, const std::string &) { ++hits; }, [](R r) { return structural(r); });
    interrupted = true;
    bool threw = false;
    try {
        interruptedSearch.run(r, .1);
    } catch (const Limit &) {
        threw = true;
    }
    interrupted = false;
    assert(threw && hits == 0);
    R reusedInput = Parser("1*(|0(11)*0?)1*", false).parse();
    DFA reusedTarget = compile(reusedInput);
    AlgebraSearch reused(
        reusedTarget, budget, {},
        [&](R candidate, const std::string &) {
            assert(compare(compileRaw(reusedInput), compileRaw(Parser(candidate->s, false).parse()))
                       .equal);
        },
        [](R r) { return structural(r); });
    auto first = reused.run(reusedInput, .1, 200);
    assert(reused.evaluated > 0);
    auto second = reused.run(reusedInput, .1, 200);
    assert(reused.evaluated > 0);
    assert(compare(compileRaw(first), compileRaw(second)).equal);
    auto started = Budget::Clock::now();
    auto tinyResult = reused.run(reusedInput, 1e-9, 200);
    assert(reused.evaluated == 0 && tinyResult->s == reusedInput->s);
    assert(std::chrono::duration<double>(Budget::Clock::now() - started).count() < .2);
    std::cout << "PASS " << completed << " random/edge/resource cases (" << longCases
              << " long phrase cases), " << offered
              << " exact independently reparsed offered results, interruption fails closed\n";
}
