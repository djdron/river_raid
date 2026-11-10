# River Raid (Atari 8-bit) → C++-Portierung: Projektstand

[English](HANDOFF.md) | [Русский](HANDOFF.ru.md) | [Українська](HANDOFF.uk.md) | Deutsch | [Polski](HANDOFF.pl.md) | [Italiano](HANDOFF.it.md) | [Español](HANDOFF.es.md) | [中文](HANDOFF.zh.md) | [日本語](HANDOFF.ja.md)

Diese Datei dient dazu, die Arbeit auf jedem Rechner oder in einem neuen Chat wieder aufzunehmen. Beginnen Sie eine neue Sitzung mit „lies HANDOFF.de.md und docs/NOTES.de.md“.

## Ziel
Eine C++-Portierung von River Raid (Activision, Atari 800, 1983, von Carol Shaw) für Windows, Linux und macOS:
- Spiellogik identisch mit dem Original, lesbar für einen gewöhnlichen Programmierer ohne 6502-Kenntnisse;
- identischer Sound: derselbe Soundtreiber plus ein POKEY-Kern;
- eine plattformübergreifende Schicht: SDL3 (SDL2 als Option, für UWP auf der Xbox);
- keine Originaldaten werden verteilt; die Portierung liest zur Laufzeit das ROM des Benutzers (entschieden: keine ROM-Daten im Code).

## Stand
Das Spiel ist fertig und spielbar: `port/game/river_raid.cpp` (Demo-Modus, Spiele 1–8, ein und zwei Spieler, das Flugzeug, Treibstoff, Gegner, Panzer, Brücken, Punkte, Leben, Highscores, Pause, Sound). Gebaut und gespielt unter Windows, macOS (Apple M2) und Linux. Bild für Bild mit dem RAM des Originals verglichen, in Bot-, Zwei-Spieler-, Leerlauf- und langen Läufen; der Soundtreiber schreibt dieselben POKEY-Werte. Kommandozeile und Tasten: `port/README.de.md`, „Das Spiel“. Wie das Spiel funktioniert, für alle: `docs/HOW_IT_WORKS.de.md`.

## Was Sie brauchen
- Den ganzen Projektordner.
- Ihr eigenes ROM `River_Raid.rom`: 8192 Bytes, ohne Header, wird bei $A000–$BFFF geladen. Es ist nicht im Repository; legen Sie es unter `rom/River_Raid.rom` ab.

## Aufbau
| Pfad | Was |
|---|---|
| `port/` | die C++-Portierung: Spiel, Fluss-Betrachter, Prüfwerkzeuge; siehe `port/README.de.md` (Bauen, Optionen, Tasten, Inhalt, Überprüfung) |
| `docs/HOW_IT_WORKS.de.md` | wie das Spiel funktioniert, für alle: Fluss, Gegner, Brücken, Kollisionen, warum es schwerer wird |
| `docs/NOTES.de.md` | Erkenntnisse mit Adressen |
| `disasm/river_raid_a8.asm` | vollständiges Listing mit Kopf (Einsprungpunkte, Bild, Variablen) |
| `tools/dis6502.py` | Disassembler: `python dis6502.py <rom> <out.asm> B500 B2A5 B485` |
| `tools/check_lfsr.py`, `tools/verify_trace.py` | Prüfung dokumentierter Regeln an Referenz-Traces |
| `tools/level_map.py` | Bilder des ganzen Flusses, Abschnitt für Abschnitt |
| `tools/doc_figures.py` | die Bilder in `docs/HOW_IT_WORKS.de.md` (`docs/img/`), aus `port/tools/river_dump` |
| `oracle/` | Referenzlauf auf libatari800: `rr_oracle.c`, `build.sh`, `README.md`, Eingabeskripte, Autopilot `-bot`, unverwundbares Flugzeug `-ghost` |
| `docs/oracle_demo.png` | Bilder eines Referenzlaufs |

