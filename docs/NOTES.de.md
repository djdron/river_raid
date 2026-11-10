# River Raid (Atari 8-bit) — Erkenntnisse

[English](NOTES.md) | [Русский](NOTES.ru.md) | [Українська](NOTES.uk.md) | Deutsch | [Polski](NOTES.pl.md) | [Italiano](NOTES.it.md) | [Español](NOTES.es.md) | [中文](NOTES.zh.md) | [日本語](NOTES.ja.md)

Abbild: 8192 Bytes, ohne CART-Header, wird bei $A000–$BFFF geladen. Code ~5,9 KB (2941 Befehle), Daten ~2,3 KB.

## Einsprungpunkte
| Adresse | Was | Wie gefunden |
|---|---|---|
| $A000 | Start + Hauptschleife pro Bild | Modul-Trailer ($BFFA) |
| $B2A5 | VBI (VVBLKI $0222): POKEY-Sound, Bildzähler, Register pro Bild | Schreibzugriff auf $0222/$0223 bei A0D1 |
| $B500 | DLI (VDSLST $0200): Sprite-Multiplexing pro Zeile | Schreibzugriff auf $0200/$0201 bei A0C7 |
| $B485 | IRQ-Platzhalter | Schreibzugriff auf $0216/$0217 |

## Bild
- `L_AE99`: wartet auf VCOUNT < $50 (ein neues Bild hat begonnen), bereitet Display List und Sprites vor.
- `L_A197`: wartet auf VCOUNT >= $50, liest den Joystick (PORTA, TRIG0/1), führt die Spiellogik aus.
- `A18D`: wartet auf VCOUNT = $78, schreibt dann NMIEN ($C0 = DLI+VBI, $40 = nur VBI).
- Ein Durchlauf der Schleife = ein Logikschritt, ein Modell „update() einmal pro Bild“ ist also richtig.

## Was für die Portierung wichtig ist
1. **Zufallszahlen.** `RANDOM` ($D20A, das Polynom von POKEY) wird an 3 Stellen gelesen. Die Spiellogik nur bei A205: Alle 16 Bilder wird ein Gegner-Slot (0–10) gewählt und in seinen Flags Bit $40 gesetzt. Die beiden anderen Lesezugriffe (B362, B3D0) erzeugen nur Rauschen für den Sound. Der Wert hängt vom CPU-Takt des Lesens ab (das 17-Bit-Polynom von POKEY läuft mit dem Maschinentakt). Die Portierung bildet den Zähler nach (`port/src/pokey_random.h`) und startet ihn von der Uhr, wie bei einem echten Einschalten; für den Bild-für-Bild-Vergleich stammen die Werte des Originals aus dem Referenz-Trace (`random.log`).
2. **PAL/NTSC.** Das PAL-Bit wird aus $D014 gelesen (A06A), in $BF gehalten und wählt die Variante der Display List ($B77A, in die Display List bei $3FA9 kopiert), die Tanklager-Grenze `$CB` ($2E/$2F) und Verzweigungen bei B135/B14F/B741. Nichts gleicht die Bildrate aus: Auf PAL läuft das Spiel mit 50 Bildern pro Sekunde, etwa 17 % langsamer. Eine eigene PAL-Version gab es nicht: Die britische Kassette ist derselbe Code (siehe „Britische Kassettenversion“). `-pal` der Portierung setzt die Tanklager-Grenze; PAL-Display-List und 50 Hz werden noch nicht nachgebildet.
3. **Schrift aus dem Betriebssystem.** Der Code kopiert den Zeichensatz aus dem OS-ROM ($E000/$E100/$E300) nach $0600–$09FF. Das ist eine äußere Abhängigkeit: eine eigene Schrift oder das ROM des Benutzers verwenden (Altirra OS ist frei).
4. **Kopplung an den Strahl.** Sprites werden von einem DLI-Kern mit WSYNC in jeder Zeile gezeichnet; in C++ wird daraus ein Software-Renderer, der vom Zustand des Bilds ausgeht.
5. **Sound.** Der Treiber lebt ganz im VBI ($B2A5–$B484): 4 POKEY-Kanäle, AUDF/AUDC berechnet aus $C6, $7A–$7F, $50, $24 und den Tabellen $B4AC/$B4C0/$B4E1. Diesen Treiber plus einen POKEY-Kern nachzubilden reicht für eine genaue Übereinstimmung.
6. **Codestil.** Monolithisch, Variablen werden von verschiedenen Teilsystemen wiederverwendet, Flags sind in Bits gepackt. Eine vollständige Zuordnung braucht Traces aus einem Emulator.

## Datentabellen (aus Codeverweisen)
Alle werden über benannte Zugriffsfunktionen in `port/src/rom.h` gelesen.
- $B4AC (4): Motorklang (AUDC) nach Knüppel hoch/runter; $B4C0 (17): Tonhöhe des Pfeifens der Panzergranate; $B4E1: Ton für ein zusätzliches Leben.
- $B4D1: Panzerformen (Zeigerpaare, gelesen über $A1/$A2 bei AEB9).
- $B776 (4 Bytes) wird nach $3F00 kopiert (Anfang der Display List); $B77A (15 Bytes, auf PAL die zweiten 15) nach $3FA9, der Rest der Display List.
- $BB70 (60 Bytes): Anzeigetext, nach $1000 kopiert; $BEEE (64): Activision-Schriftzug → $06C0; $BF2E (112): Anzeigesymbole → $0818; $BF9E (24): „P1“/„P2“ → $08D0; $BFB6 (48): die Ziffern von „1982, 1983“ → $07D0.
- $BAAF: nächste Panzerart; $BAC5: Granatenfarben; $BB24: nächste Objektart (Animation); $BB40: Versätze der Objektformen ab $B900.

