# Esperimenti: il gioco senza il modello del DLI

[English](DLI_REMOVAL.md) | [Русский](DLI_REMOVAL.ru.md) | [Українська](DLI_REMOVAL.uk.md) | [Deutsch](DLI_REMOVAL.de.md) | [Polski](DLI_REMOVAL.pl.md) | Italiano

Branch `dli_removal` e `dli_removed`. Lo scopo è una visualizzazione più semplice per i port su
macchine lente (ZX Spectrum Next, TS-Conf): sprite disegnati come immagini pronte nelle loro
posizioni, collisioni calcolate per oggetto, nessun modello riga per riga dell'interrupt della
display list (DLI). In `dli_removal` il gioco resta lo stesso dell'originale fotogramma per
fotogramma, e ogni passo è verificato contro l'originale (`rr_oracle` su libatari800,
`game_check`). `dli_removed` va un passo oltre: niente DLI, le collisioni come semplice geometria,
che differisce dall'originale in pochi fotogrammi su mille (l'ultima parte, «Branch
`dli_removed`»). I port per ZX ([river_raid_zx](https://github.com/djdron/river_raid_zx)) seguono
`dli_removed`.

## Passo 1: collisioni con la riva sulle righe senza decorazioni

Il DLI legge le collisioni con il playfield di: l'aereo (P2PF, solo il colore della riva), gli
oggetti (P3PF, solo il colore del bordo: navi ed elicotteri ci girano), il carro (P1PF, il bordo),
il proiettile e il missile dell'elicottero (M2PF, M0PF: riva o bordo). Case e strade (le
decorazioni, `decoRow`) sono disegnate nel colore del bordo e nel colore di sfondo dentro la riva.

`RiverGen::nextLine(line, bank)` dà una seconda riga, la stessa senza decorazioni (dove sono
disegnate, il riempimento della riva); `Game::bankLines` le tiene accanto a `screen`, e le
collisioni guardano quelle (`bankRuns`, attivo per impostazione predefinita; `RR_BANK_RUNS=0` lo
schermo come prima). Senza decorazioni ogni riga del fiume ha una di quattro forme (480 000 righe
delle partite 1–8):

| forma (sequenze di colore) | righe | confini |
|---|---|---|
| riva, bordo, acqua, bordo, riva | 398 644 | 4 |
| lo stesso con un'isola | 80 324 | 8 |
| con un'isola stretta (solo colore del bordo) | 1 016 | 6 |
| le righe di un ponte colpito (la riva nel colore del bordo) | 16 | 2 |

Quindi una riga si descrive con 2–8 numeri.

Verificato con `game_check` contro esecuzioni dell'originale: la modalità attract (19 900
fotogrammi), una partita del bot (8 950), una partita del bot in PAL (5 974), la partita 7 dal
ponte 50 in NTSC (2 esecuzioni, fino alla fine partita) e PAL (8 950 fotogrammi): nessuna
differenza, a parte un bit che nessuno legge. I jet volano sopra la terra e toccano le
decorazioni: il bit «tocca il bordo» del loro slot differisce (nella modalità attract in 341
fotogrammi), ma moveObjects non lo legge mai per un jet. `game_check` non confronta quel bit per i
jet quando `bankRuns` è attivo. `RR_BANK_DEBUG=1` stampa ogni riga in cui i due tipi di riga danno
collisioni diverse (erano tutti jet, tipo 7, player 3).

Esecuzioni della partita 7 dall'accensione: `oracle/scripts/start_game7.txt` con `-bot`. In NTSC il
confronto si ferma alla prima fine partita, che il port non modella (README).

## Passo 2: collisioni per estensioni

