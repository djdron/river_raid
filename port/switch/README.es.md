# River Raid en Nintendo Switch (homebrew)

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Polski](README.pl.md) | [Italiano](README.it.md) | Español | [中文](README.zh.md) | [日本語](README.ja.md)

El juego (sin el visor) como app homebrew, un `.nro` para una Switch que ejecuta homebrew con el Homebrew Launcher. Se compila con devkitPro y su SDL2 (SDL mismo no admite la Switch). Compila; aún no se ha probado en una consola.

## Qué hace falta
- devkitPro con los paquetes de Switch `switch-dev` y `switch-sdl2` (se instalan con `dkp-pacman`), o su imagen Docker `devkitpro/devkita64`.
- Su propio volcado del cartucho, `River_Raid.rom` (8192 bytes). No se mete en el `.nro`, así que el `.nro` en sí se puede compartir.

## Compilación
Desde la raíz del repositorio:
```
cmake -S port -B build-switch -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-switch
```
El resultado es `build-switch/river_raid.nro`. La CI (`.github/workflows/switch.yml`) lo compila en cada push que cambia `port/` y lo guarda como artefacto de la ejecución.

## En la consola
Copie a la tarjeta SD:
```
/switch/river_raid/river_raid.nro
/switch/river_raid/rom/River_Raid.rom
```
y arranque River Raid (port) desde el Homebrew Launcher. Sin la ROM el juego lo dice y espera a +.

Los controles son los de un mando en el PC (ver [../README.es.md](../README.es.md)): el stick izquierdo o la cruceta dirigen, A, B, Y o ZR disparan, X pausa, R guarda el estado y L vuelve a él, ZL cambia el escalado; − es SELECT y + es START, los dos a la vez cambian entre NTSC y PAL. Se sale del juego con el botón HOME (cerrar el programa). La pantalla de la Switch y su salida al televisor van solo a 60 Hz, así que una máquina PAL funciona a sus propios 49,86 fotogramas por segundo, sin ir al ritmo de la pantalla.
