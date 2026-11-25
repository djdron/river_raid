# Experimentos: el juego sin el modelo del DLI

[English](DLI_REMOVAL.md) | [Русский](DLI_REMOVAL.ru.md) | [Українська](DLI_REMOVAL.uk.md) | [Deutsch](DLI_REMOVAL.de.md) | [Polski](DLI_REMOVAL.pl.md) | [Italiano](DLI_REMOVAL.it.md) | Español | [中文](DLI_REMOVAL.zh.md) | [日本語](DLI_REMOVAL.ja.md)

Ramas `dli_removal` y `dli_removed`. El objetivo es una visualización más sencilla para los ports
a máquinas lentas (ZX Spectrum Next, TS-Conf): sprites dibujados como imágenes listas en sus
posiciones, colisiones calculadas por objeto, sin un modelo línea a línea de la interrupción de la
display list (DLI). En `dli_removal` el juego sigue siendo el mismo que el original fotograma a
fotograma, y cada paso se comprueba frente al original (`rr_oracle` sobre libatari800,
`game_check`). `dli_removed` va un paso más allá: nada de DLI, las colisiones como geometría
simple, que difiere del original en unos pocos fotogramas de cada mil (la última parte, «Rama
`dli_removed`»). Los ports para ZX ([river_raid_zx](https://github.com/djdron/river_raid_zx))
siguen `dli_removed`.

## Paso 1: colisiones con la orilla en líneas sin decoración

El DLI lee las colisiones con el playfield de: el avión (P2PF, solo el color de la orilla), los
objetos (P3PF, solo el color del borde: barcos y helicópteros giran en él), el tanque (P1PF, el
borde), la bala y el misil del helicóptero (M2PF, M0PF: orilla o borde). Las casas y carreteras
(la decoración, `decoRow`) se dibujan en el color del borde y en el color de fondo dentro de la
orilla.

`RiverGen::nextLine(line, bank)` da una segunda línea, la misma sin decoración (donde está
dibujada, el relleno de la orilla); `Game::bankLines` las guarda junto a `screen`, y las colisiones
las miran a ellas (`bankRuns`, activado por defecto; `RR_BANK_RUNS=0` la pantalla como antes). Sin
decoración cada línea del río tiene una de cuatro formas (480 000 líneas de las partidas 1–8):

| forma (tramos de color) | líneas | límites |
|---|---|---|
| orilla, borde, agua, borde, orilla | 398 644 | 4 |
| lo mismo con una isla | 80 324 | 8 |
| con una isla estrecha (solo color del borde) | 1 016 | 6 |
| las líneas de un puente alcanzado (la orilla en el color del borde) | 16 | 2 |

Así que una línea se describe con 2–8 números.

Comprobado con `game_check` frente a ejecuciones del original: el modo de demostración (19 900
fotogramas), una partida del bot (8 950), una partida del bot en PAL (5 974), la partida 7 desde el
puente 50 en NTSC (2 ejecuciones, hasta el fin de partida) y PAL (8 950 fotogramas): ninguna
diferencia, salvo un bit que nadie lee. Los cazas vuelan sobre la tierra y tocan la decoración: el
bit «toca el borde» de su slot difiere (en el modo de demostración en 341 fotogramas), pero
moveObjects nunca lo lee para un caza. `game_check` no compara ese bit para los cazas cuando
`bankRuns` está activado. `RR_BANK_DEBUG=1` imprime cada línea en que los dos tipos de línea dan
colisiones distintas (todos eran cazas, tipo 7, player 3).

Ejecuciones de la partida 7 desde el encendido: `oracle/scripts/start_game7.txt` con `-bot`. En
NTSC la comparación se detiene en el primer fin de partida, que el port no modela (README).

## Paso 2: colisiones por extensiones

`collideSpans` (el predeterminado; `RR_COLLIDE=masks` para las máscaras de píxeles de toda la
línea como antes): cada sprite en la línea de barrido son sus bits de gráfico y los ciclos de color
desde su primer bit encendido hasta el último. Frente al playfield se comprueba tramo a tramo (la
línea sin decoración, 5–9 tramos de un color); un tramo que llega más allá del primer o el último
bit encendido es un contacto, uno estrictamente entre ellos se mira bit a bit. Dos sprites: primero
sus extensiones, bit a bit solo donde se encuentran. Todavía por línea de barrido, dentro del haz:
qué contiene cada línea y cuándo lee el DLI los registros es lo del haz.

Las mismas ejecuciones de arriba: ninguna diferencia con el original. `RR_STATS=1` cuenta los
casos; `RR_ROUGH_BANK=1` y `RR_ROUGH_PAIRS=1` quitan el paso bit a bit (un contacto dondequiera que
las extensiones se encuentren), para ver si hace falta:

| ejecución | líneas con sprites | un tramo entre el primer y el último bit (en un hueco) | extensiones encontradas (no los bits) |
|---|---|---|---|
| modo de demostración | 1 164 849 | 86 228 (0) | 26 862 (70) |
| bot PAL | 278 257 | 9 201 (96) | 4 108 (0) |
| partida 7, PAL | 536 802 | 24 494 (96) | 7 693 (0) |

- La orilla: las extensiones bastan. Con `RR_ROUGH_BANK` todas las ejecuciones siguen idénticas al
  original; los tramos que cayeron en un hueco nunca cambiaron un bit que lea el DLI.
- Los sprites entre sí: los bits hacen falta. Con `RR_ROUGH_PAIRS` la bala y el proyectil del
  tanque «alcanzan» objetos a través de sus huecos: el modo de demostración difiere en 3
  fotogramas, la ejecución del bot en NTSC en 712 (un objeto destruido junto al que había pasado la
  bala del original), la partida 7 en 2–4. Las extensiones de dos sprites se encuentran poco (unos
  miles de líneas por ejecución), así que el paso bit a bit cuesta poco.

## Paso 3: la visualización sin los ciclos del DLI

`Game::displaySimple` (`port/src/display.cpp`) es ahora el predeterminado; `RR_DISPLAY=beam`
ejecuta el modelo ciclo a ciclo como antes, `RR_DISPLAY=check` ejecuta ambos desde el mismo estado
en cada fotograma e imprime cualquier diferencia en los resultados de las colisiones, los
indicadores de los objetos, la memoria de player-missile o lo que se dibuja en cada línea de
barrido.

Sigue el bucle del DLI (B565–B664) en líneas de barrido, no en ciclos. El registro X del 6502
cuenta líneas: a cada STA WSYNC le sigue un INX, así que mientras X = c se dibuja la línea c − 69.
Para cada slot:

| qué | línea de barrido (S: X cuando termina la espera del objeto, E: X tras su última fila) |
|---|---|
| HPOSP3 = x | desde S − 68 |
| dos filas vacías en la memoria del player 3 | S − 67, S − 66 |
| SIZEP3 | desde S − 66 |
| fila de forma k (desde la de arriba hacia abajo), su COLPM3, el color de la carretera del puente | S − 65 + k |
| el color de la orilla de vuelta (puente) | desde E − 68 |
| lectura de M2PL (bala – objeto) | ve hasta E − 70 (un puente: E − 69) |
| lectura de P3PL (objeto – proyectil), P2PL (avión – proyectil, objeto) | hasta E − 69 |
| lectura de P3PF (el bit «toca el borde» del objeto) | hasta E − 68 |
| lectura de M0PL (misil – avión), M0PF (misil – orilla) | hasta E − 68 (M0PF: ver abajo) |
| lectura de P1PF (tanque), P2PF (avión – orilla), M2PF (bala – orilla) | hasta E − 67 |
| HITCLR | las líneas desde E − 66 cuentan para el objeto siguiente (ver abajo) |

Un objeto cuyas filas están todas por encima de la línea en que termina su espera (empezó
demasiado cerca bajo el anterior) se salta sin lecturas y sin HITCLR. Cuando X da la vuelta a 0 el
río termina; la parte del panel lee las colisiones del objeto en curso hasta las líneas 188–190.

Dónde está la CPU tras un WSYNC (ciclo 106) lo fija el código, así que cada acceso cae en un lado
conocido del dibujo de una línea (ciclo 28) salvo dos, decididos por los ciclos que toma el inicio
de la línea siguiente (DMA, una instrucción de la display list con o sin dirección) y por los
contactos guardados justo antes:

- M0PF (B631) se lee 28 ciclos tras el WSYNC, 30 cuando M0PL fue un contacto: después del dibujo en
  una línea cuyo inicio toma 8 ciclos (con dirección), así que ve una línea más. En la ejecución
  del bot en NTSC: 1 499 veces de 68 365.
- HITCLR (B65F) llega 23 ciclos tras el WSYNC, 2 más por cada uno de los tres contactos guardados
  antes: con los tres, en una línea con dirección, llega tras el dibujo y las colisiones de esa
  línea se pierden. Nunca visto en las ejecuciones (0 de 285 623).

Colisiones: la caja de cada sprite (las líneas en las que tiene gráfico y los ciclos que cubren;
el player 3 una caja por objeto, cortada donde cambia su posición o tamaño). El playfield: cada
fila del player 1 (tanque), 2 (avión), 3 (objetos) y de los misiles 0 y 2 frente a los tramos de
su línea (paso 2). Los sprites entre sí: solo los pares que lee el DLI (bala – objeto, objeto –
proyectil, avión – proyectil, avión – objeto, misil – avión) cuyas cajas se encuentran, bit a bit
en las líneas que comparten. Lo que ve una lectura es el OR de las líneas desde el último HITCLR.

Comprobado: `RR_DISPLAY=check` en todas las ejecuciones de arriba (modo de demostración, bots NTSC y
PAL, partida 7 en NTSC y PAL, dos jugadores hasta el fotograma 2900): ninguna diferencia con el haz
en ningún fotograma. `RR_DISPLAY=simple` frente al original (`game_check`): los mismos resultados
que el haz (ninguna diferencia, partida 7 en NTSC hasta su fin de partida). La zona del río dibujada
con él frente a las pantallas del original (`-screens`, 1289 fotogramas del modo de demostración,
1272 de la partida 7 en PAL desde el puente 50): idéntica.

### Velocidad (C++)

`RR_TIME=1` imprime el tiempo que tarda `display()` por fotograma. Compilación Release, el mejor de
5 ejecuciones, y las instrucciones contadas por callgrind (partida 7 en PAL, 2000 fotogramas):

| | modo de demostración | partida 7, PAL (desde el puente 50) | instrucciones por fotograma |
|---|---|---|---|
| el haz (`RR_DISPLAY=beam`) | 8,5 µs | 10,1 µs | 101 500 |
| displaySimple, cajas buscadas en la memoria de player-missile (`RR_BOXES=scan`) | 4,0 µs | 4,0 µs | 40 600 |
| displaySimple, cajas a partir de las posiciones del juego | 3,1 µs | 3,7 µs | 34 200 |

Unas tres veces menos instrucciones, 2,5–3 veces más rápido. Ambos miran los tramos del playfield
hechos una vez al entrar cada línea (`Game::runLines`, `makeRuns`), no los píxeles de la línea. Del
tiempo del haz, dos tercios iban a las colisiones que calcula en cada línea, un tercio a sus ciclos
(`settle`). Las cajas de los sprites salen de donde los puso el juego (el predeterminado;
`RR_BOXES=scan` busca hacia abajo en la memoria de player-missile como antes): el proyectil 16
filas desde `shellY`, el tanque 11 desde `tankY`, el avión 14 desde la fila $AA, el misil del
helicóptero la fila `missileY`, la bala 8 filas bajo `bulletY` (desde la fila $1C), las filas de
cada objeto como las escribió el bucle del DLI; los bits de cada línea de una caja se siguen leyendo
de la memoria de player-missile. Igual que el haz en cada fotograma de cada ejecución: nada fuera de
estas cajas (filas que quedaron en la memoria del player 3 de fotogramas anteriores, por ejemplo)
produjo nunca una colisión que lea el DLI.

### Ejecuciones largas: `display_bench`

`port/build/display_bench <rom> <frames> [-pal] [-game N] [-seed N]` juega con un piloto
automático (dirige al centro del agua que tiene delante y dispara sin parar; el avión no puede
estrellarse y nunca gasta combustible, así que una partida dura toda la ejecución: 100 000
fotogramas llegan al puente 98 en la partida 1, al puente 147 en la partida 7). Con
`RR_DISPLAY=check` compara los dos modelos en cada fotograma, con `RR_TIME=1` mide `display()`.

Estas ejecuciones encontraron lo que las más cortas nunca mostraron: el DLI a veces se retrasa una
línea. De un WSYNC al siguiente el código debe caber antes del ciclo 106 de la línea siguiente,
descontando el inicio y el dibujo de esa línea (en una línea del río 6 u 8 + 57 ciclos); tres
tramos pueden ser más largos:

- B5B0–B5D1 (hasta la primera fila): 49 ciclos, 50 cuando el byte de forma de la primera fila está
  en la página siguiente: demasiado largo en una línea con dirección de pantalla.
- la última fila de un puente hasta B5F3: 46 ciclos, +1 por cada lectura de tabla que cruza una
  página, +2 por un impacto de la bala.
- B5F3–B61F: 44 ciclos (puente 43), +2 por cada uno de los contactos guardados ahí (proyectil, avión
  con proyectil, avión con objeto).

Cuando es demasiado largo, el STA WSYNC espera al final de la línea siguiente: todo lo que viene
después en ese fotograma pasa una línea más tarde (X = línea + 68), incluidos el fin del río y las
lecturas del panel, y una fila del player 3 escrita en la línea a la que va o después aparece allí
solo en el fotograma siguiente (GTIA ya tomó la vieja). displaySimple cuenta estos saltos
(`RR_STATS`): 0–10 en 100 000 fotogramas, todos con el avión chocando con cosas contra las que se
habría estrellado; en una partida real, rara vez. Con ellos modelados, `RR_DISPLAY=check` no da
diferencias en 6 ejecuciones de 100 000 fotogramas (partidas 1, 2, 3, 5, 7, 8, NTSC y PAL).

Tiempo de `display()`, el mejor de 3, e instrucciones por fotograma (callgrind, 5000 fotogramas):

| | partida 1, NTSC, 50 000 / 100 000 fotogramas | partida 7, PAL, 50 000 / 100 000 fotogramas | instrucciones (partida 1 / partida 7) |
|---|---|---|---|
| el haz | 10,8 / 11,1 µs | 10,5 / 9,5 µs | 120 000 / 129 000 |
| displaySimple | 5,1 / 4,8 µs | 4,8 / 5,0 µs | 65 000 / 69 000 |

Unas dos veces más rápido. Menos que en las ejecuciones del original de arriba (tres veces) porque
aquí el avión atraviesa objetos y orillas sin estrellarse: muchas más colisiones por fotograma que
en una partida real.

Más largas aún: 12 ejecuciones de 300 000 fotogramas (3,6 millones), `-bridge 100` (la partida
empieza en el puente 100; hasta el puente 392), NTSC y PAL, partidas 1, 2 (dos jugadores) y 7, y con
`-wild` (el piloto automático gira al azar, también sobre las orillas: hasta los puentes 70–159).
Ninguna diferencia en ningún fotograma. Líneas saltadas: 0–8 por ejecución. El HITCLR tardío (los
tres contactos guardados antes de él, en una línea con dirección de pantalla) nunca ocurrió, ni en
estas ejecuciones ni en otras: existe solo por los ciclos del código, no se ha visto en acción.

### Solo los objetos que giran en el borde

El bit «toca el borde» (P3PF) moveObjects lo lee solo para helicópteros ($08–$0B), globos ($0C) y
barcos ($0D): giran al primer contacto. Los cazas vuelan recto, las explosiones están quietas, los
puentes y depósitos no se mueven. `displaySimple` comprueba frente al borde solo las filas de esos
tipos (`noEdge` marca las líneas de los demás); el bit de los demás queda a cero, y `game_check` no
lo compara para ellos (`bankRuns`). Comprobado frente al original: el modo de demostración (NTSC
2 262 y 5 961 fotogramas, PAL 2 975), partidas del bot en NTSC (hasta su primer fin de partida, que
el port no modela en NTSC), en PAL (5 975 y dos jugadores 7 775) y la partida 7 desde el puente 50
en PAL (5 975): ninguna diferencia.

### El avión frente a un objeto: por cajas y bits

El bit de P2PL para el player 3 se lee tras las filas de un objeto (B60A) o al final del río y ve
las líneas desde el HITCLR anterior: solo las filas de ese objeto, en su posición y tamaño. Así que
«el avión tocó el objeto» se calcula sin las líneas de barrido: las 14 filas del avión desde $AA
frente a las filas del objeto donde las escribió el bucle, bit a bit (`planeTouches`), en los
mismos dos sitios (los ciclos de la escritura cuentan como antes); el par P2–P3 desapareció de las
colisiones de las líneas. Medido primero junto al valor del DLI (`RR_GEO_CHECK`, ya eliminado):
igual en cada fotograma de las ejecuciones del original de arriba (~47 000 fotogramas, 420 con el
contacto) y de `display_bench` (200 000 fotogramas cada una con `-wild` en NTSC y partida 7 en PAL,
300 000 desde el puente 100 en NTSC y en PAL con `-wild`: 61 382 contactos). Después `game_check`
frente al original: ninguna diferencia; `RR_DISPLAY=check` (60 000 fotogramas `-wild`, 60 000 desde
el puente 100 en PAL): nada salvo el bit del borde de los tipos que no giran (arriba).

### Todas las colisiones sin los registros de las líneas

El resto de las lecturas va igual. Las filas de cada sprite están donde las puso el juego (las
cajas de arriba: nada de ellas está en otro sitio), así que las líneas en que ocurre cada colisión
que lee el DLI se calculan una vez por fotograma: las filas del tanque frente al borde (P1PF), las
del avión frente a la orilla (P2PF), las del misil y la bala frente a cualquiera de los dos (M0PF,
M2PF), el avión frente al proyectil (P2PL) y el misil frente al avión (M0PL), bit a bit. Las
colisiones de un objeto se calculan en sus lecturas, frente a sus filas donde las escribió el
bucle: la bala (M2PL, hasta la línea anterior a la última fila, incluidas las de un puente), el
proyectil (P3PL), el avión (P2PL), el borde (P3PF, solo los tipos que giran). Una lectura cuenta una
colisión si su línea está entre la del último HITCLR y la suya, como hacían los registros. El bucle
del DLI, sus ciclos y las líneas de HITCLR se quedan (deciden esas ventanas); los registros de las
líneas (`compute`, `seen`, las cajas por línea) ya no están. Un caso difiere: cuando el DLI se
retrasa una línea, las filas escritas tarde no se toman y se ven las viejas; las colisiones son
ahora con las filas nuevas. Eso no pasó en ninguna de las ejecuciones de aquí. Comprobado:
`game_check` frente al original (todas las ejecuciones de arriba): ninguna diferencia;
`RR_DISPLAY=check` (60 000 fotogramas `-wild`, 60 000 desde el puente 100 en PAL, 300 000 desde el
puente 100 en NTSC y 300 000 en PAL con `-wild`): nada salvo el bit del borde de los tipos que no
giran.

### Los objetos dibujados a partir de sus filas

Las filas del player 3 no las lee nadie salvo GTIA: el juego nunca. Así que `displaySimple` ya no
las escribe, ni los registros del player 3 por línea (HPOSP3, SIZEP3, los colores, las dos filas
vacías sobre cada objeto). Lo que deja para dibujar es `drawn`: de cada objeto la primera línea de
barrido, posición, tamaño, filas y sus colores, como los escribe el bucle, y el color de la orilla
por línea (`bankColour` de `scan`; sus campos del player 3 quedan a 0); `drawRiver` dibuja los
objetos a partir de ello. El bucle mismo se queda como horario: qué filas muestra cada objeto, dónde
cae cada lectura y HITCLR, los ciclos. `diffWithRam` no compara las filas del player 3
($0F00–$0FBF) con `simpleDisplay`; `RR_DISPLAY=check` compara las dos imágenes en lugar de las
líneas. Cuando el DLI se retrasa, el original muestra las filas del fotograma anterior donde van las
tardías; aquí se dibujan las nuevas (en ninguna ejecución). Comprobado: `game_check` frente al
estado del original (todas las ejecuciones de arriba): ninguna diferencia; frente a sus pantallas
(`-screens`, 1300 fotogramas del modo de demostración, 1500 de la partida 7 en PAL desde el puente
50): la zona del río idéntica; `RR_DISPLAY=check` (60 000 fotogramas `-wild`, 60 000 desde el puente
100 en PAL): las imágenes iguales, nada salvo el bit del borde de los tipos que no giran.

### RR_GEOMCHECK: geometría simple junto a las lecturas

Antes de seguir, una medida: con `RR_GEOMCHECK`, `displaySimple` calcula también las colisiones de
cada fotograma sin las lecturas del DLI, sin ventana de un HITCLR a una lectura y sin ciclos. Un
objeto está alcanzado si sus filas tocan en algún punto las del sprite (el último objeto así en el
orden del bucle, porque las escrituras se sobrescriben); las demás colisiones cuentan si ocurren en
cualquier punto de las filas de los sprites; el bit del borde de un objeto sale de todas sus filas.
Los resultados de las lecturas se quedan; los fotogramas en que la geometría decide otra cosa se
cuentan por campo y se imprimen al salir. `RR_GEOMCHECK=2` imprime además cada uno de esos
fotogramas: su número, qué difiere, dónde están la bala, el avión y el tanque y las filas de cada
objeto.

El resultado: difieren el 0,01–0,25% de los fotogramas, según la ejecución, y siempre de la misma
manera: un contacto que las lecturas ven un fotograma después o no ven. La bala en la última fila
de un objeto (M2PL se lee antes de que esa fila se dibuje), las filas del avión o del tanque por
debajo de la última lectura del fotograma. Nada de lo que ve la geometría es un contacto que no
ocurriera en la pantalla.

## Rama `dli_removed`: `displayGeometry`

En esta rama `Game::displayGeometry` (`port/src/display.cpp`) es el predeterminado
(`RR_DISPLAY=geometry`); `RR_DISPLAY=simple`, `beam` y `check` siguen como arriba. No hay DLI en
él: ni bucle sobre los slots por líneas de barrido, ni ciclos, ni HITCLR, ni lecturas. Es lo que
hacen los ports para ZX, en C++, y la referencia con la que se comparan fotograma a fotograma.

### Las colisiones como geometría simple

Cada colisión cuenta donde las filas de los sprites y de los objetos se tocan en algún punto,
primero las cajas, luego los puntos (la regla de RR_GEOMCHECK de arriba). Un objeto alcanzado es el
último así bajando por la pantalla, porque las escrituras del original se sobrescriben; el
proyectil no se comprueba frente a un puente. Las colisiones con la orilla y el borde, avión–
proyectil y misil–avión guardan el slot 0 cuando ocurren en cualquier punto de las filas de los
sprites. Las filas de los sprites son las cajas a partir de las posiciones del juego (paso 3: el
proyectil 16 filas desde `shellY`, el tanque 11 desde `tankY`, el avión 14 desde $AA, el misil la
fila `missileY`, la bala 8 filas bajo `bulletY`).

Así que el juego ya no es el del original en cada fotograma: en los fotogramas que cuenta
RR_GEOMCHECK (0,01–0,25%) una bala alcanza un objeto o un avión la orilla un fotograma antes, o
cuenta un contacto que el original no vio. `game_check` frente al original es para
`RR_DISPLAY=simple`; con el predeterminado informa de esos fotogramas.

### Cada objeto en la y de su slot

Los objetos se colocan directamente desde sus slots, sin el recorrido del contador del DLI: filas
desde la línea de barrido y − 65 hacia abajo, donde las dibujaba el DLI (primero la fila de abajo
de la tabla de formas, porque las tablas las guardan de abajo arriba), cortadas en lo alto del río
(línea 28: la primera espera del DLI era X = $5D) y en su final (X más allá de 255). Los colores de
la carretera del puente salen de esas filas, tras la última el color de la orilla de la sección.
Comprobado en 120 000 fotogramas (`display_bench` desde el inicio, desde el puente 60 en PAL,
`-wild`, partida 7) frente al recorrido del contador que sustituyó: los mismos objetos, filas y
colores en cada fotograma. El bucle nunca se retrasó respecto a un objeto, así que solo los cortan
lo alto y el final del río.

### El borde por el primer y el último punto de una fila

El bit del borde de un objeto (P3PF: helicópteros, globos y barcos giran en el borde) sale de cada
una de sus filas como extensión, de su primer punto al último, frente a los tramos de píxeles del
borde de la línea (`Beam::edgeTouches`), sin mirar punto a punto: un tramo dentro de un hueco de
una fila cuenta, a diferencia de la comprobación bit a bit del original en GTIA. El Next lo
comprueba así: por línea los tramos del borde, por fila del objeto sus dos extremos. En 20 000
fotogramas de partidas del bot (más de 100 contactos) el resultado nunca fue distinto de la
comprobación punto a punto.

### Los demás sprites frente a la orilla y el borde igual

`Beam::spanTouches`: el primer y el último punto de una fila frente a los tramos de los colores
pedidos de la línea (orilla, borde o cualquiera); `edgeTouches` es ella con el borde. El tanque
frente al borde, el avión frente a la orilla, el misil y la bala frente a cualquiera (`pfAny`)
también la usan, así que cada colisión con el playfield es una sola prueba: los dos extremos de la
fila frente a los tramos de la línea. Los sprites entre sí (bala, proyectil y avión frente a un
objeto, avión y proyectil, misil y avión) siguen bit a bit: el paso 2 mostró que ahí los huecos
importan.

### El color de la orilla por línea

El original pone el color de la orilla (COLPF0) por scanline: arriba el verde del VBI, en las
filas del slot del puente los colores de la carretera, debajo el verde de la sección. Aquí cada
línea guarda el color con el que entró (`lineBankColour`; `Game::lineBank` junto a `screen`,
`drawRiver` pinta cada línea con él): el color de carretera de la fila del puente que la línea
muestra, si no `bankColour()`. La fila del puente es y + altura − $5E, una línea más vieja que
al entrar la línea: la visualización va antes de la línea nueva del frame, así que a una línea
por frame el original muestra ahí justo esa fila del puente. El verde de la sección nueva empieza
justo encima de la carretera, 11 líneas antes de que el generador cambie de nivel:
`bankColour()` se invierte mientras el puente entra. Comprobado con `display_bench` (200 000
frames cada uno: el bot, `-wild`, con choques y reinicios, PAL, juegos 3 y 7): a una línea por
frame cada línea tiene el color del original, los dos verdes y la carretera. La diferencia:
cuando el río avanza 2 líneas en un frame, o ninguna, la carretera del original se movía una
línea por las líneas de la pantalla y volvía (las líneas de sus extremos cambiaban de color;
tras un reinicio la carretera abajo de la pantalla parpadeaba); aquí se queda con sus líneas, y
el sprite del puente se mueve respecto a ellas como todos los objetos. Así un port de ZX no
necesita cambiar de color por línea: una paleta con los dos verdes y los colores de la
carretera, cada línea dibujada con el suyo.

### Cuánto cuesta y para qué

La visualización son ahora tres bucles: las cajas de los sprites frente a los tramos de sus
líneas, los pares de sprites donde sus cajas se encuentran, los objetos colocados desde sus slots y
comprobados frente a la bala, el proyectil y el avión. No se recorre ninguna línea de barrido
vacía. En el Next el mismo trabajo (`display_simple.asm`, `display_hits.asm` en river_raid_zx)
toma 45 000–62 000 de sus 567 000 ciclos por fotograma a 28 MHz; sus CODE.md y NOTES.md tienen las
cifras rutina a rutina.
