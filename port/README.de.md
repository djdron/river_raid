# River Raid als C++-Portierung

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | Deutsch | [Polski](README.pl.md) | [Italiano](README.it.md) | [Español](README.es.md) | [中文](README.zh.md) | [日本語](README.ja.md)

Ein inoffizielles Fanprojekt zu Lernzwecken, ohne Verbindung zu Activision oder Microsoft; kein Teil des Spiels ist enthalten (siehe den Hinweis in [../README.de.md](../README.de.md)).

Das ganze Spiel: `river_raid` spielt River Raid wie das Original — Demo-Modus, Spielauswahl, ein und zwei Spieler, das Flugzeug, Treibstoff, Gegner, Brücken, Punkte, Leben, Highscores, Pause und Sound. Der Flussgenerator und die Spiellogik erzeugen Bild für Bild denselben RAM-Zustand wie das Original, das Zeichnen denselben Bildschirm (siehe Überprüfung).

## Bauen
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
Der Kern und die Werkzeuge brauchen nur einen C++17-Compiler. Das Spiel `river_raid` und der Betrachter `river_view` brauchen SDL3: CMake verwendet ein installiertes SDL3 (3.4 oder neuer), wenn es eines findet, sonst lädt es beim Konfigurieren SDL 3.4.20 von GitHub (braucht git und Internet) und linkt es statisch. Unter Linux braucht das Bauen von SDL aus den Quellen die X11/Wayland-Entwicklungspakete (siehe `docs/README-linux.md` von SDL). SDL 3.4 bricht mit einem Fehler ab, wenn ein solches Paket fehlt, und nennt die Option, die ohne es baut (zum Beispiel `-DSDL_X11_XCURSOR=OFF`). Ganz ohne SDL: `cmake -S . -B build -DRR_VIEWER=OFF`.

Mit SDL2 statt SDL3 (für Plattformen, die SDL3 nicht mehr unterstützt, etwa UWP auf der Xbox): `cmake -S . -B build -DRR_SDL2=ON`. CMake verwendet dann ein installiertes SDL2 (2.0.18 oder neuer) oder lädt SDL 2.32.10. Die Programme sprechen mit SDL nur über `sdl/host.h`, das für jede Version einmal umgesetzt ist; beide liefern dasselbe Bild. Das Spiel als UWP-App für Windows und die Xbox: [uwp/README.de.md](uwp/README.de.md).

