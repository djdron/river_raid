# La reescritura orientada a objetos (rama `oop`)

[English](OOP.md) | [Русский](OOP.ru.md) | [Українська](OOP.uk.md) | [Deutsch](OOP.de.md) | [Polski](OOP.pl.md) | [Italiano](OOP.it.md) | Español | [中文](OOP.zh.md) | [日本語](OOP.ja.md)

Rama `oop`, desde `dli_removed`. El objetivo: el juego del port como objetos con nombre (el río,
el avión, su bala, el tanque y su proyectil, el misil del helicóptero, los barcos, helicópteros,
reactores, globos, depósitos de combustible, puentes y explosiones en sus huecos, los jugadores y
el panel, el sonido) en lugar de la RAM del original organizada por direcciones, mientras el
juego se juega exactamente como en `dli_removed`, fotograma a fotograma y bit a bit. Los ports de
ZX Spectrum siguen a `dli_removed` y se comprueban contra ella; `oop` es una reescritura del mismo
juego, no un juego nuevo.

## Las reglas

- **El mismo juego.** En cada fotograma el mismo estado, los mismos registros de sonido, la misma
  imagen que en `dli_removed`. Sin float: posiciones, velocidades, combustible y temporizadores
  siguen siendo bytes que dan la vuelta, con sus bytes de fracción; puntos, puentes y récords
  siguen en BCD. Las rarezas también se quedan: el orden de las cosas en un fotograma, los huecos
  y su orden, un objeto que se convierte en otro, cuántas veces y cuándo se lee RANDOM, los
  fotogramas del reinicio, el intercambio de los dos jugadores.
- **Comprobado después de cada paso** con `tools/trace/compare.sh` (abajo): todos los recorridos
  iguales, o el paso no está hecho.
- **Una cosa cada vez**, cada una en su propio commit: un tipo o clase nuevo primero envuelve el
  estado tal como está, luego el código se muda a él, luego el estado antiguo desaparece.
- **La RAM del original sigue accesible**: `saveToRam` / `loadFromRam` / `diffWithRam`
  (`ram_map.h`, `game_ram.cpp`) mantienen la disposición del original, para `game_check` y para
  las comprobaciones de los ports de ZX (`nxcheck`, `tscheck`), guarden lo que guarden dentro
  las clases.

## Las comprobaciones

`port/tools/game_trace` escribe una línea por fotograma de lo que hace el juego, por nombre: el
avión, la bala, el tanque, el proyectil, el misil, cada hueco, puntos, vidas, el otro jugador,
los récords, el generador del río, los registros de sonido, los resultados de las colisiones y un
hash de la imagen (río y panel). Juega con el piloto automático de `display_bench`; sin `-ghost`
el avión se estrella y se queda sin combustible, así que vidas, reapariciones y finales de partida
están en los recorridos.

`port/tools/trace/runs.txt` enumera 17 recorridos, 700 000 fotogramas: el modo de demostración,
los juegos 1–8 en NTSC y PAL, dos jugadores, `-wild` (dirección al azar, también sobre las
orillas), largos recorridos `-ghost` desde los puentes 60 y 100. `golden.txt` guarda el hash de
las líneas de cada recorrido tal como lo juega `dli_removed`, hecho en el commit de `REFERENCE`.

```
port/tools/trace/compare.sh <rom> [build dir] [runs...]
```

los juega (~2 minutos) y compara. Ante una diferencia compila el commit de `REFERENCE` en un
worktree temporal, juega allí también el recorrido e imprime el primer fotograma que difiere, las
dos líneas. `tools/fixed_check` comprueba los tipos numéricos (abajo) con cada entrada frente al
código que sustituyen.

## Hecho

1. `game_trace`, los recorridos, la referencia.
2. `PokeyRandom` sin sus ~1 MB de tablas: un salto del contador de n ciclos es lineal sobre
   GF(2), así que se compone de 17 aplicaciones (2^k pasos, 1,2 KB), y las longitudes que usa el
   juego se guardan como aplicaciones leídas de 4 en 4 bits, como hacen los ports de ZX Spectrum
   (5 consultas por lectura). Comprobado contra la clase antigua en 6 millones de lecturas.
3. La visualización solo como geometría: los modelos del DLI (`displayBeam`, `displaySimple`, los
   modos `RR_DISPLAY`, `RR_GEOMCHECK`, la clase `Beam`, los interruptores de los experimentos) se
   han quitado de esta rama; siguen en `dli_removal`. `display.cpp`: 276 líneas en lugar de 1331.
4. `src/fixed.h`: `Fixed8` (un byte y su byte de fracción, suma / resta con acarreo), `Bcd<N>`
   (contadores BCD: suma con el acarreo hacia arriba, la comparación del original), `addBcd`.
5. La bala (`bullet.h`) y el avión (`plane.h`: dirección, velocidad, combustible) como tipos
   propios.
