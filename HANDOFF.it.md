# River Raid (Atari 8-bit) → port in C++: stato del progetto

[English](HANDOFF.md) | [Русский](HANDOFF.ru.md) | [Українська](HANDOFF.uk.md) | [Deutsch](HANDOFF.de.md) | [Polski](HANDOFF.pl.md) | Italiano | [Español](HANDOFF.es.md) | [中文](HANDOFF.zh.md) | [日本語](HANDOFF.ja.md)

Questo file serve a riprendere il lavoro su qualsiasi computer o in una nuova chat. Iniziate una nuova sessione con «leggi HANDOFF.it.md e docs/NOTES.it.md».

## Obiettivo
Un port in C++ di River Raid (Activision, Atari 800, 1983, di Carol Shaw) per Windows, Linux e macOS:
- logica di gioco identica all'originale, leggibile da un programmatore qualunque senza conoscere il 6502;
- suono identico: lo stesso driver audio più un nucleo POKEY;
- uno strato multipiattaforma: SDL3 (SDL2 come opzione, per UWP su Xbox);
- nessun dato originale viene distribuito; il port legge la ROM dell'utente all'avvio (deciso: nessun dato della ROM nel codice).

## Stato
Il gioco è completo e giocabile: `port/game/river_raid.cpp` (modalità dimostrativa, giochi 1–8, uno e due giocatori, l'aereo, il carburante, i nemici, i carri armati, i ponti, i punti, le vite, i record, la pausa, il suono). Compilato e provato su Windows, macOS (Apple M2) e Linux. Confrontato fotogramma per fotogramma con la RAM dell'originale in esecuzioni del bot, a due giocatori, a vuoto e lunghe; il driver audio scrive gli stessi valori in POKEY. Riga di comando e tasti: `port/README.it.md`, «Il gioco». Come funziona il gioco, per tutti: `docs/HOW_IT_WORKS.it.md`.

## Cosa serve
- L'intera cartella del progetto.
- La vostra ROM `River_Raid.rom`: 8192 byte, senza intestazione, caricata a $A000–$BFFF. Non è nel repository; mettetela in `rom/River_Raid.rom`.

## Struttura
| Percorso | Cosa |
|---|---|
| `port/` | il port in C++: gioco, visualizzatore del fiume, strumenti di confronto; vedi `port/README.it.md` (compilazione, opzioni, tasti, contenuto, verifica) |
| `docs/HOW_IT_WORKS.it.md` | come funziona il gioco, per tutti: fiume, nemici, ponti, collisioni, perché diventa più difficile |
| `docs/NOTES.it.md` | scoperte con indirizzi |
| `disasm/river_raid_a8.asm` | listato completo con intestazione (punti d'ingresso, fotogramma, variabili) |
| `tools/dis6502.py` | disassemblatore: `python dis6502.py <rom> <out.asm> B500 B2A5 B485` |
| `tools/check_lfsr.py`, `tools/verify_trace.py` | verifica delle regole documentate sulle tracce di riferimento |
| `tools/level_map.py` | immagini dell'intero fiume, sezione per sezione |
| `tools/doc_figures.py` | le immagini di `docs/HOW_IT_WORKS.it.md` (`docs/img/`), da `port/tools/river_dump` |
| `oracle/` | esecuzione di riferimento su libatari800: `rr_oracle.c`, `build.sh`, `README.md`, script di input, autopilota `-bot`, aereo invulnerabile `-ghost` |
| `docs/oracle_demo.png` | fotogrammi di un'esecuzione di riferimento |

## Fatti accertati (tutti verificati)
1. **Gestori.** Ciclo principale A000–AF5A, un passaggio = un fotogramma (sincronizzato su VCOUNT $50/$78). VBI $B2A5 = driver audio POKEY e contatori. DLI $B500 = multiplexing degli sprite per riga e lettura delle collisioni. IRQ $B485 = vuoto.
2. **Generatore del fiume.** LFSR software a 24 bit (`sub_B183`, $AA/$AB/$AC), un passo per riga; tutto (rive, isole, decorazioni, oggetti) viene da esso e dal livello. Non dipende né dal giocatore né da RANDOM. Il fiume non si ripete per circa 17 000 ponti (periodo dell'LFSR dalla partenza del gioco 1: 16 766 977 passi). Il C++ (`port/src/river_gen.cpp`) coincide byte per byte: 119 758 righe e partenze da zero.
3. **Punti di partenza.** I giochi 1–8 partono dai ponti 1/5/20/50 dello stesso fiume: le tabelle $B494–$B4A8 contengono lì LFSR, riva, livello e contatore dei ponti; il generatore, avviato dalla partenza del gioco 1, arriva esattamente a quei valori. Il port riempie la stessa tabella facendo girare il generatore per `-seed N` (un altro fiume) e `-bridge N` (partenza da un ponte qualsiasi).
4. **Difficoltà.** Il livello (= il ponte verso cui si vola, fino a 80, poi 79/80 alternati) sposta le soglie con cui si confrontano i numeri casuali: nemici a metà blocco, meno depositi fino al livello 46 (47 su PAL), mongolfiere dal 3, caccia dal 5, carri sulle rive dal 7, elicotteri con missili dal 13, curve più ampie. Tabella in `docs/HOW_IT_WORKS.it.md`.
5. **RANDOM ($D20A).** La logica di gioco lo legge solo ad A205: ogni 16 fotogrammi si sveglia uno slot a caso (0–10) (bit $40 di `$42,X`). B362/B3D0: rumore per il suono. Il valore dipende dal ciclo della CPU in cui avviene la lettura; il port modella il contatore (`src/pokey_random.h`), avviato dall'orologio come a una vera accensione; `-random N` ripete un'esecuzione.
6. **Le collisioni** vengono dai registri di collisione del GTIA, letti dal DLI dopo la fascia di ogni oggetto; se un contatto sulla riga di lettura conta dipende dal ciclo. Il port esegue il DLI contando i cicli (`src/display.cpp`, la temporizzazione non esatta al ciclo di atari800).
7. **PAL/NTSC.** Una sola cartuccia per entrambi: il bit PAL ($D014 → $BF) sceglie la variante della display list ($B77A) e il limite dei depositi ($CB = $2E/$2F). Nient'altro cambia, quindi su PAL l'intero gioco va a 50 fotogrammi al secondo, circa il 17% più lento. La versione su cassetta britannica (`.cas`, 2 file: caricatore a $0A00, gioco a $1000–$31FF) è lo stesso codice spostato in RAM, con una protezione dalla copia diversa e «1982, 1983, 1984» nella riga che scorre.
8. **Il font** è copiato dalla ROM del sistema ($E000/$E100/$E300); il port ha incorporato il font di AltirraOS, `-os` carica quello dell'Atari.
9. **Esecuzione di riferimento.** atari800 fa partire POKEY dall'orologio; `rr_oracle -seed` (default 0) rende le esecuzioni identiche byte per byte. La compilazione di atari800 fallisce con `-Werror` su una ridefinizione di ULONG; `build.sh` compila con `CFLAGS="-g -O2 -DNETSIO"`. Commit di atari800 provato: 98cc2dc.

## Limiti dell'ambiente
- `oracle/build.sh` compila su Linux, macOS, WSL e MSYS2 (le funzioni dell'emulatore intercettate vengono rinominate in compilazione, niente `--wrap` di GNU); il port in C++ compila con un normale CMake (MSVC, Xcode, gcc).
- La ROM non è nel repository (`.gitignore`); mettete il vostro dump in `rom/River_Raid.rom`.
- Il codice è indentato con tabulazioni, anche le righe di continuazione (`.editorconfig`). Documentazione: `X.md` in inglese, `X.ru.md` in russo, e poi `X.uk.md`, `X.de.md`, `X.pl.md`, `X.it.md`, `X.es.md`, `X.zh.md`, `X.ja.md`; `tools/doc_languages.py` scrive la riga delle lingue sotto ogni titolo, le didascalie delle immagini sono in `tools/doc_figures_i18n.py`. Una modifica a un documento va fatta in tutte le lingue.

## Metodo
Una regola si legge dal listato e poi si verifica su una traccia di riferimento (`tools/verify_trace.py`, poi gli strumenti di confronto in C++). Le tracce da sole servono solo a capire dove guardare. Le modifiche che non dovrebbero cambiare il comportamento (rinomine, ristrutturazioni) si verificano con `port/tools/equivalence` contro un commit più vecchio: richieste 0 differenze, più tutte le verifiche di `port/README.it.md`, «Verifica».

## Decisioni
- Il port è scritto in C++, non come involucro attorno a un emulatore.
- La verifica confronta la RAM del port con il `ram.bin` di riferimento fotogramma per fotogramma; i valori RANDOM per essa vengono dal `random.log` di riferimento.
- Nessun dato della ROM nel codice: le tabelle della cartuccia si leggono tramite funzioni d'accesso con nome (`port/src/rom.h`).
- Colori di default: PAL, modello di Altirra; NTSC e il modello di atari800 con i tasti.
- La finestra mostra l'immagine del gioco (364×200 con tutta la scritta scorrevole), non l'intero fotogramma con il bordo nero (`-border`, O); scalata in modo netto per numeri interi, poi in modo morbido (`-filter`, L).
- La corsa sui cicli alla fine della partita (se il contatore dei fotogrammi venga azzerato prima o dopo il VBI di quel fotogramma dipende dai cicli del ciclo principale, vedi `port/README.it.md`, «Verifica») volutamente non viene riprodotta.
- Su uno schermo a 60/120 Hz il gioco si aggancia allo schermo (60,00 invece di 59,92 fotogrammi al secondo), il suono viene ricampionato di conseguenza.

## Questioni aperte
- L'accensione vera e propria (A000–A17C, i primi 38 fotogrammi) viene disegnata come schermo vuoto; `Game::powerOn` non è esatto.
- PAL: la display list e la temporizzazione a 50 Hz non vengono riprodotte (`-pal` cambia solo il limite dei depositi e i colori).

## Prossimi passi
1. Facoltativo: temporizzazione PAL (50 Hz, display list PAL) per chi ricorda il gioco da un televisore europeo.
2. Facoltativo: salvataggio e caricamento dello stato (tutto lo stato è in `GameState` + `RiverState`, dati semplici).
3. Facoltativo: una build UWP/Xbox su SDL2 (`-DRR_SDL2=ON`).
