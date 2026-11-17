# Dolly — Engineering Field Notes 🐑
## Dziennik błędów, niespójności, odkryć i ciekawostek

**Cel:** zachować ślad rozwoju Dolly dla przyszłych programistów, użytkowników i historyków retroinformatyki. Ten dziennik **nie zastępuje Issues, testów ani changeloga**: zapisuje *dlaczego* podjęliśmy decyzje, co wykryliśmy, jak sprawdziliśmy hipotezy oraz co pozostało niepewne.

**Zasady:**
1. Wpisów nie usuwamy bez powodu. Błędne hipotezy oznaczamy jako **sprostowane**, zachowując oryginalne ustalenie.
2. Każdy wpis ma ID, datę, kategorię, status, źródło/objaw, znaczenie, działanie oraz metodę weryfikacji.
3. Oddzielamy **potwierdzone błędy** od **hipotez**, **ograniczeń**, **decyzji projektowych** i **ciekawostek**.
4. Odnotowujemy istotne błędy testów i CI, nawet jeżeli są szybko naprawione. Nie publikujemy danych osobowych, numerów seryjnych ani prywatnych obrazów dysków.
5. Istotne odkrycia powinny mieć odnośnik do Issue, PR, commita lub testu regresyjnego. Datę i wynik weryfikacji dopisujemy po sprawdzeniu, nie na podstawie samego zamiaru.
6. **Dokument jest chronologiczny** (nowe wpisy na końcu). Numer ID nigdy nie jest ponownie używany.

---

### DJ-0001 · 2026-11-17 · Projekt / format · ZATWIERDZONE
**Temat:** Identyfikator własnego kontenera to `IFF FORM DOLY`, a nie proponowane wcześniej `FORM DLYI`.

**Historia:** podczas projektowania formatu DLY zaakceptowano IFF jako bazę, a następnie zmieniono czteroznakowy identyfikator FORM na `DOLY`.

**Znaczenie:** wszystkie parsery, narzędzia i testy muszą identyfikować `DOLY`; rozszerzenie pliku pozostaje `.dly`. Standard IFF nie jest sam w sobie kompletną definicją naszego formatu kompresji i manifestów.

**Weryfikacja:** [`src/dolly_probe.c`](../src/dolly_probe.c), [`docs/SPECIFICATION.md`](SPECIFICATION.md).

### DJ-0002 · 2026-11-17 · Architektura / bezpieczeństwo · OTWARTE OGRANICZENIE
**Temat:** Samo pomyślne odczytanie RDB/PART nie dowodzi integralności dysku.

**Objaw:** wczesny parser weryfikuje checksumy, cykle i podstawowe granice, ale nie waliduje jeszcze pełnych rezerwacji RDB, `FSHD`, `LSEG`, `BADB` ani wszystkich odmian hybrydowego MBR/RDB. GPT i łańcuchy EBR nie są wspierane.

**Ryzyko:** nieuprawnione uznanie nośnika za bezpieczny do zapisu; ukryte konflikty przestrzeni.

