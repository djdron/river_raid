# River Raid na Nintendo Switch (homebrew)

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | Polski | [Italiano](README.it.md) | [Español](README.es.md) | [中文](README.zh.md) | [日本語](README.ja.md)

Gra (bez przeglądarki) jako aplikacja homebrew, `.nro` dla Switcha, który uruchamia homebrew przez Homebrew Launcher. Budowana z devkitPro i jego SDL2 (samo SDL nie obsługuje Switcha). Buduje się; na konsoli jeszcze nie wypróbowana.

## Co jest potrzebne
- devkitPro z pakietami dla Switcha `switch-dev` i `switch-sdl2` (instalowane przez `dkp-pacman`) albo jego obraz Dockera `devkitpro/devkita64`.
- Własny zrzut kartridża jako `rom/River_Raid.rom` (8192 bajty; inna ścieżka: `-DRR_SWITCH_ROM=...`). Trafia **do** `.nro` (do jego RomFS), więc to `.nro` jest tylko twoje: nie przekazuj go innym.

## Budowanie
Z katalogu głównego repozytorium:
```
cmake -S port -B build-switch -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-switch
```
Wynikiem jest `build-switch/river_raid.nro`. Bez ROM-u też się zbuduje i wtedy szuka ROM-u na karcie SD (niżej). CI (`.github/workflows/switch.yml`) buduje go przy każdym pushu zmieniającym `port/` i zachowuje, bez ROM-u, jako artefakt przebiegu.

## Na konsoli
Skopiuj `river_raid.nro` na kartę SD, na przykład do `/switch/river_raid/`, i uruchom River Raid (port) z Homebrew Launchera. `.nro` zbudowane bez ROM-u czyta go z katalogu `rom` obok siebie:
```
/switch/river_raid/river_raid.nro
/switch/river_raid/rom/River_Raid.rom
```
Bez ROM-u gra o tym powie i będzie czekać na +.

Sterowanie jak gamepadem na PC (zob. [../README.pl.md](../README.pl.md)): lewa gałka lub krzyżak sterują, A, B, Y lub ZR strzelają, X to pauza, R zapisuje stan, a L do niego wraca, ZL przełącza skalowanie; − to SELECT, a + to START, oba razem przełączają NTSC i PAL. Z gry wychodzi się przyciskiem HOME (zamknij oprogramowanie). Ekran Switcha i jego wyjście na telewizor działają tylko w 60 Hz, więc komputer PAL działa z własnymi 49,86 klatki na sekundę, nie w takt ekranu.
