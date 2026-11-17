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


---

## Szablon nowego wpisu

```md
### DJ-0020 · YYYY-MM-DD · [kategoria] · [NOWE / HIPOTEZA / POTWIERDZONE / POPRAWIONE / SPROSTOWANE]
**Temat:**
**Objaw / kontekst:**
**Wpływ / ryzyko:**
**Diagnoza i źródła:**
**Działanie / commit / Issue:**
**Test regresyjny i wynik:**
**Co pozostaje niepewne:**
```

_Proszę aktualizować dziennik przy każdej istotnej poprawce, zaskakującym zachowaniu sprzętu, korekcie wcześniejszej hipotezy lub decyzji architektonicznej._
