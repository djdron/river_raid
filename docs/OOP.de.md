# Die objektorientierte Neufassung (Branch `oop`)

[English](OOP.md) | [Русский](OOP.ru.md) | [Українська](OOP.uk.md) | Deutsch | [Polski](OOP.pl.md) | [Italiano](OOP.it.md) | [Español](OOP.es.md) | [中文](OOP.zh.md) | [日本語](OOP.ja.md)

Branch `oop`, von `dli_removed`. Das Ziel: das Spiel der Portierung als Objekte mit Namen (der
Fluss, das Flugzeug, sein Geschoss, der Panzer und seine Granate, die Rakete des Hubschraubers,
die Schiffe, Hubschrauber, Jets, Ballons, Tanklager, Brücken und Explosionen in ihren Slots, die
Spieler und die Statusleiste, der Ton) statt des nach Adressen angelegten RAM des Originals,
wobei das Spiel genau so spielt wie in `dli_removed`, Bild für Bild und Bit für Bit. Die ZX
Spectrum-Portierungen folgen `dli_removed` und werden dagegen geprüft; `oop` ist eine Neufassung
desselben Spiels, kein neues.

## Die Regeln

- **Dasselbe Spiel.** In jedem Bild derselbe Zustand, dieselben Soundregister, dasselbe Bild wie
  in `dli_removed`. Kein float: Positionen, Geschwindigkeiten, Treibstoff und Zeitgeber bleiben
  Bytes mit Überlauf, mit ihren Bruchteil-Bytes; Punkte, Brücken und Highscores bleiben BCD. Die
  Eigenheiten bleiben auch: die Reihenfolge der Dinge in einem Bild, die Slots und ihre
  Reihenfolge, ein Objekt, das sich in ein anderes verwandelt, wie oft und wann RANDOM gelesen
  wird, die Reset-Bilder, der Wechsel der zwei Spieler.
- **Nach jedem Schritt geprüft** mit `tools/trace/compare.sh` (unten): alle Läufe gleich, sonst
  ist der Schritt nicht fertig.
- **Eins nach dem anderen**, jedes ein eigener Commit: Ein neuer Typ oder eine neue Klasse
  umhüllt zuerst den Zustand, wie er ist, dann zieht der Code hinein, dann geht der alte Zustand.
- **Das RAM des Originals bleibt erreichbar**: `saveToRam` / `loadFromRam` / `diffWithRam`
  (`ram_map.h`, `game_ram.cpp`) behalten das Layout des Originals, für `game_check` und für die
  Prüfungen der ZX-Portierungen (`nxcheck`, `tscheck`), was auch immer die Klassen innen halten.

## Die Prüfungen

`port/tools/game_trace` schreibt pro Bild eine Zeile darüber, was das Spiel tut, mit Namen: das
Flugzeug, das Geschoss, der Panzer, die Granate, die Rakete, jeder Slot, Punkte, Leben, der
andere Spieler, Highscores, der Flussgenerator, die Soundregister, die Kollisionsergebnisse und
ein Hash des Bilds (Fluss und Statusleiste). Es spielt mit dem Autopiloten von `display_bench`;
ohne `-ghost` stürzt das Flugzeug ab und geht ihm der Treibstoff aus, sodass Leben,
Wiedereinstiege und Spielenden in den Läufen vorkommen.

`port/tools/trace/runs.txt` listet 17 Läufe auf, 700 000 Bilder: den Demo-Modus, die Spiele 1–8
auf NTSC und PAL, zwei Spieler, `-wild` (zufällig gesteuert, auch über die Ufer), lange
`-ghost`-Läufe ab den Brücken 60 und 100. `golden.txt` enthält den Hash der Zeilen jedes Laufs,
wie `dli_removed` ihn spielt, erstellt beim Commit in `REFERENCE`.

```
port/tools/trace/compare.sh <rom> [build dir] [runs...]
```

