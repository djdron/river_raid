# Eksperymenty: gra bez modelu DLI

[English](DLI_REMOVAL.md) | [Русский](DLI_REMOVAL.ru.md) | [Українська](DLI_REMOVAL.uk.md) | [Deutsch](DLI_REMOVAL.de.md) | Polski | [Italiano](DLI_REMOVAL.it.md) | [Español](DLI_REMOVAL.es.md) | [中文](DLI_REMOVAL.zh.md)

Gałęzie `dli_removal` i `dli_removed`. Celem jest prostsze wyświetlanie dla portów na wolne
maszyny (ZX Spectrum Next, TS-Conf): sprite'y rysowane jako gotowe obrazki w swoich pozycjach,
kolizje liczone dla każdego obiektu, bez modelu przerwania listy wyświetlania (DLI) linia po linii.
W `dli_removal` gra pozostaje klatka po klatce taka sama jak oryginał, a każdy krok jest
sprawdzany względem oryginału (`rr_oracle` na libatari800, `game_check`). `dli_removed` idzie krok
dalej: nie ma DLI wcale, kolizje to zwykła geometria, która różni się od oryginału w kilku klatkach
na tysiąc (ostatnia część, „Gałąź `dli_removed`”). Porty na ZX
([river_raid_zx](https://github.com/djdron/river_raid_zx)) idą za `dli_removed`.

## Krok 1: kolizje z brzegiem na liniach bez dekoracji

DLI czyta kolizje z tłem dla: samolotu (P2PF, tylko kolor brzegu), obiektów (P3PF, tylko kolor
krawędzi: statki i helikoptery zawracają na niej), czołgu (P1PF, krawędź), pocisku i rakiety
helikoptera (M2PF, M0PF: brzeg lub krawędź). Domy i drogi (dekoracja, `decoRow`) są rysowane
kolorem krawędzi i kolorem tła wewnątrz brzegu.

`RiverGen::nextLine(line, bank)` daje drugą linię, tę samą bez dekoracji (tam, gdzie jest
narysowana, wypełnienie brzegu); `Game::bankLines` trzyma je obok `screen`, a kolizje patrzą na nie
(`bankRuns`, domyślnie włączone; `RR_BANK_RUNS=0` ekran jak wcześniej). Bez dekoracji każda linia
rzeki ma jeden z czterech kształtów (480 000 linii gier 1–8):

| kształt (odcinki koloru) | linii | granic |
|---|---|---|
| brzeg, krawędź, woda, krawędź, brzeg | 398 644 | 4 |
| to samo z wyspą | 80 324 | 8 |
| z wąską wyspą (tylko kolor krawędzi) | 1 016 | 6 |
| linie trafionego mostu (brzeg w kolorze krawędzi) | 16 | 2 |

Linię opisuje więc 2–8 liczb.

Sprawdzone `game_check` względem przebiegów oryginału: tryb demonstracyjny (19 900 klatek), gra
bota (8 950), gra bota na PAL (5 974), gra 7 od mostu 50 na NTSC (2 przebiegi, do końca gry) i PAL
(8 950 klatek): żadnej różnicy, poza jednym bitem, którego nikt nie czyta. Myśliwce latają nad
lądem i dotykają dekoracji: bit „dotyka krawędzi” ich slotu się różni (w trybie demonstracyjnym w
341 klatkach), ale moveObjects nigdy go nie czyta dla myśliwca. `game_check` nie porównuje tego bitu
dla myśliwców, gdy `bankRuns` jest włączone. `RR_BANK_DEBUG=1` wypisuje każdą linię, w której oba
rodzaje linii dają różne kolizje (wszystkie były myśliwcami, rodzaj 7, gracz 3).

Przebiegi gry 7 od włączenia: `oracle/scripts/start_game7.txt` z `-bot`. Na NTSC porównanie
kończy się na pierwszym końcu gry, którego port nie modeluje (README).

## Krok 2: kolizje według zakresów

`collideSpans` (domyślnie; `RR_COLLIDE=masks` dla masek pikseli całej linii jak wcześniej): każdy
sprite na linii rastra to jego bity grafiki i takty koloru od pierwszego do ostatniego ustawionego
bitu. Względem tła jest sprawdzany odcinek po odcinku (linia bez dekoracji, 5–9 odcinków jednego
koloru); odcinek sięgający poza pierwszy lub ostatni ustawiony bit to trafienie, ten ściśle między
nimi jest sprawdzany bit po bicie. Dwa sprite'y: najpierw ich zakresy, bit po bicie tylko tam, gdzie
się spotykają. Wciąż na linię rastra, wewnątrz promienia: co zawiera każda linia i kiedy DLI czyta
rejestry, jest jak w promieniu.

Te same przebiegi co wyżej: żadnej różnicy względem oryginału. `RR_STATS=1` liczy przypadki;
`RR_ROUGH_BANK=1` i `RR_ROUGH_PAIRS=1` pomijają krok bit po bicie (trafienie wszędzie, gdzie zakresy
się spotkały), żeby zobaczyć, czy jest potrzebny:

| przebieg | linii ze sprite'ami | odcinek między pierwszym a ostatnim bitem (w dziurze) | zakresy się spotkały (nie bity) |
|---|---|---|---|
| tryb demonstracyjny | 1 164 849 | 86 228 (0) | 26 862 (70) |
| bot na PAL | 278 257 | 9 201 (96) | 4 108 (0) |
| gra 7, PAL | 536 802 | 24 494 (96) | 7 693 (0) |

- Brzeg: zakresy wystarczą. Z `RR_ROUGH_BANK` wszystkie przebiegi pozostają identyczne z
  oryginałem; odcinki, które wpadły w dziurę, nigdy nie zmieniły bitu czytanego przez DLI.
- Sprite'y między sobą: bity są potrzebne. Z `RR_ROUGH_PAIRS` pocisk i pocisk czołgu „trafiają”
  obiekty przez ich dziury: tryb demonstracyjny różni się w 3 klatkach, przebieg bota na NTSC w 712
  (zniszczony obiekt, obok którego przeleciał pocisk oryginału), gra 7 w 2–4. Zakresy dwóch
  sprite'ów spotykają się rzadko (kilka tysięcy linii na przebieg), więc krok bit po bicie kosztuje
  mało.

## Krok 3: wyświetlanie bez taktów DLI

`Game::displaySimple` (`port/src/display.cpp`) jest teraz domyślne; `RR_DISPLAY=beam` uruchamia
model takt po takcie jak wcześniej, `RR_DISPLAY=check` uruchamia oba z tego samego stanu w każdej
klatce i wypisuje każdą różnicę w wynikach kolizji, flagach obiektów, pamięci player-missile lub w
tym, co narysowano na każdej linii rastra.

Idzie za pętlą DLI (B565–B664) w liniach rastra, nie w taktach. Rejestr X procesora 6502 liczy
linie: po każdym STA WSYNC następuje jeden INX, więc dopóki X = c, rysowana jest linia c − 69. Dla
każdego slotu:

| co | linia rastra (S: X, gdy kończy się oczekiwanie obiektu, E: X po jego ostatnim wierszu) |
|---|---|
| HPOSP3 = x | od S − 68 |
| dwa puste wiersze w pamięci gracza 3 | S − 67, S − 66 |
| SIZEP3 | od S − 66 |
| wiersz kształtu k (od górnego w dół), jego COLPM3, kolor drogi mostu | S − 65 + k |
| kolor brzegu z powrotem (most) | od E − 68 |
| odczyt M2PL (pocisk – obiekt) | widzi do E − 70 (most: E − 69) |
| odczyt P3PL (obiekt – pocisk czołgu), P2PL (samolot – pocisk czołgu, obiekt) | do E − 69 |
| odczyt P3PF (bit obiektu „dotyka krawędzi”) | do E − 68 |
| odczyt M0PL (rakieta – samolot), M0PF (rakieta – brzeg) | do E − 68 (M0PF: patrz niżej) |
| odczyt P1PF (czołg), P2PF (samolot – brzeg), M2PF (pocisk – brzeg) | do E − 67 |
| HITCLR | linie od E − 66 liczą się dla następnego obiektu (patrz niżej) |

Obiekt, którego wszystkie wiersze są powyżej linii, na której kończy się jego oczekiwanie (zaczął
się za blisko pod poprzednim), jest pomijany bez odczytów i bez HITCLR. Gdy X przekręca się na 0,
rzeka się kończy; część panelu czyta kolizje bieżącego obiektu do linii 188–190.

Gdzie jest procesor po WSYNC (takt 106), wyznacza kod, więc każdy dostęp wypada po znanej stronie
rysowania linii (takt 28), poza dwoma, o których decydują takty początku następnej linii (DMA,
instrukcja listy wyświetlania z adresem lub bez) i trafienia zapisane tuż przed nimi:

- M0PF (B631) jest czytany 28 taktów po WSYNC, 30, gdy M0PL był trafieniem: po rysowaniu na linii,
  której początek zajmuje 8 taktów (z adresem), więc widzi o jedną linię więcej. W przebiegu bota
  na NTSC: 1 499 razy na 68 365.
- HITCLR (B65F) przychodzi 23 takty po WSYNC, o 2 więcej za każde z trzech trafień zapisanych przed
  nim: ze wszystkimi trzema, na linii z adresem, przychodzi po rysowaniu i kolizje tej linii
  przepadają. W przebiegach nigdy nie widziane (0 z 285 623).

Kolizje: ramka każdego sprite'a (linie, na których ma grafikę, i takty, które zajmują; gracz 3 —
ramka na obiekt, przecięta tam, gdzie zmienia się jego pozycja lub szerokość). Tło: każdy wiersz
gracza 1 (czołg), 2 (samolot), 3 (obiekty) i pocisków 0 i 2 względem odcinków swojej linii (krok
2). Sprite'y między sobą: tylko pary czytane przez DLI (pocisk – obiekt, obiekt – pocisk czołgu,
samolot – pocisk czołgu, samolot – obiekt, rakieta – samolot), których ramki się spotykają, bit po
bicie na wspólnych liniach. To, co widzi odczyt, to OR linii od ostatniego HITCLR.