Unter Windows mit Visual Studio: `cmake --build build --config Release`, die Programme landen in `build\Release\`. Gebaut und gespielt unter Windows (MSVC), macOS (Xcode, Apple M2) und Linux (gcc).

## Das Spiel
```
river_raid [rom] [Optionen]
```
Ohne Argumente findet es das ROM selbst (siehe `rom`) und startet wie das Modul: Demo-Modus, dann START.

| Option | Was sie bewirkt |
|---|---|
| `rom` | das Modulabbild (8192 Bytes). Standardmäßig wird `rom/River_Raid.rom` im aktuellen Ordner und bis zu drei Ordner darüber gesucht, sodass es aus dem Repository oder aus `port/build/Release` ohne Argumente startet |
| `-os Datei` | Buchstaben und Ziffern aus Ataris eigenem Betriebssystem: ein OS-ROM-Abbild (10 KB oder 16 KB) oder eine 1-KB-Schrift. Ohne sie wird die eingebaute AltirraOS-Schrift verwendet |
| `-seed N` | ein anderer Fluss mit anderen Gegnern und Tanklagern. N ist der Zustand, mit dem der Flussgenerator beginnt, 1 bis 16777215. Spiel 1 beginnt am Anfang dieses Flusses, Spiele 3–8 an seinen Brücken 5, 20 und 50; jedes Spiel behält sein übliches Level, die Schwierigkeit steigt also wie gewohnt. Der Fluss des Moduls selbst ist `-seed 16241891` |
| `-bridge N` | jedes Spiel (1–8) beginnt an Brücke N, 1 bis 99999: mit N auf dem Brückenzähler (über 9999 seine letzten 4 Ziffern, da der Zähler des Originals rundherum läuft) und dem Level dieser Brücke (nach 80 wechselt das Level zwischen 79/80, wie im Original). Funktioniert auch mit `-seed`. Der Start wird gefunden, indem der Flussgenerator bis dorthin läuft (unter einer Sekunde für 9999, etwa 8 für 99999) |
| `-random N` | der Startwert von POKEYs RANDOM-Zähler, der entscheidet, welcher Gegner wann erwacht, und das Rauschen der Explosionen. Standardmäßig von der Uhr genommen, wie der des echten Geräts vom Moment des Einschaltens abhängt, sodass jeder Lauf anders ist; mit demselben N und denselben Eingaben wiederholt sich ein Lauf genau |
| `-pal` | als PAL-Gerät spielen: wie auf einem echten PAL-Atari werden Tanklager bis Level 47 statt 46 seltener; PAL-Farben. Das Bild-Timing bleibt NTSC (siehe Noch nicht erledigt) |
| `-colours ntsc\|pal` | die Farben eines NTSC- oder PAL-Geräts; standardmäßig PAL. C schaltet während des Spiels um |
| `-palette altirra\|atari800` | das Farbmodell: `altirra` (Standard) oder `atari800` (dunklere Mitteltöne). V schaltet während des Spiels um |
| `-border` | zeigt das ganze Bild mit seinem schwarzen Rand (384×240 abzüglich 10 Spalten an jeder Seite), wie Emulatoren es tun. Standardmäßig zeigt das Fenster nur das Spielbild mit der ganzen Laufschrift, 364×200, daher ist es im Vollbild etwa 20 % größer, und ein Fenster ist um so viel niedriger. O schaltet während des Spiels um |
| `-filter sharp\|nearest\|linear` | wie das Bild auf Fenster oder Bildschirm skaliert wird: `sharp` (Standard) skaliert um ganze Zahlen mit scharfen Pixeln und dann weich auf die Endgröße, so bleibt es scharf und jedes Pixel gleich groß; `nearest` sind einfache Blöcke, aber bei nicht ganzzahligem Maßstab (waagerecht immer, da Atari-Pixel nicht quadratisch sind) werden manche Spalten ein Bildschirmpixel breiter; `linear` ist weich und unscharf. L schaltet während des Spiels um |
| `-stats` | gibt den Renderer und die Bildwiederholrate aus, dann jede Sekunde die Bildrate, die Zeit pro Bild und die Sound-Warteschlange |
| `-nosound` | kein Sound |

Der Fenstertitel zeigt die aktuellen Farben und die Skalierung. Auf einem Bildschirm mit 60 Hz (oder 120 Hz ...) läuft das Spiel genau mit dessen Rate (60 statt der 59,92 Bilder pro Sekunde des Atari), damit jedes Bild gleich lang zu sehen ist; auf anderen Bildschirmen mit der Rate des Atari.

Beispiele: `river_raid -bridge 30` startet an Brücke 30; `river_raid -seed 12345` ist ein anderer Fluss; `river_raid -seed 12345 -bridge 80` ist dieser Fluss in seiner schwersten Form.

Nach dem Einschalten fliegt der Demo-Modus über den Fluss. START beginnt ein Spiel; SELECT (oder OPTION) wählt die Spiele 1–8: gerade Nummern sind Spiele für zwei Spieler (die sich abwechseln), die Spiele 3–4, 5–6 und 7–8 beginnen an den Brücken 5, 20 und 50. Jede Taste der Atari-Tastatur hält an. Nach dem Spielende bleibt das Bild stehen, bis der Demo-Modus wieder beginnt (etwa 35 s). Spiel und Betrachter werden auf die übliche Weise beendet: Fenster schließen, Alt+F4, Cmd+Q.

| Taste | Atari |
|---|---|
| Pfeiltasten oder W A S D | Joystick: links/rechts lenken, hoch schneller, runter langsamer; jede Richtung startet das Flugzeug an der Brücke |
| linke/rechte Strg, Z, X | Feuer |
| F4 oder Enter | START |
| F3 | SELECT |
| F2 | OPTION |
| Leertaste | eine Taste der Tastatur: Pause |
| F, F11 oder Alt+Enter | Vollbild |
| C | NTSC- / PAL-Farben |
| V | Farbmodell Altirra / atari800 |
| O | nur das Spielbild / das ganze Bild mit Rand |
| L | Skalierung: sharp / nearest / linear |
| F7 / F8 | Spielstand speichern / zu ihm zurückkehren (im Speicher, bis zum Beenden) |
| Esc | Vollbild verlassen |
| Mausklick | blendet den Mauszeiger aus, bis die Maus bewegt wird (ebenso der Wechsel ins Vollbild) |

Die Tastatur steuert beide Joysticks, sodass auch eine Person ein Spiel für zwei spielen kann. Gamepads: das erste ist Joystick 1 (und 2, solange es kein zweites gibt), das zweite ist Joystick 2; Steuerkreuz oder linker Stick, A/B/Y oder der rechte Trigger zum Feuern, X = Pause, Start = START, Back = SELECT, RB / LB = F7 / F8 (Stand speichern / zu ihm zurückkehren).

## Fluss-Betrachter
```
river_view [rom] [Optionen]
```
Er zeigt den Demo-Modus des Originals, den Flug über den Fluss, der läuft, wenn niemand START drückt: Nach dem Einschalten füllt sich der Bildschirm mit dem Fluss von Spiel 1, der dann 1 Zeile pro Bild scrollt; Schiffe und Hubschrauber erwachen und bewegen sich, Panzer fahren zu den Brücken und feuern, Hubschrauber schießen Raketen (die portierte Spiellogik, `src/game.cpp`); darunter die Anzeige und die Laufschrift mit den Mitwirkenden, „Press START key“ und dem regenbogenfarbenen Activision-Logo. Die Optionen sind die des Spiels: `rom`, `-os`, `-seed`, `-bridge` (der Betrachter fliegt zuerst vom Anfang des Flusses dorthin, nur mit dem Fluss, bei 9999 einige Sekunden, und zeigt sie so, wie der Flug sie zeigen würde; der Weg zurück bis zum Anfang lässt sich ansehen), `-random`, `-pal`, `-colours`, `-palette`, `-border`, `-filter`; dazu sein eigenes `-sound`: der Demo-Modus mit Ton (siehe M).

Buchstaben und die Ziffern 1–9 stammen aus der Schrift des Betriebssystems, die nicht im Modul steckt. Die Portierung hat die Schrift von AltirraOS eingebaut: dem freien Ersatz-Betriebssystem, das mit atari800 ausgeliefert wird (Lizenz: Kopieren und Ändern erlaubt, wenn der Copyright-Hinweis erhalten bleibt, siehe `src/os_font.cpp`); auch die Referenz-Traces wurden damit erstellt. `-os Datei` nimmt die Schrift von Ataris eigenem Betriebssystem aus einem OS-ROM-Abbild (10 KB oder 16 KB) oder einer 1-KB-Schriftdatei.

| Taste | Aktion |
|---|---|
| Leertaste | Pause |
| Hoch / Runter | schneller / langsamer, über einen Halt bis zum Rückwärtsflug (x1/4 bis x16 in beide Richtungen) |
| Mausrad, Bild↑ / Bild↓ | den Fluss von Hand verschieben, um einen Bildschirm |
| Rücktaste | zurück zur neuesten Zeile mit normaler Geschwindigkeit |
| B / Umschalt+B | nächste / vorige Brücke |
| Alt+B / Alt+Umschalt+B | 10 Brücken vor / zurück |
| Strg+B / Strg+Umschalt+B | 100 Brücken vor / zurück |
| Pos1 | zu Brücke 1, an den Anfang des Flusses |
| F1 / F2 / F3 | Soundtest: Explosion / Schuss / Punkteton über den Soundtreiber des Spiels (der Demo-Modus selbst ist stumm, wie im Original) |
| F | Vollbild |
| C | NTSC- / PAL-Farben |
| V | Farbmodell Altirra / atari800 |
| O | nur das Spielbild / das ganze Bild mit Rand |
| L | Skalierung: sharp / nearest / linear |
| M | Ton an / aus: Panzergranaten, Hubschrauberraketen und der Motor eines Flugzeugs, wie im Spiel (nicht im Original, dessen Demo-Modus stumm ist); beim Vorwärtsfliegen bei jeder Geschwindigkeit, der Motor folgt ihr wie bei einem Flugzeug, das beschleunigt oder bremst; rückwärts läuft der Ton rückwärts wie ein Band |
| Esc | Vollbild verlassen |
| Mausklick | blendet den Mauszeiger aus, bis die Maus bewegt wird (ebenso der Wechsel ins Vollbild) |

Verglichen mit den Bildschirmen des Originals (Demo-Modus, Bilder 1000–2300, mit den RANDOM-Werten des Originals): Flussbereich und Anzeige stimmen Pixel für Pixel überein, die Laufschrift ebenfalls bis auf ihre äußeren Spalten (links von 24, rechts von 359), wo der Emulator das feine Scrollen am Rand anders behandelt.

Welches Objekt wann erwacht, entscheidet POKEYs RANDOM-Register, ein Hardwarezähler, der zu einem Zeitpunkt gelesen wird, der vom Timing der ganzen Maschine abhängt. Die Portierung bildet den Zähler nach (`src/pokey_random.h`) und startet ihn von der Uhr, so wie das echte Gerät ihn beim Einschalten startet; jeder Lauf ist also anders, wie auf dem echten Gerät; `-random N` wiederholt einen Lauf.

## Inhalt
| Datei | Was |
|---|---|
| `src/rom.h` | das Modulabbild und benannte Zugriffsfunktionen für jede Tabelle, die die Portierung daraus liest (Formen, Farben, Punkte, Generatortabellen, Anzeigetext ...); Objekttypen |
| `src/river_gen.h/.cpp` | der Generator: `RiverGen::startGame(n)`, dann `nextLine(line)` — eine Bildschirmzeile pro Aufruf |
| `src/river_draw.h/.cpp` | eine Zeile so zeichnen, wie sie auf dem Bildschirm aussieht: Ufertöne, Straßen an Brücken, Objekt-Sprites (`drawScreenLine`) |
| `src/palette.h/.cpp` | Atari-Farben von NTSC- und PAL-Geräten (`atariRgb`): die Modelle von Altirra und atari800 |
| `src/game.h/.cpp` | das Spiel: Hauptschleife und VBI des Originals Bild für Bild — Demo-Modus, Konsolentasten, das Flugzeug, Treibstoff, Kollisionen, Explosionen, Punkte, Leben, Spieler, Absturz und Neustart, Spielende, Highscores, Soundtreiber |
| `src/display.cpp` | die Flussanzeige, wie der DLI sie steuert: ein Sprite für jedes Objekt wiederverwendet, Kollisionen pro Objekt, Zeichnen des Flussbereichs |
| `src/pokey_random.h` | POKEYs RANDOM-Zähler |
| `src/pokey_sound.h/.cpp` | POKEY-Sound: vier Kanäle, Teiler und Rauschzähler, Takt für Takt erzeugt |
| `src/status_bar.h/.cpp` | Anzeige und Laufschrift: Zeichensätze, Anzeige, Schritt der Laufschrift, Zeichnen |
| `src/os_font.h/.cpp` | Betriebssystem-Schrift: AltirraOS eingebaut oder aus einem OS-ROM geladen |
| `src/river_state_io.cpp`, `src/game_ram.cpp` | Zuordnung der Felder von Generator und Spiel zu den RAM-Adressen des Originals: Laden, Schreiben und Vergleichen (für die Überprüfung) |
| `game/river_raid.cpp` | das Spiel (SDL3): Bild, Sound, Tastatur und Gamepads |
| `viewer/river_view.cpp` | der Fluss-Betrachter (SDL3), siehe oben |
| `sdl/host.h`, `host_sdl3.cpp`, `host_sdl2.cpp` | Fenster, Bild, Sound, Tastatur und Gamepads für Spiel und Betrachter, über SDL3 oder SDL2; Ausblenden des Mauszeigers |
| `tools/river_map.cpp` | erzeugt den Fluss und speichert ein PNG so, wie das Spiel ihn zeigt: `river_map <rom> <Spiel 1..8> <Zeilen> out.png [-pal] [-flat]` (`-flat`: nur die Bitmap, feste Farben) |
| `tools/river_dump.cpp` | schreibt den erzeugten Fluss als Text (Farbe jedes Punkts, platzierte Objekte) für `tools/doc_figures.py`, das die Bilder in [docs/HOW_IT_WORKS.de.md](../docs/HOW_IT_WORKS.de.md) zeichnet: `python tools/doc_figures.py rom/River_Raid.rom` (braucht matplotlib) |
| `tools/sound_check.cpp` | vergleicht den Soundtreiber mit den POKEY-Schreibzugriffen des Originals (`rr_oracle -vbi`) |
| `tools/sound_render.cpp` | rendert ein Protokoll der POKEY-Schreibzugriffe mit dem POKEY der Portierung zu WAV, zum Vergleich mit dem Sound des Emulators (`rr_oracle -wav`) |
| `tools/game_check.cpp` | vergleicht das Spiel Bild für Bild mit einem RAM-Trace des Originals, mit dessen Eingaben und RANDOM-Werten (und den Flussbereich mit seinen Bildschirmen) |
| `tools/status_check.cpp` | vergleicht Anzeige und Laufschrift mit vollständigen Bildschirmen des Originals (`rr_oracle -screens A:B -ram A:B`) |
| `tools/render_check.cpp` | vergleicht das Zeichnen mit dem Bildschirm des Originals (`oracle/rr_oracle -ghost -strip 20`) |
| `tools/terrain_check.cpp` | Vergleich mit einem Referenz-Trace (`oracle/rr_oracle -ram A:B`) |
| `tools/equivalence/` | Refactoring-Prüfung: ein älterer Commit der Portierung gegen die Arbeitskopie, Bild für Bild (siehe Code) |

Der Generator liest Tabellen und Grafik aus dem ROM des Benutzers (`rom/River_Raid.rom`); im Code stecken keine Spieldaten.

## Code
Die Portierung ist so geschrieben, dass man sie ohne das Listing des Originals lesen kann:
- Zustandsfelder, Funktionen und Konstanten heißen nach ihrer Bedeutung (`planeState`, `shellTimer`, `enteringSlot`, `positionObject`, `kFacingLeft`); der Kommentar jedes Felds nennt die RAM-Adresse des Originals und der jeder Funktion die Codeadressen, aus denen sie stammt, sodass man dem Listing (`disasm/`) und den Traces weiter folgen kann;
- die Tabellen des Moduls werden über benannte Zugriffsfunktionen in `src/rom.h` gelesen (`rom.points(type)`, `rom.objectHeight(type)`, `rom.shellRise(t)` ...), nicht über Adressen;
- Werte bleiben 8-bittig mit Überlauf, wo das im Original so ist (`lo()`), denn Positionen laufen um den Bildschirm herum; wo das Original das Übertragsflag des 6502 von einer Summe in die nächste trägt, sagen `addWithCarry`/`subtractWithBorrow` das ausdrücklich;
- der Display-List-Interrupt (`display.cpp`) ist der einzige Teil, der nahe an den Befehlen bleibt: Sein Timing innerhalb der Rasterzeile entscheidet, welche Kollisionen gesehen werden, daher wird er mit Taktzählung ausgeführt.

Änderungen, die das Verhalten nicht ändern sollen, werden mit `tools/equivalence` geprüft: Es baut einen älteren Commit (ab 06ffc73) neben der Arbeitskopie und lässt beide auf 150 000 Flusszeilen jedes Spiels (NTSC und PAL), auf zufälligem Spiel ab dem Einschalten und ab 194 Zuständen aus den Traces des Originals laufen und vergleicht nach jedem Schritt den ganzen Zustand, den Bildschirm und die Soundregister. Umbenennung und Umbau von Generator, Spiel und Anzeige wurden so geprüft (0 Unterschiede), ebenso mit allen Prüfungen unten.

```
port/tools/equivalence/build.sh 06ffc73
port/build/eq/eq
```

## Wie der Generator funktioniert
Jeder Aufruf von `nextLine()` erzeugt eine neue oberste Bildschirmzeile:
1. `countLines`: Zeilenzählung (ein Block hat 32 Zeilen, ein Abschnitt 2 Blöcke, 16 Abschnitte bis zur Brücke), LFSR-Schritt, ein neues Breitenziel am Abschnittsanfang (`chooseSectionWidth`: Inseln öffnen und schließen sich);
2. `changeIsland`: Insel und Wasserbreite bewegen sich gleichmäßig zum Ziel;
3. `moveBank`: der Zufallsweg des Ufers, Umkehr an den Grenzen und vor dem neuen Objekt (`bankWouldHitObject`);
4. `startDecoration`: ein Haus oder Berg am Ufer;
5. `drawLine`: die 48 Bytes der Zeile (Füllungen, Randstücke aus der Tabelle bei `$B800`, eine Zeile der Verzierung);
6. `scrollObjects` / `placeObject`: die Objekt-Slots rücken weiter, alle 16 Zeilen wird ein neues Objekt platziert (`chooseEnemy`, `positionObject`, `placeTank`).

Die Erzeugung hängt weder vom Spieler noch von `RANDOM` ab. Das wurde am Referenzlauf geprüft: verschiedene POKEY-Seeds und ein Bot-Lauf gegenüber einem Flug ohne Eingabe ergeben in jeder Zeile denselben Zustand.

## Überprüfung
- Ab dem Referenzzustand: 119 758 Zeilen des ganzen Flusses (120 000 Bilder), 0 Unterschiede im Zustand und in den Bitmap-Zeilen.
- Von Grund auf (`startGame(1)`, ohne Emulator): nach den 165 Zeilen, die den Bildschirm füllen, stimmt der Zustand mit dem Original überein, dann 29 757 Zeilen ohne Unterschiede.
- Bot-Spiel mit zerstörten Brücken und dem Blitzeffekt: Unterschiede nur in den Bildern des Wiedereinstiegs. Die stammen aus dem Neustart-Code, der noch nicht portiert ist.
- Die Prüfung erkennt eingebaute Fehler: jede von vier absichtlichen Beschädigungen (Wanderbedingung, Wasserbreite, Zeichnen, Füllfarbe) erzeugte Unterschiede.

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/ghost_film.txt 30001 out -ghost -ram 0:30000
port/build/terrain_check rom/River_Raid.rom out/ram.bin 0 240 29760 5 -fresh 1 145
```

