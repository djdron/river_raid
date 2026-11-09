# rr_oracle — przebieg wzorcowy River Raid

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | Polski | [Italiano](README.it.md) | [Español](README.es.md) | [中文](README.zh.md) | [日本語](README.ja.md)

Uruchamia oryginalny kartridż bez okna na libatari800 (Atari 800, domyślnie NTSC, wbudowany AltirraOS, BASIC wyłączony). Służy do porównywania portu na C++ z oryginałem klatka po klatce.

## Budowanie
```
./build.sh
```
Skrypt klonuje atari800 (sprawdzony commit 98cc2dc), buduje `libatari800.a` i linkuje `rr_oracle`. Potrzebne są git, kompilator C (gcc lub clang), make, autoconf i automake; działa na Linuksie, macOS (Xcode command line tools, `brew install autoconf automake`), WSL i MSYS2. Przebieg wzorcowy przechwytuje trzy funkcje emulatora (zapisy do POKEY, odczyty RANDOM, NMI wygaszania pionowego): skrypt kompiluje jeszcze raz `pokey.c` i `cpu.c` z przemianowanymi funkcjami, więc nie trzeba żadnych sztuczek linkera.

## Uruchamianie
```
./rr_oracle <rom> <script> <frames> <outdir> [-pal] [-seed N] [-bot|-bot-nofire] [-ghost] [-noram] [-ram A:B] [-strip Y] [-screens A:B] [-shot N,N,...]
./rr_oracle ../rom/River_Raid.rom scripts/start_and_fly.txt 1000 out -shot 240,500
./rr_oracle ../rom/River_Raid.rom scripts/start_only.txt 8000 out -bot      # bot
./rr_oracle ../rom/River_Raid.rom scripts/bot_run_8000.txt 8000 out         # replay of a bot run
```

## Bot (`-bot`)
Najpierw gra skrypt (na przykład `start_only.txt`: włączenie, START, czekanie), potem przejmuje autopilot. Kieruje się ku środkowi wody przed sobą, omija bliskie obiekty, cały czas strzela i wciska „w górę”, gdy wybucha lub czeka przy moście. Bot jest prosty i przeznaczony tylko do długich śladów, nie do rekordów: nie szuka paliwa i ginie od helikopterów nadlatujących z boku.

`-bot-nofire`: ten sam autopilot bez strzelania i omijania.

Wejście faktycznie zastosowane w każdym przebiegu jest zapisywane do `inputs.txt` w formacie skryptu. Podanie go z powrotem bez `-bot` daje ślad identyczny co do bajtu (sprawdzone na 8000 klatkach).

## Opcje do filmowania poziomu
- `-ghost` — niezniszczalny samolot (5-bajtowa łatka kodu w emulowanej pamięci, tylko do filmowania).
- `-strip Y` — linia ekranu Y każdej klatki do `strip.bin`, paleta do `palette.bin`.
- `-screens A:B` — pełne ekrany klatek A…B do `screens.bin`.
- `-noram` — nie zapisuj `ram.bin` (dla długich przebiegów).
- `-ram A:B` — zapisuj `ram.bin` tylko dla klatek A…B (klatka A pod przesunięciem 0).
- `-wav` — wyjście dźwięku emulatora do `sound.wav`.
- `-vbi` — `vbi.bin`: RAM na początku każdego przerwania wygaszania pionowego (rekordy z 4 bajtów numeru klatki + 16 384 bajtów), stan, który widzi sterownik dźwięku.
- `frames.csv` ma kolumny `$60, $2F, $5D, $AA, $AB, $AC, $76, $52, $53, $54, $2D, $4E`.

## Skrypty
| Plik | Co |
|---|---|
| `start_and_fly.txt` | START, przyspieszanie, skręty, strzelanie (1000 klatek) |
| `attract_fire_only.txt` | ogień na ekranie tytułowym (gra się nie zaczyna) |
| `start_only.txt` | włączenie, START, czekanie; dla `-bot` |
| `probe_lr.txt`, `probe_hold.txt` | krótkie naciśnięcia / przytrzymania do szukania zmiennych |
| `bot_run_8000.txt` | nagrany przebieg bota: 2 mosty, 5 śmierci, koniec gry, nowy start |
| `refuel_first_depot.txt` | przelot nad pierwszą stacją paliw bez strzelania (tankowanie) |
| `attract_idle.txt` | włączenie i brak wejścia: tryb demonstracyjny (rzeka gry 1, przewijani autorzy) |
| `two_players.txt` | SELECT na grę 2, START; obaj gracze lecą prosto i strzelają, aż stracą wszystkie samoloty; dwa razy klawisz pauzy |
| `ghost_film.txt` | start i lot bez wejścia; z `-ghost -noram -strip 20` filmuje mapę poziomu |

## Skrypt wejścia
Każda linia: `<klatki> <joystick> <ogień> [start] [select] [option] [joystick2] [ogień2] [klawisz]`; `#` zaczyna komentarz. Ostatnia linia powtarza się do końca przebiegu. `klawisz` trzyma spację (dowolny klawisz wstrzymuje River Raid).
Bity joysticka: 1 w górę, 2 w dół, 4 w lewo, 8 w prawo, 0 na środku.
Gra zaczyna się klawiszem START; ogień na ekranie tytułowym jej nie uruchamia.

## Co trafia do `<outdir>`
| Plik | Zawartość |
|---|---|
| `ram.bin` | $0000–$3FFF (16 KB) po każdej klatce; klatka N jest pod przesunięciem N*16384 |
| `pokey.log` | każdy zapis do POKEY: `klatka linia takt rejestr wartość` |
| `random.log` | każdy odczyt RANDOM: `klatka linia takt wartość`. Linia ~249 to dźwięk (VBI), inne linie to logika gry (A205) |
| `frames.csv` | zastosowane wejście (joystick, ogień, START, potem po kolumnach procesora SELECT, OPTION, joystick 2, ogień 2, klawisz) i rejestry procesora na końcu klatki |
| `inputs.txt` | zastosowane wejście w formacie skryptu (do odtworzenia) |
| `fNNNNN.ppm` | zrzuty ekranu 384×240 |

## Determinizm
Sam atari800 uruchamia generator POKEY od zegara systemowego, więc przebiegi się nie powtarzają. `rr_oracle` ustawia licznik jawnie (`-seed`, domyślnie 0). Przy tym samym ROM-ie, skrypcie i ziarnie wszystkie pliki są identyczne co do bajtu (sprawdzone).

Licencja: libatari800 jest na GPL-2.0+, więc `rr_oracle` jako program z nią zlinkowany też. To narzędzie programistyczne: port na C++ się z nim nie linkuje i tylko czyta jego pliki wyjściowe.