Sprawdzone: `RR_DISPLAY=check` na wszystkich przebiegach wyżej (tryb demonstracyjny, boty na NTSC
i PAL, gra 7 na NTSC i PAL, dwóch graczy do klatki 2900): żadnej różnicy względem promienia w
żadnej klatce. `RR_DISPLAY=simple` względem oryginału (`game_check`): te same wyniki co promień
(żadnych różnic, gra 7 na NTSC do końca gry). Obszar rzeki narysowany z niego względem ekranów
oryginału (`-screens`, 1289 klatek trybu demonstracyjnego, 1272 gry 7 na PAL od mostu 50):
identyczny.

### Szybkość (C++)

`RR_TIME=1` wypisuje czas, jaki `display()` zajmuje na klatkę. Build Release, najlepszy z 5
przebiegów, i instrukcje policzone przez callgrind (gra 7 na PAL, 2000 klatek):

| | tryb demonstracyjny | gra 7, PAL (od mostu 50) | instrukcji na klatkę |
|---|---|---|---|
| promień (`RR_DISPLAY=beam`) | 8,5 µs | 10,1 µs | 101 500 |
| displaySimple, ramki szukane w pamięci player-missile (`RR_BOXES=scan`) | 4,0 µs | 4,0 µs | 40 600 |
| displaySimple, ramki z pozycji gry | 3,1 µs | 3,7 µs | 34 200 |

