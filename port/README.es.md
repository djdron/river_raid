# River Raid, port a C++

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Polski](README.pl.md) | [Italiano](README.it.md) | Español | [中文](README.zh.md) | [日本語](README.ja.md)

Un proyecto no oficial de aficionados con fines educativos, sin relación con Activision ni Microsoft; no incluye ninguna parte del juego (vea el aviso en `../README.es.md`).

El juego completo: `river_raid` juega a River Raid como el original — modo de demostración, selección de partida, uno y dos jugadores, el avión, el combustible, los enemigos, los puentes, los puntos, las vidas, los récords, la pausa y el sonido. El generador del río y la lógica del juego producen fotograma a fotograma el mismo estado de la RAM que el original, y el dibujo, la misma pantalla (vea Verificación).

## Compilación
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
El núcleo y las herramientas solo necesitan un compilador de C++17. El juego `river_raid` y el visor `river_view` necesitan SDL3: CMake usa un SDL3 instalado (3.4 o posterior) si lo encuentra; si no, durante la configuración descarga SDL 3.4.20 de GitHub (necesita git e internet) y lo enlaza estáticamente. En Linux, compilar SDL desde el código fuente necesita los paquetes de desarrollo de X11/Wayland (vea `docs/README-linux.md` de SDL). SDL 3.4 se detiene con un error cuando falta uno de esos paquetes y nombra la opción que compila sin él (por ejemplo `-DSDL_X11_XCURSOR=OFF`). Sin SDL en absoluto: `cmake -S . -B build -DRR_VIEWER=OFF`.

Con SDL2 en lugar de SDL3 (para plataformas que SDL3 ya no admite, como UWP en la Xbox): `cmake -S . -B build -DRR_SDL2=ON`. CMake usa entonces un SDL2 instalado (2.0.18 o posterior) o descarga SDL 2.32.10. Los programas hablan con SDL solo a través de `sdl/host.h`, implementado una vez para cada versión; ambas dan la misma imagen.

