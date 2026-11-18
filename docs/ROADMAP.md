# Dolly — Roadmap publiczny (plan, nie obietnica dat)

Dolly pozostaje darmowym projektem **MIT + dobrowolne Retroware**. Każda faza wymaga walidacji bezpieczeństwa i zgodności 68000/AmigaOS. Kolejność może zostać zmieniona przez wyniki testów.

| Etap | Priorytet funkcjonalny | Warunek przejścia |
|---|---|---|
| **M0** | Repo, CI, parsowanie bez zapisu, BlockSource, prototyp AmigaOS I/O | Testy hosta, analiza ryzyk i kompilacja natywna |
| **M1** | Rzeczywiste device/unit, NSD/TD64, topologia, RDB/MBR/PiStorm | Bezpieczne odczyty i jasne granice jednostek |
| **M2** | RAW, ADF/HDF, kopiowanie, retry i read-back verify | Fixture, brak nadpisania źródła, odporność na I/O |
| **M3** | Binarny `FORM DOLY`, segmenty, kompresja, checksumy, resume | Roundtrip i testy przerwanych zapisów |
| **M4** | OFS/FFS, bitmapy, własność bloków, read-only DiskDoctor | Golden fixtures DOS0–DOS7, brak zapisów |
| **M5** | Smart Clone, Used Blocks Only, odbudowa i zmiana rozmiaru | Weryfikacja nowego woluminu i bootowalności |
| **M6** | Odzysk i kontrolowane plany napraw | Naprawa tylko po zgodzie, kopie i regression tests |
| **M7** | GadTools, ReAction, Wizard, Sheep Counter, locale, scratch | Testy 68000 / Chip RAM, ergonomia i stabilność |
| **M8** | Macierz sprzętowa, publikacja stabilna i dokumentacja | Potwierdzone testy na autentycznych Amigach |

## Stan od M0 do bieżących prototypów

- **M0** dostarczył BlockSource, narzędzia hostowe, odczyt RDB/MBR, analizę kolizji i pojemności oraz strukturalny inspektor IFF. Przebieg odnotowano w [historii M0](SPRINT_M0.md).
- **M1** przyniósł kompilację MC68000, główne okno GadTools, selektory urządzeń, Sheep Inventory oraz [Surface Scan](SURFACE_SCAN.md). W FS-UAE zweryfikowano również odczyt wybranego zakresu przez granicę 4 GiB.
- **Zakres M2** ma działający prototyp [Create Image](CREATE_IMAGE.md) dla RAW i HDF. Źródło jest tylko do odczytu. Otwarta pozostaje walidacja innych urządzeń docelowych, ADF i weryfikacja po zapisie w aplikacji.
- **Zakres M3** ma jednoplikowy zapis [IFF DOLY](IFF_FORMAT.md): v1 bez kompresji i v2 z PackBits/LZSS. Weryfikacja na hoście porównała dane z obrazów emulowanych ze źródłem. Segmentacja, wznowienie i odtwarzanie pozostają otwarte.

- **Alpha 3** dodaje zakres LBA/CHS i ponowienia do Surface Scan, pauzę,
  podgląd Restore oraz checkpoint Create Image po awarii. Następne
  etapy obejmują parser i GUI DLRP, kwalifikację celu oraz kontrolowany
  zapis Restore i Copy/Clone. Zmiana rozmiaru pozostaje w planie.

Status wydań i sprzętu opisują [README](../README.md) i [macierz testów](TEST_MATRIX.md).

## Po MVP — Dolly Flock Manager

Osobna **[epika #7](https://github.com/tomaszkmarcinkowski-alt/dolly-amiga/issues/7)**: wspólny system bazowy na stadzie różnych Amig, profile sprzętowe, nakładki konfiguracji i zachowanie lokalnych danych. Docelowo aktualizacje offline i/lub przez sieć, z historią wersji, wykrywaniem konfliktów i jawną zgodą.

**Nie należy uznawać Flock Managera za część zakresu stabilnego 1.0.** Rozszerzenia architektury mogą zachowywać przyszłą możliwość jego realizacji.

## Dokumenty projektowe

- [Backlog](BACKLOG.md)
- [Aktualny M0](SPRINT_M0.md)
- [Dziennik techniczny](ENGINEERING_JOURNAL.md)
- [Format IFF](IFF_FORMAT.md)
- [Natywne I/O](AMIGA_IO.md)
