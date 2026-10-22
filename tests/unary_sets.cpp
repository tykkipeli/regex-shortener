// Independent finite-set oracle for the unary sumset optimizer.
#include "../src/optimize.hpp"
#include <cassert>
namespace rx {
std::atomic<bool> interrupted{false};
}
int main() {
    using namespace rx;
    std::mt19937 rng(381271);
    int checks = 0;
    for (int trial = 0; trial < 48; ++trial) {
        std::set<int> values;
        if (trial < 36) {
            int count = 4 + int(rng() % 7);
            for (int i = 0; i < count; ++i)
                values.insert(int(rng() % 50));
        } else {
            int shift = int(rng() % 8), a = 1 + int(rng() % 9), b = 1 + int(rng() % 11);
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 3; ++j)
                    values.insert(shift + i * a + j * b);
            values.insert(int(rng() % 55));
        }
        std::vector<int> lengths(values.begin(), values.end());
        int scale = 1 + trial % 3;
        std::vector<R> literal;
        for (int length : lengths)
            literal.push_back(rawRep(chars(1), scale * length, scale * length));
        DFA target = compileRaw(alt(literal));
        Budget budget(15);
        UnarySearch search(
            target, budget, 0, [](R r) { return structural(r); },
            [&](R r, const std::string &) {
                assert(compare(target, compileRaw(Parser(r->s, false).parse())).equal);
                ++checks;
            });
        search.stride = scale;
        R result = search.finiteFactors(lengths, 0.08);
        assert(compare(target, compileRaw(Parser(result->s, false).parse())).equal);
        ++checks;
    }
    std::cout << "OK: " << checks << " independent unary finite-set checks.\n";
}
