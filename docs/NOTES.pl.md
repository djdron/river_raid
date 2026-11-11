# River Raid (Atari 8-bit) — odkrycia

[English](NOTES.md) | [Русский](NOTES.ru.md) | [Українська](NOTES.uk.md) | [Deutsch](NOTES.de.md) | Polski | [Italiano](NOTES.it.md) | [Español](NOTES.es.md) | [中文](NOTES.zh.md) | [日本語](NOTES.ja.md)

Obraz: 8192 bajty, bez nagłówka CART, ładowany pod $A000–$BFFF. Kod ~5,9 KB (2941 instrukcji), dane ~2,3 KB.

## Punkty wejścia
| Adres | Co | Jak znalezione |
|---|---|---|
| $A000 | start + pętla główna klatki | trailer kartridża ($BFFA) |
| $B2A5 | VBI (VVBLKI $0222): dźwięk POKEY, liczniki klatek, rejestry na klatkę | zapis do $0222/$0223 w A0D1 |
| $B500 | DLI (VDSLST $0200): multipleksowanie sprite'ów linia po linii | zapis do $0200/$0201 w A0C7 |
| $B485 | zaślepka IRQ | zapis do $0216/$0217 |

## Klatka
- `L_AE99`: czeka na VCOUNT < $50 (zaczęła się nowa klatka), przygotowuje listę wyświetlania i sprite'y.
- `L_A197`: czeka na VCOUNT >= $50, czyta joystick (PORTA, TRIG0/1), wykonuje logikę gry.
- `A18D`: czeka na VCOUNT = $78, potem zapisuje NMIEN ($C0 = DLI+VBI, $40 = tylko VBI).
- Jeden przebieg pętli = jeden krok logiki, więc model „update() raz na klatkę” jest poprawny.

## Co ważne dla portu
1. **Generator losowy.** `RANDOM` ($D20A, wielomian POKEY) jest czytany w 3 miejscach. Logika gry tylko w A205: co 16 klatek wybierany jest slot wroga (0–10) i w jego flagach ustawiany bit $40. Dwa pozostałe odczyty (B362, B3D0) dają tylko szum dźwięku. Wartość zależy od taktu odczytu (17-bitowy wielomian POKEY chodzi z zegarem komputera). Port modeluje licznik (`port/src/pokey_random.h`) i uruchamia go od zegara, jak przy prawdziwym włączeniu; do porównania klatka po klatce wartości oryginału biorą się ze śladu wzorcowego (`random.log`).
2. **PAL/NTSC.** Bit PAL jest czytany z $D014 (A06A), przechowywany w $BF i wybiera wariant listy wyświetlania ($B77A, kopiowany do listy wyświetlania pod $3FA9), limit stacji paliw `$CB` ($2E/$2F) i rozgałęzienia w B135/B14F/B741. Nic nie wyrównuje liczby klatek: na PAL gra działa z 50 klatkami na sekundę, około 17% wolniej. Osobnej wersji PAL nie było: brytyjska kaseta to ten sam kod (zob. „Brytyjska wersja kasetowa”). `-pal` w porcie ustawia limit stacji; lista wyświetlania PAL i 50 Hz nie są jeszcze odtwarzane.
3. **Czcionka z systemu.** Kod kopiuje zestaw znaków z ROM-u systemu ($E000/$E100/$E300) do $0600–$09FF. To zewnętrzna zależność: trzeba użyć własnej czcionki albo ROM-u użytkownika (Altirra OS jest wolny).
4. **Powiązanie z wiązką.** Sprite'y rysuje jądro DLI z WSYNC w każdej linii; w C++ staje się to programowym rendererem pracującym na stanie klatki.
5. **Dźwięk.** Sterownik żyje w całości w VBI ($B2A5–$B484): 4 kanały POKEY, AUDF/AUDC liczone z $C6, $7A–$7F, $50, $24 i tablic $B4AC/$B4C0/$B4E1. Odtworzenie tego sterownika plus rdzeń POKEY wystarcza do dokładnej zgodności.
6. **Styl kodu.** Monolityczny, zmienne są ponownie używane przez różne podsystemy, flagi są spakowane w bity. Pełne odwzorowanie wymaga śladów z emulatora.

## Tablice danych (z odwołań w kodzie)
Wszystkie są czytane przez nazwane funkcje w `port/src/rom.h`.
- $B4AC (4): barwa silnika (AUDC) według drążka góra/dół; $B4C0 (17): wysokość gwizdu pocisku czołgu; $B4E1: dźwięk dodatkowego życia.
- $B4D1: kształty czołgu (pary wskaźników, czytane przez $A1/$A2 w AEB9).
- $B776 (4 bajty) jest kopiowany do $3F00 (początek listy wyświetlania); $B77A (15 bajtów, na PAL następne 15) do $3FA9, reszta listy wyświetlania.
- $BB70 (60 bajtów): tekst panelu, kopiowany do $1000; $BEEE (64): napis Activision → $06C0; $BF2E (112): symbole panelu → $0818; $BF9E (24): „P1”/„P2” → $08D0; $BFB6 (48): cyfry „1982, 1983” → $07D0.
- $BAAF: następny rodzaj czołgu; $BAC5: kolory pocisku; $BB24: następny rodzaj obiektu (animacja); $BB40: przesunięcia kształtów obiektów od $B900.

