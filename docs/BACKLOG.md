# Dolly — Product Backlog (wersja publiczna)

**58 wstępnych user stories** (szacunki punktów i przydziały do etapów są orientacyjne). Główne śledzenie wybranych prac: [GitHub Issues](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues). Pełny backlog nie oznacza, że wszystkie zadania są już utworzone jako Issues.

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
