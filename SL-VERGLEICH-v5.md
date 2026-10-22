# SL-haaste-Vergleich für Version 5.0

Aufgaben und öffentliche Rekordlängen: [offizielle Aufgabenliste](https://www.ohjelmointiputka.net/slh/), geprüft am 22. Oktober 2026. Veröffentlichte Rekorde sind keine bewiesenen Minima.

**43 von 45 Aufgaben erreichen die veröffentlichte Rekordlänge.** Alle 45 Ergebnisausdrücke bestehen den vollständigen Sprachvergleich mit einem unabhängig konstruierten Aufgabenautomaten. 14 Eingaben waren bereits höchstens so lang wie der Rekord und sind in dieser Zahl enthalten. Das Ziel aller 45 Rekorde ist damit noch nicht erreicht.

## Versuchsaufbau

- Eingefrorene, automatisch aus Aufgabenautomaten erzeugte Eingaben aus `examples/sl-inputs.json`. Die Eingaben stammen aus einer festen GNFA-Elimination und können durch die allgemeinen Ausdruckskonstruktoren bereits kurz sein.
- Pro Aufgabe höchstens 180 Sekunden, `--synth 0 --no-catalog --seed 1`, ein Suchlauf. Vier Aufgaben dürfen parallel laufen. Kein Katalog wird geladen, auch keine ältere persönliche Datenbank.
- Der Sucher bekommt ausschließlich den Ausdruck und allgemeine Optionen. Aufgabennummern, Rekordlängen und Referenzlösungen werden ihm nicht übergeben. Die mitgelieferten historischen Vergleichsausdrücke gehören zu Tests und Berichten.
- Das externe Testprogramm beobachtet die Fortschrittsausgabe und sendet Strg+C beim Erreichen der Rekordlänge. Dafür kennt nur das Testprogramm die Länge. Bereits rekordkurze Eingaben können unmittelbar nach der Automatenkonstruktion beendet werden. Ein solcher Test misst das Erreichen eines Vergleichswerts, nicht die volle Optimierung über 180 Sekunden.
- Jedes Ergebnis wird frisch geparst und exakt gegen den jeweiligen Aufgabenautomaten verglichen. Die Modelle stehen in `sl_benchmark.py` offen. Der Test beweist die Gleichheit zum Modell, nicht die formale Übersetzung der finnischen Beschreibung. Bei 2.5 und 2.6 dürfen sich Anfangs- und Endpaar für kurze Wörter überlappen.
- Zeitgrenzen, Rechnerauslastung und Zufallsfolge beeinflussen die Suchpfade. Die gemessenen Ergebnisse sind keine Garantie für jeden Lauf oder jede andere Darstellung derselben Eingabesprache.

## Ergebnisse

| Aufgabe | Eingabe | Version 5 | Rekord | Erreicht | Laufzeit (s) |
| --- | ---: | ---: | ---: | :---: | ---: |
| [1.1](https://www.ohjelmointiputka.net/slh/1.1/) | 10 | 10 | 10 | ja | 0.029 |
| [1.2](https://www.ohjelmointiputka.net/slh/1.2/) | 14 | 14 | 14 | ja | 0.025 |
| [1.3](https://www.ohjelmointiputka.net/slh/1.3/) | 7 | 7 | 7 | ja | 0.044 |
| [1.4](https://www.ohjelmointiputka.net/slh/1.4/) | 15 | 15 | 15 | ja | 0.096 |
| [1.5](https://www.ohjelmointiputka.net/slh/1.5/) | 19 | 15 | 15 | ja | 0.083 |
| [1.6](https://www.ohjelmointiputka.net/slh/1.6/) | 7 | 7 | 7 | ja | 0.064 |
| [1.7](https://www.ohjelmointiputka.net/slh/1.7/) | 15 | 13 | 13 | ja | 0.021 |
| [1.8](https://www.ohjelmointiputka.net/slh/1.8/) | 38 | 19 | 19 | ja | 0.024 |
| [1.9](https://www.ohjelmointiputka.net/slh/1.9/) | 33 | 16 | 16 | ja | 0.639 |
| [2.1](https://www.ohjelmointiputka.net/slh/2.1/) | 91 | 17 | 17 | ja | 0.032 |
| [2.2](https://www.ohjelmointiputka.net/slh/2.2/) | 64 | 13 | 13 | ja | 0.030 |
| [2.3](https://www.ohjelmointiputka.net/slh/2.3/) | 28 | 21 | 21 | ja | 0.040 |
| [2.4](https://www.ohjelmointiputka.net/slh/2.4/) | 16 | 16 | 16 | ja | 0.042 |
| [2.5](https://www.ohjelmointiputka.net/slh/2.5/) | 55 | 45 | 45 | ja | 19.333 |
| [2.6](https://www.ohjelmointiputka.net/slh/2.6/) | 71 | 43 | 43 | ja | 0.030 |
| [2.7](https://www.ohjelmointiputka.net/slh/2.7/) | 89 | 89 | 89 | ja | 0.028 |
| [2.8](https://www.ohjelmointiputka.net/slh/2.8/) | 56 | 37 | 37 | ja | 1.030 |
| [2.9](https://www.ohjelmointiputka.net/slh/2.9/) | 151 | 75 | 71 | offen | 180.035 |
| [3.1](https://www.ohjelmointiputka.net/slh/3.1/) | 14 | 14 | 14 | ja | 0.049 |
| [3.2](https://www.ohjelmointiputka.net/slh/3.2/) | 13 | 13 | 13 | ja | 0.019 |
| [3.3](https://www.ohjelmointiputka.net/slh/3.3/) | 30 | 17 | 17 | ja | 0.278 |
| [3.4](https://www.ohjelmointiputka.net/slh/3.4/) | 26 | 25 | 25 | ja | 0.073 |
| [3.5](https://www.ohjelmointiputka.net/slh/3.5/) | 60 | 50 | 50 | ja | 0.319 |
| [3.6](https://www.ohjelmointiputka.net/slh/3.6/) | 37 | 18 | 18 | ja | 0.021 |
| [3.7](https://www.ohjelmointiputka.net/slh/3.7/) | 87 | 9 | 9 | ja | 0.040 |
| [3.8](https://www.ohjelmointiputka.net/slh/3.8/) | 22 | 21 | 21 | ja | 0.044 |
| [3.9](https://www.ohjelmointiputka.net/slh/3.9/) | 19 | 13 | 13 | ja | 0.075 |
| [4.1](https://www.ohjelmointiputka.net/slh/4.1/) | 333 | 37 | 37 | ja | 0.020 |
| [4.2](https://www.ohjelmointiputka.net/slh/4.2/) | 84 | 19 | 19 | ja | 0.036 |
| [4.3](https://www.ohjelmointiputka.net/slh/4.3/) | 83 | 25 | 25 | ja | 0.036 |
| [4.4](https://www.ohjelmointiputka.net/slh/4.4/) | 29 | 29 | 29 | ja | 0.022 |
| [4.5](https://www.ohjelmointiputka.net/slh/4.5/) | 44 | 44 | 44 | ja | 0.020 |
| [4.6](https://www.ohjelmointiputka.net/slh/4.6/) | 70 | 49 | 49 | ja | 0.700 |
| [4.7](https://www.ohjelmointiputka.net/slh/4.7/) | 49 | 49 | 49 | ja | 0.018 |
| [4.8](https://www.ohjelmointiputka.net/slh/4.8/) | 56 | 18 | 18 | ja | 0.043 |
| [4.9](https://www.ohjelmointiputka.net/slh/4.9/) | 140 | 49 | 49 | ja | 0.700 |
| [5.1](https://www.ohjelmointiputka.net/slh/5.1/) | 89 | 69 | 69 | ja | 23.386 |
| [5.2](https://www.ohjelmointiputka.net/slh/5.2/) | 159 | 126 | 126 | ja | 23.756 |
| [5.3](https://www.ohjelmointiputka.net/slh/5.3/) | 38 | 31 | 31 | ja | 8.404 |
| [5.4](https://www.ohjelmointiputka.net/slh/5.4/) | 42 | 42 | 42 | ja | 0.019 |
| [5.5](https://www.ohjelmointiputka.net/slh/5.5/) | 239 | 49 | 49 | ja | 0.060 |
| [5.6](https://www.ohjelmointiputka.net/slh/5.6/) | 92 | 51 | 51 | ja | 0.128 |
| [5.7](https://www.ohjelmointiputka.net/slh/5.7/) | 87 | 63 | 63 | ja | 0.975 |
| [5.8](https://www.ohjelmointiputka.net/slh/5.8/) | 52 | 25 | 24 | offen | 180.088 |
| [5.9](https://www.ohjelmointiputka.net/slh/5.9/) | 108 | 108 | 108 | ja | 0.024 |

## Offene Fälle

- **2.9 – Unäre Primzahllängen kleiner als 100:** 75 Zeichen, veröffentlichter Rekord 71, Abstand 4.
- **5.8 – Ein bestimmtes Teilwort kommt nirgends vor:** 25 Zeichen, veröffentlichter Rekord 24, Abstand 1.

In früheren vollständigen und separaten Versuchen erreichte die allgemeine Längenmengensuche bei 2.9 bereits 72 Zeichen. Der oben festgehaltene abschließende Lauf kam auf 75. Unter anderem verändert die Rechnerauslastung, wie viele Kandidaten die begrenzten Suchphasen untersuchen können; der Rekord von 71 wurde in keinem Versuch erreicht.

Die vollständigen Ausdrücke, Suchstatistiken und Einstellungen stehen in `examples/benchmarks-sl-v5.json`. Zusätzliche interne Experimente mit anderen Startdarstellungen zählen nicht als Erfolge dieses vollständigen Durchlaufs.

## Reproduktion

```bash
bash build.sh
python3 sl_benchmark.py --seconds 180 --workers 4 --stop-at-record --output vergleich.json
# Feste Zeit ohne externes vorzeitiges Beenden:
python3 sl_benchmark.py --seconds 30 --workers 2 --output vergleich-30s.json
```

Die historischen Version-4-Messungen verwendeten drei Sekunden und nur 43 Aufgaben. Ihre Erfolgszahl ist deshalb kein Vergleich bei identischem Zeitbudget.
