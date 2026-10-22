# Regex Shortener 3.0 für SL-haaste

Ein eigenständiges C++17-Programm, das einen regulären Ausdruck erhält und einen möglichst kurzen äquivalenten Ausdruck sucht. Es benötigt keine externen C++-Bibliotheken. Jeder tatsächlich ausgegebene kürzere Ausdruck besteht eine exakte Automatenprüfung über **alle Wörter**, nicht bloß über eine Sammlung von Testwörtern.

## Neu in Version 3.0

Die neue Version kombiniert zusätzliche Suchverfahren mit einem persistenten Sprachkatalog:

- **Längenarithmetik:** Für einbuchstabige Sprachen erkennt sie Vorperiode und Periode des minimalen Automaten. Endliche Längenmengen werden durch arithmetische Folgen und beschränkte Summen kleiner Generatoren verdichtet. Für periodische Mengen sucht sie nach modularen Gittern und ergänzt die fehlenden kurzen Wörter.
- **Endliche Ausnahmen:** Der Automat liefert eine Länge, ab der alle Wörter über seinem tatsächlich vorkommenden Alphabet akzeptiert werden. Die Suche kombiniert diese Schranke mit nachweislich erlaubten Klassensternen und prüft die vollständige Überdeckung.
- **Nachbarschaftsstruktur:** Sprachen, in denen unmittelbar aufeinanderfolgende Zeichen verschieden sein müssen, werden am Automaten erkannt und über Alphabetzerlegungen kompakter aufgebaut. Das Verfahren hängt nicht von bestimmten Ziffern ab.
- **Schleifen und Blöcke:** Zusätzliche allgemeine Stern-Identitäten, das Ergänzen bereits verfügbarer Schleifen und eine Suche über Präfix- und Suffix-Untergruppen legen wiederholte Blöcke frei. Verkettungen bleiben beim Zusammenfassen flach, damit solche Gruppen sichtbar bleiben.
- **Leere Alternativen:** Der Drucker verwendet jetzt bei Bedarf Formen wie `(|X){n}`. Sie können kürzer sein als die entsprechende Darstellung mit Optionalität und verschachtelten Quantoren.

Bei fünf der sechs eingereichten großen Beispiele erreicht die Suche die angegebenen Vergleichslängen auch mit `--no-catalog`. Das zweite Beispiel erreicht die geprüften 126 Zeichen über den mitgelieferten Katalog. Die vollständigen Messungen mit und ohne Katalog stehen unter `examples/benchmarks-v3.json`.

### Der Sprachkatalog

`regex-shortener.db` wird standardmäßig neben der ausführbaren Datei angelegt. Der Katalog ordnet Sprachen anhand ihrer kanonischen minimalen Automaten zu. Damit erkennt er auch syntaktisch völlig verschiedene Ausdrücke als gleich. Pro Sprache behält er die kürzeste bisher gespeicherte Darstellung.

Nach einem Lauf speichert er das Ergebnis und bis zu 2.000 kurze Teilsprachen je Suchlauf. Für diese automatischen Einträge gelten 48 Zeichen und 48 DFA-Zustände als Grenzen. Insgesamt ist der Katalog auf 20.000 Sprachen begrenzt. Bei späteren Läufen werden auch zusammenhängende Ausschnitte aus bis zu acht Faktoren einer Verkettung untersucht; sie müssen nicht als eigener Syntaxbaum vorhanden sein.

**Die Einträge sind beste bekannte Darstellungen, im Allgemeinen keine bewiesenen globalen Optima.** Eine vollständige Datenbank aller kürzesten Ausdrücke würde sehr schnell zu groß. Auch ein gelernter oder nachträglich geänderter Datenbankeintrag wird vor seiner Verwendung erneut auf Äquivalenz geprüft. Die unabhängige Abschlussprüfung bleibt aktiv.

Die Datei `catalog/seed.regex` enthält ausdrücklich die sechs vom Nutzer gelieferten und auf Äquivalenz geprüften Vergleichsausdrücke. Ihre direkte Wiedererkennung wird in den Messungen getrennt von der Suche ohne Katalog ausgewiesen.

```bash
# Automatisch lernen und bei späteren Läufen wiederverwenden:
./regex-shortener --file eingabe.txt --seconds 300 --jobs 4

# Eine selbst gefundene gute Lösung aufnehmen:
./regex-shortener --learn '1*0[01]*'

# Einen anderen Katalog benutzen:
./regex-shortener --db mein-katalog.db --file eingabe.txt --seconds 60

# Die Suchverfahren ohne Katalog testen; liest und schreibt keinen Katalog:
./regex-shortener --no-catalog --file eingabe.txt --seconds 60
```

