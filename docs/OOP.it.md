# La riscrittura a oggetti (branch `oop`)

[English](OOP.md) | [Русский](OOP.ru.md) | [Українська](OOP.uk.md) | [Deutsch](OOP.de.md) | [Polski](OOP.pl.md) | Italiano | [Español](OOP.es.md) | [中文](OOP.zh.md) | [日本語](OOP.ja.md)

Branch `oop`, da `dli_removed`. L'obiettivo: il gioco del port come oggetti con un nome (il
fiume, l'aereo, il suo proiettile, il carro armato e il suo proiettile, il missile
dell'elicottero, le navi, gli elicotteri, i jet, i palloni, i depositi di carburante, i ponti e
le esplosioni nei loro slot, i giocatori e il pannello, il suono) invece della RAM dell'originale
disposta per indirizzi, mentre il gioco gioca esattamente come su `dli_removed`, fotogramma per
fotogramma e bit per bit. I port per ZX Spectrum seguono `dli_removed` e sono verificati contro di
esso; `oop` è una riscrittura dello stesso gioco, non un gioco nuovo.

## Le regole

- **Lo stesso gioco.** A ogni fotogramma lo stesso stato, gli stessi registri del suono, la
  stessa immagine di `dli_removed`. Niente float: posizioni, velocità, carburante e timer restano
  byte che si avvolgono, con i loro byte di frazione; punti, ponti e record restano in BCD.
  Restano anche le stranezze: l'ordine delle cose in un fotogramma, gli slot e il loro ordine, un
  oggetto che si trasforma in un altro, quante volte e quando si legge RANDOM, i fotogrammi del
  reset, lo scambio dei due giocatori.
- **Verificato dopo ogni passo** con `tools/trace/compare.sh` (sotto): tutte le partite uguali,
  altrimenti il passo non è fatto.
- **Una cosa alla volta**, ognuna con il suo commit: un nuovo tipo o una nuova classe prima
  avvolge lo stato così com'è, poi il codice ci si sposta dentro, poi il vecchio stato se ne va.
- **La RAM dell'originale resta raggiungibile**: `saveToRam` / `loadFromRam` / `diffWithRam`
  (`ram_map.h`, `game_ram.cpp`) mantengono la disposizione dell'originale, per `game_check` e per
  le verifiche dei port ZX (`nxcheck`, `tscheck`), qualunque cosa tengano dentro le classi.

## Le verifiche

`port/tools/game_trace` scrive una riga per fotogramma di ciò che fa il gioco, per nome: l'aereo,
il proiettile, il carro armato, il suo proiettile, il missile, ogni slot, punti, vite, l'altro
giocatore, i record, il generatore del fiume, i registri del suono, i risultati delle collisioni
e un hash dell'immagine (fiume e pannello). Gioca con il pilota automatico di `display_bench`;
senza `-ghost` l'aereo si schianta e resta senza carburante, così vite, ripartenze e fine
partita sono nelle partite.

`port/tools/trace/runs.txt` elenca 17 partite, 700 000 fotogrammi: la modalità dimostrativa, i
giochi 1–8 su NTSC e PAL, due giocatori, `-wild` (guida a caso, anche sopra le rive), lunghe
partite `-ghost` dai ponti 60 e 100. `golden.txt` contiene l'hash delle righe di ogni partita come
le gioca `dli_removed`, fatto al commit in `REFERENCE`.

```
port/tools/trace/compare.sh <rom> [build dir] [runs...]
```

le gioca (~2 minuti) e confronta. In caso di differenza compila il commit di `REFERENCE` in un
worktree temporaneo, gioca la partita anche lì e stampa il primo fotogramma diverso, entrambe le
righe. `tools/fixed_check` verifica i tipi numerici (sotto) su ogni ingresso contro il codice che
sostituiscono.

## Fatto

