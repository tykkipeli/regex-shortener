# Update auf Version 7.0

Im bestehenden Projektverzeichnis ausführen:

```bash
unzip -o ~/Downloads/sl-haaste-regex-shortener-update-v7.zip -d .
bash build.sh
./regex-shortener --version
```

Erwartete Versionsausgabe: `regex-shortener 7.0`.

Die V7-ZIP enthält `src`, `build.sh`, README und die übrigen Projektdateien direkt auf der obersten Ebene. Beim Entpacken werden die Dateien im aktuellen Projektverzeichnis aktualisiert. Ein temporäres Verzeichnis und Kopierbefehle sind nicht erforderlich.

Das Build-Skript meldet den Beginn der Kompilierung. Der Compiler selbst kann anschließend einige Minuten ohne weitere Ausgabe arbeiten. Erst der Build aktualisiert das ausführbare Programm.

Eine bestehende `regex-shortener.db` und eigene Ergebnisdateien sind nicht Teil der ZIP. Ab Version 5 ist der Katalog standardmäßig abgeschaltet. Für die ausdrückliche Wiederverwendung eines eigenen Katalogs `--db regex-shortener.db` angeben.

Für einen langen Suchlauf mit laufender Ergebnissicherung:

```bash
./regex-shortener --file eingabe.txt --seconds 28800 --jobs 2 --checkpoint beste.regex
```

Weitere Erklärungen zu Checkpoints, Suchgrenzen und Rekordsuche stehen in `README.md`.
