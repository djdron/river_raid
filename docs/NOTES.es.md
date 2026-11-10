# River Raid (Atari 8-bit) — hallazgos

[English](NOTES.md) | [Русский](NOTES.ru.md) | [Українська](NOTES.uk.md) | [Deutsch](NOTES.de.md) | [Polski](NOTES.pl.md) | [Italiano](NOTES.it.md) | Español | [中文](NOTES.zh.md) | [日本語](NOTES.ja.md)

Imagen: 8192 bytes, sin cabecera CART, se carga en $A000–$BFFF. Código ~5,9 KB (2941 instrucciones), datos ~2,3 KB.

## Puntos de entrada
| Dirección | Qué | Cómo se encontró |
|---|---|---|
| $A000 | arranque + bucle principal del fotograma | tráiler del cartucho ($BFFA) |
| $B2A5 | VBI (VVBLKI $0222): sonido POKEY, contadores de fotogramas, registros por fotograma | escritura en $0222/$0223 en A0D1 |
| $B500 | DLI (VDSLST $0200): multiplexado de sprites línea a línea | escritura en $0200/$0201 en A0C7 |
| $B485 | IRQ vacía | escritura en $0216/$0217 |

## Fotograma
- `L_AE99`: espera VCOUNT < $50 (ha empezado un fotograma nuevo), prepara la display list y los sprites.
- `L_A197`: espera VCOUNT >= $50, lee el joystick (PORTA, TRIG0/1), ejecuta la lógica del juego.
- `A18D`: espera VCOUNT = $78 y luego escribe NMIEN ($C0 = DLI+VBI, $40 = solo VBI).
- Una vuelta del bucle = un paso de lógica, así que el modelo «update() una vez por fotograma» es correcto.

## Qué importa para el port
1. **Números aleatorios.** `RANDOM` ($D20A, el polinomio de POKEY) se lee en 3 sitios. La lógica del juego solo en A205: cada 16 fotogramas elige una ranura enemiga (0–10) y pone el bit $40 en sus banderas. Las otras dos lecturas (B362, B3D0) solo dan ruido para el sonido. El valor depende del ciclo de CPU de la lectura (el polinomio de 17 bits de POKEY corre al reloj de la máquina). El port modela el contador (`port/src/pokey_random.h`) y lo arranca desde el reloj, como en un encendido real; para la comparación fotograma a fotograma los valores del original salen de la traza de referencia (`random.log`).
2. **PAL/NTSC.** El bit PAL se lee de $D014 (A06A), se guarda en $BF y elige la variante de la display list ($B77A, copiada en la display list en $3FA9), el límite de depósitos `$CB` ($2E/$2F) y las ramas en B135/B14F/B741. Nada compensa la frecuencia de fotogramas: en PAL el juego va a 50 fotogramas por segundo, alrededor de un 17% más lento. Nunca hubo una versión PAL aparte: la cinta británica es el mismo código (véase «Versión británica en cinta»). `-pal` en el port fija el límite de depósitos; la display list PAL y los 50 Hz aún no se reproducen.
3. **Fuente del sistema.** El código copia el juego de caracteres de la ROM del sistema ($E000/$E100/$E300) a $0600–$09FF. Es una dependencia externa: hace falta una fuente propia o la ROM del usuario (Altirra OS es libre).
4. **Atadura al haz.** Los sprites los dibuja un núcleo DLI con WSYNC en cada línea; en C++ eso se convierte en un renderizador por software que trabaja con el estado del fotograma.
5. **Sonido.** El controlador vive entero en el VBI ($B2A5–$B484): 4 canales POKEY, AUDF/AUDC calculados a partir de $C6, $7A–$7F, $50, $24 y las tablas $B4AC/$B4C0/$B4E1. Reproducir este controlador más un núcleo POKEY basta para una coincidencia exacta.
6. **Estilo del código.** Monolítico, las variables las reutilizan distintos subsistemas, las banderas están empaquetadas en bits. Un mapeo completo necesita trazas de un emulador.

## Tablas de datos (por referencias en el código)
Todas se leen mediante funciones con nombre en `port/src/rom.h`.
- $B4AC (4): timbre del motor (AUDC) según la palanca arriba/abajo; $B4C0 (17): tono del silbido del proyectil del tanque; $B4E1: sonido de la vida extra.
- $B4D1: formas del tanque (pares de punteros, leídos mediante $A1/$A2 en AEB9).
- $B776 (4 bytes) se copia a $3F00 (inicio de la display list); $B77A (15 bytes, en PAL los 15 siguientes) a $3FA9, el resto de la display list.
- $BB70 (60 bytes): texto del panel, copiado a $1000; $BEEE (64): rótulo de Activision → $06C0; $BF2E (112): símbolos del panel → $0818; $BF9E (24): «P1»/«P2» → $08D0; $BFB6 (48): las cifras de «1982, 1983» → $07D0.
- $BAAF: siguiente tipo de tanque; $BAC5: colores del proyectil; $BB24: siguiente tipo de objeto (animación); $BB40: desplazamientos de las formas de los objetos desde $B900.