**Działanie:** backend pozostaje wyłącznie odczytowy; nie zamykamy [Issue #4](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/4), dopóki kryteria bezpieczeństwa nie zostaną spełnione.

**Weryfikacja:** [`docs/RDB_INSPECTOR.md`](RDB_INSPECTOR.md), [`docs/MBR_NESTED_RDB.md`](MBR_NESTED_RDB.md).

### DJ-0003 · 2026-11-17 · Parser / walidacja · WSTĘPNIE POPRAWIONE; TESTY DO POWTÓRZENIA
**Temat:** Interpretacja `de_SizeBlock` w `DosEnvec`.

**Wykrycie:** istniejący kod mnożył `de_Surfaces × de_BlocksPerTrack` przez `rdb_BlockBytes`, pomijając odczytane pole `de_SizeBlock`. To pole jest zapisane w **32-bitowych słowach** (`128` oznacza 512 bajtów), a nie w bajtach.

**Działanie:** w `src/dolly_rdb.c` zmieniono wyliczanie długości cylindra tak, aby uwzględnić `de_SizeBlock × 4`, i dodano odpowiedni przypadek testowy do `tests/test_rdb.c` (commit `bc44b4eb10e93f7efbc615c935cc8c2f73655972`).

**Ważna uwaga:** poprawność *użycia* tego rozmiaru do wyznaczania fizycznego zakresu partycji **wciąż wymaga porównania** z dokumentacją konkretnej implementacji AmigaDOS, obsługą `de_SectorsPerBlock` i rzeczywistymi obrazami. Sama jednostka pola jest pewna; szczegóły geometrii nietypowych konfiguracji pozostają do weryfikacji. Nie wolno zamykać tego wpisu jako „naprawiony” wyłącznie na podstawie syntetycznego testu.

**Źródła porównawcze:** [AmigaOS RDB documentation](https://wiki.amigaos.net/wiki/RDB_%28Amiga_Rigid_Disk_Block%29); [AmigaOS Expansion Library](https://wiki.amigaos.net/wiki/Expansion_Library); [Amiga ROM Kernel Reference Manual (DOS)](https://developer.amigaos3.net/sites/default/files/downloads/2024-10/Amiga_ROM_Kernel_Reference_Manual_DOS.pdf).

**Następny krok:** sprawdzić wzory na fizyczne sektory dla `de_SizeBlock != 128` i `de_SectorsPerBlock != 1`, uzupełnić testy realnymi przypadkami.

### DJ-0004 · 2026-11-17 · Integralność danych / decyzja · ZATWIERDZONE
**Temat:** Błąd odczytu w obszarze oznaczonym jako wolny wymaga innej oceny niż błąd w pliku lub metadanych.

**Odkrycie:** bitmapa zajętości OFS/FFS pozwala odróżnić obszary potencjalnie nieistotne dla aktualnego systemu plików. Nie oznacza to jednak, że fizyczny sektor jest zdrowy albo że nie pozostały tam dane skasowanych plików.

**Działanie:** planujemy klasyfikacje `FREE`, `FILE_DATA`, `FS_METADATA`, `ALLOCATED_UNKNOWN`, `INCONSISTENT`, `UNKNOWN` z poziomem wiarygodności. Błędy RAW nie są automatycznie pomijane; w Smart Clone/Smart Imaging wolna przestrzeń może być pomijana po odpowiedniej weryfikacji.

**Status implementacji:** niezaimplementowane; decyzja produktowa.

### DJ-0005 · 2026-11-17 · PiStorm / ciekawostka · POTWIERDZONE W ZAKRESIE PROJEKTU
**Temat:** MBR typu `0x76` może zawierać niezależny RDB, którego adresy trzeba interpretować względem początku kontenera.

**Znaczenie:** nie można utożsamiać numeru LBA wewnętrznego RDB z bezwzględnym LBA karty SD. Na tym samym nośniku mogą znajdować się inne obszary, np. FAT dla Emu68.

**Działanie:** wstępny parser obsługuje zagnieżdżenie i weryfikuje zakresy. Nadal brak obsługi wszystkich odmian hybrid/shared-offset. Zob. [Issue #4](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/4).

### DJ-0006 · 2026-11-17 · Platforma / ograniczenie · OTWARTE
**Temat:** Testy na PC nie zastępują uruchomienia na Motoroli 68000.

**Znaczenie:** hostowa implementacja plikowa używa POSIX (`fseeko`, `ftello`, `off_t`), a natywny AmigaOS ma inne API I/O i ograniczenia pamięci. Deklarowana kompatybilność z AmigaOS 2.04 i 68000 nadal wymaga rzeczywistej walidacji.

**Działanie:** oddzielić backend urządzeń AmigaOS od wspólnego parsera, prowadzić testy w emulatorze i na prawdziwym sprzęcie; raportować realne zużycie pamięci. Zob. [Issue #3](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/3).

### DJ-0007 · 2026-11-17 · Tożsamość projektu / ciekawostka · ZATWIERDZONE
**Temat:** Nazwa `Sheep Counter` zamiast wcześniejszego roboczego `Surface Monitor`; owieczka na aktualnym bloku.

**Znaczenie:** humorystyczna nazwa nie zastępuje szczegółowych danych o LBA, retry, błędach i wynikach weryfikacji. Pasek statusu to wspólna usługa wszystkich ekranów. Opcjonalne krótkie beczenie 8-bit mono ~22 kHz jest pomijane przy niedoborze Chip RAM.

**Cytat projektu:** „W stodole jest tyle owiec, że nawet beczeć nie ma jak!”

### DJ-0008 · 2026-11-17 · Kierunek po MVP / ciekawostka · ODŁOŻONE
**Temat:** Dolly Flock Manager — jeden system bazowy, różne Amigi, profile sprzętowe i lokalne zmiany.

**Działanie:** osobny [Issue #7](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/7); brak wpływu na zakres M0/MVP. Przyszła synchronizacja musi szanować atrybuty AmigaDOS, lokalne konflikty i brak sieci.


### DJ-0009 · 2026-11-17 · Architektura / poprawa · POTWIERDZONE LOKALNIE
**Temat:** Przenośny interfejs `BlockSource`.

**Wykrycie:** pierwotnie `BlockSource` zawierał bezpośrednio `FILE*`, co uzależniało wspólne parsowanie od POSIX i komplikowało natywny backend AmigaOS.

**Zmiana:** callbacki `read`/`close`, niezależny `src/dolly_file_source.c` i weryfikacja błędnych liczników bajtów zwracanych przez backend. Zestaw testów hostowych zakończony powodzeniem; kompilacja natywna 68000 wciąż niepotwierdzona.

### DJ-0010 · 2026-11-17 · I/O / bezpieczeństwo · PROTOTYP
**Temat:** Granica 4 GiB w `CMD_READ`.

**Ryzyko:** 32-bitowy `io_Offset` może zawinąć się przy dużym nośniku. Sama 64-bitowa arytmetyka w rdzeniu Dolly nie zabezpiecza wywołania sterownika.

**Zmiana:** `dolly_io_plan_read()` odmawia żądań przekraczających granicę 4 GiB, chyba że wcześniej zweryfikowano obsługę `NSCMD_TD_READ64`. Dla NSD górne 32 bity offsetu są przekazywane w `io_Actual`; brak automatycznego zakładania TD64. Testowano na symulowanym Exec, **nie na fizycznym kontrolerze**.

**Źródła:** [TrackDisk64](https://wiki.amigaos.net/wiki/TrackDisk64_Standard), [NSD](https://wiki.amigaos.net/wiki/NSD_Command_Specifications).

### DJ-0011 · 2026-11-17 · QA / sprostowanie · POPRAWIONE
**Temat:** Oczekiwany kod błędu dla `UINT64_MAX-511`.

**Wykrycie:** początkowy test błędnie uznał offset za niewyrównany do 512 bajtów. W rzeczywistości jest wyrównany, a odczyt 512 bajtów przekracza koniec przestrzeni adresowej 64-bitowej.

**Zmiana:** poprawiono oczekiwany wynik z `EINVAL` na `ERANGE`; test regresyjny uruchomiony ponownie. To była usterka testu, nie implementacji urządzenia.

### DJ-0012 · 2026-11-17 · IFF / integralność · POTWIERDZONE LOKALNIE
**Temat:** Rozpoznanie nagłówka `FORM DOLY` nie oznacza poprawności jego chunków.

**Rozwiązanie:** `dolly_iff_scan()` waliduje big-endian długości, padding, ucięcia i wyjścia poza FORM. Nie wymaga odczytywania dużych payloadów ani ich alokacji. Akceptacja nieznanych chunków służy rozszerzalności.

**Testy:** 7/7 zwykłych CTest oraz 7/7 w ASan/UBSan na komputerze hosta. **Brak jeszcze walidacji semantyki `HEAD/CHNK/INDX` i sum kontrolnych danych.**

### DJ-0013 · 2026-11-17 · Sprzęt / DMA · OTWARTE
**Temat:** `MEMF_PUBLIC` i pośredni bufor nie gwarantują poprawnego DMA dla wszystkich kontrolerów.

**Ryzyko:** rozmaite sterowniki Amigi mogą wymagać własnej maski adresowej, alignmentu i limitu `MaxTransfer`; dochodzą zmiany nośnika podczas pracy.

**Działanie:** backend AmigaOS wyraźnie eksperymentalny; brak produkcyjnego odczytu z realnych urządzeń bez kontroli maski, geometrii i testów sprzętowych. Zob. [projekt adaptera](AMIGA_IO.md).

### DJ-0014 · 2026-11-17 · Amiga 68000 / mały RAM · OTWARTE
**Temat:** Nadmierna pamięć na stosie przy strukturach wynikowych RDB/MBR.

**Wykrycie:** `DollyRdb` ma wbudowane rekordy maks. 128 partycji, a `DollyPartitionMap` osadza kilka takich struktur. To wygodne na PC, lecz potencjalnie za duże dla małych stosów AmigaOS.

**Działanie:** w fazie natywnej przenieść wyniki na stertę lub scratch, z czasem rozważyć strumieniowe wyliczanie partycji. Rozmiary i zużycie pamięci muszą zostać zmierzone na docelowym sprzęcie.



### DJ-0015 · 2026-11-17 · RDB / diagnostyka · POPRAWIONE I PRZETESTOWANE LOKALNIE
**Temat:** Uszkodzony `RDSK` był zgłaszany jako „nie znaleziono RDB”.

**Wykrycie:** pierwotny skaner pomijał nagłówki `RDSK` z niepoprawnym checksumem, a po wyczerpaniu zakresu zwracał `DOLLY_RDB_NOT_FOUND`. Użytkownik nie wiedział, że istnieje uszkodzony kandydat.

**Zmiana:** skaner zapamiętuje błędną sumę kontrolną i niezgodny rozmiar bloku; jeśli nie znajdzie później poprawnej kopii `RDSK`, zwraca odpowiednio `BAD_CHECKSUM` lub `BAD_BLOCK`. Gdy znajdzie późniejszy prawidłowy nagłówek, korzysta z niego.

**Test:** fixture z uszkodzonym RDSK bez kopii oraz z późniejszą poprawną kopią; 7/7 standardowych hostowych CTest po zmianie. Kontrola zapasowych RDSK w rzeczywistych nośnikach nadal wymaga testów.

### DJ-0016 · 2026-11-17 · QA / ciekawostka · POTWIERDZONE LOKALNIE
**Temat:** Syntetyczna karta SD z MBR `0x76` umożliwia testować całą ścieżkę CLI bez ryzykowania prawdziwego dysku.

**Rozwiązanie:** `tests/test_cli_smoke.py` generuje w katalogu tymczasowym MBR, zagnieżdżony RDB/PART, prawidłowy `FORM DOLY` z nieparzystym `ANNO` i uszkodzony wariant IFF. Sprawdza działanie `dolly-layout` i `dolly-iff`.

**Test:** lokalny CTest: 8/8 zestawów; Clang/ASan dla nowego ósmego zestawu wymagają dodatkowego przebiegu CI. Dane w całości fikcyjne; brak dostępu do urządzeń fizycznych.



### DJ-0017 · 2026-11-17 · RDB / integralność danych · NOWA FUNKCJA; CZĘŚCIOWO PRZETESTOWANE
**Temat:** Audyt `de_MaxTransfer` i konserwatywne odniesienie `0x0001FE00`.

**Wykrycie/obserwacja:** na części starszych implementacji `scsi.device` / IDE zbyt duże wartości `MaxTransfer` mogą prowadzić do cichego uszkodzenia danych podczas dużych operacji. `0x1FE00` to 130 560 bajtów, czyli 255 sektorów po 512 bajtów. Nie jest to uniwersalna granica sprzętowa: starsze kontrolery mogą wymagać mniej (np. `0xFE00`), inne pozwalają na więcej.

**Działanie:** [Issue #11](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/11), nowy `dolly_maxtransfer` klasyfikujący ustawienia, odczyt pola `de_MaxTransfer` z `PART` (`DosEnvec` index 13), prezentacja w hostowym `dolly-rdb`. Ostrzeżenie nie oznacza automatycznej zmiany RDB ani systemu plików.

**Test:** nowy test klasyfikacji `tests/test_maxtransfer.c` zweryfikowany lokalnie przez GCC z ASan/UBSan (PASS); test ekstrakcji RDB dodany, pełnego przebiegu repozytorium po tej zmianie jeszcze nie potwierdzono.

**Niepewności:** dane z RDB mogą różnić się od aktywnego mountlist/DosList; wybór bezpiecznego limitu zależy od sterownika i nośnika. **`de_MaxTransfer` filesystemu nie jest tym samym co niezależny limit naszego backendu `DoIO`.**

**Źródła:** [Amiga ROM Kernel Reference Manual: DOS](https://www.aminet.net/docs/misc/rkrm-dos.pdf), [AmigaOS 3.9 FAQ](https://www.gregdonner.org/os39faq/diskfileoperation.html).

### DJ-0018 · 2026-11-17 · C / regresja · POPRAWIONE W ŹRÓDLE; DO WALIDACJI CAŁEGO PROJEKTU
**Temat:** Dosłowne sekwencje `\\n` w źródłach parsera RDB.

**Wykrycie:** w poprzednim commicie narzędzia generującego poprawkę do `de_SizeBlock` część znaków końca linii została zapisana jako dosłowne `\\n` poza literałami C, co może prowadzić do błędu kompilacji.

**Działanie:** przy okazji integracji MaxTransfer zapisano poprawne znaki końca linii w `src/dolly_rdb.c`; ponowny odczyt pliku z GitHub potwierdził brak niezamierzonych `\\n` w tym fragmencie.

**Test:** wymagany pełny przebieg CI; przegląd tekstu nie stanowi dowodu, że wszystkie buildy przeszły.



### DJ-0019 · 2026-11-17 · RDB / kompatybilność · POPRAWIONE W ŹRÓDLE; REGRESJA DO POTWIERDZENIA
**Temat:** Krótsze historyczne tablice `DosEnvec` nie powinny automatycznie oznaczać uszkodzenia rekordu `PART`.

**Wykrycie:** parser początkowo wymagał `de_TableSize >= 17`, więc nie potrafił przedstawić stanu `de_MaxTransfer` nieobecnego w starszej tablicy. To prowadziło do odrzucenia części możliwych starszych wpisów jako `BAD_BLOCK`.

**Działanie:** dostępne pola czytamy tylko w zakresie deklarowanego `de_TableSize`: geometria do indeksu 11 wymagana, `de_MaxTransfer` indeks 13 oraz `de_DosType` indeks 16 opcjonalne. Brakujące dane mają stan `NOT_PRESENT`, a nie domyślne „bezpieczne”.

**Test:** dodano syntetyczny rekord `PART` z `de_TableSize=12` do `tests/test_rdb.c`; pełny CTest po zmianach wciąż czeka na potwierdzenie. Nie przesądza to poprawności montowania takich partycji.


### DJ-0020 · 2026-11-17 · RDB / zabezpieczenia · TESTY LOKALNE PASS; INTEGRACJA DO SPRAWDZENIA
**Temat:** Niezależny audyt kolizji zakresów RDB obejmujący PART, RDSK i obszary partycji.

**Problem:** poprzedni parser odczytywał poprawne checksumy i podstawowe granice, ale na poziomie całego dysku nie sprawdzał, czy dwie partycje RDB nakładają się lub obejmują blok nagłówka i deskryptora.

**Działanie:** dodano `dolly_rdb_audit()`, flagi `OVERLAPPING_PARTS`, `HEADER_COLLISION`, `PART_COLLISION`, `OUTSIDE_CONTAINER` i `BAD_ARGUMENT`, testy oraz ostrzeżenia w `dolly-rdb` i `dolly-layout`. Sprawdzenia wykonywane wyłącznie na zakresach wcześniej zweryfikowanych; brak zapisów. Zob. [Issue #12](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/12).

**Testy:** odtworzony lokalnie wcześniejszy snapshot projektu + nowy moduł: GCC/CMake/CTest **5/5 PASS**; osobny test audytu na Clang z ASan/UBSan: **PASS**. Nie oznacza to weryfikacji całego aktualnego repozytorium po zmianach na GitHub.

**Ograniczenia:** audyt nie interpretuje jeszcze `FSHD`, `LSEG`, `BADB`, `rdb_RDBBlocksLo/Hi` ani współdzielonych hybryd MBR/RDB. Zero flag nie jest certyfikatem zdrowego dysku.



### DJ-0021 · 2026-11-17 · RDB / checksum · POPRAWIONE W ŹRÓDLE; PEŁNE CI DO POTWIERDZENIA
**Temat:** Checksum `RDSK` / `PART` może mieć prawidłową sumę przy zbyt małym `SummedLongs`.

**Wykrycie:** starszy parser akceptował arytmetycznie poprawną sumę nawet dla `SummedLongs=8` w `RDSK` lub `40` w `PART`, chociaż później czytał pola znajdujące się poza obszarem sumowania. Skutkiem była fałszywa wiarygodność geometrii, nazwy lub `DosEnvec`.

**Działanie:** `src/dolly_rdb.c` wymaga obecnie co najmniej 19 słów (4-bajtowych) dla konsumowanych pól `RDSK` oraz `33 + de_TableSize` słów dla `PART`. Wcześniej odrzuca `de_TableSize` wykraczające poza bufor.

**Test:** `tests/test_rdb.c` zawiera specjalnie spreparowane, krótsze checksumy. Ten sam algorytm i odpowiadające mu testy przeszły lokalnie w odtworzonym wcześniejszym snapshotcie (5/5 CTest), natomiast pełne bieżące repozytorium nadal wymaga niezależnego potwierdzenia builda/CI.

**Pozostałe ryzyka:** RDB ma dodatkowe typy metadanych (`FSHD`, `LSEG`, `BADB`); nie są jeszcze walidowane tym kodem.

### DJ-0022 · 2026-11-17 · PiStorm / hybrydy · CZĘŚCIOWO ZAIMPLEMENTOWANE
**Temat:** Prawidłowy MBR nie wyklucza jednoczesnego RDB w początkowych blokach nośnika.

**Wykrycie:** wcześniejsze `dolly_partition_map_scan()` wykrywało cały-dysk RDB tylko przy braku sygnatury MBR. W zaawansowanych układach jednoczesna obecność MBR i RDB mogła pozostać niewidoczna.

**Działanie:** po zbadaniu wpisów MBR parser sprawdza również pierwsze bloki całego nośnika pod kątem poprawnego `RDSK`. Zapisuje `hybrid_mbr_rdb` oraz `hybrid_mbr_overlap`, a `dolly-layout` ostrzega o niejednoznaczności. Nie podejmuje automatycznie decyzji, który widok miałby służyć do zapisu.

**Test:** syntetyczne hybrydy z kolidującymi i rozłącznymi zakresami; lokalna kompilacja i CTest 5/5 na odtworzonym starszym snapshotcie z nowymi zmianami. Testy aktualnego repozytorium i autentycznych kart PiStorm pozostają do wykonania.

**Ważne:** wykrywanie współistnienia nie jest pełną obsługą dowolnej hybrydy; nie traktujemy tego jako bezpiecznej podstawy do klonowania.

**Źródło praktyczne:** [oficjalna dokumentacja Emu68/PiStorm — przygotowanie karty SD](https://pistorm.github.io/tutorials/sd_setup/). [Issue #12](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/12).



### DJ-0023 · 2026-11-17 · RDB / błędna hybryda · ZWERYFIKOWANE LOKALNIE; CAŁE CI OTWARTE
**Temat:** Poprawny MBR może maskować uszkodzony kandydat RDB zapisany na tym samym nośniku.

**Wykrycie:** po dodaniu rozpoznawania współistnienia MBR+RDB parser rozróżniał jedynie poprawne RDB i brak RDB. Nieprawidłowa suma kontrolna `RDSK` mogła pozostać niewidoczna, mimo że czytelny MBR nadal pozwalał na korzystanie z części nośnika.

**Działanie:** do `DollyPartitionMap` dodano `suspect_whole_disk_rdb`. Nie unieważnia automatycznie poprawnego MBR, ale `dolly-layout` ostrzega o możliwych uszkodzonych metadanych RDB. Zob. [Issue #12](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/12).

**Test:** syntetyczny MBR ze skasowaną checksumą dyskowego RDB i z poprawnym RDB; lokalnie odtworzony zestaw przechodzi 5/5 CTest po uwzględnieniu wcześniejszej poprawki rozróżniającej `NOT_FOUND` i `BAD_CHECKSUM`.

**Ograniczenia:** wynik nie dowodzi żadnej fizycznej kolizji; trzeba zweryfikować `FSHD/LSEG/BADB`, kolejne kopie `RDSK` i rzeczywiste profile sterowników. Pełny aktualny build GitHub/68000 nadal niepotwierdzony.

### DJ-0024 · 2026-11-17 · 32-bitowe systemy hostowe · POPRAWIONE; TEST NA 32-BITOWYM HOŚCIE OTWARTY
**Temat:** Typ `uint64_t` w silniku nie zapewnia 64-bitowych offsetów plików, jeśli libc używa 32-bitowego `off_t`.

**Działanie:** moduły hostowych plików i testy `fseeko/ftello` używają przed nagłówkami `_FILE_OFFSET_BITS=64`. To jest mechanizm POSIX large-file support, niezależny od implementacji I/O natywnej Amigi.

**Test:** odtworzony lokalny projekt GCC/CTest 5/5 PASS, ale środowisko jest 64-bitowe. Właściwy regresyjny build i odczyt >4 GiB na 32-bitowym Linuxie nadal pozostają do wykonania.



### DJ-0025 · 2026-11-17 · Testy parserów / odporność · TEST LOKALNY PASS
**Temat:** Deterministyczne mutowanie syntetycznych rekordów MBR/RDB.

**Działanie:** dodano `tests/test_rdb_mutations.c`: 600 powtarzalnych wariantów (ustalony seed), 1–6 zmian pojedynczych bitów w MBR, RDSK, PART lub danych obrazu. Każda iteracja korzysta z pliku tymczasowego i wyłącznie odczytowego parsera. Test jest deterministyczny, więc błędne przypadki można odtworzyć.

**Weryfikacja:** w lokalnie odtworzonym zestawie z nowymi zmianami `GCC/CTest` **6/6 PASS**. To test stabilności określonego zestawu mutacji, nie pełny fuzzing, nie dowód poprawności formatu i nie test na fizycznych urządzeniach.

**Dalszy plan:** niezależny libFuzzer/AFL, seed corpus z legalnymi wariantami RDB, testy pod sanitizatorami i rzeczywiste, zanonimizowane obrazy.

### DJ-0026 · 2026-11-17 · QA / CMake Release · POPRAWIONE I ZWERYFIKOWANE LOKALNIE
**Temat:** `assert()` w testach C może być całkowicie usuwany przez `-DNDEBUG` w Release.

**Problem:** część testów Dolly wykorzystuje `assert()` nie tylko do porównań, ale również do wykonywania operacji inicjalizujących. W konfiguracji Release takie wyrażenia są usuwane, co może dawać fałszywy wynik lub uszkodzić przebieg testu.

**Działanie:** konfiguracja CMake dla testowych programów GNU/Clang dodaje `-UNDEBUG` bez wpływu na biblioteki produkcyjne.

**Test:** lokalny odtworzony zestaw GCC Release: **6/6 PASS** przy wymuszonych aktywnych asercjach.

**Pozostałe ryzyko:** docelowo zastąpić `assert()` w procedurach inicjalizacji testów jawnymi makrami CHECK z diagnostyką także poza tym zestawem kompilatorów. Aktualny GitHub Actions po zmianach pozostaje do niezależnej weryfikacji.


### DJ-0027 · 2026-11-17 · Sheep Inventory / architektura · PROTOTYP DZIAŁA LOKALNIE
**Temat:** Rozdzielenie pozyskiwania pól inwentarza od interfejsu użytkownika.

**Odkrycie:** budowanie całego raportu w jednej dużej strukturze jest niewskazane na Amidze 68000 z małym stosem i ograniczonym RAM. Informacje o partycji można generować jedna po drugiej, z polami `key`, `label`, `unit`, `origin`, `availability` i wartością typu `uint64_t`/tekst.

**Implementacja:** `include/dolly_inventory.h`, `src/dolly_inventory.c`, `tests/test_inventory.c` i prototyp `dolly-inventory`. Callback nie kopiuje dużych buforów, dopuszcza wcześniejsze zatrzymanie i nie odczytuje dodatkowych sektorów.

**Testy:** lokalna kompilacja GCC C99 z `-Wall -Wextra -Werror -pedantic`, Clang oraz GCC ASan/UBSan: wszystkie **PASS**; host CLI uruchomiony na syntetycznym MBR `0x76` z RDB na odtworzonym parserze. Nie jest to jeszcze weryfikacja pełnej bieżącej gałęzi GitHub ani kompilacja AmigaOS.

**Ograniczenia:** obecnie tylko dziewięć pól `PART`; brak informacji z aktywnego DosList/urządzeń, pełnej geometrii i woluminów. Dane obrazu nie mogą być automatycznie uznawane za stan sterownika rzeczywistego dysku. Zob. [Issue #13](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/13) i [projekt warstwy informacji](SHEEP_INVENTORY.md).

### DJ-0028 · 2026-11-17 · DosEnvec / wiarygodność · POPRAWIONE, TEST JEDNOSTKOWY PASS
**Temat:** Zero w polu `de_DosType` nie musi oznaczać tego samego co brak pola.

**Problem:** przy skróconych historycznych tablicach `DosEnvec` parser nadawał nieobecnemu `de_DosType` wartość `0`, bez oddzielnego znacznika dostępności. Mogłoby to powodować mylący raport Sheep Inventory.

**Działanie:** `DollyPartition` zawiera `dos_type_present`; ustawiane jest tylko, gdy `de_TableSize >= 16`. Warstwa Inventory przedstawia brak pola jako `UNAVAILABLE`, nie jako rzeczywisty DosType równy zero.

**Weryfikacja:** dodano asercję w `tests/test_rdb.c` i skompilowano samodzielny test strumienia pól z poprawionym modelem pod GCC/Clang. Cały bieżący CTest repozytorium pozostaje do niezależnej weryfikacji.


### DJ-0029 · 2026-11-17 · Sheep Inventory / rozszerzenie diagnostyki · PROTOTYP PRZETESTOWANY LOKALNIE
**Temat:** Strumieniowe rekordy informacji dla MBR i RDSK obok partycji PART.

**Odkrycie:** jedna biblioteka może emitować informacje diagnostyczne dla różnych klas obiektów przez ten sam callback, bez rozbudowanego GUI i bez dużego bufora raportu. Odbiorca rozróżnia źródło (`MBR`, `RDB PART`, `RDB header`, `derived`) i jednostkę.

**Implementacja:** `dolly_inventory_mbr_partition` (7 pól), `dolly_inventory_rdb_header` (7 pól), `dolly_inventory_partition` (16 pól po późniejszych rozszerzeniach), CLI `dolly-inventory`. Nowe statusy pól: `NOT_QUERIED`, `UNSUPPORTED`, `INCONSISTENT`, oprócz `PRESENT`/`UNAVAILABLE`.

**Test:** testy samego zrekonstruowanego modułu Inventory GCC/Clang C99 `-Wall -Wextra -Werror -pedantic`: **PASS**. Nie jest to pełny build aktualnego `main`.

**Następne:** identyfikatory i pochodzenie informacji w UI, źródła urządzeń i woluminów, dane aktywnego montowania, spójne formatowanie HEX/dec i anulowalne kosztowne analizy. [Issue #13](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/13).

### DJ-0030 · 2026-11-17 · AmigaDOS / syntetyczne fixtury · POPRAWIONO PRZYKŁADY; PEŁNE CI OTWARTE
**Temat:** `de_SectorPerBlock` w testowych `DosEnvec` wynosiło 0, choć klasyczna dokumentacja wymaga wartości `1`.

**Wykrycie:** fixtury generowały tablice `DosEnvec` wypełnione zerami i ustawiały jedynie wybrane pola. Odczytowe testy nie wykrywały nieprawidłowego `de_SectorPerBlock`, bo parser nie używał tego pola do liczenia zakresów.

**Działanie:** w fixturach RDB/MBR i testach CLI ustawiono `de_SectorPerBlock=1`; w inspektorach `dolly-rdb`, `dolly-layout` i `dolly-inventory` dodano ostrzeżenie dla wartości innej niż 1. Nie zmienia to żadnych danych nośnika.

**Źródło:** [AmigaOS Expansion Library — DosEnvec](https://wiki.amigaos.net/wiki/Expansion_Library) (`de_SectorPerBlock`: not used, must be 1).

**Test:** wcześniejszy strumieniowy test Inventory GCC/Clang PASS; pełny obecny zestaw CTest z nowymi fixture'ami nadal niepotwierdzony. Jest to **ostrzeżenie o niespójności**, a nie automatyczny wyrok o uszkodzeniu dysku.


### DJ-0031 · 2026-11-17 · Sheep Inventory / metadane · WDROŻONO PROTOTYP, TESTY LOKALNE PASS
**Temat:** Oddzielenie pochodzenia pola od jego *klasy wiarygodności*.

**Problem:** informacja z zapisanej tablicy partycji może wyglądać jak aktualny parametr sterownika, chociaż jest tylko deklaracją. W szczególności `de_MaxTransfer` i `de_Mask` w RDB nie dowodzą, że sterownik rzeczywiście stosuje te wartości.

**Działanie:** `DollyInventoryField.evidence` rozróżnia `DECLARED`, `OBSERVED`, `DERIVED` oraz `ESTIMATED`. Obecni dostawcy MBR/RDB emitują tylko `DECLARED` i `DERIVED`. `OBSERVED` pozostaje dla przyszłych odczytów Exec/DosList i urządzeń.

**Test:** lokalne odtworzenie zmienionego modułu Inventory, GCC/Clang C99 z ostrymi ostrzeżeniami: PASS. Pełen bieżący build repozytorium nadal nie został niezależnie wykonany.

### DJ-0032 · 2026-11-17 · Sheep Inventory / powtarzalność ostrzeżeń · WDROŻONO PROTOTYP, TESTY LOKALNE PASS
**Temat:** Logika ostrzegania o MaxTransfer i kolizjach RDB nie powinna zależeć od konkretnego programu CLI.

**Rozwiązanie:** dwa strumienie: `dolly_inventory_partition_notices()` i `dolly_inventory_rdb_notices()`, z kodem, kategorią i opisem każdego ostrzeżenia. Obsługują między innymi `MAXTRANSFER_ABOVE_REFERENCE`, `SECTORS_PER_BLOCK_NONSTANDARD`, `MEMORY_MASK_ZERO`, `RDB_PARTITIONS_OVERLAP` oraz kolizje `RDSK/PART`. `dolly-inventory` korzysta z tej warstwy zamiast duplikować algorytmy.

**Test:** test jednostkowy reguł partycji GCC/Clang i GCC ASan/UBSan PASS. Po włączeniu audytu zakresów RDB test GCC/Clang PASS, w tym wczesne przerwanie callbacku. Pełny aktualny CTest/Actions pozostaje do weryfikacji.

**Ciekawostka:** brak ostrzeżeń oznacza tylko, że *aktualnie rozpoznawane problemy* nie zostały zgłoszone. Nie oznacza, że wolumen, nośnik ani kontroler są zdrowe.


---

## Szablon nowego wpisu

```md
### DJ-0033 · YYYY-MM-DD · [kategoria] · [NOWE / HIPOTEZA / POTWIERDZONE / POPRAWIONE / SPROSTOWANE]
**Temat:**
**Objaw / kontekst:**
**Wpływ / ryzyko:**
**Diagnoza i źródła:**
**Działanie / commit / Issue:**
**Test regresyjny i wynik:**
**Co pozostaje niepewne:**
```

_Proszę aktualizować dziennik przy każdej istotnej poprawce, zaskakującym zachowaniu sprzętu, korekcie wcześniejszej hipotezy lub decyzji architektonicznej._