## Referenzlauf (erledigt)
`oracle/rr_oracle` lässt das Original auf libatari800 laufen und schreibt das RAM jedes Bilds, jeden POKEY-Schreibzugriff und jeden RANDOM-Lesezugriff (siehe `oracle/README.de.md`).
- Das Spiel startet mit der START-Taste; Feuer auf dem Titelbildschirm startet es nicht.
- atari800 startet den POKEY-Generator von der Uhr; mit `-seed` sind Läufe Byte für Byte gleich.
- RANDOM-Lesezugriffe: Rasterzeile ~249 ist Sound aus dem VBI, Rasterzeilen 190–192 und 241 sind Spiellogik (A205). Für C++ heißt das, dass im Prüfmodus die Zufallswerte des Spiels aus `random.log` genommen und nicht emuliert werden.

## Levelgenerator (durch Trace bestätigt)
- Fluss und Objektplatzierung werden von einem Software-LFSR mit 24 Bit in `sub_B183` auf $AA/$AC/$AB gesteuert. Aufgerufen von A756 und A767.
- Ein Schritt: `fb = bit7(AB) ^ bit2(AB)`, dann eine Linksverschiebung der Kette AA←fb, AC←bit7(AA), AB←bit7(AC). Das Python-Modell erklärt 265 von 268 Änderungen im Trace; die anderen 3 sind Neuladungen.
- Der Startwert von AA/AB/AC ist $E3/$D4/$F7. Er wird in $66–$68 gehalten und bei A3C8 nach dem Start und nach einem Absturz wiederhergestellt (Neustart an der Brücke).
- Mit verschiedenen POKEY-Seeds über 1000 Bilder sind $AA–$AC, $66–$68, die Objekttypen ($0500…) und ihr X ($0516…) identisch. Nur die Slot-Flags $42–$4C (Bit $40 von A205), das Hilfsbyte $07 und der Stack unterscheiden sich.
- Schluss: Das Level ist immer gleich. `RANDOM` beeinflusst nur das Verhalten der Gegner (welcher Flag $40 bekommt). Früh im Spiel scheint die Bewegung der Gegner abgeschaltet (die Prüfung von $60 bei A297), daher zeigte es sich in einem kurzen Lauf nicht.

## Variablen (Stufe 2)
Status: **geprüft** heißt, die Regel wurde mit `tools/verify_trace.py` an Referenz-Traces ohne Ausnahmen geprüft. **aus dem Code** heißt, der Wert wurde aus dem Listing gelesen, ohne eigene Prüfung am Trace. **Hypothese** heißt eine noch zu bestätigende Vermutung.

### Spieler
| Adresse | Was | Status |
|---|---|---|
| $57 | X des Flugzeugs (HPOS-Einheiten; auf dem Referenzbildschirm Mitte ≈ 2·X−55 px) | geprüft: bewegt sich nur bei ausgelenktem Knüppel |
| $58 | Nachkommateil von X; bei Überlauf bewegt sich $57 um ±1 (A462–A493) | aus dem Code |
| $59 | Geschwindigkeit: 128 normal, bis ~142 mit hoch, bis ~114 mit runter, beim Loslassen zurück auf 128 | aus dem Trace, ohne eigene Prüfung |
| $5B:$5C | Treibstoff, 16 Bit, $FFFF ist ein voller Tank. Code A3E9–A453, in jedem Flugbild: über einem Tanklager `$5B += 2` (es ist `ADC #1` mit dem von `CMP #$0E` übrig gebliebenen Übertrag), begrenzt auf $FFFF; dann immer `$5C −= $20` mit Borgen aus `$5B` (−1 in $5B alle 8 Bilder). Ist $5B beim Borgen 0, stirbt das Flugzeug. Der Verbrauch pro Bild hängt nicht von der Geschwindigkeit ab. Die Nadel der Tankanzeige HPOSM1 = $5B/8 + $5C im DLI bei B527 (dort ist $5C die Konstante `#$5C`, nicht die Variable) | geprüft, einschließlich Auftanken: +480 pro Bild, begrenzt auf $FFFF |
| $1D | Slot, mit dem das Flugzeug kollidiert ist; Bit 7 = keiner | geprüft über das Auftanken |
| $7A / $7B | Soundereignis: 4 Auftanken, 3 Tank voll (A40E–A422); $7B = 8 bei einer Änderung, dann zählt der VBI herunter | aus Code und Trace |
| $6A | Reserveleben: 4 bei Spielbeginn, 3 nach dem Start, 0 beim letzten Leben | aus dem Trace |
| $76 | Lebensphase: 128 bei Spielbeginn; 49/47 Warten an der Brücke auf den Start (jede Knüppelbewegung startet); 0 Flug; 253 und dann −3 pro Bild Explosion; 130→127 Wiedereinstieg (Treibstoff auf $FFFF) | aus dem Trace |