En Windows con Visual Studio: `cmake --build build --config Release`, los programas quedan en `build\Release\`. Compilado y probado en Windows (MSVC), macOS (Xcode, Apple M2) y Linux (gcc).

## El juego
```
river_raid [rom] [opciones]
```
Sin argumentos encuentra la ROM por sí mismo (vea `rom`) y arranca como el cartucho: modo de demostración y luego START.

| Opción | Qué hace |
|---|---|
| `rom` | la imagen del cartucho (8192 bytes). Por defecto se busca `rom/River_Raid.rom` en la carpeta actual y hasta tres carpetas por encima, así arranca sin argumentos desde el repositorio o desde `port/build/Release` |
| `-os archivo` | letras y cifras del propio sistema operativo del Atari: una imagen de la ROM del sistema (10 KB o 16 KB) o una fuente de 1 KB. Sin ella se usa la fuente integrada de AltirraOS |
| `-seed N` | otro río, con otros enemigos y depósitos. N es el estado desde el que empieza el generador del río, de 1 a 16777215. La partida 1 empieza al principio de ese río, las partidas 3–8 en sus puentes 5, 20 y 50; cada partida mantiene su nivel habitual, así que la dificultad crece como siempre. El río propio del cartucho es `-seed 16241891` |
| `-bridge N` | cada partida (1–8) empieza en el puente N, de 1 a 9999: con N en el contador de puentes y el nivel de ese puente (después del 80 el nivel alterna 79/80, como en el original). También funciona con `-seed`. El inicio se encuentra haciendo correr el generador del río hasta él (menos de un segundo incluso para 9999) |
| `-random N` | el valor inicial del contador RANDOM de POKEY, que decide qué enemigo despierta y cuándo, y el ruido de las explosiones. Por defecto se toma del reloj, igual que el de la máquina real depende del momento de encenderla, así que cada ejecución es distinta; con el mismo N y la misma entrada una ejecución se repite exactamente |
| `-pal` | jugar como una máquina PAL: como en un Atari PAL real, los depósitos de combustible se vuelven más escasos hasta el nivel 47 en lugar del 46; colores PAL. La temporización de la pantalla sigue siendo NTSC (vea Pendiente) |
| `-colours ntsc\|pal` | los colores de una máquina NTSC o PAL; PAL por defecto. C los cambia durante el juego |
| `-palette altirra\|atari800` | el modelo de color: `altirra` (por defecto) o `atari800` (tonos medios más oscuros). V lo cambia durante el juego |
| `-border` | muestra el fotograma completo con su borde negro (384×240 menos 10 columnas a cada lado), como hacen los emuladores. Por defecto la ventana muestra solo la imagen del juego con todo el rótulo deslizante, 364×200, así que es un 20% más grande. O lo cambia durante el juego |
| `-filter sharp\|nearest\|linear` | cómo se escala la imagen a la ventana o la pantalla: `sharp` (por defecto) escala por números enteros con píxeles nítidos y después suavemente hasta el tamaño final, así sigue nítida y todos los píxeles tienen el mismo tamaño; `nearest` son bloques simples, pero a una escala no entera (siempre en horizontal, porque los píxeles del Atari no son cuadrados) algunas columnas salen un píxel de pantalla más anchas; `linear` es suave y borroso. L lo cambia durante el juego |
| `-stats` | muestra el renderizador y la frecuencia de la pantalla, y luego cada segundo los fotogramas por segundo, el tiempo por fotograma y la cola de sonido |
| `-nosound` | sin sonido |

El título de la ventana muestra los colores y el escalado en uso. En una pantalla de 60 Hz (o 120 Hz...) el juego va exactamente a la frecuencia de la pantalla (60 en lugar de los 59,92 fotogramas por segundo del Atari), para que cada fotograma se vea el mismo tiempo; en otras pantallas, a la frecuencia del Atari.

Ejemplos: `river_raid -bridge 30` empieza en el puente 30; `river_raid -seed 12345` es otro río; `river_raid -seed 12345 -bridge 80` es ese río en su punto más difícil.

Tras encender, el modo de demostración sobrevuela el río. START comienza una partida; SELECT (u OPTION) elige las partidas 1–8: los números pares son partidas a dos jugadores (se turnan), las partidas 3–4, 5–6 y 7–8 empiezan en los puentes 5, 20 y 50. Cualquier tecla del teclado del Atari pausa. Tras el fin de la partida la pantalla se queda quieta hasta que vuelve el modo de demostración (unos 35 s). El juego y el visor se cierran de la forma habitual: cerrando la ventana, Alt+F4, Cmd+Q.

| Tecla | Atari |
|---|---|
| flechas o W A S D | joystick: izquierda/derecha gira, arriba más rápido, abajo más lento; cualquier dirección lanza el avión en el puente |
| Ctrl izquierdo/derecho, Z, X | disparo |
| F4 o Intro | START |
| F3 | SELECT |
| F2 | OPTION |
| Espacio | una tecla del teclado: pausa |
| F, F11 o Alt+Intro | pantalla completa |
| C | colores NTSC / PAL |
| V | modelo de color Altirra / atari800 |
| O | solo la imagen del juego / el fotograma completo con su borde |
| L | escalado: sharp / nearest / linear |
| Esc | salir de pantalla completa |
| clic del ratón | oculta el puntero hasta que se mueva el ratón (también al pasar a pantalla completa) |

El teclado maneja ambos joysticks, así que una sola persona también puede jugar una partida a dos. Mandos: el primero es el joystick 1 (y el 2 mientras no haya un segundo), el segundo es el joystick 2; cruceta o stick izquierdo, A/B/X/Y o el gatillo derecho para disparar, Start = START, Back = SELECT.

## Visor del río
```
river_view [rom] [opciones]
```
Muestra el modo de demostración del original, el vuelo sobre el río que reproduce cuando nadie pulsa START: tras encender, la pantalla se llena con el río de la partida 1, que luego avanza 1 línea por fotograma; barcos y helicópteros despiertan y se mueven, los tanques van hacia los puentes y disparan, los helicópteros lanzan misiles (la lógica del juego portada, `src/game.cpp`); debajo están el panel y la línea deslizante con los créditos, "Press START key" y el logotipo arcoíris de Activision. Las opciones son las del juego: `rom`, `-os`, `-seed`, `-bridge` (el modo de demostración vuela desde ese puente), `-random`, `-pal`, `-colours`, `-palette`, `-border`, `-filter`.

Las letras y las cifras 1–9 vienen de la fuente del sistema operativo, que no está en el cartucho. El port lleva integrada la fuente de AltirraOS: el sistema operativo libre de reemplazo que se distribuye con atari800 (licencia: se permite copiar y modificar conservando el aviso de copyright, vea `src/os_font.cpp`); las trazas de referencia también se hicieron con ella. `-os archivo` toma la fuente del propio sistema del Atari de una imagen de la ROM del sistema (10 KB o 16 KB) o de un archivo de fuente de 1 KB.

| Tecla | Acción |
|---|---|
| Espacio | pausa |
| Arriba / Abajo | más rápido / más lento, pasando por la parada hasta volar hacia atrás (de x1/4 a x16 en ambos sentidos) |
| rueda del ratón, RePág / AvPág | desplazar el río a mano, una pantalla |
| Retroceso | volver a la línea más reciente a velocidad normal |
| B / Mayús+B | puente siguiente / anterior |
| Inicio | empezar de nuevo |
| F1 / F2 / F3 | prueba de sonido: explosión / disparo / sonido de puntos a través del controlador de sonido del juego (el modo de demostración en sí es mudo, como en el original) |
| F | pantalla completa |
| C | colores NTSC / PAL |
| V | modelo de color Altirra / atari800 |
| O | solo la imagen del juego / el fotograma completo con su borde |
| L | escalado: sharp / nearest / linear |
| Esc | salir de pantalla completa |
| clic del ratón | oculta el puntero hasta que se mueva el ratón (también al pasar a pantalla completa) |

Comparado con las pantallas del original (modo de demostración, fotogramas 1000–2300, con los valores RANDOM del original): la zona del río y el panel coinciden píxel a píxel, la línea deslizante también salvo sus columnas exteriores (a la izquierda de 24, a la derecha de 359), donde el emulador trata de otra forma el desplazamiento fino en el borde.

Qué objeto despierta y cuándo lo decide el registro RANDOM de POKEY, un contador de hardware leído en un momento que depende de la temporización de toda la máquina. El port modela el contador (`src/pokey_random.h`) y lo arranca desde el reloj, igual que la máquina real lo arranca al encenderse, así que cada ejecución es distinta, como en la máquina real; `-random N` repite una ejecución.

## Contenido
| Archivo | Qué |
|---|---|
| `src/rom.h` | la imagen del cartucho y funciones de acceso con nombre para cada tabla que el port lee de ella (formas, colores, puntos, tablas del generador, texto del panel...); tipos de objeto |
| `src/river_gen.h/.cpp` | el generador: `RiverGen::startGame(n)`, luego `nextLine(line)` — una línea de pantalla por llamada |
| `src/river_draw.h/.cpp` | dibujar una línea como en la pantalla: tonos de la orilla, carreteras en los puentes, sprites de los objetos (`drawScreenLine`) |
| `src/palette.h/.cpp` | colores del Atari de máquinas NTSC y PAL (`atariRgb`): los modelos de Altirra y atari800 |
| `src/game.h/.cpp` | el juego: el bucle principal y el VBI del original fotograma a fotograma — modo de demostración, teclas de la consola, el avión, el combustible, colisiones, explosiones, puntos, vidas, jugadores, muerte y reinicio, fin de partida, récords, controlador de sonido |
| `src/display.cpp` | la visualización del río tal como la maneja el DLI: un sprite reutilizado para cada objeto, colisiones por objeto, dibujo de la zona del río |
| `src/pokey_random.h` | el contador RANDOM de POKEY |
| `src/pokey_sound.h/.cpp` | el sonido de POKEY: cuatro canales, divisores y contadores de ruido, generados ciclo a ciclo |
| `src/status_bar.h/.cpp` | panel y línea deslizante: juegos de caracteres, panel, paso del desplazamiento, dibujo |
| `src/os_font.h/.cpp` | fuente del sistema operativo: AltirraOS integrada o cargada de una ROM del sistema |
| `src/river_state_io.cpp`, `src/game_ram.cpp` | correspondencia de los campos del generador y del juego con las direcciones de RAM del original: carga, escritura y comparación (para la verificación) |
| `game/river_raid.cpp` | el juego (SDL3): imagen, sonido, teclado y mandos |
| `viewer/river_view.cpp` | el visor del río (SDL3), vea arriba |
| `sdl/host.h`, `host_sdl3.cpp`, `host_sdl2.cpp` | ventana, imagen, sonido, teclado y mandos para el juego y el visor, sobre SDL3 o SDL2; ocultación del puntero del ratón |
| `tools/river_map.cpp` | genera el río y guarda un PNG tal como lo muestra el juego: `river_map <rom> <partida 1..8> <líneas> out.png [-pal] [-flat]` (`-flat`: solo el mapa de bits, colores fijos) |
| `tools/river_dump.cpp` | escribe el río generado como texto (color de cada punto, objetos colocados) para `tools/doc_figures.py`, que dibuja las imágenes de `docs/HOW_IT_WORKS.es.md`: `python tools/doc_figures.py rom/River_Raid.rom` (necesita matplotlib) |
| `tools/sound_check.cpp` | compara el controlador de sonido con las escrituras del original en POKEY (`rr_oracle -vbi`) |
| `tools/sound_render.cpp` | convierte un registro de escrituras en POKEY a WAV con el POKEY del port, para compararlo con el sonido del emulador (`rr_oracle -wav`) |
| `tools/game_check.cpp` | compara el juego con una traza de RAM del original fotograma a fotograma, con la entrada y los valores RANDOM del original (y la zona del río con sus pantallas) |
| `tools/status_check.cpp` | compara el panel y la línea deslizante con pantallas completas del original (`rr_oracle -screens A:B -ram A:B`) |
| `tools/render_check.cpp` | compara el dibujo con la pantalla del original (`oracle/rr_oracle -ghost -strip 20`) |
| `tools/terrain_check.cpp` | comparación con una traza de referencia (`oracle/rr_oracle -ram A:B`) |
| `tools/equivalence/` | comprobación de refactorizaciones: un commit anterior del port frente a la copia de trabajo, fotograma a fotograma (vea Código) |

El generador lee tablas y gráficos de la ROM del usuario (`rom/River_Raid.rom`); en el código no hay datos del juego.

## Código
El port está escrito para leerse sin tener a mano el listado del original:
- los campos de estado, funciones y constantes se llaman por lo que significan (`planeState`, `shellTimer`, `enteringSlot`, `positionObject`, `kFacingLeft`); el comentario de cada campo da la dirección de RAM del original y el de cada función las direcciones del código de donde viene, así que el listado (`disasm/`) y las trazas se pueden seguir igualmente;
- las tablas del cartucho se leen mediante funciones de acceso con nombre en `src/rom.h` (`rom.points(type)`, `rom.objectHeight(type)`, `rom.shellRise(t)`...), no por dirección;
- los valores siguen siendo de 8 bits con desbordamiento donde lo son en el original (`lo()`), porque las posiciones dan la vuelta a la pantalla; donde el original lleva el indicador de acarreo del 6502 de una suma a la siguiente, `addWithCarry`/`subtractWithBorrow` lo dicen de forma explícita;
- la interrupción de la lista de visualización (`display.cpp`) es la única parte que sigue cerca de las instrucciones: su temporización dentro de la línea de barrido decide qué colisiones se ven, así que se ejecuta contando ciclos.

Los cambios que no deberían alterar el comportamiento se comprueban con `tools/equivalence`: compila un commit anterior (desde 06ffc73) junto a la copia de trabajo y ejecuta ambos sobre 150 000 líneas de río de cada partida (NTSC y PAL), sobre partidas al azar desde el encendido y desde 194 estados de las trazas del original, comparando tras cada paso todo el estado, la pantalla y los registros de sonido. El cambio de nombres y la reestructuración del generador, el juego y la visualización se comprobaron así (0 diferencias), además de con todas las comprobaciones de abajo.

```
port/tools/equivalence/build.sh 06ffc73
port/build/eq/eq
```

## Cómo funciona el generador
Cada llamada a `nextLine()` produce una nueva línea superior de la pantalla:
1. `countLines`: conteo de líneas (un bloque son 32 líneas, una sección 2 bloques, 16 secciones hasta el puente), paso del LFSR, un nuevo objetivo de ancho al empezar una sección (`chooseSectionWidth`: las islas se abren y se cierran);
2. `changeIsland`: la isla y el ancho del agua se mueven suavemente hacia el objetivo;
3. `moveBank`: el paseo aleatorio de la orilla, que da la vuelta en sus límites y ante el objeto nuevo (`bankWouldHitObject`);
4. `startDecoration`: una casa o una montaña en la orilla;
5. `drawLine`: los 48 bytes de la línea (rellenos, piezas de borde de la tabla en `$B800`, una fila del adorno);
6. `scrollObjects` / `placeObject`: los slots de objetos avanzan, cada 16 líneas se coloca un objeto nuevo (`chooseEnemy`, `positionObject`, `placeTank`).

La generación no depende ni del jugador ni de `RANDOM`. Se comprobó con la ejecución de referencia: distintas semillas de POKEY, y una ejecución del bot frente a un vuelo sin entrada, dan el mismo estado en cada línea.

## Verificación
- Desde el estado de referencia: 119 758 líneas de todo el río (120 000 fotogramas), 0 diferencias en el estado y en las líneas del mapa de bits.
- Desde cero (`startGame(1)`, sin emulador): tras las 165 líneas que llenan la pantalla el estado coincide con el original, y luego 29 757 líneas sin diferencias.
- Partida del bot con puentes destruidos y el efecto de destello: diferencias solo en los fotogramas de reaparición. Vienen del código de reinicio, aún no portado.
- La comprobación detecta errores introducidos: cada una de cuatro roturas deliberadas (condición del paseo, ancho del agua, dibujo, color de relleno) produjo diferencias.

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/ghost_film.txt 30001 out -ghost -ram 0:30000
port/build/terrain_check rom/River_Raid.rom out/ram.bin 0 240 29760 5 -fresh 1 145
```

