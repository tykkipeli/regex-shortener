# SL-haaste-Vergleich für Version 4.0

Öffentliche Rekordlängen und Aufgaben: [offizielle Aufgabenliste](https://www.ohjelmointiputka.net/slh/), abgerufen am 22. Oktober 2026. Die Rekordlängen können sich ändern. Die zugehörigen Aufgabenseiten sind in der JSON-Datei pro Fall verlinkt.

**34 von 43 Aufgaben erreichen die veröffentlichte Rekordlänge**, verglichen mit 26 in Version 3.0 bei denselben Eingaben und Einstellungen. 14 Eingaben waren bereits höchstens so lang wie der veröffentlichte Rekord; diese sind in beiden Zahlen enthalten. Version 4 kürzte gegenüber Version 3 bei 8 Fällen stärker, bei 2 Fällen blieb sie länger.

## Versuchsaufbau

- 43 unabhängig aus den Aufgabenbedingungen konstruierte, vollständige DFA über den Ziffern 0 bis 9; unzulässige Alphabetzeichen führen in einen verwerfenden Zustand. Die Modelle stehen offen in `sl_benchmark.py`.
- Eine feste deterministische GNFA-Eliminationsreihenfolge erzeugt die Regex-Eingaben. Sie nutzt weder eine Rekordlösung noch den Sprachkatalog oder die neue Mustersuche. Fabrikfunktionen können dabei bereits kürzen; dieser Benchmark bewertet nicht nur schwer verschleierte Eingaben.
- Pro Optimierung: drei Sekunden Suchbudget, `--synth 0 --no-catalog --seed 1`, ein Suchlauf; zwei Fälle parallel. Für beide Versionen identische Eingaben und Optionen. Such- und Prüfzeiten sind in der JSON-Datei enthalten. Die neue Version wurde nach Abschluss aller Änderungen nochmals gemessen.
- Jeder Ergebnisausdruck wird zusätzlich zur internen Abschlussprüfung exakt gegen den unabhängig konstruierten Aufgabenautomaten geprüft. Alle 43 Prüfungen bestanden. Das beweist die Übereinstimmung mit dem jeweiligen Modell; es ist keine formale Prüfung der Übersetzung der finnischen Aufgabenbeschreibung.
- Rekordlängen dienen ausschließlich dem nachträglichen Vergleich. Sie werden dem Optimierer nicht übergeben und die zugehörigen Lösungen werden nicht importiert.
- 2.5 und 2.6 sind ausgelassen: Die öffentlichen Beispiele klären nicht, ob sich Anfangs- und Endpaare bei kurzen Wörtern überlappen dürfen. Dafür wird kein ungeprüftes Ergebnis behauptet.

## Ergebnisse

| Aufgabe | Eingabe | Version 3 | Version 4 | Rekord | Erreicht |
| --- | ---: | ---: | ---: | ---: | :---: |
| [1.1](https://www.ohjelmointiputka.net/slh/1.1/) | 10 | 10 | 10 | 10 | ja |
| [1.2](https://www.ohjelmointiputka.net/slh/1.2/) | 14 | 14 | 14 | 14 | ja |
| [1.3](https://www.ohjelmointiputka.net/slh/1.3/) | 7 | 7 | 7 | 7 | ja |
| [1.4](https://www.ohjelmointiputka.net/slh/1.4/) | 15 | 15 | 15 | 15 | ja |
| [1.5](https://www.ohjelmointiputka.net/slh/1.5/) | 19 | 15 | 15 | 15 | ja |
| [1.6](https://www.ohjelmointiputka.net/slh/1.6/) | 7 | 7 | 7 | 7 | ja |
| [1.7](https://www.ohjelmointiputka.net/slh/1.7/) | 15 | 13 | 13 | 13 | ja |
| [1.8](https://www.ohjelmointiputka.net/slh/1.8/) | 38 | 35 | 19 | 19 | ja |
| [1.9](https://www.ohjelmointiputka.net/slh/1.9/) | 33 | 21 | 16 | 16 | ja |
| [2.1](https://www.ohjelmointiputka.net/slh/2.1/) | 91 | 81 | 17 | 17 | ja |
| [2.2](https://www.ohjelmointiputka.net/slh/2.2/) | 64 | 13 | 13 | 13 | ja |
| [2.3](https://www.ohjelmointiputka.net/slh/2.3/) | 28 | 23 | 21 | 21 | ja |
| [2.4](https://www.ohjelmointiputka.net/slh/2.4/) | 16 | 16 | 16 | 16 | ja |
| [2.7](https://www.ohjelmointiputka.net/slh/2.7/) | 89 | 89 | 89 | 89 | ja |
| [2.8](https://www.ohjelmointiputka.net/slh/2.8/) | 56 | 42 | 42 | 37 | offen |
| [2.9](https://www.ohjelmointiputka.net/slh/2.9/) | 151 | 100 | 100 | 71 | offen |
| [3.1](https://www.ohjelmointiputka.net/slh/3.1/) | 14 | 14 | 14 | 14 | ja |
| [3.2](https://www.ohjelmointiputka.net/slh/3.2/) | 13 | 13 | 13 | 13 | ja |
| [3.3](https://www.ohjelmointiputka.net/slh/3.3/) | 30 | 19 | 23 | 17 | offen |
| [3.4](https://www.ohjelmointiputka.net/slh/3.4/) | 26 | 25 | 25 | 25 | ja |
| [3.5](https://www.ohjelmointiputka.net/slh/3.5/) | 60 | 50 | 50 | 50 | ja |
| [3.6](https://www.ohjelmointiputka.net/slh/3.6/) | 37 | 18 | 18 | 18 | ja |
| [3.7](https://www.ohjelmointiputka.net/slh/3.7/) | 87 | 19 | 9 | 9 | ja |
| [3.8](https://www.ohjelmointiputka.net/slh/3.8/) | 22 | 21 | 21 | 21 | ja |
| [3.9](https://www.ohjelmointiputka.net/slh/3.9/) | 19 | 15 | 15 | 13 | offen |
| [4.1](https://www.ohjelmointiputka.net/slh/4.1/) | 333 | 37 | 37 | 37 | ja |
| [4.2](https://www.ohjelmointiputka.net/slh/4.2/) | 84 | 55 | 19 | 19 | ja |
| [4.3](https://www.ohjelmointiputka.net/slh/4.3/) | 83 | 56 | 25 | 25 | ja |
| [4.4](https://www.ohjelmointiputka.net/slh/4.4/) | 29 | 29 | 29 | 29 | ja |
| [4.5](https://www.ohjelmointiputka.net/slh/4.5/) | 44 | 44 | 44 | 44 | ja |
| [4.6](https://www.ohjelmointiputka.net/slh/4.6/) | 70 | 49 | 49 | 49 | ja |
| [4.7](https://www.ohjelmointiputka.net/slh/4.7/) | 49 | 49 | 49 | 49 | ja |
| [4.8](https://www.ohjelmointiputka.net/slh/4.8/) | 56 | 47 | 18 | 18 | ja |
| [4.9](https://www.ohjelmointiputka.net/slh/4.9/) | 140 | 49 | 49 | 49 | ja |
| [5.1](https://www.ohjelmointiputka.net/slh/5.1/) | 89 | 84 | 84 | 69 | offen |
| [5.2](https://www.ohjelmointiputka.net/slh/5.2/) | 159 | 148 | 148 | 126 | offen |
| [5.3](https://www.ohjelmointiputka.net/slh/5.3/) | 38 | 36 | 36 | 31 | offen |
| [5.4](https://www.ohjelmointiputka.net/slh/5.4/) | 42 | 42 | 42 | 42 | ja |
| [5.5](https://www.ohjelmointiputka.net/slh/5.5/) | 239 | 49 | 49 | 49 | ja |
| [5.6](https://www.ohjelmointiputka.net/slh/5.6/) | 92 | 51 | 51 | 51 | ja |
| [5.7](https://www.ohjelmointiputka.net/slh/5.7/) | 87 | 64 | 64 | 63 | offen |
| [5.8](https://www.ohjelmointiputka.net/slh/5.8/) | 52 | 35 | 39 | 24 | offen |
| [5.9](https://www.ohjelmointiputka.net/slh/5.9/) | 108 | 108 | 108 | 108 | ja |

## Grenzen und Reproduktion

Die Suche garantiert kein globales Optimum. Die offenen Fälle und auch die längeren Ergebnisse gegenüber Version 3 sind oben sichtbar. Neue Heuristiken verändern die gewählten Zwischenformen und verteilen das endliche Zeitbudget anders. Längere Suchzeit und mehrere unabhängige Läufe können andere Ergebnisse liefern; sie garantieren keine bestimmte Rekordlänge. Die 126-Zeichen-Darstellung für 5.2 ist im älteren Startkatalog vorhanden, wird hier aber wegen `--no-catalog` ausdrücklich nicht verwendet.

```bash
bash build.sh
python3 sl_benchmark.py --seconds 3 --output vergleich.json
# Zusätzlich dieselben Fälle mit einer alten ausführbaren Datei vergleichen:
python3 sl_benchmark.py --seconds 3 --baseline-binary /pfad/zur/alten/regex-shortener --output vergleich.json
```

Alle Eingaben, vollständigen Ausgaben, Statistiken und Quellen stehen in `examples/benchmarks-sl-v4.json`. Die früheren sechs großen Nutzerbeispiele werden separat in `examples/benchmarks-v4.json` mit und ohne Katalog verglichen. Das neu gemeldete 54-Zeichen-Beispiel erreicht 18 Zeichen ohne Katalog; seine Messung steht in `examples/latest-v4.json`.
