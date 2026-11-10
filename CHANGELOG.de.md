# Änderungen

> *English version: [CHANGELOG.md](CHANGELOG.md).*

## 1.0.0 (31.10.2026)

Erste Fassung. Herausgelöst aus dem [Gothic-I-II-Universal-Coop](https://github.com/corshaegar/Gothic-I-II-Universal-Coop)
(`OthelloUnlock.cpp`, dort seit Coop-Fassung v79 vom 21.10.2026 in Gebrauch).

- Schaltet Othellos Plugin-Sperre im Arbeitsspeicher ab: den Prüf-Verteiler mit der Meldung „Remove the following
  unsupported plugins“ und die beiden Whitelist-Abfragen der stillen Sabotage.
- Ein Thread im Hintergrund stellt sabotierte Engine-Bytes wieder her, falls die Muster einer abweichenden
  Othello-Fassung nicht passen.
- Eigenständig: nur Windows-Aufrufe, kein Union-SDK, statisch gebundene Laufzeit.
- Protokoll `OthelloFixMods.log` neben der DLL, nur wenn Othello erkannt wurde.
- Test gegen ein Speicherabbild der echten `OTHELLO_ABI.DLL` (18 Prüfungen).