Katalogdateien werden bei parallelen Prozessen gesperrt, vor dem Schreiben zusammengeführt und atomar ersetzt. Falls das Speichern fehlschlägt, bleibt das geprüfte Suchergebnis erhalten. Die neue ZIP enthält keine persönliche `.db`-Datei; ein vorhandener Katalog bleibt beim Aktualisieren erhalten.

## Neu in Version 2.0

Die Suche kann Zeichenklassen gezielt wieder zerlegen, obwohl sie beim Einlesen zusammengefasst werden. Sie erkennt beispielsweise `[0-9] = 0 | [1-9]` und verwendet die allgemeinen Identitäten

```text
A(A|B)* = (AB*)+
(A|B)*A = (B*A)+
```

Das funktioniert auch mit zusammengesetzten Wortblöcken und in größeren Ausdrücken. Zudem werden überflüssige Wiederholungen an den Grenzen eines Sterns vereinfacht. Die Suche über Zwischenformen erhält nun passende Klassen und deren Differenzen aus der gesamten Eingabe.

Damit gelingt das gemeldete Beispiel bereits in der deterministischen Anfangsphase, auch mit `--synth 0`:

```text
5|[1-9][0-9]*[05]  →  ([1-9]0*)+[05]|5
17 Zeichen            16 Zeichen
```

`01(01|2)*` wird entsprechend zu `(012*)+` (9 → 7 Zeichen). Die Reihenfolge der Alternativen kann von der Eingabe abweichen.

Beim Aktualisieren die Dateien über die alte Version entpacken und **erneut `bash build.sh` ausführen**, damit auch die ausführbare Datei ersetzt wird. Mit `./regex-shortener --version` lässt sich die Version prüfen.

## Start auf Ubuntu / Linux

Das ZIP entpacken, ein Terminal im entpackten Ordner öffnen und ausführen:

```bash
bash build.sh
./regex-shortener '(01|001|0001)*' --seconds 30
```

Benötigt wird ein C++17-Compiler, beispielsweise `g++`. Falls er noch fehlt:

```bash
sudo apt install g++
```

Für eine längere Suche auf einem Rechner mit mehreren Kernen:

```bash
./regex-shortener --file eingabe.txt --seconds 300 --jobs 4
```

`eingabe.txt` enthält nur den Ausdruck, ohne Anführungszeichen. Alternativ:

```bash
echo '(01|001|0001)*' | ./regex-shortener --seconds 60
```

**Strg+C** beendet die Suche und gibt das beste bereits bestätigte Ergebnis aus. Meldungen und Verbesserungen erscheinen auf `stderr`; `stdout` enthält ausschließlich das Ergebnis. Damit kann es direkt gespeichert werden:

```bash
./regex-shortener --file eingabe.txt --seconds 300 --jobs 4 > ergebnis.txt
```

Die Umleitung wird von der Shell sofort geöffnet. Für Ein- und Ausgabe verschiedene Dateinamen verwenden.

## Eingabesyntax

Der Ausdruck beschreibt die **gesamte** Zeichenkette. Alphabet sind die Ziffern `0` bis `9`.

| Syntax | Bedeutung |
| --- | --- |
| `00\|111` | Alternative |
| `(01)` | Gruppe |
| `0*` | Mindestens null Wiederholungen |
| `0+` | Mindestens eine Wiederholung |
| `0?` | Null oder eine Wiederholung |
| `0{5}` | Genau fünf Wiederholungen |
| `0{2,5}` | Zwei bis fünf Wiederholungen |
| `0{2,}` | Mindestens zwei Wiederholungen |
| `[145]` | Eine der angegebenen Ziffern |
| `[2-46-9]` | Eine Ziffer aus den angegebenen Bereichen |

Aufeinanderfolgende Quantoren brauchen Klammern: `(0{2})*` ist gültig, `0{2}*` wird zurückgewiesen. Leerräume, Anker, Punkte, negierte Klassen, Backreferences und Lookaheads gehören nicht zur Syntax.

