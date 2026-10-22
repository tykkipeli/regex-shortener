# Regex Shortener 6.0

Eigenständiger C++17-Optimierer für reguläre Ausdrücke über den Ziffern 0 bis 9. Er sucht kurze äquivalente Darstellungen und prüft jede ausgegebene Kürzung exakt mit Automaten. Es werden keine externen C++-Bibliotheken benötigt.

Der vollständige SL-haaste-Test erreicht **44 von 45 veröffentlichten Rekordlängen**, mit bis zu 180 Sekunden pro Aufgabe und ohne Katalog. 14 Eingaben waren bereits rekordkurz. Aufgabe 2.9 bleibt mit 72 statt 71 Zeichen offen; die Messdaten und Einschränkungen stehen in `SL-VERGLEICH-v6.md`.

## Start und längere Suche

```bash
bash build.sh
./regex-shortener --file eingabe.txt --seconds 120
./regex-shortener --file eingabe.txt --seconds 300 --jobs 4
```

`eingabe.txt` enthält nur den Ausdruck. Alternativ kann er als einzelnes, mit einfachen Anführungszeichen geschütztes Argument oder über stdin eingegeben werden. Fortschritt erscheint auf stderr, das Ergebnis auf stdout. Mit Strg+C wird das beste bereits bestätigte Ergebnis ausgegeben.

```bash
./regex-shortener '5|[1-9][0-9]*[05]' --seconds 15
./regex-shortener --equiv '0*(10*)*' '[01]*'
./regex-shortener --match '(01)*' '0101'
```

`--json` liefert zusätzliche Statistiken. Ein längeres Suchbudget und mehrere unabhängige Suchläufe erhöhen die Chancen; die Heuristik garantiert damit kein globales Optimum.

## Bewiesen kürzeste Ergebnisse

Nach der Suche prüft das Programm zusätzlich, ob sich die Mindestlänge beweisen lässt. Es verwendet allgemeine Untergrenzen und eine vollständige Suche in einer endlichen Beobachtungsalgebra. Klammern, Zeichenklassen und sämtliche erlaubten Quantoren werden mit ihren wirklichen Syntaxkosten berücksichtigt. Ein Rekordtreffer oder eine erfolglose Heuristik liefert keinen Optimalitätsbeweis.

```bash
./regex-shortener '[01]*' --prove-only
./regex-shortener '0{97}' --prove-only --json
./regex-shortener --file eingabe.txt --seconds 120 --prove-seconds 3 --prove-length 10
```

Standard: zusätzlich höchstens 0,25 Sekunden, Syntaxgrenze 8. `--prove-seconds 0` deaktiviert die Prüfung. Der Status erscheint auf stderr; stdout bleibt der reine Ausdruck. JSON enthält `optimal` und ein eigenes `optimality`-Objekt. Ein Zeit-, Speicher- oder Beobachtungslimit ergibt **nicht bewiesen**. Der vollständige Beweisweg und seine Grenzen stehen in `OPTIMALITAET.md`.

## Allgemeine Suchverfahren

