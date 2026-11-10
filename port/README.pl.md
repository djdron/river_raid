# River Raid — port na C++

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | Polski | [Italiano](README.it.md) | [Español](README.es.md) | [中文](README.zh.md) | [日本語](README.ja.md)

Nieoficjalny projekt fanowski w celach edukacyjnych, niepowiązany z Activision ani Microsoft; nie zawiera żadnej części gry (zob. zastrzeżenie w `../README.pl.md`).

Cała gra: `river_raid` gra w River Raid tak jak oryginał — tryb demonstracyjny, wybór gry, jeden i dwóch graczy, samolot, paliwo, wrogowie, mosty, punkty, życia, rekordy, pauza i dźwięk. Generator rzeki i logika gry dają klatka po klatce ten sam stan RAM co oryginał, a rysowanie ten sam ekran (zob. Weryfikacja).

## Budowanie
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
Rdzeń i narzędzia potrzebują tylko kompilatora C++17. Gra `river_raid` i przeglądarka `river_view` potrzebują SDL3: CMake używa zainstalowanego SDL3 (3.4 lub nowszego), jeśli go znajdzie, a w przeciwnym razie podczas konfiguracji pobiera SDL 3.4.20 z GitHuba (potrzebne git i internet) i linkuje go statycznie. Na Linuksie budowanie SDL ze źródeł wymaga pakietów deweloperskich X11/Wayland (zob. `docs/README-linux.md` w SDL). SDL 3.4 kończy się błędem, gdy takiego pakietu brakuje, i podaje opcję, która buduje bez niego (na przykład `-DSDL_X11_XCURSOR=OFF`). Zupełnie bez SDL: `cmake -S . -B build -DRR_VIEWER=OFF`.

Z SDL2 zamiast SDL3 (dla platform, których SDL3 już nie obsługuje, jak UWP na Xboksie): `cmake -S . -B build -DRR_SDL2=ON`. CMake używa wtedy zainstalowanego SDL2 (2.0.18 lub nowszego) albo pobiera SDL 2.32.10. Programy rozmawiają z SDL tylko przez `sdl/host.h`, zaimplementowany raz dla każdej wersji; obie dają ten sam obraz.

