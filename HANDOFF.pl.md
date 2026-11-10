# River Raid (Atari 8-bit) → port na C++: stan projektu

[English](HANDOFF.md) | [Русский](HANDOFF.ru.md) | [Українська](HANDOFF.uk.md) | [Deutsch](HANDOFF.de.md) | Polski | [Italiano](HANDOFF.it.md) | [Español](HANDOFF.es.md) | [中文](HANDOFF.zh.md) | [日本語](HANDOFF.ja.md)

Ten plik służy do podjęcia pracy na dowolnym komputerze lub w nowym czacie. Zacznij nową sesję od „przeczytaj HANDOFF.pl.md i docs/NOTES.pl.md”.

## Cel
Port River Raid (Activision, Atari 800, 1983, autorka Carol Shaw) na C++ dla Windows, Linuksa i macOS:
- logika gry identyczna z oryginałem, czytelna dla zwykłego programisty bez znajomości 6502;
- identyczny dźwięk: ten sam sterownik dźwięku plus rdzeń POKEY;
- warstwa wieloplatformowa: SDL3 (SDL2 jako opcja, dla UWP na Xboksie);
- żadne oryginalne dane nie są rozpowszechniane; port czyta ROM użytkownika w czasie działania (postanowione: żadnych danych ROM w kodzie).

## Stan
Gra jest gotowa i grywalna: `port/game/river_raid.cpp` (tryb demonstracyjny, gry 1–8, jeden i dwóch graczy, samolot, paliwo, wrogowie, czołgi, mosty, punkty, życia, rekordy, pauza, dźwięk). Zbudowana i sprawdzona w grze na Windows, macOS (Apple M2) i Linuksie. Porównana klatka po klatce z RAM-em oryginału w przebiegach bota, gry dla dwóch, bezczynności i długich przebiegach; sterownik dźwięku zapisuje do POKEY te same wartości. Wiersz poleceń i klawisze: `port/README.pl.md`, „Gra”. Jak działa gra, dla każdego: `docs/HOW_IT_WORKS.pl.md`.

## Czego potrzebujesz
- Całego folderu projektu.
- Własnego ROM-u `River_Raid.rom`: 8192 bajty, bez nagłówka, ładowany pod $A000–$BFFF. Nie ma go w repozytorium; połóż go w `rom/River_Raid.rom`.

## Układ
| Ścieżka | Co |
|---|---|
| `port/` | port na C++: gra, przeglądarka rzeki, narzędzia porównawcze; zob. `port/README.pl.md` (budowanie, opcje, klawisze, zawartość, weryfikacja) |
| `docs/HOW_IT_WORKS.pl.md` | jak działa gra, dla każdego: rzeka, wrogowie, mosty, kolizje, dlaczego robi się trudniej |
| `docs/NOTES.pl.md` | odkrycia z adresami |
| `disasm/river_raid_a8.asm` | pełny listing z nagłówkiem (punkty wejścia, klatka, zmienne) |
| `tools/dis6502.py` | deasembler: `python dis6502.py <rom> <out.asm> B500 B2A5 B485` |
| `tools/check_lfsr.py`, `tools/verify_trace.py` | sprawdzanie udokumentowanych reguł na śladach wzorcowych |
| `tools/level_map.py` | obrazy całej rzeki, odcinek po odcinku |
| `tools/doc_figures.py` | obrazki w `docs/HOW_IT_WORKS.pl.md` (`docs/img/`), z `port/tools/river_dump` |
| `oracle/` | przebieg wzorcowy na libatari800: `rr_oracle.c`, `build.sh`, `README.md`, skrypty wejścia, autopilot `-bot`, niezniszczalny samolot `-ghost` |
| `docs/oracle_demo.png` | klatki z przebiegu wzorcowego |