`collideSpans` (il predefinito; `RR_COLLIDE=masks` per le maschere di pixel dell'intera riga come
prima): ogni sprite sulla riga di scansione è i suoi bit di grafica e i cicli di colore dal primo
all'ultimo bit acceso. Contro il playfield si verifica sequenza per sequenza (la riga senza
decorazioni, 5–9 sequenze di un colore); una sequenza che va oltre il primo o l'ultimo bit acceso è
un contatto, una strettamente in mezzo si guarda bit per bit. Due sprite: prima le loro estensioni,
bit per bit solo dove si incontrano. Ancora per riga di scansione, dentro il raggio: cosa contiene
ogni riga e quando il DLI legge i registri è quello del raggio.

Le stesse esecuzioni di sopra: nessuna differenza dall'originale. `RR_STATS=1` conta i casi;
`RR_ROUGH_BANK=1` e `RR_ROUGH_PAIRS=1` tolgono il passo bit per bit (un contatto ovunque le
estensioni si incontrino), per vedere se serve:

| esecuzione | righe con sprite | una sequenza tra il primo e l'ultimo bit (in un buco) | estensioni incontrate (non i bit) |
|---|---|---|---|
| modalità attract | 1 164 849 | 86 228 (0) | 26 862 (70) |
| bot PAL | 278 257 | 9 201 (96) | 4 108 (0) |
| partita 7, PAL | 536 802 | 24 494 (96) | 7 693 (0) |

- La riva: le estensioni bastano. Con `RR_ROUGH_BANK` tutte le esecuzioni restano identiche
  all'originale; le sequenze cadute in un buco non hanno mai cambiato un bit che il DLI legge.
- Sprite tra loro: i bit servono. Con `RR_ROUGH_PAIRS` il proiettile e il colpo del carro
  «colpiscono» gli oggetti attraverso i loro buchi: la modalità attract differisce in 3 fotogrammi,
  l'esecuzione del bot in NTSC in 712 (un oggetto distrutto accanto al quale il proiettile
  dell'originale era passato), la partita 7 in 2–4. Le estensioni di due sprite si incontrano di
  rado (qualche migliaio di righe per esecuzione), quindi il passo bit per bit costa poco.

## Passo 3: la visualizzazione senza i cicli del DLI

`Game::displaySimple` (`port/src/display.cpp`) è ora il predefinito; `RR_DISPLAY=beam` esegue il
modello ciclo per ciclo come prima, `RR_DISPLAY=check` esegue entrambi dallo stesso stato a ogni
fotogramma e stampa ogni differenza nei risultati delle collisioni, nei flag degli oggetti, nella
memoria player-missile o in ciò che è disegnato su ogni riga di scansione.

Segue il ciclo del DLI (B565–B664) in righe di scansione, non in cicli. Il registro X del 6502
conta le righe: ogni STA WSYNC è seguito da un INX, quindi finché X = c si disegna la riga c − 69.
Per ogni slot:

| cosa | riga di scansione (S: X quando finisce l'attesa dell'oggetto, E: X dopo la sua ultima riga) |
|---|---|
| HPOSP3 = x | da S − 68 |
| due righe vuote nella memoria del player 3 | S − 67, S − 66 |
| SIZEP3 | da S − 66 |
| riga di forma k (dalla più alta in giù), il suo COLPM3, il colore della strada del ponte | S − 65 + k |
| il colore della riva di nuovo (ponte) | da E − 68 |
| lettura M2PL (proiettile – oggetto) | vede fino a E − 70 (un ponte: E − 69) |
| lettura P3PL (oggetto – colpo), P2PL (aereo – colpo, oggetto) | fino a E − 69 |
| lettura P3PF (il bit «tocca il bordo» dell'oggetto) | fino a E − 68 |
| lettura M0PL (missile – aereo), M0PF (missile – riva) | fino a E − 68 (M0PF: vedi sotto) |
| lettura P1PF (carro), P2PF (aereo – riva), M2PF (proiettile – riva) | fino a E − 67 |
| HITCLR | le righe da E − 66 in poi contano per l'oggetto successivo (vedi sotto) |

Un oggetto le cui righe sono tutte sopra la riga in cui finisce la sua attesa (è iniziato troppo
vicino sotto il precedente) viene saltato senza letture e senza HITCLR. Quando X si riavvolge a 0 il
fiume finisce; la parte del pannello legge le collisioni dell'oggetto in corso fino alle righe
188–190.

Dove si trova la CPU dopo un WSYNC (ciclo 106) è fissato dal codice, quindi ogni accesso cade su un
lato noto del disegno di una riga (ciclo 28) tranne due, decisi dai cicli che prende l'inizio
della riga successiva (DMA, un'istruzione della display list con o senza indirizzo) e dai contatti
memorizzati appena prima:

- M0PF (B631) è letto 28 cicli dopo il WSYNC, 30 quando M0PL era un contatto: dopo il disegno su
  una riga il cui inizio prende 8 cicli (con indirizzo), quindi vede una riga in più.
  Nell'esecuzione del bot in NTSC: 1 499 volte su 68 365.
- HITCLR (B65F) arriva 23 cicli dopo il WSYNC, 2 in più per ciascuno dei tre contatti memorizzati
  prima: con tutti e tre, su una riga con indirizzo, arriva dopo il disegno e le collisioni di
  quella riga si perdono. Mai visto nelle esecuzioni (0 su 285 623).

Collisioni: il riquadro di ogni sprite (le righe su cui ha grafica e i cicli che coprono; il player
3 un riquadro per oggetto, tagliato dove cambiano posizione o dimensione). Il playfield: ogni riga
del player 1 (carro), 2 (aereo), 3 (oggetti) e dei missili 0 e 2 contro le sequenze della sua riga
(passo 2). Gli sprite tra loro: solo le coppie che il DLI legge (proiettile – oggetto, oggetto –
colpo, aereo – colpo, aereo – oggetto, missile – aereo) i cui riquadri si incontrano, bit per bit
sulle righe in comune. Ciò che vede una lettura è l'OR delle righe dall'ultimo HITCLR.

Verificato: `RR_DISPLAY=check` su tutte le esecuzioni di sopra (modalità attract, bot NTSC e PAL,
partita 7 in NTSC e PAL, due giocatori fino al fotogramma 2900): nessuna differenza dal raggio in
nessun fotogramma. `RR_DISPLAY=simple` contro l'originale (`game_check`): gli stessi risultati del
raggio (nessuna differenza, partita 7 in NTSC fino alla fine partita). L'area del fiume disegnata
da esso contro gli schermi dell'originale (`-screens`, 1289 fotogrammi della modalità attract, 1272
della partita 7 in PAL dal ponte 50): identica.

### Velocità (C++)

`RR_TIME=1` stampa il tempo che `display()` impiega per fotogramma. Build Release, il migliore di 5
esecuzioni, e le istruzioni contate da callgrind (partita 7 in PAL, 2000 fotogrammi):

| | modalità attract | partita 7, PAL (dal ponte 50) | istruzioni per fotogramma |
|---|---|---|---|
| il raggio (`RR_DISPLAY=beam`) | 8,5 µs | 10,1 µs | 101 500 |
| displaySimple, riquadri cercati nella memoria player-missile (`RR_BOXES=scan`) | 4,0 µs | 4,0 µs | 40 600 |
| displaySimple, riquadri dalle posizioni del gioco | 3,1 µs | 3,7 µs | 34 200 |

Circa tre volte meno istruzioni, 2,5–3 volte più veloce. Entrambi guardano le sequenze del
playfield fatte una volta all'entrata di ogni riga (`Game::runLines`, `makeRuns`), non i pixel
della riga. Del tempo del raggio due terzi andavano alle collisioni calcolate su ogni riga, un
terzo ai suoi cicli (`settle`). I riquadri degli sprite vengono da dove il gioco li ha messi (il
predefinito; `RR_BOXES=scan` cerca in giù nella memoria player-missile come prima): il colpo 16
righe da `shellY`, il carro 11 da `tankY`, l'aereo 14 dalla riga $AA, il missile dell'elicottero
la riga `missileY`, il proiettile 8 righe sotto `bulletY` (dalla riga $1C), le righe di ogni
oggetto come le ha scritte il ciclo del DLI; i bit di ogni riga in un riquadro si leggono ancora
dalla memoria player-missile. Uguale al raggio in ogni fotogramma di ogni esecuzione: niente fuori
da questi riquadri (righe rimaste nella memoria del player 3 da fotogrammi precedenti, per esempio)
ha mai prodotto una collisione che il DLI legge.

### Esecuzioni lunghe: `display_bench`

`port/build/display_bench <rom> <frames> [-pal] [-game N] [-seed N]` gioca con un pilota automatico
(punta al centro dell'acqua davanti e spara di continuo; l'aereo non può schiantarsi e non consuma
carburante, quindi una partita dura tutta l'esecuzione: 100 000 fotogrammi arrivano al ponte 98
nella partita 1, al ponte 147 nella partita 7). Con `RR_DISPLAY=check` confronta i due modelli a
ogni fotogramma, con `RR_TIME=1` misura `display()`.

Queste esecuzioni hanno trovato ciò che le più brevi non mostravano mai: il DLI a volte resta
indietro di una riga. Da un WSYNC al successivo il codice deve stare prima del ciclo 106 della riga
successiva, tolti l'inizio e il disegno di quella riga (su una riga del fiume 6 o 8 + 57 cicli);
tre tratti possono essere più lunghi:

- B5B0–B5D1 (fino alla prima riga): 49 cicli, 50 quando il byte di forma della prima riga è nella
  pagina successiva: troppo lungo su una riga con indirizzo dello schermo.
- l'ultima riga di un ponte fino a B5F3: 46 cicli, +1 per ogni lettura di tabella oltre una pagina,
  +2 per un colpo del proiettile.
- B5F3–B61F: 44 cicli (ponte 43), +2 per ciascuno dei contatti memorizzati lì (colpo, aereo con
  colpo, aereo con oggetto).

Quando è troppo lungo, lo STA WSYNC attende la fine della riga successiva: tutto ciò che viene dopo
in quel fotogramma accade una riga più tardi (X = riga + 68), compresi la fine del fiume e le
letture del pannello, e una riga del player 3 scritta sulla riga a cui è destinata o dopo appare lì
solo nel fotogramma successivo (GTIA ha già preso la vecchia). displaySimple conta questi salti
(`RR_STATS`): 0–10 su 100 000 fotogrammi, tutti con l'aereo che colpisce cose contro cui si sarebbe
schiantato; in una partita vera di rado. Con questi modellati, `RR_DISPLAY=check` non dà differenze
in 6 esecuzioni da 100 000 fotogrammi (partite 1, 2, 3, 5, 7, 8, NTSC e PAL).

Tempo di `display()`, il migliore di 3, e istruzioni per fotogramma (callgrind, 5000 fotogrammi):

| | partita 1, NTSC, 50 000 / 100 000 fotogrammi | partita 7, PAL, 50 000 / 100 000 fotogrammi | istruzioni (partita 1 / partita 7) |
|---|---|---|---|
| il raggio | 10,8 / 11,1 µs | 10,5 / 9,5 µs | 120 000 / 129 000 |
| displaySimple | 5,1 / 4,8 µs | 4,8 / 5,0 µs | 65 000 / 69 000 |

Circa due volte più veloce. Meno che nelle esecuzioni dell'originale sopra (tre volte) perché qui
l'aereo vola attraverso oggetti e rive senza schiantarsi: molte più collisioni per fotogramma di una
partita vera.

Ancora più lunghe: 12 esecuzioni da 300 000 fotogrammi (3,6 milioni), `-bridge 100` (la partita
inizia dal ponte 100; fino al ponte 392), NTSC e PAL, partite 1, 2 (due giocatori) e 7, e con
`-wild` (il pilota automatico sterza a caso, anche sopra le rive: fino ai ponti 70–159). Nessuna
differenza in nessun fotogramma. Righe saltate: 0–8 per esecuzione. L'HITCLR in ritardo (tutti e
tre i contatti memorizzati prima, su una riga con indirizzo dello schermo) non è mai successo, né in
queste esecuzioni né in altre: c'è solo per i cicli del codice, mai visto in azione.

