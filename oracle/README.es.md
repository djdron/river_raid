# rr_oracle — ejecución de referencia de River Raid

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Polski](README.pl.md) | [Italiano](README.it.md) | Español | [中文](README.zh.md) | [日本語](README.ja.md)

Ejecuta el cartucho original sin ventana sobre libatari800 (Atari 800, NTSC por defecto, AltirraOS integrado, BASIC desactivado). Sirve para comparar el port a C++ con el original fotograma a fotograma.

## Compilación
```
./build.sh
```
El script clona atari800 (commit probado 98cc2dc), compila `libatari800.a` y enlaza `rr_oracle`. Necesita git, un compilador de C (gcc o clang), make, autoconf y automake; funciona en Linux, macOS (Xcode command line tools, `brew install autoconf automake`), WSL y MSYS2. La ejecución de referencia intercepta tres funciones del emulador (escrituras en POKEY, lecturas de RANDOM, el NMI del retorno vertical): el script compila otra vez `pokey.c` y `cpu.c` con esas funciones renombradas, así que no hace falta ningún truco del enlazador.

## Ejecución
```
./rr_oracle <rom> <script> <frames> <outdir> [-pal] [-seed N] [-bot|-bot-nofire] [-ghost] [-noram] [-ram A:B] [-strip Y] [-screens A:B] [-shot N,N,...]
./rr_oracle ../rom/River_Raid.rom scripts/start_and_fly.txt 1000 out -shot 240,500
./rr_oracle ../rom/River_Raid.rom scripts/start_only.txt 8000 out -bot      # bot
./rr_oracle ../rom/River_Raid.rom scripts/bot_run_8000.txt 8000 out         # replay of a bot run
```

## Bot (`-bot`)
Primero juega el script (por ejemplo `start_only.txt`: encendido, START, espera) y después toma el control el piloto automático. Se dirige hacia el centro del agua que tiene delante, esquiva los objetos cercanos, dispara todo el tiempo y pulsa «arriba» mientras explota o espera en el puente. El bot es sencillo y pensado solo para trazas largas, no para récords: no busca combustible y muere por los helicópteros que llegan de lado.

`-bot-nofire`: el mismo piloto automático sin disparar ni esquivar.

La entrada aplicada realmente en cualquier ejecución se escribe en `inputs.txt` con el formato de los scripts. Volver a darla sin `-bot` produce una traza idéntica byte a byte (comprobado en 8000 fotogramas).

## Opciones para filmar el nivel
- `-ghost` — avión invulnerable (parche de 5 bytes de código en la memoria emulada, solo para filmar).
- `-strip Y` — la línea Y de la pantalla de cada fotograma en `strip.bin`, la paleta en `palette.bin`.
- `-screens A:B` — pantallas completas de los fotogramas A…B en `screens.bin`.
- `-noram` — no escribir `ram.bin` (para ejecuciones largas).
- `-ram A:B` — escribir `ram.bin` solo para los fotogramas A…B (fotograma A en el desplazamiento 0).
- `-wav` — la salida de sonido del emulador en `sound.wav`.
- `-vbi` — `vbi.bin`: la RAM al principio de cada interrupción de retorno vertical (registros de 4 bytes de número de fotograma + 16 384 bytes), el estado que ve el controlador de sonido.
- `frames.csv` tiene las columnas `$60, $2F, $5D, $AA, $AB, $AC, $76, $52, $53, $54, $2D, $4E`.

## Scripts
| Archivo | Qué |
|---|---|
| `start_and_fly.txt` | START, aceleración, giros, disparos (1000 fotogramas) |
| `attract_fire_only.txt` | disparo en la pantalla de título (el juego no empieza) |
| `start_only.txt` | encendido, START, espera; para `-bot` |
| `probe_lr.txt`, `probe_hold.txt` | pulsaciones cortas / mantenidas para buscar variables |
| `bot_run_8000.txt` | ejecución grabada del bot: 2 puentes, 5 muertes, fin de partida, nuevo inicio |
| `refuel_first_depot.txt` | vuelo sobre el primer depósito sin disparar (repostaje) |
| `attract_idle.txt` | encendido y ninguna entrada: el modo de demostración (río de la partida 1, créditos deslizantes) |
| `two_players.txt` | SELECT a la partida 2, START; ambos jugadores vuelan recto disparando hasta perder todos los aviones; dos veces la tecla de pausa |
| `ghost_film.txt` | inicio y vuelo sin entrada; con `-ghost -noram -strip 20` filma el mapa del nivel |

## Script de entrada
Cada línea: `<fotogramas> <joystick> <disparo> [start] [select] [option] [joystick2] [disparo2] [tecla]`; `#` empieza un comentario. La última línea se repite hasta el final de la ejecución. `tecla` mantiene pulsada la barra espaciadora (cualquier tecla pausa River Raid).
Bits del joystick: 1 arriba, 2 abajo, 4 izquierda, 8 derecha, 0 centrado.
El juego empieza con la tecla START; disparar en la pantalla de título no lo inicia.

## Qué va a `<outdir>`
| Archivo | Contenido |
|---|---|
| `ram.bin` | $0000–$3FFF (16 KB) tras cada fotograma; el fotograma N está en el desplazamiento N*16384 |
| `pokey.log` | cada escritura en POKEY: `fotograma línea ciclo registro valor` |
| `random.log` | cada lectura de RANDOM: `fotograma línea ciclo valor`. La línea ~249 es el sonido (VBI), las demás líneas son la lógica del juego (A205) |
| `frames.csv` | entrada aplicada (joystick, disparo, START, luego tras las columnas de la CPU SELECT, OPTION, joystick 2, disparo 2, tecla) y registros de la CPU al final del fotograma |
| `inputs.txt` | entrada aplicada con el formato de los scripts (para repetirla) |
| `fNNNNN.ppm` | capturas de pantalla de 384×240 |

## Determinismo
atari800 por sí mismo arranca el generador de POKEY desde el reloj del sistema, así que las ejecuciones no se repiten. `rr_oracle` fija el contador de forma explícita (`-seed`, 0 por defecto). Con la misma ROM, el mismo script y la misma semilla todos los archivos son idénticos byte a byte (comprobado).

Licencia: libatari800 es GPL-2.0+, así que `rr_oracle`, como programa enlazado con ella, también. Es una herramienta de desarrollo: el port a C++ no se enlaza con ella y solo lee sus archivos de salida.