## Przebieg wzorcowy (zrobione)
`oracle/rr_oracle` uruchamia oryginał na libatari800 i zapisuje RAM każdej klatki, każdy zapis do POKEY i każdy odczyt RANDOM (zob. `oracle/README.pl.md`).
- Gra zaczyna się klawiszem START; ogień na ekranie tytułowym jej nie uruchamia.
- atari800 uruchamia generator POKEY od zegara; z `-seed` przebiegi są identyczne co do bajtu.
- Odczyty RANDOM: linia ~249 to dźwięk z VBI, linie 190–192 i 241 to logika gry (A205). Dla C++ oznacza to, że w trybie weryfikacji losowe wartości gry bierze się z `random.log`, a nie emuluje.

## Generator poziomu (potwierdzony śladem)
- Rzekę i rozmieszczenie obiektów prowadzi programowy 24-bitowy LFSR w `sub_B183` na $AA/$AC/$AB. Wywoływany z A756 i A767.
- Jeden krok: `fb = bit7(AB) ^ bit2(AB)`, potem przesunięcie łańcucha w lewo AA←fb, AC←bit7(AA), AB←bit7(AC). Model w Pythonie wyjaśnia 265 z 268 zmian w śladzie; pozostałe 3 to przeładowania.
- Wartość początkowa AA/AB/AC to $E3/$D4/$F7. Jest przechowywana w $66–$68 i przywracana w A3C8 po starcie i po śmierci (restart od mostu).
- Przy różnych ziarnach POKEY przez 1000 klatek $AA–$AC, $66–$68, typy obiektów ($0500…) i ich X ($0516…) są identyczne. Różnią się tylko flagi slotów $42–$4C (bit $40 z A205), bajt roboczy $07 i stos.
- Wniosek: poziom jest zawsze ten sam. `RANDOM` wpływa tylko na zachowanie wrogów (który dostanie flagę $40). Na początku gry ruch wrogów wydaje się wyłączony (sprawdzenie $60 w A297), więc w krótkim przebiegu tego nie było widać.

## Zmienne (etap 2)
Status: **sprawdzone** — reguła sprawdzona przez `tools/verify_trace.py` na śladach wzorcowych bez wyjątków. **z kodu** — wartość odczytana z listingu bez osobnego sprawdzenia śladem. **hipoteza** — przypuszczenie do potwierdzenia.

### Gracz
| Adres | Co | Status |
|---|---|---|
| $57 | X samolotu (jednostki HPOS; na ekranie wzorcowym środek ≈ 2·X−55 px) | sprawdzone: zmienia się tylko przy wychylonym drążku |
| $58 | część ułamkowa X; przy przepełnieniu $57 zmienia się o ±1 (A462–A493) | z kodu |
| $59 | prędkość: 128 normalna, do ~142 w górę, do ~114 w dół, po puszczeniu z powrotem 128 | ze śladu, bez osobnego sprawdzenia |
| $5B:$5C | paliwo, 16 bitów, $FFFF to pełny bak. Kod A3E9–A453, w każdej klatce lotu: nad stacją `$5B += 2` (to `ADC #1` z przeniesieniem pozostałym po `CMP #$0E`), z limitem $FFFF; potem zawsze `$5C −= $20` z pożyczką z `$5B` (−1 w $5B co 8 klatek). Jeśli przy pożyczce $5B jest 0, samolot ginie. Zużycie na klatkę nie zależy od prędkości. Wskazówka paliwa HPOSM1 = $5B/8 + $5C w DLI w B527 (tam $5C to stała `#$5C`, a nie zmienna) | sprawdzone, także tankowanie: +480 na klatkę, limit $FFFF |
| $1D | slot, z którym zderzył się samolot; bit 7 = żaden | sprawdzone przez tankowanie |
| $7A / $7B | zdarzenie dźwiękowe: 4 tankowanie, 3 pełny bak (A40E–A422); $7B = 8 przy zmianie, potem VBI odlicza w dół | z kodu i śladu |
| $6A | zapasowe życia: 4 na starcie gry, 3 po starcie samolotu, 0 na ostatnim życiu | ze śladu |
| $76 | faza życia: 128 na starcie gry; 49/47 czekanie przy moście na start (każdy ruch drążka startuje); 0 lot; 253 i dalej −3 na klatkę wybuch; 130→127 odrodzenie (paliwo do $FFFF) | ze śladu |

### Punkty i mosty
| Adres | Co | Status |
|---|---|---|
| $63/$64/$65 | punkty, BCD, najstarszy bajt pierwszy, przechowywane podzielone przez 10 (950 na ekranie = `00 00 95`); przy przepełnieniu 999999 i $6A = 0 (A563) | sprawdzone |
| $BB0B + typ | punkty/10 za obiekt (BCD): 8 helikopter 60, $0D statek 30, $0E most 500, $0F paliwo 80, $10 most z czołgiem 750; tablica ma też 7 → 100, $0A/$0B → 150 (nie widziane w śladach) | sprawdzone dla 8, $0D, $0E, $0F, $10 |
| $61/$62 | numer bieżącego mostu, BCD (jak „BRIDGE n” na ekranie), +1 po zniszczeniu mostu (A4F0) | z kodu i śladu |
| $38 bit 7 | czołg na drodze został zniszczony razem z mostem (A513–A517) | z kodu, 1 przypadek w śladzie |
| $39 | X czołgu na drodze (HPOSP1 w DLI w B54D) | z kodu |

