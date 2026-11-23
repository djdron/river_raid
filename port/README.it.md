# River Raid, port in C++

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Polski](README.pl.md) | Italiano | [Español](README.es.md) | [中文](README.zh.md) | [日本語](README.ja.md)

Un progetto amatoriale non ufficiale a scopo didattico, non affiliato ad Activision o Microsoft; non contiene alcuna parte del gioco (vedi l'avvertenza in [../README.it.md](../README.it.md)).

Il gioco completo: `river_raid` gioca a River Raid come l'originale — modalità dimostrativa, scelta del gioco, uno e due giocatori, l'aereo, il carburante, i nemici, i ponti, i punti, le vite, i record, la pausa e il suono. Il generatore del fiume e la logica di gioco producono fotogramma per fotogramma lo stesso stato della RAM dell'originale, il disegno lo stesso schermo (vedi Verifica).

## Compilazione
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
Il nucleo e gli strumenti richiedono solo un compilatore C++17. Il gioco `river_raid` e il visualizzatore `river_view` richiedono SDL3: CMake usa un SDL3 installato (3.4 o successivo) se lo trova, altrimenti durante la configurazione scarica SDL 3.4.20 da GitHub (servono git e internet) e lo collega staticamente. Su Linux, compilare SDL dai sorgenti richiede i pacchetti di sviluppo X11/Wayland (vedi `docs/README-linux.md` di SDL). SDL 3.4 si ferma con un errore quando manca un pacchetto del genere e indica l'opzione che compila senza (per esempio `-DSDL_X11_XCURSOR=OFF`). Del tutto senza SDL: `cmake -S . -B build -DRR_VIEWER=OFF`.

Con SDL2 al posto di SDL3 (per piattaforme che SDL3 non supporta): `cmake -S . -B build -DRR_SDL2=ON`. CMake usa allora un SDL2 installato (2.0.18 o successivo) o scarica SDL 2.32.10. I programmi parlano con SDL solo tramite `sdl/host.h`, implementato una volta per ogni versione; entrambe danno la stessa immagine. Il gioco come app UWP per Windows e Xbox: [uwp/README.it.md](uwp/README.it.md). Come homebrew per Nintendo Switch: [switch/README.it.md](switch/README.it.md).

Su Windows con Visual Studio: `cmake --build build --config Release`, i programmi finiscono in `build\Release\`. Compilato e provato su Windows (MSVC), macOS (Xcode, Apple M2) e Linux (gcc).

## Il gioco
```
river_raid [rom] [opzioni]
```
Senza argomenti trova la ROM da solo (vedi `rom`) e parte come la cartuccia: modalità dimostrativa, poi START.

| Opzione | Cosa fa |
|---|---|
| `rom` | l'immagine della cartuccia (8192 byte). Di default `rom/River_Raid.rom` viene cercato nella cartella corrente e fino a tre cartelle sopra, così parte senza argomenti dal repository o da `port/build/Release` |
| `-os file` | lettere e cifre dal sistema operativo dell'Atari stesso: un'immagine della ROM del sistema (10 KB o 16 KB) o un font da 1 KB. Senza, si usa il font AltirraOS incorporato |
| `-seed N` | un altro fiume, con altri nemici e depositi. N è lo stato da cui parte il generatore del fiume, da 1 a 16777215. Il gioco 1 parte dall'inizio di quel fiume, i giochi 3–8 dai suoi ponti 5, 20 e 50; ogni gioco mantiene il suo livello abituale, quindi la difficoltà cresce come sempre. Il fiume della cartuccia stessa è `-seed 16241891` |
| `-bridge N` | ogni gioco (1–8) parte dal ponte N, da 1 a 99999: con N sul contatore dei ponti (oltre 9999 le sue ultime 4 cifre, perché il contatore dell'originale ricomincia da capo) e il livello di quel ponte (dopo l'80 il livello alterna 79/80, come nell'originale). Funziona anche con `-seed`. La partenza si trova facendo girare il generatore del fiume fino a lì (meno di un secondo per 9999, circa 8 per 99999) |
| `-random N` | il valore iniziale del contatore RANDOM di POKEY, che decide quale nemico si sveglia quando e il rumore delle esplosioni. Di default è preso dall'orologio, come quello della macchina vera dipende dal momento dell'accensione, quindi ogni esecuzione è diversa; con lo stesso N e gli stessi input un'esecuzione si ripete esattamente |
| `-pal` | gioca come una macchina PAL (Europa): 49,86 fotogrammi al secondo invece di 59,92, quindi tutto il gioco va circa il 17% più lento, come su un televisore europeo; il pannello disposto come lo mostra una macchina PAL (più alto); suoni un po' più bassi; i depositi di carburante diventano più rari fino al livello 47 invece che al 46; colori PAL. Su uno schermo a 50 o 100 Hz il gioco va al suo ritmo |
| `-colours ntsc\|pal` | i colori di una macchina NTSC o PAL; PAL di default. C cambia durante il gioco |
| `-palette altirra\|atari800` | il modello dei colori: `altirra` (default) o `atari800` (toni medi più scuri). V cambia durante il gioco |
| `-border` | mostra l'intero fotogramma con il suo bordo nero (384×240 meno 10 colonne per lato), come fanno gli emulatori. Di default la finestra mostra solo l'immagine del gioco con tutta la scritta scorrevole, 364×200 (`-pal`: 364×213), quindi a schermo intero è circa il 20% più grande, e una finestra è altrettanto più bassa. O cambia durante il gioco |
| `-filter sharp\|nearest\|linear` | come l'immagine viene scalata alla finestra o allo schermo: `sharp` (default) scala per numeri interi con pixel netti e poi in modo morbido fino alla dimensione finale, così resta nitida e ogni pixel ha la stessa dimensione; `nearest` sono blocchi semplici, ma a una scala non intera (sempre in orizzontale, perché i pixel dell'Atari non sono quadrati) alcune colonne risultano un pixel dello schermo più larghe; `linear` è morbido e sfocato. L cambia durante il gioco |
| `-fullscreen` | parte a schermo intero (F cambia; con `-fullscreen_exclusive` nella sua modalità) |
| `-fullscreen_exclusive` | lo schermo intero (F) porta lo schermo in una modalità esclusiva alla risoluzione del desktop e a 60 Hz, 50 Hz con `-pal`, o un loro multiplo (120, 100...), quello più vicino alla frequenza del desktop (P cambia insieme alla macchina), così il gioco vi si aggancia; se lo schermo non ne ha nessuna, lo schermo intero resta sul desktop così com'è. Uscendo dallo schermo intero torna la modalità del desktop. Non su Wayland, che non lascia ai programmi cambiare la modalità dello schermo |
| `-stats` | stampa il renderer e la frequenza dello schermo, poi ogni secondo la frequenza dei fotogrammi, il tempo per fotogramma e la coda del suono |
| `-nosound` | niente suono |

Il titolo della finestra mostra la macchina (PAL o NTSC), i colori e la scalatura in uso. Su uno schermo a 60 Hz (o 120 Hz...) il gioco va esattamente alla frequenza dello schermo (60 invece dei 59,92 fotogrammi al secondo dell'Atari), così ogni fotogramma resta a schermo per lo stesso tempo; su altri schermi alla frequenza dell'Atari.

Esempi: `river_raid -bridge 30` parte dal ponte 30; `river_raid -seed 12345` è un fiume diverso; `river_raid -seed 12345 -bridge 80` è quel fiume al massimo della difficoltà.

Dopo l'accensione la modalità dimostrativa sorvola il fiume. START inizia una partita; SELECT (o OPTION) sceglie i giochi 1–8: i numeri pari sono giochi a due (i giocatori si danno il turno), i giochi 3–4, 5–6 e 7–8 partono dai ponti 5, 20 e 50. Qualsiasi tasto della tastiera dell'Atari mette in pausa. Dopo la fine della partita lo schermo resta fermo finché la modalità dimostrativa non riparte (circa 35 s). Il gioco e il visualizzatore si chiudono nel modo solito: chiudendo la finestra, Alt+F4, Cmd+Q.

| Tasto | Atari |
|---|---|
| frecce o W A S D | joystick: sinistra/destra sterza, su più veloce, giù più lento; qualsiasi direzione fa partire l'aereo al ponte |
| Ctrl sinistro/destro, Z, X | fuoco |
| F4 o Invio | START |
| F3 | SELECT |
| F2 | OPTION |
| Spazio | un tasto della tastiera: pausa |
| F, F11 o Alt+Invio | schermo intero |
| C | colori NTSC / PAL |
| V | modello di colore Altirra / atari800 |
| O | solo l'immagine del gioco / l'intero fotogramma con il bordo |
| P | l'altra macchina, NTSC / PAL, riaccesa (come `-pal`; sull'Xbox il televisore passa a 50 Hz e ritorna) |
| L | scalatura: sharp / nearest / linear |
| F7 / F8 | salvare lo stato del gioco / tornarci (in memoria, fino all'uscita) |
| Esc | esce dallo schermo intero |
| clic del mouse | nasconde il puntatore finché il mouse non si muove (lo stesso passando a schermo intero) |

La tastiera comanda entrambi i joystick, così anche una persona sola può fare una partita a due. Gamepad: il primo è il joystick 1 (e il 2 finché non c'è un secondo), il secondo è il joystick 2; croce direzionale o levetta sinistra, A/B/Y o il grilletto destro per sparare, X = pausa, Start = START, Back = SELECT, RB / LB = F7 / F8 (salvare lo stato / tornarci), LT = L (scala), Back e Start insieme = P (l'altra macchina). La levetta sinistra va secondo il suo angolo, quindi tirarla indietro un po' di lato rallenta soltanto; per sterzare va spinta più a fondo (55%) che per accelerare o rallentare (30%).

## Visualizzatore del fiume
```
river_view [rom] [opzioni]
```
Mostra la modalità dimostrativa dell'originale, il volo sul fiume che parte quando nessuno preme START: dopo l'accensione lo schermo si riempie del fiume del gioco 1, che poi scorre di 1 riga per fotogramma; navi ed elicotteri si svegliano e si muovono, i carri armati vanno verso i ponti e sparano, gli elicotteri lanciano missili (la logica di gioco portata, `src/game*.cpp`); sotto ci sono il pannello e la riga che scorre con i titoli, "Press START key" e il logo arcobaleno di Activision. Le opzioni sono quelle del gioco: `rom`, `-os`, `-seed`, `-bridge` (il visualizzatore prima ci arriva in volo dall'inizio del fiume, solo con il fiume, per 9999 qualche secondo, e lo mostra come sarebbe nel volo; si può guardare indietro fino all'inizio), `-random`, `-pal`, `-colours`, `-palette`, `-border`, `-filter`; e la sua `-sound`: la modalità dimostrativa con il suono (vedi M).

Le lettere e le cifre 1–9 vengono dal font del sistema operativo, che non è nella cartuccia. Il port ha incorporato il font di AltirraOS: il sistema operativo sostitutivo libero distribuito con atari800 (licenza: copia e modifica permesse mantenendo l'avviso di copyright, vedi `src/os_font.cpp`); anche le tracce di riferimento sono state fatte con esso. `-os file` prende il font del sistema dell'Atari stesso da un'immagine della ROM del sistema (10 KB o 16 KB) o da un file di font da 1 KB.

| Tasto | Azione |
|---|---|
| Spazio | pausa |
| Su / Giù | più veloce / più lento, passando per l'arresto fino al volo all'indietro (da x1/4 a x16 in entrambi i sensi) |
| rotella del mouse, PagSu / PagGiù | far scorrere il fiume a mano, di uno schermo |
| Backspace | torna alla riga più recente a velocità normale |
| B / Maiusc+B | ponte successivo / precedente |
| Alt+B / Alt+Maiusc+B | 10 ponti avanti / indietro |
| Ctrl+B / Ctrl+Maiusc+B | 100 ponti avanti / indietro |
| Home | al ponte 1, all'inizio del fiume |
| F1 / F2 / F3 | prova del suono: esplosione / sparo / suono dei punti tramite il driver audio del gioco (la modalità dimostrativa in sé è muta, come nell'originale) |
| F | schermo intero |
| C | colori NTSC / PAL |
| V | modello di colore Altirra / atari800 |
| O | solo l'immagine del gioco / l'intero fotogramma con il bordo |
| L | scalatura: sharp / nearest / linear |
| M | suono sì / no: proiettili dei carri armati, missili degli elicotteri e il motore di un aereo, come nel gioco (non nell'originale, dove la modalità dimostrativa è muta); andando avanti a qualsiasi velocità, il motore la segue come un aereo che accelera o rallenta; indietro il suono va al contrario, come un nastro |
| Esc | esce dallo schermo intero |
| clic del mouse | nasconde il puntatore finché il mouse non si muove (lo stesso passando a schermo intero) |

Confronto con gli schermi dell'originale (modalità dimostrativa, fotogrammi 1000–2300, con i valori RANDOM dell'originale): l'area del fiume e il pannello coincidono pixel per pixel, la riga che scorre anche, tranne le colonne esterne (a sinistra di 24, a destra di 359), dove l'emulatore gestisce diversamente lo scorrimento fine sul bordo.

Quale oggetto si sveglia quando lo decide il registro RANDOM di POKEY, un contatore hardware letto in un momento che dipende dalla temporizzazione dell'intera macchina. Il port modella il contatore (`src/pokey_random.h`) e lo fa partire dall'orologio, come la macchina vera lo fa partire all'accensione, quindi ogni esecuzione è diversa, come sulla macchina vera; `-random N` ripete un'esecuzione.

## Contenuto
| File | Cosa |
|---|---|
| `src/rom.h` | l'immagine della cartuccia e funzioni d'accesso con nome per ogni tabella che il port ne legge (forme, colori, punti, tabelle del generatore, testo del pannello...); tipi di oggetto |
| `src/fixed.h` | i piccoli tipi numerici del gioco, tenuti come li tiene l'originale: byte che si avvolgono, un byte di frazione il cui riporto fa avanzare un intero, contatori BCD (`Bcd<N>`); li verifica `tools/fixed_check.cpp` |
| `src/river_gen.h/.cpp` | il generatore del fiume: `RiverGen::startGame(n)`, poi `nextLine(line, bank, obstacle)` — una riga dello schermo per chiamata; degli oggetti sa solo dove si trova quello che sta entrando in cima (`BankObstacle`) |
| `src/objects.h/.cpp` | gli oggetti: `Objects` tiene gli 11 slot, il carro armato, il suo proiettile e il missile dell'elicottero e piazza nuovi oggetti man mano che arriva il fiume (`afterLine`); una classe per ogni tipo di comportamento (`Jet`, `Helicopter`, `Balloon`, `Ship`, `Bridge`, `FuelDepot`, `Explosion`) li anima e li muove |
| `src/tank.h/.cpp` | il carro armato sulla strada della riva, il suo proiettile e il missile dell'elicottero, con i loro movimenti |
| `src/world.h/.cpp` | `World`: il fiume e gli oggetti insieme, una riga alla volta; `World::State` è ciò che ne tengono i salvataggi |
| `src/river.h/.cpp` | il fiume sullo schermo: le sue righe, le stesse senza decorazioni e come tratti di un solo colore (per le collisioni), l'indirizzo della riga in cima nella memoria dello schermo dell'originale |
| `src/river_draw.h/.cpp` | disegno di una riga come sullo schermo: tonalità della riva, strade ai ponti, sprite degli oggetti (`drawScreenLine`) |
| `src/plane.h`, `src/bullet.h` | l'aereo (guida, velocità, carburante, stato) e il suo proiettile |
| `src/match.h/.cpp` | la partita: numero del gioco, di chi è il turno, punti, ponti e vite di entrambi i giocatori, i record; i punti e gli aerei bonus; come appaiono nel pannello |
| `src/players.h`, `src/panel.h` | punti, ponti e vite dell'altro giocatore, i record con i loro byte di controllo; i 60 caratteri del pannello di stato |
| `src/sound.h/.cpp` | i suoni che il gioco richiede e il driver che li suona (VBI B2A5–B3D8) |
| `src/palette.h/.cpp` | colori Atari delle macchine NTSC e PAL (`atariRgb`): i modelli di Altirra e atari800 |
| `src/game.h`, `src/game*.cpp` | il gioco: ciclo principale e VBI dell'originale fotogramma per fotogramma — modalità dimostrativa, tasti della console, l'aereo, il carburante, le collisioni, le esplosioni, i punti, le vite, i giocatori, morte e ripartenza, fine partita, record; contiene le parti qui sopra |
| `src/display.cpp` | la parte del fiume nella visualizzazione: ogni oggetto all'altezza del suo slot, le collisioni come semplice geometria (senza interrupt della display list, vedi [docs/DLI_REMOVAL.it.md](../docs/DLI_REMOVAL.it.md)), l'immagine dell'area del fiume |
| `src/pokey_random.h` | il contatore RANDOM di POKEY |
| `src/pokey_sound.h/.cpp` | il suono di POKEY: quattro canali, divisori e contatori del rumore, generati ciclo per ciclo |
| `src/status_bar.h/.cpp` | pannello e riga che scorre: set di caratteri, pannello, passo dello scorrimento, disegno |
| `src/os_font.h/.cpp` | font del sistema operativo: AltirraOS incorporato o caricato da una ROM del sistema |
| `src/ram_map.h`, `src/ram_map.cpp`, `src/game_ram.cpp` | gli indirizzi della RAM dell'originale per i byte del gioco: ogni classe elenca i propri (`mapRam`), `RamMap` li carica, li scrive e li confronta (per la verifica) |
| `game/river_raid.cpp` | il gioco (SDL3): immagine, suono, tastiera e gamepad |
| `viewer/river_view.cpp` | il visualizzatore del fiume (SDL3), vedi sopra |
| `sdl/host.h`, `host_sdl3.cpp`, `host_sdl2.cpp` | finestra, immagine, suono, tastiera e gamepad per il gioco e il visualizzatore, su SDL3 o SDL2; occultamento del puntatore del mouse |
| `tools/river_map.cpp` | genera il fiume e salva un PNG come lo mostra il gioco: `river_map <rom> <gioco 1..8> <righe> out.png [-pal] [-flat]` (`-flat`: solo la bitmap, colori fissi) |
| `tools/river_dump.cpp` | scrive il fiume generato come testo (colore di ogni punto, oggetti piazzati) per `tools/doc_figures.py`, che disegna le immagini di [docs/HOW_IT_WORKS.it.md](../docs/HOW_IT_WORKS.it.md): `python tools/doc_figures.py rom/River_Raid.rom` (richiede matplotlib) |
| `tools/sound_check.cpp` | confronta il driver audio con le scritture su POKEY dell'originale (`rr_oracle -vbi`) |
| `tools/sound_render.cpp` | trasforma un registro di scritture su POKEY in WAV con il POKEY del port, per confrontarlo con il suono dell'emulatore (`rr_oracle -wav`) |
| `tools/game_check.cpp` | confronta il gioco con una traccia della RAM dell'originale fotogramma per fotogramma, con gli input e i valori RANDOM dell'originale (e l'area del fiume con i suoi schermi) |
| `tools/status_check.cpp` | confronta pannello e riga che scorre con schermi completi dell'originale (`rr_oracle -screens A:B -ram A:B`) |
| `tools/render_check.cpp` | confronta il disegno con lo schermo dell'originale (`oracle/rr_oracle -ghost -strip 20`) |
| `tools/terrain_check.cpp` | confronto con una traccia di riferimento (`oracle/rr_oracle -ram A:B`) |
| `tools/equivalence/` | verifica del refactoring: un commit più vecchio del port contro la copia di lavoro, fotogramma per fotogramma (vedi Codice) |
| `tools/game_trace.cpp`, `tools/trace/` | una riga per fotogramma di ciò che fa il gioco, con i suoi nomi; `tools/trace/compare.sh` gioca 17 partite registrate e le confronta fotogramma per fotogramma con un commit di riferimento (`-quick`: 5 di esse): la verifica per le modifiche che non devono cambiare il gioco (vedi Codice) |
| `tools/display_bench.cpp` | fa girare il gioco per molti fotogrammi con un pilota automatico (tempi, partite lunghe) |
| `tools/fixed_check.cpp` | verifica i tipi numerici di `src/fixed.h` su ogni ingresso contro il codice che hanno sostituito |

Il generatore legge tabelle e grafica dalla ROM dell'utente (`rom/River_Raid.rom`); nel codice non ci sono dati del gioco.

## Codice
Il port è scritto per essere letto senza avere a portata di mano il listato dell'originale:
- il gioco è fatto di oggetti che tengono il proprio stato e sanno che cosa fanno: l'aereo, il suo proiettile, il fiume (`RiverGen`), gli oggetti sul fiume (`Objects`, una classe per ogni tipo di comportamento), il carro armato, il suo proiettile e il missile, i giocatori, il pannello, i suoni; `World` fa avanzare il fiume e i suoi oggetti riga per riga, `Game` tiene tutto ed esegue il fotogramma come l'originale ([docs/OOP.it.md](../docs/OOP.it.md): come il codice ci è arrivato);
- campi, funzioni e costanti hanno nomi che dicono che cosa significano (`plane.state`, `shell.timer`, `enteringSlot`, `positionObject`, `kFacingLeft`); il commento di ogni campo dà il suo indirizzo nella RAM dell'originale e quello di ogni funzione gli indirizzi del codice da cui viene, così il listato (`disasm/`) e le tracce si possono ancora seguire; ogni classe elenca anche i propri byte con i loro indirizzi (`mapRam`), così lo stato si può confrontare con la RAM dell'originale;
- le tabelle della cartuccia si leggono tramite funzioni d'accesso con nome in `src/rom.h` (`rom.points(type)`, `rom.objectHeight(type)`, `rom.shellRise(t)`...), non per indirizzo;
- i valori restano a 8 bit con ritorno a capo dove lo sono nell'originale (`lo()`), perché le posizioni fanno il giro dello schermo; dove l'originale porta il flag di riporto del 6502 da una somma alla successiva, `addWithCarry`/`subtractWithBorrow` lo dicono esplicitamente; i contatori BCD (`Bcd<N>`, `src/fixed.h`) sommano come somma il 6502 in modo decimale, e non c'è virgola mobile;
- la visualizzazione non ha interrupt della display list: ogni oggetto sta all'altezza del suo slot e le collisioni sono semplice geometria (`display.cpp`); il gioco differisce dall'originale nello 0,01–0,25% dei fotogrammi, per un contatto che l'originale vede un fotogramma dopo o per niente ([docs/DLI_REMOVAL.it.md](../docs/DLI_REMOVAL.it.md)).

Le modifiche che non dovrebbero cambiare il comportamento si verificano con `tools/equivalence`: compila un commit più vecchio (da 06ffc73 in poi) accanto alla copia di lavoro e li esegue entrambi su 150 000 righe di fiume di ogni gioco (NTSC e PAL), su partite casuali dall'accensione e da 194 stati delle tracce dell'originale, confrontando dopo ogni passo l'intero stato, lo schermo e i registri audio. La rinomina e la ristrutturazione di generatore, gioco e visualizzazione sono state verificate così (0 differenze), oltre che con tutte le verifiche qui sotto.

```
port/tools/equivalence/build.sh 06ffc73
port/build/eq/eq
```

La riscrittura a oggetti (branch `oop`) si verifica con `tools/trace/compare.sh`: `game_trace` scrive una riga per fotogramma di ciò che fa il gioco (l'aereo, il suo proiettile, il carro armato, il suo proiettile, il missile, gli slot, punti, vite, il fiume, i registri del suono e un hash dell'immagine) con i nomi del gioco stesso, così si possono confrontare versioni del port che tengono lo stato in modo diverso. Lo script gioca 17 partite (NTSC e PAL, uno e due giocatori, la modalità dimostrativa, schianti e fine partita, un pilota automatico che vola sopra le rive) e le confronta fotogramma per fotogramma con il commit di riferimento in `tools/trace/REFERENCE`; `-quick` ne gioca 5. Ogni passo della riscrittura ha dato le stesse partite di `dli_removed`.

```
port/tools/trace/compare.sh -quick rom/River_Raid.rom
```

## Come funziona il generatore
Ogni chiamata a `World::nextLine()` produce una nuova riga in cima allo schermo:
1. `countLines`: conteggio delle righe (un blocco è 32 righe, una sezione 2 blocchi, 16 sezioni fino al ponte), passo dell'LFSR, un nuovo obiettivo di larghezza all'inizio di una sezione (`chooseSectionWidth`: le isole si aprono e si chiudono);
2. `changeIsland`: l'isola e la larghezza dell'acqua si muovono gradualmente verso l'obiettivo;
3. `moveBank`: il cammino casuale della riva, con inversione ai limiti e davanti al nuovo oggetto (`bankWouldHitObject`);
4. `startDecoration`: una casa o una montagna sulla riva;
5. `drawLine`: i 48 byte della riga (riempimenti, pezzi di bordo dalla tabella a `$B800`, una riga di decorazione);
6. poi gli oggetti (`Objects::afterLine`): gli slot scendono di una riga e ogni 16 righe viene piazzato un nuovo oggetto (`chooseEnemy`, `positionObject`, `placeTank`). Il generatore degli oggetti sa solo dove si trova il più nuovo (`BankObstacle`), che il passo 3 evita.

La generazione non dipende né dal giocatore né da `RANDOM`. Lo si è verificato sull'esecuzione di riferimento: seed POKEY diversi, e un'esecuzione del bot rispetto a un volo senza input, danno lo stesso stato su ogni riga.

## Verifica
Le verifiche contro l'originale qui sotto sono state fatte sul branch `main`, la cui visualizzazione esegue l'interrupt della display list dell'originale. Questo branch gioca esattamente come `dli_removed` (`tools/trace/compare.sh`), che differisce dall'originale solo nelle collisioni ([docs/DLI_REMOVAL.it.md](../docs/DLI_REMOVAL.it.md)).

- Partendo dallo stato di riferimento: 119 758 righe dell'intero fiume (120 000 fotogrammi), 0 differenze nello stato e nelle righe della bitmap.
- Da zero (`startGame(1)`, senza emulatore): dopo le 165 righe che riempiono lo schermo lo stato coincide con l'originale, poi 29 757 righe senza differenze.
- Partita del bot con ponti distrutti e l'effetto lampo: differenze solo nei fotogrammi di ricomparsa. Vengono dal codice di ripartenza, non ancora portato.
- La verifica scopre gli errori introdotti: ognuno di quattro guasti voluti (condizione del cammino, larghezza dell'acqua, disegno, colore di riempimento) ha prodotto differenze.

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/ghost_film.txt 30001 out -ghost -ram 0:30000
port/build/terrain_check rom/River_Raid.rom out/ram.bin 0 240 29760 5 -fresh 1 145
```

Disegno: in ogni fotogramma in cui una nuova riga entra nella riga 20 dello schermo, la riga dell'originale viene confrontata con `drawScreenLine()` della riga del generatore C++ (avviato da zero).
- Con gli oggetti dove li ha piazzati il generatore: 96,8% delle righe identiche su 29 000 fotogrammi (95,3% su 120 000). Le altre differiscono dove il gioco ha spostato un oggetto.
- Con le posizioni degli oggetti prese dal gioco in esecuzione (`-ram`, x dello slot, flag, tipo): 98,1% identiche; ogni riga rimanente differisce solo mentre è attivo il carro armato sulla strada (`$38`), che disegna la logica di gioco. 0 righe non spiegate su 29 600.

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/ghost_film.txt 30001 out -ghost -strip 20 -ram 0:30000
port/build/render_check rom/River_Raid.rom out/strip.bin out/frames.csv -ram out/ram.bin
```
Pannello e riga che scorre: i set di caratteri costruiti dal font del sistema e dalla cartuccia sono uguali alla RAM dell'originale, lo stato dello scorrimento ($3FB3/$26/$27) coincide in ogni fotogramma e le righe disegnate 180–239 sono identiche in tutti i 1299 fotogrammi, tranne le colonne esterne della riga che scorre (vedi Visualizzatore del fiume).

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/attract_idle.txt 2300 out -screens 1000:2299 -ram 1000:2299
port/build/status_check rom/River_Raid.rom out/screens.bin out/ram.bin
```
Fotogramma di gioco nella modalità dimostrativa (`game_check`): avviato dallo stato dopo l'accensione (`Game::startAttract`) e con i valori RANDOM dell'originale, il port fa 19 900 fotogrammi (fino al ponte 20) senza alcuna differenza dalla RAM dell'originale in nessun byte confrontato: stato del generatore, slot degli oggetti (posizione, direzione, tipo, bit di collisione), carro armato, il suo proiettile, missile dell'elicottero, risultati delle collisioni, memoria player-missile, le righe del fiume sullo schermo. L'area del fiume disegnata da esso è uguale agli schermi dell'originale in tutti i 1299 fotogrammi confrontati.

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/attract_idle.txt 20000 out -strip 20
port/build/game_check rom/River_Raid.rom out/ram.bin out/random.log 38 19900 -attract
```

Il gioco con un giocatore (`game_check` su esecuzioni con input). Dallo stato della modalità dimostrativa il port riceve gli input e i valori RANDOM dell'originale e va avanti da solo, senza mai essere ricaricato; a ogni fotogramma la sua RAM viene confrontata con quella dell'originale (generatore, slot, aereo, carburante, proiettile, collisioni, punti, vite, entrambi i giocatori, record e i loro byte di controllo, pannello, riga che scorre, memoria player-missile, righe dello schermo):
- partita del bot a un giocatore (`start_only.txt -bot -seed 3`, 8 950 fotogrammi: modalità dimostrativa, START, 4 aerei, ponti, morti e ripartenze, fine partita, di nuovo modalità dimostrativa): identica in ogni fotogramma confrontato;
- partita a due con SELECT e il tasto di pausa (`two_players.txt`, fino alla fine della partita al fotogramma 3 007): identica fino alla fine della partita (un fotogramma, vedi sotto);
- l'area del fiume disegnata dal port (aereo, proiettile, nemici, esplosioni, carro armato, proiettili) è uguale agli schermi dell'originale in tutti i 1 565 fotogrammi confrontati di una partita del bot.

La macchina PAL (`rr_oracle -pal`, `game_check -pal`): gli stessi controlli su esecuzioni dell'originale come Atari PAL: modalità dimostrativa (2 974 fotogrammi), una partita del bot fino al game over (5 974), una partita a due giocatori (7 774), l'area del fiume sugli schermi (2 546 fotogrammi), il driver del suono (2 991) e il pannello con la riga che scorre (1 299): nessuna differenza. Così si è trovato anche cosa cambia un fotogramma PAL oltre alla display list: con 312 righe la CPU ha più tempo per fotogramma, quindi l'accensione arriva alla modalità dimostrativa al fotogramma 25 invece che al 38 e sub $B0F1 cancella la memoria in 8 fotogrammi invece di 13; e poiché l'interrupt di display del pannello finisce 12 righe dopo, al game over il VBI arriva prima di sub $B21A.

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/start_only.txt 6000 out -pal -bot
port/build/game_check rom/River_Raid.rom out/ram.bin out/random.log 25 5974 -attract -pal
```

Dove il risultato dell'originale dipende dai cicli della CPU:
- Il DLI (`display.cpp` nel branch `main`) viene eseguito contando i cicli, come l'emulatore di riferimento scandisce una riga: all'inizio gli accessi player/missile e alla display list, la CPU fino al ciclo 28, poi viene disegnata l'intera riga (collisioni comprese), poi gli accessi ai dati dello schermo, WSYNC al ciclo 106. Così si sa se una lettura di un registro di collisione vede già la riga corrente: un proiettile che tocca un oggetto solo sulla riga della lettura conta su un ponte (quel percorso legge dopo il ciclo 28) e non altrove, come nell'originale.
- L'istantanea della RAM viene presa alla fine del fotogramma; di solito il ciclo principale dell'originale sta allora aspettando il fotogramma successivo (AE99–AE9E). Subito dopo l'inizio di una partita, mentre il fiume viene riempito di nuovo a 2 righe per fotogramma, a volte sta ancora disegnando la seconda riga (1 fotogramma per riempimento). `game_check` segnala questi fotogrammi a parte e non li confronta; il fotogramma successivo è di nuovo identico.
- Non modellato: la fine della partita. La logica di gioco del ciclo principale inizia alla riga 160 e divide la CPU con il DLI e il VBI (riga 248). Nel fotogramma della fine della partita il controllo del record e i suoi byte di controllo (sub $B261, $B298) richiedono migliaia di cicli, quindi `sub_B21A` imposta il contatore dei fotogrammi a $FF prima del VBI, dopo di esso o (partita a due) solo nel fotogramma seguente, a seconda dei cicli di tutto ciò che lo precede in quel fotogramma. Il port lo fa prima del VBI; allora il contatore dei fotogrammi può differire di uno, il che sposta di un fotogramma la modalità dimostrativa seguente. Riprodurlo richiederebbe il conteggio dei cicli dell'intero ciclo principale in quel fotogramma (compreso il gestore degli interrupt del sistema).

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/start_only.txt 9000 out -bot -seed 3
port/build/game_check rom/River_Raid.rom out/ram.bin out/random.log 38 8950 -attract
```

Suono. Il driver audio (VBI B2A5–B3D8, `Game::soundDriver`) è stato eseguito sulla RAM che l'originale aveva all'inizio di ogni VBI di una partita del bot (2989 fotogrammi, 2199 con suono: motore, avviso di carburante basso, esplosioni, spari, punti, proiettile del carro): ogni valore AUDF/AUDC è uguale a quello scritto dall'originale. Il modello di POKEY (`src/pokey_sound.cpp`) è stato confrontato con il suono di atari800 per le stesse scritture nei registri: lo stesso volume nel tempo (correlazione degli inviluppi 0,95), gli stessi toni puri alla stessa altezza, rumore con la stessa forma dello spettro (coseno degli spettri smussati 0,94); l'andamento del rumore campione per campione è diverso, perché dipende da dove si trovano in quel momento i contatori del rumore.

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/start_only.txt 3000 out -bot -wav -vbi
port/build/sound_check rom/River_Raid.rom out/vbi.bin out/pokey.log out/random.log
port/build/sound_render out/pokey.log port.wav
```

## Non ancora fatto
- L'accensione vera e propria (A000–A17C: i primi 38 fotogrammi prima della modalità dimostrativa) viene disegnata come schermo vuoto; `Game::powerOn` non è ancora esatto.
- I colori seguono i modelli dell'uscita GTIA degli emulatori (Altirra di default, atari800 con V; entrambi verificati byte per byte con il codice della tavolozza degli emulatori stessi alle impostazioni di default, atari800 esteso dai suoi livelli video 16–235 a 0–255), non una tavolozza misurata su un televisore vero; quanto appaia luminoso dipende dalle impostazioni del televisore tanto quanto dalla macchina.
