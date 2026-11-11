# River Raid (Atari 8-bit) — scoperte

[English](NOTES.md) | [Русский](NOTES.ru.md) | [Українська](NOTES.uk.md) | [Deutsch](NOTES.de.md) | [Polski](NOTES.pl.md) | Italiano | [Español](NOTES.es.md) | [中文](NOTES.zh.md) | [日本語](NOTES.ja.md)

Immagine: 8192 byte, senza intestazione CART, caricata in $A000–$BFFF. Codice ~5,9 KB (2941 istruzioni), dati ~2,3 KB.

## Punti di ingresso
| Indirizzo | Cosa | Come trovato |
|---|---|---|
| $A000 | avvio + ciclo principale del fotogramma | trailer della cartuccia ($BFFA) |
| $B2A5 | VBI (VVBLKI $0222): suono POKEY, contatori dei fotogrammi, registri per fotogramma | scrittura in $0222/$0223 a A0D1 |
| $B500 | DLI (VDSLST $0200): multiplexing degli sprite riga per riga | scrittura in $0200/$0201 a A0C7 |
| $B485 | IRQ vuoto | scrittura in $0216/$0217 |

## Fotogramma
- `L_AE99`: attende VCOUNT < $50 (è iniziato un nuovo fotogramma), prepara la display list e gli sprite.
- `L_A197`: attende VCOUNT >= $50, legge il joystick (PORTA, TRIG0/1), esegue la logica di gioco.
- `A18D`: attende VCOUNT = $78, poi scrive NMIEN ($C0 = DLI+VBI, $40 = solo VBI).
- Un giro del ciclo = un passo di logica, quindi il modello «update() una volta per fotogramma» è corretto.

## Cosa conta per il port
1. **Numeri casuali.** `RANDOM` ($D20A, il polinomio di POKEY) è letto in 3 punti. La logica di gioco solo a A205: ogni 16 fotogrammi sceglie uno slot nemico (0–10) e imposta il bit $40 nei suoi flag. Le altre due letture (B362, B3D0) danno solo rumore per il suono. Il valore dipende dal ciclo di CPU della lettura (il polinomio a 17 bit di POKEY gira al clock della macchina). Il port modella il contatore (`port/src/pokey_random.h`) e lo avvia dall'orologio, come in una vera accensione; per il confronto fotogramma per fotogramma i valori dell'originale vengono dalla traccia di riferimento (`random.log`).
2. **PAL/NTSC.** Il bit PAL è letto da $D014 (A06A), tenuto in $BF, e sceglie la variante della display list ($B77A, copiata nella display list in $3FA9), il limite dei depositi `$CB` ($2E/$2F) e le diramazioni a B135/B14F/B741. Niente compensa la frequenza dei fotogrammi: in PAL il gioco gira a 50 fotogrammi al secondo, circa il 17% più lento. Non è mai esistita una versione PAL separata: la cassetta britannica è lo stesso codice (vedi «Versione britannica su cassetta»). `-pal` nel port imposta il limite dei depositi; la display list PAL e i 50 Hz non sono ancora riprodotti.
3. **Font del sistema.** Il codice copia il set di caratteri dalla ROM del sistema ($E000/$E100/$E300) in $0600–$09FF. È una dipendenza esterna: serve un font proprio o la ROM dell'utente (Altirra OS è libero).
4. **Legame con il pennello elettronico.** Gli sprite sono disegnati da un nucleo DLI con WSYNC su ogni riga; in C++ diventa un renderer software che lavora sullo stato del fotogramma.
5. **Suono.** Il driver vive tutto nel VBI ($B2A5–$B484): 4 canali POKEY, AUDF/AUDC calcolati da $C6, $7A–$7F, $50, $24 e dalle tabelle $B4AC/$B4C0/$B4E1. Riprodurre questo driver più un nucleo POKEY basta per una corrispondenza esatta.
6. **Stile del codice.** Monolitico, le variabili sono riusate da sottosistemi diversi, i flag sono impacchettati in bit. Una mappatura completa richiede tracce da un emulatore.

## Tabelle dati (dai riferimenti nel codice)
Tutte sono lette tramite funzioni con nome in `port/src/rom.h`.
- $B4AC (4): timbro del motore (AUDC) secondo la leva su/giù; $B4C0 (17): altezza del fischio del proiettile del carro armato; $B4E1: suono della vita extra.
- $B4D1: forme del carro armato (coppie di puntatori, lette tramite $A1/$A2 a AEB9).
- $B776 (4 byte) è copiato in $3F00 (inizio della display list); $B77A (15 byte, in PAL i 15 seguenti) in $3FA9, il resto della display list.
- $BB70 (60 byte): testo del pannello, copiato in $1000; $BEEE (64): scritta Activision → $06C0; $BF2E (112): simboli del pannello → $0818; $BF9E (24): «P1»/«P2» → $08D0; $BFB6 (48): le cifre di «1982, 1983» → $07D0.
- $BAAF: tipo successivo di carro armato; $BAC5: colori del proiettile; $BB24: tipo successivo di oggetto (animazione); $BB40: offset delle forme degli oggetti da $B900.

