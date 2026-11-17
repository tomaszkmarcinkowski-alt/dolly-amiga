# Dolly — Product Backlog (wersja publiczna)

**72 wstępnych user stories** (szacunki punktów i przydziały do etapów są orientacyjne). Główne śledzenie wybranych prac: [GitHub Issues](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues). Pełny backlog nie oznacza, że wszystkie zadania są już utworzone jako Issues.

**Zasada:** MVP i aktywne M0–M8 są osobne od [post-MVP Dolly Flock Manager](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/7).

## Epic: Architecture

### DOL-001 — Kontrakt BlockDevice

**Priorytet:** P0 · **Etap:** M0 · **Story Points:** 5

**User story:** Jako programista chcę mieć funkcję „Kontrakt BlockDevice”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Backend pamięciowy odrzuca odczyt i zapis poza granicami bez naruszenia danych.

### DOL-002 — Hostowe testy na obrazach syntetycznych

**Priorytet:** P0 · **Etap:** M0 · **Story Points:** 8

**User story:** Jako tester chcę mieć funkcję „Hostowe testy na obrazach syntetycznych”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Golden fixtures OFS/FFS dają identyczne wyniki i nigdy nie zapisują źródła.

### DOL-003 — Powtarzalna kompilacja dla 68000

**Priorytet:** P0 · **Etap:** M0 · **Story Points:** 5

**User story:** Jako programista chcę mieć funkcję „Powtarzalna kompilacja dla 68000”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Czyste środowisko kompiluje program wykonywalny AmigaOS 2.04.

### DOL-004 — Wspólna OperationSession

**Priorytet:** P0 · **Etap:** M0 · **Story Points:** 5

**User story:** Jako użytkownik chcę mieć funkcję „Wspólna OperationSession”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Przełączenie Wizard → Full GUI zachowuje wszystkie parametry i historię.

### DOL-005 — Wersjonowane API pluginów

**Priorytet:** P1 · **Etap:** M0 · **Story Points:** 5

**User story:** Jako autor pluginu chcę mieć funkcję „Wersjonowane API pluginów”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Niekompatybilna wersja kończy się komunikatem zamiast awarii.

## Epic: Devices

### DOL-006 — Otwieranie device/unit i rozpoznawanie komend

**Priorytet:** P0 · **Etap:** M1 · **Story Points:** 8

**User story:** Jako użytkownik chcę mieć funkcję „Otwieranie device/unit i rozpoznawanie komend”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Probe istniejącego sterownika pokazuje obsługiwane komendy i ograniczenia.

### DOL-007 — NSD i TD64

**Priorytet:** P0 · **Etap:** M1 · **Story Points:** 8

**User story:** Jako użytkownik chcę mieć funkcję „NSD i TD64”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Sektor ponad 4 GiB czytany poprawnie wyłącznie po potwierdzeniu możliwości.

### DOL-008 — RDB i listy powiązane

**Priorytet:** P0 · **Etap:** M1 · **Story Points:** 8

**User story:** Jako użytkownik chcę mieć funkcję „RDB i listy powiązane”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** RDB z PART/FSHD daje prawidłowe zakresy i DOS type.

### DOL-009 — Ochrona przed uszkodzonym i cyklicznym RDB

**Priorytet:** P0 · **Etap:** M1 · **Story Points:** 5

**User story:** Jako użytkownik chcę mieć funkcję „Ochrona przed uszkodzonym i cyklicznym RDB”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Cykliczna lista PART kończy analizę kontrolowanym błędem.

### DOL-010 — MBR z partycją 0x76

**Priorytet:** P0 · **Etap:** M1 · **Story Points:** 8

**User story:** Jako użytkownik PiStorm chcę mieć funkcję „MBR z partycją 0x76”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** RDB wewnątrz MBR jest prezentowany z prawidłowym offsetem.

### DOL-011 — Hybrydowy MBR/RDB

**Priorytet:** P0 · **Etap:** M1 · **Story Points:** 8

**User story:** Jako użytkownik chcę mieć funkcję „Hybrydowy MBR/RDB”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Współistniejące lub nachodzące zakresy są rozpoznawane i zgłaszane.

### DOL-012 — Wstępna analiza GPT

**Priorytet:** P1 · **Etap:** M1 · **Story Points:** 5

**User story:** Jako użytkownik chcę mieć funkcję „Wstępna analiza GPT”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Obecność GPT jest wykrywana bez funkcji zapisu GPT.

### DOL-013 — Ręczny offset i geometria

**Priorytet:** P0 · **Etap:** M1 · **Story Points:** 5

**User story:** Jako ekspert chcę mieć funkcję „Ręczny offset i geometria”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Ręczne ustawienia nadpisują sugestię, a wynik jest ponownie walidowany.

### DOL-014 — Topologia unit/kontroler i overlap

**Priorytet:** P0 · **Etap:** M1 · **Story Points:** 8