- **Endliche Sprachen als Relationen:** Jede Trennstelle eines Wortes liefert eine Präfix-Suffix-Relation. Rechtecküberdeckungen dieser Relation ergeben konkatenierte Teilsprachen. Überlappende gewichtete Überdeckungen können beide Seiten gleichzeitig teilen. Bis zu 128 erforderliche Wörter werden mit zwei portablen 64-Bit-Masken verarbeitet. Größere erlaubte endliche Mengen können in einem bewiesenen Alternativkontext zusätzliche Wörter liefern.
- **Kontextuelle Schleifen:** Kurze Bausteine werden zu Schleifen mit optionalen Eingangs- und Ausgangsblöcken kombiniert. Übergangsrelationen des gegebenen DFA und unabhängig erzeugte Präfix-/Suffixkontexte filtern Kandidaten; ein frischer vollständiger Sprachvergleich entscheidet über die Ausgabe. Es werden weder ein verbotenes Wort noch ein erwarteter Lösungsausdruck vorgegeben.
- **Endliche Längenmengen:** Additive Hankel-Relationen, abgeschlossene Rechtecke, Summenmengen, Verschiebungen und überlappende Überdeckungen erzeugen arithmetische Faktorisierungen. Auch unregelmäßige kleinere Teilrechtecke können günstiger sein als ihre maximale Überdeckung. Verbesserungen tiefer Teilprobleme werden sofort wieder in den gesamten Ausdruck eingesetzt. Eine zusätzliche Mutationssuche verwendet boolesche Längenpolynome mit Überlaufkontrolle und lässt vorübergehend falsche Mengen zu; veröffentlichte Ergebnisse benötigen weiterhin den vollständigen Automatenbeweis.
- **Fortsetzungsprofile:** Der Automat liefert Zustände, ab denen alle ausreichend langen Fortsetzungen erlaubt sind. Daraus entstehen kurze Teilsprache-Kandidaten. Der exakt berechnete endliche Rest wird separat verdichtet und mit dem Kandidaten vereinigt.
- **Makroaktionen und Sternbasen:** Aus beliebigen Unterausdrücken und Fenstern werden Übergangsrelationen berechnet. Schleifen mit Identitätswirkung, gleiche Aktionen und aus dem Automaten hergeleitete Wiederholungszahlen liefern neue Generatorbasen. Sie werden auch für Stern-Teilsprachen eines größeren Ausdrucks gesucht.
- **Mehrere Automatenmodelle:** Neben DFA und umgekehrtem DFA werden partielle Ableitungs-NFAs und aus Sprachinklusionen gewonnene Residual-NFAs untersucht. Ein Suchstrahl hält ganze partielle Eliminationsgraphen und normalisiert ihre Kanten bereits während der Elimination.
- **Kontextabhängige Superoptimierung:** Änderungen an Quantoren, optionalen Gruppen, Generatoren und zyklischen Blöcken dürfen die Teilsprache zunächst verändern. Nur der vollständige Kontext entscheidet durch eine exakte Prüfung, ob die Änderung verwendbar ist.
- **Semantische Suchinseln:** Mutationen dürfen auch vorübergehend falsche Ausdrücke erzeugen. DFA-basierte Stichproben und kürzeste Gegenbeispiele steuern die Suche; ein optionales Pareto-Archiv erhält verschiedene Kompromisse zwischen Fehlerzahl und Länge. Stichproben ersetzen niemals den Sprachbeweis.
- **Mehrere gleichwertige Darstellungen:** Ein einziges lexikographisches Normalformular kann einen nützlichen Suchraum verdecken. Deshalb bleiben verschiedene korrekte Ausdrucksformen sowie frühere kürzeste Darstellungen als Ausgangspunkte erhalten. Die Verfahren tauschen frisch geprüfte kurze Teilsprachen aus.

Diese Verfahren erhalten weder eine Aufgabennummer noch eine Rekordlänge oder eine Referenzlösung. Die unabhängigen Testprogramme enthalten außerdem zufällig erzeugte Automaten und Sprachen, Ziffernpermutationen, Umkehrungen, unendliche Kontexte und bewusst falsche Startkandidaten.

## Kein automatischer Lösungskatalog

**Standardmäßig wird kein Katalog gelesen oder geschrieben.** Auch eine Datenbank aus einer älteren Version bleibt bei einem normalen Aufruf unbenutzt. Die frühere automatische Übernahme von sechs mitgelieferten Vergleichslösungen wurde entfernt; `catalog/seed.regex` ist absichtlich leer und wird nicht mehr geladen.

Ein eigener Lernkatalog bleibt ausdrücklich aktivierbar:

```bash
./regex-shortener --db meine-ergebnisse.db --file eingabe.txt --seconds 120
./regex-shortener --db meine-ergebnisse.db --learn '1*0[01]*'
./regex-shortener --no-catalog --file eingabe.txt --seconds 120
```