## Ustalone fakty (wszystko sprawdzone)
1. **Procedury obsługi.** Pętla główna A000–AF5A, jeden przebieg = jedna klatka (synchronizacja po VCOUNT $50/$78). VBI $B2A5 = sterownik dźwięku POKEY i liczniki. DLI $B500 = multipleksowanie sprite'ów linia po linii i odczyt kolizji. IRQ $B485 = zaślepka.
2. **Generator rzeki.** Programowy 24-bitowy LFSR (`sub_B183`, $AA/$AB/$AC), jeden krok na linię; wszystko (brzegi, wyspy, ozdoby, obiekty) pochodzi z niego i z poziomu. Nie zależy ani od gracza, ani od RANDOM. Rzeka nie powtarza się przez 16,8 mln mostów (okres LFSR: 16 766 977 kroków, 993 kroki na most). C++ (`port/src/river_gen.cpp`) zgadza się bajt w bajt: 119 758 linii i starty od zera.
3. **Punkty startowe.** Gry 1–8 zaczynają się przy mostach 1/5/20/50 tej samej rzeki: tablice $B494–$B4A8 zawierają tam LFSR, brzeg, poziom i licznik mostów; generator uruchomiony od startu gry 1 dochodzi dokładnie do tych wartości. Port wypełnia tę samą tablicę przebiegiem generatora dla `-seed N` (inna rzeka) i `-bridge N` (start od dowolnego mostu).
4. **Trudność.** Poziom (= most, do którego lecisz, do 80, potem na zmianę 79/80) przesuwa progi, z którymi porównywane są liczby losowe: wrogowie w środku bloku, mniej stacji paliw do poziomu 46 (47 na PAL), balony od 3, myśliwce od 5, czołgi na brzegu od 7, helikoptery z rakietami od 13, szersze zakręty. Tabela w `docs/HOW_IT_WORKS.pl.md`.
5. **RANDOM ($D20A).** Logika gry czyta go tylko w A205: co 16 klatek budzi się losowy slot (0–10) (bit $40 w `$42,X`). B362/B3D0: szum dla dźwięku. Wartość zależy od taktu odczytu; port modeluje licznik (`src/pokey_random.h`), uruchamiany od zegara jak przy prawdziwym włączeniu; `-random N` powtarza przebieg.
6. **Kolizje** pochodzą z rejestrów kolizji GTIA, które DLI czyta po pasie każdego obiektu; czy dotknięcie w linii odczytu się liczy, zależy od taktu. Port wykonuje DLI z liczeniem taktów (`src/display.cpp`, nie co do taktu dokładne taktowanie atari800).
7. **PAL/NTSC.** Jeden kartridż dla obu: bit PAL ($D014 → $BF) wybiera wariant listy wyświetlania ($B77A) i limit stacji paliw ($CB = $2E/$2F). Nic więcej się nie zmienia, więc na PAL cała gra działa z 50 klatkami na sekundę, około 17% wolniej. Brytyjska wersja kasetowa (`.cas`, 2 pliki: loader pod $0A00, gra pod $1000–$31FF) to ten sam kod przeniesiony do RAM-u, z innym zabezpieczeniem przed kopiowaniem i „1982, 1983, 1984” w przewijanym pasku.
8. **Czcionka** jest kopiowana z ROM-u systemu ($E000/$E100/$E300); port ma wbudowaną czcionkę AltirraOS, `-os` wczytuje czcionkę Atari.
9. **Przebieg wzorcowy.** atari800 uruchamia POKEY od zegara; `rr_oracle -seed` (domyślnie 0) sprawia, że przebiegi są identyczne co do bajtu. Budowanie atari800 kończy się błędem z `-Werror` na redefinicji ULONG; `build.sh` buduje z `CFLAGS="-g -O2 -DNETSIO"`. Sprawdzony commit atari800: 98cc2dc.
10. **Determinizm.** Rzeka zależy tylko od mostu (993 kroki LFSR od mostu do mostu; powtarza się po 16,8 mln mostów), obiekty tylko od ostatnich ~200 klatek. Dlatego do dowolnej chwili można dojść, przepuszczając samą rzekę, a potem kilkaset całych klatek; tak przewija przeglądarka. Szczegóły: `docs/NOTES.pl.md`, „Determinizm i skoki”. Zapisy stanu w grze korzystają z tego samego: pamięć ekranu jest odtwarzana ze stanu generatora, więc zapis zajmuje około 0,5 KB.