Około trzy razy mniej instrukcji, 2,5–3 razy szybciej. Oba patrzą na odcinki tła zrobione raz,
gdy wchodzi linia (`Game::runLines`, `makeRuns`), nie na piksele linii. Z czasu promienia dwie
trzecie szło na kolizje liczone na każdej linii, jedna trzecia na takty (`settle`). Ramki
sprite'ów pochodzą z miejsc, gdzie postawiła je gra (domyślnie; `RR_BOXES=scan` szuka w dół w
pamięci player-missile jak wcześniej): pocisk czołgu 16 wierszy od `shellY`, czołg 11 od `tankY`,
samolot 14 od wiersza $AA, rakieta helikoptera wiersz `missileY`, pocisk 8 wierszy pod `bulletY`
(od wiersza $1C), wiersze każdego obiektu tak, jak zapisała je pętla DLI; bity każdej linii w
ramce wciąż czyta się z pamięci player-missile. To samo co promień w każdej klatce każdego
przebiegu: nic poza tymi ramkami (powiedzmy wiersze pozostałe w pamięci gracza 3 z wcześniejszych
klatek) nigdy nie dało kolizji czytanej przez DLI.

### Długie przebiegi: `display_bench`

`port/build/display_bench <rom> <frames> [-pal] [-game N] [-seed N]` gra z autopilotem (steruje na
środek wody przed sobą i strzela bez przerwy; samolot nie może się rozbić i nie zużywa paliwa, więc
jedna gra trwa cały przebieg: 100 000 klatek dochodzi do mostu 98 w grze 1, do mostu 147 w grze 7).
Z `RR_DISPLAY=check` porównuje oba modele w każdej klatce, z `RR_TIME=1` mierzy `display()`.