6. Sin memoria de player-missile: las filas de los sprites salen de su estado (`Game::spriteRow`:
   cada forma de la ROM en su fila, el misil una fila, la bala 8), mostradas desde el principio de
   cada fotograma hasta que un reinicio las borra. Que el original comparta esa memoria (las filas
   de la bala borran las del misil) lo deshace la copia del fotograma siguiente antes de que nadie
   la lea, así que el juego es el mismo. `saveToRam` sigue escribiendo la memoria como la tiene el
   original.
7. El tanque, su proyectil y el misil del helicóptero (`tank.h`, `tank.cpp`): una estructura cada
   uno, guardados por el río (se desplazan con él; desde el paso 13 por `Objects`), con sus
   movimientos (`Tank::drive`, `Shell::move`, `Missile::move`).
8. Los objetos de los huecos (`objects.h`, `objects.cpp`): una clase por cada tipo de
   comportamiento (`Explosion`, `Jet`, `Helicopter`, `Balloon`, `Ship`, `Bridge`, `FuelDepot`).
   Un hueco guarda los bytes del original; su tipo es también su fotograma de animación y cambia
   cuando recibe un impacto, así que `objectKind(kind)` da en cada fotograma el objeto de la clase
   que sabe lo que hace ese tipo: su animación, su movimiento (el reactor a través de la pantalla;
   helicópteros, globos y barcos de un lado a otro una vez despiertos, dando la vuelta en el
   borde), si la visualización lo comprueba contra el borde.
9. Los jugadores (`players.h`): `PlayerData` (el otro jugador) con puentes `Bcd<2>` y puntos
   `Bcd<3>`, los mismos tipos para el jugador activo, `HighScores` (la tabla y sus bytes de
   control); el panel (`panel.h`): sus 60 caracteres con `print` (dígitos BCD), `showLives`,
   `markPlayers`, `showGameNumber`.
10. El sonido (`sound.h`, `sound.cpp`): `Sounds` guarda lo que pide el juego (el pitido de inicio,
    el sonido de evento y su duración, explosión, disparo, bonificación, misil, el tono que baja
    tras un choque) y su `drive()` es el controlador (B2A5–B3D8), al que el avión, la pausa, el
    modo de demostración y el proyectil se le dan mediante un `SoundContext`; `PokeyRegs` también
    se mudó allí.
11. El río en la pantalla (`river.h`, `river.cpp`): sus líneas, las mismas sin decoración, sus
    tramos, la dirección de la línea superior en la memoria de pantalla del original y los estados
    del generador para partidas guardadas pequeñas; `push()` añade la siguiente línea del
    generador, `rebuild()` las rehace al cargar un estado.
12. Sin `GameState`: sus campos son los miembros del propio `Game` (`eachState()` los enumera para
    guardar, cargar y reiniciar).
13. El río aparte de los objetos (`world.h`, `world.cpp`): `RiverGen` hace solo el río
    (`RiverState`: orillas, islas, secciones, el LFSR); los huecos, el tanque, su proyectil y el
    misil son `Objects`, que coloca los objetos nuevos después de cada línea (`afterLine`,
    $ABE1–$AE93) y le dice al generador dónde está el objeto que está entrando (`BankObstacle`),
    para que la orilla lo esquive. `World` contiene ambos y los hace avanzar línea a línea; su
    `State` es lo que guardan las partidas guardadas y los estados de las líneas del río.
14. La exportación a RAM desde las clases (`ram_map.h`, `ram_map.cpp`): cada clase enumera sus
    bytes con sus direcciones en su `mapRam(RamMap &)` (el avión, la bala, los sonidos, las
    colisiones, los jugadores, los récords, el panel, la línea de desplazamiento, el río, los
    huecos, el tanque, su proyectil y el misil, la línea superior de la pantalla); `World` y
    `Game` juntan las listas, y `RamMap` las carga, las escribe y las compara. Lo que se calcula en
    vez de guardarse (la memoria de player-missile, las líneas de pantalla según la lista de
    visualización) se queda en `game_ram.cpp` y `River`.
15. Los documentos: las tablas de archivos de los README y sus partes de código, generador y
    verificación, HANDOFF (la rama), NOTES (partidas guardadas), HOW_IT_WORKS (las colisiones), en
    todos los idiomas; los comentarios del código fuente que aún hablaban del DLI o de que el río
    guarda los objetos.
16. El final: los 17 recorridos iguales (700 000 fotogramas), también en una compilación con
    AddressSanitizer, UndefinedBehaviorSanitizer y las aserciones de la biblioteca estándar, sin
    ningún aviso; este documento en todos los idiomas.

## Lo siguiente

Nada previsto. Un cambio a partir de aquí se comprueba igual: `compare.sh -quick` después de cada
paso, los 17 recorridos al final.
