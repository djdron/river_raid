# River Raid su Xbox (UWP)

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Polski](README.pl.md) | Italiano | [Español](README.es.md) | [中文](README.zh.md) | [日本語](README.ja.md)

Il gioco (senza il visualizzatore) come app UWP: gira su Windows 10/11 e su una Xbox One o Series in Dev Mode. È compilato su SDL2, perché SDL3 non supporta più UWP. **Non ancora provato**: scritto senza Windows a disposizione; se un passo non funziona come descritto, per favore riferite cosa è successo.

## Cosa serve
- Windows 10/11 con Visual Studio 2022: i carichi di lavoro «Sviluppo di applicazioni desktop con C++» e «Sviluppo di applicazioni per la piattaforma UWP» (con gli strumenti C++ per UWP, v143).
- CMake 3.16 o successivo (va bene quello di Visual Studio) e git: SDL2 viene scaricato e compilato insieme al gioco.
- Per la Xbox: un account sviluppatore e la console in Dev Mode (l'app «Dev Mode Activation» sulla Xbox).
- Il vostro dump della cartuccia come `rom/River_Raid.rom` (8192 byte): viene messo **nel pacchetto**, quindi il pacchetto è solo per voi; non distribuitelo.

## Compilazione
```
cmake -S port -B build-uwp -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0
```
Aprite `build-uwp/river_raid_port.sln`, scegliete Release e x64 e impostate `river_raid` come progetto di avvio. Avviatelo prima sul computer locale (Windows): la finestra dovrebbe mostrare il gioco.

Il pacchetto è firmato con il certificato di prova incluso in CMake (`CN=CMake Test Cert`). Il vostro: `-DRR_UWP_CERTIFICATE=my.pfx -DRR_UWP_PUBLISHER="CN=..."` (il soggetto del certificato). Un altro percorso della ROM: `-DRR_UWP_ROM=...`.

## Sulla Xbox
Va bene l'uno o l'altro modo:
- **Da Visual Studio:** destinazione di debug «Computer remoto», l'indirizzo IP della Xbox (lo mostra Dev Home), autenticazione «Universal (Unencrypted Protocol)»; abbinamento con il PIN mostrato da Dev Home.
- **Come pacchetto tramite Device Portal:** in Visual Studio Progetto → Pubblica → Crea pacchetti dell'app → Trasferimento locale, x64; poi in un browser sul PC aprite `https://<ip xbox>:11443` (Device Portal, da attivare in Dev Home), Add, e scegliete il `.msix` con le dipendenze dalla sua cartella (il pacchetto VCLibs).

Poi in Dev Home selezionate l'app, View details, e impostate App type su **Game**: altrimenti un'app riceve meno risorse della console.

## Comandi sulla Xbox
| Gamepad | |
|---|---|
| Croce direzionale o levetta sinistra | joystick |
| A, B, Y o grilletto destro | fuoco |
| X | il tasto della pausa (la Xbox non ha tastiera) |
| Menu | START |
| View | SELECT |
| LB / RB | salva lo stato / tornaci |

Colori, tavolozza, bordo e scala sono quelli predefiniti (colori PAL, Altirra, solo l'immagine del gioco, nitida). B è anche il tasto Indietro del sistema; il gioco lo intercetta, così non chiude l'app. Il tasto Xbox esce dal gioco.

## Cosa potrebbe restare da fare
- L'area sicura del televisore: le app UWP sulla Xbox possono essere disegnate con un margine dai bordi dello schermo. L'immagine è comunque circondata dal nero; se il margine dà fastidio, l'app può chiedere tutto lo schermo (`ApplicationView::SetDesiredBoundsMode`).
- Come sono messi insieme i file (`uwp/uwp.cmake`, `uwp/Package.appxmanifest.in`): il supporto di CMake ai progetti UWP e il punto d'ingresso WinRT di SDL2 (`SDL_winrt_main_NonXAML.cpp`, compilato come C++/CX) sono usati come da documentazione, ma qui non sono stati compilati.