## Gesicherte Fakten (alle geprüft)
1. **Handler.** Hauptschleife A000–AF5A, ein Durchlauf = ein Bild (synchronisiert über VCOUNT $50/$78). VBI $B2A5 = POKEY-Soundtreiber und Zähler. DLI $B500 = Sprite-Multiplexing pro Zeile und Lesen der Kollisionen. IRQ $B485 = Platzhalter.
2. **Flussgenerator.** Software-LFSR mit 24 Bit (`sub_B183`, $AA/$AB/$AC), ein Schritt pro Zeile; alles (Ufer, Inseln, Verzierungen, Objekte) stammt daraus und aus dem Level. Er hängt weder vom Spieler noch von RANDOM ab. Der Fluss wiederholt sich 16,8 Millionen Brücken lang nicht (LFSR-Periode 16 766 977 Schritte, 993 Schritte pro Brücke). C++ (`port/src/river_gen.cpp`) stimmt Byte für Byte: 119 758 Zeilen und Starts von Grund auf.
3. **Startpunkte.** Die Spiele 1–8 beginnen an den Brücken 1/5/20/50 desselben Flusses: Die Tabellen $B494–$B4A8 enthalten dort LFSR, Ufer, Level und Brückenzähler; der Generator, ab dem Start von Spiel 1 laufen gelassen, erreicht genau diese Werte. Die Portierung füllt dieselbe Tabelle, indem sie den Generator laufen lässt, für `-seed N` (ein anderer Fluss) und `-bridge N` (Start an einer beliebigen Brücke).
4. **Schwierigkeit.** Das Level (= die Brücke, auf die man zufliegt, bis 80, dann abwechselnd 79/80) verschiebt die Schwellen, mit denen die Zufallszahlen verglichen werden: Gegner mitten im Block, weniger Tanklager bis Level 46 (47 auf PAL), Ballons ab 3, Düsenjäger ab 5, Uferpanzer ab 7, Raketen-Hubschrauber ab 13, weitere Kurven. Tabelle in `docs/HOW_IT_WORKS.de.md`.
5. **RANDOM ($D20A).** Die Spiellogik liest es nur bei A205: alle 16 Bilder erwacht ein zufälliger Slot (0–10) (Bit $40 von `$42,X`). B362/B3D0: Rauschen für den Sound. Der Wert hängt vom CPU-Takt des Lesens ab; die Portierung bildet den Zähler nach (`src/pokey_random.h`), gestartet von der Uhr wie bei einem echten Einschalten; `-random N` wiederholt einen Lauf.
6. **Kollisionen** kommen aus den Kollisionsregistern des GTIA, die der DLI nach jedem Objektstreifen liest; ob eine Berührung in der Lesezeile zählt, hängt vom Takt ab. Die Portierung führt den DLI mit Taktzählung aus (`src/display.cpp`, das nicht taktgenaue Timing von atari800).
7. **PAL/NTSC.** Ein Modul für beide: Das PAL-Bit ($D014 → $BF) wählt die Variante der Display List ($B77A) und die Tanklager-Grenze ($CB = $2E/$2F). Sonst ändert sich nichts, auf PAL läuft das ganze Spiel also mit 50 Bildern pro Sekunde, etwa 17 % langsamer. Die britische Kassettenversion (`.cas`, 2 Dateien: Lader bei $0A00, Spiel bei $1000–$31FF) ist derselbe Code, in den RAM verschoben, mit einem anderen Kopierschutz und „1982, 1983, 1984“ in der Laufschrift.
8. **Schrift** wird aus dem OS-ROM kopiert ($E000/$E100/$E300); die Portierung hat die Schrift von AltirraOS eingebaut, `-os` lädt die von Atari.
9. **Referenzlauf.** atari800 startet POKEY von der Uhr; `rr_oracle -seed` (Standard 0) macht Läufe Byte für Byte gleich. Der Build von atari800 scheitert mit `-Werror` an einer Neudefinition von ULONG; `build.sh` baut mit `CFLAGS="-g -O2 -DNETSIO"`. Geprüfter atari800-Commit: 98cc2dc.
10. **Determinismus.** Der Fluss hängt nur von der Brücke ab (993 LFSR-Schritte von Brücke zu Brücke; er wiederholt sich nach 16,8 Millionen Brücken), die Objekte nur von den letzten ~200 Bildern. Daher erreicht man jeden Zeitpunkt, indem man nur den Fluss laufen lässt und dann einige hundert ganze Bilder; so spult der Betrachter. Einzelheiten: `docs/NOTES.de.md`, „Determinismus und Sprünge“.