Zeichnen: In jedem Bild, in dem eine neue Zeile in Bildschirmzeile 20 kommt, wird die Zeile des Originals mit `drawScreenLine()` der Zeile aus dem C++-Generator (von Grund auf gestartet) verglichen.
- Mit den Objekten dort, wo der Generator sie platziert hat: 96,8 % der Zeilen identisch über 29 000 Bilder (95,3 % über 120 000). Die übrigen unterscheiden sich, wo das Spiel ein Objekt bewegt hat.
- Mit Objektpositionen aus dem laufenden Spiel (`-ram`, Slot-x, Flags, Art): 98,1 % identisch; jede verbleibende Zeile unterscheidet sich nur, solange der Panzer auf der Straße (`$38`) aktiv ist, den die Spiellogik zeichnet. 0 ungeklärte Zeilen in 29 600.

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/ghost_film.txt 30001 out -ghost -strip 20 -ram 0:30000
port/build/render_check rom/River_Raid.rom out/strip.bin out/frames.csv -ram out/ram.bin
```
Anzeige und Laufschrift: Die aus Betriebssystem-Schrift und Modul gebauten Zeichensätze gleichen dem RAM des Originals, der Zustand der Laufschrift ($3FB3/$26/$27) stimmt in jedem Bild, und die gezeichneten Zeilen 180–239 sind in allen 1299 Bildern identisch, außer den äußeren Spalten der Laufschrift (siehe Fluss-Betrachter).

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/attract_idle.txt 2300 out -screens 1000:2299 -ram 1000:2299
port/build/status_check rom/River_Raid.rom out/screens.bin out/ram.bin
```
Spielbild im Demo-Modus (`game_check`): Ab dem Zustand nach dem Einschalten (`Game::startAttract`) gestartet und mit den RANDOM-Werten des Originals versorgt, läuft die Portierung 19 900 Bilder (bis Brücke 20) ohne Unterschied zum RAM des Originals in irgendeinem verglichenen Byte: Generatorzustand, Objekt-Slots (Position, Richtung, Art, Kollisionsbit), Panzer, seine Granate, Hubschrauberrakete, Kollisionsergebnisse, Player-Missile-Speicher, die Flusszeilen auf dem Bildschirm. Der daraus gezeichnete Flussbereich gleicht den Bildschirmen des Originals in allen 1299 verglichenen Bildern.

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/attract_idle.txt 20000 out -strip 20
port/build/game_check rom/River_Raid.rom out/ram.bin out/random.log 38 19900 -attract
```

Das Spiel mit einem Spieler (`game_check` bei Läufen mit Eingabe). Ab dem Zustand des Demo-Modus bekommt die Portierung die Eingaben und RANDOM-Werte des Originals und läuft dann selbständig, ohne je neu geladen zu werden; in jedem Bild wird ihr RAM mit dem des Originals verglichen (Generator, Slots, Flugzeug, Treibstoff, Kugel, Kollisionen, Punkte, Leben, beide Spieler, Highscores und ihre Prüfbytes, Anzeige, Laufschrift, Player-Missile-Speicher, Bildschirmzeilen):
- Bot-Spiel für einen Spieler (`start_only.txt -bot -seed 3`, 8 950 Bilder: Demo-Modus, START, 4 Flugzeuge, Brücken, Abstürze und Neustarts, Spielende, wieder Demo-Modus): in jedem verglichenen Bild identisch;
- Spiel für zwei mit SELECT und der Pausetaste (`two_players.txt`, bis zum Spielende in Bild 3 007): identisch bis zum Spielende (ein Bild, siehe unten);
- der von der Portierung gezeichnete Flussbereich (Flugzeug, Kugel, Gegner, Explosionen, Panzer, Granaten) gleicht den Bildschirmen des Originals in allen 1 565 verglichenen Bildern eines Bot-Spiels.

Wo das Ergebnis des Originals von CPU-Takten abhängt:
- Der DLI (`display.cpp`) läuft mit Taktzählung, so wie der Referenz-Emulator eine Rasterzeile einteilt: Player/Missile- und Display-List-Zugriffe am Anfang, die CPU bis Takt 28, dann wird die ganze Zeile gezeichnet (samt Kollisionen), dann die Bildschirmdaten-Zugriffe, WSYNC bei Takt 106. So ist bekannt, ob ein Lesen eines Kollisionsregisters die aktuelle Rasterzeile schon sieht: Eine Kugel, die ein Objekt nur in der Zeile des Lesens berührt, zählt auf einer Brücke (dieser Pfad liest nach Takt 28) und anderswo nicht, wie im Original.
- Der RAM-Schnappschuss wird am Ende des Bilds genommen; normalerweise wartet die Hauptschleife des Originals dann auf das nächste Bild (AE99–AE9E). Gleich nach dem Spielbeginn, während der Fluss mit 2 Zeilen pro Bild neu gefüllt wird, zeichnet sie manchmal noch die zweite Zeile (1 Bild pro Neufüllung). `game_check` meldet solche Bilder getrennt und vergleicht sie nicht; das nächste Bild ist wieder identisch.
- Nicht nachgebildet: das Spielende. Die Spiellogik der Hauptschleife beginnt in Rasterzeile 160 und teilt sich die CPU mit DLI und VBI (Rasterzeile 248). Im Bild des Spielendes brauchen die Highscore-Prüfung und ihre Prüfbytes (sub $B261, $B298) tausende Takte, sodass `sub_B21A` den Bildzähler vor dem VBI auf $FF setzt, nach ihm oder (Spiel für zwei) erst im nächsten Bild, je nach den Takten von allem davor in diesem Bild. Die Portierung tut es vor dem VBI; dann kann der Bildzähler um eins abweichen, was den folgenden Demo-Modus um ein Bild verschiebt. Um das nachzubilden, bräuchte man die Taktzahl der ganzen Hauptschleife in diesem Bild (einschließlich des Interrupt-Handlers des Betriebssystems).

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/start_only.txt 9000 out -bot -seed 3
port/build/game_check rom/River_Raid.rom out/ram.bin out/random.log 38 8950 -attract
```