### Punkte und Brücken
| Adresse | Was | Status |
|---|---|---|
| $63/$64/$65 | Punkte, BCD, höchstwertiges Byte zuerst, gespeichert durch 10 geteilt (950 auf dem Bildschirm = `00 00 95`); bei Überlauf 999999 und $6A = 0 (A563) | geprüft |
| $BB0B + Typ | Punkte/10 pro Objekt (BCD): 8 Hubschrauber 60, $0D Schiff 30, $0E Brücke 500, $0F Treibstoff 80, $10 Brücke mit Panzer 750; die Tabelle hat auch 7 → 100, $0A/$0B → 150 (in Traces nicht gesehen) | geprüft für 8, $0D, $0E, $0F, $10 |
| $61/$62 | Nummer der aktuellen Brücke, BCD (wie „BRIDGE n“ auf dem Bildschirm), +1 wenn eine Brücke zerstört wird (A4F0) | aus Code und Trace |
| $38 Bit 7 | der Straßenpanzer wurde mit der Brücke zerstört (A513–A517) | aus dem Code, 1 Fall im Trace |
| $39 | X des Straßenpanzers (HPOSP1 im DLI bei B54D) | aus dem Code |

Punkte gibt es auch für den Gegner, in den das Flugzeug gestürzt ist: Sein Slot geht in Zustand 3. Von einer Kugel abgeschossen geht er in Zustand 2.

### Objekt-Slots (11, Index X = 0…10)
Ein Slot ist ein waagerechter Streifen des Bildschirms, keine feste Objektnummer: Beim Scrollen wandert ein Objekt in den nächsten Slot (im Trace ging ein Tanklager durch die Slots 8 → 9 → 10). Die Kollisionsprüfung in einem Bild geschieht vor diesem Wechsel.