spielt sie (~2 Minuten) und vergleicht. Bei einem Unterschied baut es den Commit aus `REFERENCE`
in einem temporären Worktree, spielt den Lauf auch dort und gibt das erste abweichende Bild aus,
beide Zeilen. `tools/fixed_check` prüft die Zahlentypen (unten) für jede Eingabe gegen den Code,
den sie ersetzen.

## Erledigt

1. `game_trace`, die Läufe, die Referenz.
2. `PokeyRandom` ohne seine ~1 MB Tabellen: Ein Sprung des Zählers um n Takte ist linear über
   GF(2), also wird er aus 17 Abbildungen zusammengesetzt (2^k Schritte, 1,2 KB), und die
   Längen, die das Spiel benutzt, werden als Abbildungen gehalten, die 4 Bits auf einmal lesen,
   wie in den ZX-Spectrum-Portierungen (5 Tabellenzugriffe pro Lesen). Gegen die alte Klasse an
   6 Millionen Lesevorgängen geprüft.
3. Die Anzeige nur als Geometrie: Die Modelle des DLI (`displayBeam`, `displaySimple`, die
   `RR_DISPLAY`-Modi, `RR_GEOMCHECK`, die Klasse `Beam`, die Schalter der Experimente) sind aus
   diesem Branch entfernt; sie bleiben in `dli_removal`. `display.cpp`: 276 Zeilen statt 1331.
4. `src/fixed.h`: `Fixed8` (ein Byte und sein Bruchteil-Byte, Addieren / Subtrahieren mit
   Übertrag), `Bcd<N>` (BCD-Zähler: Addieren mit Übertrag nach oben, der Vergleich des
   Originals), `addBcd`.
5. Das Geschoss (`bullet.h`) und das Flugzeug (`plane.h`: Steuerung, Tempo, Treibstoff) als
   eigene Typen.
6. Kein Player-Missile-Speicher: Die Zeilen der Sprites kommen aus ihrem Zustand
   (`Game::spriteRow`: jede Form aus dem ROM in ihrer Zeile, die Rakete eine Zeile, das Geschoss
   8), gezeigt ab dem Anfang jedes Bilds, bis ein Reset sie löscht. Dass sich im Original Teile
   dieses Speichers überschneiden (die Zeilen des Geschosses löschen die der Rakete), macht die
   Kopie im nächsten Bild rückgängig, bevor jemand sie liest, also ist das Spiel dasselbe.
   `saveToRam` schreibt den Speicher weiterhin so, wie das Original ihn hat.
7. Der Panzer, seine Granate und die Rakete des Hubschraubers (`tank.h`, `tank.cpp`): je eine
   Struktur, gehalten vom Fluss (sie scrollen mit ihm; ab Schritt 13 von `Objects`), mit ihren
   Bewegungen (`Tank::drive`, `Shell::move`, `Missile::move`).
8. Die Objekte in den Slots (`objects.h`, `objects.cpp`): eine Klasse je Art von Verhalten
   (`Explosion`, `Jet`, `Helicopter`, `Balloon`, `Ship`, `Bridge`, `FuelDepot`). Ein Slot hält
   die Bytes des Originals; seine Art ist zugleich sein Animationsbild und ändert sich bei einem
   Treffer, also liefert `objectKind(kind)` in jedem Bild das Objekt der Klasse, die weiß, was
   diese Art tut: ihre Animation, ihre Bewegung (der Jet über den Bildschirm; Hubschrauber,
   Ballons und Schiffe hin und her, sobald sie geweckt sind, mit Umkehr am Rand), ob die Anzeige
   sie gegen den Rand prüft.
9. Die Spieler (`players.h`): `PlayerData` (der andere Spieler) mit `Bcd<2>`-Brücken und
   `Bcd<3>`-Punkten, dieselben Typen für den aktiven Spieler, `HighScores` (die Tabelle und ihre
   Prüfbytes); die Statusleiste (`panel.h`): ihre 60 Zeichen mit `print` (BCD-Ziffern),
   `showLives`, `markPlayers`, `showGameNumber`.
