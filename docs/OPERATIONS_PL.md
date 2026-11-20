# Operacje Dolly krok po kroku

Przewodnik opisuje natywny build testowy Alpha 3.4 dla Amigi MC68000 i AmigaOS 2.04+. Nazwy kontrolek odpowiadają angielskiemu interfejsowi Dolly.

## Wybór źródła i celu

1. Wybierz **Operation**. Dolly uaktywni typy źródeł w **Source** i, po wyborze źródła, odpowiednie typy w **Destination**. Ramki pod przyciskami pokazują wybrane obiekty.
2. **Physical device** wskazuje cały unit sterownika. **Partition** wskazuje zakres partycji odczytany z tablicy partycji. **Logical device** wskazuje urządzenie AmigaDOS, którego sterownik, unit i zakres Dolly odczytuje z konfiguracji DOS. **File** wskazuje plik wybrany w ASL.
3. Przy **Start** Dolly ponownie sprawdza geometrię i zakres potrzebny danej operacji. Dla surowego I/O bierze parametry **Mask**, **MaxTransfer** i **BufMemType** z RDB oraz stosuje ręczne wartości z **Settings**, jeśli je podano. Przy brakujących danych proponuje bezpieczne wartości i umożliwia przerwanie pracy w celu zmiany ustawień.
4. Starsze wersje `scsi.device` mają domyślną blokadę adresowania powyżej 4 GiB, z kwalifikowanym wyjątkiem UAE-IDE dla odczytu. Dolly sprawdza dostępne polecenia dla danego zakresu osobno. Sam override limitu starego sterownika nie włącza obsługi poleceń 64-bitowych. Zapis powyżej granicy wymaga obsługi NSD READ64 i WRITE64.
5. Gdy RDB deklaruje sektory poza pojemnością raportowaną przez sterownik, selektor pokazuje różnicę. Ustawienie **Allow partitions outside range** pozwala wybrać sposób postępowania dla obsługiwanych odczytów. Opcja uzupełniania zerami zachowuje zadeklarowaną długość. Zapis na partycję wymaga zakresu w granicach sterownika.

## Przygotowanie surowego zapisu i zakończenie

Ten przebieg jest wspólny dla **Sector-to-sector**, **Smart Copy** i **Restore Image**.

1. Dolly sprawdza zależności między zakresami źródła i celu, a przy obrazie również woluminy wszystkich plików obrazu. Potwierdzone nakładanie blokuje operację. Osobny komunikat informuje o nierozpoznanej lokalizacji obrazu. Ostrzeżenie o spadku prędkości na tym samym unicie można wyłączyć w ustawieniach.
2. Źródłowe woluminy wymagają odizolowania przez `INHIBIT`. Dla zamontowanych woluminów celu Dolly prosi o zgodę na odmontowanie. Gdy wpis DOS istnieje bez aktywnego woluminu, Dolly po cichu wstrzymuje działający handler i na celu próbuje usunąć wpis. Może kontynuować po nieudanym usunięciu, jeśli wpis jest nadal nieaktywny lub ten sam handler pozostał wstrzymany, a wolumin się nie pojawił. `AUTOMOUNT` określa politykę RDB, a bieżący stan pochodzi z listy DOS.
3. **Warning Review** pokazuje ostrzeżenia o nadpisaniu, rozmiarze, montowaniu i dostępie sterownika. Użytkownik zatwierdza kolejne pozycje lub anuluje operację. Okno nie ma odliczania. Smart Copy może wcześniej poprosić o odizolowanie źródła, aby odczytać stabilną bitmapę.
4. Po akceptacji Dolly ponownie sprawdza nośnik, geometrię, porównuje odciski sektorów brzegowych i sprawdza uchwyty sterownika. Niewyjaśnione uchwyty wywołują ostrzeżenie **Driver access**, ponieważ Exec podaje wspólny licznik dla całego sterownika. Zamknięcie wcześniej zaakceptowanego uchwytu jest dopuszczalne, jeśli stan DOS nadal spełnia warunki. Pojawienie się nowego dostępu ponad sprawdzony stan zatrzymuje zapis.
5. Dolly przesyła dane w ograniczonych porcjach, zapisuje i opróżnia bufor sterownika. **Verify writes** dodaje odczyt zwrotny i porównanie każdej porcji. Po udanym kopiowaniu Dolly może zaproponować zmianę nazw. Zwalnia wstrzymania źródła i ponawia próbę usunięcia pozostałych nieaktywnych wpisów celu. Automatyczne montowanie celu po operacji jest zaplanowane.

Szczegóły i wymagania sterowników opisują [Device writes](DEVICE_WRITE.md) oraz [AmigaOS I/O](AMIGA_IO.md).

