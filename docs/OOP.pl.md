# Przepisanie na kod obiektowy (gałąź `oop`)

[English](OOP.md) | [Русский](OOP.ru.md) | [Українська](OOP.uk.md) | [Deutsch](OOP.de.md) | Polski | [Italiano](OOP.it.md) | [Español](OOP.es.md) | [中文](OOP.zh.md) | [日本語](OOP.ja.md)

Gałąź `oop`, od `dli_removed`. Cel: gra portu jako obiekty z nazwami (rzeka, samolot, jego
pocisk, czołg i jego pocisk, rakieta helikoptera, statki, helikoptery, odrzutowce, balony, bazy
paliwowe, mosty i wybuchy w slotach, gracze i panel, dźwięk) zamiast RAM oryginału ułożonej według
adresów, przy czym gra ma grać dokładnie tak jak w `dli_removed`, klatka po klatce i bit po
bicie. Porty na ZX Spectrum idą za `dli_removed` i są z nią porównywane; `oop` to przepisanie tej
samej gry, a nie nowa gra.

## Zasady

- **Ta sama gra.** W każdej klatce ten sam stan, te same rejestry dźwięku, ten sam obraz co w
  `dli_removed`. Bez float: pozycje, prędkości, paliwo i liczniki czasu zostają bajtami z
  zawijaniem, ze swoimi bajtami części ułamkowej; punkty, mosty i rekordy zostają w BCD.
  Osobliwości też zostają: kolejność rzeczy w klatce, sloty i ich kolejność, obiekt zmieniający
  się w inny, ile razy i kiedy czytany jest RANDOM, klatki resetu, zamiana dwóch graczy.
- **Sprawdzane po każdym kroku** przez `tools/trace/compare.sh` (niżej): wszystkie przebiegi
  takie same, inaczej krok nie jest zrobiony.
- **Jedna rzecz naraz**, każda osobnym commitem: nowy typ lub klasa najpierw opakowuje stan
  takim, jaki jest, potem przenosi się do niej kod, potem stary stan odchodzi.
- **RAM oryginału pozostaje dostępna**: `saveToRam` / `loadFromRam` / `diffWithRam`
  (`ram_map.h`, `game_ram.cpp`) zachowują układ oryginału, dla `game_check` i dla sprawdzeń
  portów na ZX (`nxcheck`, `tscheck`), cokolwiek klasy trzymają w środku.

## Sprawdzenia

`port/tools/game_trace` pisze linię na klatkę o tym, co robi gra, po nazwach: samolot, pocisk,
czołg, pocisk czołgu, rakieta, każdy slot, punkty, życia, drugi gracz, rekordy, generator rzeki,
rejestry dźwięku, wyniki kolizji i skrót obrazu (rzeka i panel). Gra autopilot z
`display_bench`; bez `-ghost` samolot się rozbija i kończy mu się paliwo, więc życia, powroty i
końce gry są w przebiegach.

`port/tools/trace/runs.txt` wymienia 17 przebiegów, 700 000 klatek: tryb demonstracyjny, gry 1–8
na NTSC i PAL, dwóch graczy, `-wild` (sterowanie losowe, także nad brzegami), długie przebiegi
`-ghost` od mostów 60 i 100. `golden.txt` zawiera skrót linii każdego przebiegu tak, jak gra je
`dli_removed`, zrobiony przy commicie z `REFERENCE`.

```
port/tools/trace/compare.sh <rom> [build dir] [runs...]
```

odtwarza je (~2 minuty) i porównuje. Przy różnicy buduje commit z `REFERENCE` w tymczasowym
worktree, odtwarza tam przebieg i wypisuje pierwszą różniącą się klatkę, obie linie.
`tools/fixed_check` sprawdza typy liczbowe (niżej) dla każdego wejścia wobec kodu, który
zastępują.

## Zrobione

1. `game_trace`, przebiegi, wzorzec.
2. `PokeyRandom` bez swoich ~1 MB tablic: skok licznika o n cykli jest liniowy nad GF(2), więc
   składa się go z 17 odwzorowań (2^k kroków, 1,2 KB), a długości używane przez grę są trzymane
   jako odwzorowania czytane po 4 bity, jak w portach na ZX Spectrum (5 odczytów tablic na
   odczyt). Sprawdzone wobec starej klasy na 6 milionach odczytów.
3. Wyświetlanie tylko jako geometria: modele DLI (`displayBeam`, `displaySimple`, tryby
   `RR_DISPLAY`, `RR_GEOMCHECK`, klasa `Beam`, przełączniki eksperymentów) usunięto z tej
   gałęzi; zostają w `dli_removal`. `display.cpp`: 276 linii zamiast 1331.