**User story:** Jako użytkownik chcę mieć funkcję „Topologia unit/kontroler i overlap”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Wspólne fizyczne urządzenie źródła/celu/scratch wywołuje ostrzeżenie.

## Epic: RAW

### DOL-015 — Zgodność ADF/HDF/RAW

**Priorytet:** P0 · **Etap:** M2 · **Story Points:** 5

**User story:** Jako użytkownik chcę mieć funkcję „Zgodność ADF/HDF/RAW”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Przywrócony obraz na zgodnym celu jest bajtowo identyczny.

### DOL-016 — Sektorowe obrazowanie z buforowaniem

**Priorytet:** P0 · **Etap:** M2 · **Story Points:** 8

**User story:** Jako użytkownik chcę mieć funkcję „Sektorowe obrazowanie z buforowaniem”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Każdy prawidłowo odczytany zakres dociera do wyniku.

### DOL-017 — Retry odczytu, zapisu i weryfikacji

**Priorytet:** P0 · **Etap:** M2 · **Story Points:** 5

**User story:** Jako użytkownik chcę mieć funkcję „Retry odczytu, zapisu i weryfikacji”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Limit retry jest przestrzegany i raportowany dla LBA.

### DOL-018 — Weryfikacja zapisu read-back

**Priorytet:** P0 · **Etap:** M2 · **Story Points:** 5

**User story:** Jako użytkownik chcę mieć funkcję „Weryfikacja zapisu read-back”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Rozbieżny sektor celu jest zgłaszany z numerem LBA.

### DOL-019 — Wznawianie obrazowania RAW

**Priorytet:** P1 · **Etap:** M2 · **Story Points:** 8

**User story:** Jako użytkownik chcę mieć funkcję „Wznawianie obrazowania RAW”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Po wznowieniu czytane są tylko brakujące zakresy.

### DOL-020 — Raport nieodczytanych sektorów

**Priorytet:** P0 · **Etap:** M2 · **Story Points:** 5

**User story:** Jako archiwista chcę mieć funkcję „Raport nieodczytanych sektorów”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Raport rozróżnia źródłowe dane, fill i pominięte zakresy.

## Epic: DOLY

### DOL-021 — Binarny format IFF FORM DOLY

**Priorytet:** P0 · **Etap:** M3 · **Story Points:** 8

**User story:** Jako programista chcę mieć funkcję „Binarny format IFF FORM DOLY”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Publiczna specyfikacja definiuje dokładne offsety i endian pól.

### DOL-022 — Jednoplikowy DLY

**Priorytet:** P0 · **Etap:** M3 · **Story Points:** 8

**User story:** Jako użytkownik chcę mieć funkcję „Jednoplikowy DLY”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Roundtrip danych i checksum po kompresji/dekompresji jest zgodny.

### DOL-023 — Segmentowanie dużych obrazów

**Priorytet:** P0 · **Etap:** M3 · **Story Points:** 8

**User story:** Jako użytkownik chcę mieć funkcję „Segmentowanie dużych obrazów”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Manifest wskazuje wszystkie kompletne segmenty po przekroczeniu limitu.

### DOL-024 — Kodeki NONE/SPARSE/DEFLATE

**Priorytet:** P0 · **Etap:** M3 · **Story Points:** 8

**User story:** Jako użytkownik chcę mieć funkcję „Kodeki NONE/SPARSE/DEFLATE”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Każdy kodek zachowuje oryginalną długość i dane.

### DOL-025 — CRC i odbudowa indeksu

**Priorytet:** P0 · **Etap:** M3 · **Story Points:** 8

**User story:** Jako użytkownik chcę mieć funkcję „CRC i odbudowa indeksu”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Urwany ostatni chunk nie niszczy wcześniej poprawnych chunków.

### DOL-026 — Import przez xadmaster.library

**Priorytet:** P2 · **Etap:** M3 · **Story Points:** 5

**User story:** Jako użytkownik chcę mieć funkcję „Import przez xadmaster.library”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Zgodne archiwum jest czytane przez opcjonalny adapter.

## Epic: OFSFFS

### DOL-027 — Rozpoznawanie DOS0–DOS7

**Priorytet:** P0 · **Etap:** M4 · **Story Points:** 8

**User story:** Jako użytkownik chcę mieć funkcję „Rozpoznawanie DOS0–DOS7”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Każdy DOS type jest rozpoznawany na wzorcowym obrazie.

### DOL-028 — Rootblock, checksum i bitmapy

**Priorytet:** P0 · **Etap:** M4 · **Story Points:** 8

**User story:** Jako użytkownik chcę mieć funkcję „Rootblock, checksum i bitmapy”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Uszkodzona bitmapa daje raport bez zmiany źródła.

### DOL-029 — Przypisywanie bloków do plików

**Priorytet:** P0 · **Etap:** M4 · **Story Points:** 13