Na Windowsie z Visual Studio: `cmake --build build --config Release`, programy trafiają do `build\Release\`. Zbudowano i sprawdzono w grze na Windows (MSVC), macOS (Xcode, Apple M2) i Linuksie (gcc).

## Gra
```
river_raid [rom] [opcje]
```
Bez argumentów sama znajduje ROM (zob. `rom`) i startuje jak kartridż: tryb demonstracyjny, potem START.

| Opcja | Co robi |
|---|---|
| `rom` | obraz kartridża (8192 bajty). Domyślnie `rom/River_Raid.rom` jest szukany w bieżącym folderze i do trzech folderów wyżej, więc gra startuje bez argumentów z repozytorium lub z `port/build/Release` |
| `-os plik` | litery i cyfry z systemu samego Atari: obraz ROM systemu (10 KB lub 16 KB) albo plik czcionki 1 KB. Bez niej używana jest wbudowana czcionka AltirraOS |
| `-seed N` | inna rzeka, z innymi wrogami i stacjami paliw. N to stan, od którego zaczyna generator rzeki, od 1 do 16777215. Gra 1 zaczyna się na początku tej rzeki, gry 3–8 przy jej mostach 5, 20 i 50; każda gra zachowuje swój zwykły poziom, więc trudność rośnie jak zwykle. Własna rzeka kartridża to `-seed 16241891` |
| `-bridge N` | każda gra (1–8) zaczyna się przy moście N, od 1 do 9999: z N na liczniku mostów i poziomem tego mostu (po 80 poziom zmienia się 79/80, jak w oryginale). Działa też z `-seed`. Start jest wyznaczany przez przebieg generatora rzeki aż do niego (poniżej sekundy nawet dla 9999) |
| `-random N` | wartość początkowa licznika RANDOM układu POKEY, który decyduje, który wróg kiedy się budzi, i o szumie wybuchów. Domyślnie brana z zegara, tak jak w prawdziwym komputerze zależy od chwili włączenia, więc każde uruchomienie jest inne; z tym samym N i tymi samymi naciśnięciami przebieg powtarza się dokładnie |
| `-pal` | graj jak komputer PAL: jak na prawdziwym Atari PAL stacje paliw rzedną do poziomu 47 zamiast 46; kolory PAL. Taktowanie obrazu pozostaje NTSC (zob. Jeszcze nie zrobione) |
| `-colours ntsc\|pal` | kolory komputera NTSC lub PAL; domyślnie PAL. C przełącza w trakcie gry |
| `-palette altirra\|atari800` | model kolorów: `altirra` (domyślnie) lub `atari800` (ciemniejsze tony średnie). V przełącza w trakcie gry |
| `-border` | pokazuje całą klatkę z czarną ramką (384×240 bez 10 kolumn z każdej strony), jak emulatory. Domyślnie okno pokazuje tylko obraz gry z całym przewijanym paskiem, 364×200, więc na pełnym ekranie jest on o około 20% większy, a okno o tyle samo niższe. O przełącza w trakcie gry |
| `-filter sharp\|nearest\|linear` | jak obraz jest skalowany do okna lub ekranu: `sharp` (domyślnie) skaluje o całkowitą wielokrotność z ostrymi pikselami, a potem płynnie do docelowego rozmiaru, więc pozostaje ostry, a każdy piksel ma ten sam rozmiar; `nearest` to zwykłe bloki, ale przy niecałkowitej skali (w poziomie zawsze, bo piksele Atari nie są kwadratowe) niektóre kolumny wychodzą o piksel ekranu szersze; `linear` jest gładki i rozmyty. L przełącza w trakcie gry |
| `-stats` | wypisuje renderer i częstotliwość ekranu, potem co sekundę liczbę klatek, czas na klatkę i kolejkę dźwięku |
| `-nosound` | bez dźwięku |

Tytuł okna pokazuje bieżące kolory i skalowanie. Na ekranie 60 Hz (lub 120 Hz...) gra działa dokładnie z jego częstotliwością (60 zamiast 59,92 klatek na sekundę Atari), więc każda klatka jest widoczna równie długo; na innych ekranach z częstotliwością Atari.

Przykłady: `river_raid -bridge 30` startuje przy moście 30; `river_raid -seed 12345` to inna rzeka; `river_raid -seed 12345 -bridge 80` to ta rzeka w najtrudniejszej postaci.

Po włączeniu tryb demonstracyjny przelatuje nad rzeką. START rozpoczyna grę; SELECT (lub OPTION) wybiera gry 1–8: parzyste numery to gry dla dwóch graczy (grają na zmianę), gry 3–4, 5–6 i 7–8 zaczynają się przy mostach 5, 20 i 50. Dowolny klawisz klawiatury Atari to pauza. Po końcu gry obraz stoi, dopóki znów nie ruszy tryb demonstracyjny (około 35 s). Grę i przeglądarkę zamyka się zwykłym sposobem: zamknięcie okna, Alt+F4, Cmd+Q.

| Klawisz | Atari |
|---|---|
| strzałki lub W A S D | joystick: lewo/prawo skręt, góra szybciej, dół wolniej; dowolny kierunek wypuszcza samolot przy moście |
| lewy/prawy Ctrl, Z, X | ogień |
| F4 lub Enter | START |
| F3 | SELECT |
| F2 | OPTION |
| Spacja | klawisz klawiatury: pauza |
| F, F11 lub Alt+Enter | pełny ekran |
| C | kolory NTSC / PAL |
| V | model kolorów Altirra / atari800 |
| O | tylko obraz gry / cała klatka z ramką |
| L | skalowanie: sharp / nearest / linear |
| F7 / F8 | zapisać stan gry / wrócić do niego (w pamięci, do wyjścia) |
| Esc | wyjście z pełnego ekranu |
| kliknięcie myszą | ukrywa wskaźnik myszy, dopóki mysz się nie ruszy (przejście na pełny ekran też) |

Klawiatura steruje oboma joystickami, więc także jedna osoba może grać w grę dla dwóch. Gamepady: pierwszy to joystick 1 (i 2, dopóki nie ma drugiego), drugi to joystick 2; krzyżak lub lewy drążek, A/B/X/Y lub prawy spust do strzału, Start = START, Back = SELECT, LB / RB = F7 / F8 (zapisać stan / wrócić do niego).

## Przeglądarka rzeki
```
river_view [rom] [opcje]
```
Pokazuje tryb demonstracyjny oryginału, przelot nad rzeką, który gra odtwarza, gdy nikt nie wciska START: po włączeniu ekran wypełnia się rzeką gry 1, która potem przewija się o 1 linię na klatkę; statki i helikoptery budzą się i pływają, czołgi jadą do mostów i strzelają, helikoptery wystrzeliwują rakiety (przeniesiona logika gry, `src/game.cpp`); poniżej panel i przewijany pasek z autorami, „Press START key” i tęczowym logo Activision. Opcje są takie jak w grze: `rom`, `-os`, `-seed`, `-bridge` (przeglądarka najpierw dolatuje do niego od początku rzeki, samą rzeką, dla 9999 to kilka sekund, i pokazuje go tak, jak wyglądałby w locie; wstecz można oglądać aż do początku), `-random`, `-pal`, `-colours`, `-palette`, `-border`, `-filter`; oraz własna `-sound`: tryb demonstracyjny z dźwiękiem (zob. M).

Litery i cyfry 1–9 pochodzą z czcionki systemu, której nie ma w kartridżu. Port ma wbudowaną czcionkę AltirraOS: wolnego zamiennika systemu dostarczanego z atari800 (licencja: kopiowanie i modyfikacja dozwolone z zachowaniem informacji o prawach autorskich, zob. `src/os_font.cpp`); z nią powstały też ślady wzorcowe. `-os plik` bierze czcionkę systemu samego Atari z obrazu ROM systemu (10 KB lub 16 KB) albo z pliku czcionki 1 KB.

| Klawisz | Działanie |
|---|---|
| Spacja | pauza |
| Góra / Dół | szybciej / wolniej, przez zatrzymanie aż do lotu wstecz (x1/4 do x16 w obie strony) |
| kółko myszy, PgUp / PgDn | przewijanie rzeki ręcznie, o ekran |
| Backspace | powrót do najnowszej linii z normalną prędkością |
| B / Shift+B | następny / poprzedni most |
| Alt+B / Alt+Shift+B | 10 mostów naprzód / wstecz |
| Ctrl+B / Ctrl+Shift+B | 100 mostów naprzód / wstecz |
| Home | do mostu 1, na początek rzeki |
| F1 / F2 / F3 | test dźwięku: wybuch / strzał / dźwięk punktów przez sterownik dźwięku gry (sam tryb demonstracyjny jest cichy, jak w oryginale) |
| F | pełny ekran |
| C | kolory NTSC / PAL |
| V | model kolorów Altirra / atari800 |
| O | tylko obraz gry / cała klatka z ramką |
| L | skalowanie: sharp / nearest / linear |
| M | dźwięk wł. / wył.: pociski czołgów, rakiety helikopterów i silnik samolotu, jak w grze (nie z oryginału, gdzie tryb demonstracyjny jest cichy); przy ruchu naprzód z każdą prędkością, silnik idzie za nią jak w samolocie, który przyspiesza lub zwalnia; wstecz dźwięk leci od tyłu, jak przewijana taśma |
| Esc | wyjście z pełnego ekranu |
| kliknięcie myszą | ukrywa wskaźnik myszy, dopóki mysz się nie ruszy (przejście na pełny ekran też) |

W porównaniu z ekranami oryginału (tryb demonstracyjny, klatki 1000–2300, z wartościami RANDOM oryginału): obszar rzeki i panel zgadzają się piksel w piksel, przewijany pasek także, poza skrajnymi kolumnami (na lewo od 24, na prawo od 359), gdzie emulator inaczej obsługuje płynne przewijanie przy krawędzi.

To, który obiekt kiedy się budzi, rozstrzyga rejestr RANDOM układu POKEY, sprzętowy licznik odczytywany w chwili zależnej od taktowania całego komputera. Port modeluje ten licznik (`src/pokey_random.h`) i uruchamia go od zegara, tak jak prawdziwy komputer uruchamia go przy włączeniu, więc każde uruchomienie jest inne, jak na prawdziwym sprzęcie; `-random N` powtarza przebieg.

## Zawartość
| Plik | Co |
|---|---|
| `src/rom.h` | obraz kartridża i nazwane funkcje dostępu do każdej tablicy, którą port z niego czyta (kształty, kolory, punkty, tablice generatora, tekst panelu...); typy obiektów |
| `src/river_gen.h/.cpp` | generator: `RiverGen::startGame(n)`, potem `nextLine(line)` — jedna linia ekranu na wywołanie |
| `src/river_draw.h/.cpp` | rysowanie linii tak jak na ekranie: odcienie brzegu, drogi przy mostach, sprite'y obiektów (`drawScreenLine`) |
| `src/palette.h/.cpp` | kolory Atari komputerów NTSC i PAL (`atariRgb`): modele Altirra i atari800 |
| `src/game.h/.cpp` | gra: pętla główna i VBI oryginału klatka po klatce — tryb demonstracyjny, klawisze konsoli, samolot, paliwo, kolizje, wybuchy, punkty, życia, gracze, śmierć i restart, koniec gry, rekordy, sterownik dźwięku |
| `src/display.cpp` | wyświetlanie rzeki tak, jak steruje nim DLI: jeden sprite użyty dla każdego obiektu, kolizje na obiekt, rysowanie obszaru rzeki |
| `src/pokey_random.h` | licznik RANDOM układu POKEY |
| `src/pokey_sound.h/.cpp` | dźwięk POKEY: cztery kanały, dzielniki i liczniki szumu, generowane takt po takcie |
| `src/status_bar.h/.cpp` | panel i przewijany pasek: zestawy znaków, panel, krok przewijania, rysowanie |
| `src/os_font.h/.cpp` | czcionka systemu: wbudowana AltirraOS albo wczytana z ROM systemu |
| `src/river_state_io.cpp`, `src/game_ram.cpp` | odwzorowanie pól generatora i gry na adresy RAM oryginału: wczytywanie, zapis i porównanie (do weryfikacji) |
| `game/river_raid.cpp` | gra (SDL3): obraz, dźwięk, klawiatura i gamepady |
| `viewer/river_view.cpp` | przeglądarka rzeki (SDL3), zob. wyżej |
| `sdl/host.h`, `host_sdl3.cpp`, `host_sdl2.cpp` | okno, obraz, dźwięk, klawiatura i gamepady dla gry i przeglądarki, na SDL3 lub SDL2; ukrywanie wskaźnika myszy |
| `tools/river_map.cpp` | generuje rzekę i zapisuje PNG tak, jak pokazuje ją gra: `river_map <rom> <gra 1..8> <linii> out.png [-pal] [-flat]` (`-flat`: tylko bitmapa, stałe kolory) |
| `tools/river_dump.cpp` | zapisuje wygenerowaną rzekę jako tekst (kolor każdego punktu, postawione obiekty) dla `tools/doc_figures.py`, który rysuje obrazki w `docs/HOW_IT_WORKS.pl.md`: `python tools/doc_figures.py rom/River_Raid.rom` (wymaga matplotlib) |
| `tools/sound_check.cpp` | porównuje sterownik dźwięku z zapisami oryginału do POKEY (`rr_oracle -vbi`) |
| `tools/sound_render.cpp` | renderuje dziennik zapisów do POKEY do WAV za pomocą POKEY portu, do porównania z dźwiękiem emulatora (`rr_oracle -wav`) |
| `tools/game_check.cpp` | porównuje grę ze śladem RAM oryginału klatka po klatce, z jego wejściem i wartościami RANDOM (i obszar rzeki z jego ekranami) |
| `tools/status_check.cpp` | porównuje panel i przewijany pasek z pełnymi ekranami oryginału (`rr_oracle -screens A:B -ram A:B`) |
| `tools/render_check.cpp` | porównuje rysowanie z ekranem oryginału (`oracle/rr_oracle -ghost -strip 20`) |
| `tools/terrain_check.cpp` | porównanie ze śladem wzorcowym (`oracle/rr_oracle -ram A:B`) |
| `tools/equivalence/` | sprawdzanie refaktoryzacji: starszy commit portu wobec kopii roboczej, klatka po klatce (zob. Kod) |

Generator czyta tablice i grafikę z ROM-u użytkownika (`rom/River_Raid.rom`); w kodzie nie ma danych gry.

## Kod
Port jest napisany tak, by dało się go czytać bez listingu oryginału pod ręką:
- pola stanu, funkcje i stałe są nazwane według znaczenia (`planeState`, `shellTimer`, `enteringSlot`, `positionObject`, `kFacingLeft`); komentarz każdego pola podaje adres RAM oryginału, a każdej funkcji adresy kodu, z którego pochodzi, więc nadal można śledzić listing (`disasm/`) i ślady;
- tablice kartridża są czytane przez nazwane funkcje w `src/rom.h` (`rom.points(type)`, `rom.objectHeight(type)`, `rom.shellRise(t)`...), a nie po adresie;
- wartości pozostają 8-bitowe z zawijaniem tam, gdzie takie są w oryginale (`lo()`), bo pozycje zawijają się wokół ekranu; tam, gdzie oryginał przenosi flagę przeniesienia 6502 z jednej sumy do następnej, mówią to wprost `addWithCarry`/`subtractWithBorrow`;
- przerwanie listy wyświetlania (`display.cpp`) to jedyna część, która pozostaje blisko instrukcji: jej taktowanie w obrębie linii skanowania decyduje, które kolizje zostaną zauważone, więc jest wykonywane z liczeniem taktów.

Zmiany, które nie powinny zmieniać zachowania, sprawdza `tools/equivalence`: buduje starszy commit (od 06ffc73) obok kopii roboczej i uruchamia obie wersje na 150 000 liniach rzeki każdej gry (NTSC i PAL), na losowej grze od włączenia i od 194 stanów ze śladów oryginału, porównując po każdym kroku cały stan, ekran i rejestry dźwięku. Tak sprawdzono zmianę nazw i przebudowę generatora, gry i wyświetlania (0 różnic), a także wszystkimi sprawdzeniami poniżej.

```
port/tools/equivalence/build.sh 06ffc73
port/build/eq/eq
```

## Jak działa generator
Każde wywołanie `nextLine()` tworzy nową górną linię ekranu:
1. `countLines`: liczenie linii (blok to 32 linie, odcinek 2 bloki, 16 odcinków do mostu), krok LFSR, nowa docelowa szerokość na początku odcinka (`chooseSectionWidth`: wyspy otwierają się i zamykają);
2. `changeIsland`: wyspa i szerokość wody płynnie zmierzają do celu;
3. `moveBank`: losowe błądzenie brzegu, zawracanie na granicach i przed nowym obiektem (`bankWouldHitObject`);
4. `startDecoration`: dom lub góra na brzegu;
5. `drawLine`: 48 bajtów linii (wypełnienia, kawałki skraju z tablicy pod `$B800`, wiersz ozdoby);
6. `scrollObjects` / `placeObject`: sloty obiektów się przesuwają, nowy obiekt jest stawiany co 16 linii (`chooseEnemy`, `positionObject`, `placeTank`).

Generowanie nie zależy ani od gracza, ani od `RANDOM`. Sprawdzono to na przebiegu wzorcowym: różne ziarna POKEY, a także przebieg bota wobec lotu bez wejścia, dają ten sam stan w każdej linii.

## Weryfikacja
- Od stanu wzorcowego: 119 758 linii całej rzeki (120 000 klatek), 0 różnic w stanie i w liniach bitmapy.
- Od zera (`startGame(1)`, bez emulatora): po 165 liniach wypełniających ekran stan zgadza się z oryginałem, potem 29 757 linii bez różnic.
- Gra bota ze zniszczonymi mostami i efektem błysku: różnice tylko w klatkach odrodzenia. Pochodzą z kodu restartu, jeszcze nieprzeniesionego.
- Sprawdzenie wyłapuje wprowadzone błędy: każde z czterech celowych uszkodzeń (warunek błądzenia, szerokość wody, rysowanie, kolor wypełnienia) dało różnice.

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/ghost_film.txt 30001 out -ghost -ram 0:30000
port/build/terrain_check rom/River_Raid.rom out/ram.bin 0 240 29760 5 -fresh 1 145
```

