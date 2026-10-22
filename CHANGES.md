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