### Solo gli oggetti che girano al bordo

Il bit «tocca il bordo» (P3PF) moveObjects lo legge solo per elicotteri ($08–$0B), mongolfiere
($0C) e navi ($0D): girano al primo contatto. I jet volano dritti, le esplosioni stanno ferme, ponti
e depositi restano dove sono. `displaySimple` verifica contro il bordo solo le righe di questi tipi
(`noEdge` segna le righe degli altri); il bit degli altri resta azzerato, e `game_check` non lo
confronta per loro (`bankRuns`). Verificato contro l'originale: la modalità attract (NTSC 2 262 e
5 961 fotogrammi, PAL 2 975), partite del bot in NTSC (fino alla prima fine partita, che il port non
modella in NTSC), in PAL (5 975 e due giocatori 7 775) e la partita 7 dal ponte 50 in PAL (5 975):
nessuna differenza.

### L'aereo contro un oggetto: per riquadri e bit

Il bit di P2PL per il player 3 è letto dopo le righe di un oggetto (B60A) o alla fine del fiume e
vede le righe dall'HITCLR prima di esse: solo le righe di quell'oggetto, nella sua posizione e
dimensione. Quindi «l'aereo ha toccato l'oggetto» si calcola senza le righe di scansione: le 14
righe dell'aereo da $AA contro le righe dell'oggetto dove le ha scritte il ciclo, bit per bit
(`planeTouches`), negli stessi due punti (i cicli della scrittura contano come prima); la coppia
P2–P3 è sparita dalle collisioni delle righe. Misurato prima accanto al valore del DLI
(`RR_GEO_CHECK`, poi rimosso): uguale in ogni fotogramma delle esecuzioni dell'originale sopra
(~47 000 fotogrammi, 420 con il contatto) e di `display_bench` (200 000 fotogrammi ciascuna con
`-wild` in NTSC e partita 7 in PAL, 300 000 dal ponte 100 in NTSC e in PAL con `-wild`: 61 382
contatti). Poi `game_check` contro l'originale: nessuna differenza; `RR_DISPLAY=check` (60 000
fotogrammi `-wild`, 60 000 dal ponte 100 in PAL): nient'altro che il bit del bordo dei tipi che non
girano (sopra).

