# rr_oracle — Referenzlauf für River Raid

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | Deutsch | [Polski](README.pl.md) | [Italiano](README.it.md) | [Español](README.es.md) | [中文](README.zh.md) | [日本語](README.ja.md)

Lässt das Originalmodul ohne Fenster auf libatari800 laufen (Atari 800, standardmäßig NTSC, eingebautes AltirraOS, BASIC aus). Dient dazu, die C++-Portierung Bild für Bild mit dem Original zu vergleichen.

## Bauen
```
./build.sh
```
Das Skript klont atari800 (geprüfter Commit 98cc2dc), baut `libatari800.a` und linkt `rr_oracle`. Braucht git, einen C-Compiler (gcc oder clang), make, autoconf und automake; funktioniert unter Linux, macOS (Xcode Command Line Tools, `brew install autoconf automake`), WSL und MSYS2. Der Referenzlauf fängt drei Emulatorfunktionen ab (POKEY-Schreibzugriffe, RANDOM-Lesezugriffe, das NMI des vertikalen Strahlrücklaufs): Das Skript kompiliert `pokey.c` und `cpu.c` ein zweites Mal mit umbenannten Funktionen, sodass keine Linker-Tricks nötig sind.

## Ausführen
```
./rr_oracle <rom> <script> <frames> <outdir> [-pal] [-seed N] [-bot|-bot-nofire] [-ghost] [-noram] [-ram A:B] [-strip Y] [-screens A:B] [-shot N,N,...]
./rr_oracle ../rom/River_Raid.rom scripts/start_and_fly.txt 1000 out -shot 240,500
./rr_oracle ../rom/River_Raid.rom scripts/start_only.txt 8000 out -bot      # bot
./rr_oracle ../rom/River_Raid.rom scripts/bot_run_8000.txt 8000 out         # replay of a bot run
```

## Bot (`-bot`)
Zuerst spielt das Skript (zum Beispiel `start_only.txt`: Einschalten, START, Warten), dann übernimmt der Autopilot. Er steuert auf die Mitte des Wassers voraus zu, weicht nahen Objekten aus, feuert ständig und drückt „hoch“, während er explodiert oder an der Brücke wartet. Der Bot ist einfach und nur für lange Traces gedacht, nicht für Highscores: Er sucht keinen Treibstoff und stirbt an Hubschraubern, die von der Seite kommen.

`-bot-nofire`: derselbe Autopilot ohne Feuern und Ausweichen.

Die in einem Lauf tatsächlich angewandten Eingaben werden im Skriptformat in `inputs.txt` geschrieben. Gibt man sie ohne `-bot` wieder ein, entsteht ein Byte für Byte identischer Trace (geprüft an 8000 Bildern).

## Optionen zum Filmen des Levels
- `-ghost` — unverwundbares Flugzeug (Code-Patch von 5 Bytes im emulierten Speicher, nur zum Filmen).
- `-strip Y` — Bildschirmzeile Y jedes Bilds nach `strip.bin`, die Palette nach `palette.bin`.
- `-screens A:B` — vollständige Bildschirme der Bilder A…B nach `screens.bin`.
- `-noram` — kein `ram.bin` schreiben (für lange Läufe).
- `-ram A:B` — `ram.bin` nur für die Bilder A…B schreiben (Bild A bei Offset 0).
- `-wav` — die Soundausgabe des Emulators nach `sound.wav`.
- `-vbi` — `vbi.bin`: das RAM zu Beginn jedes Vertical-Blank-Interrupts (Datensätze aus 4 Bytes Bildnummer + 16 384 Bytes), der Zustand, den der Soundtreiber sieht.
- `frames.csv` hat die Spalten `$60, $2F, $5D, $AA, $AB, $AC, $76, $52, $53, $54, $2D, $4E`.

## Skripte
| Datei | Was |
|---|---|
| `start_and_fly.txt` | START, Beschleunigen, Kurven, Feuern (1000 Bilder) |
| `attract_fire_only.txt` | Feuer auf dem Titelbildschirm (das Spiel startet nicht) |
| `start_only.txt` | Einschalten, START, Warten; für `-bot` |
| `probe_lr.txt`, `probe_hold.txt` | kurzes Tippen / Halten zum Auffinden von Variablen |
| `bot_run_8000.txt` | aufgezeichneter Bot-Lauf: 2 Brücken, 5 Abstürze, Spielende, neuer Start |
| `refuel_first_depot.txt` | Flug über das erste Tanklager ohne zu feuern (Auftanken) |
| `attract_idle.txt` | Einschalten und keine Eingabe: der Demo-Modus (Fluss von Spiel 1, Laufschrift mit den Mitwirkenden) |
| `two_players.txt` | SELECT auf Spiel 2, START; beide Spieler fliegen geradeaus und feuern, bis alle Flugzeuge verloren sind; zweimal die Pausetaste |
| `ghost_film.txt` | Start und Flug ohne Eingabe; mit `-ghost -noram -strip 20` filmt es die Levelkarte |

## Eingabeskript
Jede Zeile: `<Bilder> <Joystick> <Feuer> [start] [select] [option] [Joystick2] [Feuer2] [Taste]`; `#` beginnt einen Kommentar. Die letzte Zeile wiederholt sich bis zum Ende des Laufs. `Taste` hält die Leertaste (jede Taste pausiert River Raid).
Joystick-Bits: 1 hoch, 2 runter, 4 links, 8 rechts, 0 Mitte.
Das Spiel startet mit der START-Taste; Feuer auf dem Titelbildschirm startet es nicht.

## Was nach `<outdir>` kommt
| Datei | Inhalt |
|---|---|
| `ram.bin` | $0000–$3FFF (16 KB) nach jedem Bild; Bild N steht bei Offset N*16384 |
| `pokey.log` | jeder POKEY-Schreibzugriff: `Bild Rasterzeile Takt Register Wert` |
| `random.log` | jeder RANDOM-Lesezugriff: `Bild Rasterzeile Takt Wert`. Rasterzeile ~249 ist Sound (VBI), andere Rasterzeilen sind Spiellogik (A205) |
| `frames.csv` | angewandte Eingabe (Joystick, Feuer, START, dann nach den CPU-Spalten SELECT, OPTION, Joystick 2, Feuer 2, Taste) und CPU-Register am Ende des Bilds |
| `inputs.txt` | angewandte Eingabe im Skriptformat (zum Wiederabspielen) |
| `fNNNNN.ppm` | Bildschirmfotos 384×240 |

## Determinismus
atari800 selbst startet den POKEY-Generator von der Systemuhr, daher sind Läufe nicht wiederholbar. `rr_oracle` setzt den Zähler ausdrücklich (`-seed`, Standard 0). Mit demselben ROM, Skript und Seed sind alle Dateien Byte für Byte identisch (geprüft).

Lizenz: libatari800 steht unter GPL-2.0+, also auch `rr_oracle` als damit gelinktes Programm. Es ist ein Entwicklungswerkzeug: Die C++-Portierung linkt nicht damit und liest nur seine Ausgabedateien.
