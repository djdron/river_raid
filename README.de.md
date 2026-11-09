# River Raid (Atari 8-bit) → C++

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | Deutsch | [Polski](README.pl.md) | [Italiano](README.it.md) | [Español](README.es.md) | [中文](README.zh.md) | [日本語](README.ja.md)

Reverse Engineering und C++-Portierung von Activisions River Raid für Atari 400/800 (1983) mit dem
Ziel, Spiellogik und Sound 1:1 auf heutigen Windows-, Linux- und macOS-Rechnern wiederzugeben.

**[Wie River Raid funktioniert](docs/HOW_IT_WORKS.de.md)**: eine bebilderte Erklärung für alle, ohne
Programmierkenntnisse: wie der endlose Fluss entsteht, warum er in jedem Spiel gleich ist, wie
Gegner und Brücken platziert werden, warum es schwerer wird und wie ein einziges Sprite jeden
Gegner auf dem Bildschirm zeigt.

[![Von Brücke 1 bis Brücke 2](docs/img/stretch.de.svg)](docs/HOW_IT_WORKS.de.md)

- [`HANDOFF.de.md`](HANDOFF.de.md) — Projektstand, geprüfte Fakten, Entscheidungen, nächste Schritte (hier anfangen)
- [`docs/HOW_IT_WORKS.de.md`](docs/HOW_IT_WORKS.de.md) — wie das Spiel funktioniert, für alle: Fluss, Gegner, Brücken, Kollisionen
- [`docs/NOTES.de.md`](docs/NOTES.de.md) — Erkenntnisse mit Adressen
- `disasm/` — kommentiertes Disassembly
- `tools/` — Disassembler, Prüfskripte
- `oracle/` — Referenzlauf auf libatari800 (RAM- und POKEY-Traces Bild für Bild)
- [`port/`](port/README.de.md) — die C++-Portierung: das Spiel (`river_raid`), der Fluss-Betrachter, Prüfwerkzeuge
- `rom/` — hier Ihr eigenes ROM-Abbild ablegen (nicht enthalten)

## Hinweis
Dies ist ein inoffizielles Fanprojekt zu Lernzwecken: Es soll zeigen, wie ein Spiel von 1983 funktioniert, wie sein Code den Fluss, die Gegner und den Sound erzeugt, und das in lesbarem C++ darstellen. Es steht in keiner Verbindung zu Activision oder Microsoft und wird von ihnen weder unterstützt noch gefördert. River Raid und Activision sind Marken ihrer Inhaber; sie werden hier nur genannt, um zu sagen, um welches Spiel es geht.

Kein Teil des Spiels ist enthalten: kein ROM, keine Grafik, kein Sound. Um die Portierung zu starten, brauchen Sie Ihr eigenes Abbild des River-Raid-Moduls (8 KB, siehe [`rom/README.de.md`](rom/README.de.md)). Bitte fragen Sie nicht in Issues, wo man es bekommt.

## Lizenz
Der eigene Code und die Dokumentation des Projekts stehen unter der MIT-Lizenz, siehe [`LICENSE`](LICENSE). Ausnahmen:
- `port/src/os_font.cpp` enthält den Zeichensatz von AltirraOS (Copyright © 2008–2018 Avery Lee) unter dessen eigener freizügiger Lizenz (all-permissive); der Hinweis steht in dieser Datei.
- `oracle/rr_oracle` wird mit libatari800 gelinkt und steht deshalb unter GPL-2.0+ (siehe [`oracle/README.de.md`](oracle/README.de.md)). Die Portierung linkt nicht damit.
- Die beiden Palettenmodelle in `port/src/palette.cpp` folgen den Farbmodellen der Emulatoren Altirra und atari800; der Code ist neu geschrieben.
- River Raid selbst (das ROM, sein Code, Grafik und Sound) gehört den Rechteinhabern und fällt nicht unter diese Lizenz.