**User story:** Jako użytkownik chcę mieć funkcję „Przypisywanie bloków do plików”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Błędny LBA jest mapowany na ścieżkę lub jawne UNKNOWN.

### DOL-030 — FREE/FILE_DATA/METADATA/UNKNOWN

**Priorytet:** P0 · **Etap:** M4 · **Story Points:** 8

**User story:** Jako użytkownik chcę mieć funkcję „FREE/FILE_DATA/METADATA/UNKNOWN”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Błąd w wolnej przestrzeni ma klasyfikację i poziom zaufania.

### DOL-031 — Osierocone nagłówki i rekonstrukcja drzew

**Priorytet:** P1 · **Etap:** M4 · **Story Points:** 13

**User story:** Jako ratownik danych chcę mieć funkcję „Osierocone nagłówki i rekonstrukcja drzew”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Odzysk trafia wyłącznie na wskazany bezpieczny cel.

### DOL-032 — DirCache i długie nazwy

**Priorytet:** P0 · **Etap:** M4 · **Story Points:** 13

**User story:** Jako użytkownik AmigaOS 3.2 chcę mieć funkcję „DirCache i długie nazwy”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Nazwy i odwołania DOS6/DOS7 są prawidłowe.

## Epic: SMART

### DOL-033 — Estimator minimalnej pojemności celu

**Priorytet:** P0 · **Etap:** M5 · **Story Points:** 8

**User story:** Jako użytkownik chcę mieć funkcję „Estimator minimalnej pojemności celu”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Uwzględnione są dane oraz metadane wymagane na mniejszym celu.

### DOL-034 — Przebudowa OFS/FFS przy zmianie rozmiaru

**Priorytet:** P0 · **Etap:** M5 · **Story Points:** 13

**User story:** Jako użytkownik chcę mieć funkcję „Przebudowa OFS/FFS przy zmianie rozmiaru”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Smart Clone na innej pojemności przechodzi niezależną walidację.

### DOL-035 — Bootowalność i RDB po migracji

**Priorytet:** P1 · **Etap:** M5 · **Story Points:** 8

**User story:** Jako użytkownik chcę mieć funkcję „Bootowalność i RDB po migracji”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Klon zachowuje uzgodnione informacje rozruchowe.

### DOL-036 — Used Blocks Only

**Priorytet:** P1 · **Etap:** M5 · **Story Points:** 8

**User story:** Jako użytkownik chcę mieć funkcję „Used Blocks Only”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Obraz po odtworzeniu zachowuje pełny logiczny filesystem.

## Epic: DOCTOR

### DOL-037 — Raport DiskDoctor bez zapisu

**Priorytet:** P0 · **Etap:** M4 · **Story Points:** 8

**User story:** Jako użytkownik chcę mieć funkcję „Raport DiskDoctor bez zapisu”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Analiza wadliwego woluminu nie zapisuje żadnego bloku źródła.

### DOL-038 — Plan napraw bitmap i łańcuchów

**Priorytet:** P0 · **Etap:** M6 · **Story Points:** 13

**User story:** Jako użytkownik chcę mieć funkcję „Plan napraw bitmap i łańcuchów”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Przed Repair widoczne są adresy, ryzyka i skutki proponowanych zmian.

### DOL-039 — Naprawa po jawnym potwierdzeniu

**Priorytet:** P0 · **Etap:** M6 · **Story Points:** 8

**User story:** Jako użytkownik chcę mieć funkcję „Naprawa po jawnym potwierdzeniu”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Odmowa potwierdzenia oznacza zero zapisów.

## Epic: GUI

### DOL-040 — GadTools GUI

**Priorytet:** P0 · **Etap:** M7 · **Story Points:** 13

**User story:** Jako użytkownik OS2.04 chcę mieć funkcję „GadTools GUI”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Wszystkie funkcje rdzenia dostępne bez ReAction.

### DOL-041 — ReAction frontend

**Priorytet:** P1 · **Etap:** M7 · **Story Points:** 13

**User story:** Jako użytkownik OS3.2 chcę mieć funkcję „ReAction frontend”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Nowszy GUI działa, gdy wymagane klasy są obecne.

### DOL-042 — Wizard z przejściem do pełnego GUI

**Priorytet:** P0 · **Etap:** M7 · **Story Points:** 8

**User story:** Jako początkujący chcę mieć funkcję „Wizard z przejściem do pełnego GUI”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Przejście z kreatora zachowuje sesję.

### DOL-043 — Ekran własny i publiczny Intuition

**Priorytet:** P1 · **Etap:** M7 · **Story Points:** 8

**User story:** Jako użytkownik chcę mieć funkcję „Ekran własny i publiczny Intuition”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Dolly nie zamyka ekranu używanego przez inne okna.

### DOL-044 — Sheep Counter PAL 320×256

**Priorytet:** P0 · **Etap:** M7 · **Story Points:** 8