Te przebiegi znalazły to, czego krótsze nigdy nie pokazały: DLI czasem spóźnia się o linię. Od
jednego WSYNC do następnego kod musi się zmieścić przed taktem 106 następnej linii, odliczając
początek i rysowanie tej linii (na linii rzeki 6 lub 8 + 57 taktów); trzy odcinki mogą być
dłuższe:

- B5B0–B5D1 (do pierwszego wiersza): 49 taktów, 50, gdy bajt kształtu pierwszego wiersza jest na
  następnej stronie: za długo na linii z adresem ekranu.
- ostatni wiersz mostu do B5F3: 46 taktów, +1 za każdy odczyt tablicy przez granicę strony, +2 za
  trafienie pocisku.
- B5F3–B61F: 44 takty (most 43), +2 za każde zapisane tam trafienie (pocisk czołgu, samolot z
  pociskiem czołgu, samolot z obiektem).

Gdy jest za długo, STA WSYNC czeka na koniec następnej linii: wszystko po nim w tej klatce dzieje
się o linię później (X = linia + 68), łącznie z końcem rzeki i odczytami panelu, a wiersz gracza 3
zapisany na linii, dla której jest, lub później, pojawia się tam dopiero w następnej klatce (GTIA
już pobrało stary). displaySimple liczy te pominięcia (`RR_STATS`): 0–10 na 100 000 klatek,
wszystkie z samolotem wpadającym na rzeczy, o które by się rozbił; w prawdziwej grze z rzadka. Z
nimi zamodelowanymi `RR_DISPLAY=check` nie daje różnicy w 6 przebiegach po 100 000 klatek (gry 1,
2, 3, 5, 7, 8, NTSC i PAL).

Czas `display()`, najlepszy z 3, i instrukcje na klatkę (callgrind, 5000 klatek):

| | gra 1, NTSC, 50 000 / 100 000 klatek | gra 7, PAL, 50 000 / 100 000 klatek | instrukcje (gra 1 / gra 7) |
|---|---|---|---|
| promień | 10,8 / 11,1 µs | 10,5 / 9,5 µs | 120 000 / 129 000 |
| displaySimple | 5,1 / 4,8 µs | 4,8 / 5,0 µs | 65 000 / 69 000 |

Około dwa razy szybciej. Mniej niż w przebiegach oryginału wyżej (trzy razy), bo tu samolot
przelatuje przez obiekty i brzegi bez rozbijania się: dużo więcej kolizji na klatkę niż w
prawdziwej grze.

Jeszcze dłużej: 12 przebiegów po 300 000 klatek (3,6 miliona), `-bridge 100` (gra zaczyna się od
mostu 100; do mostu 392), NTSC i PAL, gry 1, 2 (dwóch graczy) i 7, oraz z `-wild` (autopilot
steruje losowo, także nad brzegami: do mostów 70–159). Żadnej różnicy w żadnej klatce. Pominiętych
linii: 0–8 na przebieg. Spóźniony HITCLR (wszystkie trzy trafienia zapisane przed nim, na linii z
adresem ekranu) nigdy się nie zdarzył, ani w tych przebiegach, ani w innych: wynika tylko z taktów
kodu, w działaniu go nie widziano.

