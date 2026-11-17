# Dolly — Roadmap publiczny (plan, nie obietnica dat)

Dolly pozostaje darmowym projektem **MIT + dobrowolne Retroware**. Każda faza wymaga walidacji bezpieczeństwa i zgodności 68000/AmigaOS. Kolejność może zostać zmieniona przez wyniki testów.

| Etap | Priorytet funkcjonalny | Warunek przejścia |
|---|---|---|
| **M0** | Repo, CI, parsowanie bez zapisu, BlockSource, prototyp AmigaOS I/O | Hostowe testy, zidentyfikowane ryzyka, prawdziwy native build nadal blocker |
| **M1** | Rzeczywiste device/unit, NSD/TD64, topologia, RDB/MBR/PiStorm | Bezpieczne odczyty i jasne granice jednostek |
| **M2** | RAW, ADF/HDF, kopiowanie, retry i read-back verify | Fixture, brak nadpisania źródła, odporność na I/O |
| **M3** | Binarny `FORM DOLY`, segmenty, kompresja, checksumy, resume | Roundtrip i testy przerwanych zapisów |
| **M4** | OFS/FFS, bitmapy, własność bloków, read-only DiskDoctor | Golden fixtures DOS0–DOS7, brak zapisów |
| **M5** | Smart Clone, Used Blocks Only, odbudowa i zmiana rozmiaru | Weryfikacja nowego woluminu i bootowalności |
| **M6** | Odzysk i kontrolowane plany napraw | Naprawa tylko po zgodzie, kopie i regression tests |
| **M7** | GadTools, ReAction, Wizard, Sheep Counter, locale, scratch | Testy 68000 / Chip RAM, ergonomia i stabilność |
| **M8** | Macierz sprzętowa, publikacja stabilna i dokumentacja | Potwierdzone testy na autentycznych Amigach |

## Aktualny M0 (17 listopada 2026)

- Dostępny kod read-only: `dolly-inspect`, `dolly-rdb`, `dolly-layout`, `dolly-iff`.
- Strukturalny parser IFF `FORM DOLY` nie jest jeszcze pełnym czytnikiem DLY.
- `OpenDevice/DoIO` działa wyłącznie jako **prototyp poddany testowi na symulowanym Exec**; brak kompilacji 68000 i testów na rzeczywistym kontrolerze.
- Lokalnie: **8/8 testów CTest z GCC**, **7/7 z Clang i ASan/UBSan przed dodaniem testu integracyjnego CLI**. Wynik nowego GitHub Actions niepotwierdzony.
- Otwarte zagrożenia: duże stosy RDB, DMA/MaxTransfer, brak TD64, pełna kontrola struktur rezerwowych RDB.

## Po MVP — Dolly Flock Manager

Osobna **[epika #7](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/7)**: wspólny system bazowy na stadzie różnych Amig, profile sprzętowe, nakładki konfiguracji i zachowanie lokalnych danych. Docelowo aktualizacje offline i/lub przez sieć, z historią wersji, wykrywaniem konfliktów i jawną zgodą.

**Nie należy uznawać Flock Managera za część zakresu stabilnego 1.0.** Rozszerzenia architektury mogą zachowywać przyszłą możliwość jego realizacji.

## Dokumenty projektowe

- [Backlog 58 stories](BACKLOG.md)
- [Aktualny M0](SPRINT_M0.md)
- [Dziennik techniczny](ENGINEERING_JOURNAL.md)
- [Format IFF](IFF_FORMAT.md)
- [Natywne I/O](AMIGA_IO.md)
