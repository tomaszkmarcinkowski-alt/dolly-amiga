# Dolly — sklonuj dysk, policz owce

> Historical project record. See the [documentation index](README.md) for the current release.

*Projekt narzędzia do obrazowania, migracji i ratowania dysków klasycznej Amigi*

W świecie Amigi nośniki pamięci mają za sobą często kilkadziesiąt lat pracy. Dyskietki zawodzą, stare dyski twarde zaczynają gubić sektory, a współczesne rozwiązania — od kart CF po PiStorm/Emu68 — wprowadzają zupełnie nowe układy partycji. Przeniesienie sprawnego Workbencha na inny nośnik bywa łatwe tylko wtedy, gdy nic nie idzie źle.

**Dolly** powstaje właśnie na takie sytuacje. To darmowy program do tworzenia obrazów, klonowania, diagnostyki i odzyskiwania danych z dysków Amigi. Nazwa nawiązuje oczywiście do słynnej sklonowanej owcy.

## Nie tylko kopiowanie sektorów

Podstawą pozostanie wierna kopia RAW oraz bezpośrednia obsługa obrazów ADF i HDF. Dolly ma też rozpoznawać urządzenia i partycje RDB, również te zagnieżdżone w MBR typu `0x76`, spotykane w pewnych układach PiStorm. Własny format **IFF `FORM DOLY`** ma przechowywać niezależne bloki danych, sumy kontrolne, mapę błędów, informacje o nośniku i historię prób odczytu. To ukłon w stronę Amigi, gdzie IFF od dawna stanowi naturalny sposób organizowania danych.

Drugim filarem będzie **Smart Clone**. Zamiast zawsze kopiować blok po bloku, Dolly ma analizować OFS/FFS, a następnie przebudować strukturę partycji na nośniku o innej pojemności. Przeniesienie większego, mało zapełnionego woluminu na mniejszy dysk powinno być możliwe, jeśli dane i konieczne metadane się zmieszczą.

Trzeci filar to **DiskDoctor**: najpierw analiza bez zapisu, potem odzysk na inny nośnik, a dopiero na końcu — z wyraźnym potwierdzeniem — plan naprawy. Dolly ma umieć powiedzieć nie tylko „nie mogę odczytać sektora”, ale również „ten sektor wygląda na wolny” albo „należy do pliku”. Co ważne, wolna przestrzeń nie daje automatycznej gwarancji zdrowia nośnika ani zachowania skasowanych danych.

## Rozsądne minimum sprzętowe

Docelowym punktem odniesienia pozostaje Amiga z Motorolą 68000 i AmigaOS 2.04. Program nie powinien wymagać FPU, MMU, RTG ani dużej ilości RAM. Stąd projektowanie strumieniowe, małe bufory, opcjonalne pliki scratch i rozdzielenie rdzenia od interfejsu. Użytkownik starszego systemu otrzyma GadTools, a nowocześniejszej instalacji — opcjonalny ReAction.

Pełnoekranowy monitor **Sheep Counter** pokaże mapę sektorów z licznikami błędów i retry. Mała owieczka będzie wskazywała właśnie przetwarzany blok. Dolly otrzyma też krótki, opcjonalny sample beczenia i stałą linię statusu. Żarty pozostaną dodatkiem do precyzyjnych komunikatów diagnostycznych.

## Retroware: bez opłat, za to z odrobiną historii

Dolly jest rozwijana publicznie na licencji MIT. Program ma być darmowy, a korzystanie z niego nie będzie wymagało wpłat ani prezentów. Jeśli ktoś zechce podziękować autorowi, może dobrowolnie ofiarować amigowy sprzęt lub inny ciekawy artefakt retroinformatyczny — sprawny albo uszkodzony. Całe komputery, płyty, procesory, pamięci, klawiatury i stacje dysków są mile widzianymi owieczkami w tej stodole. Wysyłkę należy uprzednio uzgodnić z autorem.

## Stan prac

**Listopad 2026.** Od narzędzi inspekcji M0 projekt przeszedł do natywnego prototypu GadTools dla MC68000. Sheep Inventory pokazuje dane urządzeń i partycji, Surface Scan zapisuje logi błędów i ponowień, a Create Image tworzy obrazy RAW, HDF oraz IFF DOLY z dwiema metodami kompresji. Testy odbyły się w FS-UAE. Smart Clone, DiskDoctor oraz walidacja na rzeczywistych kontrolerach pozostają w planie. [Aktualny stan projektu](ROADMAP.md)

Każdy istotny błąd, niespójność czy nieoczekiwane odkrycie trafi do osobnego, publicznego dziennika technicznego. Chcemy zachować nie tylko rezultaty, ale także drogę ich powstawania.

**Dolly — No sector left behind. No sheep uncounted.**

Repozytorium: https://github.com/tomaszkmarcinkowski-alt/dolly-amiga
