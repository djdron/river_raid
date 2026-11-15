# River Raid su Nintendo Switch (homebrew)

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Polski](README.pl.md) | Italiano | [Español](README.es.md) | [中文](README.zh.md) | [日本語](README.ja.md)

Il gioco (senza il visualizzatore) come app homebrew, un `.nro` per una Switch che avvia homebrew tramite l'Homebrew Launcher. Compilato con devkitPro e il suo SDL2 (SDL stesso non supporta la Switch). Si compila; non ancora provato su una console.

## Cosa serve
- devkitPro con i pacchetti per Switch `switch-dev` e `switch-sdl2` (installati con `dkp-pacman`), o la sua immagine Docker `devkitpro/devkita64`.
- Un vostro dump della cartuccia, `River_Raid.rom` (8192 byte). Non viene messo nel `.nro`, quindi il `.nro` stesso si può condividere.

## Compilazione
Dalla radice del repository:
```
cmake -S port -B build-switch -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-switch
```
Il risultato è `build-switch/river_raid.nro`. La CI (`.github/workflows/switch.yml`) lo compila a ogni push che cambia `port/` e lo conserva come artefatto dell'esecuzione.

## Sulla console
Copiate sulla scheda SD:
```
/switch/river_raid/river_raid.nro
/switch/river_raid/rom/River_Raid.rom
```
e avviate River Raid (port) dall'Homebrew Launcher. Senza la ROM il gioco lo dice e aspetta +.

I comandi sono quelli di un gamepad su PC (vedi [../README.it.md](../README.it.md)): la levetta sinistra o la croce direzionale sterzano, A, B, Y o ZR sparano, X mette in pausa, R salva lo stato e L vi ritorna, ZL cambia la scala; − è SELECT e + è START, insieme passano tra NTSC e PAL. Si esce dal gioco con il tasto HOME (chiudi il software). Lo schermo della Switch e la sua uscita TV vanno solo a 60 Hz, quindi una macchina PAL gira ai suoi 49,86 fotogrammi al secondo, non agganciata allo schermo.