## Sheep Inventory

1. Wybierz **Inventory** oraz **Physical device** lub **Partition**. Dolly pokazuje driver, unit, nazwę, geometrię, pojemność i dane ATA IDENTIFY, jeśli sterownik je udostępnia. Widok odczytuje tablicę całego wskazanego unitu.
2. Wybierz pozycję po lewej, aby obejrzeć szczegóły partycji. Kliknięcie pozycji nagłówka lub szczegółów kopiuje jej tekst do schowka. Lewa lista służy do wyboru partycji.
3. **Backup RDB** otwiera ASL i zapisuje zweryfikowany zakres od LBA 0 przez zarezerwowany obszar RDB. Gdy brakuje bezpiecznego zakresu, przycisk jest wyszarzony. **Backup MBR** dla obsługiwanego układu zapisuje pełny sektor LBA 0.
4. **Restore RDB** otwiera potwierdzenie z odliczaniem 10 sekund. Dolly sprawdza zgodność kopii z docelowym RDB, kwalifikację sterownika i stan offline. Następnie zapisuje obowiązkową kopię odzyskiwania, ponownie odczytuje bieżący stan, zapisuje zmienione sektory i weryfikuje cały obszar. To odzyskiwanie zgodnych metadanych RDB. **Restore MBR** pozostaje zadaniem do wdrożenia.

Zobacz [Sheep Inventory](SHEEP_INVENTORY.md).

## Surface Scan

1. Wskaż urządzenie, partycję albo urządzenie logiczne. Parametry skanu domyślnie obejmują cały obiekt. Możesz ograniczyć zakres przez początkowy i końcowy LBA lub CHS i ustawić liczbę prób ponowienia.
2. Dolly weryfikuje zakres i przygotowuje źródło. Czyta kolejne porcje sektorów. Gdy odczyt porcji się nie powiedzie, sprawdza jej sektory pojedynczo i stosuje podany limit ponowień.
3. Główne okno pokazuje postęp, a **Details** otwiera Sheep Counter. Na siatce **R** oznacza sektor odzyskany po ponowieniu, a **X** co najmniej jeden nieodczytany sektor w danym polu.
4. Po operacji **Show Log** pokazuje urządzenie, geometrię i zdarzenia z absolutnym LBA oraz CHS, gdy można go wyliczyć. **Save Log** zapisuje log tekstowy przez ASL.

Zobacz [Surface Scan](SURFACE_SCAN.md).

## Create Image

1. Wybierz urządzenie, partycję lub urządzenie logiczne. **Destination** otwiera ASL do zapisu. **RAW image** zapisuje sektory w kolejności i może mieć np. rozszerzenie `.raw` lub `.hdf`. **IFF DOLY** udostępnia **Not compressed**, **Fast** (PackBits) i **Compact** (LZSS z wyborem kodowania porcji).
2. Dolly sprawdza zgodność lokalizacji pliku ze źródłem, dostępność miejsca i granicę pojedynczego pliku 2 GiB minus 1 bajt. Dla obrazu przekraczającego limit proponuje podział w IFF DOLY i podaje liczbę części. Ostrzeżenie o wolnym miejscu podaje szacowany niedobór, a dla kompresji górny szacunek potrzebnego miejsca.
3. Dolly izoluje źródło i czyta porcje sektorów, a wynik zapisuje w tymczasowej lokalizacji. IFF DOLY zapisuje metadane zakresu i sumy kontrolne. Zestaw dzielony ma plik główny `plik.dly`, kolejne części `plik.001`, `plik.002` itd. oraz metadane zestawu w częściach.
4. Jeśli zapis części się nie powiedzie, można wybrać inny katalog dla tej części. Po sprawdzeniu nowego celu Dolly ponawia część od początku. Gotowy wynik publikuje po zamknięciu plików. Istniejący plik końcowy powoduje odmowę.

Zobacz [Create Image](CREATE_IMAGE.md), [duże obrazy](LARGE_IMAGE_FILES.md) i [format IFF DOLY](IFF_FORMAT.md).

## Restore Image

