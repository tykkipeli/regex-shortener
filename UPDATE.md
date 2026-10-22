# Update auf Version 3.0

Im bestehenden Projektverzeichnis ausführen:

```bash
update_dir="$(mktemp -d)" &&
unzip -q ~/Downloads/sl-haaste-regex-shortener-update-v3.zip -d "$update_dir" &&
cp -a "$update_dir/regex-shortener/." . &&
bash build.sh &&
./regex-shortener --version
```

Erwartete Versionsausgabe: `regex-shortener 3.0`.

Eine bestehende `regex-shortener.db` wird durch die ZIP nicht überschrieben. Beim nächsten Programmstart wird sie wiederverwendet. Weitere Erklärungen, Suchbefehle und Katalogoptionen stehen in `README.md`.