1. `game_trace`, le partite, il riferimento.
2. `PokeyRandom` senza le sue ~1 MB di tabelle: un salto del contatore di n cicli è lineare su
   GF(2), quindi è fatto di 17 mappe (2^k passi, 1,2 KB), e le lunghezze che il gioco usa sono
   tenute come mappe lette 4 bit alla volta, come fanno i port per ZX Spectrum (5 accessi per
   lettura). Verificato contro la vecchia classe su 6 milioni di letture.
3. La visualizzazione solo come geometria: i modelli del DLI (`displayBeam`, `displaySimple`, le
   modalità `RR_DISPLAY`, `RR_GEOMCHECK`, la classe `Beam`, gli interruttori degli esperimenti)
   sono stati tolti da questo branch; restano su `dli_removal`. `display.cpp`: 276 righe invece
   di 1331.
4. `src/fixed.h`: `Fixed8` (un byte e il suo byte di frazione, somma / sottrazione con il
   riporto), `Bcd<N>` (contatori BCD: somma con il riporto verso l'alto, il confronto
   dell'originale), `addBcd`.
5. Il proiettile (`bullet.h`) e l'aereo (`plane.h`: guida, velocità, carburante) come tipi propri.
6. Niente memoria player-missile: le righe degli sprite vengono dal loro stato
   (`Game::spriteRow`: ogni forma dalla ROM alla sua riga, il missile una riga, il proiettile 8),
   mostrate dall'inizio di ogni fotogramma finché un reset non le cancella. La condivisione di
   quella memoria nell'originale (le righe del proiettile cancellano quelle del missile) viene
   annullata dalla copia del fotogramma successivo prima che qualcuno la legga, quindi il gioco è
   lo stesso. `saveToRam` scrive ancora la memoria come la ha l'originale.
7. Il carro armato, il suo proiettile e il missile dell'elicottero (`tank.h`, `tank.cpp`): una
   struttura ciascuno, tenuti dal fiume (scorrono con esso; dal passo 13 da `Objects`), con i
   loro movimenti (`Tank::drive`, `Shell::move`, `Missile::move`).
8. Gli oggetti negli slot (`objects.h`, `objects.cpp`): una classe per ogni tipo di
   comportamento (`Explosion`, `Jet`, `Helicopter`, `Balloon`, `Ship`, `Bridge`, `FuelDepot`). Uno
   slot tiene i byte dell'originale; il suo tipo è anche il suo fotogramma di animazione e cambia
   quando viene colpito, così `objectKind(kind)` dà a ogni fotogramma l'oggetto della classe che
   sa che cosa fa quel tipo: la sua animazione, il suo movimento (il jet attraverso lo schermo;
   elicotteri, palloni e navi avanti e indietro una volta svegliati, girando al bordo), se la
   visualizzazione lo verifica contro il bordo.
9. I giocatori (`players.h`): `PlayerData` (l'altro giocatore) con ponti `Bcd<2>` e punti
   `Bcd<3>`, gli stessi tipi per il giocatore attivo, `HighScores` (la tabella e i suoi byte di
   controllo); il pannello (`panel.h`): i suoi 60 caratteri con `print` (cifre BCD),
   `showLives`, `markPlayers`, `showGameNumber`.
10. Il suono (`sound.h`, `sound.cpp`): `Sounds` tiene ciò che il gioco chiede (il bip di avvio, il
    suono dell'evento e la sua durata, esplosione, sparo, bonus, missile, il tono che scende dopo
    uno schianto) e il suo `drive()` è il driver (B2A5–B3D8), a cui l'aereo, la pausa, la
    modalità dimostrativa e il proiettile sono dati tramite un `SoundContext`; anche `PokeyRegs`
    si è spostato lì.
11. Il fiume sullo schermo (`river.h`, `river.cpp`): le sue righe, le stesse senza decorazioni, i
    loro tratti, l'indirizzo della riga in cima nella memoria dello schermo dell'originale e gli
    stati del generatore per salvataggi piccoli; `push()` aggiunge la riga successiva del
    generatore, `rebuild()` le rifà quando si carica uno stato.
12. Niente `GameState`: i suoi campi sono i membri di `Game` stesso (`eachState()` li elenca per
    salvare, caricare e azzerare).
13. Il fiume separato dagli oggetti (`world.h`, `world.cpp`): `RiverGen` fa solo il fiume
    (`RiverState`: rive, isole, sezioni, l'LFSR); gli slot, il carro armato, il suo proiettile e
    il missile sono `Objects`, che piazza i nuovi oggetti dopo ogni riga (`afterLine`,
    $ABE1–$AE93) e dice al generatore dove si trova l'oggetto che sta entrando (`BankObstacle`),
    così la riva lo evita. `World` tiene entrambi e li fa avanzare riga per riga; il suo `State` è
    ciò che tengono i salvataggi e gli stati delle righe del fiume.
14. L'esportazione della RAM dalle classi (`ram_map.h`, `ram_map.cpp`): ogni classe elenca i
    propri byte con i loro indirizzi nel suo `mapRam(RamMap &)` (l'aereo, il proiettile, i suoni,
    le collisioni, i giocatori, i record, il pannello, la scritta scorrevole, il fiume, gli slot,
    il carro armato, il suo proiettile e il missile, la riga in cima allo schermo); `World` e
    `Game` mettono insieme le liste, e `RamMap` le carica, le scrive e le confronta. Ciò che si
    calcola invece di tenerlo (la memoria player-missile, le righe dello schermo dalla display
    list) resta in `game_ram.cpp` e `River`.
15. I documenti: le tabelle dei file dei README e le loro parti su codice, generatore e verifica,
    HANDOFF (il branch), NOTES (i salvataggi), HOW_IT_WORKS (le collisioni), in tutte le lingue;
    i commenti nel sorgente che parlavano ancora del DLI o del fiume che tiene gli oggetti.
16. La fine: tutte le 17 partite uguali (700 000 fotogrammi), anche su una build con
    AddressSanitizer, UndefinedBehaviorSanitizer e le asserzioni della libreria standard, senza
    alcuna segnalazione; questo documento in ogni lingua.
17. `game.cpp` diviso in file per argomento, la classe intatta: `game.cpp` (il fotogramma
    nell'ordine dell'originale), `game_plane.cpp` (l'aereo, le esplosioni, i punti),
    `game_players.cpp` (partite e giocatori, il pannello), `game_reset.cpp` (accensione,
    modalità dimostrativa, nuova partita, reset, l'aereo successivo), `game_state.cpp`
    (salvataggi); ciò che condividono è in `game_internal.h`.
18. La partita (`match.h`, `match.cpp`): `Match` tiene il numero del gioco, di chi è il turno,
    ponti, punti e vite di entrambi i giocatori, `respawned` e i record, con `setUp` (sub
    $AF80), `selectNext`, `swapPlayers` (riceve lo stato del fiume: anche il livello e il punto
    di ripartenza passano all'altro giocatore), `updateHighScore`, `addPoints` (A547: i punti,
    l'aereo bonus, la fine oltre 999 999) e i numeri del pannello (non mostrati nella modalità
    dimostrativa, cosa che gli dice `Game`). I suoi byte seguono l'ordine dei campi di prima,
    quindi i salvataggi restano uguali. Trovato strada facendo: `World::State` aveva due byte di
    riempimento, salvati con ciò che c'era in memoria; ora sono byte zero espliciti, e lo stesso
    stato viene sempre salvato con gli stessi byte.
19. I campi di `Game` privati (`name_`). Fuori, getter costanti (`world()`, `plane()`,
    `match()`, `pokey()`...), `output()` (ciò che un fotogramma mostra e suona: il pannello, il
    carburante, la scritta scorrevole, il logo, lo schermo spento, i registri del suono) e
    alcune azioni con un nome invece di scritture nei campi: `setRiver`, `keepLineStates`,
    `skipState` / `setSkipState` (le ancore del visualizzatore), `playSound` e `stayAwake` (il
    visualizzatore), `ghost` (la modalità fantasma degli strumenti), `setPokey` (`sound_check`).
    `sound_check` carica i byte che il driver legge tramite `mapRam`, gli stessi byte di prima.
20. La console (`console.h`, `console.cpp`): `Console` tiene il contatore dei fotogrammi, i flag
    e il timer della modalità dimostrativa, il tempo di inattività, la pausa, il ritardo di
    SELECT, `powerOnDone` e `screenOn`, con `keys` (A19E-A1F5: attività, pausa, schermo spento),
    `attractTimerTick` (A2F5), `countFrame` (i contatori del VBI, anche per `skipFrame`) e
    `selectRepeat` (A699). Dove l'ordine del fotogramma li mescola al resto (una nuova partita,
    la scelta del gioco, la fine della partita, il VBI PAL prima della fine), `Game` imposta
    ancora i campi da sé. Gli stessi byte nello stesso ordine, quindi i salvataggi restano
    uguali; pausa, SELECT, OPTION, START e lo spegnersi dello schermo sono stati verificati
    contro il commit precedente su 390 000 fotogrammi con questi tasti.
21. Il ciclo principale come passi con un nome, nell'ordine dell'originale e senza spostare
    nulla: `mainLoop` è `console_.keys` (A19E-A1F5), `wakeObject` (A1FF), `driveTank` (A21B),
    `moveObjects` (A26C), `attractTimerRunsOut` (A2F5), `readStick` (A312), `updatePlane`
    (A33F); `mainLoopEnd`, da cui riparte una ripartenza, è `moveMissile` (A5B7), `moveShell`
    (A607), `consoleKeys` (A673), `scroll` (A6DE). Un passo che avvia un reset restituisce true
    e il ciclo si ferma lì, come prima. Alla fine tutte le 17 partite uguali, anche sulla build
    con i sanitizer.
22. Piccole pulizie. In `game.h` ciò che usano solo il visualizzatore e gli strumenti (le parti
    di `frame()`, `skipFrame` e `SkipState`, `attractSound`, `playSound`, `stayAwake`, `ghost`,
    l'immagine della RAM, `setPokey`) è in un unico blocco dopo l'interfaccia del gioco;
    `resetState` ed `eachState` sono privati. Le impostazioni del suono della modalità
    dimostrativa per il visualizzatore sono un solo `AttractSound` (sound.h). `game_internal.h`
    è `namespace rr::game_detail` con `inline constexpr` invece di uno spazio dei nomi anonimo
    in un header. I numeri della partita nel pannello sono `Scoreboard` (match.h), a cui
    partita, pannello e modalità dimostrativa si danno una volta; `showOtherScore` è diventato
    `player2Score`, con il perché stampa i punti di chi ha il turno ($63-$65), come fa sub
    $B068.
23. Velocità: le righe di un oggetto sullo schermo (`DrawnObject`) sono array di dimensione
    fissa (al massimo $80 righe) in un vettore che conserva la sua capacità, invece di due
    vettori di byte rifatti per ogni oggetto a ogni fotogramma. Un fotogramma ora non alloca
    nulla: il 44% di istruzioni in meno, e `display_bench` (1 000 000 di fotogrammi) è più
    veloce di `dli_removed` dell'8–17%, mentre prima era più lento del 25%.
24. Le righe del fiume in buffer circolari (`Recent`, river.h): le righe, le stesse senza
    decorazioni, i loro tratti e gli stati prima di esse (256 ciascuno) accolgono una nuova riga
    senza spostare le altre, mentre un vettore le spostava tutte a ogni riga. Niente più copie
    per riga: 0,54 miliardi di istruzioni per 20 000 fotogrammi (1,17 miliardi prima del passo
    23), e `display_bench` (1 000 000 di fotogrammi) è più veloce di `dli_removed` del 25–40%.

## Prossimi passi

Niente in programma. `Game` è il direttore d'orchestra del fotogramma; ciò che tiene ha le sue
classi (`World`, `Match`, `Console`, `Plane`, `Sounds`...). Una modifica da qui in poi si
verifica allo stesso modo: `compare.sh -quick` dopo ogni passo, tutte le 17 partite alla fine.