`--learn` ohne `--db` verwendet eine Datenbank neben dem Programm. Die Einträge sind beste bekannte Darstellungen, keine allgemein bewiesenen Optima. Jeder Treffer wird vor seiner Verwendung erneut geprüft. Die Update-ZIP enthält keine persönliche Datenbank.

## SL-haaste als unabhängiger Maßstab

`sl_benchmark.py` modelliert alle 45 Aufgaben als vollständige Automaten über 0 bis 9. `examples/sl-inputs.json` enthält eingefrorene, zuvor durch eine feste Automatenelimination erzeugte Eingaben. Der Suchprozess erhält ausschließlich den Ausdruck und allgemeine Suchparameter. Die Rekordlänge gelangt nicht in den Optimierer.

```bash
python3 sl_benchmark.py --seconds 30 --workers 2 --output vergleich.json
python3 sl_benchmark.py --seconds 180 --workers 4 --stop-at-record --output vergleich.json
```

`--stop-at-record` ist eine Option des **Testprogramms**: Es beobachtet die reguläre Fortschrittsausgabe und sendet Strg+C, sobald die öffentliche Vergleichslänge erreicht ist. Es verändert weder Suchregeln noch Kandidaten. Das spart bei einfachen Aufgaben die übrige Wartezeit. Ohne diese Option läuft jeder Fall sein volles Suchbudget. Bereits kurze Eingaben werden in der Auswertung gesondert ausgewiesen.

Jedes Ergebnis wird zusätzlich exakt gegen den unabhängig konstruierten Aufgabenautomaten geprüft. Das belegt die Übereinstimmung mit dem Aufgabenmodell; es ist kein formaler Beweis der Übersetzung der finnischen Beschreibung. Bei 2.5 und 2.6 können sich Anfangs- und Endpaar entsprechend der üblichen Bedeutung bei kurzen Wörtern überlappen.

Vollständige Messungen, Budgets und verbleibende Abstände stehen in `examples/benchmarks-sl-v6.json` und `SL-VERGLEICH-v6.md`. Die öffentlich veröffentlichten Rekordlängen sind Vergleichswerte und keine Minimalitätsbeweise.

## Syntax

Der Ausdruck beschreibt die gesamte Zeichenkette. Erlaubt sind Ziffern und `| () * + ? {a} {a,b} {a,} [145] [2-46-9]` mit üblicher Bedeutung. Leere Eingabe, Gruppen und Alternativen bezeichnen die leere Zeichenkette. Verschachtelte Quantoren brauchen Gruppen: `(0{2})*` ist erlaubt, `0{2}*` nicht. Anker, Punkte, negierte Klassen, Lookarounds und Rückverweise gehören nicht zur Syntax.

Eingaben bis 10.000 Zeichen und 256 verschachtelten Gruppen werden syntaktisch verarbeitet. Übergroße Zahlen werden nicht stillschweigend abgeschnitten; die Automatenlimits entscheiden, ob eine Optimierung möglich ist. `--max-states` und `--max-nfa` steuern die Ressourcenlimits. Ein fehlgeschlagener Beweis führt zur unveränderten Eingabe.

## Prüfen

```bash
bash test.sh
RX_FUZZ_CASES=1000 python3 tests/test.py
```

Die Tests enthalten unabhängige Wortprüfungen mit Python, Vergleiche mit nicht minimierten Automaten und separate Prüfungen der neuen Suchverfahren. `--synth N` steuert nur die ältere aufzählende Grammatiksuche; die kontextabhängigen und semantischen Suchinseln haben eigene begrenzte Phasen innerhalb des gemeinsamen Zeitbudgets.

Unter Ubuntu werden ein C++17-Compiler, pthreads und für Tests Python 3 benötigt. Ein optionales CMake-Projekt ist ebenfalls enthalten. Die Aktualisierung im bestehenden Projektverzeichnis ist in `UPDATE.md` beschrieben.
