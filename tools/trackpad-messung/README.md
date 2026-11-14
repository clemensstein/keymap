# Trackpad-Messung (Windows)

Misst, wie gleichmäßig die Bewegungsmeldungen von Trackpad und Maus am PC ankommen. Jede Meldung wird über die Windows-Raw-Input-API mit Zeitstempel aufgezeichnet und pro Gerät ausgewertet.

## Bauen

MSVC (Developer Command Prompt):

```
cl /O2 /EHsc trackpad_messung.cpp user32.lib hid.lib
```

MinGW:

```
g++ -O2 -static -o trackpad_messung.exe trackpad_messung.cpp -luser32 -lhid
```

## Messen

```
trackpad_messung.exe 20 kabellos.csv
```

Nach einem Countdown von 3 Sekunden läuft die Messung 20 Sekunden. Bewege den Cursor in dieser Zeit in langen, schnellen Zügen hin und her. Das Programm zeigt pro Gerät:

- **Melderate:** typischer Abstand zwischen zwei Meldungen. Per Bluetooth sind höchstens 7,5 ms (133 Hz) möglich.
- **Lücken:** Abstände über dem 1,8-Fachen des typischen Abstands. Das sieht man als Stocken.
- **Doppelpakete:** zwei Meldungen in unter 1,5 ms. Ein Stau wurde nachgeholt.
- **Verteilung:** Histogramm aller Abstände.

Die Rohdaten (Zeit, Gerät, dx, dy) landen in der CSV-Datei.

Für einen Vergleich einmal kabellos und einmal mit der linken Hälfte am USB-Kabel messen. Gern auch einmal die Maus.
