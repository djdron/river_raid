# River Raid на Nintendo Switch (homebrew)

[English](README.md) | Русский | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Polski](README.pl.md) | [Italiano](README.it.md) | [Español](README.es.md) | [中文](README.zh.md) | [日本語](README.ja.md)

Игра (без просмотрщика) как homebrew-приложение, `.nro` для Switch, на которой homebrew запускается через Homebrew Launcher. Собирается с devkitPro и его SDL2 (сам SDL Switch не поддерживает). Сборка проходит; на консоли ещё не проверялось.

## Что нужно
- devkitPro с пакетами для Switch `switch-dev` и `switch-sdl2` (ставятся через `dkp-pacman`) или его Docker-образ `devkitpro/devkita64`.
- Ваш собственный дамп картриджа, `River_Raid.rom` (8192 байта). В `.nro` он не кладётся, так что сам `.nro` можно передавать другим.

## Сборка
Из корня репозитория:
```
cmake -S port -B build-switch -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-switch
```
Получится `build-switch/river_raid.nro`. CI (`.github/workflows/switch.yml`) собирает его при каждом пуше, меняющем `port/`, и сохраняет как артефакт запуска.

## На консоли
Скопируйте на SD-карту:
```
/switch/river_raid/river_raid.nro
/switch/river_raid/rom/River_Raid.rom
```
и запустите River Raid (port) из Homebrew Launcher. Без ROM игра сообщит об этом и будет ждать +.

Управление как у геймпада на ПК (см. [../README.ru.md](../README.ru.md)): левый стик или крестовина — руль, A, B, Y или ZR — огонь, X — пауза, R сохраняет состояние, L возвращается к нему, ZL переключает масштабирование; − — это SELECT, + — START, оба вместе переключают NTSC и PAL. Выход из игры — кнопкой HOME (закрыть программу). Экран Switch и её выход на телевизор работают только на 60 Гц, поэтому машина PAL идёт со своими 49,86 кадра в секунду, без привязки к экрану.