Rysowanie: w każdej klatce, w której nowa linia wchodzi w linię ekranu 20, linia oryginału jest porównywana z `drawScreenLine()` linii z generatora C++ (uruchomionego od zera).
- Z obiektami tam, gdzie postawił je generator: 96,8% linii identycznych w 29 000 klatek (95,3% w 120 000). Pozostałe różnią się tam, gdzie gra przesunęła obiekt.
- Z pozycjami obiektów wziętymi z działającej gry (`-ram`, x slotu, flagi, rodzaj): 98,1% identycznych; każda pozostała linia różni się tylko wtedy, gdy aktywny jest czołg na drodze (`$38`), którego rysuje logika gry. 0 niewyjaśnionych linii na 29 600.

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/ghost_film.txt 30001 out -ghost -strip 20 -ram 0:30000
port/build/render_check rom/River_Raid.rom out/strip.bin out/frames.csv -ram out/ram.bin
```
Panel i przewijany pasek: zestawy znaków zbudowane z czcionki systemu i kartridża są równe RAM-owi oryginału, stan przewijania ($3FB3/$26/$27) zgadza się w każdej klatce, a narysowane linie 180–239 są identyczne we wszystkich 1299 klatkach, poza skrajnymi kolumnami paska (zob. Przeglądarka rzeki).

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/attract_idle.txt 2300 out -screens 1000:2299 -ram 1000:2299
port/build/status_check rom/River_Raid.rom out/screens.bin out/ram.bin
```
Klatka gry w trybie demonstracyjnym (`game_check`): uruchomiony od stanu po włączeniu (`Game::startAttract`) i z wartościami RANDOM oryginału port przechodzi 19 900 klatek (do mostu 20) bez żadnej różnicy wobec RAM-u oryginału w żadnym porównywanym bajcie: stan generatora, sloty obiektów (pozycja, kierunek, rodzaj, bit kolizji), czołg, jego pocisk, rakieta helikoptera, wyniki kolizji, pamięć player-missile, linie rzeki na ekranie. Narysowany z tego obszar rzeki jest równy ekranom oryginału we wszystkich 1299 porównanych klatkach.

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/attract_idle.txt 20000 out -strip 20
port/build/game_check rom/River_Raid.rom out/ram.bin out/random.log 38 19900 -attract
```

Gra z graczem (`game_check` na przebiegach z wejściem). Od stanu trybu demonstracyjnego port dostaje wejście i wartości RANDOM oryginału i dalej działa sam, nigdy nie jest przeładowywany; w każdej klatce jego RAM jest porównywany z RAM-em oryginału (generator, sloty, samolot, paliwo, pocisk, kolizje, punkty, życia, obaj gracze, rekordy i ich bajty kontrolne, panel, przewijany pasek, pamięć player-missile, linie ekranu):
- gra bota dla jednego gracza (`start_only.txt -bot -seed 3`, 8 950 klatek: tryb demonstracyjny, START, 4 samoloty, mosty, śmierci i restarty, koniec gry, znowu tryb demonstracyjny): identyczna w każdej porównanej klatce;
- gra dla dwóch z SELECT i klawiszem pauzy (`two_players.txt`, do końca gry w klatce 3 007): identyczna do końca gry (jedna klatka, zob. niżej);
- obszar rzeki narysowany przez port (samolot, pocisk, wrogowie, wybuchy, czołg, pociski) jest równy ekranom oryginału we wszystkich 1 565 porównanych klatkach gry bota.

Gdzie wynik oryginału zależy od taktów procesora:
- DLI (`display.cpp`) działa z liczeniem taktów tak, jak emulator wzorcowy rozkłada linię skanowania: na początku pobrania player/missile i listy wyświetlania, procesor do taktu 28, potem rysowana jest cała linia (wraz z kolizjami), potem pobrania danych ekranu, WSYNC na takcie 106. Wiadomo więc, czy odczyt rejestru kolizji widzi już bieżącą linię: pocisk dotykający obiektu tylko w linii odczytu liczy się na moście (ta ścieżka czyta po takcie 28), a gdzie indziej nie, jak w oryginale.
- Migawka RAM-u jest robiona na końcu klatki; zwykle pętla główna oryginału czeka wtedy na następną klatkę (AE99–AE9E). Zaraz po starcie gry, gdy rzeka jest wypełniana na nowo po 2 linie na klatkę, czasem wciąż rysuje drugą linię (1 klatka na wypełnienie). `game_check` zgłasza takie klatki osobno i ich nie porównuje; następna klatka jest znowu identyczna.
- Nie zamodelowane: koniec gry. Logika gry w pętli głównej zaczyna się w linii 160 i dzieli procesor z DLI i VBI (linia 248). W klatce końca gry sprawdzenie rekordu i jego bajty kontrolne (sub $B261, $B298) zajmują tysiące taktów, więc `sub_B21A` ustawia licznik klatek na $FF przed VBI, po nim albo (gra dla dwóch) dopiero w następnej klatce, zależnie od taktów wszystkiego przed nim w tej klatce. Port robi to przed VBI; wtedy licznik klatek może różnić się o jeden, co przesuwa następny tryb demonstracyjny o klatkę. Odtworzenie tego wymagałoby liczby taktów całej pętli głównej w tej klatce (wraz z obsługą przerwań systemu).

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/start_only.txt 9000 out -bot -seed 3
port/build/game_check rom/River_Raid.rom out/ram.bin out/random.log 38 8950 -attract
```