| Adresse | Was | Status |
|---|---|---|
| $0C+X | senkrechte Position des Objekts (+1 pro Bild bei normaler Geschwindigkeit; ≈ 240–247 beim Flugzeug) | aus dem Trace |
| $0500+X | aktuelle Art des Objekts. ≥ 7 ist ein lebendes Objekt: 7…$0D Gegner (8/9 Hubschrauberbilder, $0D Schiff), $0E Brücke, $0F Tanklager. < 7 ist leer oder explodierend: 2 von einer Kugel getroffen, 3 Kollision; 0, 1, 4–6 kommen auch vor. Bei Kollision (A3E9): 7…$0D Absturz, $0E Absturz + `INC $2E`, $0F Auftanken | geprüft (2/3 über Punkte, $0F über Auftanken) |
| $050B+X | Objekttyp (Index in die Punktetabelle): 0, 1, 8, $0D, $0E, $0F in Traces | geprüft über Punkte |
| $0516+X | X des Objekts | aus dem Code (A500–A511, verglichen mit $39) |
| $42+X | Flags: Bit 7, Bit 6 ($40, gesetzt aus RANDOM bei A205), niedere Bits | nicht analysiert |
| $4E | Index des bearbeiteten Slots (verglichen mit #$0B) | aus dem Code |

### Referenzläufe für die Zuordnung
- `oracle/scripts/bot_run_8000.txt`: vom Bot aufgezeichnete Eingabe (`rr_oracle -bot`), 8000 Bilder. Enthält 2 Brücken (eine mit Panzer), 5 Abstürze, Spielende um Bild 3300, dann den Titelbildschirm und einen neuen Start um 5323. Kein echtes Auftanken: Der Bot schießt die Tanklager ab.
- Das Wiederabspielen dieser Datei ohne den Bot ergibt Byte für Byte identische `ram.bin`, `pokey.log` und `random.log`.

- `oracle/scripts/refuel_first_depot.txt`: Flug über das erste Tanklager ohne zu schießen (32 Bilder über dem Lager, der Tank erreicht $FFFF).

## Geländeerzeugung (Code A6FD–A9D7, Zeichnen AA8C–AB4B)
Das Ufer wird nicht als Bild gespeichert, sondern als einige Zahlen pro Zeile. Jede neue Bildschirmzeile ist ein Generatorschritt, danach wird die Zeile in den Ringpuffer des Bildschirms gezeichnet (die Zeilenadresse kommt aus der Display List, $3F04/$3F05).

**Scrollen (aus dem Code).** In jedem Bild wird die Geschwindigkeit `$59` zweimal zum Akkumulator `$30` addiert, und jeder Überlauf erzeugt eine neue Zeile. Bei Geschwindigkeit 128 ist das genau 1 Zeile pro Bild; bei `$59 ≥ $FE` wird eine Zeile ohne Akkumulieren erzeugt.

**Struktur (aus dem Code).**
- `$2F` — Zeilennummer im Block, 0…31.
- `$5A` — verbleibende Blöcke im Abschnitt (2).
- `$5D` — verbleibende Abschnitte bis zur Brücke, mit 16 neu geladen; am Ende wird `sub_B194` aufgerufen.
- Das LFSR `sub_B183` macht in jeder Zeile einen Schritt, außer in Zeilen mit gesetztem Bit 7 in `$23` (gerade Brückenanfahrten).
- An einer Abschnittsgrenze wird ein neues Breitenziel `$5F` aus dem niederen Nibble von `$AA` genommen, mit Grenzen (Minimum 2, Maximum 15/14/8 je nach `$2D`, `$60`, `$B5`), dann `$34 = $5F·4`.

**Zahlen pro Zeile.**
| Adresse | Was | Status |
|---|---|---|
| $52 | Uferposition | geprüft (Zufallsweg, unten) |
| $C3 | Wanderrichtung: +1, $FF (−1) oder 0 | geprüft |
| $54 | halbe Breite der Insel / des inneren Teils | geprüft (Glättung) |
| $53 | Wasserbreite; ändert sich um −2 × den Schritt von `$54`, die Gesamtbreite bleibt also erhalten | geprüft |
| $34, $35 | Ziel für `$54` und der Schritt dorthin (±1, wenn die Differenz < 32 ist, sonst ±2) | geprüft |
| $2D | Bit 7 — Inselmodus (ändert das Zeichnen), Bit 6 | aus dem Code |
| $C4, $C5 | Grenzen von `$52`: $1C…$24 bei `$60 < 3`, sonst weiter (1…$40 mit Varianten nach `$60`, `$052D`, `$36`) | aus dem Code, 28…36 in Traces gesehen |
| $23 Bit 7 | nahe der Brücke folgt das Ufer der Tabelle `$BA8F[$2F] + $52`, ohne Zufall | geprüft über C++ (`terrain_check`) |
| $052D | ≠ 0: das Ufer wird in den Korridor $18…$28 gezogen | geprüft über C++ (`terrain_check`) |

**Zufallsweg des Ufers (A84F–A96A), geprüft.** Jede Zeile nimmt das niedere Nibble `n = $AC & 15` (das LFSR-Byte nach dem Schritt dieser Zeile):
- ist `$C3 = 0`, beginnt die Bewegung: +1 bei `n ≥ 8`, sonst −1;
- bewegt sich das Ufer schon: bei `n < 8` hält es; bei `n = $D` kehrt es um und macht einen Schritt; bei `n = $F` hält es, wenn `$0E` (`$0D` auf einer Insel) kleiner als `$5F` oder `$2C` ist; bei jedem anderen Wert macht es einen Schritt;
- ein Schritt, der die Grenzen `$C4/$C5` überschreiten oder in das Objekt im aktuellen Slot `$4E` laufen würde (Typ ≥ 8; die Grenze verwendet `$54/$34`, `$0516+X`, `$B5`, `$0521+X`), wird durch Umkehren an Ort und Stelle ersetzt.

Feinheit beim Timing: Die Slots und `$4E` werden so genommen, wie sie am Anfang des Bilds sind, `$54/$34/$2D/$B5/$37` dagegen mit den in diesem Bild schon neu berechneten Werten. Mit dieser Regel stimmt das Modell auf allen 4669 Zeilen mit den Traces überein.

**Glättung der Insel (A7FB–A824), geprüft.** Solange `$35 ≠ 0`, macht jede Zeile `$54 += $35` und `$53 −= 2·$35`. Erreicht `$54` den Wert `$34`, wird der Schritt `$35` gelöscht. Geprüft an 4825 Zeilen, von denen sich 1195 tatsächlich änderten.

**Verzierungen (Häuser, Berge).** Beginnen am Anfang eines Blocks (`$2F = 0`), wenn Bit 5 von `$AC` gesetzt ist. Versatz, Breite und Zeilenzahl kommen aus den Tabellen `$BBCC`, `$BBD4`, `$BBB2`, `$BBEB` nach `$AA` und `$5F`, der Bitmap-Zeiger aus `$BBF0`. Geprüft über C++.

**Objekte.** Alle 16 Zeilen wird ein neues Objekt in den obersten freien Slot gesetzt (`$4D`−1); der Typ wird nach `$AB` über die Tabelle `$BB1C` gewählt, die Sprite-Breite aus `$BBA2` (nach `$0521+X`, das ist `SIZEP3`: 1 für das Schiff, 3 für die Brücke, sonst 0), die Zeilenzahl aus `$BB60` (+4). Solange das Objekt noch hereingezeichnet wird (`$32` Zeilen), weicht das Ufer diesem Objekt aus (`$4E`). Geprüft über C++.

**Brücken.** Der Generator zeichnet dort, wo die Brücke ist, nur einen geraden Kanal (Ufer aus der Tabelle `$BA8F`) und setzt ein Objekt vom Typ `$0E` in einen Slot mit Flag `$80/$84` in `$42+X` (eine Kopie von `$23`). Die Brücke selbst erscheint erst beim Anzeigen, im DLI (B575–B5FC): Für einen Slot mit Bit 7 in seinen Flags kommt die Uferfarbe `COLPF0` in jeder Zeile aus der Tabelle `$B4E5` (`04 04 04 04 08×6 1C 1C 06×5 04 04 04`: graue Straßenbänder und die gelbe Markierung), sodass die Straße über den ganzen Bildschirm über die Uferpixel läuft; der Teil über dem Wasser ist das P3-Sprite dieses Slots in vierfacher Breite, wie bei Schiffen und Hubschraubern. Nach dem Objekt wird die Uferfarbe aus `$0B` wiederhergestellt (`$D2`/`$D6`: Das Ufer wechselt je nach Abschnittsparität zwischen zwei Grüntönen). Der Straßenpanzer ist Sprite P1 (`$39`). Die Bitmap-Zeilen einer Brücke ändern sich nach der Erzeugung nicht (geprüft an 119 Zeilen).

**Farben und Sprites pro Zeile (geprüft über C++, `port/src/river_draw.cpp`).** Die Grundfarben kommen aus dem vertikalen Strahlrücklauf (B408–B43E): Wasser `COLPF2` = $84 ($42 während des Blitzens), Rand `COLPF1` = $20, Ufer `COLPF0` aus `sub_AFD0`: `($23 & 4) ^ $D6`, mit umgedrehtem Bit 2, solange das neueste Objekt (`$4E`) eine Brücke mit y < $5C ist. Der DLI geht die Slots ab `$4D` aufwärts mit einem Rasterzeilenzähler durch (Wert $5D in Zeile 20, wo neue Zeilen hereinkommen): Der erste Slot mit `0 ≤ y + $BB60[Typ] − Zähler ≤ $BB60[Typ]` ergibt die Zeile r; die Form ist `$B900 + $BB40[Art (+16 bei Flag-Bit 3)] + r`, die Farbe `$B700 + $BB30[Typ] + r`, HPOSP3 = `$0516+X` (32 = linker Rand des Puffers), SIZEP3 = `$0521+X`; für einen Brücken-Slot COLPF0 = `$B4E5 + r`. Verglichen mit Zeile 20 des Originals: 96,8 % der Zeilen identisch mit den Generatorpositionen, 98,1 % mit den Objektpositionen des Spiels, alle übrigen Zeilen, solange der Straßenpanzer aktiv ist.

**Demo-Modus und der untere Bildschirmrand (geprüft über C++, `port/src/status_bar.cpp`).** Ohne START lässt das Spiel ab dem Einschalten den Fluss von Spiel 1 laufen: Die Laufschrift bewegt sich ab Bild 11, der Fluss ab Bild 38, 1 Zeile pro Bild, durch die Brücken, mit demselben Generator (der Streifen von Zeile 20 stimmt auf 19 960 Bildern mit C++ überein, bis auf bewegte Objekte). Display List bei $3F00: 24 Leerzeilen, 160 Zeilen in Modus E, dann die Anzeige: 3 Zeilen in Modus 6 (normale Breite, `CHBASE` $08, Farben $1E/$8E/$1E/$00 auf $06, `PRIOR` 4) ab $1000, aus `$BB70` aufgebaut; die Tanknadel ist Missile 1 bei `$5B/8 + $5C`. Letzte Zeile: Modus 6 breit mit `HSCROL`, `CHBASE` $06, Farbe $0C, Text bei `$B800 + $3FB3`; der VBI (B44B) schaltet `HSCROL` 7→0 und dann den Text um ein Zeichen weiter ($4B…$D6, wiederholend), solange `$2A` ≠ 0, und bewegt das Logo `$27` mit. Das Logo ist Player 3 in vierfacher Breite (Daten $FC) plus Player 2 ($FF), pro Zeile um einen Takt nach links versetzt, in den Farben `$B48D`. Die Zeichensätze bei $0600/$0800 sind die Schrift des Betriebssystems (A0FB) mit den Zeichen des Moduls bei $06C0, $0818, $08D0, $07D0 (die Ziffern von „1982, 1983“).

**Objektbewegung, Panzer, Raketen (geprüft über C++, `port/src/game.cpp`, `display.cpp`).** Alle 16 Bilder bekommt ein durch `RANDOM & 15` gewählter Slot (11–15 → 6–10) Bit 6 in `$42+X` („wach“), sofern es nicht das Objekt ist, das gerade noch hereingezeichnet wird. Objekte der Art 7 (Düsenjäger) bewegen sich in jedem Bild; die Arten 8–13 nur, wenn wach, ab Brücke 2 (`$60 >> 1 ≠ 0`), in abwechselnden Bildern nach Slot-Parität (`X ^ $18 ^ $33`), Art $0C nur in Bildern mit `$18 & 2 = 0`; 1 Takt pro Schritt, Bit 3 = links, Umlauf $2C…$D4. Bit 0 von `$42+X` setzt der DLI, wenn das Sprite des Objekts auf dem Bildschirm die Uferrandfarbe berührt hat (P3PF Bit 1); der nächste Schritt kehrt die Richtung dann einmal um (Bit 4 markiert die Umkehr, bis die Berührung endet). Arten unter $0C werden alle 8 Bilder über `$BB24` animiert (alle 2 bei Arten 7+). Der Panzer (`$38`–`$3A`) fährt alle 16 Bilder (4 bei Arten 4–7), bis Player 1 die Randfarbe berührt, dann (`$41` Bit 7) feuert er: Die Granate ist Player 0, `$50` zählt 0…$38 (Flugtabelle `$B4AF`, Geschwindigkeit `$3E/$3F`), dann Explosionsbilder. Ein Hubschrauber der Art $0A/$0B in Slot `$51` feuert Missile 0 (`$C1` x, `$C2` y, Schritt `$C0` = ±2), bis sie das Ufer trifft (M0PF). Die Kollisionsregister des DLI werden nach jedem Objekt gelesen und gelöscht, sodass die Kollisionen pro Slot bekannt sind (`$1A`–`$22` halten die Slotnummer). Mit wiedergegebenem RANDOM stimmt C++ auf 19 900 Demo-Bildern ab dem Einschalten mit dem RAM des Originals überein.

**Soundtreiber (geprüft über C++, `Game::soundDriver`).** Einmal pro Bild im VBI (B2A5–B3D8), AUDCTL bleibt 0 (Takt 64 kHz, 17-Bit-Rauschen). `$C6` ≠ 0: Startpiepton auf Kanal 1 ($40/$AF), der Rest aus. Kanal 1: Soundereignis `$7A` 1–4 für `$7B` Bilder (Tonhöhe $39/$11/$1F/$3F, Lautstärke aus `$7B`); sonst, wenn das Flugzeug fliegt, die Treibstoffwarnung bei `$5B` < $40 (ein Piepton alle 63 Bilder, ein fallender Ton unter $04) oder der Motor: AUDF = $1F − `$59`/16 (oder das fallende `$052C` nach einem Absturz), AUDC aus den Hoch/Runter-Bits des Knüppels (`$B4AC`: $84 normal, $83/$86). Kanal 2: Explosionsrauschen `$7C` (AUDF = RANDOM | $18) oder während des Spiels die Hubschrauberrakete `$7F`. Kanal 3: Schuss `$7D` oder der Punkteton `$7E` (`$B4E1`). Kanal 4: die Panzergranate `$50` (`$B4C0` im Flug, dann Rauschen). Der Demo-Modus ist stumm: `$76` ≠ 0 schaltet Kanal 1 ab, und sonst wird nichts ausgelöst.

**Das Spiel mit einem Spieler (geprüft über C++, `port/src/game.cpp`).** `$76` ist der Zustand des Flugzeugs: 0 Flug; $FE…$80 Explosion (126 Bilder, `$5E` Flugzeugbild alle 8 herunter); dann $7F: Sind Leben übrig, wird der Fluss zurückgesetzt (`sub_B0F1` mit X = $12, das Bildschirm und Player-Missile-Speicher 13 Bilder lang löscht, während nur Interrupts laufen) und bei `$59` = $FE (2 Zeilen pro Bild) neu gefüllt, während `$76` von $7F bis $30 zählt; bei $30 wird ein Leben abgezogen und `$052C` = $80 (der steigende Motorton); bei $2F wartet das Flugzeug an der Brücke ohne Scrollen, bis eine Knüppel- oder Feuereingabe `$76` = 0 und `$59` = $40 setzt. Ohne Leben: Spielende (Highscore `sub_B261` mit Prüfbytes `$90`–`$9F`, `sub_B21A`: `$76` = `$2A` = $FF, „Game over“ in der Laufschrift). Im Flug (A3E9–A5B4): Kollision des Flugzeugs mit einem Objekt (`$1D`): Arten 7–$0E Absturz (eine Brücke zählt auch `$2E`), Art $0F tankt auf (`$5B` + 2 pro Bild, Töne `$7A` 4/3); Granatenexplosion (`$1E` mit `$50` ≥ $11), Hubschrauberrakete (`$21`) oder das Ufer (`$1B`) Absturz; Treibstoff verbrennt $20/256 pro Bild; die Seitwärtsbewegung beschleunigt (`$2B` += 8, Bruchteil `$58`), Bilder 7/8/9; Geschwindigkeit `$59` ±2 zwischen $40 und $FE, zurück auf $80 bei zentriertem Knüppel; die Kugel (Missile 2, 8 Zeilen ab `$56`, 6 Zeilen pro Bild aufwärts) zerstört Objekte der Art ≥ 7 (`$1C`), die Punktetabelle `$BB0B` nach Typ, ein Leben alle 10 000 Punkte (bis 9). Eine zerstörte Brücke zählt `$61/$62`, nimmt einen darauf stehenden Panzer mit und lässt ab Brücke 5 einen Panzer in der Nähe feuern. `sub_B1C0` entscheidet, wo das nächste Flugzeug beginnt: hinter einer zerstörten Brücke ab dem nächsten Abschnitt, sonst einen Kontrollpunkt zurück. Spiele für zwei (ungerades `$09`) tauschen `$60`–`$6A` mit `$6B`–`$75` (`sub_B247`) nach jedem Flugzeug; Spieler 2 benutzt den zweiten Joystick (oberes Nibble von PORTA). START (A673) beginnt ein neues Spiel (B0F1 mit X = $29, 13 Bilder); SELECT/OPTION alle 30 Bilder das nächste Spiel; jede Taste der Tastatur pausiert (`$24`: 3 → 2 Pause → 1 → 0).

**Zeichnen (geprüft über C++).** Eine Zeile sind 48 Bytes Bitmap (ANTIC-Modus E, 4 Pixel pro Byte), aufgebaut aus Füllläufen (Unterprogramme `AFEC`/`AFF0`/`AFF4`: `$AA`, `$FF`, `$55`) und 2-Byte-Randstücken aus der Tabelle bei `$B800` (`B034`/`B03B`): Ufer, Wasser, Insel (mit Bit 7 von `$2D`), Wasser, Ufer. Die Pixelposition geteilt durch 4 ist die Bytenummer, die unteren 2 Bits wählen das Randstück. Eine Zeile der Verzierung kopiert `B01A`. Die Ufer sind nicht gespiegelt: Der rechte Rand wird aus dem linken über Wasser- und Inselbreite berechnet, deshalb mäandert der Fluss. Auf 2200 Zeilen aus C++: Wenn sich beide Ränder bewegen, bewegen sie sich in 810 von 944 Fällen gleich; die Flussmitte wandert zwischen 71 und 122 Pixeln.

## C++-Generator (`port/`)
- Die gesamte Verarbeitung pro Zeile (A711–AE96 und Unterprogramme) ist nach `port/src/river_gen.cpp` portiert. Die Prüfung `port/tools/terrain_check` vergleicht jedes Generatorfeld und die 48 Bytes jeder neuen Bildschirmzeile mit dem Original.
- Ergebnis: 119 758 Zeilen des ganzen Flusses ohne Unterschiede; ein Start von Grund auf (`startGame`) stimmt ebenfalls.
- Der Generator hängt weder vom Spieler (Bot gegen Flug ohne Eingabe) noch von `RANDOM` ab (3 Seeds, 120 000 Bilder, Byte für Byte gleich einschließlich des Bildschirmstreifens).
- Bildschirm: Bitmap im ANTIC-Modus E, 48 Bytes pro Zeile (192 Pixel, 4 pro Byte), Ringpuffer, Adresse der neuen Zeile in `$3F04/$3F05`, −$30 pro Zeile. Pixelwerte: 1 Ufer, 3 Wasser, 2 Rand, 0 selten (innerhalb von Randstücken).
- Spiele 1–8: `$09` wählt eine Zeile der Tabellen `$B494`–`$B4A8`: LFSR, Ufer und Startabschnitt. Spiele 1–2 beginnen an Brücke 1, 3–4 an 5, 5–6 an 20, 7–8 an 50. Das sind Kontrollpunkte desselben Flusses: C++ erreicht Brücke 5/20/50 mit genau den Tabellenwerten (`$B4A4`: Level, `$B4A8`: Brückenzähler in BCD). Die Portierung baut dieselbe Tabelle, indem sie den Generator laufen lässt, für `-seed N` (der Fluss von Spiel 1 ab LFSR-Zustand N; der Fluss des Moduls ist $F7D4E3 = 16241891) und für `-bridge N` (jedes Spiel ab Brücke N, 1–9999; nach 80 geht das Level 79, 80, 79 ...).
- `$CB` = $2E auf NTSC und $2F auf PAL (A077, aus dem PAL-Register); es beeinflusst die Tanklager-Wahrscheinlichkeit am Blockanfang (AC48). Der Fluss unterscheidet sich zwischen PAL und NTSC leicht.
- Start und Wiedereinstieg: `sub_B0F1` lädt $4D–$5F aus `$BAE1`, löscht $2B–$4C und $0500–$05FF, setzt die Startbrücke in Slot 10 (Typ $0E, y=$4A, Höhe $17), Modus $23 = $84/$80. Dann stellt A3C8 LFSR und Ufer aus dem Kontrollpunkt $66–$69 wieder her. Danach erzeugt das Spiel 160–165 Zeilen mit 2 pro Bild (Geschwindigkeit $FE), um den Bildschirm zu füllen.
- Eine Falle bei der Überprüfung: Während der Bildschirm mit 2 Zeilen pro Bild gefüllt wird, kann die Bildgrenze des Emulators zwischen zwei Zeilen fallen, daher ist der Bild-für-Bild-Vergleich in diesen Bildern falsch. Im normalen Spiel (eine Zeile pro Bild) liegt der Schnappschuss immer auf einer Zeilengrenze.

## Levelkarte
- Der Ufergenerator hängt nicht vom Flugzeug ab: Das Ausweichen vor Objekten schaut auf den Typ `$050B`, der sich nicht ändert, wenn ein Objekt abgeschossen wird. `$60` (Abschnittsnummer) steigt alle 1024 Zeilen, wenn die Brücke erzeugt wird, nicht wenn sie zerstört wird (im Bot-Spiel änderte sich `$60` in Bild 1133, die Brücke wurde in 1177 zerstört).
- Deshalb lässt sich das ganze Level mit einem unverwundbaren Flugzeug filmen: `rr_oracle -ghost` patcht 5 Bytes im emulierten Speicher (A3E9/A424/A42E/A432 `LDX Flag` → `LDX #$FF`, A443 Treibstoffverbrauch $20 → 0). Keine Abstürze in 120 000 Bildern.
- Bei normaler Geschwindigkeit bewegt sich das Level genau 1 Bildschirmzeile pro Bild, und die neue Zeile erscheint in Bildschirmzeile 20. Ein Streifen der Zeile 20 aus jedem Bild stimmt bis auf bewegte Objekte mit dem laufenden Bildschirm überein.
- `$60` steigt bis $50 (80), dann wechselt es zwischen $4F/$50; der LFSR-Zustand an Abschnittsgrenzen wiederholte sich über 117 Abschnitte nicht, der Fluss läuft also nicht im Kreis, aber die Schwierigkeit steigt nach Abschnitt 80 nicht mehr.
- Erstellen der Bilder pro Abschnitt: `tools/level_map.py`. Der Betrachter ist als Seite „Карта реки River Raid“ veröffentlicht.

## Methode
Eine Regel wird zuerst aus dem Listing gelesen und dann an einem Referenz-Trace geprüft (`tools/verify_trace.py`). Passt der Trace nicht, wird nach dem Lesefehler gesucht. Beispiel: Das Auftanken wurde aus dem Code A3E9–A453 abgeleitet (einschließlich des nicht offensichtlichen +2 statt +1 wegen des Übertragsflags) und am Trace bestätigt; die einzige Abweichung zeigte, dass die Kollisionsprüfung geschieht, bevor Objekte zwischen Slots wechseln.

## Britische Kassettenversion
`River Raid.cas` (UK, 600 Baud): zwei Dateien. Die Bootdatei (7 Blöcke bei $0A00, Start $0A06) prüft auf 24K RAM („NOT ENOUGH RAM MEMORY - NEED 24K“), zeigt einen Titelbildschirm, lädt $2200 Bytes nach $1000 und springt dann nach $3000. Das ist der Code des Moduls, verschoben, Befehl für Befehl verglichen: ab A21B sind alle gut 2 900 Befehle dieselben Operationen; Code und Daten von $A000 nach $1000 verschoben, die RAM-Seiten des Spiels um $2000 nach oben (Display List $3F00 → $5F00, Bildschirmring $2000–$3FFF → $4000–$5FFF, $1000 → $3F00), Zero-Page-Variablen meist +2 ($25 → $A1, $28 → $A2); Zeiger in den Tabellen auf dieselbe Weise verschoben. Echte Unterschiede:
- die Initialisierung (A000–A0EE) nach $3000 verschoben, mit einer Prüfsumme des geladenen Codes in $E4/$E5;
- Kopierschutz: Stimmt die Prüfsumme nicht oder steht ein `D:`-Handler in HATABS (von DOS gestartet), hängt die Hauptschleife oder springt in die Mitte eines Befehls. Der eigene Schutz des Moduls (A151–A158) schreibt $FF in seinen eigenen Code bei $A19B, wenn $6000 nicht $47 enthält: im ROM harmlos, in einer RAM-Kopie tödlich;
- die Laufschrift sagt „1982, 1983, 1984“: ein zusätzliches Zeichen „4“ (nach $07C0 kopiert), der Text 5 Zeichen länger, Scroll-Anfang/-Ende $20/$4B → $1B/$46.

Generator, Spiellogik, Sound und Tabellen sind identisch; ebenso die PAL-Prüfung.

## Determinismus und Sprünge
- **Der Fluss hängt nur von sich selbst ab.** Der Generator hängt weder vom Spieler noch von RANDOM noch von den Objekten ab: Das Ausweichen schaut auf den Typ eines Objekts (`$050B`) und das X des Objekts, das gerade hereingezeichnet wird und sich noch nicht bewegt. Das LFSR macht einen Schritt pro Zeile außer auf den geraden Anfahrten zu einer Brücke: genau 993 Schritte von Brücke zu Brücke. Seine Periode ab dem Start von Spiel 1 sind 16 766 977 Schritte, und ggT(993, 16 766 977) = 1, also kehrt der Zustand an einer Brücke erst nach 16 766 977 Brücken zurück (mit der Parität des Levels, 79/80, vielleicht nach doppelt so vielen; nicht durchgelaufen): etwa 9 Jahre Flug ohne Halt. Der Brückenzähler der Anzeige hat 4 Stellen: nach 9999 zeigt er nichts (0000) und zählt ab 1 weiter.
- **Level.** In sub $B194 (`sectionCheckpoint`) wird ein Level ab $50 vor dem Erhöhen um 1 als $4E genommen, daher wechselt das Level nach 80 zwischen 79/80. Ungerade und gerade Abschnitte unterscheiden sich (ungerade: Breitencode immer 7, Objekte irgendwo quer; gerade: zufällige Breite, Inseln), der Wechsel erhält beide Arten; die Schwierigkeit steigt nicht mehr.
- **Kontrollpunkte.** An jeder Brücke speichert der Generator LFSR und Ufer. Ein Start an Brücke N (`-bridge N` im Spiel, ein Wiedereinstieg) stellt sie wieder her: Der Fluss ist von dort an derselbe wie bei einem Flug ohne Halt, die Objekte nicht (alle frisch platziert, keins geweckt, keins bewegt).
- **RANDOM.** Ein 17-Bit-Zähler, der in jedem Maschinentakt einen Schritt durch einen festen Kreis von 131 071 Zuständen macht. Die Portierung hält seine Position in diesem Kreis (`src/pokey_random.h`), daher ist der Wert zu jedem Takt sofort bekannt. Welcher Gegner aufwacht, hängt also von der Zahl der Bilder seit dem Einschalten ab.
- **Objekte leben nur, solange sie auf dem Bildschirm sind.** Der Generator setzt ein Objekt oben ein, und es ist weg, wenn es unten hinausgescrollt ist, etwa 200 Bilder später; ebenso der Panzer (prepare: `tankY` ≥ $BC) und seine Granate. Daher folgt der ganze Zustand zu jedem Zeitpunkt aus dem Fluss zu diesem Zeitpunkt plus den letzten ~200 Bildern. `Game::skipFrame` macht ein Bild „nur Fluss“: das Scrollen, die Bildzähler, den Timer des Demo-Modus, die Laufschrift und das, was prepare() mit dem Zustand des Flusses macht (Panzer und Granate verschwinden unter dem Bildschirm; ohne das hält ein alter Panzer den Generator davon ab, den nächsten zu setzen). Danach ganze Bilder. Geprüft gegen den Flug ganz von vorn (4 Flüsse und Starts mit `-bridge`, je 77 Punkte, jeweils die 6000 Bilder danach): ab 200 ganzen Bildern wird jedes Bild gleich gezeichnet und der ganze Zustand ist gleich. Variablen eines inaktiven Panzers, einer Granate oder Hubschrauberrakete können andere alte Werte enthalten; sie werden vor der Verwendung neu gesetzt und nicht gezeichnet.
- **Im Demo-Modus zerstört nichts etwas.** Die Explosion der Panzergranate zerstört ein Objekt nur im Flugcode des Flugzeugs (A4C1), der ohne Flugzeug nicht läuft.
- **Bildschirmschoner.** Nach 128 × 2048 Bildern (etwa 73 Minuten) ohne Eingabe Bit 7 in $78: Die Hauptschleife hält an (A1F5) und der Bildschirm wird schwarz, bis zu einem Joystick, einer Konsolentaste oder einer Taste der Tastatur. Das Spiel behält das bei; der Betrachter löscht $78 in jedem Bild.
- **Spulen im Betrachter** (`viewer/river_view.cpp`). Anker (Zustand des Flusses und die Zähler, ~170 Bytes) alle 1024 Bilder der Gegenwart; ganze Schnappschüsse (~16 KB) alle 128 Bilder, nur für die letzten 8192 Bilder vor der Gegenwart und um die Stelle der Wiederholung (64). Zeile L wird vom nächsten Schnappschuss oder von einem Anker aus gezeigt: nur der Fluss, dann 400 ganze Bilder (512 mehr, wenn danach zurückgespult wird). B findet die nächste Brücke allein mit dem Generator und fliegt ebenso dorthin; `-bridge N` fliegt vom Anfang des Flusses. Speicher: ~190 Bytes pro Brücke plus feste ~2 MB.

## Nächste Schritte
Nichts Offenes in der Analyse. Für die Portierung: die genaue Einschaltsequenz (A000–A17C) und das PAL-Timing (50 Hz, die PAL-Display-List).