4. `src/fixed.h`: `Fixed8` (bajt i jego bajt części ułamkowej, dodawanie / odejmowanie z
   przeniesieniem), `Bcd<N>` (liczniki BCD: dodawanie z przeniesieniem w górę, porównanie jak w
   oryginale), `addBcd`.
5. Pocisk (`bullet.h`) i samolot (`plane.h`: sterowanie, prędkość, paliwo) jako własne typy.
6. Bez pamięci sprite'ów: wiersze sprite'ów wynikają z ich stanu (`Game::spriteRow`: każdy
   kształt z ROM w swoim wierszu, rakieta jeden wiersz, pocisk 8), pokazywane od początku każdej
   klatki, aż wyczyści je reset. Współdzielenie tej pamięci w oryginale (wiersze pocisku czyszczą
   wiersze rakiety) cofa kopia w następnej klatce, zanim ktokolwiek je przeczyta, więc gra jest
   ta sama. `saveToRam` nadal zapisuje tę pamięć tak, jak ma ją oryginał.
7. Czołg, jego pocisk i rakieta helikoptera (`tank.h`, `tank.cpp`): po strukturze na każde,
   trzymane przez rzekę (przewijają się razem z nią; od kroku 13 przez `Objects`), z ich ruchem
   (`Tank::drive`, `Shell::move`, `Missile::move`).
8. Obiekty w slotach (`objects.h`, `objects.cpp`): klasa na każdy rodzaj zachowania
   (`Explosion`, `Jet`, `Helicopter`, `Balloon`, `Ship`, `Bridge`, `FuelDepot`). Slot trzyma
   bajty oryginału; jego rodzaj jest zarazem klatką animacji i zmienia się po trafieniu, więc
   `objectKind(kind)` w każdej klatce daje obiekt klasy, która wie, co ten rodzaj robi: jego
   animację, ruch (odrzutowiec przez ekran; helikoptery, balony i statki tam i z powrotem po
   obudzeniu, zawracając przy krawędzi), czy wyświetlanie sprawdza jego dotknięcie krawędzi.
9. Gracze (`players.h`): `PlayerData` (drugi gracz) z mostami `Bcd<2>` i punktami `Bcd<3>`, te
   same typy dla aktywnego gracza, `HighScores` (tabela i jej bajty kontrolne); panel
   (`panel.h`): jego 60 znaków z `print` (cyfry BCD), `showLives`, `markPlayers`,
   `showGameNumber`.
10. Dźwięk (`sound.h`, `sound.cpp`): `Sounds` trzyma to, o co prosi gra (sygnał startu, dźwięk
    zdarzenia i jego czas, wybuch, strzał, premia, rakieta, opadający ton po rozbiciu), a jego
    `drive()` to sterownik (B2A5–B3D8), któremu samolot, pauza, tryb demonstracyjny i pocisk
    czołgu są podawane przez `SoundContext`; `PokeyRegs` też się tam przeniósł.
11. Rzeka na ekranie (`river.h`, `river.cpp`): jej linie, te same bez ozdób, ich odcinki, adres
    górnej linii w pamięci ekranu oryginału i stany generatora dla małych zapisów; `push()`
    dodaje następną linię generatora, `rebuild()` tworzy je od nowa przy wczytaniu stanu.
12. Bez `GameState`: jego pola stały się własnymi polami `Game` (`eachState()` wymienia je do
    zapisu, wczytania i resetu).
13. Rzeka oddzielnie od obiektów (`world.h`, `world.cpp`): `RiverGen` tworzy tylko rzekę
    (`RiverState`: brzegi, wyspy, odcinki, LFSR); sloty, czołg, jego pocisk i rakieta to
    `Objects`, który po każdej linii stawia nowe obiekty (`afterLine`, $ABE1–$AE93) i mówi
    generatorowi, gdzie jest obiekt właśnie wchodzący u góry (`BankObstacle`), by brzeg go
    omijał. `World` trzyma jedno i drugie i prowadzi je linia po linii; jego `State` to to, co
    zachowują zapisy stanu i stany linii rzeki.
