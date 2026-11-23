# River Raid (Atari 8-bit) → port in C++: stato del progetto

[English](HANDOFF.md) | [Русский](HANDOFF.ru.md) | [Українська](HANDOFF.uk.md) | [Deutsch](HANDOFF.de.md) | [Polski](HANDOFF.pl.md) | Italiano | [Español](HANDOFF.es.md) | [中文](HANDOFF.zh.md) | [日本語](HANDOFF.ja.md)

Questo file serve a riprendere il lavoro su qualsiasi computer o in una nuova chat. Iniziate una nuova sessione con «leggi HANDOFF.it.md e docs/NOTES.it.md».

## Obiettivo
Un port in C++ di River Raid (Activision, Atari 800, 1983, di Carol Shaw) per Windows, Linux e macOS:
- logica di gioco identica all'originale, leggibile da un programmatore qualunque senza conoscere il 6502;
- suono identico: lo stesso driver audio più un nucleo POKEY;
- uno strato multipiattaforma: SDL3 (SDL2 come opzione; UWP su Xbox con un fork di SDL3 che lo mantiene, vedi [port/uwp/README.it.md](port/uwp/README.it.md); l'homebrew per Nintendo Switch sull'SDL2 di devkitPro, vedi [port/switch/README.it.md](port/switch/README.it.md));
- nessun dato originale viene distribuito; il port legge la ROM dell'utente all'avvio (deciso: nessun dato della ROM nel codice).