## Esecuzione di riferimento (fatto)
`oracle/rr_oracle` esegue l'originale su libatari800 e scrive la RAM di ogni fotogramma, ogni scrittura in POKEY e ogni lettura di RANDOM (vedi [oracle/README.it.md](../oracle/README.it.md)).
- Il gioco parte con il tasto START; il fuoco nella schermata del titolo non lo avvia.
- atari800 avvia il generatore di POKEY dall'orologio; con `-seed` le esecuzioni sono identiche byte per byte.
- Letture di RANDOM: la riga di scansione ~249 è il suono dal VBI, le righe 190–192 e 241 sono la logica di gioco (A205). Per il C++ significa che in modalità di verifica i valori casuali del gioco si prendono da `random.log` e non si emulano.

## Generatore del livello (confermato dalla traccia)
- Il fiume e il posizionamento degli oggetti sono guidati da un LFSR software a 24 bit in `sub_B183` su $AA/$AC/$AB. Chiamato da A756 e A767.
- Un passo: `fb = bit7(AB) ^ bit2(AB)`, poi uno scorrimento a sinistra della catena AA←fb, AC←bit7(AA), AB←bit7(AC). Il modello in Python spiega 265 su 268 cambiamenti nella traccia; gli altri 3 sono ricaricamenti.
- Il valore iniziale di AA/AB/AC è $E3/$D4/$F7. È tenuto in $66–$68 e ripristinato a A3C8 dopo l'avvio e dopo una morte (ripartenza dal ponte).
- Con semi POKEY diversi, su 1000 fotogrammi $AA–$AC, $66–$68, i tipi degli oggetti ($0500…) e la loro X ($0516…) sono identici. Differiscono solo i flag degli slot $42–$4C (bit $40 da A205), il byte di lavoro $07 e lo stack.
- Conclusione: il livello è sempre lo stesso. `RANDOM` influisce solo sul comportamento dei nemici (quale riceve il flag $40). All'inizio della partita il movimento dei nemici sembra disattivato (il controllo di $60 a A297), quindi in un'esecuzione breve non si vedeva.

## Variabili (fase 2)
Stato: **verificato** significa che la regola è stata controllata con `tools/verify_trace.py` sulle tracce di riferimento senza eccezioni. **dal codice** significa che il valore è stato letto dal listato senza un controllo a parte sulla traccia. **ipotesi** è una supposizione ancora da confermare.

### Giocatore
| Indirizzo | Cosa | Stato |
|---|---|---|
| $57 | X dell'aereo (unità HPOS; sullo schermo di riferimento centro ≈ 2·X−55 px) | verificato: si muove solo con la leva inclinata |
| $58 | parte frazionaria di X; al superamento $57 si sposta di ±1 (A462–A493) | dal codice |
| $59 | velocità: 128 normale, fino a ~142 con su, fino a ~114 con giù, torna a 128 al rilascio | dalla traccia, senza controllo a parte |
| $5B:$5C | carburante, 16 bit, $FFFF è il serbatoio pieno. Codice A3E9–A453, in ogni fotogramma di volo: sopra un deposito `$5B += 2` (è `ADC #1` con il riporto rimasto da `CMP #$0E`), limitato a $FFFF; poi sempre `$5C −= $20` con prestito da `$5B` (−1 in $5B ogni 8 fotogrammi). Se al prestito $5B vale 0, l'aereo muore. Il consumo per fotogramma non dipende dalla velocità. L'ago dell'indicatore del carburante HPOSM1 = $5B/8 + $5C nel DLI a B527 (lì $5C è la costante `#$5C`, non la variabile) | verificato, compreso il rifornimento: +480 per fotogramma, limite $FFFF |
| $1D | slot con cui l'aereo è entrato in collisione; bit 7 = nessuno | verificato tramite il rifornimento |
| $7A / $7B | evento sonoro: 4 rifornimento, 3 serbatoio pieno (A40E–A422); $7B = 8 a ogni cambio, poi il VBI conta alla rovescia | dal codice e dalla traccia |
| $6A | vite di riserva: 4 all'inizio della partita, 3 dopo il decollo, 0 sull'ultima vita | dalla traccia |
| $76 | fase della vita: 128 all'inizio della partita; 49/47 attesa al ponte prima del decollo (qualsiasi movimento della leva fa partire); 0 volo; 253 e poi −3 per fotogramma esplosione; 130→127 ricomparsa (carburante a $FFFF) | dalla traccia |

### Punti e ponti
| Indirizzo | Cosa | Stato |
|---|---|---|
| $63/$64/$65 | punti, BCD, byte più significativo per primo, memorizzati divisi per 10 (950 sullo schermo = `00 00 95`); al superamento 999999 e $6A = 0 (A563) | verificato |
| $BB0B + tipo | punti/10 per oggetto (BCD): 8 elicottero 60, $0D nave 30, $0E ponte 500, $0F carburante 80, $10 ponte con carro armato 750; la tabella ha anche 7 → 100, $0A/$0B → 150 (non visti nelle tracce) | verificato per 8, $0D, $0E, $0F, $10 |
| $61/$62 | numero del ponte corrente, BCD (come «BRIDGE n» sullo schermo), +1 quando un ponte è distrutto (A4F0) | dal codice e dalla traccia |
| $38 bit 7 | il carro armato sulla strada è stato distrutto insieme al ponte (A513–A517) | dal codice, 1 caso nella traccia |
| $39 | X del carro armato sulla strada (HPOSP1 nel DLI a B54D) | dal codice |

