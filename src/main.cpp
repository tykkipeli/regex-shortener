#include "optimize.hpp"
#include "optimality.hpp"
#include <atomic>
#include <csignal>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <thread>
namespace rx {
std::atomic<bool> interrupted{false};
}
static void stop_search(int) {
    rx::interrupted.store(true, std::memory_order_relaxed);
}
static std::string quote(const std::string &s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += char(c);
        } else if (c == '\n')
            out += "\\n";
        else if (c == '\r')
            out += "\\r";
        else if (c == '\t')
            out += "\\t";
        else if (c < 32) {
            const char *h = "0123456789abcdef";
            out += "\\u00";
            out += h[c >> 4];
            out += h[c & 15];
        } else
            out += char(c);
    }
    return out + '"';
}
static void help() {
    std::cout << R"(regex-shortener 7.0 — aequivalente SL-haaste-Ausdruecke kuerzen (C++17)

  ./regex-shortener 'REGEX' [Optionen]
  echo 'REGEX' | ./regex-shortener [Optionen]
  ./regex-shortener --file eingabe.txt [Optionen]
  ./regex-shortener --equiv 'REGEX1' 'REGEX2'
  ./regex-shortener --match 'REGEX' 'WORT'

Optionen:
  --seconds N       Suchbudget in Sekunden (Standard: 15; Vorbereitung enthalten)
  --jobs N          Kooperierende Suchlaeufe parallel (Standard: 1)
  --seed N          Start der Zufallsfolge (Standard: 1)
  --checkpoint DATEI Besten geprueften Ausdruck fortlaufend atomar speichern
  --synth N         Budget der Grammatik-Aufzaehlung (Standard: 20000; 0 = aus)
  --beam N          Anzahl alternativer Ausgangsformen (Standard: 12)
  --max-states N    DFA-Zustandslimit (Standard: 4096)
  --max-nfa N       NFA-Zustandslimit (Standard: 12000)
  --json            Maschinenlesbare Ausgabe mit Statistik
  --prove-seconds N Zusaetzliches Optimalitaetsbudget (Standard: 0.25; 0 = aus)
  --prove-length N  Untersuchte kuerzere Syntaxlaengen (Standard: 8)
  --prove-only      Nur die Optimalitaetspruefung ausfuehren
  --quiet           Keine Zwischenberichte auf stderr
  --db DATEI        Eigenen Sprachkatalog aktivieren (Standard: abgeschaltet)
  --no-catalog      Katalog weder lesen noch schreiben
  --learn REGEX     Ausdruck in den Katalog aufnehmen
  --version         Versionsnummer
  --help            Diese Hilfe

stdout enthaelt nur den besten Ausdruck (oder JSON). Fortschritt geht auf stderr.
Strg+C beendet die Suche und gibt das beste bereits gepruefte Ergebnis aus.
Es wird die ganze Zeichenkette abgeglichen. Alphabet: 0 bis 9.
Unterstuetzt: | () * + ? {a} {a,b} {a,} und [0-9].
Keine Anker, Lookaheads, Rueckverweise, Punkte oder negierten Klassen.
)";
}
int main(int argc, char **argv) {
    using namespace rx;
    std::signal(SIGINT, stop_search);
    std::signal(SIGTERM, stop_search);
    try {
        Options opts;
        bool json = false, noCatalog = true;
        bool explicitlyDisabledCatalog = false;
        std::string database, checkpoint;
        int jobs = 1, proveLength = 8;
        double proveSeconds = 0.25;
        std::string input, file, mode = "opt";
        std::vector<std::string> args;
        auto value = [&](int &i) {
            if (i + 1 >= argc)
                throw std::runtime_error("Wert fuer " + std::string(argv[i]) + " fehlt");
            return std::string(argv[++i]);
        };
        auto integer = [&](int &i, I low, I high) {
            std::string s = value(i);
            size_t end = 0;
            I n = std::stoll(s, &end);
            if (end != s.size() || n < low || n > high)
                throw std::runtime_error("Ungueltiger Zahlenwert: " + s);
            return n;
        };
        for (int i = 1; i < argc; ++i) {
            std::string s = argv[i];
            if (s == "--version") {
                std::cout << "regex-shortener 7.0\n";
                return 0;
            }
            if (s == "--help" || s == "-h") {
                help();
                return 0;
            } else if (s == "--seconds") {
                std::string v = value(i);
                size_t end = 0;
                opts.seconds = std::stod(v, &end);
                if (end != v.size() || !std::isfinite(opts.seconds) || opts.seconds <= 0 ||
                    opts.seconds > 604800)
                    throw std::runtime_error("--seconds muss zwischen 0 und 604800 liegen");
            } else if (s == "--seed")
                opts.seed = uint64_t(integer(i, 0, std::numeric_limits<I>::max()));
            else if (s == "--checkpoint")
                checkpoint = value(i);
            else if (s == "--jobs")
                jobs = int(integer(i, 1, 64));
            else if (s == "--beam")
                opts.beam = int(integer(i, 1, 200));
            else if (s == "--synth")
                opts.synthesis = int(integer(i, 0, 10000000));
            else if (s == "--prove-length")
                proveLength = int(integer(i, 0, 64));
            else if (s == "--prove-seconds") {
                std::string v = value(i);
                size_t end = 0;
                proveSeconds = std::stod(v, &end);
                if (end != v.size() || !std::isfinite(proveSeconds) || proveSeconds < 0 || proveSeconds > 604800)
                    throw std::runtime_error("--prove-seconds muss zwischen 0 und 604800 liegen");
            }
            else if (s == "--max-states")
                opts.limits.dfa = int(integer(i, 2, 100000));
            else if (s == "--max-nfa")
                opts.limits.nfa = int(integer(i, 2, 100000));
            else if (s == "--quiet" || s == "-q")
                opts.quiet = true;
            else if (s == "--json")
                json = true;
            else if (s == "--db") {
                database = value(i);
                if (!explicitlyDisabledCatalog)
                    noCatalog = false;
            } else if (s == "--no-catalog") {
                noCatalog = true;
                explicitlyDisabledCatalog = true;
            } else if (s == "--learn") {
                mode = "learn";
                if (!explicitlyDisabledCatalog)
                    noCatalog = false;
            } else if (s == "--prove-only") {
                mode = "prove";
            } else if (s == "--file")
                file = value(i);
            else if (s == "--equiv")
                mode = "equiv";
            else if (s == "--match")
                mode = "match";
            else if (s == "--") {
                while (++i < argc)
                    args.emplace_back(argv[i]);
                break;
            } else if (s.size() > 1 && s[0] == '-')
                throw std::runtime_error("Unbekannte Option: " + s);
            else
                args.push_back(s);
        }
        if (mode == "equiv" || mode == "match") {
            if (args.size() != 2 || !file.empty())
                throw std::runtime_error("--equiv/--match erwartet genau zwei Argumente");
            Budget b(opts.seconds);
            R a = Parser(args[0], false).parse();
            DFA d = compile(a, opts.limits, &b);
            if (mode == "match") {
                bool m = matches(d, args[1]);
                std::cout << (m ? "true" : "false") << '\n';
                return m ? 0 : 1;
            }
            DFA e = compile(Parser(args[1], false).parse(), opts.limits, &b);
            auto c = compare(d, e, opts.limits, &b);
            if (json)
                std::cout << "{\"equivalent\":" << (c.equal ? "true" : "false")
                          << ",\"witness\":" << (c.equal ? "null" : quote(c.witness)) << "}\n";
            else if (c.equal)
                std::cout << "AEQUIVALENT\n";
            else
                std::cout << "VERSCHIEDEN; kuerzestes Gegenbeispiel: " << quote(c.witness) << "\n";
            return c.equal ? 0 : 1;
        }
        if (args.size() > 1 || (!file.empty() && !args.empty()))
            throw std::runtime_error("Genau einen Ausdruck oder --file angeben");
        if (!file.empty()) {
            std::ifstream f(file);
            if (!f)
                throw std::runtime_error("Datei kann nicht geoeffnet werden: " + file);
            std::ostringstream data;
            data << f.rdbuf();
            input = data.str();
            while (!input.empty() && (input.back() == '\n' || input.back() == '\r'))
                input.pop_back();
        } else if (!args.empty())
            input = args[0];
        else {
            if (!std::getline(std::cin, input))
                throw std::runtime_error("Keine Eingabe");
            if (!input.empty() && input.back() == '\r')
                input.pop_back();
        }
        R raw = Parser(input, false).parse();
        std::shared_ptr<LanguageCatalog> catalog;
        if (!noCatalog) {
            std::error_code error;
            auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
            auto directory =
                error ? std::filesystem::absolute(argv[0]).parent_path() : executable.parent_path();
            if (database.empty())
                database = (directory / "regex-shortener.db").string();
            catalog = std::make_shared<LanguageCatalog>();
            catalog->load(database);
            opts.catalog = catalog;
        }
        if (mode == "learn") {
            if (!catalog)
                throw std::runtime_error("--learn braucht einen aktiven Katalog");
            Budget check(opts.seconds);
            catalog->remember(key(compile(raw, opts.limits, &check)), raw);
            catalog->save(database);
            std::cout << "Gelernt; " << catalog->entries.size() << " Sprachen im Katalog.\n";
            return 0;
        }
        struct Result {
            R r;
            Stats stats;
            bool verified = false;
            std::exception_ptr error;
            std::vector<std::pair<std::string, R>> learned;
        };
        std::mutex sharedMutex;
        R shared = raw;
        std::shared_ptr<DFA> checkpointTarget;
        bool checkpointFailed = false;
        auto saveCheckpoint = [&](const std::string &text) {
            std::string temporary = checkpoint + ".tmp." + std::to_string(::getpid());
            {
                std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
                output << text << '\n';
                output.close();
                if (!output) {
                    std::remove(temporary.c_str());
                    throw std::runtime_error("Checkpoint kann nicht geschrieben werden: " + checkpoint);
                }
            }
            if (std::rename(temporary.c_str(), checkpoint.c_str())) {
                std::remove(temporary.c_str());
                throw std::runtime_error("Checkpoint kann nicht ersetzt werden: " + checkpoint);
            }
        };
        if (!checkpoint.empty())
            saveCheckpoint(input); // Fail early for an unwritable destination.
        auto share = [&](R candidate, const std::string &source) {
            std::lock_guard<std::mutex> guard(sharedMutex);
            if (!better(candidate, shared))
                return;
            bool shorter = candidate->s.size() < shared->s.size();
            shared = candidate;
            if (jobs > 1 && shorter && !opts.quiet)
                std::cerr << "[" << source << "] " << candidate->s.size()
                          << " Zeichen: " << candidate->s << "\n";
            if (!checkpoint.empty() && !checkpointFailed)
                try {
                    Budget check(10);
                    check.honorStop = false;
                    if (!checkpointTarget)
                        checkpointTarget = std::make_shared<DFA>(compileRaw(raw, opts.limits, &check));
                    DFA printed = compileRaw(Parser(candidate->s, false).parse(), opts.limits, &check);
                    if (compare(*checkpointTarget, printed, opts.limits, &check).equal)
                        saveCheckpoint(candidate->s);
                } catch (const std::exception &e) {
                    checkpointFailed = true;
                    if (!opts.quiet)
                        std::cerr << "Checkpoint: " << e.what() << "; Suche laeuft weiter.\n";
                }
        };
        std::vector<Result> results(jobs);
        std::vector<std::thread> threads;
        auto started = Budget::Clock::now();
        auto work = [&](int i) {
            try {
                if (mode == "prove") {
                    results[i].r=raw;
                    return;
                }
                Options o = opts;
                o.seed += uint64_t(i) * 0x9e3779b97f4a7c15ULL;
                o.primary = i == 0;
                if (jobs > 1)
                    o.quiet = true;
                o.onImprovement = share;
                o.sharedBest = [&]() {
                    std::lock_guard<std::mutex> guard(sharedMutex);
                    return shared;
                };
                Optimizer optimizer(o);
                results[i].r = optimizer.run(raw);
                results[i].stats = optimizer.stats;
                results[i].verified = optimizer.verified;
                if (catalog) {
                    std::vector<std::pair<std::string, R>> all;
                    for (const auto &[k, r] : optimizer.semanticBank)
                        if (r->s.size() <= 48 && k.size() <= 41 * 48)
                            all.push_back({k, r});
                    std::sort(all.begin(), all.end(), [](const auto &a, const auto &b) {
                        return better(a.second, b.second);
                    });
                    if (all.size() > 2000)
                        all.resize(2000);
                    results[i].learned = std::move(all);
                }
            } catch (...) {
                results[i].error = std::current_exception();
            }
        };
        if (jobs == 1)
            work(0);
        else {
            for (int i = 0; i < jobs; ++i)
                threads.emplace_back(work, i);
            for (auto &t : threads)
                t.join();
        }
        R best = raw;
        Stats stats;
        bool proof = false;
        for (auto &r : results) {
            if (r.error)
                std::rethrow_exception(r.error);
            if (r.r && better(r.r, best))
                best = r.r;
            proof |= r.verified;
            stats.verified += r.stats.verified;
            stats.rejected += r.stats.rejected;
            stats.eliminations += r.stats.eliminations;
            stats.synthesized += r.stats.synthesized;
            stats.catalogHits += r.stats.catalogHits;
            stats.contextCandidates += r.stats.contextCandidates;
            stats.semanticCandidates += r.stats.semanticCandidates;
            stats.automataBranches += r.stats.automataBranches;
            stats.closureCandidates += r.stats.closureCandidates;
        }
        std::string out = wrap(best, 1);
        // The original text is always a valid fallback, including redundant
        // parentheses that have disappeared from the parsed representation.
        if (out.size() >= input.size())
            out = input;
        // Independent reparse of the emitted syntax: do not trust AST printing.
        bool finalProof = out == input;
        if (!finalProof)
            try {
                Budget check(30);
                check.honorStop = false;
                Limits l = opts.limits;
                R emitted = Parser(out, false).parse();
                finalProof =
                    compare(compileRaw(raw, l, &check), compileRaw(emitted, l, &check), l, &check)
                        .equal;
            } catch (const Limit &) {
                finalProof = false;
            }
        if (!finalProof) {
            if (!opts.quiet)
                std::cerr << "Abschlusspruefung konnte die Kuerzung nicht bestaetigen; Original "
                             "bleibt erhalten.\n";
            out = input;
        }
        OptimalityResult optimal;
        optimal.expression = out;
        if (proveSeconds > 0 && !interrupted.load())
            try {
                Budget preparation(proveSeconds);
                DFA language = compile(Parser(out,false).parse(), opts.limits, &preparation);
                proof = true;
                double remaining=std::chrono::duration<double>(preparation.end-Budget::Clock::now()).count();
                optimal = OptimalityCheck(language,opts.limits).run(out,std::max(0.0,remaining),proveLength);
                out = optimal.expression;
            } catch (const Limit &) {
                optimal.reason = "resource_limit";
            }
        else if (interrupted.load())
            optimal.reason = "interrupted";
        if (catalog)
            try {
                for (const auto &r : results)
                    for (const auto &[k, expr] : r.learned)
                        catalog->remember(k, expr);
                Budget check(5);
                check.honorStop = false;
                R emitted = Parser(out, false).parse();
                catalog->remember(key(compile(emitted, opts.limits, &check)), emitted);
                catalog->save(database);
            } catch (const std::exception &e) {
                if (!opts.quiet)
                    std::cerr << "Katalog: " << e.what() << "; Suchergebnis bleibt erhalten.\n";
            }
        if (!checkpoint.empty())
            try { saveCheckpoint(out); }
            catch (const std::exception &e) {
                if (!opts.quiet) std::cerr << e.what() << "; Ergebnis steht auf stdout.\n";
            }
        double elapsed = std::chrono::duration<double>(Budget::Clock::now() - started).count();
        if (!opts.quiet)
            std::cerr << "Fertig: " << input.size() << " -> " << out.size() << " Zeichen; "
                      << stats.eliminations << " Eliminationslaeufe, " << stats.synthesized
                      << " synthetisierte Sprachen; " << std::fixed << std::setprecision(2)
                      << elapsed << " s.\n";
        if (!opts.quiet && proveSeconds > 0)
            std::cerr << (optimal.proven ? "BEWIESEN KUERZEST: " : "Optimalitaet nicht bewiesen: ")
                      << "Laengenuntergrenze " << optimal.lowerBound << ", Ergebnislaenge "
                      << out.size() << " (" << optimal.reason << ").\n";
        if (json)
            std::cout << "{\"input\":" << quote(input) << ",\"regex\":" << quote(out)
                      << ",\"before\":" << input.size() << ",\"after\":" << out.size()
                      << ",\"equivalent\":true,\"proof\":"
                      << quote(out == input ? "identity" : "dfa_product")
                      << ",\"input_dfa_compiled\":" << (proof ? "true" : "false")
                      << ",\"seconds\":" << std::fixed << std::setprecision(6) << elapsed
                      << ",\"eliminations\":" << stats.eliminations
                      << ",\"synthesized_languages\":" << stats.synthesized
                      << ",\"rejected_candidates\":" << stats.rejected
                      << ",\"catalog_hits\":" << stats.catalogHits
                      << ",\"context_candidates\":" << stats.contextCandidates
                      << ",\"semantic_candidates\":" << stats.semanticCandidates
                      << ",\"automata_branches\":" << stats.automataBranches
                      << ",\"closure_candidates\":" << stats.closureCandidates
                      << ",\"optimal\":" << (optimal.proven ? "true" : "false")
                      << ",\"optimality\":{\"proven\":" << (optimal.proven ? "true" : "false")
                      << ",\"lower_bound\":" << optimal.lowerBound
                      << ",\"method\":" << quote(optimal.method)
                      << ",\"reason\":" << quote(optimal.reason)
                      << ",\"profiles\":" << optimal.profiles
                      << ",\"refinements\":" << optimal.refinements
                      << ",\"seconds\":" << optimal.seconds << "}}\n";
        else
            std::cout << out << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Fehler: " << e.what() << '\n';
        return 2;
    }
}
