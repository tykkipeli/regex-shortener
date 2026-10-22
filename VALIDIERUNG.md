# Validierung von Version 7.0

Geprüft am 22. Oktober 2026 unter Linux mit GCC 13.3.0. Der direkte Release-Build (`-std=c++17 -O3 -DNDEBUG -pthread -Wall -Wextra -Wpedantic`) war erfolgreich und ohne Compilerwarnungen. Es werden keine externen Solver oder C++-Bibliotheken verwendet.

## Neue Längengrammatik

- **4.443 unabhängige Prüfungen** im Release-Modultest bestanden. Faltung und kompatible Verschiebungen werden mit unabhängig berechneten Ganzzahlmengen verglichen. Stichproben der erzeugten Syntax werden frisch kompiliert und an allen Längen 0 bis 127 geprüft. 18 zufällige Holdout-Mengen verwenden sämtliche Ziffern 0 bis 9; 69 angebotene Verbesserungen wurden vollständig geprüft.
- Die Primzahlbedingung wird ausschließlich im Test als unabhängiges Prädikat berechnet und in einen einfachen Kettenautomaten übersetzt. Eine lange Vereinigung einzelner Längen dient als Eingabe. Der Suchcode findet ohne Katalog selbst eine exakt gleichwertige Lösung mit höchstens 71 Zeichen.
- AddressSanitizer und UndefinedBehaviorSanitizer bestanden **4.431 Prüfungen**, einschließlich desselben Primzahltests und 63 bestätigter Holdout-Verbesserungen. Unterschiede bei Zwischenverbesserungen und Stichprobenzahlen entstehen durch lokale Zeitgrenzen. Alle festen Fälle werden ausgeführt. Es gab keine Sanitizer-Meldungen; Leak-Erkennung war wegen der Laufzeitumgebung deaktiviert.
- Die Grammatikbank ist heuristisch begrenzt. Ihre Dominanzregeln und Ressourcenabbrüche werden niemals als Beweis der kürzesten möglichen Darstellung verwendet.

## Gesamtes Programm und unveränderte Beweislogik

- **6.520 mathematische Kernprüfungen** bestanden, einschließlich unabhängiger DFA-Minimalitätsprüfung.
- **1.000 Ende-zu-Ende-Zufallstests** bestanden, einschließlich Wortprüfungen gegen Python `re.fullmatch`, Syntax, Limits, Katalog, Parallelbetrieb und Strg+C.
- Neue CLI-Tests beobachten einen verbesserten Checkpoint bereits während eines noch laufenden Zwei-Job-Prozesses, unterbrechen ihn, vergleichen den gespeicherten Ausdruck mit der Abschlussausgabe und prüfen ihn exakt. Erneutes Einlesen und Speichern derselben Datei, Epsilon-Ausgabe, fehlende Elternverzeichnisse und die Beseitigung temporärer Dateien sind abgedeckt.
- Die Optimalitätsprüfung bestand erneut **8.135 unabhängige Prüfungen**.
- Der unabhängige rohe Syntaxprüfer bestand erneut **17.535 Ausdrücke**, **255 Wörter**, **16 Mindestlängenfälle** und drei unvollständige beziehungsweise abgeschaltete Prüfungen.
- Die unveränderten weiteren Modulprüfungen aus V5/V6 bleiben in `test.sh` enthalten. Sie wurden in diesem Update nicht sämtlich erneut separat kompiliert. Ein vollständiger Sanitizerlauf des ganzen CLI-Programms oder ein ausgeführter CMake-Build wird nicht behauptet.

## Vollständiger SL-haaste-Vergleich

Der kalte Durchlauf erreicht **45 von 45 öffentlichen Rekordlängen**, ohne Katalog. Alle 45 Ergebnisausdrücke bestehen den vollständigen Sprachvergleich zum unabhängig konstruierten Aufgabenmodell. 14 Eingaben waren schon rekordkurz.

Einstellungen: höchstens 180 Sekunden pro Aufgabe, `--synth 0 --no-catalog --seed 1`, ein Job pro Aufgabe, höchstens vier verschiedene Aufgaben parallel. Nur der externe Benchmark kennt die Rekordlänge und darf nach ihrem Erreichen Strg+C senden. Der Sucher erhält keinen solchen Zielwert.

2.9 erreicht die 71 Zeichen im vollständigen Vergleich nach rund **5,3 Sekunden**. Drei separate kalte CLI-Messungen mit Checkpoint fanden 71 Zeichen nach **4,355 s** (ein Job, Seed 1), **4,530 s** (vier Jobs, Seed 1) und **4,346 s** (ein Job, Seed 2). Jeder Abschluss und Checkpoint wurde erneut exakt zur ursprünglichen Eingabe verglichen. Ein zusätzlicher Lauf über volle **75 Sekunden mit vier Jobs**, ohne externe Unterbrechung, behielt eine exakt bestätigte 71-Zeichen-Lösung; Checkpoint und Abschluss stimmen überein. Diese Messungen sagen nichts Verbindliches über andere Rechner aus.

Messdaten und Testbedingungen stehen in `SL-VERGLEICH-v7.md`, `examples/benchmarks-sl-v7.json` und `examples/performance-v7.json`.

Ein separater Standard-Optimalitätslauf prüft alle 45 Ergebnisausdrücke und bestätigt für 1.3 und 1.6 globale Mindestlängen. Die übrigen 43 Ergebnisse, einschließlich der 71 Zeichen für 2.9, werden nicht als global optimal ausgegeben. Sämtliche 45 Nachprüfungen bestehen erneut den vollständigen Vergleich mit der eingefrorenen ursprünglichen Eingabe. Daten: `examples/optimality-v7.json`.

Reproduktion:

```bash
bash test.sh
RX_FUZZ_CASES=1000 python3 tests/test.py ./regex-shortener
python3 sl_benchmark.py --seconds 180 --workers 4 --stop-at-record --output vergleich.json
```