### Tylko obiekty, które zawracają przy krawędzi

Bit „dotyka krawędzi” (P3PF) moveObjects czyta tylko dla helikopterów ($08–$0B), balonów ($0C) i
statków ($0D): zawracają przy pierwszym dotknięciu. Myśliwce lecą prosto, wybuchy stoją, mosty i
składy paliwa się nie ruszają. `displaySimple` sprawdza względem krawędzi tylko wiersze tych
rodzajów (`noEdge` oznacza linie pozostałych); bit pozostałych zostaje wyzerowany, a `game_check` go
dla nich nie porównuje (`bankRuns`). Sprawdzone względem oryginału: tryb demonstracyjny (NTSC 2 262
i 5 961 klatek, PAL 2 975), gry bota na NTSC (do pierwszego końca gry, którego port na NTSC nie
modeluje), na PAL (5 975 i dwóch graczy 7 775) i gra 7 od mostu 50 na PAL (5 975): żadnej różnicy.

### Samolot względem obiektu: według ramek i bitów

Bit P2PL dla gracza 3 jest czytany po wierszach obiektu (B60A) lub na końcu rzeki i widzi linie od
HITCLR przed nimi: tylko wiersze tego obiektu, w jego pozycji i szerokości. Więc „samolot dotknął
obiektu” liczy się bez linii rastra: 14 wierszy samolotu od $AA względem wierszy obiektu tam, gdzie
zapisała je pętla, bit po bicie (`planeTouches`), w tych samych dwóch miejscach (takty zapisu
liczą się jak wcześniej); para P2–P3 zniknęła z kolizji linii. Najpierw zmierzone obok wartości DLI
(`RR_GEO_CHECK`, od tego czasu usunięte): to samo w każdej klatce przebiegów oryginału wyżej
(~47 000 klatek, 420 z trafieniem) i `display_bench` (po 200 000 klatek z `-wild` na NTSC i gra 7
na PAL, 300 000 od mostu 100 na NTSC i na PAL z `-wild`: 61 382 trafienia). Potem `game_check`
względem oryginału: żadnej różnicy; `RR_DISPLAY=check` (60 000 klatek `-wild`, 60 000 od mostu 100
na PAL): nic poza bitem krawędzi rodzajów, które nie zawracają (wyżej).

### Wszystkie kolizje bez rejestrów linii

Pozostałe odczyty idą tak samo. Wiersze każdego sprite'a są tam, gdzie postawiła je gra (ramki
wyżej: nic z nich nie ma gdzie indziej), więc linie, na których dzieje się każda kolizja czytana
przez DLI, liczy się raz na klatkę: wiersze czołgu względem krawędzi (P1PF), samolotu względem
brzegu (P2PF), rakiety i pocisku względem obu (M0PF, M2PF), samolot względem pocisku czołgu (P2PL)
i rakieta względem samolotu (M0PL), bit po bicie. Kolizje obiektu liczy się przy jego odczytach,
względem jego wierszy tam, gdzie zapisała je pętla: pocisk (M2PL, do linii przed ostatnim wierszem,
także mostu), pocisk czołgu (P3PL), samolot (P2PL), krawędź (P3PF, tylko rodzaje, które zawracają).
Odczyt liczy kolizję, jeśli jej linia jest między linią ostatniego HITCLR a jego własną, jak było z
rejestrami. Pętla DLI, jej takty i linie HITCLR zostają (wyznaczają te okna); rejestry linii
(`compute`, `seen`, ramki na linię) zniknęły. Jeden przypadek się różni: gdy DLI spóźnia się o
linię, późno zapisane wiersze nie są pobierane i widać stare; kolizje są teraz z nowymi wierszami.
W żadnym z przebiegów tutaj tak się nie stało. Sprawdzone: `game_check` względem oryginału
(wszystkie przebiegi wyżej): żadnej różnicy; `RR_DISPLAY=check` (60 000 klatek `-wild`, 60 000 od
mostu 100 na PAL, 300 000 od mostu 100 na NTSC i 300 000 na PAL z `-wild`): nic poza bitem krawędzi
rodzajów, które nie zawracają.

