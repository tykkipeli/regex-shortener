// Independent task models are supplied by sl_benchmark.py. This adapter
// generates a plain GNFA baseline or checks the emitted regex against a DFA.
#include "../src/optimize.hpp"
namespace rx {
std::atomic<bool> interrupted{false};
}
int main(int argc, char **argv) {
    using namespace rx;
    try {
        int n;
        std::cin >> n;
        if (!std::cin || n < 1 || n > 2000)
            return 2;
        DFA d;
        d.t.resize(n);
        d.accept.resize(n);
        for (int p = 0; p < n; ++p) {
            int a;
            std::cin >> a;
            d.accept[p] = a;
            for (int &q : d.t[p]) {
                std::cin >> q;
                if (q < 0 || q >= n)
                    return 2;
            }
        }
        if (!std::cin)
            return 2;
        Budget b(30);
        if (argc == 2) {
            DFA actual = compileRaw(Parser(argv[1], false).parse(), {}, &b);
            auto c = compare(d, actual, {}, &b);
            if (!c.equal)
                std::cerr << "Separating word: '" << c.witness << "'\n";
            return c.equal ? 0 : 1;
        }
        Options o;
        o.quiet = true;
        o.seconds = 10;
        Optimizer optimizer(o);
        d = minimize(d, &b);
        // One fixed elimination order: no record expression, seed catalog,
        // stochastic search, or newly added pattern search is used here.
        R r = optimizer.eliminate(optimizer.graph(d), 0);
        if (!compare(d, compileRaw(Parser(r->s, false).parse(), {}, &b), {}, &b).equal)
            return 3;
        std::cout << r->s << '\n';
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 2;
    }
}
