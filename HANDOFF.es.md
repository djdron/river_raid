# River Raid (Atari 8-bit) → port a C++: estado del proyecto

[English](HANDOFF.md) | [Русский](HANDOFF.ru.md) | [Українська](HANDOFF.uk.md) | [Deutsch](HANDOFF.de.md) | [Polski](HANDOFF.pl.md) | [Italiano](HANDOFF.it.md) | Español | [中文](HANDOFF.zh.md) | [日本語](HANDOFF.ja.md)

Este archivo sirve para retomar el trabajo en cualquier ordenador o en un chat nuevo. Empiece una sesión nueva con «lee HANDOFF.es.md y docs/NOTES.es.md».

## Objetivo
Un port a C++ de River Raid (Activision, Atari 800, 1983, de Carol Shaw) para Windows, Linux y macOS:
- lógica del juego idéntica a la del original, legible para un programador cualquiera sin saber 6502;
- sonido idéntico: el mismo controlador de sonido más un núcleo POKEY;
- una capa multiplataforma: SDL3 (SDL2 como opción, para UWP en la Xbox);
- no se distribuye ningún dato original; el port lee la ROM del usuario al ejecutarse (decidido: ningún dato de la ROM en el código).

## Estado
El juego está completo y se puede jugar: `port/game/river_raid.cpp` (modo de demostración, partidas 1–8, uno y dos jugadores, el avión, el combustible, los enemigos, los tanques, los puentes, los puntos, las vidas, los récords, la pausa, el sonido). Compilado y probado en Windows, macOS (Apple M2) y Linux. Comparado fotograma a fotograma con la RAM del original en ejecuciones del bot, a dos jugadores, en reposo y largas; el controlador de sonido escribe en POKEY los mismos valores. Línea de comandos y teclas: `port/README.es.md`, «El juego». Cómo funciona el juego, para todos: `docs/HOW_IT_WORKS.es.md`.

## Qué necesita
- Toda la carpeta del proyecto.
- Su propia ROM `River_Raid.rom`: 8192 bytes, sin cabecera, se carga en $A000–$BFFF. No está en el repositorio; póngala en `rom/River_Raid.rom`.

## Estructura
| Ruta | Qué |
|---|---|
| `port/` | el port a C++: juego, visor del río, herramientas de comparación; vea `port/README.es.md` (compilación, opciones, teclas, contenido, verificación) |
| `docs/HOW_IT_WORKS.es.md` | cómo funciona el juego, para todos: río, enemigos, puentes, colisiones, por qué se vuelve más difícil |
| `docs/NOTES.es.md` | hallazgos con direcciones |
| `disasm/river_raid_a8.asm` | listado completo con cabecera (puntos de entrada, fotograma, variables) |
| `tools/dis6502.py` | desensamblador: `python dis6502.py <rom> <out.asm> B500 B2A5 B485` |
| `tools/check_lfsr.py`, `tools/verify_trace.py` | comprobación de las reglas documentadas con las trazas de referencia |
| `tools/level_map.py` | imágenes de todo el río, sección por sección |
| `tools/doc_figures.py` | las imágenes de `docs/HOW_IT_WORKS.es.md` (`docs/img/`), a partir de `port/tools/river_dump` |
| `oracle/` | ejecución de referencia sobre libatari800: `rr_oracle.c`, `build.sh`, `README.md`, scripts de entrada, piloto automático `-bot`, avión invulnerable `-ghost` |
| `docs/oracle_demo.png` | fotogramas de una ejecución de referencia |