10. Der Ton (`sound.h`, `sound.cpp`): `Sounds` hält, was das Spiel anfordert (den Startpiep, den
    Ereignis-Ton und seine Dauer, Explosion, Schuss, Bonus, Rakete, die fallende Tonhöhe nach
    einem Absturz), und sein `drive()` ist der Treiber (B2A5–B3D8), dem das Flugzeug, die Pause,
    der Demo-Modus und die Granate über einen `SoundContext` gegeben werden; `PokeyRegs` ist auch
    dorthin gezogen.
11. Der Fluss auf dem Bildschirm (`river.h`, `river.cpp`): seine Zeilen, dieselben ohne
    Verzierung, ihre Abschnitte, die Adresse der obersten Zeile im Bildschirmspeicher des
    Originals und die Zustände des Generators für kleine Spielstände; `push()` fügt die nächste
    Zeile des Generators hinzu, `rebuild()` erzeugt sie neu, wenn ein Zustand geladen wird.
12. Kein `GameState`: Seine Felder sind die eigenen Member von `Game` (`eachState()` zählt sie
    zum Speichern, Laden und Zurücksetzen auf).
13. Der Fluss getrennt von den Objekten (`world.h`, `world.cpp`): `RiverGen` erzeugt nur den
    Fluss (`RiverState`: Ufer, Inseln, Abschnitte, das LFSR); die Slots, der Panzer, seine
    Granate und die Rakete sind `Objects`, das nach jeder Zeile die neuen Objekte setzt
    (`afterLine`, $ABE1–$AE93) und dem Generator sagt, wo das gerade hereinkommende Objekt ist
    (`BankObstacle`), damit das Ufer ihm ausweicht. `World` hält beides und lässt es Zeile für
    Zeile laufen; sein `State` ist, was die Spielstände und die Zeilenzustände des Flusses
    behalten.
14. Der RAM-Export aus den Klassen (`ram_map.h`, `ram_map.cpp`): Jede Klasse listet ihre Bytes
    mit ihren Adressen in ihrem `mapRam(RamMap &)` auf (das Flugzeug, das Geschoss, die Töne, die
    Kollisionen, die Spieler, die Highscores, die Statusleiste, die Laufschrift, der Fluss, die
    Slots, der Panzer, seine Granate und die Rakete, die oberste Bildschirmzeile); `World` und
    `Game` fügen die Listen zusammen, und `RamMap` lädt, schreibt und vergleicht sie. Was
    berechnet statt gehalten wird (der Player-Missile-Speicher, die Bildschirmzeilen aus der
    Display-List), bleibt in `game_ram.cpp` und `River`.
15. Die Dokumente: die Dateitabellen der READMEs und ihre Teile zu Code, Generator und
    Überprüfung, HANDOFF (der Branch), NOTES (Spielstände), HOW_IT_WORKS (die Kollisionen), in
    allen Sprachen; die Kommentare im Quelltext, die noch vom DLI sprachen oder davon, dass der
    Fluss die Objekte hält.
16. Der Abschluss: alle 17 Läufe gleich (700 000 Bilder), auch auf einem Build mit
    AddressSanitizer, UndefinedBehaviorSanitizer und den Prüfungen der Standardbibliothek, ohne
    eine Meldung; dieses Dokument in jeder Sprache.
17. `game.cpp` nach Themen in Dateien geteilt, die Klasse unberührt: `game.cpp` (das Bild in der
    Reihenfolge des Originals), `game_plane.cpp` (das Flugzeug, Explosionen, Punkte),
    `game_players.cpp` (Spiele und Spieler, die Statusleiste), `game_reset.cpp` (Einschalten,
    Demo-Modus, neues Spiel, Reset, das nächste Flugzeug), `game_state.cpp` (Spielstände); was
    sie gemeinsam haben, steht in `game_internal.h`.