Leere Gruppen, Alternativen und die leere Eingabe bezeichnen wie in den eingereichten SL-haaste-Beispielen die leere Zeichenkette: `()`, `0|` oder `(|00|0{7})`. Der Optimierer kann solche Formen ausgeben. Die Sprache, die nur das leere Wort enthält, kann als leere Ausgabezeile erscheinen; mit `--json` ist das Ergebnis ausdrücklich als `"regex":""` sichtbar. `0{0}` bleibt ebenfalls gültig. Klassen enthalten immer mindestens eine Ziffer.

Die SL-haaste-Länge von 500 Zeichen ist kein Optimierungsziel oder Ausgabelimit des Programms. Es verarbeitet Eingaben bis 10.000 Zeichen und 256 verschachtelten Gruppen. Beliebig lange dezimale Wiederholungszahlen werden syntaktisch akzeptiert; die Automatenlimits entscheiden, ob eine Optimierung möglich ist.

## Was die Suche macht

1. **Syntaxbaum und algebraische Kürzungen:** Unnötige Gruppen, Zeichenklassen, gemeinsame Präfixe und Suffixe, Wiederholungsintervalle, identische Teilblöcke und Kleene-Stern-Identitäten. Eine dynamische Programmierung sucht nach wiederkehrenden Blöcken unterschiedlicher Perioden. Die Kosten berücksichtigen die tatsächlich nötigen Klammern; etwa `00` ist kürzer als `0{2}`.
2. **Exakte semantische Vereinfachung:** Thompson-NFA, Potenzmengenkonstruktion und Hopcroft-Minimierung. Teilmengenprüfungen erkennen überflüssige Alternativen; Kontextprüfungen können auch Faktoren und Zweige entfernen, die erst im Zusammenhang entbehrlich werden.
3. **Suche nach kürzeren Teilsprachen:** Eine begrenzte Grammatiksuche erzeugt Ausdrücke, fasst exakt gleiche Sprachen zusammen und speichert die kürzeste gefundene Darstellung. Der Suchvorrat enthält die Teilbäume der Eingabe, relevante Zeichenklassen, Verkettungen, Alternativen und Wiederholungen.
4. **Mehrere Automatenzerlegungen:** Verallgemeinerte Zustandselimination auf dem minimalen DFA, dem umgekehrten Automaten sowie Thompson-Automaten in beiden Richtungen. Mehrere Kostenfunktionen und zufällige Eliminationsreihenfolgen erzeugen unterschiedliche Darstellungen derselben Sprache. Bei größeren Eingaben werden zusätzlich kleinere Teilautomaten separat neu aufgebaut.
5. **Suche über äquivalente Zwischenformen:** Ein begrenzter Suchstrahl behält unterschiedliche Ausdrücke. Distributivität, Sternzerlegung und zyklische Verschiebungen erlauben auch vorübergehend längere Zwischenformen. Ein gemeinsamer Vorrat kürzerer Teilsprachen überträgt Erkenntnisse zwischen den Suchwegen.
6. **Parallele Mehrfachsuche:** `--jobs N` führt unabhängige Suchen mit unterschiedlichen Zufallsfolgen aus und wählt das beste Ergebnis.
7. **Abschlussprüfung:** Der ausgegebene Text wird neu geparst. Die beiden nicht minimierten deterministischen Automaten werden als Produktautomat verglichen. Damit prüft der letzte Schritt zusätzlich die Ausgabeformatierung und benötigt die Korrektheit der Minimierung nicht als Voraussetzung.

Die Suche ist eine Heuristik: **Äquivalenz wird bewiesen, globale Kürzestheit im Allgemeinen nicht.** Mehr Zeit oder mehr Suchläufe können weitere Verbesserungen bringen. Das Ergebnis wird nie länger als die Eingabe. Bei einem Ressourcenlimit bleibt ein bestätigtes Ergebnis erhalten; kann schon die Eingabe nicht kompiliert werden, bleibt sie unverändert.

## Einstellungen

```text
--seconds N       Suchbudget pro Lauf in Sekunden; Standard 15
--jobs N          Anzahl paralleler Läufe; Standard 1
--seed N          Anfangswert der Zufallsfolge; Standard 1
--synth N         Kandidatenbudget der Sprachsuche; Standard 20000
--beam N          Zahl alternativer Ausgangsformen; Standard 12
--max-states N    Zustandslimit bei der Determinisierung; Standard 4096
--max-nfa N       Zustandslimit des Thompson-NFA; Standard 12000
--json            Ausdruck, Längen, Prüfverfahren und Suchstatistik als JSON
--quiet           Fortschrittsmeldungen unterdrücken
--file DATEI      Ausdruck aus Datei lesen
--db DATEI        Pfad des persistenten Sprachkatalogs
--no-catalog      Katalog weder lesen noch schreiben
--learn REGEX     Gute Darstellung einer Sprache aufnehmen
--version         Versionsnummer
--help            Hilfe
```