Dibujo: en cada fotograma en que una línea nueva entra en la línea 20 de la pantalla, la línea del original se compara con `drawScreenLine()` de la línea del generador C++ (arrancado desde cero).
- Con los objetos donde los colocó el generador: 96,8% de las líneas idénticas en 29 000 fotogramas (95,3% en 120 000). Las demás difieren donde el juego ha movido un objeto.
- Con las posiciones de los objetos tomadas del juego en marcha (`-ram`, x del slot, indicadores, tipo): 98,1% idénticas; cada línea restante difiere solo mientras está activo el tanque de la carretera (`$38`), que dibuja la lógica del juego. 0 líneas sin explicar de 29 600.

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/ghost_film.txt 30001 out -ghost -strip 20 -ram 0:30000
port/build/render_check rom/River_Raid.rom out/strip.bin out/frames.csv -ram out/ram.bin
```
Panel y línea deslizante: los juegos de caracteres construidos a partir de la fuente del sistema y el cartucho son iguales a la RAM del original, el estado del desplazamiento ($3FB3/$26/$27) coincide en cada fotograma y las líneas dibujadas 180–239 son idénticas en los 1299 fotogramas, salvo las columnas exteriores de la línea deslizante (vea Visor del río).

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/attract_idle.txt 2300 out -screens 1000:2299 -ram 1000:2299
port/build/status_check rom/River_Raid.rom out/screens.bin out/ram.bin
```
Fotograma de juego en el modo de demostración (`game_check`): arrancado desde el estado tras encender (`Game::startAttract`) y con los valores RANDOM del original, el port recorre 19 900 fotogramas (hasta el puente 20) sin ninguna diferencia con la RAM del original en ningún byte comparado: estado del generador, slots de objetos (posición, dirección, tipo, bit de colisión), tanque, su proyectil, misil del helicóptero, resultados de colisiones, memoria player-missile, las líneas del río en pantalla. La zona del río dibujada a partir de ello es igual a las pantallas del original en los 1299 fotogramas comparados.

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/attract_idle.txt 20000 out -strip 20
port/build/game_check rom/River_Raid.rom out/ram.bin out/random.log 38 19900 -attract
```

El juego con un jugador (`game_check` en ejecuciones con entrada). Desde el estado del modo de demostración, el port recibe la entrada y los valores RANDOM del original y sigue solo, sin recargarse nunca; en cada fotograma su RAM se compara con la del original (generador, slots, avión, combustible, bala, colisiones, puntos, vidas, ambos jugadores, récords y sus bytes de control, panel, línea deslizante, memoria player-missile, líneas de pantalla):
- partida del bot a un jugador (`start_only.txt -bot -seed 3`, 8 950 fotogramas: modo de demostración, START, 4 aviones, puentes, muertes y reinicios, fin de partida, de nuevo modo de demostración): idéntica en cada fotograma comparado;
- partida a dos con SELECT y la tecla de pausa (`two_players.txt`, hasta el fin de partida en el fotograma 3 007): idéntica hasta el fin de partida (un fotograma, vea abajo);
- la zona del río dibujada por el port (avión, bala, enemigos, explosiones, tanque, proyectiles) es igual a las pantallas del original en los 1 565 fotogramas comparados de una partida del bot.

Donde el resultado del original depende de los ciclos de la CPU:
- El DLI (`display.cpp`) se ejecuta contando ciclos, tal como el emulador de referencia reparte una línea de barrido: al principio los accesos de player/missile y de la lista de visualización, la CPU hasta el ciclo 28, luego se dibuja toda la línea (colisiones incluidas), después los accesos a los datos de pantalla, WSYNC en el ciclo 106. Así se sabe si una lectura de un registro de colisión ya ve la línea actual: una bala que toca un objeto solo en la línea de la lectura cuenta en un puente (esa ruta lee después del ciclo 28) y no en otros sitios, como en el original.
- La instantánea de la RAM se toma al final del fotograma; normalmente el bucle principal del original está entonces esperando el siguiente fotograma (AE99–AE9E). Justo después de empezar una partida, mientras el río se vuelve a llenar a 2 líneas por fotograma, a veces aún está dibujando la segunda línea (1 fotograma por llenado). `game_check` informa de esos fotogramas aparte y no los compara; el siguiente fotograma vuelve a ser idéntico.
- No modelado: el fin de partida. La lógica del juego del bucle principal empieza en la línea 160 y comparte la CPU con el DLI y el VBI (línea 248). En el fotograma del fin de partida, la comprobación del récord y sus bytes de control (sub $B261, $B298) llevan miles de ciclos, así que `sub_B21A` pone el contador de fotogramas a $FF antes del VBI, después de él o (partida a dos) solo en el fotograma siguiente, según los ciclos de todo lo anterior en ese fotograma. El port lo hace antes del VBI; entonces el contador de fotogramas puede diferir en uno, lo que desplaza un fotograma el modo de demostración siguiente. Reproducirlo exigiría la cuenta de ciclos de todo el bucle principal en ese fotograma (incluido el manejador de interrupciones del sistema).

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/start_only.txt 9000 out -bot -seed 3
port/build/game_check rom/River_Raid.rom out/ram.bin out/random.log 38 8950 -attract
```