## Ejecución de referencia (hecho)
`oracle/rr_oracle` ejecuta el original sobre libatari800 y escribe la RAM de cada fotograma, cada escritura en POKEY y cada lectura de RANDOM (véase `oracle/README.es.md`).
- El juego empieza con la tecla START; disparar en la pantalla de título no lo inicia.
- atari800 arranca el generador de POKEY desde el reloj; con `-seed` las ejecuciones son idénticas byte a byte.
- Lecturas de RANDOM: la línea de barrido ~249 es el sonido desde el VBI, las líneas 190–192 y 241 son la lógica del juego (A205). Para C++ esto significa que en modo de verificación los valores aleatorios del juego se toman de `random.log` y no se emulan.

## Generador del nivel (confirmado por la traza)
- El río y la colocación de objetos los guía un LFSR por software de 24 bits en `sub_B183` sobre $AA/$AC/$AB. Llamado desde A756 y A767.
- Un paso: `fb = bit7(AB) ^ bit2(AB)`, luego un desplazamiento a la izquierda de la cadena AA←fb, AC←bit7(AA), AB←bit7(AC). El modelo en Python explica 265 de 268 cambios de la traza; los otros 3 son recargas.
- El valor inicial de AA/AB/AC es $E3/$D4/$F7. Se guarda en $66–$68 y se restaura en A3C8 tras el inicio y tras una muerte (reinicio desde el puente).
- Con semillas de POKEY distintas, durante 1000 fotogramas $AA–$AC, $66–$68, los tipos de objeto ($0500…) y su X ($0516…) son idénticos. Solo difieren las banderas de las ranuras $42–$4C (bit $40 de A205), el byte de trabajo $07 y la pila.
- Conclusión: el nivel es siempre el mismo. `RANDOM` solo afecta al comportamiento de los enemigos (cuál recibe la bandera $40). Al principio de la partida el movimiento de los enemigos parece desactivado (la comprobación de $60 en A297), por eso no se notaba en una ejecución corta.

## Variables (fase 2)
Estado: **comprobado** significa que la regla se comprobó con `tools/verify_trace.py` sobre trazas de referencia sin excepciones. **del código** significa que el valor se leyó del listado sin una comprobación aparte con la traza. **hipótesis** es una suposición aún por confirmar.

### Jugador
| Dirección | Qué | Estado |
|---|---|---|
| $57 | X del avión (unidades HPOS; en la pantalla de referencia centro ≈ 2·X−55 px) | comprobado: solo se mueve con la palanca inclinada |
| $58 | parte fraccionaria de X; al desbordarse $57 se mueve ±1 (A462–A493) | del código |
| $59 | velocidad: 128 normal, hasta ~142 con arriba, hasta ~114 con abajo, vuelve a 128 al soltar | de la traza, sin comprobación aparte |
| $5B:$5C | combustible, 16 bits, $FFFF es el depósito lleno. Código A3E9–A453, en cada fotograma de vuelo: sobre un depósito `$5B += 2` (es `ADC #1` con el acarreo que dejó `CMP #$0E`), limitado a $FFFF; luego siempre `$5C −= $20` con préstamo de `$5B` (−1 en $5B cada 8 fotogramas). Si al pedir prestado $5B es 0, el avión muere. El consumo por fotograma no depende de la velocidad. La aguja del indicador de combustible HPOSM1 = $5B/8 + $5C en el DLI en B527 (allí $5C es la constante `#$5C`, no la variable) | comprobado, incluido el repostaje: +480 por fotograma, límite $FFFF |
| $1D | ranura con la que chocó el avión; bit 7 = ninguna | comprobado mediante el repostaje |
| $7A / $7B | evento de sonido: 4 repostaje, 3 depósito lleno (A40E–A422); $7B = 8 en cada cambio, luego el VBI cuenta hacia atrás | del código y la traza |
| $6A | vidas de reserva: 4 al empezar la partida, 3 tras el despegue, 0 en la última vida | de la traza |
| $76 | fase de la vida: 128 al empezar la partida; 49/47 espera en el puente antes de despegar (cualquier movimiento de la palanca despega); 0 vuelo; 253 y luego −3 por fotograma explosión; 130→127 reaparición (combustible a $FFFF) | de la traza |

### Puntos y puentes
| Dirección | Qué | Estado |
|---|---|---|
| $63/$64/$65 | puntos, BCD, byte más significativo primero, guardados divididos entre 10 (950 en pantalla = `00 00 95`); al desbordarse 999999 y $6A = 0 (A563) | comprobado |
| $BB0B + tipo | puntos/10 por objeto (BCD): 8 helicóptero 60, $0D barco 30, $0E puente 500, $0F combustible 80, $10 puente con tanque 750; la tabla también tiene 7 → 100, $0A/$0B → 150 (no vistos en trazas) | comprobado para 8, $0D, $0E, $0F, $10 |
| $61/$62 | número del puente actual, BCD (como «BRIDGE n» en pantalla), +1 al destruir un puente (A4F0) | del código y la traza |
| $38 bit 7 | el tanque de la carretera se destruyó junto con el puente (A513–A517) | del código, 1 caso en la traza |
| $39 | X del tanque de la carretera (HPOSP1 en el DLI en B54D) | del código |