Sound. Der Soundtreiber (VBI B2A5–B3D8, `Game::soundDriver`) lief auf dem RAM, den das Original zu Beginn jedes VBI eines Bot-Spiels hatte (2989 Bilder, 2199 mit Sound: Motor, Treibstoffwarnung, Explosionen, Schüsse, Punkte, Panzergranate): Jeder AUDF/AUDC-Wert gleicht dem, was das Original geschrieben hat. Das POKEY-Modell (`src/pokey_sound.cpp`) wurde mit dem Sound von atari800 für dieselben Registerschreibzugriffe verglichen: dieselbe Lautstärke über die Zeit (Korrelation der Hüllkurven 0,95), dieselben reinen Töne in derselben Höhe, Rauschen mit derselben Spektrumsform (Kosinus der geglätteten Spektren 0,94); das Muster des Rauschens von Abtastwert zu Abtastwert unterscheidet sich, da es davon abhängt, wo die Rauschzähler gerade stehen.

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/start_only.txt 3000 out -bot -wav -vbi
port/build/sound_check rom/River_Raid.rom out/vbi.bin out/pokey.log out/random.log
port/build/sound_render out/pokey.log port.wav
```

## Noch nicht erledigt
- Das Einschalten selbst (A000–A17C: die ersten 38 Bilder vor dem Demo-Modus) wird als leerer Bildschirm gezeichnet; `Game::powerOn` ist noch nicht genau.
- Das PAL-Gerät (`-pal`): der Generator kennt es, Display List und Timing nicht.
- Die Farben folgen den Modellen der GTIA-Ausgabe aus den Emulatoren (standardmäßig Altirra, atari800 auf V; beide Byte für Byte gegen den Palettencode der Emulatoren selbst mit ihren Standardeinstellungen geprüft, atari800 von seinen Videopegeln 16–235 auf 0–255 gestreckt), nicht einer an einem echten Fernseher gemessenen Palette; wie hell es aussieht, hängt ebenso von den Einstellungen des Fernsehers ab wie vom Gerät.
