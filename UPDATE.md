# Update auf Version 6.0

Im bestehenden Projektverzeichnis ausführen:

```bash
update_dir="$(mktemp -d)" &&
unzip -q ~/Downloads/sl-haaste-regex-shortener-update-v6.zip -d "$update_dir" &&
cp -a "$update_dir/regex-shortener/." . &&
bash build.sh &&
./regex-shortener --version
```

Erwartete Versionsausgabe: `regex-shortener 6.0`.

Eine bestehende `regex-shortener.db` wird durch die ZIP nicht überschrieben. Weitere Erklärungen, Suchbefehle und Katalogoptionen stehen in `README.md`.

Ab Version 5 ist der Katalog standardmäßig abgeschaltet. Für die ausdrückliche Wiederverwendung eines eigenen bisherigen Katalogs `--db regex-shortener.db` angeben.
