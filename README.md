# Regex Shortener 7.0

Eigenständiger C++17-Optimierer für reguläre Ausdrücke über den Ziffern 0 bis 9. Er sucht kurze äquivalente Darstellungen und prüft jede ausgegebene Kürzung exakt mit Automaten. Es werden keine externen C++-Bibliotheken benötigt.

Der vollständige kalte SL-haaste-Test erreicht **45 von 45 veröffentlichten Rekordlängen**, ohne Katalog. Alle 45 Ausgaben werden exakt gegen die unabhängig modellierten Aufgabenautomaten geprüft. 14 Eingaben waren bereits rekordkurz. Aufgabe 2.9 erreicht jetzt 71 Zeichen; im vollständigen Vergleich nach rund 5,3 Sekunden. Das sind Messwerte auf diesem Rechner, keine Zeit- oder Ergebnisgarantie für jede Eingabedarstellung. Details: `SL-VERGLEICH-v7.md`.

## Start und längere Suche

```bash
bash build.sh
./regex-shortener --file eingabe.txt --seconds 120
./regex-shortener --file eingabe.txt --seconds 300 --jobs 2 --checkpoint beste.regex
```

`eingabe.txt` enthält nur den Ausdruck. Alternativ kann er als einzelnes, mit einfachen Anführungszeichen geschütztes Argument oder über stdin eingegeben werden. Fortschritt erscheint auf stderr, das Ergebnis auf stdout. Mit Strg+C wird das beste bereits bestätigte Ergebnis ausgegeben.

```bash
./regex-shortener '5|[1-9][0-9]*[05]' --seconds 15
./regex-shortener --equiv '0*(10*)*' '[01]*'
./regex-shortener --match '(01)*' '0101'
```

`--json` liefert zusätzliche Statistiken. Die parallelen Suchläufe tauschen bestätigte Verbesserungen aus; der Fortschritt zeigt das beste Ergebnis aller Jobs. Die anfängliche deterministische Längengrammatik läuft nur im ersten Job, damit nicht alle Jobs dieselbe große Bank erneut aufbauen. Mehr Jobs teilen sich CPU, Cache und Speicher. Auf einem Laptop sind zunächst ein oder zwei Jobs sinnvoll; vier Jobs müssen nicht schneller sein. Ein Zeitbudget begrenzt verstrichene Zeit, nicht die Zahl ausgeführter Kandidaten.

## Über Nacht nach neuen Rekorden suchen

```bash
./regex-shortener --file eingabe.txt --seconds 28800 --jobs 2 \
  --checkpoint beste.regex > ergebnis.regex 2> suche.log
```

Das sucht acht Stunden. `cat beste.regex` zeigt den laufend gespeicherten besten Ausdruck; `tail -f suche.log` zeigt Verbesserungen. Das Protokoll meldet neue Ergebnisse; längere Suchphasen können ohne zusätzliche Ausgabe laufen. Mit Strg+C wird das beste bestätigte Ergebnis ordentlich ausgegeben. `--checkpoint` schreibt zunächst die gültige Eingabe und ersetzt die Datei danach atomar nur durch erneut vollständig geprüfte Verbesserungen. Auch die abschließende Ausgabe wird gespeichert. Elternverzeichnisse müssen existieren. Bei Speicherausfällen läuft die Suche weiter und meldet den Fehler auf stderr.

Ein neuer Lauf kann den Ausdruck erneut verwenden:

```bash
./regex-shortener --file beste.regex --checkpoint beste.regex \
  --seconds 28800 --jobs 2 --seed 2
```

Der Checkpoint enthält den Ausdruck, nicht den internen Suchzustand. Der neue Seed verändert die zufälligen Suchpfade; bei parallelen Läufen beeinflusst auch die zeitliche Reihenfolge den Austausch. Bei endlichen unären Sprachen werden zusätzlich die Grammatikgrenzen schrittweise erhöht und weitere Polynommutationen mit neuen Starts ausgeführt. Die vollständige Grammatikbank bleibt aus Speichergründen begrenzt.

Das Programm kann als Werkzeug für Rekordsuche nützlich sein. Es kann bekannte Lösungen verbessern und jeden tatsächlichen Treffer exakt bestätigen. Eine längere Laufzeit ist aber keine erschöpfende Suche über alle Ausdrücke und garantiert keine weitere Verbesserung. Acht Stunden ohne Treffer beweisen insbesondere keine Minimalität.

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
- **Grammatik endlicher Längenpolynome:** Kurze Ausdrücke werden mit ihren tatsächlichen Syntaxkosten aus Union, Addition der Längenmengen und beschränkter Wiederholung aufgebaut. Nur Mengen, die sich in irgendeinen Konkatenationskontext der Zielsprache verschieben lassen, werden behalten. Unterschiedliche Teilausdrücke dürfen im Gesamtergebnis überlappen. Größere Produkte entstehen aus synthetisierten Faktoren und ihren kompatiblen Quotienten; komplementäre Teilmengen und gewichtete Überdeckungen bilden den vollständigen Ausdruck. Es gibt weder eine Liste von Primzahlen noch eine gespeicherte 71-Zeichen-Lösung im Suchcode. Die Bank ist eine Heuristik und wird nie als Optimalitätsbeweis ausgegeben.
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

Vollständige Messungen, Budgets und verbleibende Abstände stehen in `examples/benchmarks-sl-v7.json` und `SL-VERGLEICH-v7.md`. Die öffentlich veröffentlichten Rekordlängen sind Vergleichswerte und keine Minimalitätsbeweise.

## Syntax

Der Ausdruck beschreibt die gesamte Zeichenkette. Erlaubt sind Ziffern und `| () * + ? {a} {a,b} {a,} [145] [2-46-9]` mit üblicher Bedeutung. Leere Eingabe, Gruppen und Alternativen bezeichnen die leere Zeichenkette. Verschachtelte Quantoren brauchen Gruppen: `(0{2})*` ist erlaubt, `0{2}*` nicht. Anker, Punkte, negierte Klassen, Lookarounds und Rückverweise gehören nicht zur Syntax.

Eingaben bis 10.000 Zeichen und 256 verschachtelten Gruppen werden syntaktisch verarbeitet. Übergroße Zahlen werden nicht stillschweigend abgeschnitten; die Automatenlimits entscheiden, ob eine Optimierung möglich ist. `--max-states` und `--max-nfa` steuern die Ressourcenlimits. Ein fehlgeschlagener Beweis führt zur unveränderten Eingabe.

## Prüfen

```bash
bash test.sh
RX_FUZZ_CASES=1000 python3 tests/test.py
```

Die Tests enthalten unabhängige Wortprüfungen mit Python, Vergleiche mit nicht minimierten Automaten und separate Prüfungen der neuen Suchverfahren. `--synth N` steuert nur die ältere aufzählende Grammatiksuche; die kontextabhängigen und semantischen Suchinseln haben eigene begrenzte Phasen innerhalb des gemeinsamen Zeitbudgets.

Unter Ubuntu werden GCC oder Clang mit C++17, pthreads und für Tests Python 3 benötigt. Die neue Längengrammatik verwendet den von diesen Compilern unterstützten 128-Bit-Ganzzahltyp. Der eigentliche Sucher benötigt weder Python noch SciPy oder einen externen Solver. Ein optionales CMake-Projekt ist ebenfalls enthalten. Die Aktualisierung im bestehenden Projektverzeichnis ist in `UPDATE.md` beschrieben.