## Ograniczenia środowiska
- `oracle/build.sh` buduje się na Linuksie, macOS, WSL i MSYS2 (przechwytywane funkcje emulatora są przemianowywane przy kompilacji, bez GNU `--wrap`); port na C++ buduje się zwykłym CMake (MSVC, Xcode, gcc).
- ROM-u nie ma w repozytorium (`.gitignore`); połóż swój zrzut w `rom/River_Raid.rom`.
- Kod jest wcinany tabulatorami, także linie kontynuacji (`.editorconfig`). Dokumentacja: `X.md` po angielsku, `X.ru.md` po rosyjsku, a także `X.uk.md`, `X.de.md`, `X.pl.md`, `X.it.md`, `X.es.md`, `X.zh.md`, `X.ja.md`; `tools/doc_languages.py` zapisuje linię języków pod każdym tytułem, podpisy obrazków są w `tools/doc_figures_i18n.py`. Zmiana dokumentu trafia do wszystkich języków.

## Metoda
Regułę odczytuje się z listingu, a potem sprawdza na śladzie wzorcowym (`tools/verify_trace.py`, potem narzędzia porównawcze C++). Same ślady służą tylko do znalezienia, gdzie szukać. Zmiany, które nie powinny zmieniać zachowania (zmiana nazw, przebudowa), sprawdza się `port/tools/equivalence` wobec starszego commitu: wymagane 0 różnic, plus wszystkie sprawdzenia z `port/README.pl.md`, „Weryfikacja”.

## Decyzje
- Port jest pisany w C++, a nie jako nakładka na emulator.
- Weryfikacja porównuje RAM portu z wzorcowym `ram.bin` klatka po klatce; wartości RANDOM do niej pochodzą z wzorcowego `random.log`.
- Żadnych danych ROM w kodzie: tablice kartridża są czytane przez nazwane funkcje (`port/src/rom.h`).
- Domyślne kolory: PAL, model Altirra; NTSC i model atari800 pod klawiszami.
- Okno pokazuje obraz gry (364×200 z całym przewijanym paskiem), a nie całą klatkę z czarną ramką (`-border`, O); skalowany ostro o całkowitą wielokrotność, potem płynnie (`-filter`, L).
- Wyścig taktów przy końcu gry (czy licznik klatek zostanie wyzerowany przed, czy po VBI tej klatki, zależy od taktów pętli głównej, zob. `port/README.pl.md`, „Weryfikacja”) świadomie nie jest odtwarzany.
- Na ekranie 60/120 Hz gra wiąże się z ekranem (60,00 zamiast 59,92 klatek na sekundę), dźwięk jest pod to przeliczany.

## Otwarte pytania
- Samo włączenie (A000–A17C, pierwsze 38 klatek) jest rysowane jako pusty ekran; `Game::powerOn` nie jest dokładny.
- PAL: lista wyświetlania i taktowanie 50 Hz nie są odtwarzane (`-pal` zmienia tylko limit stacji paliw i kolory).

## Następne kroki
1. Opcjonalnie: taktowanie PAL (50 Hz, lista wyświetlania PAL) dla tych, którzy pamiętają grę z europejskiego telewizora.
2. Opcjonalnie: zapis i odczyt stanu (cały stan jest w `GameState` + `RiverState`, proste dane).
3. Opcjonalnie: wersja UWP/Xbox na SDL2 (`-DRR_SDL2=ON`).
