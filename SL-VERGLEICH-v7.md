# Vollständiger SL-haaste-Vergleich – Version 7.0

Öffentliche Rekordlängen und Definitionen: https://www.ohjelmointiputka.net/slh/, am 22. Oktober 2026 abgerufen.

**45 von 45 veröffentlichten Rekordlängen erreicht.** Alle 45 Ausgaben bestehen den vollständigen Sprachvergleich mit dem unabhängig konstruierten Aufgabenautomaten. 14 Eingaben waren bereits rekordkurz. Das neue Ergebnis für 2.9 ist 71 Zeichen.

## Testbedingungen

- Eingaben unverändert aus `examples/sl-inputs.json`; diese wurden vor der neuen Suche durch eine feste Automatenelimination erzeugt.
- Höchstens 180 Sekunden pro Aufgabe, `--synth 0 --no-catalog --seed 1`, ein Job pro Suchprozess. Vier verschiedene Aufgaben dürfen parallel laufen. Der Sucher liest weder Aufgabennummern noch Rekordlängen oder Referenzlösungen.
- Das externe Testprogramm kennt die Rekordlänge und sendet beim Erreichen Strg+C. Bereits rekordkurze Eingaben können nach der Automatenvorbereitung beendet werden. Diese Zeitersparnis verändert den Suchalgorithmus nicht.
- Alle ausgegebenen Zeichenfolgen werden frisch geparst und exakt gegen das Aufgabenmodell verglichen. Dies beweist die Übereinstimmung mit diesem Modell; die Übersetzung der finnischen Beschreibung ist nicht maschinell formalisiert. Bei 2.5 und 2.6 dürfen sich Anfangs- und Endpaar für kurze Wörter überlappen.
- Hardware, Auslastung, Eingabedarstellung, Zeitgrenzen und Zufallsfolge beeinflussen die Suchpfade. Der Test belegt diese konkreten Läufe und garantiert nicht für jede gleichwertige Eingabe dieselbe Laufzeit oder Länge.

## Ergebnisse

