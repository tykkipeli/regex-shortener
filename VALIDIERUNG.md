# Validierung von Version 3.0

Geprüft am 22. Oktober 2026 mit GCC 13.3.0 unter Linux.

- Release-Build mit C++17, `-O3 -DNDEBUG -pthread -Wall -Wextra -Wpedantic`: erfolgreich und ohne Compilerwarnungen.
- **6.160 interne Prüfungen bestanden.** Enthalten sind systematische Wiederholungs- und Umformungsprüfungen, eine unabhängige DFA-Minimalitätsprüfung für 2.000 zufällige Automaten, endliche Längenmengen und verschiedene Alphabete für Nachbarschaftssprachen.
- **1.000 Zufallstests bestanden**, mit unabhängigen Vergleichen gegen Python `re.fullmatch` sowie der öffentlichen Wortprüfung.
- Sämtliche früheren Kürzungs-, Syntax-, Gegenbeispiel-, Ressourcenlimit-, Parallel- und Unterbrechungstests bestanden.
- Für das Beispiel ohne Teilwort `101010`: alle 8.191 binären Wörter bis Länge 12 gegen die direkte Teilwortbedingung geprüft.
- Persistenter Katalog: Lernen, Wiederladen, Fenster einer Verkettung, Prüfung beschädigter Einträge, abschaltbarer Katalog und konkurrierende Schreibzugriffe bestanden.
- Alle sechs neuen Vergleichsausdrücke wurden unabhängig auf vollständige Äquivalenz zur jeweiligen Eingabe geprüft.
- AddressSanitizer und UndefinedBehaviorSanitizer: interne Prüfungen, tatsächliche Optimierung, Katalogspeicherung und Fensterersetzung ohne Fehler. Die Leak-Prüfung wurde wegen Einschränkungen der Ausführungsumgebung deaktiviert.
- Der optionale CMake-Build wurde nicht ausgeführt, da CMake hier nicht installiert ist.

## Die sechs neuen Beispiele

Je Variante drei Sekunden, ein Suchlauf, `--synth 0`. Der Kataloglauf verwendet eine frische temporäre Datenbank und den offen mitgelieferten Startkatalog. Die Werte sind keine Minimalitätsbehauptung.

| Beispiel | Eingabe | Vergleich | Ohne Katalog | Mit Katalog |
| --- | ---: | ---: | ---: | ---: |
| 1: unary35 | 146 | 51 | 51 | 51 |
| 2: binary-blocks | 405 | 126 | 148 | 126 |
| 3: pigeonhole4 | 91 | 37 | 37 | 37 |
| 4: ternary-blocks | 60 | 50 | 50 | 50 |
| 5: parity-blocks | 51 | 44 | 44 | 44 |
| 6: four-symbols | 408 | 49 | 49 | 49 |

Die vollständigen Ausgaben und Einstellungen stehen in `examples/benchmarks-v3.json`. Reproduktion:

```bash
python3 benchmark.py --seconds 3 > vergleich.json
```

Beim zweiten Beispiel stammt die 126-Zeichen-Darstellung aus dem vom Nutzer gelieferten und geprüften Katalogeintrag. Die Suche allein fand in diesem Lauf 148 Zeichen. Bei den übrigen fünf Beispielen werden die Vergleichslängen auch ohne Katalog erreicht.

Jede ausgegebene Kürzung besteht weiterhin eine unabhängige Abschlussprüfung: Der ausgegebene Text wird neu geparst und die nicht minimierten deterministischen Automaten werden im Produktautomaten verglichen. Ein Katalogtreffer oder ein erschöpftes Ressourcenlimit ersetzt diesen Beweis nicht.