Sonido. El controlador de sonido (VBI B2A5–B3D8, `Game::soundDriver`) se ejecutó sobre la RAM que tenía el original al principio de cada VBI de una partida del bot (2989 fotogramas, 2199 con sonido: motor, aviso de combustible bajo, explosiones, disparos, puntos, proyectil del tanque): cada valor AUDF/AUDC es igual al que escribió el original. El modelo de POKEY (`src/pokey_sound.cpp`) se comparó con el sonido de atari800 para las mismas escrituras en los registros: el mismo volumen a lo largo del tiempo (correlación de las envolventes 0,95), los mismos tonos puros a la misma altura, ruido con la misma forma de espectro (coseno de los espectros suavizados 0,94); el patrón del ruido muestra a muestra difiere, porque depende de dónde estén en ese momento los contadores de ruido.

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/start_only.txt 3000 out -bot -wav -vbi
port/build/sound_check rom/River_Raid.rom out/vbi.bin out/pokey.log out/random.log
port/build/sound_render out/pokey.log port.wav
```

## Pendiente
- El encendido en sí (A000–A17C: los primeros 38 fotogramas antes del modo de demostración) se dibuja como una pantalla vacía; `Game::powerOn` aún no es exacto.
- La máquina PAL (`-pal`): el generador la conoce, la lista de visualización y la temporización no.
- Los colores siguen los modelos de la salida GTIA de los emuladores (Altirra por defecto, atari800 con V; ambos comprobados byte a byte con el código de paleta de los propios emuladores con sus ajustes por defecto, atari800 estirado de sus niveles de vídeo 16–235 a 0–255), no una paleta medida en un televisor real; lo brillante que se vea depende tanto de los ajustes del televisor como de la máquina.
