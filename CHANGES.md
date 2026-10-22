# Version 6.0

- Kontextuelle Schleifensynthese aus kurzen allgemeinen Bausteinen, DFA-Übergangsrelationen und unabhängig erzeugten Präfix-/Suffixkontexten. Aufgabe 5.8 erreicht damit aus der kalten Eingabe 24 Zeichen ohne Katalog oder Referenzlösung.
- Unregelmäßige kleinere Zeilenmengen ergänzen die Überdeckung endlicher unärer Summenmengen.
- Vollständige Optimalitätsprüfung über eine teilwortabgeschlossene Beobachtungsalgebra, ergänzt durch allgemeine Alphabet- und Unendlichkeitsuntergrenzen.
- Neue Optionen `--prove-only`, `--prove-seconds` und `--prove-length`; `optimal` und `optimality` in JSON, eindeutiger Beweisstatus auf stderr.
- Jede behauptete Mindestlänge schließt kürzere Syntax vollständig aus. Ressourcenlimits ergeben ausdrücklich keinen Beweis. Eine Rekordlänge wird nie als Optimalitätsbeweis verwendet.
- Vollständiger Vergleich: 44 von 45 veröffentlichten Rekordlängen; 2.9 bleibt bei 72 statt 71 Zeichen offen.
- Neue unabhängige Tests einschließlich roher Syntaxaufzählung, Python-Wortprüfung und Sanitizern. Details in `VALIDIERUNG.md` und `OPTIMALITAET.md`.
- Neue Update-Datei `sl-haaste-regex-shortener-update-v6.zip`.

# Version 5.0

- Keine automatisch geladenen Referenzlösungen; Katalog nur auf ausdrücklichen Wunsch.
- Allgemeine endliche Wortüberdeckungen mit 128-Wort-Masken und kontextabhängigen Teilüberdeckungen.
- Additive Faktorisierung endlicher Längenmengen mit Hankel-Relationen und sofortiger Weitergabe tiefer Verbesserungen.
- Mutationssuche mit booleschen Längenpolynomen, Überlaufkontrolle und exakter Abschlussprüfung.
- Fortsetzungsprofile und exakter endlicher Rest.
- Aus Übergangsaktionen gewonnene Makrozyklen und lokale Sternbasen.
- Residual- und Ableitungs-NFAs, Suche über partielle Eliminationsgraphen.
- Kontextabhängige und semantische Mutationen mit Gegenbeispielen, Pareto-Suchinseln und äquivalenten Archiven.
- Verschiedene Suchphasen erhalten eigene fortlaufende Zufallsstarts; rohe und normalisierte gleichwertige Formen bleiben nach separatem Beweis als Startpunkte erhalten.
- Alle 45 offiziellen Aufgaben im unabhängigen Benchmark; eingefrorene Eingaben und optionale externe Erfolgsunterbrechung.
- Neue unabhängige Testprogramme für jede Suchkomponente.
- Update-Datei `sl-haaste-regex-shortener-update-v5.zip`.

# Version 4.0

- Allgemeine Synthese aus endlichen Ausnahmen und vermiedenen Teilfolgen.
- Aus DFA-Übergängen gewonnene Zeichenklassen und exakte semantische Überdeckungen; Gegenbeispiele steuern fehlende Zweige.
- Additive Generatoren für unäre Sprachen und Übertragung auf längenabhängige Sprachen über Zeichenklassen, stets mit anschließender Sprachprüfung.
- Beide Reihenfolgen der Normalisierung erhalten und vergleichen: gemeinsames Ausklammern zuerst oder lokale Umformungen zuerst.
- Kürzere Ausgabe für leere Alternativen und nullable Wiederholungen.
- Neue unabhängige Teilfolgen-Automaten, Druckerprüfungen und komplementäre Wortsprachen als Regressionen.
- Reproduzierbarer Vergleich mit 43 offiziellen Aufgaben und öffentlich veröffentlichten Rekordlängen, vollständig ohne Katalog.
- Neue Update-Datei `sl-haaste-regex-shortener-update-v4.zip`.

# Version 3.0

- Persistenter, nach minimalen DFA indizierter Katalog für beste bekannte Ausdrücke und kurze Teilsprachen.
- Automatisches Lernen, `--learn`, `--db` und `--no-catalog`; atomare, prozessübergreifend gesperrte Speicherung.
- Katalogsuche auch über zusammenhängende Fenster einer flachen Verkettung.
- Neue Synthese für einbuchstabige Sprachen: Perioden, arithmetische Folgen, beschränkte Summen, modulare Gitter und endliche Restmengen.
- Automatische Überdeckungen durch Längenschranken und Klassensterne.
- Erkennung von Sprachen ohne gleiche benachbarte Zeichen und kompakter Aufbau über Alphabetzerlegung.
- Allgemeine Schleifenpolsterung, Gruppierung wiederholter Blöcke und Faktorisierung von Präfix- und Suffix-Untergruppen.
- Kürzere Ausgabe durch leere Alternativen; flache Verkettungen werden auch nach der Zusammenfassung von Wiederholungen erhalten.
- Die sechs geprüften Nutzervorschläge werden transparent als Startkatalog mitgeliefert. Messungen unterscheiden die Suche mit und ohne diesen Katalog.
- Update-ZIP mit neuem Namen: `sl-haaste-regex-shortener-update-v3.zip`.

# Version 2.0

- Zeichenklassen können anhand des Kontexts in kleinere Klassen und Restmengen zerlegt werden.
- Neue allgemeine Umformungen `A(A|B)* = (AB*)+` und `(A|B)*A = (B*A)+`, einschließlich verwandter Formen mit obligatorischen Wiederholungen.
- Die Regeln erkennen auch zusammengesetzte Wortblöcke. Ihre Länge wird aus den Alternativen ermittelt und ist nicht auf ein festes Suchfenster beschränkt.
- Überflüssige Wiederholungen vor oder nach einer Schleife werden direkt vereinfacht, wenn die erforderliche Sprachzerlegung feststeht.
- Die Suche über Zwischenformen erhält Zeichenklassen und Mengendifferenzen aus der gesamten Eingabe, um zusätzliche Zerlegungen auch ohne unmittelbaren Längengewinn auszuprobieren.
- Neue systematische Äquivalenz- und Regressionstests; direkter Vergleich mit Version 1 in `examples/benchmarks-v2.json`.
- `--version` zeigt `regex-shortener 2.0` an.

Beim Aktualisieren nach dem Entpacken `bash build.sh` ausführen. Die neue ZIP enthält Quellcode; eine vorhandene ausführbare Datei wird erst beim Bauen aktualisiert.