## Hechos establecidos (todos verificados)
1. **Manejadores.** Bucle principal A000–AF5A, una pasada = un fotograma (sincronizado con VCOUNT $50/$78). VBI $B2A5 = controlador de sonido POKEY y contadores. DLI $B500 = multiplexado de sprites por línea y lectura de colisiones. IRQ $B485 = vacío.
2. **Generador del río.** LFSR por software de 24 bits (`sub_B183`, $AA/$AB/$AC), un paso por línea; todo (orillas, islas, adornos, objetos) sale de él y del nivel. No depende ni del jugador ni de RANDOM. El río no se repite en unos 17 000 puentes (periodo del LFSR desde el inicio de la partida 1: 16 766 977 pasos). El C++ (`port/src/river_gen.cpp`) coincide byte a byte: 119 758 líneas e inicios desde cero.
3. **Puntos de inicio.** Las partidas 1–8 empiezan en los puentes 1/5/20/50 del mismo río: las tablas $B494–$B4A8 guardan allí el LFSR, la orilla, el nivel y el contador de puentes; el generador, puesto en marcha desde el inicio de la partida 1, llega exactamente a esos valores. El port rellena la misma tabla haciendo correr el generador para `-seed N` (otro río) y `-bridge N` (empezar en cualquier puente).
4. **Dificultad.** El nivel (= el puente hacia el que se vuela, hasta 80, luego 79/80 alternados) desplaza los umbrales con los que se comparan los números aleatorios: enemigos a mitad de bloque, menos depósitos hasta el nivel 46 (47 en PAL), globos desde el 3, cazas desde el 5, tanques en la orilla desde el 7, helicópteros con misiles desde el 13, curvas más amplias. Tabla en `docs/HOW_IT_WORKS.es.md`.
5. **RANDOM ($D20A).** La lógica del juego solo lo lee en A205: cada 16 fotogramas despierta un slot al azar (0–10) (bit $40 de `$42,X`). B362/B3D0: ruido para el sonido. El valor depende del ciclo de la CPU en que se lee; el port modela el contador (`src/pokey_random.h`), arrancado desde el reloj como en un encendido real; `-random N` repite una ejecución.
6. **Las colisiones** vienen de los registros de colisión del GTIA, que el DLI lee tras la franja de cada objeto; que un contacto en la línea de lectura cuente depende del ciclo. El port ejecuta el DLI contando ciclos (`src/display.cpp`, la temporización no exacta al ciclo de atari800).
7. **PAL/NTSC.** Un solo cartucho para ambos: el bit PAL ($D014 → $BF) elige la variante de la lista de visualización ($B77A) y el límite de depósitos ($CB = $2E/$2F). Nada más cambia, así que en PAL todo el juego va a 50 fotogramas por segundo, un 17% más lento. La versión británica en casete (`.cas`, 2 archivos: cargador en $0A00, juego en $1000–$31FF) es el mismo código trasladado a la RAM, con otra protección anticopia y «1982, 1983, 1984» en la línea deslizante.
8. **La fuente** se copia de la ROM del sistema ($E000/$E100/$E300); el port lleva integrada la fuente de AltirraOS, `-os` carga la del Atari.
9. **Ejecución de referencia.** atari800 arranca POKEY desde el reloj; `rr_oracle -seed` (0 por defecto) hace las ejecuciones idénticas byte a byte. La compilación de atari800 falla con `-Werror` por una redefinición de ULONG; `build.sh` compila con `CFLAGS="-g -O2 -DNETSIO"`. Commit de atari800 probado: 98cc2dc.

## Límites del entorno
- `oracle/build.sh` compila en Linux, macOS, WSL y MSYS2 (las funciones del emulador interceptadas se renombran al compilar, sin `--wrap` de GNU); el port a C++ compila con un CMake normal (MSVC, Xcode, gcc).
- La ROM no está en el repositorio (`.gitignore`); ponga su volcado en `rom/River_Raid.rom`.
- El código se sangra con tabuladores, también las líneas de continuación (`.editorconfig`). Documentación: `X.md` en inglés, `X.ru.md` en ruso, y además `X.uk.md`, `X.de.md`, `X.pl.md`, `X.it.md`, `X.es.md`, `X.zh.md`, `X.ja.md`; `tools/doc_languages.py` escribe la línea de idiomas bajo cada título, los rótulos de las imágenes están en `tools/doc_figures_i18n.py`. Un cambio en un documento se hace en todos los idiomas.

## Método
Una regla se lee del listado y luego se comprueba con una traza de referencia (`tools/verify_trace.py`, luego las herramientas de comparación en C++). Las trazas por sí solas solo sirven para saber dónde mirar. Los cambios que no deberían alterar el comportamiento (renombrados, reestructuraciones) se comprueban con `port/tools/equivalence` frente a un commit anterior: se exigen 0 diferencias, más todas las comprobaciones de `port/README.es.md`, «Verificación».

## Decisiones
- El port está escrito en C++, no como envoltorio de un emulador.
- La verificación compara la RAM del port con el `ram.bin` de referencia fotograma a fotograma; los valores RANDOM para ello vienen del `random.log` de referencia.
- Ningún dato de la ROM en el código: las tablas del cartucho se leen mediante funciones de acceso con nombre (`port/src/rom.h`).
- Colores por defecto: PAL, modelo de Altirra; NTSC y el modelo de atari800 con teclas.
- La ventana muestra la imagen del juego (364×200 con todo el rótulo deslizante), no el fotograma completo con su borde negro (`-border`, O); escalada nítida por números enteros y luego suavemente (`-filter`, L).
- La carrera de ciclos al final de la partida (que el contador de fotogramas se reinicie antes o después del VBI de ese fotograma depende de los ciclos del bucle principal, vea `port/README.es.md`, «Verificación») no se reproduce a propósito.
- En una pantalla de 60/120 Hz el juego se sincroniza con la pantalla (60,00 en lugar de 59,92 fotogramas por segundo), y el sonido se recalcula en consecuencia.

## Preguntas abiertas
- El encendido en sí (A000–A17C, los primeros 38 fotogramas) se dibuja como una pantalla vacía; `Game::powerOn` no es exacto.
- PAL: la lista de visualización y la temporización de 50 Hz no se reproducen (`-pal` solo cambia el límite de depósitos y los colores).

## Próximos pasos
1. Opcional: temporización PAL (50 Hz, lista de visualización PAL) para quienes conocieron el juego en un televisor europeo.
2. Opcional: guardar y cargar el estado (todo el estado está en `GameState` + `RiverState`, datos simples).
3. Opcional: una versión UWP/Xbox sobre SDL2 (`-DRR_SDL2=ON`).