## Stato
Il gioco è completo e giocabile: `port/game/river_raid.cpp` (modalità dimostrativa, giochi 1–8, uno e due giocatori, l'aereo, il carburante, i nemici, i carri armati, i ponti, i punti, le vite, i record, la pausa, il suono). Compilato e provato su Windows, macOS (Apple M2) e Linux. Confrontato fotogramma per fotogramma con la RAM dell'originale in esecuzioni del bot, a due giocatori, a vuoto e lunghe; il driver audio scrive gli stessi valori in POKEY. Riga di comando e tasti: [port/README.it.md](port/README.it.md), «Il gioco». Come funziona il gioco, per tutti: [docs/HOW_IT_WORKS.it.md](docs/HOW_IT_WORKS.it.md).

## Cosa serve
- L'intera cartella del progetto.
- La vostra ROM `River_Raid.rom`: 8192 byte, senza intestazione, caricata a $A000–$BFFF. Non è nel repository; mettetela in `rom/River_Raid.rom`.

## Struttura
| Percorso | Cosa |
|---|---|
| `port/` | il port in C++: gioco, visualizzatore del fiume, strumenti di confronto; vedi [port/README.it.md](port/README.it.md) (compilazione, opzioni, tasti, contenuto, verifica) |
| [docs/HOW_IT_WORKS.it.md](docs/HOW_IT_WORKS.it.md) | come funziona il gioco, per tutti: fiume, nemici, ponti, collisioni, perché diventa più difficile |
| [docs/NOTES.it.md](docs/NOTES.it.md) | scoperte con indirizzi |
| [docs/DLI_REMOVAL.it.md](docs/DLI_REMOVAL.it.md) | esperimenti: la visualizzazione senza il modello del DLI (branch `dli_removal`, `dli_removed`) |
| [docs/OOP.md](docs/OOP.md) | la riscrittura a oggetti (branch `oop`): regole, verifiche, passi (per ora in inglese) |
| `disasm/river_raid_a8.asm` | listato completo con intestazione (punti d'ingresso, fotogramma, variabili) |
| `tools/dis6502.py` | disassemblatore: `python dis6502.py <rom> <out.asm> B500 B2A5 B485` |
| `tools/check_lfsr.py`, `tools/verify_trace.py` | verifica delle regole documentate sulle tracce di riferimento |
| `tools/level_map.py` | immagini dell'intero fiume, sezione per sezione |
| `tools/doc_figures.py` | le immagini di [docs/HOW_IT_WORKS.it.md](docs/HOW_IT_WORKS.it.md) (`docs/img/`), da `port/tools/river_dump` |
| `oracle/` | esecuzione di riferimento su libatari800: `rr_oracle.c`, `build.sh`, `README.md`, script di input, autopilota `-bot`, aereo invulnerabile `-ghost` |
| `docs/oracle_demo.png` | fotogrammi di un'esecuzione di riferimento |

## Fatti accertati (tutti verificati)
1. **Gestori.** Ciclo principale A000–AF5A, un passaggio = un fotogramma (sincronizzato su VCOUNT $50/$78). VBI $B2A5 = driver audio POKEY e contatori. DLI $B500 = multiplexing degli sprite per riga e lettura delle collisioni. IRQ $B485 = vuoto.
2. **Generatore del fiume.** LFSR software a 24 bit (`sub_B183`, $AA/$AB/$AC), un passo per riga; tutto (rive, isole, decorazioni, oggetti) viene da esso e dal livello. Non dipende né dal giocatore né da RANDOM. Il fiume non si ripete per 16,8 milioni di ponti (periodo dell'LFSR: 16 766 977 passi, 993 passi per ponte). Il C++ (`port/src/river_gen.cpp`) coincide byte per byte: 119 758 righe e partenze da zero.
3. **Punti di partenza.** I giochi 1–8 partono dai ponti 1/5/20/50 dello stesso fiume: le tabelle $B494–$B4A8 contengono lì LFSR, riva, livello e contatore dei ponti; il generatore, avviato dalla partenza del gioco 1, arriva esattamente a quei valori. Il port riempie la stessa tabella facendo girare il generatore per `-seed N` (un altro fiume) e `-bridge N` (partenza da un ponte qualsiasi).
4. **Difficoltà.** Il livello (= il ponte verso cui si vola, fino a 80, poi 79/80 alternati) sposta le soglie con cui si confrontano i numeri casuali: nemici a metà blocco, meno depositi fino al livello 46 (47 su PAL), mongolfiere dal 3, caccia dal 5, carri sulle rive dal 7, elicotteri con missili dal 13, curve più ampie. Tabella in [docs/HOW_IT_WORKS.it.md](docs/HOW_IT_WORKS.it.md).
5. **RANDOM ($D20A).** La logica di gioco lo legge solo ad A205: ogni 16 fotogrammi si sveglia uno slot a caso (0–10) (bit $40 di `$42,X`). B362/B3D0: rumore per il suono. Il valore dipende dal ciclo della CPU in cui avviene la lettura; il port modella il contatore (`src/pokey_random.h`), avviato dall'orologio come a una vera accensione; `-random N` ripete un'esecuzione.
6. **Le collisioni** vengono dai registri di collisione del GTIA, letti dal DLI dopo la fascia di ogni oggetto; se un contatto sulla riga di lettura conta dipende dal ciclo. Il port esegue il DLI contando i cicli (`src/display.cpp`, la temporizzazione non esatta al ciclo di atari800). Nei branch `dli_removed` e `oop` il DLI non c'è: le collisioni sono semplice geometria (sotto).
7. **PAL/NTSC.** Una sola cartuccia per entrambi: il bit PAL ($D014 → $BF) sceglie la variante della display list ($B77A) e il limite dei depositi ($CB = $2E/$2F). La logica di gioco per fotogramma è la stessa, quindi su PAL l'intero gioco va a 50 fotogrammi al secondo, circa il 17% più lento; cambia solo ciò che dipende dal tempo di CPU in un fotogramma: l'accensione arriva alla modalità dimostrativa al fotogramma 25 invece che al 38, sub $B0F1 cancella la memoria in 8 fotogrammi invece di 13 e al game over il VBI arriva prima di sub $B21A. Il `-pal` del port fa tutto questo (verificato con `rr_oracle -pal`, vedi [port/README.it.md](port/README.it.md)). La versione su cassetta britannica (`.cas`, 2 file: caricatore a $0A00, gioco a $1000–$31FF) è lo stesso codice spostato in RAM, con una protezione dalla copia diversa e «1982, 1983, 1984» nella riga che scorre.
8. **Il font** è copiato dalla ROM del sistema ($E000/$E100/$E300); il port ha incorporato il font di AltirraOS, `-os` carica quello dell'Atari.
9. **Esecuzione di riferimento.** atari800 fa partire POKEY dall'orologio; `rr_oracle -seed` (default 0) rende le esecuzioni identiche byte per byte. La compilazione di atari800 fallisce con `-Werror` su una ridefinizione di ULONG; `build.sh` compila con `CFLAGS="-g -O2 -DNETSIO"`. Commit di atari800 provato: 98cc2dc.
10. **Determinismo.** Il fiume dipende solo dal ponte (993 passi dell'LFSR da un ponte all'altro; si ripete dopo 16,8 milioni di ponti), gli oggetti solo dagli ultimi ~200 fotogrammi. Quindi si raggiunge qualsiasi momento facendo girare il solo fiume e poi qualche centinaio di fotogrammi interi; così scorre il visualizzatore. Dettagli: [docs/NOTES.it.md](docs/NOTES.it.md), «Determinismo e salti». I salvataggi nel gioco usano lo stesso: la memoria dello schermo viene rifatta dallo stato del generatore (nel branch `oop`, da quello del fiume e degli oggetti), quindi un salvataggio occupa circa 0,2–0,6 KB.

## Limiti dell'ambiente
- `oracle/build.sh` compila su Linux, macOS, WSL e MSYS2 (le funzioni dell'emulatore intercettate vengono rinominate in compilazione, niente `--wrap` di GNU); il port in C++ compila con un normale CMake (MSVC, Xcode, gcc).
- La ROM non è nel repository (`.gitignore`); mettete il vostro dump in `rom/River_Raid.rom`.
- Il codice è indentato con tabulazioni, anche le righe di continuazione (`.editorconfig`). Documentazione: `X.md` in inglese, `X.ru.md` in russo, e poi `X.uk.md`, `X.de.md`, `X.pl.md`, `X.it.md`, `X.es.md`, `X.zh.md`, `X.ja.md`; `tools/doc_languages.py` scrive la riga delle lingue sotto ogni titolo, le didascalie delle immagini sono in `tools/doc_figures_i18n.py`. Una modifica a un documento va fatta in tutte le lingue.

## UWP e Xbox
- Compilato su un fork di SDL3 che mantiene UWP: [djdron/SDL3-uwp](https://github.com/djdron/SDL3-uwp), ramo `river-raid` (su XboxEmulationHub/SDL3-uwp, ramo `uwp-3.4.20`); `port/CMakeLists.txt` lo prende con `CMAKE_SYSTEM_NAME=WindowsStore`. Le patch sono solo per questo progetto: SDL non accetta contributi scritti con l'IA generativa, quindi non vengono proposte lì. Cosa cambiano: [docs/NOTES.it.md](docs/NOTES.it.md), «UWP e il fork di SDL3». Dopo una modifica al fork, cancellate `_deps/sdl3-*` nella cartella di build e rilanciate CMake.
- CI: `.github/workflows/uwp.yml` compila l'app UWP su windows-latest, senza la ROM, a ogni push che cambia `port/`; mostra solo che compila e si impacchetta. Una modifica al solo fork non lo avvia.
- Provato con Visual Studio 2026 su Windows 11 (con scala al 175%) e su una Xbox Series S in Dev Mode, su SDL2 e su SDL3.
- Sull'Xbox: qualcuno deve aver effettuato l'accesso (altrimenti Visual Studio segnala 0x8004090a); in Dev Home impostate App type su Game. Un tasto tenuto premuto mentre l'app parte non le arriva mai, quindi PAL si sceglie nel gioco (Back + Start). In Launch with parameters di Dev Home (Use custom protocol) tutto ciò che segue il primo spazio viene scartato, quindi gli spazi si scrivono come `%20`: `riverraid:-pal%20-bridge%20100`. Su PC il link (Win+R) e i Command Line Arguments di Visual Studio accettano gli spazi così come sono.

## Branch `dli_removed`
Questo branch (da `dli_removal`) non ha il DLI: `Game::displayGeometry` mette ogni oggetto alla y del suo slot e calcola le collisioni come semplice geometria, dove le righe degli sprite e degli oggetti si toccano in qualsiasi punto; la riva e il bordo dal primo e dall'ultimo punto di una riga contro le sequenze della riga. Qui è la modalità predefinita (`RR_DISPLAY=geometry`); `RR_DISPLAY=simple` è il ciclo del DLI con le sue letture, come l'originale, `beam` il modello a cicli, `check` entrambi. Il gioco differisce dall'originale nello 0,01–0,25% dei fotogrammi (un contatto che l'originale vede un fotogramma dopo o non vede affatto), quindi `game_check` contro l'originale si esegue con `RR_DISPLAY=simple`; `RR_DISPLAY=simple RR_GEOMCHECK=1` conta i fotogrammi in cui geometria e letture differiscono (`=2` stampa ciascuno). I port per ZX Spectrum Next e TS-Conf ([river_raid_zx](https://github.com/djdron/river_raid_zx)) sono costruiti su questa visualizzazione e confrontati con essa fotogramma per fotogramma. I passi e le loro verifiche: [docs/DLI_REMOVAL.it.md](docs/DLI_REMOVAL.it.md).

## Branch `oop`
Questo branch (da `dli_removed`) è lo stesso gioco scritto come oggetti che tengono il proprio stato: l'aereo, il suo proiettile, il generatore del fiume (`RiverGen`, solo il fiume), gli oggetti sul fiume (`Objects`, una classe per ogni tipo di comportamento), il carro armato, il suo proiettile e il missile, i giocatori, il pannello, i suoni, il fiume sullo schermo; `World` fa avanzare insieme il fiume e i suoi oggetti, `Game` tiene tutto ed esegue il fotogramma. Byte e BCD come li tiene l'originale, niente virgola mobile; niente memoria player-missile e nessuno dei modelli del DLI (`RR_DISPLAY` non c'è più: la visualizzazione è la geometria di `dli_removed`). Gioca esattamente come `dli_removed`: `port/tools/trace/compare.sh` confronta 17 partite con il commit di riferimento fotogramma per fotogramma (`-quick`: 5). Ogni classe elenca i propri byte nella RAM dell'originale (`mapRam`), così `game_check` e le verifiche dei port ZX funzionano ancora. I passi: [docs/OOP.md](docs/OOP.md) (per ora in inglese).

## Metodo
Una regola si legge dal listato e poi si verifica su una traccia di riferimento (`tools/verify_trace.py`, poi gli strumenti di confronto in C++). Le tracce da sole servono solo a capire dove guardare. Le modifiche che non dovrebbero cambiare il comportamento (rinomine, ristrutturazioni) si verificano con `port/tools/equivalence` contro un commit più vecchio: richieste 0 differenze, più tutte le verifiche di [port/README.it.md](port/README.it.md), «Verifica». La riscrittura a oggetti (branch `oop`) si verifica invece con `port/tools/trace/compare.sh`: 17 partite del gioco, fotogramma per fotogramma, contro il commit di riferimento.

## Decisioni
- Il port è scritto in C++, non come involucro attorno a un emulatore.
- La verifica confronta la RAM del port con il `ram.bin` di riferimento fotogramma per fotogramma; i valori RANDOM per essa vengono dal `random.log` di riferimento.
- Nessun dato della ROM nel codice: le tabelle della cartuccia si leggono tramite funzioni d'accesso con nome (`port/src/rom.h`).
- Colori di default: PAL, modello di Altirra; NTSC e il modello di atari800 con i tasti.
- La finestra mostra l'immagine del gioco (364×200 (`-pal`: 364×213) con tutta la scritta scorrevole), non l'intero fotogramma con il bordo nero (`-border`, O); scalata in modo netto per numeri interi, poi in modo morbido (`-filter`, L).
- La corsa sui cicli alla fine della partita (se il contatore dei fotogrammi venga azzerato prima o dopo il VBI di quel fotogramma dipende dai cicli del ciclo principale, vedi [port/README.it.md](port/README.it.md), «Verifica») volutamente non viene riprodotta.
- Su uno schermo a 60/120 Hz il gioco si aggancia allo schermo (60,00 invece di 59,92 fotogrammi al secondo; con `-pal` su uno schermo a 50/100 Hz 50,00 invece di 49,86), il suono viene ricampionato di conseguenza.
- PAL/NTSC si può cambiare anche nel gioco (P; gamepad Back + Start): la macchina viene riaccesa come l'altra; sull'Xbox il televisore passa a 50 Hz e ritorna.

## Questioni aperte
- L'accensione vera e propria (A000–A17C, i primi 38 fotogrammi) viene disegnata come schermo vuoto; `Game::powerOn` non è esatto.

## Prossimi passi
Niente in programma.
