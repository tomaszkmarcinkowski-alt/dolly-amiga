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

---

## Szablon nowego wpisu

```md
### DJ-0009 · YYYY-MM-DD · [kategoria] · [NOWE / HIPOTEZA / POTWIERDZONE / POPRAWIONE / SPROSTOWANE]
**Temat:**
**Objaw / kontekst:**
**Wpływ / ryzyko:**
**Diagnoza i źródła:**
**Działanie / commit / Issue:**
**Test regresyjny i wynik:**
**Co pozostaje niepewne:**
```

_Proszę aktualizować dziennik przy każdej istotnej poprawce, zaskakującym zachowaniu sprzętu, korekcie wcześniejszej hipotezy lub decyzji architektonicznej._
