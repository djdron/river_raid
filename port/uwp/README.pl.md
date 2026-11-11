# River Raid na Xboksie (UWP)

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | Polski | [Italiano](README.it.md) | [Español](README.es.md) | [中文](README.zh.md) | [日本語](README.ja.md)

Gra (bez przeglądarki) jako aplikacja UWP: działa na Windows 10/11 oraz na Xboksie One lub Series w Dev Mode. Budowana na SDL2, bo SDL3 nie obsługuje już UWP. **Jeszcze nie wypróbowane**: napisane bez Windows pod ręką; jeśli któryś krok nie działa tak, jak opisano, proszę napisać, co się stało.

## Co jest potrzebne
- Windows 10/11 z Visual Studio 2022: obciążenia „Programowanie aplikacji klasycznych w języku C++” i „Programowanie aplikacji na platformę uniwersalną systemu Windows” (z narzędziami C++ dla UWP, v143).
- CMake 3.16 lub nowszy (wystarczy ten z Visual Studio) i git: SDL2 jest pobierany i budowany razem z grą.
- Dla Xboksa: konto dewelopera i konsola w Dev Mode (aplikacja „Dev Mode Activation” na Xboksie).
- Twój zrzut kartridża jako `rom/River_Raid.rom` (8192 bajty): trafia **do pakietu**, więc pakiet jest tylko dla ciebie; nie udostępniaj go.

## Budowanie
```
cmake -S port -B build-uwp -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0
```
Otwórz `build-uwp/river_raid_port.sln`, wybierz Release i x64 i ustaw `river_raid` jako projekt startowy. Najpierw uruchom na komputerze lokalnym (Windows): okno powinno pokazać grę.

Pakiet jest podpisywany certyfikatem testowym dołączonym do CMake (`CN=CMake Test Cert`). Własny: `-DRR_UWP_CERTIFICATE=my.pfx -DRR_UWP_PUBLISHER="CN=..."` (podmiot certyfikatu). Inna ścieżka do ROM-u: `-DRR_UWP_ROM=...`.

## Na Xboksa
Działa każdy sposób:
- **Z Visual Studio:** cel debugowania „Maszyna zdalna”, adres IP Xboksa (pokazuje go Dev Home), uwierzytelnianie „Universal (Unencrypted Protocol)”; parowanie kodem PIN, który pokazuje Dev Home.
- **Pakietem przez Device Portal:** w Visual Studio Projekt → Publikuj → Utwórz pakiety aplikacji → Ładowanie bezpośrednie, x64; potem w przeglądarce na PC otwórz `https://<ip xboksa>:11443` (Device Portal, włączany w Dev Home), Add i wybierz `.msix` z zależnościami z jego folderu (pakiet VCLibs).

Potem w Dev Home wybierz aplikację, View details i ustaw App type na **Game**: inaczej aplikacja dostaje mniej zasobów konsoli.

## Sterowanie na Xboksie
| Gamepad | |
|---|---|
| Krzyżak lub lewa gałka | joystick |
| A, B, Y lub prawy spust | ogień |
| X | klawisz pauzy (Xbox nie ma klawiatury) |
| Menu | START |
| View | SELECT |
| RB / LB | zapisz stan / wróć do niego |

Kolory, paleta, ramka i skalowanie są domyślne (kolory PAL, Altirra, tylko obraz gry, ostre). B to także systemowy przycisk Wstecz; gra go przechwytuje, więc nie zamyka aplikacji. Przycisk Xbox wychodzi z gry.

## Co może wymagać dopracowania
- Bezpieczny obszar telewizora: aplikacje UWP na Xboksie mogą być rysowane z marginesem od krawędzi ekranu. Obraz i tak otacza czerń; jeśli margines przeszkadza, aplikacja może poprosić o cały ekran (`ApplicationView::SetDesiredBoundsMode`).
- Jak złożone są pliki (`uwp/uwp.cmake`, `uwp/Package.appxmanifest.in`): obsługa projektów UWP w CMake i punkt wejścia WinRT z SDL2 (`SDL_winrt_main_NonXAML.cpp`, kompilowany jako C++/CX) są użyte zgodnie z dokumentacją, ale nie były tu budowane.
