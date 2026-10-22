# Validierung von Version 6.0

Geprüft am 22. Oktober 2026 mit GCC 13.3.0 unter Linux. Der Release-Build verwendet C++17, `-O3 -DNDEBUG -pthread -Wall -Wextra -Wpedantic` und ist ohne Compilerwarnungen erfolgreich.

## Neu und erneut geprüft

- **6.520 mathematische Kernprüfungen** bestanden, einschließlich einer unabhängigen Minimalitätsprüfung für 2.000 zufällige DFA.
- **1.000 Ende-zu-Ende-Zufallstests** bestanden, mit unabhängigen Vergleichen gegen Python `re.fullmatch`, öffentlicher Wortprüfung sowie Syntax-, Ressourcen-, Katalog-, Parallel- und Unterbrechungstests.
- Das neue 24-Zeichen-Ergebnis für 5.8 wurde zusätzlich an allen **8.191 binären Wörtern bis Länge 12** direkt mit der Bedingung „101010 kommt nicht als Teilwort vor“ verglichen. Dies ist ein separater Test; der Sucher bekommt dieses Wort nicht als Zusatzinformation.
- **8.135 unabhängige Prüfungen** für die Beobachtungsalgebra und den Optimalitätsprüfer bestanden. Zufällige Ausdrücke werden mit unabhängig erzeugten endlichen Wortmengen verglichen; außerdem werden bestätigte Minima, falsche obere Schranken, deaktivierte Prüfung, Syntaxgrenzen, Speichergrenzen und Unterbrechung geprüft.
- Ein zweiter Prüfer erzeugt **17.535 rohe Syntaxausdrücke bis Länge 6**, ohne die semantische Zusammenfassung oder gewichtete Suche des C++-Prüfers zu verwenden. Er vergleicht **255 Wörter** mit Python, prüft **16 Optimalitätsfälle** und löst jede kürzere Beobachtungskollision durch einen vollständigen Automatenvergleich. Dazu kommen drei ausdrücklich unvollständige oder abgeschaltete Prüfungen. Unbeschränkte Backtracking-Auswertung langer Testwörter ist dafür nicht erforderlich.
- **Kontextuelle Schleifen:** 145 unabhängige Prüfungen bestanden. Enthalten sind 30 zufällige Übergangsautomaten, 24 verschiedene unabhängig konstruierte Teilwortautomaten mit anderen Ziffern und Perioden sowie unabhängige Prüfungen der Übergangsrelationen. In diesem Release-Lauf wurden vier weitere kürzere Holdout-Ergebnisse exakt bestätigt.
- **Unäre Mengen:** 48 zufällige beziehungsweise strukturierte Mengen und Skalierungen 1 bis 3 bestanden erneut die unabhängige exakte Prüfung; einschließlich angebotener Zwischenverbesserungen wurden 162 Sprachvergleiche ausgeführt.

## Speicher und Ressourcen

AddressSanitizer und UndefinedBehaviorSanitizer bestanden für die Optimalitätsprüfung (8.135 Prüfungen), Kontextschleifen (144 Prüfungen) und die geänderte unäre Mengensuche (106 Sprachprüfungen). Unterschiede in der Zahl angebotener Zwischenverbesserungen entstehen durch die lokalen Zeitlimits; sämtliche festen Testfälle werden weiterhin ausgeführt. Es gab keine Sanitizer-Meldungen. Die Leak-Prüfung wurde wegen der Ausführungsumgebung deaktiviert.

Die unveränderten Verfahren und ihre bereits bestandenen Modulprüfungen aus Version 5 bleiben in `test.sh` enthalten: endliche Sprachen, allgemeine Überdeckungen, Automatenportfolio, Kontextmutationen, semantische Synthese, Makroaktionen, Fortsetzungsprofile und Polynom-Evolution. Die historischen Berichte dokumentieren ihre damaligen Messungen. Ein vollständiger erneuter Sanitizerlauf des gesamten Kommandozeilenprogramms wird nicht behauptet.

CMake wurde hier nicht ausgeführt; der direkte C++17-Build und die direkten Test-Builds wurden ausgeführt.

## Vollständiger Aufgabenvergleich

Der kalte Durchlauf mit höchstens 180 Sekunden pro Aufgabe erreicht **44 von 45 öffentlichen Rekordlängen**, ohne Katalog. **Alle 45 Ausgaben** bestehen den exakten Vergleich zum unabhängig modellierten Aufgabenautomaten. 14 Eingaben waren bereits rekordkurz. Nur 2.9 bleibt mit 72 statt 71 Zeichen offen.

Die Rekordlänge gelangt nicht in den Sucher. Das externe Testprogramm darf nach einem Rekordtreffer Strg+C senden; diese externe Zeitersparnis ist in der JSON-Datei ausgewiesen. Die Modelle, ihre Grenzen, vollständigen Ergebnisse und Einstellungen stehen in `SL-VERGLEICH-v6.md` und `examples/benchmarks-sl-v6.json`.

Ein separater Durchlauf prüft die Optimalität der 45 Ergebnisausdrücke mit dem Standardbudget. Zwei Mindestlängen werden bewiesen; bei den übrigen wird ausdrücklich kein globales Optimum behauptet. Jeder dieser Ausdrücke wurde zusätzlich erneut exakt zur ursprünglichen Benchmarkeingabe verglichen. Daten: `examples/optimality-v6.json`.

Eine Stichprobe, ein Katalogtreffer, ein Rekordtreffer oder ein Ressourcenlimit ersetzt niemals einen Sprach- oder Optimalitätsbeweis. Die mathematische Begründung der neuen vollständigen Untergrenzensuche steht in `OPTIMALITAET.md`.

Reproduktion:

```bash
bash test.sh
RX_FUZZ_CASES=1000 python3 tests/test.py ./regex-shortener
python3 sl_benchmark.py --seconds 180 --workers 4 --stop-at-record --output vergleich.json
```