### Tutte le collisioni senza i registri delle righe

Le altre letture vanno allo stesso modo. Le righe di ogni sprite sono dove il gioco le ha messe (i
riquadri sopra: niente di loro è altrove), quindi le righe in cui avviene ogni collisione che il DLI
legge si calcolano una volta per fotogramma: le righe del carro contro il bordo (P1PF), quelle
dell'aereo contro la riva (P2PF), quelle del missile e del proiettile contro entrambi (M0PF, M2PF),
l'aereo contro il colpo (P2PL) e il missile contro l'aereo (M0PL), bit per bit. Le collisioni di un
oggetto si calcolano alle sue letture, contro le sue righe dove le ha scritte il ciclo: il
proiettile (M2PL, fino alla riga prima dell'ultima, comprese quelle di un ponte), il colpo (P3PL),
l'aereo (P2PL), il bordo (P3PF, solo i tipi che girano). Una lettura conta una collisione se la sua
riga è tra quella dell'ultimo HITCLR e la propria, come facevano i registri. Il ciclo del DLI, i
suoi cicli e le righe di HITCLR restano (decidono quelle finestre); i registri delle righe
(`compute`, `seen`, i riquadri per riga) non ci sono più. Un caso differisce: quando il DLI resta
indietro di una riga, le righe scritte in ritardo non vengono prese e si vedono le vecchie; le
collisioni ora sono con le nuove righe. In nessuna delle esecuzioni qui è successo. Verificato:
`game_check` contro l'originale (tutte le esecuzioni sopra): nessuna differenza; `RR_DISPLAY=check`
(60 000 fotogrammi `-wild`, 60 000 dal ponte 100 in PAL, 300 000 dal ponte 100 in NTSC e 300 000 in
PAL con `-wild`): nient'altro che il bit del bordo dei tipi che non girano.

### Gli oggetti disegnati dalle loro righe

Le righe del player 3 non le legge nessuno tranne GTIA: il gioco mai. Quindi `displaySimple` non le
scrive più, né i registri del player 3 per riga (HPOSP3, SIZEP3, i colori, le due righe vuote sopra
ogni oggetto). Ciò che lascia per il disegno è `drawn`: di ogni oggetto la prima riga di scansione,
posizione, dimensione, righe e i loro colori, come li scrive il ciclo, e il colore della riva per
riga (`bankColour` di `scan`; i suoi campi del player 3 restano 0); `drawRiver` disegna gli oggetti
da lì. Il ciclo stesso resta come orario: quali righe mostra ogni oggetto, dove cade ogni lettura e
HITCLR, i cicli. `diffWithRam` non confronta le righe del player 3 ($0F00–$0FBF) con
`simpleDisplay`; `RR_DISPLAY=check` confronta le due immagini invece delle righe. Quando il DLI
resta indietro, l'originale mostra le righe dell'ultimo fotogramma dove vanno quelle in ritardo;
qui si disegnano le nuove (in nessuna esecuzione). Verificato: `game_check` contro lo stato
dell'originale (tutte le esecuzioni sopra): nessuna differenza; contro i suoi schermi (`-screens`,
1300 fotogrammi della modalità attract, 1500 della partita 7 in PAL dal ponte 50): l'area del fiume
identica; `RR_DISPLAY=check` (60 000 fotogrammi `-wild`, 60 000 dal ponte 100 in PAL): le immagini
uguali, nient'altro che il bit del bordo dei tipi che non girano.