Für besonders intensive Suche, sofern Zeit und Speicher verfügbar sind:

```bash
./regex-shortener --file eingabe.txt --seconds 1800 --jobs 4 --synth 300000 --beam 24
```

`--seconds` ist ein regelmäßig geprüftes Zeitbudget. Eine einzelne laufende Umformung kann es etwas überschreiten. Das Einlesen und Speichern des Katalogs kommen als Vorbereitung bzw. Abschluss hinzu. Die abschließende unabhängige Äquivalenzprüfung hat zusätzlich bis zu 30 Sekunden. `--jobs` vervielfacht den möglichen Speicherverbrauch. Die Determinisierung kann exponentiell wachsen; `--max-states` und `--max-nfa` begrenzen sie ausdrücklich. Ein erschöpftes Limit gilt niemals als Äquivalenzbeweis.

Eine feste Zufallsfolge macht die Suchreihenfolge nachvollziehbar. Bei einem Zeitlimit hängen der erreichte Suchstand und damit das Ergebnis auch von der Rechnergeschwindigkeit und Auslastung ab.

## Ausdrücke vergleichen oder Wörter prüfen

```bash
./regex-shortener --equiv '(0|1)*' '[01]*'
./regex-shortener --equiv '(00)*' '0*'
./regex-shortener --match '(01)*' '0101'
```

Bei verschiedenen Sprachen wird ein **kürzestes Gegenbeispiel** ausgegeben; `""` bedeutet die leere Zeichenkette. Alle zehn Ziffern werden berücksichtigt, auch wenn sie nur in einem der Ausdrücke auftreten.

Rückgabewerte:

- `0`: Optimierung erfolgreich; beim Vergleich äquivalent; beim Worttest akzeptiert.
- `1`: Beim Vergleich verschieden; beim Worttest abgelehnt.
- `2`: Syntaxfehler, ungültige Option oder nicht durchführbare Vergleichsprüfung.

## Tests und Beispiele

```bash
bash test.sh
RX_FUZZ_CASES=1000 bash test.sh
```

Die Tests umfassen Syntaxfehler, Grenzfälle von Wiederholungen, Verschachtelungen, unabhängige DFA-Minimalitätsprüfung, echte Kürzungen, vollständige Sprachvergleiche, Gegenbeispiele, Ressourcenlimits, Parallelbetrieb und Strg+C. Zufallstests vergleichen die Akzeptanz zusätzlich mit Pythons unabhängigem `re.fullmatch`.

Unter `examples/avoid-101010.txt` liegt ein absichtlich längerer Ausdruck für binäre Wörter ohne Teilwort `101010`. Er eignet sich zum Vergleichen verschiedener Suchbudgets:

```bash
./regex-shortener --file examples/avoid-101010.txt --seconds 60 --jobs 4
```

`examples/benchmarks.json` enthält die ursprünglichen Beispielergebnisse. `examples/benchmarks-v2.json` vergleicht Version 1 und 2 mit denselben Zeit- und Suchbudgets. Die Ergebnisse sind keine Minimalitätsbehauptung.

CMake ist ebenfalls möglich:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Quellcode

- `src/regex.hpp`: Syntaxbaum, Parser, Druckregeln und elementare Identitäten.
- `src/automata.hpp`: Thompson-Konstruktion, Determinisierung, Hopcroft-Minimierung, Umkehrung und exakter Vergleich.
- `src/optimize.hpp`: Algebra, semantischer Suchvorrat, Synthese, Zustandselimination und Suche über Zwischenformen.
- `src/catalog.hpp`: Persistenter Katalog, geprüfte Wiederverwendung und atomare Speicherung.
- `src/patterns.hpp`: Endliche Ausnahmen und Nachbarschaftssprachen.
- `src/unary.hpp`: Endliche Längenmengen und modulare Gitter.
- `src/main.cpp`: Kommandozeile, parallele Läufe, Unterbrechungen und unabhängige Abschlussprüfung.
- `tests/core.cpp`: Interne mathematische Prüfungen.
- `tests/test.py`: End-to-End- und unabhängige Zufallstests.
