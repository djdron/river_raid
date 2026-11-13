# River Raid na Xboksie (UWP)

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | Polski | [Italiano](README.it.md) | [Español](README.es.md) | [中文](README.zh.md) | [日本語](README.ja.md)

Gra (bez przeglądarki) jako aplikacja UWP: działa na Windows 10/11 oraz na Xboksie One lub Series w Dev Mode. Budowana na SDL2, bo SDL3 nie obsługuje już UWP. Wypróbowane na Windows 11 i na Xboksie Series S, zbudowane w Visual Studio 2026.

## Co jest potrzebne
- Windows 10/11 z Visual Studio 2022 lub 2026 i narzędziami C++ dla UWP. W 2022: obciążenia „Programowanie aplikacji klasycznych w języku C++” i „Programowanie aplikacji na platformę uniwersalną systemu Windows” (z narzędziami C++ dla UWP). W 2026: „Programowanie aplikacji klasycznych w języku C++” oraz w obciążeniu „Tworzenie aplikacji WinUI” składnik „C++ Universal Windows Platform tools”.
- CMake 3.16 lub nowszy (wystarczy ten z Visual Studio) i git: SDL2 jest pobierany i budowany razem z grą.
- Dla Xboksa: konto dewelopera i konsola w Dev Mode (aplikacja „Dev Mode Activation” na Xboksie).
- Twój zrzut kartridża jako `rom/River_Raid.rom` (8192 bajty): trafia **do pakietu**, więc pakiet jest tylko dla ciebie; nie udostępniaj go.

## Budowanie
```
cmake -S port -B build-uwp -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0
```
W Visual Studio 2026 generator to `-G "Visual Studio 18 2026"` (CMake 4.2 lub nowszy, np. ten dołączony do niego).

Otwórz `build-uwp/river_raid_port.sln`, wybierz Release i x64 i ustaw `river_raid` jako projekt startowy. Najpierw uruchom na komputerze lokalnym (Windows): okno powinno pokazać grę.

Pakiet jest podpisywany certyfikatem testowym dołączonym do CMake (`CN=CMake Test Cert`). Własny: `-DRR_UWP_CERTIFICATE=my.pfx -DRR_UWP_PUBLISHER="CN=..."` (podmiot certyfikatu). Inna ścieżka do ROM-u: `-DRR_UWP_ROM=...`.

## Na Xboksa
Działa każdy sposób:
- **Z Visual Studio:** cel debugowania „Maszyna zdalna”, adres IP Xboksa (pokazuje go Dev Home), uwierzytelnianie „Universal (Unencrypted Protocol)”; parowanie kodem PIN, który pokazuje Dev Home.
- **Pakietem przez Device Portal:** w Visual Studio Projekt → Publikuj → Utwórz pakiety aplikacji → Ładowanie bezpośrednie, x64; potem w przeglądarce na PC otwórz `https://<ip xboksa>:11443` (Device Portal, włączany w Dev Home), Add i wybierz `.msix` z zależnościami z jego folderu (pakiet VCLibs).

Potem w Dev Home wybierz aplikację, View details i ustaw App type na **Game**: inaczej aplikacja dostaje mniej zasobów konsoli. Na Xboksie ktoś musi być zalogowany: inaczej aplikacja się nie uruchamia (Visual Studio: „Unable to activate Windows Store app … 0x8004090a”).

Gamepad działa tak samo jak na PC (zob. [../README.pl.md](../README.pl.md)), X to pauza. Bez klawiatury pozostałe ustawienia są domyślne: kolory PAL, Altirra, tylko obraz gry, ostre skalowanie (przełącza je LT). B to także systemowy przycisk Wstecz; gra go przechwytuje, więc nie zamyka aplikacji. Przycisk Xbox wychodzi z gry.

Jeśli przy starcie gry trzymasz LT, uruchomi się jako komputer PAL (jak `-pal`: 50 klatek na sekundę, około 17% wolniej) i przełączy telewizor na 50 Hz, jeśli konsola na to pozwala (Settings → TV & display options → Video modes → Allow 50 Hz), a na końcu przywróci poprzedni tryb. Bez 50 Hz gra i tak działa jako PAL, tylko nie w takt telewizora.
