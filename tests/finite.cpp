#include "../src/finite.hpp"
#include "../src/optimize.hpp"
#include <cassert>
namespace rx {
std::atomic<bool> interrupted{false};
}
using namespace rx;
int main() {
    std::mt19937 rng(71);
    int offers = 0;
    for (int trial = 0; trial < 120; ++trial) {
        // Construct the reference directly as a union of literal words;
        // rectangle extraction and cover search do not participate in it.
        std::set<std::string> words;
        for (int j = 0, n = 1 + int(rng() % 25); j < n; ++j) {
            std::string word;
            for (int i = 0, length = int(rng() % 5); i < length; ++i)
                word += char('0' + rng() % 4);
            words.insert(word);
        }
        std::vector<R> literals;
        for (const auto &word : words) {
            std::vector<R> charsOfWord;
            for (char c : word)
                charsOfWord.push_back(chars(1 << (c - '0')));
            literals.push_back(cat(charsOfWord));
        }
        DFA source = compileRaw(alt(literals));
        Budget budget(2);
        FiniteSearch search(
            source, budget, {},
            [&](R r, const std::string &) {
                ++offers;
                assert(compare(source, compileRaw(Parser(r->s, false).parse())).equal);
            },
            [](R r) { return structural(r); });
        try {
            search.run();
        } catch (const Limit &) {
            // A finite resource cap is permitted, an unsafe offer is not.
        }
    }
    for (const auto &input : {"0*", "[01]*1", "(01)+"}) {
        DFA source = compile(Parser(input).parse());
        Budget budget(1);
        FiniteSearch search(
            source, budget, {}, [](R, const std::string &) { std::abort(); },
            [](R r) { return r; });
        search.run();
    }
    assert(offers >= 120);
    std::cout << "OK: 120 unabhaengige endliche Sprachen, " << offers
              << " exakt gepruefte Kandidaten; unendliche Sprachen erkannt.\n";
}