### RR_GEOMCHECK: semplice geometria accanto alle letture

Prima di andare oltre, una misura: con `RR_GEOMCHECK` impostato, `displaySimple` calcola le
collisioni di ogni fotogramma anche senza le letture del DLI, senza finestra da un HITCLR a una
lettura e senza cicli. Un oggetto è colpito se le sue righe toccano in qualsiasi punto quelle dello
sprite (l'ultimo tale oggetto nell'ordine del ciclo, perché le scritture si sovrascrivono); le
altre collisioni contano se avvengono in qualsiasi punto delle righe degli sprite; il bit del bordo
di un oggetto viene da tutte le sue righe. I risultati delle letture restano; i fotogrammi in cui
la geometria decide altrimenti sono contati per campo e stampati all'uscita. `RR_GEOMCHECK=2`
stampa anche ogni tale fotogramma: il suo numero, cosa differisce, dove sono proiettile, aereo e
carro e le righe di ogni oggetto.

Il risultato: differisce lo 0,01–0,25% dei fotogrammi, a seconda dell'esecuzione, e sempre allo
stesso modo: un contatto che le letture vedono un fotogramma dopo o non vedono affatto. Il
proiettile sull'ultima riga di un oggetto (M2PL è letto prima che quella riga sia disegnata), le
righe dell'aereo o del carro sotto l'ultima lettura del fotogramma. Niente di ciò che vede la
geometria è un contatto che non sia avvenuto sullo schermo.

## Branch `dli_removed`: `displayGeometry`

In questo branch `Game::displayGeometry` (`port/src/display.cpp`) è il predefinito
(`RR_DISPLAY=geometry`); `RR_DISPLAY=simple`, `beam` e `check` restano come sopra. Non c'è DLI: niente
ciclo sugli slot per righe di scansione, niente cicli, niente HITCLR, niente letture. È ciò che fanno
i port per ZX, in C++, e il riferimento contro cui sono verificati fotogramma per fotogramma.

### Le collisioni come semplice geometria