### Obiekty rysowane z ich wierszy

Wierszy gracza 3 nie czyta nikt poza GTIA: gra nigdy. Dlatego `displaySimple` już ich nie pisze, ani
rejestrów gracza 3 na linię (HPOSP3, SIZEP3, kolory, dwa puste wiersze nad każdym obiektem). Do
rysowania zostawia `drawn`: dla każdego obiektu pierwszą linię rastra, pozycję, szerokość, wiersze
i ich kolory, tak jak pisze je pętla, oraz kolor brzegu na linię (`bankColour` w `scan`; pola
gracza 3 zostają 0); `drawRiver` rysuje z niego obiekty. Sama pętla zostaje jako rozkład: które
wiersze pokazuje każdy obiekt, gdzie jest każdy odczyt i HITCLR, takty. `diffWithRam` nie porównuje
wierszy gracza 3 ($0F00–$0FBF) przy `simpleDisplay`; `RR_DISPLAY=check` porównuje zamiast linii dwa
obrazy. Gdy DLI się spóźnia, oryginał pokazuje w miejscu spóźnionych wierszy te z poprzedniej
klatki; tu rysowane są nowe (w żadnym przebiegu). Sprawdzone: `game_check` względem stanu oryginału
(wszystkie przebiegi wyżej): żadnej różnicy; względem jego ekranów (`-screens`, 1300 klatek trybu
demonstracyjnego, 1500 gry 7 na PAL od mostu 50): obszar rzeki identyczny; `RR_DISPLAY=check`
(60 000 klatek `-wild`, 60 000 od mostu 100 na PAL): obrazy takie same, nic poza bitem krawędzi
rodzajów, które nie zawracają.

### RR_GEOMCHECK: zwykła geometria obok odczytów

Zanim pójdziemy dalej, pomiar: z `RR_GEOMCHECK` `displaySimple` liczy kolizje każdej klatki także
bez odczytów DLI, bez okna od HITCLR do odczytu i bez taktów. Obiekt jest trafiony, jeśli jego
wiersze w ogóle dotykają wierszy sprite'a (ostatni taki obiekt w kolejności pętli, bo zapisy się
nadpisują); pozostałe kolizje się liczą, jeśli zdarzają się gdziekolwiek na wierszach sprite'ów;
bit krawędzi obiektu pochodzi ze wszystkich jego wierszy. Wyniki odczytów zostają; klatki, w
których geometria decyduje inaczej, są liczone według pól i wypisywane przy wyjściu.
`RR_GEOMCHECK=2` wypisuje też każdą taką klatkę: jej numer, co się różni, miejsca pocisku, samolotu
i czołgu oraz wiersze każdego obiektu.

Wynik: różni się 0,01–0,25% klatek, zależnie od przebiegu, i zawsze tak samo: dotknięcie, które
odczyty widzą klatkę później albo wcale. Pocisk na ostatnim wierszu obiektu (M2PL jest czytany,
zanim ten wiersz zostanie narysowany), wiersze samolotu lub czołgu poniżej ostatniego odczytu w
klatce. Nic, co widzi geometria, nie jest dotknięciem, którego nie było na ekranie.

## Gałąź `dli_removed`: `displayGeometry`

W tej gałęzi domyślne jest `Game::displayGeometry` (`port/src/display.cpp`, `RR_DISPLAY=geometry`);
`RR_DISPLAY=simple`, `beam` i `check` zostają jak wyżej. Nie ma w nim DLI: żadnej pętli po slotach
w liniach rastra, taktów, HITCLR ani odczytów. To jest to, co robią porty na ZX, w C++, i wzorzec, z
którym porównuje się je klatka po klatce.

### Kolizje jako zwykła geometria

