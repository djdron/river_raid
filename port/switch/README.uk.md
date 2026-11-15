# River Raid на Nintendo Switch (homebrew)

[English](README.md) | [Русский](README.ru.md) | Українська | [Deutsch](README.de.md) | [Polski](README.pl.md) | [Italiano](README.it.md) | [Español](README.es.md) | [中文](README.zh.md) | [日本語](README.ja.md)

Гра (без переглядача) як homebrew-застосунок, `.nro` для Switch, на якій homebrew запускається через Homebrew Launcher. Збирається з devkitPro і його SDL2 (сам SDL Switch не підтримує). Збірка проходить; на консолі ще не перевірялося.

## Що потрібно
- devkitPro з пакетами для Switch `switch-dev` і `switch-sdl2` (ставляться через `dkp-pacman`) або його Docker-образ `devkitpro/devkita64`.
- Ваш власний дамп картриджа як `rom/River_Raid.rom` (8192 байти; інший шлях: `-DRR_SWITCH_ROM=...`). Він кладеться **всередину** `.nro` (у його RomFS), тож цей `.nro` лише ваш: не передавайте його іншим.

## Збірка
З кореня репозиторію:
```
cmake -S port -B build-switch -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-switch
```
Вийде `build-switch/river_raid.nro`. Без ROM він теж збереться й тоді шукатиме ROM на SD-карті (нижче). CI (`.github/workflows/switch.yml`) збирає його під час кожного пушу, що змінює `port/`, і зберігає, без ROM, як артефакт запуску.

## На консолі
Скопіюйте `river_raid.nro` на SD-карту, наприклад до `/switch/river_raid/`, і запустіть River Raid (port) з Homebrew Launcher. `.nro`, зібраний без ROM, читає його з теки `rom` поруч із собою:
```
/switch/river_raid/river_raid.nro
/switch/river_raid/rom/River_Raid.rom
```
Без ROM гра повідомить про це й чекатиме на +.

Керування як у геймпада на ПК (див. [../README.uk.md](../README.uk.md)): лівий стик або хрестовина — кермо, A, B, Y або ZR — вогонь, X — пауза, R зберігає стан, L повертається до нього, ZL перемикає масштабування; − — це SELECT, + — START, обидві разом перемикають NTSC і PAL. Вихід із гри — кнопкою HOME (закрити програму). Екран Switch і її вихід на телевізор працюють лише на 60 Гц, тому машина PAL іде зі своїми 49,86 кадру на секунду, без прив'язки до екрана.