Ogni collisione conta dove le righe degli sprite e degli oggetti si toccano in qualsiasi punto,
prima i riquadri, poi i punti (la regola di RR_GEOMCHECK sopra). Un oggetto colpito è l'ultimo tale
scendendo lungo lo schermo, perché le scritture dell'originale si sovrascrivono; il colpo non è
verificato contro un ponte. Le collisioni con riva e bordo, aereo–colpo e missile–aereo tengono lo
slot 0 quando avvengono in qualsiasi punto delle righe degli sprite. Le righe degli sprite sono i
riquadri dalle posizioni del gioco (passo 3: il colpo 16 righe da `shellY`, il carro 11 da `tankY`,
l'aereo 14 da $AA, il missile la riga `missileY`, il proiettile 8 righe sotto `bulletY`).

Quindi il gioco non è più quello dell'originale in ogni fotogramma: nei fotogrammi che conta
RR_GEOMCHECK (0,01–0,25%) un proiettile colpisce un oggetto o un aereo la riva un fotogramma prima,
oppure conta un contatto che l'originale ha mancato. `game_check` contro l'originale è per
`RR_DISPLAY=simple`; con il predefinito segnala quei fotogrammi.

### Ogni oggetto alla y del suo slot

Gli oggetti sono messi direttamente dai loro slot, senza il giro del contatore del DLI: righe dalla
riga di scansione y − 65 in giù, dove le disegnava il DLI (prima la riga in fondo della tabella
delle forme, perché le tabelle le tengono dal basso in alto), tagliate in cima al fiume (riga 28: la
prima attesa del DLI era X = $5D) e alla sua fine (X oltre 255). I colori della strada del ponte
vengono da quelle righe, dopo l'ultima il colore della riva della sezione. Verificato su 120 000
fotogrammi (`display_bench` dall'inizio, dal ponte 60 in PAL, `-wild`, partita 7) contro il giro
del contatore che ha sostituito: gli stessi oggetti, righe e colori in ogni fotogramma. Il ciclo
non è mai rimasto indietro rispetto a un oggetto, quindi li tagliano solo la cima e la fine del
fiume.

### Il bordo dal primo e dall'ultimo punto di una riga

Il bit del bordo di un oggetto (P3PF: elicotteri, mongolfiere, navi girano al bordo) viene da
ciascuna sua riga come estensione, dal primo punto all'ultimo, contro le sequenze di pixel del
bordo della riga (`Beam::edgeTouches`), senza guardare punto per punto: una sequenza dentro un buco
di una riga conta, diversamente dalla verifica bit per bit dell'originale in GTIA. Il Next lo
verifica così: per riga le sequenze del bordo, per riga dell'oggetto le sue due estremità. Su
20 000 fotogrammi delle partite del bot (più di 100 contatti) il risultato non è mai stato diverso
dalla verifica punto per punto.

### Gli altri sprite contro riva e bordo allo stesso modo

`Beam::spanTouches`: il primo e l'ultimo punto di una riga contro le sequenze dei colori voluti
della riga (riva, bordo o entrambi); `edgeTouches` è essa con il bordo. Il carro contro il bordo,
l'aereo contro la riva, il missile e il proiettile contro entrambi (`pfAny`) la usano anch'essi,
quindi ogni collisione con il playfield è un solo test: le due estremità della riga contro le
sequenze della riga. Gli sprite tra loro (proiettile, colpo e aereo contro un oggetto, aereo e
colpo, missile e aereo) restano bit per bit: il passo 2 ha mostrato che lì i buchi contano.

### Quanto costa e a cosa serve

La visualizzazione ora è tre cicli: i riquadri degli sprite contro le sequenze delle loro righe, le
coppie di sprite dove i loro riquadri si incontrano, gli oggetti messi dai loro slot e verificati
contro proiettile, colpo e aereo. Nessuna riga di scansione vuota viene percorsa. Sul Next lo
stesso lavoro (`display_simple.asm`, `display_hits.asm` in river_raid_zx) richiede 45 000–62 000
dei suoi 567 000 cicli per fotogramma a 28 MHz; i suoi CODE.md e NOTES.md hanno i numeri routine
per routine.