Każda kolizja liczy się tam, gdzie wiersze sprite'ów i obiektów w ogóle się stykają, najpierw
ramki, potem punkty (reguła RR_GEOMCHECK wyżej). Trafiony obiekt to ostatni taki w dół ekranu, bo
zapisy oryginału się nadpisują; pocisk czołgu nie jest sprawdzany względem mostu. Kolizje z brzegiem
i krawędzią, samolot–pocisk czołgu i rakieta–samolot trzymają slot 0, gdy zdarzają się gdziekolwiek
na wierszach sprite'ów. Wiersze sprite'ów to ramki z pozycji gry (krok 3: pocisk czołgu 16 wierszy
od `shellY`, czołg 11 od `tankY`, samolot 14 od $AA, rakieta wiersz `missileY`, pocisk 8 wierszy
pod `bulletY`).

Gra nie jest więc już w każdej klatce taka jak oryginał: w klatkach liczonych przez RR_GEOMCHECK
(0,01–0,25%) pocisk trafia obiekt albo samolot brzeg klatkę wcześniej albo liczy się dotknięcie,
które oryginał pominął. `game_check` względem oryginału jest dla `RR_DISPLAY=simple`; w trybie
domyślnym zgłasza te klatki.

### Każdy obiekt na y swojego slotu

Obiekty są stawiane wprost ze swoich slotów, bez przejścia licznikiem DLI: wiersze od linii rastra
y − 65 w dół, tam, gdzie rysował je DLI (najpierw dolny wiersz tablicy kształtu, bo tablice trzymają
je od dołu), przycięte u góry rzeki (linia 28: pierwsze oczekiwanie DLI było X = $5D) i na jej końcu
(X za 255). Kolory drogi mostu pochodzą z tych wierszy, po ostatnim kolor brzegu sekcji. Sprawdzone
na 120 000 klatek (`display_bench` od początku, od mostu 60 na PAL, `-wild`, gra 7) względem
zastąpionego przejścia licznikiem: te same obiekty, wiersze i kolory w każdej klatce. Pętla nigdy
nie spóźniła się względem obiektu, więc przycinają je tylko góra i koniec rzeki.

### Krawędź według pierwszego i ostatniego punktu wiersza

Bit krawędzi obiektu (P3PF: helikoptery, balony, statki zawracają przy krawędzi) pochodzi z każdego
jego wiersza jako zakresu, od pierwszego punktu do ostatniego, względem odcinków pikseli krawędzi
linii (`Beam::edgeTouches`), bez sprawdzania punkt po punkcie: odcinek wewnątrz dziury wiersza się
liczy, inaczej niż w sprawdzaniu bit po bicie w GTIA oryginału. Tak sprawdza Next: na linię
odcinki krawędzi, na wiersz obiektu jego dwa końce. W 20 000 klatek gier bota (ponad 100 dotknięć)
wynik nigdy nie różnił się od sprawdzania punkt po punkcie.

### Pozostałe sprite'y względem brzegu i krawędzi tak samo

`Beam::spanTouches`: pierwszy i ostatni punkt wiersza względem odcinków żądanych kolorów linii
(brzeg, krawędź lub którykolwiek); `edgeTouches` to ona z krawędzią. Czołg względem krawędzi,
samolot względem brzegu, rakieta i pocisk względem obu (`pfAny`) też z niej korzystają, więc każda
kolizja z tłem to jeden test: dwa końce wiersza względem odcinków linii. Sprite'y między sobą
(pocisk, pocisk czołgu i samolot względem obiektu, samolot i pocisk czołgu, rakieta i samolot)
zostają bit po bicie: krok 2 pokazał, że tam dziury się liczą.

### Ile to kosztuje i po co

Wyświetlanie to teraz trzy pętle: ramki sprite'ów względem odcinków ich linii, pary sprite'ów tam,
gdzie ich ramki się spotykają, obiekty postawione ze slotów i sprawdzone względem pocisku, pocisku
czołgu i samolotu. Żadna pusta linia rastra nie jest przechodzona. Na Next ta sama praca
(`display_simple.asm`, `display_hits.asm` w river_raid_zx) zajmuje 45 000–62 000 z jego 567 000
taktów na klatkę przy 28 MHz; jego CODE.md i NOTES.md mają liczby procedura po procedurze.