**User story:** Jako użytkownik 68000 chcę mieć funkcję „Sheep Counter PAL 320×256”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Własny lekki ekran pokazuje mapę i status operacji.

### DOL-045 — Liczniki retry/błędów w siatce

**Priorytet:** P0 · **Etap:** M7 · **Story Points:** 5

**User story:** Jako użytkownik chcę mieć funkcję „Liczniki retry/błędów w siatce”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Komórki mapy pokazują skrócone liczniki i szczegóły.

### DOL-046 — Owca na aktualnym bloku

**Priorytet:** P1 · **Etap:** M7 · **Story Points:** 3

**User story:** Jako użytkownik chcę mieć funkcję „Owca na aktualnym bloku”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Przesunięcie owcy nie niszczy koloru/statusu komórki.

### DOL-047 — Automatyczny Sheep Counter po bezczynności

**Priorytet:** P1 · **Etap:** M7 · **Story Points:** 5

**User story:** Jako użytkownik chcę mieć funkcję „Automatyczny Sheep Counter po bezczynności”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Po timeout ekran monitorowania uruchamia się bez utraty zadania.

## Epic: UX

### DOL-048 — Stały pasek statusu z priorytetami

**Priorytet:** P0 · **Etap:** M7 · **Story Points:** 5

**User story:** Jako użytkownik chcę mieć funkcję „Stały pasek statusu z priorytetami”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Błąd krytyczny zawsze ma pierwszeństwo przed żartem.

### DOL-049 — Opcjonalny sample beczenia ~22 kHz

**Priorytet:** P2 · **Etap:** M7 · **Story Points:** 3

**User story:** Jako użytkownik chcę mieć funkcję „Opcjonalny sample beczenia ~22 kHz”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Przy niskim Chip RAM sample jest pomijany z komunikatem.

### DOL-050 — locale.library z fallbackiem

**Priorytet:** P0 · **Etap:** M7 · **Story Points:** 5

**User story:** Jako użytkownik chcę mieć funkcję „locale.library z fallbackiem”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Polski katalog działa, a starszy OS dostaje wbudowany fallback.

## Epic: Safety

### DOL-051 — DecisionManager i pewność detekcji

**Priorytet:** P0 · **Etap:** M1 · **Story Points:** 8

**User story:** Jako użytkownik chcę mieć funkcję „DecisionManager i pewność detekcji”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Wątpliwe offsety mają uzasadnienie, pewność i możliwość korekty.

### DOL-052 — Dolly Gone Berserk

**Priorytet:** P1 · **Etap:** M7 · **Story Points:** 8

**User story:** Jako ekspert chcę mieć funkcję „Dolly Gone Berserk”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Niebezpieczne nadpisanie wymaga osobnego jawnego potwierdzenia.

## Epic: Scratch

### DOL-053 — Dyskowy ScratchStore dla małego RAM

**Priorytet:** P0 · **Etap:** M4 · **Story Points:** 8

**User story:** Jako użytkownik Amigi z małą pamięcią chcę mieć funkcję „Dyskowy ScratchStore dla małego RAM”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Indeksy są przetwarzane bez przekroczenia budżetu RAM.

### DOL-054 — AddBuffers po zgodzie

**Priorytet:** P1 · **Etap:** M7 · **Story Points:** 5

**User story:** Jako użytkownik chcę mieć funkcję „AddBuffers po zgodzie”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Bufory zmieniają się wyłącznie za zgodą i są przywracane, gdy bezpieczne.

### DOL-055 — Grupowanie I/O na wspólnej magistrali

**Priorytet:** P2 · **Etap:** M7 · **Story Points:** 8

**User story:** Jako użytkownik chcę mieć funkcję „Grupowanie I/O na wspólnej magistrali”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Planista ogranicza przełączenia bez zmiany wyników.

## Epic: Release

### DOL-056 — Publiczny workflow GitHub Scrum

**Priorytet:** P0 · **Etap:** M0 · **Story Points:** 5

**User story:** Jako współtwórca chcę mieć funkcję „Publiczny workflow GitHub Scrum”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Issues/PR template i kryteria akceptacji są dostępne publicznie.

### DOL-057 — MIT i Retroware

**Priorytet:** P0 · **Etap:** M0 · **Story Points:** 3

**User story:** Jako użytkownik chcę mieć funkcję „MIT i Retroware”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Brak prezentu nigdy nie ogranicza funkcji.

### DOL-058 — Walidacja sprzętowa i wydanie

**Priorytet:** P0 · **Etap:** M8 · **Story Points:** 13

**User story:** Jako tester chcę mieć funkcję „Walidacja sprzętowa i wydanie”, aby bezpiecznie i przewidywalnie wykonywać zadanie zgodnie z jej przeznaczeniem.

**Kryterium akceptacji:** Krytyczne testy z macierzy realnych Amig są zakończone i udokumentowane.

---