18. Die Partie (`match.h`, `match.cpp`): `Match` hält die Spielnummer, wer dran ist, Brücken,
    Punkte und Leben beider Spieler, `respawned` und die Highscores, mit `setUp` (sub $AF80),
    `selectNext`, `swapPlayers` (bekommt den Zustand des Flusses: Level und Neustartpunkt
    wechseln auch den Spieler), `updateHighScore`, `addPoints` (A547: Punkte, das Bonusflugzeug,
    das Ende nach 999 999) und den Zahlen in der Statusleiste (im Demo-Modus nicht gezeigt, was
    `Game` ihm sagt). Seine Bytes kommen in der Reihenfolge der früheren Felder, also bleiben
    Spielstände gleich. Unterwegs gefunden: `World::State` hatte zwei Füllbytes, gespeichert mit
    dem, was gerade im Speicher stand; jetzt sind es ausdrückliche Nullbytes, und derselbe
    Zustand wird immer als dieselben Bytes gespeichert.
19. Die Felder von `Game` privat (`name_`). Außen konstante Getter (`world()`, `plane()`,
    `match()`, `pokey()`...), `output()` (was ein Bild zeigt und spielt: die Statusleiste, der
    Treibstoff, die Laufschrift, das Logo, Bildschirm aus, die Soundregister) und ein paar
    benannte Aktionen statt Schreibzugriffen auf die Felder: `setRiver`, `keepLineStates`,
    `skipState` / `setSkipState` (die Anker des Flussbetrachters), `playSound` und `stayAwake`
    (der Betrachter), `ghost` (der Geistermodus der Werkzeuge), `setPokey` (`sound_check`).
    `sound_check` lädt die Bytes, die der Treiber liest, über `mapRam`, dieselben Bytes wie
    zuvor.
20. Die Konsole (`console.h`, `console.cpp`): `Console` hält den Bildzähler, die Flags und den
    Zeitgeber des Demo-Modus, die Leerlaufzeit, die Pause, die Verzögerung von SELECT,
    `powerOnDone` und `screenOn`, mit `keys` (A19E-A1F5: Aktivität, Pause, Bildschirm aus),
    `attractTimerTick` (A2F5), `countFrame` (die Zähler des VBI, auch für `skipFrame`) und
    `selectRepeat` (A699). Wo die Reihenfolge des Bilds sie mit dem Rest mischt (ein neues
    Spiel, die Spielwahl, Spielende, der PAL-VBI vor dem Spielende), setzt `Game` die Felder
    weiter selbst. Dieselben Bytes in derselben Reihenfolge, also bleiben Spielstände gleich;
    Pause, SELECT, OPTION, START und das Ausgehen des Bildschirms wurden auf 390 000 Bildern mit
    solchen Tasten gegen den Commit davor geprüft.
21. Die Hauptschleife als benannte Schritte, in der Reihenfolge des Originals und ohne
    Umstellung: `mainLoop` ist `console_.keys` (A19E-A1F5), `wakeObject` (A1FF), `driveTank`
    (A21B), `moveObjects` (A26C), `attractTimerRunsOut` (A2F5), `readStick` (A312),
    `updatePlane` (A33F); `mainLoopEnd`, wo ein Wiedereinstieg weitergeht, ist `moveMissile`
    (A5B7), `moveShell` (A607), `consoleKeys` (A673), `scroll` (A6DE). Ein Schritt, der einen
    Reset beginnt, gibt true zurück, und die Schleife endet dort wie zuvor. Am Ende alle 17
    Läufe gleich, auch auf dem Sanitizer-Build.

## Als Nächstes

Nichts geplant. `Game` ist der Dirigent des Bilds; was es hält, hat eigene Klassen (`World`,
`Match`, `Console`, `Plane`, `Sounds`...). Eine Änderung von hier an wird genauso geprüft:
`compare.sh -quick` nach jedem Schritt, alle 17 Läufe am Ende.
