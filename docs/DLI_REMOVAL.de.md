# Experimente: das Spiel ohne das DLI-Modell

[English](DLI_REMOVAL.md) | [Русский](DLI_REMOVAL.ru.md) | [Українська](DLI_REMOVAL.uk.md) | Deutsch

Branches `dli_removal` und `dli_removed`. Ziel ist eine einfachere Anzeige für Portierungen auf
langsame Maschinen (ZX Spectrum Next, TS-Conf): Sprites als fertige Bilder an ihren Positionen
gezeichnet, Kollisionen pro Objekt berechnet, kein zeilenweises Modell des Display-List-Interrupts
(DLI). In `dli_removal` bleibt das Spiel Bild für Bild dasselbe wie das Original, und jeder Schritt
wird gegen das Original geprüft (`rr_oracle` auf libatari800, `game_check`). `dli_removed` geht
einen Schritt weiter: gar kein DLI, die Kollisionen als einfache Geometrie, die auf einigen Bildern
von tausend vom Original abweicht (der letzte Teil, „Branch `dli_removed`“). Die ZX-Ports
([river_raid_zx](https://github.com/djdron/river_raid_zx)) folgen `dli_removed`.

## Schritt 1: Kollisionen mit dem Ufer an Zeilen ohne Dekoration

Der DLI liest die Playfield-Kollisionen von: dem Flugzeug (P2PF, nur die Uferfarbe), den Objekten
(P3PF, nur die Kantenfarbe: Schiffe und Hubschrauber wenden an ihr), dem Panzer (P1PF, die Kante),
der Kugel und der Rakete des Hubschraubers (M2PF, M0PF: Ufer oder Kante). Häuser und Straßen (die
Dekoration, `decoRow`) werden in der Kantenfarbe und der Hintergrundfarbe im Ufer gezeichnet.

`RiverGen::nextLine(line, bank)` liefert eine zweite Zeile, dieselbe ohne Dekoration (dort, wo sie
gezeichnet ist, die Uferfüllung); `Game::bankLines` hält sie neben `screen`, und die Kollisionen
schauen auf sie (`bankRuns`, standardmäßig an; `RR_BANK_RUNS=0` der Bildschirm wie zuvor). Ohne
Dekoration hat jede Zeile des Flusses eine von vier Formen (480 000 Zeilen der Spiele 1–8):

| Form (Farbläufe) | Zeilen | Grenzen |
|---|---|---|
| Ufer, Kante, Wasser, Kante, Ufer | 398 644 | 4 |
| dasselbe mit einer Insel | 80 324 | 8 |
| mit einer schmalen Insel (nur Kantenfarbe) | 1 016 | 6 |
| die Zeilen einer getroffenen Brücke (das Ufer in der Kantenfarbe) | 16 | 2 |

Eine Zeile wird also durch 2–8 Zahlen beschrieben.

Geprüft mit `game_check` gegen Läufe des Originals: der Attract-Modus (19 900 Bilder), ein
Bot-Spiel (8 950), ein PAL-Bot-Spiel (5 974), Spiel 7 ab Brücke 50 auf NTSC (2 Läufe, bis zum
Spielende) und PAL (8 950 Bilder): kein Unterschied, bis auf ein Bit, das niemand liest. Jets
fliegen über das Land und berühren die Dekoration: das Bit „berührt die Kante“ ihres Slots
unterscheidet sich (im Attract-Modus auf 341 Bildern), aber moveObjects liest es für einen Jet nie.
`game_check` vergleicht dieses Bit für Jets nicht, wenn `bankRuns` an ist. `RR_BANK_DEBUG=1` gibt
jede Zeile aus, in der die beiden Zeilenarten verschiedene Kollisionen ergeben (alle waren Jets,
Art 7, Player 3).

Läufe von Spiel 7 ab dem Einschalten: `oracle/scripts/start_game7.txt` mit `-bot`. Auf NTSC endet
der Vergleich beim ersten Spielende, das die Portierung nicht modelliert (README).

## Schritt 2: Kollisionen über Spannen

`collideSpans` (der Standard; `RR_COLLIDE=masks` für die Pixelmasken der ganzen Zeile wie zuvor):
jedes Sprite auf der Rasterzeile ist seine Grafikbits und die Farbtakte von seinem ersten bis zu
seinem letzten gesetzten Bit. Gegen das Playfield wird es Lauf für Lauf geprüft (die Zeile ohne
Dekoration, 5–9 Läufe einer Farbe); ein Lauf, der über das erste oder letzte gesetzte Bit
hinausreicht, ist ein Treffer, einer echt dazwischen wird Bit für Bit angesehen. Zwei Sprites:
zuerst ihre Spannen, Bit für Bit nur dort, wo sie sich treffen. Immer noch pro Rasterzeile,
innerhalb des Strahls: was jede Zeile enthält und wann der DLI die Register liest, ist die des
Strahls.

Dieselben Läufe wie oben: kein Unterschied zum Original. `RR_STATS=1` zählt die Fälle;
`RR_ROUGH_BANK=1` und `RR_ROUGH_PAIRS=1` lassen den Bit-für-Bit-Schritt weg (ein Treffer, wo immer
sich die Spannen treffen), um zu sehen, ob er nötig ist:

| Lauf | Zeilen mit Sprites | ein Lauf zwischen erstem und letztem Bit (in einem Loch) | Spannen getroffen (nicht die Bits) |
|---|---|---|---|
| Attract-Modus | 1 164 849 | 86 228 (0) | 26 862 (70) |
| PAL-Bot | 278 257 | 9 201 (96) | 4 108 (0) |
| Spiel 7, PAL | 536 802 | 24 494 (96) | 7 693 (0) |

- Das Ufer: Spannen reichen. Mit `RR_ROUGH_BANK` bleiben alle Läufe gleich dem Original; die
  Läufe, die in ein Loch fielen, haben nie ein Bit geändert, das der DLI liest.
- Sprites untereinander: die Bits sind nötig. Mit `RR_ROUGH_PAIRS` „treffen“ die Kugel und das
  Geschoss des Panzers Objekte durch ihre Löcher: der Attract-Modus weicht auf 3 Bildern ab, der
  NTSC-Bot-Lauf auf 712 (ein Objekt zerstört, an dem die Kugel des Originals vorbeiflog), Spiel 7
  auf 2–4. Die Spannen zweier Sprites treffen sich selten (einige tausend Zeilen pro Lauf), daher
  kostet der Bit-für-Bit-Schritt wenig.

## Schritt 3: die Anzeige ohne die Takte des DLI

`Game::displaySimple` (`port/src/display.cpp`) ist jetzt der Standard; `RR_DISPLAY=beam` führt das
taktgenaue Modell wie zuvor aus, `RR_DISPLAY=check` führt beide in jedem Bild vom selben Zustand
aus und gibt jeden Unterschied in den Kollisionsergebnissen, den Flags der Objekte, dem
Player-Missile-Speicher oder dem, was auf jeder Rasterzeile gezeichnet ist, aus.

Es folgt der Schleife des DLI (B565–B664) in Rasterzeilen, nicht in Takten. Das X-Register des
6502 zählt Rasterzeilen: auf jedes STA WSYNC folgt ein INX, also wird, solange X = c, die
Rasterzeile c − 69 gezeichnet. Für jeden Slot:

| was | Rasterzeile (S: X, wenn das Warten des Objekts endet, E: X nach seiner letzten Zeile) |
|---|---|
| HPOSP3 = x | ab S − 68 |
| zwei leere Zeilen im Speicher von Player 3 | S − 67, S − 66 |
| SIZEP3 | ab S − 66 |
| Formzeile k (von der obersten abwärts), ihr COLPM3, die Straßenfarbe der Brücke | S − 65 + k |
| die Uferfarbe zurück (Brücke) | ab E − 68 |
| M2PL lesen (Kugel – Objekt) | sieht bis E − 70 (eine Brücke: E − 69) |
| P3PL lesen (Objekt – Geschoss), P2PL (Flugzeug – Geschoss, Objekt) | bis E − 69 |
| P3PF lesen (das Bit „berührt die Kante“ des Objekts) | bis E − 68 |
| M0PL lesen (Rakete – Flugzeug), M0PF (Rakete – Ufer) | bis E − 68 (M0PF: siehe unten) |
| P1PF lesen (Panzer), P2PF (Flugzeug – Ufer), M2PF (Kugel – Ufer) | bis E − 67 |
| HITCLR | die Rasterzeilen ab E − 66 zählen für das nächste Objekt (siehe unten) |

Ein Objekt, dessen Zeilen alle über der Rasterzeile liegen, an der sein Warten endet (es begann zu
dicht unter dem vorigen), wird ohne Lesen und ohne HITCLR übersprungen. Wenn X auf 0 überläuft,
endet der Fluss; der Panel-Teil liest die Kollisionen des laufenden Objekts bis zu den Rasterzeilen
188–190.

Wo die CPU nach einem WSYNC steht (Takt 106), ist durch den Code festgelegt, sodass jeder Zugriff
auf eine bekannte Seite des Zeichnens einer Rasterzeile (Takt 28) fällt, bis auf zwei, die von den
Takten des Beginns der nächsten Rasterzeile (DMA, ein Display-List-Befehl mit oder ohne Adresse)
und von den kurz davor gespeicherten Treffern abhängen:

- M0PF (B631) wird 28 Takte nach dem WSYNC gelesen, 30, wenn M0PL ein Treffer war: nach dem
  Zeichnen auf einer Zeile, deren Beginn 8 Takte braucht (mit Adresse), sieht es also eine
  Rasterzeile mehr. Im NTSC-Bot-Lauf: 1 499 Mal von 68 365.
- HITCLR (B65F) kommt 23 Takte nach dem WSYNC, 2 mehr für jeden der drei davor gespeicherten
  Treffer: mit allen dreien, auf einer Zeile mit Adresse, kommt es nach dem Zeichnen, und die
  Kollisionen dieser Rasterzeile gehen verloren. In den Läufen nie gesehen (0 von 285 623).

Kollisionen: der Rahmen jedes Sprites (die Rasterzeilen, auf denen es Grafik hat, und die Takte,
die sie abdecken; Player 3 ein Rahmen pro Objekt, geteilt, wo sich seine Position oder Größe
ändert). Das Playfield: jede Zeile von Player 1 (Panzer), 2 (Flugzeug), 3 (Objekte) und den
Missiles 0 und 2 gegen die Läufe ihrer Rasterzeile (Schritt 2). Die Sprites untereinander: nur die
Paare, die der DLI liest (Kugel – Objekt, Objekt – Geschoss, Flugzeug – Geschoss, Flugzeug –
Objekt, Rakete – Flugzeug), deren Rahmen sich treffen, Bit für Bit auf den gemeinsamen
Rasterzeilen. Was ein Lesen sieht, ist das ODER der Rasterzeilen seit dem letzten HITCLR.

Geprüft: `RR_DISPLAY=check` auf allen Läufen oben (Attract-Modus, NTSC- und PAL-Bots, Spiel 7 auf
NTSC und PAL, zwei Spieler bis Bild 2900): in keinem Bild ein Unterschied zum Strahl.
`RR_DISPLAY=simple` gegen das Original (`game_check`): dieselben Ergebnisse wie der Strahl (keine
Unterschiede, Spiel 7 auf NTSC bis zu seinem Spielende). Der daraus gezeichnete Flussbereich gegen
die Bildschirme des Originals (`-screens`, 1289 Bilder des Attract-Modus, 1272 von Spiel 7 auf PAL
ab Brücke 50): identisch.

### Geschwindigkeit (C++)

`RR_TIME=1` gibt die Zeit aus, die `display()` pro Bild braucht. Release-Build, das Beste aus 5
Läufen, und die von callgrind gezählten Befehle (Spiel 7 auf PAL, 2000 Bilder):

| | Attract-Modus | Spiel 7, PAL (ab Brücke 50) | Befehle pro Bild |
|---|---|---|---|
| der Strahl (`RR_DISPLAY=beam`) | 8,5 µs | 10,1 µs | 101 500 |
| displaySimple, Rahmen im Player-Missile-Speicher gesucht (`RR_BOXES=scan`) | 4,0 µs | 4,0 µs | 40 600 |
| displaySimple, Rahmen aus den Positionen des Spiels | 3,1 µs | 3,7 µs | 34 200 |

Etwa dreimal weniger Befehle, 2,5–3-mal schneller. Beide schauen auf die Läufe des Playfields, die
einmal beim Eintreten jeder Zeile gemacht werden (`Game::runLines`, `makeRuns`), nicht auf die
Pixel der Zeile. Von der Zeit des Strahls gingen zwei Drittel an die Kollisionen, die er auf jeder
Rasterzeile berechnet, ein Drittel an seine Takte (`settle`). Die Rahmen der Sprites kommen
daher, wohin das Spiel sie gesetzt hat (der Standard; `RR_BOXES=scan` sucht wie zuvor im
Player-Missile-Speicher nach unten): das Geschoss 16 Zeilen ab `shellY`, der Panzer 11 ab `tankY`,
das Flugzeug 14 ab Zeile $AA, die Rakete des Hubschraubers die Zeile `missileY`, die Kugel 8 Zeilen
unter `bulletY` (ab Zeile $1C), die Zeilen jedes Objekts, wie die Schleife des DLI sie schrieb; die
Bits jeder Rasterzeile in einem Rahmen werden noch aus dem Player-Missile-Speicher gelesen. Gleich
wie der Strahl in jedem Bild jedes Laufs: nichts außerhalb dieser Rahmen (etwa Zeilen, die aus
früheren Bildern im Speicher von Player 3 übrig sind) hat je eine Kollision erzeugt, die der DLI
liest.

### Lange Läufe: `display_bench`

`port/build/display_bench <rom> <frames> [-pal] [-game N] [-seed N]` spielt mit einem Autopiloten
(er steuert zur Mitte des Wassers voraus und feuert ständig; das Flugzeug kann nicht abstürzen und
verbraucht nie Treibstoff, sodass ein Spiel den ganzen Lauf dauert: 100 000 Bilder erreichen
Brücke 98 in Spiel 1, Brücke 147 in Spiel 7). Mit `RR_DISPLAY=check` vergleicht es die beiden
Modelle in jedem Bild, mit `RR_TIME=1` misst es `display()`.

Diese Läufe fanden, was die kürzeren nie zeigten: der DLI fällt manchmal eine Rasterzeile zurück.
Von einem WSYNC zum nächsten muss der Code vor Takt 106 der nächsten Rasterzeile passen, abzüglich
Beginn und Zeichnen dieser Rasterzeile (auf einer Flusszeile 6 oder 8 + 57 Takte); drei Abschnitte
können länger sein:

- B5B0–B5D1 (bis zur ersten Zeile): 49 Takte, 50, wenn das Formbyte der ersten Zeile auf der
  nächsten Seite liegt: zu lang auf einer Zeile mit Bildschirmadresse.
- die letzte Zeile einer Brücke bis B5F3: 46 Takte, +1 für jedes Tabellenlesen über eine
  Seitengrenze, +2 für einen Kugeltreffer.
- B5F3–B61F: 44 Takte (Brücke 43), +2 für jeden dort gespeicherten Treffer (Geschoss, Flugzeug mit
  Geschoss, Flugzeug mit Objekt).

Wenn es zu lang ist, wartet das STA WSYNC auf das Ende der nächsten Rasterzeile: alles danach in
diesem Bild geschieht eine Rasterzeile später (X = Rasterzeile + 68), das Flussende und das Lesen
des Panels eingeschlossen, und eine Zeile von Player 3, die auf oder nach der Rasterzeile
geschrieben wird, für die sie ist, erscheint dort erst im nächsten Bild (GTIA hat die alte schon
geholt). displaySimple zählt diese Auslassungen (`RR_STATS`): 0–10 in 100 000 Bildern, alle mit
dem Flugzeug, das in Dinge fliegt, an denen es abgestürzt wäre; in einem echten Spiel selten. Mit
ihnen modelliert ergibt `RR_DISPLAY=check` keinen Unterschied in 6 Läufen von 100 000 Bildern
(Spiele 1, 2, 3, 5, 7, 8, NTSC und PAL).

Zeit von `display()`, das Beste aus 3, und Befehle pro Bild (callgrind, 5000 Bilder):

| | Spiel 1, NTSC, 50 000 / 100 000 Bilder | Spiel 7, PAL, 50 000 / 100 000 Bilder | Befehle (Spiel 1 / Spiel 7) |
|---|---|---|---|
| der Strahl | 10,8 / 11,1 µs | 10,5 / 9,5 µs | 120 000 / 129 000 |
| displaySimple | 5,1 / 4,8 µs | 4,8 / 5,0 µs | 65 000 / 69 000 |

Etwa doppelt so schnell. Weniger als bei den Läufen des Originals oben (dreimal), weil das Flugzeug
hier durch Objekte und Ufer fliegt, ohne abzustürzen: weit mehr Kollisionen pro Bild als in einem
echten Spiel.

Noch länger: 12 Läufe von 300 000 Bildern (3,6 Millionen), `-bridge 100` (das Spiel beginnt an
Brücke 100; bis Brücke 392), NTSC und PAL, Spiele 1, 2 (zwei Spieler) und 7, und mit `-wild` (der
Autopilot steuert zufällig, auch über die Ufer: bis Brücke 70–159). In keinem Bild ein
Unterschied. Ausgelassene Rasterzeilen: 0–8 pro Lauf. Das späte HITCLR (alle drei Treffer davor
gespeichert, auf einer Zeile mit Bildschirmadresse) kam nie vor, weder in diesen noch in anderen
Läufen: es ergibt sich allein aus den Takten des Codes, in Aktion nicht gesehen.

### Nur die Objekte, die an der Kante wenden

Das Bit „berührt die Kante“ (P3PF) liest moveObjects nur für Hubschrauber ($08–$0B), Ballons ($0C)
und Schiffe ($0D): sie wenden bei der ersten Berührung. Jets fliegen geradeaus, Explosionen stehen
still, Brücken und Depots bleiben stehen. `displaySimple` prüft nur die Zeilen dieser Arten gegen
die Kante (`noEdge` markiert die Rasterzeilen der anderen); das Bit der anderen bleibt gelöscht,
und `game_check` vergleicht es für sie nicht (`bankRuns`). Geprüft gegen das Original: der
Attract-Modus (NTSC 2 262 und 5 961 Bilder, PAL 2 975), Bot-Spiele auf NTSC (bis zum ersten
Spielende, das die Portierung auf NTSC nicht modelliert), auf PAL (5 975 und zwei Spieler 7 775)
und Spiel 7 ab Brücke 50 auf PAL (5 975): kein Unterschied.

### Das Flugzeug gegen ein Objekt: über Rahmen und Bits

Das P2PL-Bit für Player 3 wird nach den Zeilen eines Objekts (B60A) oder am Flussende gelesen und
sieht die Rasterzeilen seit dem HITCLR davor: nur die Zeilen dieses Objekts, an seiner Position und
in seiner Größe. Also wird „das Flugzeug hat das Objekt berührt“ ohne die Rasterzeilen berechnet:
die 14 Zeilen des Flugzeugs ab $AA gegen die Zeilen des Objekts, wo die Schleife sie schrieb, Bit
für Bit (`planeTouches`), an denselben zwei Stellen (die Takte des Speicherns zählen wie zuvor);
das Paar P2–P3 ist aus den Kollisionen der Rasterzeilen verschwunden. Zuerst neben dem Wert des
DLI gemessen (`RR_GEO_CHECK`, inzwischen entfernt): gleich in jedem Bild der Läufe des Originals
oben (~47 000 Bilder, 420 mit dem Treffer) und von `display_bench` (je 200 000 Bilder mit `-wild`
auf NTSC und Spiel 7 auf PAL, 300 000 ab Brücke 100 auf NTSC und auf PAL mit `-wild`: 61 382
Treffer). Dann `game_check` gegen das Original: kein Unterschied; `RR_DISPLAY=check` (60 000 Bilder
`-wild`, 60 000 ab Brücke 100 auf PAL): nichts außer dem Kantenbit der Arten, die nicht wenden
(oben).

### Alle Kollisionen ohne die Register der Rasterzeilen

Die übrigen Lesevorgänge gehen genauso. Die Zeilen jedes Sprites sind dort, wo das Spiel sie
hingesetzt hat (die Rahmen oben: nichts davon ist anderswo), also werden die Rasterzeilen, auf
denen jede Kollision geschieht, die der DLI liest, einmal pro Bild berechnet: die Zeilen des
Panzers gegen die Kante (P1PF), die des Flugzeugs gegen das Ufer (P2PF), die der Rakete und der
Kugel gegen beides (M0PF, M2PF), das Flugzeug gegen das Geschoss (P2PL) und die Rakete gegen das
Flugzeug (M0PL), Bit für Bit. Die Kollisionen eines Objekts werden bei seinen Lesevorgängen
berechnet, gegen seine Zeilen, wo die Schleife sie schrieb: die Kugel (M2PL, bis zur Rasterzeile
vor der letzten Zeile, die einer Brücke eingeschlossen), das Geschoss (P3PL), das Flugzeug (P2PL),
die Kante (P3PF, nur die Arten, die wenden). Ein Lesen zählt eine Kollision, wenn ihre Rasterzeile
zwischen der des letzten HITCLR und seiner eigenen liegt, wie bei den Registern. Die Schleife des
DLI, ihre Takte und die Rasterzeilen von HITCLR bleiben (sie bestimmen diese Fenster); die Register
der Rasterzeilen (`compute`, `seen`, die Rahmen pro Rasterzeile) sind weg. Ein Fall unterscheidet
sich: wenn der DLI eine Rasterzeile zurückfällt, werden die spät geschriebenen Zeilen nicht geholt,
und die alten sind zu sehen; die Kollisionen sind jetzt mit den neuen Zeilen. Das kam in keinem
der Läufe hier vor. Geprüft: `game_check` gegen das Original (alle Läufe oben): kein Unterschied;
`RR_DISPLAY=check` (60 000 Bilder `-wild`, 60 000 ab Brücke 100 auf PAL, 300 000 ab Brücke 100 auf
NTSC und 300 000 auf PAL mit `-wild`): nichts außer dem Kantenbit der Arten, die nicht wenden.

### Die Objekte aus ihren Zeilen gezeichnet

Die Zeilen von Player 3 liest niemand außer GTIA: das Spiel nie. Daher schreibt `displaySimple`
sie nicht mehr, ebenso wenig die Register von Player 3 pro Rasterzeile (HPOSP3, SIZEP3, die Farben,
die zwei leeren Zeilen über jedem Objekt). Was es zum Zeichnen hinterlässt, ist `drawn`: von jedem
Objekt die erste Rasterzeile, Position, Größe, Zeilen und ihre Farben, wie die Schleife sie
schreibt, und die Uferfarbe pro Rasterzeile (`bankColour` in `scan`; seine Player-3-Felder bleiben
0); `drawRiver` zeichnet die Objekte daraus. Die Schleife selbst bleibt als Fahrplan: welche
Zeilen jedes Objekt zeigt, wo jedes Lesen und HITCLR kommt, die Takte. `diffWithRam` vergleicht
die Zeilen von Player 3 ($0F00–$0FBF) bei `simpleDisplay` nicht; `RR_DISPLAY=check` vergleicht
statt der Rasterzeilen die beiden Bilder. Wenn der DLI zurückfällt, zeigt das Original an der
Stelle der späten Zeilen die des letzten Bildes; hier werden die neuen gezeichnet (in keinem
Lauf). Geprüft: `game_check` gegen den Zustand des Originals (alle Läufe oben): kein Unterschied;
gegen seine Bildschirme (`-screens`, 1300 Bilder des Attract-Modus, 1500 von Spiel 7 auf PAL ab
Brücke 50): der Flussbereich identisch; `RR_DISPLAY=check` (60 000 Bilder `-wild`, 60 000 ab
Brücke 100 auf PAL): die Bilder gleich, nichts außer dem Kantenbit der Arten, die nicht wenden.

### RR_GEOMCHECK: einfache Geometrie neben den Lesevorgängen

Bevor es weitergeht, eine Messung: mit `RR_GEOMCHECK` berechnet `displaySimple` die Kollisionen
jedes Bildes auch ohne die Lesevorgänge des DLI, ohne Fenster von einem HITCLR zu einem Lesen und
ohne Takte. Ein Objekt ist getroffen, wenn seine Zeilen die des Sprites überhaupt berühren (das
letzte solche Objekt in der Reihenfolge der Schleife, da die Speichervorgänge einander
überschreiben); die anderen Kollisionen zählen, wenn sie irgendwo auf den Zeilen der Sprites
geschehen; das Kantenbit eines Objekts kommt aus allen seinen Zeilen. Die Ergebnisse der
Lesevorgänge bleiben; die Bilder, in denen die Geometrie anders entscheidet, werden pro Feld
gezählt und beim Beenden ausgegeben. `RR_GEOMCHECK=2` gibt zusätzlich jedes solche Bild aus: seine
Nummer, was abweicht, die Orte von Kugel, Flugzeug und Panzer und die Zeilen jedes Objekts.

Das Ergebnis: 0,01–0,25 % der Bilder weichen ab, je nach Lauf, und immer auf dieselbe Weise: eine
Berührung, die die Lesevorgänge ein Bild später oder gar nicht sehen. Die Kugel auf der letzten
Zeile eines Objekts (M2PL wird gelesen, bevor diese Zeile gezeichnet ist), die Zeilen des Flugzeugs
oder des Panzers unter dem letzten Lesen des Bildes. Nichts, was die Geometrie sieht, ist eine
Berührung, die nicht auf dem Bildschirm geschah.

## Branch `dli_removed`: `displayGeometry`

In diesem Branch ist `Game::displayGeometry` (`port/src/display.cpp`) der Standard
(`RR_DISPLAY=geometry`); `RR_DISPLAY=simple`, `beam` und `check` bleiben wie oben. Darin ist kein
DLI: keine Schleife über die Slots in Rasterzeilen, keine Takte, kein HITCLR, keine Lesevorgänge.
Es ist das, was die ZX-Ports tun, in C++, und die Referenz, gegen die sie Bild für Bild geprüft
werden.

### Die Kollisionen als einfache Geometrie

Jede Kollision zählt dort, wo sich die Zeilen der Sprites und der Objekte überhaupt berühren,
zuerst die Rahmen, dann die Punkte (die Regel von RR_GEOMCHECK oben). Ein getroffenes Objekt ist
das letzte solche den Bildschirm hinab, da die Speichervorgänge des Originals einander
überschreiben; das Geschoss wird nicht gegen eine Brücke geprüft. Die Kollisionen mit Ufer und
Kante, Flugzeug–Geschoss und Rakete–Flugzeug halten Slot 0, wenn sie irgendwo auf den Zeilen der
Sprites geschehen. Die Zeilen der Sprites sind die Rahmen aus den Positionen des Spiels (Schritt 3:
das Geschoss 16 Zeilen ab `shellY`, der Panzer 11 ab `tankY`, das Flugzeug 14 ab $AA, die Rakete
die Zeile `missileY`, die Kugel 8 Zeilen unter `bulletY`).

Das Spiel ist also nicht mehr in jedem Bild das des Originals: auf den Bildern, die RR_GEOMCHECK
zählt (0,01–0,25 %), trifft eine Kugel ein Objekt oder ein Flugzeug das Ufer ein Bild früher, oder
eine Berührung, die das Original verpasste, zählt. `game_check` gegen das Original ist für
`RR_DISPLAY=simple`; mit dem Standard meldet es diese Bilder.

### Jedes Objekt am y seines Slots

Die Objekte werden direkt aus ihren Slots gesetzt, ohne den Zählergang des DLI: Zeilen ab
Rasterzeile y − 65 abwärts, wo der DLI sie zeichnete (die unterste Zeile der Formtabelle zuerst, da
die Tabellen sie von unten nach oben halten), abgeschnitten am oberen Rand des Flusses
(Rasterzeile 28: das erste Warten des DLI war X = $5D) und an seinem Ende (X über 255). Die
Straßenfarben der Brücke kommen aus diesen Zeilen, nach der letzten die Uferfarbe des Abschnitts.
Geprüft über 120 000 Bilder (`display_bench` vom Start, ab Brücke 60 auf PAL, `-wild`, Spiel 7)
gegen den ersetzten Zählergang: dieselben Objekte, Zeilen und Farben in jedem Bild. Die Schleife
fiel nie hinter ein Objekt zurück, also schneiden sie nur oberer Rand und Ende des Flusses ab.

### Die Kante über den ersten und letzten Punkt einer Zeile

Das Kantenbit eines Objekts (P3PF: Hubschrauber, Ballons, Schiffe wenden an der Kante) kommt aus
jeder seiner Zeilen als Spanne, vom ersten bis zum letzten Punkt, gegen die Läufe der Kantenpixel
der Zeile (`Beam::edgeTouches`), ohne Punkt für Punkt zu schauen: ein Lauf in einem Loch einer
Zeile zählt, anders als die Bit-für-Bit-Prüfung des Originals in GTIA. Der Next prüft es so: pro
Zeile die Läufe der Kante, pro Objektzeile ihre zwei Enden. Über 20 000 Bilder der Bot-Spiele (mehr
als 100 Berührungen) war das Ergebnis nie anders als bei der Punkt-für-Punkt-Prüfung.

### Die anderen Sprites gegen Ufer und Kante genauso

`Beam::spanTouches`: der erste und letzte Punkt einer Zeile gegen die Läufe der gewünschten Farben
der Zeile (Ufer, Kante oder beides); `edgeTouches` ist es mit der Kante. Der Panzer gegen die
Kante, das Flugzeug gegen das Ufer, die Rakete und die Kugel gegen beides (`pfAny`) nutzen es
ebenfalls, also ist jede Kollision mit dem Playfield ein Test: die zwei Enden der Zeile gegen die
Läufe der Zeile. Die Sprites untereinander (Kugel, Geschoss und Flugzeug gegen ein Objekt,
Flugzeug und Geschoss, Rakete und Flugzeug) bleiben Bit für Bit: Schritt 2 zeigte, dass dort die
Löcher zählen.

### Was es kostet und wofür

Die Anzeige besteht jetzt aus drei Schleifen: die Rahmen der Sprites gegen die Läufe ihrer
Zeilen, die Paare von Sprites, wo sich ihre Rahmen treffen, die aus ihren Slots gesetzten und
gegen Kugel, Geschoss und Flugzeug geprüften Objekte. Keine Rasterzeile, die nichts enthält, wird
durchlaufen. Auf dem Next braucht dieselbe Arbeit (`display_simple.asm`, `display_hits.asm` in
river_raid_zx) 45 000–62 000 seiner 567 000 Takte pro Bild bei 28 MHz; seine CODE.md und NOTES.md
haben die Zahlen Routine für Routine.