También se dan puntos por el enemigo contra el que chocó el avión: su ranura pasa al estado 3. Derribado por una bala pasa al estado 2.

### Ranuras de objetos (11, índice X = 0…10)
Una ranura es una franja horizontal de la pantalla, no un número fijo de objeto: al desplazarse, un objeto pasa a la ranura siguiente (en la traza un depósito pasó por las ranuras 8 → 9 → 10). La comprobación de colisiones de un fotograma ocurre antes de ese paso.

| Dirección | Qué | Estado |
|---|---|---|
| $0C+X | posición vertical del objeto (+1 por fotograma a velocidad normal; ≈ 240–247 junto al avión) | de la traza |
| $0500+X | tipo actual del objeto. ≥ 7 es un objeto vivo: 7…$0D enemigos (8/9 fotogramas del helicóptero, $0D barco), $0E puente, $0F depósito. < 7 está vacío o explotando: 2 alcanzado por una bala, 3 colisión; también aparecen 0, 1, 4–6. En colisión (A3E9): 7…$0D muerte, $0E muerte + `INC $2E`, $0F repostaje | comprobado (2/3 mediante puntos, $0F mediante repostaje) |
| $050B+X | tipo de objeto (índice en la tabla de puntos): 0, 1, 8, $0D, $0E, $0F en las trazas | comprobado mediante puntos |
| $0516+X | X del objeto | del código (A500–A511, comparado con $39) |
| $42+X | banderas: bit 7, bit 6 ($40, puesto desde RANDOM en A205), bits bajos | sin analizar |
| $4E | índice de la ranura que se procesa (comparado con #$0B) | del código |

### Ejecuciones de referencia para el mapeo
- `oracle/scripts/bot_run_8000.txt`: entrada grabada por el bot (`rr_oracle -bot`), 8000 fotogramas. Contiene 2 puentes (uno con tanque), 5 muertes, el fin de la partida hacia el fotograma 3300, luego la pantalla de título y un nuevo inicio hacia el 5323. Sin repostaje real: el bot derriba los depósitos.
- Reproducir este archivo sin el bot da `ram.bin`, `pokey.log` y `random.log` idénticos byte a byte.

- `oracle/scripts/refuel_first_depot.txt`: vuelo sobre el primer depósito sin disparar (32 fotogramas sobre el depósito, el combustible llega a $FFFF).

## Generación del terreno (código A6FD–A9D7, dibujo AA8C–AB4B)
La orilla no se guarda como imagen, sino como unos pocos números por línea. Cada nueva línea de la pantalla es un paso del generador, tras el cual la línea se dibuja en el búfer circular de la pantalla (la dirección de la línea sale de la display list, $3F04/$3F05).

**Desplazamiento (del código).** En cada fotograma la velocidad `$59` se suma dos veces al acumulador `$30`, y cada desbordamiento produce una línea nueva. A velocidad 128 es exactamente 1 línea por fotograma; con `$59 ≥ $FE` se produce una línea sin acumular.

**Estructura (del código).**
- `$2F` — número de línea en el bloque, 0…31.
- `$5A` — bloques que quedan en el tramo (2).
- `$5D` — tramos que quedan hasta el puente, se recarga con 16; al final se llama a `sub_B194`.
- El LFSR `sub_B183` da un paso en cada línea, salvo en las líneas con el bit 7 en `$23` (las aproximaciones rectas al puente).
- En el límite de un tramo se toma un nuevo objetivo de anchura `$5F` del nibble bajo de `$AA` con límites (mínimo 2, máximo 15/14/8 según `$2D`, `$60`, `$B5`), luego `$34 = $5F·4`.

**Números por línea.**
| Dirección | Qué | Estado |
|---|---|---|
| $52 | posición de la orilla | comprobado (camino aleatorio, abajo) |
| $C3 | dirección del camino: +1, $FF (−1) o 0 | comprobado |
| $54 | media anchura de la isla / de la parte interior | comprobado (suavizado) |
| $53 | anchura del agua; cambia en −2 × el paso de `$54`, así que la anchura total se conserva | comprobado |
| $34, $35 | objetivo para `$54` y el paso hacia él (±1 si la diferencia es < 32, si no ±2) | comprobado |
| $2D | bit 7 — modo isla (cambia el dibujo), bit 6 | del código |
| $C4, $C5 | límites de `$52`: $1C…$24 con `$60 < 3`, si no más amplios (1…$40 con variantes según `$60`, `$052D`, `$36`) | del código, vistos 28…36 en trazas |
| $23 bit 7 | cerca del puente la orilla sigue la tabla `$BA8F[$2F] + $52`, sin azar | comprobado mediante C++ (`terrain_check`) |
| $052D | ≠ 0: la orilla se lleva al pasillo $18…$28 | comprobado mediante C++ (`terrain_check`) |

**Camino aleatorio de la orilla (A84F–A96A), comprobado.** Cada línea toma el nibble bajo `n = $AC & 15` (el byte del LFSR tras el paso de esta línea):
- si `$C3 = 0`, el movimiento empieza: +1 con `n ≥ 8`, si no −1;
- si la orilla ya se mueve: con `n < 8` se queda quieta; con `n = $D` da la vuelta y da un paso; con `n = $F` se queda quieta si `$0E` (`$0D` en una isla) es menor que `$5F` o que `$2C`; con cualquier otro valor da un paso;
- un paso que saldría de los límites `$C4/$C5` o chocaría con el objeto de la ranura actual `$4E` (tipo ≥ 8; el límite usa `$54/$34`, `$0516+X`, `$B5`, `$0521+X`) se sustituye por una vuelta en el sitio.

Sutileza de temporización: las ranuras y `$4E` se toman como están al principio del fotograma, y `$54/$34/$2D/$B5/$37` ya recalculados en este fotograma. Con esta regla el modelo coincide con las trazas en las 4669 líneas.

**Suavizado de la isla (A7FB–A824), comprobado.** Mientras `$35 ≠ 0`, cada línea hace `$54 += $35` y `$53 −= 2·$35`. Cuando `$54` alcanza `$34`, el paso `$35` se pone a cero. Comprobado en 4825 líneas, de las que 1195 cambiaron de verdad.

**Decoraciones (casas, montañas).** Empiezan al principio de un bloque (`$2F = 0`) cuando el bit 5 de `$AC` está puesto. Desplazamiento, anchura y número de filas salen de las tablas `$BBCC`, `$BBD4`, `$BBB2`, `$BBEB` según `$AA` y `$5F`, el puntero al mapa de bits de `$BBF0`. Comprobado mediante C++.

**Objetos.** Cada 16 líneas se coloca un objeto nuevo en la ranura libre más alta (`$4D`−1); el tipo se elige según `$AB` mediante la tabla `$BB1C`, la anchura del sprite de `$BBA2` (en `$0521+X`, es decir, `SIZEP3`: 1 para el barco, 3 para el puente, si no 0), el número de líneas de `$BB60` (+4). Mientras el objeto aún se está dibujando (`$32` líneas), la orilla esquiva ese objeto (`$4E`). Comprobado mediante C++.

**Puentes.** El generador solo dibuja un canal recto donde está el puente (orilla de la tabla `$BA8F`) y pone un objeto de tipo `$0E` en una ranura con bandera `$80/$84` en `$42+X` (una copia de `$23`). El puente en sí aparece solo al mostrarse, en el DLI (B575–B5FC): para una ranura con el bit 7 en sus banderas, el color de la orilla `COLPF0` en cada línea sale de la tabla `$B4E5` (`04 04 04 04 08×6 1C 1C 06×5 04 04 04`: las franjas grises de la carretera y la señalización amarilla), así que la carretera cruza toda la pantalla sobre los píxeles de la orilla; el tramo sobre el agua es el sprite P3 de esa ranura con anchura cuádruple, como en barcos y helicópteros. Tras el objeto el color de la orilla se restaura desde `$0B` (`$D2`/`$D6`: la orilla alterna dos verdes según la paridad del tramo). El tanque de la carretera es el sprite P1 (`$39`). Las líneas del mapa de bits de un puente no cambian tras generarse (comprobado en 119 líneas).

**Colores y sprites por línea (comprobados mediante C++, `port/src/river_draw.cpp`).** Los colores base llegan desde el retorno vertical (B408–B43E): agua `COLPF2` = $84 ($42 durante el destello), borde `COLPF1` = $20, orilla `COLPF0` de `sub_AFD0`: `($23 & 4) ^ $D6`, con el bit 2 invertido mientras el objeto más reciente (`$4E`) sea un puente con y < $5C. El DLI recorre las ranuras desde `$4D` hacia arriba con un contador de líneas de barrido (valor $5D en la línea 20, por donde entran las líneas nuevas): la primera ranura con `0 ≤ y + $BB60[tipo] − contador ≤ $BB60[tipo]` da la fila r; la forma es `$B900 + $BB40[tipo actual (+16 si el bit 3 de las banderas)] + r`, el color `$B700 + $BB30[tipo] + r`, HPOSP3 = `$0516+X` (32 = borde izquierdo del búfer), SIZEP3 = `$0521+X`; para una ranura de puente COLPF0 = `$B4E5 + r`. Frente a la línea 20 del original: 96,8% de las líneas idénticas con las posiciones del generador, 98,1% con las posiciones de objetos del juego, todas las demás mientras el tanque de la carretera está activo.

**Modo de demostración y la parte baja de la pantalla (comprobados mediante C++, `port/src/status_bar.cpp`).** Sin START el juego hace correr el río de la partida 1 desde el encendido: el rótulo deslizante arranca en el fotograma 11, el río en el 38, 1 línea por fotograma, a través de los puentes, con el mismo generador (la franja de la línea 20 coincide con C++ en 19 960 fotogramas salvo los objetos en movimiento). Display list en $3F00: 24 líneas en blanco, 160 líneas en modo E, luego el panel: 3 filas en modo 6 (anchura normal, `CHBASE` $08, colores $1E/$8E/$1E/$00 sobre $06, `PRIOR` 4) desde $1000, rellenadas a partir de `$BB70`; la aguja de combustible es el misil 1 en `$5B/8 + $5C`. Última fila: modo 6 ancho con `HSCROL`, `CHBASE` $06, color $0C, texto en `$B800 + $3FB3`; el VBI (B44B) avanza `HSCROL` 7→0 y luego el texto un carácter ($4B…$D6, en bucle) mientras `$2A` ≠ 0, y mueve con él el logotipo `$27`. El logotipo es el player 3 con anchura cuádruple (datos $FC) más el player 2 ($FF) desplazado un ciclo a la izquierda en cada fila, en los colores `$B48D`. Los juegos de caracteres en $0600/$0800 son la fuente del sistema (A0FB) con los caracteres del cartucho en $06C0, $0818, $08D0, $07D0 (las cifras de «1982, 1983»).

**Movimiento de objetos, tanque, misiles (comprobados mediante C++, `port/src/game.cpp`, `display.cpp`).** Cada 16 fotogramas una ranura elegida con `RANDOM & 15` (11–15 → 6–10) recibe el bit 6 en `$42+X` («despierta»), salvo que sea el objeto que aún se está dibujando. Los objetos de tipo 7 (caza) se mueven en cada fotograma; los tipos 8–13 solo despiertos, desde el puente 2 (`$60 >> 1 ≠ 0`), en fotogramas alternos según la paridad de la ranura (`X ^ $18 ^ $33`), el tipo $0C solo en fotogramas con `$18 & 2 = 0`; 1 ciclo por paso, bit 3 = izquierda, vuelta $2C…$D4. El bit 0 de `$42+X` lo pone el DLI cuando el sprite del objeto tocó en pantalla el color del borde de la orilla (P3PF bit 1); el paso siguiente invierte entonces la dirección una vez (el bit 4 marca el giro hasta que acaba el contacto). Los tipos por debajo de $0C se animan mediante `$BB24` cada 8 fotogramas (cada 2 para los tipos 7+). El tanque (`$38`–`$3A`) avanza cada 16 fotogramas (4 para los tipos 4–7) hasta que el player 1 toca el color del borde, y luego (`$41` bit 7) dispara: el proyectil es el player 0, `$50` cuenta 0…$38 (tabla de vuelo `$B4AF`, velocidad `$3E/$3F`), luego los fotogramas de la explosión. Un helicóptero de tipo $0A/$0B en la ranura `$51` lanza el misil 0 (`$C1` x, `$C2` y, paso `$C0` = ±2) hasta que da en la orilla (M0PF). Los registros de colisión del DLI se leen tras cada objeto y se borran, así que las colisiones se conocen por ranura (`$1A`–`$22` guardan el número de ranura). Con RANDOM reproducido, C++ coincide con la RAM del original en 19 900 fotogramas del modo de demostración desde el encendido.

**Controlador de sonido (comprobado mediante C++, `Game::soundDriver`).** Una vez por fotograma en el VBI (B2A5–B3D8), AUDCTL se queda en 0 (reloj de 64 kHz, ruido de 17 bits). `$C6` ≠ 0: pitido de inicio en el canal 1 ($40/$AF), el resto apagado. Canal 1: evento de sonido `$7A` 1–4 durante `$7B` fotogramas (tono $39/$11/$1F/$3F, volumen de `$7B`); si no, cuando el avión vuela, el aviso de combustible con `$5B` < $40 (un pitido cada 63 fotogramas, un tono descendente por debajo de $04) o el motor: AUDF = $1F − `$59`/16 (o el descendente `$052C` tras un choque), AUDC de los bits arriba/abajo de la palanca (`$B4AC`: $84 normal, $83/$86). Canal 2: ruido de explosión `$7C` (AUDF = RANDOM | $18) o, durante el juego, el misil del helicóptero `$7F`. Canal 3: el disparo `$7D` o el sonido de puntos `$7E` (`$B4E1`). Canal 4: el proyectil del tanque `$50` (`$B4C0` en vuelo, luego ruido). El modo de demostración es mudo: `$76` ≠ 0 apaga el canal 1, y nada más se activa.

**La partida con un jugador (comprobada mediante C++, `port/src/game.cpp`).** `$76` es el estado del avión: 0 vuelo; $FE…$80 explosión (126 fotogramas, `$5E` fotograma del avión hacia abajo cada 8); luego $7F: si quedan vidas, el río se reinicia (`sub_B0F1` con X = $12, que borra la pantalla y la memoria player-missile durante 13 fotogramas mientras solo corren las interrupciones) y se rellena de nuevo con `$59` = $FE (2 líneas por fotograma) mientras `$76` cuenta de $7F a $30; en $30 se quita una vida y `$052C` = $80 (el sonido creciente del motor); en $2F el avión espera en el puente sin desplazamiento hasta que un movimiento de la palanca o el disparo pone `$76` = 0 y `$59` = $40. Sin vidas: fin de la partida (récord `sub_B261` con bytes de control `$90`–`$9F`, `sub_B21A`: `$76` = `$2A` = $FF, «Game over» en el rótulo deslizante). En vuelo (A3E9–A5B4): colisión del avión con un objeto (`$1D`): tipos 7–$0E choque (un puente además cuenta `$2E`), tipo $0F reposta (`$5B` + 2 por fotograma, sonidos `$7A` 4/3); explosión del proyectil (`$1E` con `$50` ≥ $11), misil del helicóptero (`$21`) o la orilla (`$1B`) choque; el combustible se quema a $20/256 por fotograma; el movimiento lateral acelera (`$2B` += 8, fracción `$58`), fotogramas 7/8/9; velocidad `$59` ±2 entre $40 y $FE, de vuelta a $80 con la palanca centrada; la bala (misil 2, 8 filas desde `$56`, 6 filas hacia arriba por fotograma) destruye objetos de tipo ≥ 7 (`$1C`), la tabla de puntos `$BB0B` según el tipo, una vida cada 10 000 puntos (hasta 9). Un puente destruido cuenta `$61/$62`, se lleva un tanque que esté encima y desde el puente 5 hace disparar a un tanque cercano. `sub_B1C0` decide dónde empieza el avión siguiente: tras un puente destruido, desde el tramo siguiente; si no, un punto de control atrás. Las partidas a dos (`$09` impar) intercambian `$60`–`$6A` con `$6B`–`$75` (`sub_B247`) tras cada avión; el jugador 2 usa el segundo joystick (nibble alto de PORTA). START (A673) empieza una partida nueva (B0F1 con X = $29, 13 fotogramas); SELECT/OPTION cada 30 fotogramas la partida siguiente; cualquier tecla del teclado pausa (`$24`: 3 → 2 pausa → 1 → 0).

**Dibujo (comprobado mediante C++).** Una línea son 48 bytes de mapa de bits (modo ANTIC E, 4 píxeles por byte), formados por rellenos (subrutinas `AFEC`/`AFF0`/`AFF4`: `$AA`, `$FF`, `$55`) y trozos de borde de 2 bytes de la tabla en `$B800` (`B034`/`B03B`): orilla, agua, isla (con el bit 7 de `$2D`), agua, orilla. La posición del píxel dividida entre 4 es el número de byte, los 2 bits bajos eligen el trozo de borde. Una fila de decoración la copia `B01A`. Las orillas no son simétricas: el borde derecho se calcula a partir del izquierdo mediante las anchuras del agua y de la isla, por eso el río serpentea. En 2200 líneas de C++: cuando se mueven ambos bordes, se mueven igual en 810 de 944 casos; el centro del río vaga entre 71 y 122 píxeles.

## Generador en C++ (`port/`)
- Todo el procesamiento por línea (A711–AE96 y subrutinas) está portado a `port/src/river_gen.cpp`. La comprobación `port/tools/terrain_check` compara cada campo del generador y los 48 bytes de cada línea nueva de la pantalla con el original.
- Resultado: 119 758 líneas del río entero sin diferencias; un inicio desde cero (`startGame`) también coincide.
- El generador no depende ni del jugador (bot frente a vuelo sin entrada) ni de `RANDOM` (3 semillas, 120 000 fotogramas, idénticos byte a byte, franja de pantalla incluida).
- Pantalla: mapa de bits en modo ANTIC E, 48 bytes por línea (192 píxeles, 4 por byte), búfer circular, dirección de la línea nueva en `$3F04/$3F05`, −$30 por línea. Valores de píxel: 1 orilla, 3 agua, 2 borde, 0 rara vez (dentro de los trozos de borde).
- Partidas 1–8: `$09` elige una fila de las tablas `$B494`–`$B4A8`: LFSR, orilla y tramo inicial. Las partidas 1–2 empiezan en el puente 1, 3–4 en el 5, 5–6 en el 20, 7–8 en el 50. Son puntos de control del mismo río: C++ llega al puente 5/20/50 exactamente con los valores de la tabla (`$B4A4`: nivel, `$B4A8`: contador de puentes en BCD). El port construye la misma tabla haciendo correr el generador para `-seed N` (el río de la partida 1 desde el estado LFSR N; el río del cartucho es $F7D4E3 = 16241891) y para `-bridge N` (cada partida desde el puente N, 1–9999; tras el 80 el nivel va 79, 80, 79...).
- `$CB` = $2E en NTSC y $2F en PAL (A077, del registro PAL); influye en la probabilidad de un depósito al principio del bloque (AC48). El río difiere un poco entre PAL y NTSC.
- Inicio y reaparición: `sub_B0F1` carga $4D–$5F desde `$BAE1`, borra $2B–$4C y $0500–$05FF, coloca el puente inicial en la ranura 10 (tipo $0E, y=$4A, altura $17), modo $23 = $84/$80. Luego A3C8 restaura el LFSR y la orilla desde el punto de control $66–$69. Después el juego genera 160–165 líneas a 2 por fotograma (velocidad $FE) para llenar la pantalla.
- Una trampa en la verificación: mientras la pantalla se llena a 2 líneas por fotograma, el límite del fotograma del emulador puede caer entre dos líneas, así que la comparación fotograma a fotograma en esos fotogramas es errónea. En el juego normal (una línea por fotograma) la instantánea siempre cae en un límite de línea.

## Mapa del nivel
- El generador de la orilla no depende del avión: esquivar objetos mira el tipo `$050B`, que no cambia cuando un objeto es derribado. `$60` (número de tramo) crece cada 1024 líneas cuando se genera el puente, no cuando se destruye (en la partida del bot `$60` cambió en el fotograma 1133, el puente se destruyó en el 1177).
- Por eso se puede filmar el nivel entero con un avión invulnerable: `rr_oracle -ghost` parchea 5 bytes en la memoria emulada (A3E9/A424/A42E/A432 `LDX bandera` → `LDX #$FF`, A443 consumo de combustible $20 → 0). Ninguna muerte en 120 000 fotogramas.
- A velocidad normal el nivel se mueve exactamente 1 línea de pantalla por fotograma, y la línea nueva aparece en la línea 20 de la pantalla. Una franja de la línea 20 de cada fotograma coincide con la pantalla en directo salvo los objetos en movimiento.
- `$60` sube hasta $50 (80) y luego alterna $4F/$50; el estado del LFSR en los límites de tramo no se repitió en 117 tramos, así que el río no se repite, pero la dificultad deja de crecer tras el tramo 80.
- Construcción de las imágenes por tramo: `tools/level_map.py`. El visor está publicado como la página «Карта реки River Raid».

## Método
Una regla se lee primero del listado y luego se comprueba con una traza de referencia (`tools/verify_trace.py`). Si la traza no coincide, se busca el error de lectura. Ejemplo: el repostaje se dedujo del código A3E9–A453 (incluido el poco evidente +2 en vez de +1 por la bandera de acarreo) y se confirmó con la traza; la única discrepancia mostró que la comprobación de colisiones ocurre antes de que los objetos pasen de una ranura a otra.

## Versión británica en cinta
`River Raid.cas` (UK, 600 baudios): dos archivos. El de arranque (7 bloques en $0A00, inicio $0A06) comprueba que haya al menos 24K de RAM («NOT ENOUGH RAM MEMORY - NEED 24K»), muestra una pantalla de título, carga $2200 bytes en $1000 y salta a $3000. Es el código del cartucho trasladado a RAM, comparado instrucción por instrucción: a partir de A21B las más de 2900 instrucciones son las mismas operaciones; código y datos movidos de $A000 a $1000, las páginas de RAM del juego hacia arriba en $2000 (display list $3F00 → $5F00, anillo de pantalla $2000–$3FFF → $4000–$5FFF, $1000 → $3F00), las variables de página cero en su mayoría +2 ($25 → $A1, $28 → $A2); los punteros de las tablas desplazados igual. Diferencias reales:
- la inicialización (A000–A0EE) movida a $3000, con una suma de control del código cargado en $E4/$E5;
- protección anticopia: si la suma no cuadra o hay un manejador `D:` en HATABS (arranque desde DOS), el bucle principal se cuelga o salta a la mitad de una instrucción. La protección propia del cartucho (A151–A158) escribe $FF en su propio código en $A19B si $6000 no contiene $47: inofensivo en ROM, fatal en una copia en RAM;
- el rótulo deslizante dice «1982, 1983, 1984»: un carácter «4» de más (copiado a $07C0), el texto 5 caracteres más largo, inicio/fin del desplazamiento $20/$4B → $1B/$46.

Generador, lógica del juego, sonido y tablas son idénticos; también la comprobación PAL.

## Determinismo y saltos
- **El río depende solo de sí mismo.** El generador no depende ni del jugador, ni de RANDOM, ni de los objetos: rodear un objeto mira su tipo (`$050B`) y la X del objeto que aún se está dibujando, que todavía no se mueve. El LFSR da un paso por línea salvo en las aproximaciones rectas a un puente: exactamente 993 pasos de un puente al siguiente. Su periodo desde el inicio de la partida 1 es de 16 766 977 pasos, y mcd(993, 16 766 977) = 1, así que el estado en un puente vuelve solo tras 16 766 977 puentes (con la paridad del nivel, 79/80, quizá tras el doble; no se ha corrido): unos 9 años de vuelo sin parar. El contador de puentes del panel tiene 4 cifras: tras 9999 no muestra nada (0000) y sigue desde 1.
- **Nivel.** En sub $B194 (`sectionCheckpoint`) un nivel de $50 o más se toma como $4E antes de subir uno, así que tras 80 el nivel alterna 79/80. Los tramos impares y pares difieren (impares: código de anchura siempre 7, objetos en cualquier sitio de través; pares: anchura aleatoria, islas), la alternancia conserva ambos tipos; la dificultad deja de crecer.
- **Puntos de control.** En cada puente el generador guarda el LFSR y la orilla. Un inicio en el puente N (`-bridge N` en el juego, una reaparición) los restaura: el río desde ahí es el mismo que en un vuelo sin parar, los objetos no (todos recién colocados, ninguno despierto ni movido).
- **RANDOM.** Un contador de 17 bits que en cada ciclo de la máquina da un paso por un círculo fijo de 131 071 estados. El port guarda su posición en ese círculo (`src/pokey_random.h`), así que el valor en cualquier ciclo se conoce al instante. Qué enemigo despierta depende, pues, del número de fotogramas desde el encendido.
- **Los objetos viven solo mientras están en pantalla.** El generador pone un objeto arriba, y desaparece al salir por el borde inferior, unos 200 fotogramas después; igual el tanque (prepare: `tankY` ≥ $BC) y su proyectil. Por eso todo el estado en cualquier momento se sigue del río en ese momento más los últimos ~200 fotogramas. `Game::skipFrame` hace un fotograma «solo río»: el desplazamiento, los contadores de fotogramas, el temporizador del modo de demostración, el rótulo deslizante y lo que prepare() hace al estado del río (el tanque y su proyectil saliendo por debajo de la pantalla; sin eso un tanque viejo impide al generador colocar el siguiente). Después, fotogramas enteros. Comprobado contra el vuelo desde el principio (4 ríos e inicios con `-bridge`, 77 puntos cada uno, los 6000 fotogramas tras cada uno): desde 200 fotogramas enteros, cada fotograma se dibuja igual y todo el estado coincide. Las variables de un tanque, proyectil o misil de helicóptero inactivo pueden tener otros valores viejos; se vuelven a fijar antes de usarse y no se dibujan.
- **En el modo de demostración nada destruye nada.** La explosión del proyectil del tanque destruye un objeto solo en el código de vuelo del avión (A4C1), que sin avión no se ejecuta.
- **Salvapantallas.** Tras 128 × 2048 fotogramas (unos 73 minutos) sin entrada, bit 7 de $78: el bucle principal se para (A1F5) y la pantalla se pone negra, hasta cualquier joystick, tecla de consola o del teclado. El juego lo mantiene; el visor borra $78 en cada fotograma.
- **La búsqueda en el visor** (`viewer/river_view.cpp`). Anclas (el estado del río y los contadores, ~170 bytes) cada 1024 fotogramas del presente; instantáneas enteras (~16 KB) cada 128 fotogramas, solo para los últimos 8192 fotogramas antes del presente y alrededor del punto de la repetición (64). La línea L se muestra desde la instantánea más cercana o desde un ancla: solo el río, luego 400 fotogramas enteros (512 más si después se va hacia atrás). B encuentra el siguiente puente con el generador solo y vuela hasta él del mismo modo; `-bridge N` vuela desde el principio del río. Memoria: ~190 bytes por puente más ~2 MB fijos.
- **Partidas guardadas en el juego** (F7 / F8, en el mando LB / RB; `Game::save` / `load`). A diferencia del visor, aquí no se puede dejar asentar nada: el avión se cruza con los objetos, así que se guarda todo el estado, pero solo lo que no se puede rehacer. Se guardan: las variables (`GameState`) con la memoria player-missile, el estado del generador y el estado del generador antes de la línea más vieja de la memoria de pantalla con el número de líneas; la memoria de pantalla (las últimas 256 líneas del río, 12 KB) se rehace a partir de él, porque las líneas dependen solo del generador (para ello el juego recuerda el estado del generador antes de cada línea en pantalla, `keepLineStates`, unos 31 KB mientras funciona). No se guardan: los datos por línea de barrido (`scan`), que display() borra y rehace en cada fotograma, ni la imagen, que dibuja el fotograma siguiente (también en pausa: la pausa solo detiene la lógica del juego). La memoria player-missile (1,25 KB: 4 players como columnas de 8 píxeles de 256 líneas de alto, más los misiles; el chip de vídeo lee un byte por línea para cada uno) no se puede rehacer: el juego no siempre borra una forma antes de dibujarla en otro sitio, y los restos intervienen en las colisiones; pero es casi toda ceros. Todo se comprime por series de ceros (un byte cero y luego la longitud de la serie): 400–620 bytes con el contador RANDOM y el sonido de POKEY (en vez de 110 KB con la memoria de pantalla y la imagen). Comprobado: 12 partidas guardadas (vuelo a distintas velocidades, explosión, rellenado tras un choque, fin de partida, pausa), los 900 fotogramas tras cada una: imagen y sonido iguales.

## Próximos pasos
No queda nada abierto en el análisis. Para el port: la secuencia exacta de encendido (A000–A17C) y la temporización PAL (50 Hz, la display list PAL).