**Uwaga o aktualności:** M0 zawiera już częściowe implementacje wybranych historii. Szczegółowy status należy sprawdzać w Issues i [SPRINT_M0](SPRINT_M0.md), a nie w samych estymacjach backlogu.


## Epic: Storage safety / MaxTransfer diagnostics

### DOL-059 — Sprawdzanie de_MaxTransfer dla każdej partycji

**Priorytet:** P1 · **Etap:** M0/M1 · **Story Points:** 3 · [Issue #11](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/11)

**User story:** Jako użytkownik klonujący lub diagnozujący dysk Amigi chcę znać wartość MaxTransfer na każdej partycji i otrzymać ostrzeżenie, gdy przekracza konserwatywne `0x1FE00`, aby uniknąć ryzyka cichego uszkodzenia danych na niektórych konfiguracjach IDE.

**Akceptacja:** Raport pokazuje HEX i bajty, rozróżnia wartość `0x1FE00`, większą, mniejszą, zerową i niedostępną; ostrzega bez automatycznej zmiany. Uwzględnia profil sterownika, gdy jest znany. Nie utożsamia filesystemowego `de_MaxTransfer` z limitem pojedynczej operacji surowego I/O.

**Stan:** wstępna diagnostyka hostowa i CLI dodana; porównanie z aktywnymi mountlistami i profilami sterowników pozostaje otwarte.


## Epic: Diagnostics UX / Sheep Inventory

### DOL-060 — Sheep Inventory: szczegółowy inwentarz dowolnego obiektu

**Priorytet:** P1 (wstępnie) · **Etap:** M5 (UI), możliwość wcześniejszego prototypu CLI · **Story Points:** do rozbicia · [Issue #13](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/13)

**User story:** Jako użytkownik Dolly chcę otworzyć okno **Sheep Inventory** dla wybranego urządzenia, dysku, obrazu, partycji, woluminu lub systemu plików, by zobaczyć możliwie kompletne informacje techniczne, geometrię, właściwości, stan i zdiagnozowane niespójności.

**Akceptacja:** pola pogrupowane kontekstowo; źródło i status wiarygodności każdego pomiaru; Hex/bajty/LBA i inne jednostki, dane kontrolera oraz urządzenia, RDB/MBR, MaxTransfer/Mask, filesystem; niewspierane dane oznaczone jako nieznane. Okno tylko do odczytu, na małej pamięci 68000, z oddzielnym uruchamianiem kosztownych analiz, opcją kopiowania informacji i wersją CLI.

**Uwagi:** diagnostyka i informacja, nie naprawa. Wynik nie certyfikuje bezpieczeństwa dysku. Wspólne dane Inventory mogą kiedyś służyć Flock Managerowi, ale nie przenosimy tam żadnych prywatnych informacji automatycznie.


### DOL-061 — Komunikaty dla użytkownika przy problemach z DMA

**Priorytet:** P0 (bezpieczeństwo) · **Etap:** M0 / przed dopuszczeniem fizycznego I/O · [Issue #8](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/8)

**User story:** Jako użytkownik Dolly chcę otrzymać jasne, widoczne ostrzeżenie, kiedy program wykryje nieznany profil DMA, bufor poza maską adresową, niewłaściwe wyrównanie lub brak odpowiedniej pamięci, aby wiedzieć, dlaczego operacja została zatrzymana i co należy sprawdzić.

**Akceptacja:**
- Dla każdej odmowy operacji z powodu DMA: **kod diagnostyczny + zrozumiały komunikat + możliwa przyczyna + 1–3 konkretne, bezpieczne kroki rozwiązania problemu**.
- Jeśli rozwiązanie zależy od kontrolera i sterownika, komunikat ma wyraźnie wskazać co sprawdzić i nie podawać niezweryfikowanej wartości `Mask`, `MaxTransfer` czy `Alignment`.
- Wspólne wskazówki naprawcze wykorzystywane przez CLI, przyszłe GadTools i logi; brak automatycznej zmiany ustawień.
- Wersje EN/PL, z zachowaniem kodu błędu do wyszukiwania w dokumentacji; docelowo lokalizacja przez katalog Locale.
- Ostrzeżenie jest dostępne w CLI i przyszłym oknie GadTools; nie może ginąć jako anonimowe `EINVAL` lub `EUNSUPPORTED`.
- Program **zatrzymuje operację**, nie podejmuje samodzielnie ryzykownych prób innych masek, wyrównań ani zmiany ustawień.
- Komunikat odróżnia *niepotwierdzoną zgodność DMA* od *faktycznego uszkodzenia danych*; nie twierdzi, że dane są uszkodzone.
- W logu zachować urządzenie/unit oraz kod problemu, a adresy/maski ujawniać tylko w diagnostyce zaawansowanej, bez prywatnych danych.
- Przewidzieć lokalizację PL/EN (Locale), testy poprawności callbacku także na fake Exec, przypadki braku bufora.
- **Status:** wdrożono strukturalny katalog przyczyn i podpowiedzi, przenośne `dolly-dma-advice` CLI (w tym cel do kompilacji m68k), testy katalogu i ścieżki CLI dodane do CMake. Rzeczywiste okno GadTools, pełny CI i test AmigaOS pozostają otwarte.


## Epic: Modular GUI / low-resolution safety

### DOL-062 — GadTools jako jedyne GUI MVP; architektura niezaleznych backendow po MVP

**Priorytet:** P1 dla architektury, P0 dla 640×256 · **Etap:** MVP M3/M5 (GadTools), dodatkowe backendy poza MVP · [Issue #14](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/14)

**User story:** Dolly w MVP udostepnia tylko GadTools na AmigaOS 2.04+/68000; ReAction i MUI beda rozwijane i dostarczane jako oddzielne opcjonalne moduly dopiero po MVP.

**Zalozenia:** niezalezny od GUI core, wersjonowany interfejs backendu, bez wymuszonych zaleznosci na ReAction/MUI, czytelny fallback do GadTools w razie niedostepnosci pluginu. Dopiero po sprawdzeniu natywnego ABI projektujemy wczytywanie modulow. Kazde okno, dialog i komunikat GadTools musi miescic sie w 640×256 przy Topaz/8 z uwzglednieniem obramowania, tytulu i przyciskow; dlugie informacje dzielimy na strony.

**Font:** domyslnie rzeczywisty systemowy Screen Font; opcjonalny font ustawiony przez uzytkownika w Dolly ma pierwszenstwo, jesli okno sie miesci. W przeciwnym razie najpierw font ekranu, ostatecznie Topaz/8 i uklady kompaktowe. Gdy nawet to nie miesci sie bezpiecznie — odmowa otwarcia wadliwego okna i alternatywne powiadomienie/CLI.

**Status:** przenosny kalkulator geometrii/fallbacku, kontrakt GUI i testy graniczne sa opublikowane; rzeczywiste okna i plugin loader jeszcze nie sa zaimplementowane.

### DOL-063 — Powiadomienia o zmianie ekranu, fontu i ponowne przeliczanie GUI

**Priorytet:** P0 (niedopuszczenie do przyciecia kontrolek) · **Etap:** M3/M5 · [Issue #15](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/15)

**User story:** Dolly reaguje na zmiany ScreenMode i Screen Font, zamykanie/otwieranie ekranu publicznego oraz zmiany fontu w swoich preferencjach. Po zdarzeniu ponownie mierzy ekran, font, ramki i okna, zachowuje stan i wybiera bezpieczny layout.

**Implementacja:** AmigaDOS V36+ `StartNotify` dla `ENV:sys/screenmode.prefs` i `ENV:sys/screenfont.prefs`; niezalezny od systemu mechanizm laczenia zdarzen; `screennotify.library` jako **opcjonalny** adapter cyklu zycia ekranu, a nie wymagana biblioteka. Sam `IDCMP_NEWPREFS` nie wykrywa wszystkich zmian fontu/ekranu.

**Akceptacja:** brak uzywania starych wskaznikow `Screen*`; po zmianie ekran i fonty pozyskiwane na nowo, a GUI przebudowywane dopiero gdy ekran jest dostepny. Bez dodatkowej biblioteki nadal dziala powiadamianie o preferencjach, ponowne mierzenie przy otwarciu/aktywacji/zmianie rozmiaru; nie obiecujemy wykrycia kazdego zdarzenia zamkniecia ekranu w takim trybie. Sprawdzic OS 2.04, 3.1, 3.1.4 i 3.2 pod WinUAE.

**Status:** opublikowano model zdarzen, testy i eksperymentalny adapter AmigaDOS; natywna kompilacja i `screennotify.library` API niepotwierdzone.


### DOL-064 — Wspolny katalog programu przez AmigaDOS Assign DOLLY:

**Priorytet:** P1 · **Etap:** M0 (kontrakt sciezek), pozniejsze wydania (integracja runtime) · [Issue #16](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/16)

**User story:** Jako uzytkownik Dolly chce przechowywac program, konfiguracje, katalogi lokalizacyjne, dokumentacje i ewentualne przyszle pluginy w jednym katalogu dostepnym przez `DOLLY:`, niezaleznie od aktualnego katalogu powloki.

**Akceptacja:** `DOLLY:` jest jedynym logicznym rootem zasobow; przenosny mechanizm budowania sciezek odrzuca przeskoki `../`, bezposrednie sciezki `SYS:`, bledne segmenty i nadmierne dlugosci. Backend AmigaDOS ma weryfikowac, czy przypisanie wskazuje poprawny katalog, a w razie braku wyjasnic uzytkownikowi procedure naprawcza. Nie przestawiac Assign automatycznie na przypadkowy katalog. Systemowe `LIBS:`, `FONTS:` i `LOCALE:` nie sa nadpisywane.

**Stan:** `dolly_paths` + test GCC/Clang/ASan/UBSan lokalnie PASS; natywne rozwiazywanie Assign i test AmigaOS nadal otwarte.

### DOL-065 — Klasyczny skrypt instalacyjny Installer

**Priorytet:** P1 · **Etap:** przed pierwszym wydaniem do instalacji (po M0) · [Issue #16](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/16)

**User story:** Jako uzytkownik AmigaOS chce zainstalowac Dolly w wybranym katalogu, utworzyc przypisanie `DOLLY:` i opcjonalnie zapisac je w `S:User-startup`, z zachowaniem dotychczasowych ustawien oraz mozliwoscia bezpiecznej aktualizacji.

**Akceptacja:** uzyc **klasycznego Commodore Installer**, nie pythonowej Installation Utility z OS4. Instalacja binarki i zasobow, pytanie o nadpisanie istniejacego Assign, opt-in dla trwalego wpisu w User-startup, bez duplikatow i z kopia zapasowa; aktualizacja, uninstall i obnizenie wymagan na dyskietkowych Amigach. Nie zakladac obecnoci programu Installer na kazdym czystym OS 2.04; udostepnic instrukcje instalacji recznej, jesli potrzebna.

**Stan:** wymagania i `docs/INSTALLATION.md` zapisane; skrypt Installer pozostaje do wykonania po M0.

## Epic: Post-MVP GUI backends

### DOL-066 — Opcjonalny zewnetrzny backend ReAction

**Priorytet:** P2 · **Etap:** wyłącznie po MVP · [Issue #14](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/14)

**User story:** Jako uzytkownik nowszego systemu chce osobno doinstalowac backend ReAction bez zmiany silnika Dolly i bez uzalezniania wydania podstawowego od klas ReAction.

**Akceptacja:** odrebny modul/pakiet, kompatybilnosc ABI, rozpoznanie dostepnych klas, mozliwosc powrotu do GadTools, PL/EN, obsluga zmiany Screen Font i niski narzut pamieci. Nie jest warunkiem zadnego wydania MVP.

### DOL-067 — Opcjonalny zewnetrzny backend MUI

**Priorytet:** P2 · **Etap:** po MVP, w dalszym rozwoju · [Issue #14](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/14)

**User story:** Jako uzytkownik posiadajacy MUI chce w przyszlosci osobno zainstalowac backend GUI dla Dolly, przy niezmienionym silniku oraz pelnej dostepnosci GadTools.

**Akceptacja:** niezalezny modul i zaleznosci, jawne wykrywanie instalacji MUI, bezpieczny fallback do GadTools, brak narzutu w standardowym MVP, mozliwosc uzycia `DOLLY:Plugins/`. Nastepny etap po podstawowym wydaniu.


## Epic: Optional resources and verified device knowledge

### DOL-068 — Missing, unreadable or incompatible DOLLY: files never block other features

**Priority:** P0 (safety, predictable startup) · **Stage:** M0 contract; MVP integration · [Issue #17](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/17)

**User story:** Dolly identifies each missing or invalid optional file, gives its complete `DOLLY:` path, names the feature that is disabled and suggests how to restore it. Other features and the GadTools MVP can still start.

**Mandatory version gate:** Every optional resource must match the exact **current Dolly release/build ID** **and its expected format/API version**, and its own integrity / binding to its version metadata must be verified. An absent, unreadable, corrupted, unversioned, older or newer file is **never loaded**, even if the file name is correct; only its dependent feature is disabled. Messages distinguish these failure types (for example, "Plik z innego stada — wersja nie pasuje!"). A future native provider must inspect the actual resource bytes, not just a neighboring sidecar or a file name.

**Acceptance:** No message for an optional add-on which was not requested; concise dialog at 640×256 with details/pagination, exact affected path, required and found version when known, affected capability and safe reinstallation/update guidance. Existing core operations remain available; any physical DMA path remains blocked if the only profile was rejected. Do not automatically download, replace or force-load incompatible data.

**Status:** a pure C99 optional-resource manifest and strict version/format/integrity decision path are in source and tests; native file probing and the real startup/GadTools integration remain open.

### DOL-069 — Versioned, sourced controller databases for DMA help and later auto-configuration

**Priority:** P1 for read-only help; P2 for optional automatic configuration · **Stage:** M0 design, MVP diagnosis if validated, advanced changes post-MVP · [Issue #18](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/18)

**User story:** If a device has a DMA or transfer problem, Dolly searches a compatible database in `DOLLY:Plugins/Devices/` and shows the **known problem description, original evidence/reference, matched controller, driver and revision**, then suggests safe diagnostic steps without guessing values.

**Acceptance:** device/vendor documentation or carefully documented real hardware tests, database release/build ID, format/schema version, integrity checks, detailed hardware and driver matching, provenance/confidence, contradiction handling. Unmatched/ambiguous/unverified versions fall back to generic DMA help, not to universal unverified parameters. A verified database match never enables raw DMA operations by itself. Automatic profile selection requires independent runtime verification and explicit safety rules; no user-visible "safe" guarantee based solely on plugin text.

**Status:** initial profile matcher and sourced DMA advice composition are in source, with example synthetic profiles and tests; real curated hardware entries, native loader, versioned database package and GUI integration remain open.


## M0 integrity, boot rescue and controlled device configuration

### DOL-070 — Integralność programu i dodatków: wersja + CRC32

**Priorytet:** P0 dla poprawnego startu, P1 dla paczki wydaniowej · **Etap:** M0/MVP · [Issue #19](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/19)

**Decyzja:** wyłącznie **identyfikator bieżącego wydania + suma kontrolna CRC32**, bez obowiązkowych podpisów cyfrowych czy rozbudowanego systemu kryptograficznego. W paczce wydania manifest zawiera ścieżkę, numer wydania, długość i CRC32 każdego pliku. Dodatki dodatkowo weryfikują format/API.

**Program główny:** pakiet jest sprawdzany przed dystrybucją, a **sam główny program Dolly** po rozpoczęciu działania odczytuje swój rzeczywisty plik wykonywalny i sprawdza wersję, długość oraz CRC32 **przed udostępnieniem operacji dyskowych**. Program nie może samodzielnie udowodnić autentyczności własnego pliku; manifest z tej samej paczki chroni jedynie przed **przypadkowym uszkodzeniem** lub przypadkową podmianą, nie celowym atakiem.

**Dodatki:** błędne CRC32, inny rozmiar, niezgodne wydanie i niezgodne API skutkują wyłączeniem tylko zależnej funkcji oraz dokładnym ostrzeżeniem: ścieżka, przyczyna, konsekwencje i sposób naprawy. GadTools i core nadal startują. Baza urządzeń odrzucona za błąd CRC nie może ustawiać parametrów DMA.

**Stan:** przenośny streaming CRC32, generator i offline weryfikator manifestu, testy oraz logika odmowy użycia niezgodnych plików dodane do źródeł. Brak jeszcze natywnej integracji odnajdywania własnego pliku wykonywalnego, odczytu plików AmigaDOS i testu WinUAE; oddzielny bootstrap nie jest planowany.

### DOL-071 — Minimalna dyskietka ratunkowa Dolly z GadTools

**Priorytet:** P2 · **Etap:** przygotowanie po M0, pełne wydanie w dalszym etapie · [Issue #20](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/20)

**Cel:** oddzielny wariant **Dolly Rescue**: maksymalnie mały program z obowiązkowym GadTools, podstawową diagnostyką i bezpiecznymi odczytami. Stworzyć powtarzalny proces przygotowania bootującej dyskietki DD 880 KiB lub obrazu ADF, mierzący miejsce na pliki i wymagania pamięci.

**Akceptacja:** ograniczyć zależności do minimum, nie dołączać dodatkowych GUI, ciężkich bibliotek, rozbudowanych baz i zbędnych katalogów. Wariant ma obsługiwać brakujące dodatki i CRC32/wersję. Uwzględnić wymagane komponenty AmigaOS i ich licencje — bez nieuprawnionej redystrybucji Workbencha/Kickstartu. Testy w WinUAE 68000/2.04 i na rzeczywistym komputerze. Nie deklarować, że cały pakiet się zmieści, dopóki nie zostanie zbudowany i zmierzony.

### DOL-072 — Wprowadzanie konfiguracji z baz urządzeń do ustawień Dolly

**Priorytet:** P1/P2 · **Etap:** M0 model propozycji, użycie po niezależnej walidacji sprzętu · [Issue #21](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/21)

**Cel:** baza kontrolerów (o potwierdzonym wydaniu/CRC32/API) może podpowiadać i przygotowywać **lokalne ustawienia Dolly**, zamiast ograniczać się do ogólnych porad. Zmiana pokazuje ustawienie przed/po, źródło wiedzy i wymaga zgody użytkownika.

**Ograniczenia:** w obecnym M0 dopuszczamy wyłącznie propozycję zmniejszenia lokalnego rozmiaru transferu — zgodną z rozmiarem sektora, limitem 64 KiB oraz dokładnie rozpoznanym modelem/sterownikiem/rewizją. Bez automatycznej zmiany Mask, ustawień AmigaDOS, RDB lub sprzętu; profil nigdy sam nie autoryzuje DMA. Dodać rollback, wykrywanie konfliktów, rozdzielenie danych obserwowanych od deklarowanych i testy realnych kontrolerów przed aktywacją automatyczną.

**Stan:** model propozycji i testy jednostkowe dodane do repozytorium; faktyczna aplikacja ustawień/GUI i walidacja kontrolera pozostają otwarte.
