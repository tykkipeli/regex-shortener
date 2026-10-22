# Was „bewiesen kürzest“ bedeutet

Die Länge ist die Anzahl der Zeichen des ausgegebenen SL-haaste-Ausdrucks. Gezählt werden auch Klammern, Trennzeichen und Ziffern in Wiederholungszahlen. Verglichen wird mit allen Ausdrücken der unterstützten positiven SL-haaste-Syntax über 0 bis 9, nicht nur mit den Umformungen oder Vorlagen des heuristischen Suchers. Die leere Eingabe bezeichnet die leere Zeichenkette und hat Länge null.

Die Prüfung braucht keine Rekorddatenbank. Ein veröffentlichter Rekord ist für sich genommen kein Optimalitätsbeweis.

```bash
./regex-shortener '[01]*' --prove-only
./regex-shortener '0{97}' --prove-only --json
./regex-shortener --file eingabe.txt --seconds 60 --prove-seconds 3 --prove-length 10
```

Standardmäßig folgt auf die heuristische Suche eine zusätzliche Prüfung mit höchstens 0,25 Sekunden und einer Syntaxgrenze von 8. `--prove-seconds 0` schaltet sie ab. `--prove-only` überspringt die heuristische Suche. Die Prüfung kann dabei selbst einen kürzeren Ausdruck finden.

Die Syntaxgrenze N umfasst sämtliche Kandidaten mit weniger als N Zeichen. Für ein Ergebnis der Länge N genügt das zum Beweis; für längere Ergebnisse kann daraus zunächst nur eine Untergrenze entstehen.

Der normale Ausgabekanal enthält weiterhin ausschließlich den Ausdruck. Der Beweisstatus erscheint auf stderr. Mit `--json` stehen `optimal` sowie `optimality.proven`, `lower_bound`, `method` und `reason` maschinenlesbar zur Verfügung. `optimal: false` bedeutet **nicht bewiesen**, nicht „nachweislich nicht optimal“. Strg+C beendet auch die Beweissuche; die bereits bestätigte Ausgabe bleibt erhalten.

## Zwei Beweiswege

### Alphabet und Unendlichkeit

Der vollständige Automat liefert das produktive Alphabet: diejenigen Ziffern, die tatsächlich in einem akzeptierten Wort vorkommen. Jede dieser Ziffern muss durch mindestens eine aktive Literal- oder Zeichenklassenvorkommnis beigetragen werden. Eine vollständige Überdeckungsrechnung über höchstens zehn Ziffern berechnet eine Untergrenze für die dafür nötigen Syntaxzeichen, wobei alle anderen Zeichen kostenlos sein dürfen. Eine unendliche Sprache erfordert zusätzlich einen unbeschränkten Quantor mit mindestens einem weiteren Zeichen. Diese Relaxation kann die wahre Mindestlänge nur unterschätzen.

So ist beispielsweise `0*` mit zwei Zeichen und `[0-9]*` mit sechs Zeichen sofort bewiesen kürzest.

### Vollständige Suche in einer endlichen Beobachtungsalgebra

1. Es wird eine endliche Menge W von Wörtern gewählt, die die leere Zeichenkette und sämtliche Teilwörter jedes enthaltenen Wortes umfasst. Die Beobachtung eines Ausdrucks ist die Menge seiner auf W akzeptierten Wörter.
2. Vereinigung, Verkettung und Wiederholung lassen sich auf diesen Beobachtungen exakt berechnen. Für eine Verkettung werden alle Zerlegungen eines beobachteten Wortes verwendet; die benötigten Präfixe und Suffixe liegen wegen der Teilwortabgeschlossenheit ebenfalls in W. Gleiche Beobachtungen bleiben daher in jedem Ausdruckskontext gleich.
3. Eine gewichtete vollständige Grammatiksuche untersucht alle Beobachtungen kürzerer Syntax. Sie unterscheidet vier syntaktische Rollen: Alternative, Verkettung, quantifizierter Faktor und Atom. Damit gehen die Kosten erforderlicher Klammern und das Verbot unmittelbar gestapelter Quantoren nicht verloren. Zwei Beobachtungen dürfen nur innerhalb derselben Rolle zusammengefasst werden.
4. Alle nichtleeren Zeichenklassen des produktiven Alphabets werden berücksichtigt. Sortierung und Bereichsschreibweise liefern eine kürzeste Klassenschreibweise; Alternativen aus Literalen bleiben ebenfalls verfügbar. Wirkungslose Teilbäume unter Wiederholung null können ohne Längenzuwachs entfernt werden. Eine echte kürzeste Darstellung benötigt deshalb keine Ziffer außerhalb des produktiven Alphabets.
5. Wiederholungszahlen werden nicht willkürlich beschränkt: Ist M die größte Wortlänge in W, dann haben alle Potenzen oberhalb M entweder leere Beobachtung (ohne leeres Wort im Operanden) oder bereits eine stabile Beobachtung (mit leerem Wort). Beliebig große Zahlen dürfen deshalb für diese Abstraktion auf M+1 abgebildet werden. Die Dezimalschreibweise wird dabei nicht länger. Auch sehr große hypothetische kürzere Zählausdrücke sind damit abgedeckt.
6. Hat **kein** kürzerer Ausdruck dieselbe Beobachtung wie die Zielsprache, kann erst recht kein kürzerer Ausdruck auf allen Wörtern äquivalent sein. Die vorhandene, vollständig geprüfte Darstellung liefert zugleich die obere Schranke. Stimmen beide Grenzen überein, ist ihre Länge global minimal.

Ein passender Beobachtungswert allein ist dagegen kein Äquivalenzbeweis. Der Kandidat wird neu geparst und mit vollständigen Automaten im Produkt verglichen. Ein Gegenbeispiel erweitert W samt seinen Teilwörtern; anschließend beginnt die vollständige Suche erneut. Ein tatsächlich äquivalenter Kandidat an der bereits bewiesenen Untergrenze liefert selbst eine kürzeste Darstellung.

## Grenzen

Die vollständige Suche kann stark wachsen. Zeit, maximal untersuchte Syntaxlänge, höchstens 63 Beobachtungswörter und das Profil-Speicherlimit begrenzen den Aufwand. Das Erreichen einer Grenze liefert niemals „bewiesen kürzest“. Eine bereits fertig bewiesene Untergrenze bleibt als solche verfügbar; sie reicht gegebenenfalls noch nicht bis zur Ergebnislänge.

Eine lange Ausgabe kann trotzdem leicht beweisbar sein, wenn schon eine der allgemeinen Untergrenzen ihre Länge erreicht. Für viele längere strukturierte Sprachen reicht die aktuelle Abstraktion innerhalb des Budgets dagegen nicht aus. Die exakte Äquivalenzprüfung und die Optimalitätsprüfung werden deshalb getrennt ausgewiesen.

Die Beweisimplementierung wird durch eine unabhängige rohe Syntaxaufzählung, Python-Wortprüfungen, Zufallsausdrücke und getrennte Überprüfungen der Beobachtungsoperationen getestet. Reproduktion: `bash test.sh`.