Punkty dostaje się też za wroga, w którego wleciał samolot: jego slot przechodzi w stan 3. Zestrzelony pociskiem przechodzi w stan 2.

### Sloty obiektów (11, indeks X = 0…10)
Slot to poziomy pas ekranu, a nie stały numer obiektu: przy przewijaniu obiekt przechodzi do następnego slotu (w śladzie stacja przeszła przez sloty 8 → 9 → 10). Sprawdzenie kolizji w klatce odbywa się przed tym przejściem.

| Adres | Co | Status |
|---|---|---|
| $0C+X | pionowe położenie obiektu (+1 na klatkę przy normalnej prędkości; ≈ 240–247 przy samolocie) | ze śladu |
| $0500+X | bieżący rodzaj obiektu. ≥ 7 to żywy obiekt: 7…$0D wrogowie (8/9 klatki helikoptera, $0D statek), $0E most, $0F stacja. < 7 to pusto lub wybuch: 2 zestrzelony pociskiem, 3 kolizja; występują też 0, 1, 4–6. Przy kolizji (A3E9): 7…$0D śmierć, $0E śmierć + `INC $2E`, $0F tankowanie | sprawdzone (2/3 przez punkty, $0F przez tankowanie) |
| $050B+X | typ obiektu (indeks w tablicy punktów): w śladach 0, 1, 8, $0D, $0E, $0F | sprawdzone przez punkty |
| $0516+X | X obiektu | z kodu (A500–A511, porównywane z $39) |
| $42+X | flagi: bit 7, bit 6 ($40, ustawiany z RANDOM w A205), niższe bity | nie analizowane |
| $4E | indeks przetwarzanego slotu (porównywany z #$0B) | z kodu |

### Przebiegi wzorcowe do odwzorowania
- `oracle/scripts/bot_run_8000.txt`: wejście nagrane przez bota (`rr_oracle -bot`), 8000 klatek. Zawiera 2 mosty (jeden z czołgiem), 5 śmierci, koniec gry około klatki 3300, potem ekran tytułowy i nowy start około 5323. Brak prawdziwego tankowania: bot zestrzeliwuje stacje.
- Odtworzenie tego pliku bez bota daje identyczne co do bajtu `ram.bin`, `pokey.log` i `random.log`.

- `oracle/scripts/refuel_first_depot.txt`: przelot nad pierwszą stacją bez strzelania (32 klatki nad stacją, bak dochodzi do $FFFF).

## Generowanie terenu (kod A6FD–A9D7, rysowanie AA8C–AB4B)
Brzeg nie jest przechowywany jako obraz, tylko jako kilka liczb na linię. Każda nowa linia ekranu to jeden krok generatora, po którym linia jest rysowana do bufora pierścieniowego ekranu (adres linii pochodzi z listy wyświetlania, $3F04/$3F05).

**Przewijanie (z kodu).** W każdej klatce prędkość `$59` jest dwukrotnie dodawana do akumulatora `$30`, a każde przepełnienie daje jedną nową linię. Przy prędkości 128 to dokładnie 1 linia na klatkę; przy `$59 ≥ $FE` linia powstaje bez akumulowania.

**Struktura (z kodu).**
- `$2F` — numer linii w bloku, 0…31.
- `$5A` — ile bloków zostało w odcinku (2).
- `$5D` — ile odcinków zostało do mostu, przeładowywane wartością 16; na końcu wywoływane jest `sub_B194`.
- LFSR `sub_B183` robi krok w każdej linii poza liniami z bitem 7 w `$23` (proste podejścia do mostu).
- Na granicy odcinka nowy cel szerokości `$5F` bierze się z młodszej połówki `$AA` z ograniczeniami (minimum 2, maksimum 15/14/8 zależnie od `$2D`, `$60`, `$B5`), potem `$34 = $5F·4`.

**Liczby na linię.**
| Adres | Co | Status |
|---|---|---|
| $52 | położenie brzegu | sprawdzone (losowe błądzenie, niżej) |
| $C3 | kierunek błądzenia: +1, $FF (−1) lub 0 | sprawdzone |
| $54 | połowa szerokości wyspy / części wewnętrznej | sprawdzone (wygładzanie) |
| $53 | szerokość wody; zmienia się o −2 × krok `$54`, więc całkowita szerokość się zachowuje | sprawdzone |
| $34, $35 | cel dla `$54` i krok do niego (±1, jeśli różnica < 32, inaczej ±2) | sprawdzone |
| $2D | bit 7 — tryb wyspy (zmienia rysowanie), bit 6 | z kodu |
| $C4, $C5 | granice `$52`: $1C…$24 przy `$60 < 3`, inaczej szersze (1…$40 z wariantami według `$60`, `$052D`, `$36`) | z kodu, w śladach widziane 28…36 |
| $23 bit 7 | przy moście brzeg idzie za tablicą `$BA8F[$2F] + $52`, bez losowości | sprawdzone przez C++ (`terrain_check`) |
| $052D | ≠ 0: brzeg jest ściągany do korytarza $18…$28 | sprawdzone przez C++ (`terrain_check`) |

**Losowe błądzenie brzegu (A84F–A96A), sprawdzone.** Każda linia bierze młodszą połówkę `n = $AC & 15` (bajt LFSR po kroku tej linii):
- jeśli `$C3 = 0`, ruch się zaczyna: +1 przy `n ≥ 8`, inaczej −1;
- jeśli brzeg już się porusza: przy `n < 8` stoi; przy `n = $D` zawraca i robi krok; przy `n = $F` stoi, jeśli `$0E` (`$0D` na wyspie) jest mniejsze od `$5F` lub `$2C`; przy każdej innej wartości robi krok;
- krok, który przekroczyłby granice `$C4/$C5` albo wpadł na obiekt w bieżącym slocie `$4E` (typ ≥ 8; granica liczona przez `$54/$34`, `$0516+X`, `$B5`, `$0521+X`), zastępowany jest zawróceniem w miejscu.

Subtelność taktowania: sloty i `$4E` są brane takie, jak na początku klatki, a `$54/$34/$2D/$B5/$37` już przeliczone w tej klatce. Z tą regułą model zgadza się ze śladami na wszystkich 4669 liniach.

**Wygładzanie wyspy (A7FB–A824), sprawdzone.** Dopóki `$35 ≠ 0`, każda linia robi `$54 += $35` i `$53 −= 2·$35`. Gdy `$54` osiągnie `$34`, krok `$35` jest zerowany. Sprawdzone na 4825 liniach, z których 1195 rzeczywiście się zmieniło.

**Ozdoby (domy, góry).** Zaczynają się na początku bloku (`$2F = 0`), gdy bit 5 `$AC` jest ustawiony. Przesunięcie, szerokość i liczba wierszy pochodzą z tablic `$BBCC`, `$BBD4`, `$BBB2`, `$BBEB` według `$AA` i `$5F`, wskaźnik bitmapy z `$BBF0`. Sprawdzone przez C++.

**Obiekty.** Co 16 linii nowy obiekt jest stawiany w najwyższym wolnym slocie (`$4D`−1); typ wybierany jest według `$AB` przez tablicę `$BB1C`, szerokość sprite'a z `$BBA2` (do `$0521+X`, czyli `SIZEP3`: 1 dla statku, 3 dla mostu, inaczej 0), liczba linii z `$BB60` (+4). Dopóki obiekt jest jeszcze dorysowywany (`$32` linii), brzeg omija ten obiekt (`$4E`). Sprawdzone przez C++.

**Mosty.** Generator rysuje tylko prosty kanał tam, gdzie jest most (brzeg z tablicy `$BA8F`), i stawia obiekt typu `$0E` w slocie z flagą `$80/$84` w `$42+X` (kopia `$23`). Sam most pojawia się dopiero przy wyświetlaniu, w DLI (B575–B5FC): dla slotu z bitem 7 we flagach kolor brzegu `COLPF0` w każdej linii pochodzi z tablicy `$B4E5` (`04 04 04 04 08×6 1C 1C 06×5 04 04 04`: szare pasy drogi i żółte oznakowanie), więc droga biegnie przez cały ekran po pikselach brzegu; przęsło nad wodą to sprite P3 tego slotu w poczwórnej szerokości, jak u statków i helikopterów. Po obiekcie kolor brzegu jest przywracany z `$0B` (`$D2`/`$D6`: brzeg zmienia dwa odcienie zieleni według parzystości odcinka). Czołg na drodze to sprite P1 (`$39`). Linie bitmapy mostu nie zmieniają się po wygenerowaniu (sprawdzone na 119 liniach).

**Kolory i sprite'y na linię (sprawdzone przez C++, `port/src/river_draw.cpp`).** Kolory bazowe przychodzą z wygaszania pionowego (B408–B43E): woda `COLPF2` = $84 ($42 podczas błysku), skraj `COLPF1` = $20, brzeg `COLPF0` z `sub_AFD0`: `($23 & 4) ^ $D6`, z odwróconym bitem 2, dopóki najnowszy obiekt (`$4E`) to most z y < $5C. DLI przechodzi sloty od `$4D` w górę z licznikiem linii skanowania (wartość $5D w linii 20, gdzie wchodzą nowe linie): pierwszy slot z `0 ≤ y + $BB60[typ] − licznik ≤ $BB60[typ]` daje wiersz r; kształt to `$B900 + $BB40[rodzaj (+16 przy bicie 3 flag)] + r`, kolor `$B700 + $BB30[typ] + r`, HPOSP3 = `$0516+X` (32 = lewa krawędź bufora), SIZEP3 = `$0521+X`; dla slotu mostu COLPF0 = `$B4E5 + r`. W porównaniu z linią 20 oryginału: 96,8% linii identycznych z pozycjami generatora, 98,1% z pozycjami obiektów gry, wszystkie pozostałe linie, gdy aktywny jest czołg na drodze.

**Tryb demonstracyjny i dół ekranu (sprawdzone przez C++, `port/src/status_bar.cpp`).** Bez START gra odtwarza rzekę gry 1 od włączenia: przewijany pasek rusza od klatki 11, rzeka od klatki 38, 1 linia na klatkę, przez mosty, tym samym generatorem (pasek linii 20 zgadza się z C++ na 19 960 klatkach poza ruchomymi obiektami). Lista wyświetlania pod $3F00: 24 puste linie, 160 linii trybu E, potem panel: 3 wiersze trybu 6 (normalna szerokość, `CHBASE` $08, kolory $1E/$8E/$1E/$00 na $06, `PRIOR` 4) od $1000, wypełnione z `$BB70`; wskazówka paliwa to pocisk 1 pod `$5B/8 + $5C`. Ostatni wiersz: tryb 6 szeroki z `HSCROL`, `CHBASE` $06, kolor $0C, tekst pod `$B800 + $3FB3`; VBI (B44B) przesuwa `HSCROL` 7→0, a potem tekst o jeden znak ($4B…$D6, w kółko), dopóki `$2A` ≠ 0, i przesuwa razem logo `$27`. Logo to gracz 3 w poczwórnej szerokości (dane $FC) plus gracz 2 ($FF) przesunięty o takt w lewo w każdym wierszu, w kolorach `$B48D`. Zestawy znaków pod $0600/$0800 to czcionka systemu (A0FB) ze znakami kartridża pod $06C0, $0818, $08D0, $07D0 (cyfry „1982, 1983”).

**Ruch obiektów, czołg, rakiety (sprawdzone przez C++, `port/src/game.cpp`, `display.cpp`).** Co 16 klatek slot wybrany przez `RANDOM & 15` (11–15 → 6–10) dostaje bit 6 w `$42+X` („obudzony”), chyba że to obiekt wciąż dorysowywany. Obiekty rodzaju 7 (myśliwiec) poruszają się w każdej klatce; rodzaje 8–13 tylko obudzone, od mostu 2 (`$60 >> 1 ≠ 0`), co drugą klatkę według parzystości slotu (`X ^ $18 ^ $33`), rodzaj $0C tylko w klatkach z `$18 & 2 = 0`; 1 takt na krok, bit 3 = w lewo, zawijanie $2C…$D4. Bit 0 `$42+X` ustawia DLI, gdy sprite obiektu dotknął na ekranie koloru skraju brzegu (P3PF bit 1); następny ruch odwraca wtedy raz kierunek (bit 4 oznacza zawrót, dopóki dotyk się nie skończy). Rodzaje poniżej $0C są animowane przez `$BB24` co 8 klatek (co 2 dla rodzajów 7+). Czołg (`$38`–`$3A`) jedzie co 16 klatek (co 4 dla rodzajów 4–7), aż gracz 1 dotknie koloru skraju, potem (`$41` bit 7) strzela: pocisk to gracz 0, `$50` liczy 0…$38 (tablica lotu `$B4AF`, prędkość `$3E/$3F`), potem klatki wybuchu. Helikopter rodzaju $0A/$0B w slocie `$51` wystrzeliwuje pocisk 0 (`$C1` x, `$C2` y, krok `$C0` = ±2), aż trafi w brzeg (M0PF). Rejestry kolizji DLI są czytane po każdym obiekcie i zerowane, więc kolizje są znane na slot (`$1A`–`$22` trzymają numer slotu). Z odtworzonym RANDOM C++ zgadza się z RAM-em oryginału przez 19 900 klatek trybu demonstracyjnego od włączenia.

**Sterownik dźwięku (sprawdzone przez C++, `Game::soundDriver`).** Raz na klatkę w VBI (B2A5–B3D8), AUDCTL pozostaje 0 (zegar 64 kHz, szum 17-bitowy). `$C6` ≠ 0: sygnał startu na kanale 1 ($40/$AF), reszta wyłączona. Kanał 1: zdarzenie dźwiękowe `$7A` 1–4 przez `$7B` klatek (wysokość $39/$11/$1F/$3F, głośność z `$7B`); inaczej, gdy samolot leci, ostrzeżenie o paliwie przy `$5B` < $40 (sygnał co 63 klatki, opadający ton poniżej $04) albo silnik: AUDF = $1F − `$59`/16 (albo opadający `$052C` po katastrofie), AUDC z bitów drążka góra/dół (`$B4AC`: $84 normalnie, $83/$86). Kanał 2: szum wybuchu `$7C` (AUDF = RANDOM | $18) albo, w trakcie gry, rakieta helikoptera `$7F`. Kanał 3: strzał `$7D` albo dźwięk punktów `$7E` (`$B4E1`). Kanał 4: pocisk czołgu `$50` (`$B4C0` w locie, potem szum). Tryb demonstracyjny jest cichy: `$76` ≠ 0 wyłącza kanał 1, a nic innego nie jest uruchamiane.

**Gra z graczem (sprawdzone przez C++, `port/src/game.cpp`).** `$76` to stan samolotu: 0 lot; $FE…$80 wybuch (126 klatek, `$5E` klatka samolotu w dół co 8); potem $7F: jeśli zostały życia, rzeka jest resetowana (`sub_B0F1` z X = $12, który czyści ekran i pamięć player-missile przez 13 klatek, gdy działają tylko przerwania) i wypełniana na nowo przy `$59` = $FE (2 linie na klatkę), podczas gdy `$76` liczy $7F…$30; przy $30 zabierane jest życie i `$052C` = $80 (narastający dźwięk silnika); przy $2F samolot czeka przy moście bez przewijania, aż dowolny ruch drążka lub ogień ustawi `$76` = 0 i `$59` = $40. Bez żyć: koniec gry (rekord `sub_B261` z bajtami kontrolnymi `$90`–`$9F`, `sub_B21A`: `$76` = `$2A` = $FF, „Game over” w przewijanym pasku). Lot (A3E9–A5B4): kolizja samolotu z obiektem (`$1D`): rodzaje 7–$0E katastrofa (most liczy też `$2E`), rodzaj $0F tankuje (`$5B` + 2 na klatkę, dźwięki `$7A` 4/3); wybuch pocisku (`$1E` przy `$50` ≥ $11), rakieta helikoptera (`$21`) albo brzeg (`$1B`) katastrofa; paliwo spala się po $20/256 na klatkę; ruch w bok przyspiesza (`$2B` += 8, ułamek `$58`), klatki 7/8/9; prędkość `$59` ±2 między $40 a $FE, z powrotem do $80 przy drążku na środku; pocisk (pocisk 2, 8 wierszy od `$56`, 6 wierszy w górę na klatkę) niszczy obiekty rodzaju ≥ 7 (`$1C`), tablica punktów `$BB0B` według typu, życie co 10 000 punktów (do 9). Zniszczony most liczy `$61/$62`, zabiera stojący na nim czołg, a od mostu 5 sprawia, że pobliski czołg strzela. `sub_B1C0` decyduje, skąd startuje następny samolot: za zniszczonym mostem od następnego odcinka, inaczej o punkt kontrolny wstecz. Gry dla dwóch (nieparzyste `$09`) zamieniają `$60`–`$6A` z `$6B`–`$75` (`sub_B247`) po każdym samolocie; gracz 2 używa drugiego joysticka (starsza połówka PORTA). START (A673) zaczyna nową grę (B0F1 z X = $29, 13 klatek); SELECT/OPTION co 30 klatek następna gra; dowolny klawisz klawiatury to pauza (`$24`: 3 → 2 pauza → 1 → 0).

**Rysowanie (sprawdzone przez C++).** Linia to 48 bajtów bitmapy (tryb ANTIC E, 4 piksele na bajt), składanych z wypełnień (podprogramy `AFEC`/`AFF0`/`AFF4`: `$AA`, `$FF`, `$55`) i 2-bajtowych kawałków skraju z tablicy pod `$B800` (`B034`/`B03B`): brzeg, woda, wyspa (z bitem 7 `$2D`), woda, brzeg. Pozycja piksela podzielona przez 4 to numer bajtu, dolne 2 bity wybierają kawałek skraju. Wiersz ozdoby kopiuje `B01A`. Brzegi nie są lustrzane: prawa krawędź liczona jest z lewej przez szerokości wody i wyspy, więc rzeka meandruje. Na 2200 liniach z C++: gdy ruszają się obie krawędzie, w 810 przypadkach na 944 ruszają się tak samo; środek rzeki wędruje między 71 a 122 pikselami.

## Generator w C++ (`port/`)
- Całe przetwarzanie linii (A711–AE96 i podprogramy) przeniesiono do `port/src/river_gen.cpp`. Sprawdzenie `port/tools/terrain_check` porównuje każde pole generatora i 48 bajtów każdej nowej linii ekranu z oryginałem.
- Wynik: 119 758 linii całej rzeki bez różnic; start od zera (`startGame`) też się zgadza.
- Generator nie zależy ani od gracza (bot wobec lotu bez wejścia), ani od `RANDOM` (3 ziarna, 120 000 klatek, identycznie co do bajtu, łącznie z paskiem ekranu).
- Ekran: bitmapa trybu ANTIC E, 48 bajtów na linię (192 piksele, 4 na bajt), bufor pierścieniowy, adres nowej linii w `$3F04/$3F05`, −$30 na linię. Wartości pikseli: 1 brzeg, 3 woda, 2 skraj, 0 rzadko (wewnątrz kawałków skraju).
- Gry 1–8: `$09` wybiera wiersz tablic `$B494`–`$B4A8`: LFSR, brzeg i odcinek startowy. Gry 1–2 zaczynają się od mostu 1, 3–4 od 5, 5–6 od 20, 7–8 od 50. To punkty kontrolne tej samej rzeki: C++ dochodzi do mostu 5/20/50 dokładnie z wartościami z tablicy (`$B4A4`: poziom, `$B4A8`: licznik mostów w BCD). Port buduje tę samą tablicę przebiegiem generatora dla `-seed N` (rzeka gry 1 od stanu LFSR N; rzeka kartridża to $F7D4E3 = 16241891) i dla `-bridge N` (każda gra od mostu N, 1–9999; po 80 poziom idzie 79, 80, 79...).
- `$CB` = $2E na NTSC i $2F na PAL (A077, z rejestru PAL); wpływa na szanse stacji paliw na początku bloku (AC48). Rzeka na PAL i NTSC nieco się różni.
- Start i odrodzenie: `sub_B0F1` wczytuje $4D–$5F z `$BAE1`, czyści $2B–$4C i $0500–$05FF, stawia most startowy w slocie 10 (typ $0E, y=$4A, wysokość $17), tryb $23 = $84/$80. Potem A3C8 przywraca LFSR i brzeg z punktu kontrolnego $66–$69. Następnie gra generuje 160–165 linii po 2 na klatkę (prędkość $FE), by wypełnić ekran.
- Pułapka weryfikacji: gdy ekran jest wypełniany po 2 linie na klatkę, granica klatki emulatora może wypaść między liniami, więc porównanie klatka po klatce w tych klatkach jest błędne. W normalnej grze (jedna linia na klatkę) migawka jest zawsze na granicy linii.

## Mapa poziomu
- Generator brzegu nie zależy od samolotu: omijanie obiektów patrzy na typ `$050B`, który się nie zmienia, gdy obiekt zostanie zestrzelony. `$60` (numer odcinka) rośnie co 1024 linie, gdy generowany jest most, a nie gdy zostaje zniszczony (w grze bota `$60` zmieniło się w klatce 1133, most zniszczono w 1177).
- Dzięki temu cały poziom można sfilmować niezniszczalnym samolotem: `rr_oracle -ghost` łata 5 bajtów w emulowanej pamięci (A3E9/A424/A42E/A432 `LDX flaga` → `LDX #$FF`, A443 zużycie paliwa $20 → 0). Żadnej śmierci w 120 000 klatkach.
- Przy normalnej prędkości poziom przesuwa się dokładnie o 1 linię ekranu na klatkę, a nowa linia pojawia się w linii ekranu 20. Pasek linii 20 z każdej klatki zgadza się z żywym ekranem poza ruchomymi obiektami.
- `$60` rośnie do $50 (80), potem zmienia się $4F/$50; stan LFSR na granicach odcinków nie powtórzył się przez 117 odcinków, więc rzeka się nie zapętla, ale trudność przestaje rosnąć po odcinku 80.
- Budowanie obrazów po odcinkach: `tools/level_map.py`. Przeglądarka jest opublikowana jako strona „Карта реки River Raid”.

## Metoda
Regułę najpierw odczytuje się z listingu, potem sprawdza na śladzie wzorcowym (`tools/verify_trace.py`). Jeśli ślad się nie zgadza, szuka się błędu odczytu. Przykład: tankowanie wyprowadzono z kodu A3E9–A453 (łącznie z nieoczywistym +2 zamiast +1 z powodu flagi przeniesienia) i potwierdzono na śladzie; jedyna niezgodność pokazała, że sprawdzenie kolizji odbywa się przed przejściem obiektów między slotami.

## Brytyjska wersja kasetowa
`River Raid.cas` (UK, 600 bodów): dwa pliki. Plik startowy (7 bloków pod $0A00, start $0A06) sprawdza, czy jest co najmniej 24K RAM-u („NOT ENOUGH RAM MEMORY - NEED 24K”), pokazuje ekran tytułowy, ładuje $2200 bajtów pod $1000 i skacze do $3000. To kod kartridża przeniesiony w inne miejsce, porównany instrukcja po instrukcji: od A21B wszystkie ponad 2900 instrukcji to te same operacje; kod i dane przesunięte z $A000 na $1000, strony RAM gry w górę o $2000 (lista wyświetlania $3F00 → $5F00, pierścień ekranu $2000–$3FFF → $4000–$5FFF, $1000 → $3F00), zmienne strony zerowej głównie +2 ($25 → $A1, $28 → $A2); wskaźniki w tablicach przesunięte tak samo. Prawdziwe różnice:
- inicjalizacja (A000–A0EE) przeniesiona do $3000, z sumą kontrolną wczytanego kodu w $E4/$E5;
- zabezpieczenie przed kopiowaniem: jeśli suma się nie zgadza albo w HATABS jest obsługa `D:` (start spod DOS-u), pętla główna się zawiesza albo skacze w środek instrukcji. Własne zabezpieczenie kartridża (A151–A158) zapisuje $FF do własnego kodu pod $A19B, jeśli w $6000 nie ma $47: w ROM-ie nieszkodliwe, w kopii w RAM-ie zabójcze;
- przewijany pasek mówi „1982, 1983, 1984”: dodatkowy znak „4” (kopiowany do $07C0), tekst dłuższy o 5 znaków, początek/koniec przewijania $20/$4B → $1B/$46.

Generator, logika gry, dźwięk i tablice są identyczne; sprawdzenie PAL także.

## Determinizm i skoki
- **Rzeka zależy tylko od siebie.** Generator nie zależy ani od gracza, ani od RANDOM, ani od obiektów: omijanie obiektu patrzy na jego typ (`$050B`) i X obiektu, który dopiero wjeżdża na ekran i jeszcze się nie rusza. LFSR robi krok na każdą linię poza prostymi podejściami do mostu: dokładnie 993 kroki od mostu do mostu. Jego okres od startu gry 1 to 16 766 977 kroków, a NWD(993, 16 766 977) = 1, więc stan przy moście wraca dopiero po 16 766 977 mostach (z parzystością poziomu, 79/80, być może po dwa razy tylu; nie przeliczano): około 9 lat lotu bez przerwy. Licznik mostów na panelu ma 4 cyfry: po 9999 pokazuje pustkę (0000) i liczy dalej od 1.
- **Poziom.** W sub $B194 (`sectionCheckpoint`) poziom $50 i wyższy jest przed zwiększeniem o 1 brany jako $4E, więc po 80 poziom zmienia się 79/80. Odcinki nieparzyste i parzyste się różnią (nieparzyste: kod szerokości zawsze 7, obiekty gdziekolwiek w poprzek; parzyste: losowa szerokość, wyspy), zmiana zachowuje oba rodzaje; trudność już nie rośnie.
- **Punkty kontrolne.** Przy każdym moście generator zapamiętuje LFSR i brzeg. Start od mostu N (`-bridge N` w grze, odrodzenie po śmierci) je przywraca: rzeka dalej jest ta sama co w locie bez przerwy, obiekty nie (wszystkie świeżo rozstawione, żaden nie obudzony ani przesunięty).
- **RANDOM.** 17-bitowy licznik, który w każdym takcie maszyny robi krok po stałym kole 131 071 stanów. Port przechowuje jego położenie na tym kole (`src/pokey_random.h`), więc wartość w dowolnym takcie jest znana od razu. To, który wróg się obudzi, zależy więc od liczby klatek od włączenia.
- **Obiekty żyją tylko na ekranie.** Generator stawia obiekt u góry, a ten znika, gdy zjedzie za dolną krawędź, po około 200 klatkach; tak samo czołg (prepare: `tankY` ≥ $BC) i jego pocisk. Dlatego cały stan w dowolnej chwili wynika z rzeki w tej chwili plus ostatnich ~200 klatek. `Game::skipFrame` robi klatkę „tylko rzeka”: przewijanie, liczniki klatek, timer trybu demonstracyjnego, przewijany pasek i to, co prepare() robi ze stanem rzeki (zniknięcie czołgu i pocisku pod ekranem; bez tego stary czołg nie pozwala generatorowi postawić następnego). Potem całe klatki. Sprawdzone z lotem od początku (4 rzeki i starty z `-bridge`, po 77 punktów, po 6000 klatek po każdym): od 200 całych klatek każda klatka rysuje się tak samo i cały stan się zgadza. Zmienne nieaktywnego czołgu, pocisku czy rakiety helikoptera mogą mieć inne stare wartości; przed użyciem są ustawiane od nowa i nie są rysowane.
- **W trybie demonstracyjnym nic niczego nie niszczy.** Wybuch pocisku czołgu niszczy obiekt tylko w kodzie lotu samolotu (A4C1), który bez samolotu nie działa.
- **Wygaszacz.** Po 128 × 2048 klatkach (około 73 minut) bez wejścia bit 7 w $78: pętla główna staje (A1F5), a ekran gaśnie do dowolnego joysticka, klawisza konsoli lub klawiatury. Gra to zachowuje; przeglądarka zeruje $78 w każdej klatce.
- **Przewijanie w przeglądarce** (`viewer/river_view.cpp`). Kotwice (stan rzeki i liczniki, ~170 bajtów) co 1024 klatki teraźniejszości; pełne migawki (~16 KB) co 128 klatek, tylko dla ostatnich 8192 klatek przed teraźniejszością i wokół miejsca powtórki (64). Linia L jest pokazywana od najbliższej migawki albo od kotwicy: tylko rzeka, potem 400 całych klatek (jeszcze 512, jeśli potem przewijanie wstecz). B znajduje następny most samym generatorem i leci tam tak samo; `-bridge N` leci od początku rzeki. Pamięć: ~190 bajtów na most plus stałe ~2 MB.
- **Zapisy stanu w grze** (F7 / F8, na gamepadzie RB / LB; `Game::save` / `load`). Inaczej niż w przeglądarce nic tu nie można „ustabilizować”: samolot styka się z obiektami, więc zapisuje się cały stan, ale tylko to, czego nie da się odtworzyć. Zapisywane są: zmienne (`GameState`) razem z pamięcią sprite'ów, stan generatora oraz stan generatora przed najstarszą linią pamięci ekranu z liczbą linii; pamięć ekranu (ostatnie 256 linii rzeki, 12 KB) jest z niego odtwarzana, bo linie zależą tylko od generatora (gra pamięta w tym celu stan generatora przed każdą linią na ekranie, `keepLineStates`, około 31 KB w trakcie działania). Niezapisywane: dane na linię skanowania (`scan`), które display() czyści i buduje od nowa w każdej klatce, oraz obraz, który rysuje następna klatka (także w pauzie: pauza zatrzymuje tylko logikę gry). Pamięci sprite'ów (1,25 KB: 4 graczy jako kolumny 8 pikseli wysokości 256 linii, plus pociski; układ wideo czyta po bajcie na linię dla każdego) nie da się odtworzyć: gra nie zawsze ściera kształt, zanim narysuje go gdzie indziej, a resztki biorą udział w kolizjach; jest jednak niemal w całości zerowa. Wszystko jest pakowane seriami zer (bajt zerowy, potem długość serii): 400–620 bajtów razem z licznikiem RANDOM i dźwiękiem POKEY (zamiast 110 KB z pamięcią ekranu i obrazem). Sprawdzone: 12 zapisów (lot z różnymi prędkościami, wybuch, ponowne wypełnianie ekranu po śmierci, koniec gry, pauza), po 900 klatek po każdym: obraz i dźwięk te same.

## Następne kroki
W analizie nic nie zostało otwarte. Dla portu: dokładna sekwencja włączenia (A000–A17C) i taktowanie PAL (50 Hz, lista wyświetlania PAL).