I punti si ottengono anche per il nemico contro cui si è schiantato l'aereo: il suo slot passa allo stato 3. Abbattuto da un proiettile passa allo stato 2.

### Slot degli oggetti (11, indice X = 0…10)
Uno slot è una fascia orizzontale dello schermo, non un numero fisso di oggetto: durante lo scorrimento un oggetto passa allo slot successivo (nella traccia un deposito è passato per gli slot 8 → 9 → 10). Il controllo delle collisioni in un fotogramma avviene prima di questo passaggio.

| Indirizzo | Cosa | Stato |
|---|---|---|
| $0C+X | posizione verticale dell'oggetto (+1 per fotogramma a velocità normale; ≈ 240–247 vicino all'aereo) | dalla traccia |
| $0500+X | tipo attuale dell'oggetto. ≥ 7 è un oggetto vivo: 7…$0D nemici (8/9 fotogrammi dell'elicottero, $0D nave), $0E ponte, $0F deposito. < 7 è vuoto o in esplosione: 2 colpito da un proiettile, 3 collisione; compaiono anche 0, 1, 4–6. In collisione (A3E9): 7…$0D morte, $0E morte + `INC $2E`, $0F rifornimento | verificato (2/3 tramite i punti, $0F tramite il rifornimento) |
| $050B+X | tipo dell'oggetto (indice nella tabella dei punti): 0, 1, 8, $0D, $0E, $0F nelle tracce | verificato tramite i punti |
| $0516+X | X dell'oggetto | dal codice (A500–A511, confrontato con $39) |
| $42+X | flag: bit 7, bit 6 ($40, impostato da RANDOM a A205), bit bassi | non analizzati |
| $4E | indice dello slot in elaborazione (confrontato con #$0B) | dal codice |

### Esecuzioni di riferimento per la mappatura
- `oracle/scripts/bot_run_8000.txt`: input registrato dal bot (`rr_oracle -bot`), 8000 fotogrammi. Contiene 2 ponti (uno con carro armato), 5 morti, la fine della partita intorno al fotogramma 3300, poi la schermata del titolo e una nuova partenza intorno al 5323. Nessun vero rifornimento: il bot abbatte i depositi.
- Riprodurre questo file senza il bot dà `ram.bin`, `pokey.log` e `random.log` identici byte per byte.

- `oracle/scripts/refuel_first_depot.txt`: volo sopra il primo deposito senza sparare (32 fotogrammi sopra il deposito, il serbatoio arriva a $FFFF).

## Generazione del terreno (codice A6FD–A9D7, disegno AA8C–AB4B)
La riva non è memorizzata come immagine, ma come alcuni numeri per riga. Ogni nuova riga dello schermo è un passo del generatore, dopo il quale la riga viene disegnata nel buffer circolare dello schermo (l'indirizzo della riga viene dalla display list, $3F04/$3F05).

**Scorrimento (dal codice).** In ogni fotogramma la velocità `$59` viene sommata due volte all'accumulatore `$30`, e ogni superamento produce una nuova riga. A velocità 128 è esattamente 1 riga per fotogramma; con `$59 ≥ $FE` una riga viene prodotta senza accumulare.

**Struttura (dal codice).**
- `$2F` — numero della riga nel blocco, 0…31.
- `$5A` — blocchi rimasti nella sezione (2).
- `$5D` — sezioni rimaste fino al ponte, ricaricato a 16; alla fine viene chiamato `sub_B194`.
- L'LFSR `sub_B183` fa un passo su ogni riga, tranne le righe con il bit 7 in `$23` (gli avvicinamenti diritti al ponte).
- Al confine di una sezione un nuovo obiettivo di larghezza `$5F` viene preso dal nibble basso di `$AA` con dei limiti (minimo 2, massimo 15/14/8 secondo `$2D`, `$60`, `$B5`), poi `$34 = $5F·4`.

**Numeri per riga.**
| Indirizzo | Cosa | Stato |
|---|---|---|
| $52 | posizione della riva | verificato (cammino casuale, sotto) |
| $C3 | direzione del cammino: +1, $FF (−1) o 0 | verificato |
| $54 | mezza larghezza dell'isola / della parte interna | verificato (smussatura) |
| $53 | larghezza dell'acqua; cambia di −2 × il passo di `$54`, così la larghezza totale si conserva | verificato |
| $34, $35 | obiettivo per `$54` e il passo verso di esso (±1 se la differenza è < 32, altrimenti ±2) | verificato |
| $2D | bit 7 — modalità isola (cambia il disegno), bit 6 | dal codice |
| $C4, $C5 | limiti di `$52`: $1C…$24 con `$60 < 3`, altrimenti più larghi (1…$40 con varianti secondo `$60`, `$052D`, `$36`) | dal codice, nelle tracce visti 28…36 |
| $23 bit 7 | vicino al ponte la riva segue la tabella `$BA8F[$2F] + $52`, senza casualità | verificato tramite il C++ (`terrain_check`) |
| $052D | ≠ 0: la riva è tirata nel corridoio $18…$28 | verificato tramite il C++ (`terrain_check`) |

**Cammino casuale della riva (A84F–A96A), verificato.** Ogni riga prende il nibble basso `n = $AC & 15` (il byte dell'LFSR dopo il passo di questa riga):
- se `$C3 = 0`, il movimento comincia: +1 con `n ≥ 8`, altrimenti −1;
- se la riva si muove già: con `n < 8` sta ferma; con `n = $D` inverte e fa un passo; con `n = $F` sta ferma se `$0E` (`$0D` su un'isola) è minore di `$5F` o di `$2C`; con qualsiasi altro valore fa un passo;
- un passo che supererebbe i limiti `$C4/$C5` o finirebbe sull'oggetto nello slot corrente `$4E` (tipo ≥ 8; il limite usa `$54/$34`, `$0516+X`, `$B5`, `$0521+X`) è sostituito da un'inversione sul posto.

Sottigliezza di temporizzazione: gli slot e `$4E` si prendono come sono all'inizio del fotogramma, mentre `$54/$34/$2D/$B5/$37` già ricalcolati in questo fotogramma. Con questa regola il modello coincide con le tracce su tutte le 4669 righe.

**Smussatura dell'isola (A7FB–A824), verificata.** Finché `$35 ≠ 0`, ogni riga fa `$54 += $35` e `$53 −= 2·$35`. Quando `$54` raggiunge `$34`, il passo `$35` viene azzerato. Verificata su 4825 righe, di cui 1195 sono davvero cambiate.

**Decorazioni (case, montagne).** Iniziano all'inizio di un blocco (`$2F = 0`) quando il bit 5 di `$AC` è impostato. Offset, larghezza e numero di righe vengono dalle tabelle `$BBCC`, `$BBD4`, `$BBB2`, `$BBEB` secondo `$AA` e `$5F`, il puntatore alla bitmap da `$BBF0`. Verificate tramite il C++.

**Oggetti.** Ogni 16 righe un nuovo oggetto viene messo nello slot libero più alto (`$4D`−1); il tipo è scelto secondo `$AB` tramite la tabella `$BB1C`, la larghezza dello sprite da `$BBA2` (in `$0521+X`, cioè `SIZEP3`: 1 per la nave, 3 per il ponte, altrimenti 0), il numero di righe da `$BB60` (+4). Finché l'oggetto è ancora in fase di disegno (`$32` righe), la riva evita quell'oggetto (`$4E`). Verificato tramite il C++.

**Ponti.** Il generatore disegna solo un canale diritto dove si trova il ponte (riva dalla tabella `$BA8F`) e mette un oggetto di tipo `$0E` in uno slot con flag `$80/$84` in `$42+X` (una copia di `$23`). Il ponte vero e proprio compare solo alla visualizzazione, nel DLI (B575–B5FC): per uno slot con il bit 7 nei flag il colore della riva `COLPF0` su ogni riga viene dalla tabella `$B4E5` (`04 04 04 04 08×6 1C 1C 06×5 04 04 04`: le fasce grigie della strada e la segnaletica gialla), così la strada attraversa tutto lo schermo sopra i pixel della riva; la campata sopra l'acqua è lo sprite P3 di quello slot a larghezza quadrupla, come per navi ed elicotteri. Dopo l'oggetto il colore della riva viene ripristinato da `$0B` (`$D2`/`$D6`: la riva alterna due verdi secondo la parità della sezione). Il carro armato sulla strada è lo sprite P1 (`$39`). Le righe della bitmap di un ponte non cambiano dopo la generazione (verificato su 119 righe).

**Colori e sprite per riga (verificati tramite il C++, `port/src/river_draw.cpp`).** I colori di base arrivano dal ritorno verticale (B408–B43E): acqua `COLPF2` = $84 ($42 durante il lampo), bordo `COLPF1` = $20, riva `COLPF0` da `sub_AFD0`: `($23 & 4) ^ $D6`, con il bit 2 invertito finché l'oggetto più recente (`$4E`) è un ponte con y < $5C. Il DLI percorre gli slot da `$4D` verso l'alto con un contatore di righe di scansione (valore $5D sulla riga 20, dove entrano le nuove righe): il primo slot con `0 ≤ y + $BB60[tipo] − contatore ≤ $BB60[tipo]` dà la riga r; la forma è `$B900 + $BB40[tipo attuale (+16 se il bit 3 dei flag)] + r`, il colore `$B700 + $BB30[tipo] + r`, HPOSP3 = `$0516+X` (32 = bordo sinistro del buffer), SIZEP3 = `$0521+X`; per uno slot di ponte COLPF0 = `$B4E5 + r`. Rispetto alla riga 20 dell'originale: 96,8% delle righe identiche con le posizioni del generatore, 98,1% con le posizioni degli oggetti del gioco, tutte le restanti mentre il carro armato sulla strada è attivo.

**Modalità dimostrativa e la parte bassa dello schermo (verificate tramite il C++, `port/src/status_bar.cpp`).** Senza START il gioco fa scorrere il fiume della partita 1 dall'accensione: la scritta scorrevole parte dal fotogramma 11, il fiume dal fotogramma 38, 1 riga per fotogramma, attraverso i ponti, con lo stesso generatore (la striscia della riga 20 coincide con il C++ su 19 960 fotogrammi, tranne gli oggetti in movimento). Display list in $3F00: 24 righe vuote, 160 righe in modo E, poi il pannello: 3 righe in modo 6 (larghezza normale, `CHBASE` $08, colori $1E/$8E/$1E/$00 su $06, `PRIOR` 4) da $1000, riempite da `$BB70`; l'ago del carburante è il missile 1 a `$5B/8 + $5C`. Ultima riga: modo 6 largo con `HSCROL`, `CHBASE` $06, colore $0C, testo a `$B800 + $3FB3`; il VBI (B44B) fa avanzare `HSCROL` 7→0 e poi il testo di un carattere ($4B…$D6, a ciclo) finché `$2A` ≠ 0, e sposta insieme il logo `$27`. Il logo è il player 3 a larghezza quadrupla (dati $FC) più il player 2 ($FF) spostato di un ciclo a sinistra su ogni riga, nei colori `$B48D`. I set di caratteri in $0600/$0800 sono il font del sistema (A0FB) con i caratteri della cartuccia in $06C0, $0818, $08D0, $07D0 (le cifre di «1982, 1983»).

**Movimento degli oggetti, carro armato, missili (verificati tramite il C++, `port/src/game.cpp`, `display.cpp`).** Ogni 16 fotogrammi uno slot scelto da `RANDOM & 15` (11–15 → 6–10) riceve il bit 6 in `$42+X` («sveglio»), a meno che non sia l'oggetto ancora in fase di disegno. Gli oggetti di tipo 7 (caccia) si muovono in ogni fotogramma; i tipi 8–13 solo se svegli, dal ponte 2 (`$60 >> 1 ≠ 0`), a fotogrammi alterni secondo la parità dello slot (`X ^ $18 ^ $33`), il tipo $0C solo nei fotogrammi con `$18 & 2 = 0`; 1 ciclo per passo, bit 3 = sinistra, avvolgimento $2C…$D4. Il bit 0 di `$42+X` lo imposta il DLI quando lo sprite dell'oggetto ha toccato sullo schermo il colore del bordo della riva (P3PF bit 1); il passo seguente allora inverte la direzione una volta (il bit 4 segna l'inversione finché il contatto non termina). I tipi sotto $0C si animano tramite `$BB24` ogni 8 fotogrammi (ogni 2 per i tipi 7+). Il carro armato (`$38`–`$3A`) avanza ogni 16 fotogrammi (4 per i tipi 4–7) finché il player 1 tocca il colore del bordo, poi (`$41` bit 7) spara: il proiettile è il player 0, `$50` conta 0…$38 (tabella di volo `$B4AF`, velocità `$3E/$3F`), poi i fotogrammi dell'esplosione. Un elicottero di tipo $0A/$0B nello slot `$51` lancia il missile 0 (`$C1` x, `$C2` y, passo `$C0` = ±2) finché colpisce la riva (M0PF). I registri di collisione del DLI vengono letti dopo ogni oggetto e azzerati, quindi le collisioni sono note per slot (`$1A`–`$22` contengono il numero dello slot). Con RANDOM riprodotto, il C++ coincide con la RAM dell'originale su 19 900 fotogrammi della modalità dimostrativa dall'accensione.

**Driver del suono (verificato tramite il C++, `Game::soundDriver`).** Una volta per fotogramma nel VBI (B2A5–B3D8), AUDCTL resta 0 (clock 64 kHz, rumore a 17 bit). `$C6` ≠ 0: bip di partenza sul canale 1 ($40/$AF), il resto spento. Canale 1: evento sonoro `$7A` 1–4 per `$7B` fotogrammi (altezza $39/$11/$1F/$3F, volume da `$7B`); altrimenti, quando l'aereo vola, l'allarme carburante con `$5B` < $40 (un bip ogni 63 fotogrammi, un tono discendente sotto $04) oppure il motore: AUDF = $1F − `$59`/16 (o il discendente `$052C` dopo uno schianto), AUDC dai bit su/giù della leva (`$B4AC`: $84 normale, $83/$86). Canale 2: rumore dell'esplosione `$7C` (AUDF = RANDOM | $18) oppure, durante il gioco, il missile dell'elicottero `$7F`. Canale 3: lo sparo `$7D` o il suono dei punti `$7E` (`$B4E1`). Canale 4: il proiettile del carro armato `$50` (`$B4C0` in volo, poi rumore). La modalità dimostrativa è muta: `$76` ≠ 0 spegne il canale 1, e nient'altro viene attivato.

**La partita con un giocatore (verificata tramite il C++, `port/src/game.cpp`).** `$76` è lo stato dell'aereo: 0 volo; $FE…$80 esplosione (126 fotogrammi, `$5E` fotogramma dell'aereo giù ogni 8); poi $7F: se restano vite, il fiume viene azzerato (`sub_B0F1` con X = $12, che pulisce lo schermo e la memoria player-missile per 13 fotogrammi mentre girano solo gli interrupt) e riempito di nuovo con `$59` = $FE (2 righe per fotogramma) mentre `$76` conta da $7F a $30; a $30 si toglie una vita e `$052C` = $80 (il suono crescente del motore); a $2F l'aereo aspetta al ponte senza scorrimento finché un movimento della leva o il fuoco imposta `$76` = 0 e `$59` = $40. Senza vite: fine della partita (record `sub_B261` con byte di controllo `$90`–`$9F`, `sub_B21A`: `$76` = `$2A` = $FF, «Game over» nella scritta scorrevole). In volo (A3E9–A5B4): collisione dell'aereo con un oggetto (`$1D`): tipi 7–$0E schianto (un ponte conta anche `$2E`), tipo $0F rifornisce (`$5B` + 2 per fotogramma, suoni `$7A` 4/3); esplosione del proiettile (`$1E` con `$50` ≥ $11), missile dell'elicottero (`$21`) o la riva (`$1B`) schianto; il carburante brucia $20/256 per fotogramma; il movimento laterale accelera (`$2B` += 8, frazione `$58`), fotogrammi 7/8/9; velocità `$59` ±2 tra $40 e $FE, di nuovo $80 con la leva al centro; il proiettile (missile 2, 8 righe da `$56`, 6 righe in su per fotogramma) distrugge gli oggetti di tipo ≥ 7 (`$1C`), la tabella dei punti `$BB0B` secondo il tipo, una vita ogni 10 000 punti (fino a 9). Un ponte distrutto conta `$61/$62`, porta via un carro armato che ci sta sopra e dal ponte 5 fa sparare un carro armato vicino. `sub_B1C0` decide dove parte l'aereo successivo: oltre un ponte distrutto dalla sezione successiva, altrimenti un punto di controllo indietro. Le partite a due (`$09` dispari) scambiano `$60`–`$6A` con `$6B`–`$75` (`sub_B247`) dopo ogni aereo; il giocatore 2 usa il secondo joystick (nibble alto di PORTA). START (A673) inizia una nuova partita (B0F1 con X = $29, 13 fotogrammi); SELECT/OPTION ogni 30 fotogrammi la partita successiva; qualsiasi tasto della tastiera mette in pausa (`$24`: 3 → 2 pausa → 1 → 0).

**Disegno (verificato tramite il C++).** Una riga è 48 byte di bitmap (modo ANTIC E, 4 pixel per byte), composti da riempimenti (sottoprogrammi `AFEC`/`AFF0`/`AFF4`: `$AA`, `$FF`, `$55`) e pezzi di bordo di 2 byte dalla tabella in `$B800` (`B034`/`B03B`): riva, acqua, isola (con il bit 7 di `$2D`), acqua, riva. La posizione del pixel divisa per 4 è il numero del byte, i 2 bit bassi scelgono il pezzo di bordo. Una riga di decorazione la copia `B01A`. Le rive non sono speculari: il bordo destro si calcola dal sinistro tramite le larghezze dell'acqua e dell'isola, perciò il fiume serpeggia. Su 2200 righe dal C++: quando si muovono entrambi i bordi, si muovono allo stesso modo in 810 casi su 944; il centro del fiume vaga tra 71 e 122 pixel.

## Generatore in C++ (`port/`)
- Tutta l'elaborazione per riga (A711–AE96 e i sottoprogrammi) è portata in `port/src/river_gen.cpp`. Il controllo `port/tools/terrain_check` confronta ogni campo del generatore e i 48 byte di ogni nuova riga dello schermo con l'originale.
- Risultato: 119 758 righe dell'intero fiume senza differenze; anche una partenza da zero (`startGame`) coincide.
- Il generatore non dipende né dal giocatore (bot contro volo senza input) né da `RANDOM` (3 semi, 120 000 fotogrammi, identici byte per byte, striscia dello schermo compresa).
- Schermo: bitmap in modo ANTIC E, 48 byte per riga (192 pixel, 4 per byte), buffer circolare, indirizzo della nuova riga in `$3F04/$3F05`, −$30 per riga. Valori dei pixel: 1 riva, 3 acqua, 2 bordo, 0 raro (dentro i pezzi di bordo).
- Partite 1–8: `$09` sceglie una riga delle tabelle `$B494`–`$B4A8`: LFSR, riva e sezione iniziale. Le partite 1–2 partono dal ponte 1, 3–4 dal 5, 5–6 dal 20, 7–8 dal 50. Sono punti di controllo dello stesso fiume: il C++ arriva al ponte 5/20/50 esattamente con i valori della tabella (`$B4A4`: livello, `$B4A8`: contatore dei ponti in BCD). Il port costruisce la stessa tabella facendo girare il generatore per `-seed N` (il fiume della partita 1 dallo stato LFSR N; il fiume della cartuccia è $F7D4E3 = 16241891) e per `-bridge N` (ogni partita dal ponte N, 1–9999; dopo 80 il livello va 79, 80, 79...).
- `$CB` = $2E in NTSC e $2F in PAL (A077, dal registro PAL); influisce sulla probabilità di un deposito all'inizio del blocco (AC48). Il fiume differisce leggermente tra PAL e NTSC.
- Partenza e ricomparsa: `sub_B0F1` carica $4D–$5F da `$BAE1`, pulisce $2B–$4C e $0500–$05FF, mette il ponte di partenza nello slot 10 (tipo $0E, y=$4A, altezza $17), modalità $23 = $84/$80. Poi A3C8 ripristina LFSR e riva dal punto di controllo $66–$69. Dopo il gioco genera 160–165 righe a 2 per fotogramma (velocità $FE) per riempire lo schermo.
- Una trappola nella verifica: mentre lo schermo si riempie a 2 righe per fotogramma, il confine del fotogramma dell'emulatore può cadere tra due righe, quindi il confronto fotogramma per fotogramma in quei fotogrammi è sbagliato. Nel gioco normale (una riga per fotogramma) l'istantanea è sempre su un confine di riga.

## Mappa del livello
- Il generatore della riva non dipende dall'aereo: l'aggiramento degli oggetti guarda il tipo `$050B`, che non cambia quando un oggetto viene abbattuto. `$60` (numero della sezione) cresce ogni 1024 righe quando il ponte viene generato, non quando viene distrutto (nella partita del bot `$60` è cambiato al fotogramma 1133, il ponte è stato distrutto al 1177).
- Quindi l'intero livello si può filmare con un aereo invulnerabile: `rr_oracle -ghost` modifica 5 byte nella memoria emulata (A3E9/A424/A42E/A432 `LDX flag` → `LDX #$FF`, A443 consumo di carburante $20 → 0). Nessuna morte in 120 000 fotogrammi.
- A velocità normale il livello si sposta esattamente di 1 riga dello schermo per fotogramma, e la nuova riga compare sulla riga 20 dello schermo. Una striscia della riga 20 di ogni fotogramma coincide con lo schermo dal vivo, tranne gli oggetti in movimento.
- `$60` sale fino a $50 (80), poi alterna $4F/$50; lo stato dell'LFSR ai confini delle sezioni non si è ripetuto in 117 sezioni, quindi il fiume non si ripete, ma la difficoltà smette di crescere dopo la sezione 80.
- Costruzione delle immagini per sezione: `tools/level_map.py`. Il visualizzatore è pubblicato come pagina «Карта реки River Raid».

## Metodo
Una regola si legge prima dal listato, poi si verifica su una traccia di riferimento (`tools/verify_trace.py`). Se la traccia non coincide, si cerca l'errore di lettura. Esempio: il rifornimento è stato ricavato dal codice A3E9–A453 (compreso il non ovvio +2 invece di +1 dovuto al flag di riporto) e confermato dalla traccia; l'unica discrepanza ha mostrato che il controllo delle collisioni avviene prima che gli oggetti passino da uno slot all'altro.

## Versione britannica su cassetta
`River Raid.cas` (UK, 600 baud): due file. Quello di avvio (7 blocchi in $0A00, partenza $0A06) controlla che ci siano almeno 24K di RAM («NOT ENOUGH RAM MEMORY - NEED 24K»), mostra una schermata del titolo, carica $2200 byte in $1000 e poi salta a $3000. È il codice della cartuccia spostato in RAM, confrontato istruzione per istruzione: da A21B in poi tutte le oltre 2900 istruzioni sono le stesse operazioni; codice e dati spostati da $A000 a $1000, le pagine di RAM del gioco in su di $2000 (display list $3F00 → $5F00, anello dello schermo $2000–$3FFF → $4000–$5FFF, $1000 → $3F00), le variabili di pagina zero per lo più +2 ($25 → $A1, $28 → $A2); i puntatori nelle tabelle spostati allo stesso modo. Differenze vere:
- l'inizializzazione (A000–A0EE) spostata in $3000, con una somma di controllo del codice caricato in $E4/$E5;
- protezione dalla copia: se la somma non torna o c'è un gestore `D:` in HATABS (avvio da DOS), il ciclo principale si blocca o salta nel mezzo di un'istruzione. La protezione propria della cartuccia (A151–A158) scrive $FF nel proprio codice in $A19B se $6000 non contiene $47: innocuo in ROM, fatale in una copia in RAM;
- la scritta scorrevole dice «1982, 1983, 1984»: un carattere «4» in più (copiato in $07C0), il testo più lungo di 5 caratteri, inizio/fine dello scorrimento $20/$4B → $1B/$46.

Generatore, logica di gioco, suono e tabelle sono identici; anche il controllo PAL.

## Determinismo e salti
- **Il fiume dipende solo da sé.** Il generatore non dipende né dal giocatore, né da RANDOM, né dagli oggetti: l'aggiramento di un oggetto guarda il suo tipo (`$050B`) e la X dell'oggetto ancora in fase di disegno, che non si muove ancora. L'LFSR fa un passo per riga, tranne sui tratti diritti verso un ponte: esattamente 993 passi da un ponte all'altro. Il suo periodo dalla partenza del gioco 1 è di 16 766 977 passi, e MCD(993, 16 766 977) = 1, quindi lo stato a un ponte torna solo dopo 16 766 977 ponti (con la parità del livello, 79/80, forse dopo il doppio; non provato): circa 9 anni di volo senza fermarsi. Il contatore dei ponti del pannello ha 4 cifre: dopo 9999 non mostra nulla (0000) e riparte da 1.
- **Livello.** In sub $B194 (`sectionCheckpoint`) un livello da $50 in su viene preso come $4E prima di salire di uno, quindi dopo 80 il livello alterna 79/80. Le sezioni dispari e pari sono diverse (dispari: codice di larghezza sempre 7, oggetti ovunque di traverso; pari: larghezza casuale, isole), l'alternanza conserva entrambi i tipi; la difficoltà smette di crescere.
- **Punti di controllo.** A ogni ponte il generatore salva l'LFSR e la riva. Una partenza dal ponte N (`-bridge N` nel gioco, una ricomparsa) li ripristina: da lì il fiume è lo stesso di un volo senza fermarsi, gli oggetti no (tutti appena piazzati, nessuno sveglio, nessuno spostato).
- **RANDOM.** Un contatore a 17 bit che a ogni ciclo della macchina fa un passo lungo un giro fisso di 131 071 stati. Il port tiene la sua posizione nel giro (`src/pokey_random.h`), quindi il valore a qualsiasi ciclo si conosce subito. Quale nemico si sveglia dipende dunque dal numero di fotogrammi dall'accensione.
- **Gli oggetti vivono solo finché sono sullo schermo.** Il generatore mette un oggetto in alto, e l'oggetto sparisce una volta uscito dal bordo inferiore, circa 200 fotogrammi dopo; così il carro armato (prepare: `tankY` ≥ $BC) e il suo proiettile. Quindi tutto lo stato in un dato momento segue dal fiume in quel momento più gli ultimi ~200 fotogrammi. `Game::skipFrame` fa un fotogramma «solo fiume»: lo scorrimento, i contatori dei fotogrammi, il timer della modalità dimostrativa, la scritta scorrevole e ciò che prepare() fa allo stato del fiume (il carro armato e il proiettile che escono sotto lo schermo; senza questo un vecchio carro armato impedisce al generatore di piazzare il successivo). Poi fotogrammi interi. Verificato contro il volo dall'inizio (4 fiumi e partenze con `-bridge`, 77 punti ciascuno, i 6000 fotogrammi dopo ognuno): da 200 fotogrammi interi in poi ogni fotogramma è disegnato uguale e tutto lo stato coincide. Le variabili di un carro armato, proiettile o missile di elicottero inattivo possono avere altri valori vecchi; vengono reimpostate prima dell'uso e non sono disegnate.
- **Nella modalità dimostrativa niente distrugge niente.** L'esplosione del proiettile del carro armato distrugge un oggetto solo nel codice di volo dell'aereo (A4C1), che senza aereo non gira.
- **Salvaschermo.** Dopo 128 × 2048 fotogrammi (circa 73 minuti) senza input, bit 7 in $78: il ciclo principale si ferma (A1F5) e lo schermo diventa nero, fino a un joystick, un tasto della console o della tastiera. Il gioco mantiene questo comportamento; il visualizzatore azzera $78 a ogni fotogramma.
- **Lo scorrimento nel visualizzatore** (`viewer/river_view.cpp`). Ancore (lo stato del fiume e i contatori, ~170 byte) ogni 1024 fotogrammi del presente; istantanee intere (~16 KB) ogni 128 fotogrammi, solo per gli ultimi 8192 fotogrammi prima del presente e intorno al punto del replay (64). La riga L si mostra dall'istantanea più vicina o da un'ancora: solo il fiume, poi 400 fotogrammi interi (altri 512 se poi si scorre indietro). B trova il ponte successivo con il solo generatore e ci vola allo stesso modo; `-bridge N` vola dall'inizio del fiume. Memoria: ~190 byte per ponte più ~2 MB fissi.
- **Salvataggi nel gioco** (F7 / F8, sul gamepad RB / LB; `Game::save` / `load`). A differenza del visualizzatore, qui non si può lasciar assestare nulla: l'aereo incontra gli oggetti, quindi si salva tutto lo stato, ma solo ciò che non si può rifare. Salvati: le variabili (`GameState`) con la memoria player-missile, lo stato del generatore e lo stato del generatore prima della riga più vecchia della memoria dello schermo con il numero di righe; la memoria dello schermo (le ultime 256 righe del fiume, 12 KB) viene rifatta da lì, perché le righe dipendono solo dal generatore (per questo il gioco ricorda lo stato del generatore prima di ogni riga sullo schermo, `keepLineStates`, circa 31 KB durante il gioco). Non salvati: i dati per riga di scansione (`scan`), che display() azzera e ricostruisce a ogni fotogramma, e l'immagine, disegnata dal fotogramma successivo (anche in pausa: la pausa ferma solo la logica del gioco). La memoria player-missile (1,25 KB: 4 player come colonne di 8 pixel alte 256 righe, più i missili; il chip video legge un byte per riga per ciascuno) non si può rifare: il gioco non sempre cancella una forma prima di disegnarla altrove, e i resti partecipano alle collisioni; ma è quasi tutta di zeri. Tutto è compresso per sequenze di zeri (un byte zero, poi la lunghezza della sequenza): 400–620 byte con il contatore RANDOM e il suono di POKEY (invece di 110 KB con memoria dello schermo e immagine). Verificato: 12 salvataggi (volo a varie velocità, esplosione, riempimento dopo uno schianto, fine partita, pausa), i 900 fotogrammi dopo ciascuno: immagine e suono uguali.

## Prossimi passi
Nell'analisi non resta nulla di aperto. Per il port: l'esatta sequenza di accensione (A000–A17C) e la temporizzazione PAL (50 Hz, la display list PAL).
