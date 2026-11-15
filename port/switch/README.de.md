# River Raid auf der Nintendo Switch (Homebrew)

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | Deutsch | [Polski](README.pl.md) | [Italiano](README.it.md) | [Español](README.es.md) | [中文](README.zh.md) | [日本語](README.ja.md)

Das Spiel (ohne den Betrachter) als Homebrew-App, eine `.nro` für eine Switch, die Homebrew über den Homebrew Launcher startet. Gebaut mit devkitPro und dessen SDL2 (SDL selbst unterstützt die Switch nicht). Es lässt sich bauen; auf einer Konsole noch nicht ausprobiert.

## Was man braucht
- devkitPro mit den Switch-Paketen `switch-dev` und `switch-sdl2` (installiert mit `dkp-pacman`) oder sein Docker-Image `devkitpro/devkita64`.
- Einen eigenen Dump des Moduls als `rom/River_Raid.rom` (8192 Bytes; ein anderer Pfad: `-DRR_SWITCH_ROM=...`). Er kommt **in** die `.nro` (in ihr RomFS), diese `.nro` gehört also nur Ihnen: nicht weitergeben.

## Bauen
Aus dem Wurzelverzeichnis des Repositorys:
```
cmake -S port -B build-switch -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-switch
```
Das ergibt `build-switch/river_raid.nro`. Ohne ROM wird sie auch gebaut und sucht das ROM dann auf der SD-Karte (unten). Der CI (`.github/workflows/switch.yml`) baut sie bei jedem Push, der `port/` ändert, und bewahrt sie, ohne ROM, als Artefakt des Laufs auf.

## Auf der Konsole
`river_raid.nro` auf die SD-Karte kopieren, zum Beispiel nach `/switch/river_raid/`, und River Raid (port) aus dem Homebrew Launcher starten. Eine ohne ROM gebaute `.nro` liest es aus dem Ordner `rom` neben sich:
```
/switch/river_raid/river_raid.nro
/switch/river_raid/rom/River_Raid.rom
```
Ohne ROM sagt das Spiel das und wartet auf +.

Die Steuerung ist die eines Gamepads am PC (siehe [../README.de.md](../README.de.md)): linker Stick oder Steuerkreuz lenken, A, B, Y oder ZR feuern, X pausiert, R speichert den Zustand und L kehrt zu ihm zurück, ZL schaltet die Skalierung um; − ist SELECT und + ist START, beide zusammen schalten zwischen NTSC und PAL. Verlassen wird das Spiel mit der HOME-Taste (Software schließen). Der Bildschirm der Switch und ihr Fernsehausgang laufen nur mit 60 Hz, deshalb läuft ein PAL-Gerät mit seinen eigenen 49,86 Bildern pro Sekunde, nicht im Takt des Bildschirms.
