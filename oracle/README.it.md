# rr_oracle — esecuzione di riferimento di River Raid

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Polski](README.pl.md) | Italiano | [Español](README.es.md) | [中文](README.zh.md) | [日本語](README.ja.md)

Esegue la cartuccia originale senza finestra su libatari800 (Atari 800, NTSC di default, AltirraOS incorporato, BASIC disattivato). Serve a confrontare il port in C++ con l'originale fotogramma per fotogramma.

## Compilazione
```
./build.sh
```
Lo script clona atari800 (commit provato 98cc2dc), compila `libatari800.a` e collega `rr_oracle`. Servono git, un compilatore C (gcc o clang), make, autoconf e automake; funziona su Linux, macOS (Xcode command line tools, `brew install autoconf automake`), WSL e MSYS2. L'esecuzione di riferimento intercetta tre funzioni dell'emulatore (scritture su POKEY, letture di RANDOM, l'NMI del ritorno verticale): lo script compila un'altra volta `pokey.c` e `cpu.c` con quelle funzioni rinominate, così non servono trucchi del linker.

## Esecuzione
```
./rr_oracle <rom> <script> <frames> <outdir> [-pal] [-seed N] [-bot|-bot-nofire] [-ghost] [-noram] [-ram A:B] [-strip Y] [-screens A:B] [-shot N,N,...]
./rr_oracle ../rom/River_Raid.rom scripts/start_and_fly.txt 1000 out -shot 240,500
./rr_oracle ../rom/River_Raid.rom scripts/start_only.txt 8000 out -bot      # bot
./rr_oracle ../rom/River_Raid.rom scripts/bot_run_8000.txt 8000 out         # replay of a bot run
```

## Bot (`-bot`)
Prima gioca lo script (per esempio `start_only.txt`: accensione, START, attesa), poi subentra l'autopilota. Punta verso il centro dell'acqua davanti, schiva gli oggetti vicini, spara sempre e preme «su» mentre esplode o aspetta al ponte. Il bot è semplice e pensato solo per tracce lunghe, non per i record: non cerca il carburante e muore per gli elicotteri che arrivano di lato.

`-bot-nofire`: lo stesso autopilota senza sparare né schivare.

L'input effettivamente applicato in ogni esecuzione viene scritto in `inputs.txt` nel formato degli script. Ridandolo senza `-bot` si ottiene una traccia identica byte per byte (verificato su 8000 fotogrammi).

## Opzioni per filmare il livello
- `-ghost` — aereo invulnerabile (patch di 5 byte di codice nella memoria emulata, solo per filmare).
- `-strip Y` — la riga Y dello schermo di ogni fotogramma in `strip.bin`, la tavolozza in `palette.bin`.
- `-screens A:B` — schermi completi dei fotogrammi A…B in `screens.bin`.
- `-noram` — non scrivere `ram.bin` (per esecuzioni lunghe).
- `-ram A:B` — scrivere `ram.bin` solo per i fotogrammi A…B (fotogramma A all'offset 0).
- `-wav` — l'uscita audio dell'emulatore in `sound.wav`.
- `-vbi` — `vbi.bin`: la RAM all'inizio di ogni interrupt di vertical blank (record di 4 byte di numero del fotogramma + 16 384 byte), lo stato che vede il driver audio.
- `frames.csv` ha le colonne `$60, $2F, $5D, $AA, $AB, $AC, $76, $52, $53, $54, $2D, $4E`.

## Script
| File | Cosa |
|---|---|
| `start_and_fly.txt` | START, accelerazione, virate, fuoco (1000 fotogrammi) |
| `attract_fire_only.txt` | fuoco sulla schermata del titolo (il gioco non parte) |
| `start_only.txt` | accensione, START, attesa; per `-bot` |
| `probe_lr.txt`, `probe_hold.txt` | tocchi brevi / pressioni prolungate per trovare le variabili |
| `bot_run_8000.txt` | esecuzione del bot registrata: 2 ponti, 5 morti, fine partita, nuova partenza |
| `refuel_first_depot.txt` | volo sopra il primo deposito senza sparare (rifornimento) |
| `attract_idle.txt` | accensione e nessun input: la modalità dimostrativa (fiume del gioco 1, titoli che scorrono) |
| `two_players.txt` | SELECT sul gioco 2, START; entrambi i giocatori volano dritti sparando finché non perdono tutti gli aerei; due volte il tasto di pausa |
| `ghost_film.txt` | partenza e volo senza input; con `-ghost -noram -strip 20` filma la mappa del livello |

## Script di input
Ogni riga: `<fotogrammi> <joystick> <fuoco> [start] [select] [option] [joystick2] [fuoco2] [tasto]`; `#` inizia un commento. L'ultima riga si ripete fino alla fine dell'esecuzione. `tasto` tiene premuta la barra spaziatrice (qualsiasi tasto mette in pausa River Raid).
Bit del joystick: 1 su, 2 giù, 4 sinistra, 8 destra, 0 al centro.
Il gioco parte con il tasto START; il fuoco sulla schermata del titolo non lo fa partire.

## Cosa finisce in `<outdir>`
| File | Contenuto |
|---|---|
| `ram.bin` | $0000–$3FFF (16 KB) dopo ogni fotogramma; il fotogramma N è all'offset N*16384 |
| `pokey.log` | ogni scrittura su POKEY: `fotogramma riga ciclo registro valore` |
| `random.log` | ogni lettura di RANDOM: `fotogramma riga ciclo valore`. La riga ~249 è il suono (VBI), le altre righe sono la logica di gioco (A205) |
| `frames.csv` | input applicato (joystick, fuoco, START, poi dopo le colonne della CPU SELECT, OPTION, joystick 2, fuoco 2, tasto) e registri della CPU alla fine del fotogramma |
| `inputs.txt` | input applicato nel formato degli script (per riprodurlo) |
| `fNNNNN.ppm` | schermate 384×240 |

## Determinismo
atari800 di suo fa partire il generatore di POKEY dall'orologio di sistema, quindi le esecuzioni non sono ripetibili. `rr_oracle` imposta il contatore in modo esplicito (`-seed`, default 0). Con la stessa ROM, lo stesso script e lo stesso seed tutti i file sono identici byte per byte (verificato).

Licenza: libatari800 è GPL-2.0+, quindi lo è anche `rr_oracle` come programma collegato a essa. È uno strumento di sviluppo: il port in C++ non si collega a essa e legge solo i suoi file di uscita.