Dźwięk. Sterownik dźwięku (VBI B2A5–B3D8, `Game::soundDriver`) uruchomiono na RAM-ie, jaki miał oryginał na początku każdego VBI gry bota (2989 klatek, 2199 z dźwiękiem: silnik, ostrzeżenie o paliwie, wybuchy, strzały, punkty, pocisk czołgu): każda wartość AUDF/AUDC jest równa temu, co zapisał oryginał. Model POKEY (`src/pokey_sound.cpp`) porównano z dźwiękiem atari800 dla tych samych zapisów do rejestrów: ta sama głośność w czasie (korelacja obwiedni 0,95), te same czyste tony na tej samej wysokości, szum o tym samym kształcie widma (kosinus wygładzonych widm 0,94); wzór szumu próbka po próbce się różni, bo zależy od tego, gdzie akurat są liczniki szumu.

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/start_only.txt 3000 out -bot -wav -vbi
port/build/sound_check rom/River_Raid.rom out/vbi.bin out/pokey.log out/random.log
port/build/sound_render out/pokey.log port.wav
```

## Jeszcze nie zrobione
- Samo włączenie (A000–A17C: pierwsze 38 klatek przed trybem demonstracyjnym) jest rysowane jako pusty ekran; `Game::powerOn` nie jest jeszcze dokładny.
- Komputer PAL (`-pal`): generator go zna, lista wyświetlania i taktowanie nie.
- Kolory idą za modelami wyjścia GTIA z emulatorów (domyślnie Altirra, atari800 pod V; oba sprawdzone bajt w bajt z kodem palety samych emulatorów przy ich ustawieniach domyślnych, atari800 rozciągnięty z poziomów wideo 16–235 do 0–255), a nie za paletą zmierzoną na prawdziwym telewizorze; to, jak jasno wygląda obraz, zależy od ustawień telewizora w tym samym stopniu co od komputera.
