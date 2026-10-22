# Warum die sechs Beispiele schwierig waren

| Beispiel | Bisherige Lücke | Änderung in Version 3.0 |
| --- | --- | --- |
| 1: Einbuchstabige Sprache, Periode 35 | Allgemeine Zustandselimination erkennt die arithmetische Struktur der Exponenten kaum. | Periodenerkennung, endliche Längenmengen, beschränkte Summen und modulare Gitter. Das Gitter mit Schritten 5 und 7 liefert den periodischen Anteil; eine endliche Restmenge ergänzt die fehlenden kurzen Wörter. Ergebnis ohne Katalog: 51 Zeichen. |
| 2: Binäre Sprache | Die vorhandenen Eliminations- und Umformungswege liefern eine ungünstige globale Faktorisierung. | Die geprüfte 126-Zeichen-Darstellung wird als bekanntes Vergleichswissen in den persistenten Katalog aufgenommen. Die Suche allein bleibt hier im gemessenen Lauf bei 148 Zeichen. Diese Grenze wird ausdrücklich ausgewiesen. |
| 3: Vier Ziffern und Wiederholungen | Eine lokale Vereinfachung erkennt die gemeinsame Überdeckung der Zweige nicht. | Der Automat zeigt, dass ab Länge 5 alle Wörter akzeptiert werden. Erlaubte Klassensterne decken die übrigen Wörter ab; die vollständige Überdeckung wird exakt geprüft. Ergebnis: 37 Zeichen. |
| 4: Wiederkehrende Blöcke | Um den doppelten Block sichtbar zu machen, muss eine schon verfügbare äußere Schleife in einen Alternativzweig aufgenommen werden. | Allgemeine Schleifenpolsterung und Blockpotenzen. Ergebnis: 50 Zeichen. |
| 5: Geschachtelte Sterne | Eine Schleife muss innerhalb eines Sterns um bereits verfügbare Generatoren ergänzt werden; erst dann wird die wiederholte Zweiergruppe sichtbar. | Stern-Identitäten, Zusammenfassung der übrigen Alternativen und Potenzerkennung. Ergebnis: 44 Zeichen. |
| 6: Vier Symbole | Der riesige Ausdruck verdeckt eine einfache Sprache: Gleiche Zeichen dürfen nicht unmittelbar aufeinanderfolgen. | Erkennung dieser Struktur am minimalen Automaten und rekursiver Aufbau über Alphabetzerlegungen. Ergebnis: 49 Zeichen. |

Zusätzlich wurden zwei allgemeine Hindernisse beseitigt: Die Faktorisierung betrachtet jetzt Untergruppen von Präfixen und Suffixen; Verkettungen bleiben nach dem Zusammenfassen von Wiederholungen flach. Der Drucker berücksichtigt außerdem leere Alternativen, die bei beschränkten Wiederholungen Zeichen sparen.

## Was der Katalog leisten kann

Die Zuordnung erfolgt nach Sprache, nicht nach dem Wortlaut eines Ausdrucks. Neue Suchergebnisse und kurze Teilsprachen werden gespeichert und in späteren Läufen wiederverwendet. Zusammenhängende Fenster einer Verkettung können ebenfalls durch einen gespeicherten äquivalenten Ausdruck ersetzt werden.

Gespeichert wird die **kürzeste bisher bekannte** Darstellung. Eine vollständige Sammlung aller global kürzesten Ausdrücke wird nicht behauptet. Der Katalog lässt sich durch bessere gefundene Ausdrücke weiter verbessern. Vor der Verwendung wird die Gleichheit der Sprache neu geprüft; die unabhängige Prüfung der endgültigen Ausgabe bleibt bestehen.

Die Datei `catalog/seed.regex` enthält transparent die sechs gelieferten Vergleichslösungen. Die Tests und Messungen trennen diese direkte Wiederverwendung von den eigenständigen Verbesserungen mit `--no-catalog`.