## Grenzen der Umgebung
- `oracle/build.sh` baut unter Linux, macOS, WSL und MSYS2 (abgefangene Emulatorfunktionen werden beim Kompilieren umbenannt, kein GNU `--wrap`); die C++-Portierung baut mit gewöhnlichem CMake (MSVC, Xcode, gcc).
- Das ROM ist nicht im Repository (`.gitignore`); legen Sie Ihr Abbild unter `rom/River_Raid.rom` ab.
- Code wird mit Tabs eingerückt, auch Fortsetzungszeilen (`.editorconfig`). Dokumentation: `X.md` auf Englisch, `X.ru.md` auf Russisch, dazu `X.uk.md`, `X.de.md`, `X.pl.md`, `X.it.md`, `X.es.md`, `X.zh.md`, `X.ja.md`; `tools/doc_languages.py` schreibt die Sprachzeile unter jeden Titel, die Bildbeschriftungen stehen in `tools/doc_figures_i18n.py`. Eine Änderung an einem Dokument geht in alle Sprachen.

## Methode
Eine Regel wird aus dem Listing gelesen und dann an einem Referenz-Trace geprüft (`tools/verify_trace.py`, dann die C++-Prüfwerkzeuge). Traces allein dienen nur dazu, die richtige Stelle zu finden. Änderungen, die das Verhalten nicht ändern sollen (Umbenennung, Umbau), werden mit `port/tools/equivalence` gegen einen älteren Commit geprüft: 0 Unterschiede verlangt, dazu alle Prüfungen in `port/README.de.md`, „Überprüfung“.

## Entscheidungen
- Die Portierung ist in C++ geschrieben, nicht als Hülle um einen Emulator.
- Die Überprüfung vergleicht das RAM der Portierung Bild für Bild mit dem Referenz-`ram.bin`; die RANDOM-Werte dafür stammen aus dem Referenz-`random.log`.
- Keine ROM-Daten im Code: Die Tabellen des Moduls werden über benannte Zugriffsfunktionen gelesen (`port/src/rom.h`).
- Standardfarben: PAL, Modell von Altirra; NTSC und das Modell von atari800 per Taste.
- Das Fenster zeigt das Spielbild (364×200 mit der ganzen Laufschrift), nicht das ganze Bild mit seinem schwarzen Rand (`-border`, O); skaliert scharf um ganze Zahlen, dann weich (`-filter`, L).
- Das Taktrennen beim Spielende (ob der Bildzähler vor oder nach dem VBI dieses Bilds zurückgesetzt wird, hängt von den Takten der Hauptschleife ab, siehe `port/README.de.md`, „Überprüfung“) wird bewusst nicht nachgebildet.
- Auf einem 60/120-Hz-Bildschirm koppelt sich das Spiel an den Bildschirm (60,00 statt 59,92 Bilder pro Sekunde), der Sound wird entsprechend umgerechnet.

## Offene Fragen
- Das Einschalten selbst (A000–A17C, die ersten 38 Bilder) wird als leerer Bildschirm gezeichnet; `Game::powerOn` ist nicht genau.
- PAL: Display List und 50-Hz-Timing werden nicht nachgebildet (`-pal` ändert nur die Tanklager-Grenze und die Farben).

## Nächste Schritte
1. Optional: PAL-Timing (50 Hz, PAL-Display-List) für alle, die das Spiel von einem europäischen Fernseher kennen.
2. Optional: Speichern und Laden des Zustands (der ganze Zustand steckt in `GameState` + `RiverState`, einfache Daten).
3. Optional: ein UWP/Xbox-Build auf SDL2 (`-DRR_SDL2=ON`).