| Aufgabe | Eingabe | Version 7 | Rekord | Erreicht | Laufzeit (s) |
| --- | ---: | ---: | ---: | :---: | ---: |
| [1.1](https://www.ohjelmointiputka.net/slh/1.1/) | 10 | 10 | 10 | ja | 0.006 |
| [1.2](https://www.ohjelmointiputka.net/slh/1.2/) | 14 | 14 | 14 | ja | 0.000 |
| [1.3](https://www.ohjelmointiputka.net/slh/1.3/) | 7 | 7 | 7 | ja | 0.000 |
| [1.4](https://www.ohjelmointiputka.net/slh/1.4/) | 15 | 15 | 15 | ja | 0.000 |
| [1.5](https://www.ohjelmointiputka.net/slh/1.5/) | 19 | 15 | 15 | ja | 0.003 |
| [1.6](https://www.ohjelmointiputka.net/slh/1.6/) | 7 | 7 | 7 | ja | 0.000 |
| [1.7](https://www.ohjelmointiputka.net/slh/1.7/) | 15 | 13 | 13 | ja | 0.003 |
| [1.8](https://www.ohjelmointiputka.net/slh/1.8/) | 38 | 19 | 19 | ja | 0.008 |
| [1.9](https://www.ohjelmointiputka.net/slh/1.9/) | 33 | 16 | 16 | ja | 0.584 |
| [2.1](https://www.ohjelmointiputka.net/slh/2.1/) | 91 | 17 | 17 | ja | 0.005 |
| [2.2](https://www.ohjelmointiputka.net/slh/2.2/) | 64 | 13 | 13 | ja | 0.146 |
| [2.3](https://www.ohjelmointiputka.net/slh/2.3/) | 28 | 21 | 21 | ja | 0.004 |
| [2.4](https://www.ohjelmointiputka.net/slh/2.4/) | 16 | 16 | 16 | ja | 0.000 |
| [2.5](https://www.ohjelmointiputka.net/slh/2.5/) | 55 | 45 | 45 | ja | 16.153 |
| [2.6](https://www.ohjelmointiputka.net/slh/2.6/) | 71 | 43 | 43 | ja | 0.005 |
| [2.7](https://www.ohjelmointiputka.net/slh/2.7/) | 89 | 89 | 89 | ja | 0.001 |
| [2.8](https://www.ohjelmointiputka.net/slh/2.8/) | 56 | 37 | 37 | ja | 0.738 |
| [2.9](https://www.ohjelmointiputka.net/slh/2.9/) | 151 | 71 | 71 | ja | 5.256 |
| [3.1](https://www.ohjelmointiputka.net/slh/3.1/) | 14 | 14 | 14 | ja | 0.000 |
| [3.2](https://www.ohjelmointiputka.net/slh/3.2/) | 13 | 13 | 13 | ja | 0.000 |
| [3.3](https://www.ohjelmointiputka.net/slh/3.3/) | 30 | 17 | 17 | ja | 0.224 |
| [3.4](https://www.ohjelmointiputka.net/slh/3.4/) | 26 | 25 | 25 | ja | 0.067 |
| [3.5](https://www.ohjelmointiputka.net/slh/3.5/) | 60 | 50 | 50 | ja | 0.149 |
| [3.6](https://www.ohjelmointiputka.net/slh/3.6/) | 37 | 18 | 18 | ja | 0.001 |
| [3.7](https://www.ohjelmointiputka.net/slh/3.7/) | 87 | 9 | 9 | ja | 0.002 |
| [3.8](https://www.ohjelmointiputka.net/slh/3.8/) | 22 | 21 | 21 | ja | 0.079 |
| [3.9](https://www.ohjelmointiputka.net/slh/3.9/) | 19 | 13 | 13 | ja | 0.056 |
| [4.1](https://www.ohjelmointiputka.net/slh/4.1/) | 333 | 37 | 37 | ja | 0.002 |
| [4.2](https://www.ohjelmointiputka.net/slh/4.2/) | 84 | 19 | 19 | ja | 0.005 |
| [4.3](https://www.ohjelmointiputka.net/slh/4.3/) | 83 | 25 | 25 | ja | 0.025 |
| [4.4](https://www.ohjelmointiputka.net/slh/4.4/) | 29 | 29 | 29 | ja | 0.001 |
| [4.5](https://www.ohjelmointiputka.net/slh/4.5/) | 44 | 44 | 44 | ja | 0.001 |
| [4.6](https://www.ohjelmointiputka.net/slh/4.6/) | 70 | 49 | 49 | ja | 0.622 |
| [4.7](https://www.ohjelmointiputka.net/slh/4.7/) | 49 | 49 | 49 | ja | 0.000 |
| [4.8](https://www.ohjelmointiputka.net/slh/4.8/) | 56 | 18 | 18 | ja | 0.001 |
| [4.9](https://www.ohjelmointiputka.net/slh/4.9/) | 140 | 49 | 49 | ja | 0.645 |
| [5.1](https://www.ohjelmointiputka.net/slh/5.1/) | 89 | 69 | 69 | ja | 18.643 |
| [5.2](https://www.ohjelmointiputka.net/slh/5.2/) | 159 | 126 | 126 | ja | 18.896 |
| [5.3](https://www.ohjelmointiputka.net/slh/5.3/) | 38 | 31 | 31 | ja | 5.543 |
| [5.4](https://www.ohjelmointiputka.net/slh/5.4/) | 42 | 42 | 42 | ja | 0.000 |
| [5.5](https://www.ohjelmointiputka.net/slh/5.5/) | 239 | 49 | 49 | ja | 0.011 |
| [5.6](https://www.ohjelmointiputka.net/slh/5.6/) | 92 | 51 | 51 | ja | 0.069 |
| [5.7](https://www.ohjelmointiputka.net/slh/5.7/) | 87 | 63 | 63 | ja | 0.982 |
| [5.8](https://www.ohjelmointiputka.net/slh/5.8/) | 52 | 24 | 24 | ja | 0.161 |
| [5.9](https://www.ohjelmointiputka.net/slh/5.9/) | 108 | 108 | 108 | ja | 0.000 |

## Aufgabe 2.9 und Zeitmessung

Der kalte vollständige Aufgabenvergleich erreicht 71 Zeichen nach rund 5,3 Sekunden. Drei weitere kalte CLI-Läufe mit Checkpoint und externer Unterbrechung beim Erreichen von 71 Zeichen ergaben:

| Jobs | Seed | Zeit bis 71 (s) |
| ---: | ---: | ---: |
| 1 | 1 | 4.355 |
| 4 | 1 | 4.530 |
| 1 | 2 | 4.346 |

Gemessen wurde bis zur Fortschrittsmeldung des geprüften 71-Zeichen-Treffers. Der aktivierte Checkpoint wurde bis zum Prozessende zusätzlich geprüft und gespeichert. Die vier Jobs sind kooperierende Läufe; nur der erste erzeugt die anfängliche deterministische Grammatikbank. In diesen Läufen sind vier Jobs etwas langsamer als einer. Ein langsamerer Rechner kann deutlich mehr Zeit benötigen. Ein Vergleich zum V6-Ergebnis von 72 Zeichen bei 180 Sekunden steht in `examples/benchmarks-sl-v6.json`; das ist kein Messpaar unter identischer Auslastung.

Ein selbstständig gefundener und vollständig geprüfter Ausdruck ist:

```text
(0(00|0{8}){2}|0{41})(|00|0{12}(|0{14})(|0{30}))|000?|0{71}|0{79}|0{89}
```

Die allgemeine Längengrammatik baut kurze Mengenbeschreibungen aus Union, Verkettung und beschränkter Wiederholung. Kompatible Quotienten erzeugen größere Produkte; komplementäre Restmengen und Überdeckungen bilden die ganze Sprache. Im Suchcode stehen weder die Primzahlbedingung noch diese Lösung oder ihre Länge. Der Sucher benötigt keinen externen Optimierer.

## Beweisstatus

71 erreicht den öffentlichen Rekord und ist exakt äquivalent zur Aufgabe. **Ein globaler Mindestlängenbeweis für 71 wird nicht behauptet.**

Ein separater Durchlauf mit `--prove-only`, Standardbudget 0,25 Sekunden und Syntaxgrenze 8 prüft die 45 Ausgaben. Für 1.3 (`[01]{5}`) und 1.6 (`(0{5})*`) wird die globale Mindestlänge bewiesen. Für die anderen 43 Ergebnisse wird innerhalb dieser Grenzen keine Optimalität behauptet. Alle 45 Nachprüfungen wurden wieder exakt gegen die ursprüngliche Benchmarkeingabe verglichen. Details: `examples/optimality-v7.json` und `OPTIMALITAET.md`.

Die Grammatikbank des neuen heuristischen Suchers ist speicher- und längenbegrenzt. Sie ist vom vollständigen Optimalitätsprüfer getrennt. Aus einer ausgeschöpften Bank oder einer Nacht ohne Verbesserung wird niemals ein Minimalitätsbeweis abgeleitet.

## Reproduktion

```bash
bash build.sh
python3 sl_benchmark.py --seconds 180 --workers 4 --stop-at-record --output vergleich.json
```

Vollständige Ausdrücke und Einstellungen: `examples/benchmarks-sl-v7.json`; zusätzliche Zeitmessungen: `examples/performance-v7.json`.