1. **Source** otwiera ASL. Dolly rozpoznaje obraz surowy RAW/HDF lub IFF DOLY. Wybierz całe urządzenie albo partycję RDB jako cel.
2. Dla zestawu dzielonego wybierz pierwszy plik. Dolly sprawdza listę części, numerację i tożsamość zestawu. Brakującą lub błędną część można wskazać z innego katalogu przez **Locate files...**.
3. Dolly ocenia rozmiar, rodzaj obrazu i metadane możliwe do odczytania. Rozbieżności opisuje w **Warning Review**. Po akceptacji różnicy rozmiaru zapis obejmuje wspólny zakres. Mniejszy obraz pozostawia końcówkę celu bez zmian. Większy obraz traci końcówkę przy zapisie i może dać niekompletny system plików.
4. IFF DOLY przechodzi pełną walidację danych i sum kontrolnych przed pierwszym zapisem. Błąd części zatrzymuje operację. Następnie Dolly zapisuje sektory w granicach zatwierdzonego zakresu i wykonuje opcjonalną weryfikację.

## Copy

### Sector-to-sector

1. Wybierz dwa całe urządzenia albo dwie partycje RDB w zakresie sterownika. Dolly wymaga równej długości i wielkości sektora oraz odrębnych zakresów.
2. Po zatwierdzeniu ostrzeżeń Dolly odczytuje i kopiuje każdy sektor. Przy kopii całego dysku obejmuje to również tablicę partycji oraz sektory niezajęte przez system plików.

### Smart Copy

1. Wybierz dla każdego końca **Partition** lub **Logical device**. Mieszane pary działają w obu kierunkach. Dolly przypisuje urządzenie logiczne do konkretnej partycji RDB i sprawdza brak nakładania, równą długość, wielkość bloku, bloki zarezerwowane i DosType.
2. Dolly izoluje źródło, sprawdza OFS/FFS i odczytuje bitmapę. Status i pasek postępu informują o sprawdzaniu stron bitmapy. Uszkodzona bitmapa lub niezgodna struktura blokuje operację.
3. Plan w **Warning Review** pokazuje zajęte bloki wraz z metadanymi, rozmiar danych do zapisu, pomijane bloki i liczbę zakresów. Po potwierdzeniu Dolly sprawdza RDB i zgodność planu z aktualną bitmapą.
4. Dolly zapisuje wyłącznie bloki zajęte i zarezerwowane na odpowiadające im pozycje celu. Poprzednia zawartość wolnych bloków celu zostaje zachowana. Opcjonalna weryfikacja obejmuje zapisane zakresy.

### Clone and resize

1. Wybierz dwie partycje RDB. Dolly czyta bitmapę źródła FFS i porównuje ją z rozmiarem celu.
2. **Warning Review** pokazuje ostatni zajęty blok, minimalny rozmiar przy zachowaniu położenia bloków oraz zajęte bloki poza zakresem celu. Dla większego celu analizuje położenie przyszłego bloku głównego i dodatkowe strony bitmapy.
3. Build udostępnia analizę i podgląd metadanych w pamięci. Właściwe skalowanie jest zaplanowane.

### Zmiana nazw po kopii lub przywracaniu

1. Po kopii całego dysku Dolly może zaproponować zmianę kolidujących nazw RDB, np. `DH0 → DH0_0`.
2. Dla rozpoznanego pełnego woluminu OFS/FFS Dolly może oddzielnie zaproponować zmianę etykiety, np. `Work → Copy_of_Work`. Niesformatowana partycja może mieć nazwę urządzenia w RDB, a etykieta woluminu wymaga poprawnego bloku głównego.
3. Każde okno pokazuje stare i nowe nazwy. **Rename** zatwierdza daną grupę, a **Cancel** pozwala zachować nazwy. Dolly weryfikuje zaakceptowane zmiany na odizolowanym celu.

## Disk Vet

Dostępne są selektory **Analyze (read only)** i **Analyze & repair**. Drugi udostępnia **Repair in place** lub **File recovery** z wyborem katalogu przez ASL. Build udostępnia konfigurację przyszłej operacji. Analiza, naprawa i odzyskiwanie plików są zaplanowane.

## Postęp, przerwanie i pliki DLRP

- **Start** podczas pracy zmienia się w **Pause**. **Stop** wstrzymuje operację i pozwala ją przerwać lub wznowić. **Quit** przy aktywnej operacji sygnalizuje blokadę w pasku statusu.
- Przy nieoczekiwanym błędzie zapisu lub odczytu Dolly może zaproponować zapis **Operation Restore File** w formacie IFF DLRP dla obsługiwanych przypadków Create Image, Restore Image i kopiowania sektorowego. Zapis DLRP zachowuje informacje o stanie operacji. Surface Scan i ręczny Stop/Quit pozostają poza tym mechanizmem.
- Odczyt DLRP i wznowienie w GUI są zaplanowane. Smart Copy wymaga osobnego zapisu zakresów, a zestaw dzielony ma odrębne odzyskiwanie części.

Zobacz [format DLRP](DLRP_FORMAT.md) i [macierz testów](TEST_MATRIX.md).