14. Eksport do RAM z klas (`ram_map.h`, `ram_map.cpp`): każda klasa wymienia swoje bajty z ich
    adresami w swoim `mapRam(RamMap &)` (samolot, pocisk, dźwięki, kolizje, gracze, rekordy,
    panel, przewijany napis, rzeka, sloty, czołg, jego pocisk i rakieta, górna linia ekranu);
    `World` i `Game` składają listy razem, a `RamMap` je wczytuje, zapisuje i porównuje. To, co
    jest wyliczane, a nie trzymane (pamięć sprite'ów, linie ekranu według listy wyświetlania),
    zostało w `game_ram.cpp` i `River`.
15. Dokumenty: tabele plików w README i ich części o kodzie, generatorze i weryfikacji, HANDOFF
    (gałąź), NOTES (zapisy stanu), HOW_IT_WORKS (kolizje), we wszystkich językach; komentarze w
    kodzie źródłowym, które jeszcze mówiły o DLI albo o tym, że obiekty trzyma rzeka.
16. Zakończenie: wszystkie 17 przebiegów takie same (700 000 klatek), także na buildzie z
    AddressSanitizer, UndefinedBehaviorSanitizer i asercjami biblioteki standardowej, bez
    żadnego komunikatu; ten dokument we wszystkich językach.
17. `game.cpp` pocięty na pliki według tematów, klasa nietknięta: `game.cpp` (klatka w
    kolejności oryginału), `game_plane.cpp` (samolot, wybuchy, punkty), `game_players.cpp` (gry
    i gracze, panel), `game_reset.cpp` (włączenie, tryb demonstracyjny, nowa gra, reset,
    następny samolot), `game_state.cpp` (zapisy stanu); to, co wspólne, jest w
    `game_internal.h`.
18. Partia (`match.h`, `match.cpp`): `Match` trzyma numer gry, czyja kolej, mosty, punkty i
    życia obu graczy, `respawned` i rekordy, z `setUp` (sub $AF80), `selectNext`, `swapPlayers`
    (dostaje stan rzeki: poziom i punkt restartu też przechodzą do drugiego gracza),
    `updateHighScore`, `addPoints` (A547: punkty, premiowy samolot, koniec gry po 999 999) i
    liczbami na panelu (w trybie demonstracyjnym niewyświetlanymi, o czym mówi mu `Game`). Jego
    bajty idą w kolejności dawnych pól, więc zapisy stanu się nie zmieniły. Przy okazji
    znalezione: `World::State` miał dwa bajty wyrównania, zapisywane z tym, co akurat było w
    pamięci; teraz to jawne bajty zerowe i ten sam stan zawsze zapisuje się tymi samymi bajtami.
19. Pola `Game` zamknięte (`name_`). Na zewnątrz stałe gettery (`world()`, `plane()`, `match()`,
    `pokey()`...), `output()` (co klatka pokazuje i gra: panel, paliwo, przewijany napis, logo,
    wyłączony ekran, rejestry dźwięku) i kilka nazwanych działań zamiast zapisu do pól:
    `setRiver`, `keepLineStates`, `skipState` / `setSkipState` (kotwice przeglądarki rzeki),
    `playSound` i `stayAwake` (przeglądarka), `ghost` (tryb ducha w narzędziach), `setPokey`
    (`sound_check`). `sound_check` wczytuje bajty, które czyta sterownik, przez `mapRam` — te
    same bajty co wcześniej.
20. Konsola (`console.h`, `console.cpp`): `Console` trzyma licznik klatek, flagi i zegar trybu
    demonstracyjnego, czas bezczynności, pauzę, opóźnienie SELECT, `powerOnDone` i `screenOn`, z
    `keys` (A19E-A1F5: aktywność, pauza, wyłączony ekran), `attractTimerTick` (A2F5),
    `countFrame` (liczniki VBI, także dla `skipFrame`) i `selectRepeat` (A699). Tam, gdzie
    kolejność klatki miesza je z resztą (nowa gra, wybór gry, koniec gry, VBI na PAL przed
    końcem gry), `Game` nadal sam ustawia pola. Te same bajty w tej samej kolejności, więc
    zapisy stanu się nie zmieniły; pauzę, SELECT, OPTION, START i gaśnięcie ekranu sprawdzono
    wobec poprzedniego commitu na 390 000 klatkach z takimi klawiszami.
21. Pętla główna jako nazwane kroki, w kolejności oryginału i bez przestawiania: `mainLoop` to
    `console_.keys` (A19E-A1F5), `wakeObject` (A1FF), `driveTank` (A21B), `moveObjects` (A26C),
    `attractTimerRunsOut` (A2F5), `readStick` (A312), `updatePlane` (A33F); `mainLoopEnd`, od
    którego toczy się dalej powrót, to `moveMissile` (A5B7), `moveShell` (A607), `consoleKeys`
    (A673), `scroll` (A6DE). Krok, który zaczyna reset, zwraca true i pętla się na nim
    zatrzymuje, jak wcześniej. Na koniec wszystkie 17 przebiegów takie same, także na buildzie z
    sanitizerami.

## Dalej

Nic nie jest zaplanowane. `Game` jest dyrygentem klatki; to, co trzyma, ma własne klasy
(`World`, `Match`, `Console`, `Plane`, `Sounds`...). Kolejne zmiany sprawdza się tak samo:
`compare.sh -quick` po każdym kroku, wszystkie 17 przebiegów na końcu.
