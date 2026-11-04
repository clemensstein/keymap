<img width="1635" height="580" alt="image" src="https://github.com/user-attachments/assets/fe29702c-7ece-41ed-bf31-d3208876e438" />

<img width="1645" height="578" alt="image" src="https://github.com/user-attachments/assets/701fda5f-6462-4819-a56a-0f2283b2b27d" />

<img width="1643" height="584" alt="image" src="https://github.com/user-attachments/assets/70210d19-22b9-4a83-9a34-572ee10e942b" />

<img width="1367" height="474" alt="image" src="https://github.com/user-attachments/assets/a29a5473-c26e-44ec-9060-989fab3a449b" />





## Toucan2

Konfiguration für die beekeeb Toucan2. Die Shields `toucan` und `nice_view_gem` stammen aus [beekeeb/zmk-keyboard-toucan2](https://github.com/beekeeb/zmk-keyboard-toucan2) (MIT, siehe `boards/shields/toucan/LICENSE-beekeeb`).

- **Keymap:** `config/toucan.keymap`, abgeleitet von `config/piantor_pro_bt.keymap`.
- **Ebene 4 (MAUS):** aktiv, solange das Trackpad berührt wird. Die Daumentasten sind dann Mausklicks.
- **Ebene 5 (SYSTEM):** Ebene 1 und Ebene 2 gleichzeitig halten. Dort liegen Bluetooth-Profile, `&bootloader` (links oben außen für die linke Hälfte, rechts oben außen für die rechte), `&studio_unlock` und `&sys_reset`.
- **Windows-Modus:** in `boards/shields/toucan/toucan.dtsi` per `#define TOUCAN_WIN_MODE` aktiviert.
- **Bluetooth-Einstellungen:** `config/toucan_left.conf` und `config/toucan_right.conf`.

Firmware bauen: GitHub → Actions → **Build Toucan2 firmware** → **Run workflow** (für die Piantor entsprechend **Build Piantor Pro BT firmware**). Gebaut wird nur manuell, nicht bei jedem Push. Dateien im Artefakt `toucan2-firmware`:

- `toucan_left rgbled_adapter nice_view_gem-seeeduino_xiao_ble-zmk.uf2` → linke Hälfte
- `toucan_right rgbled_adapter-seeeduino_xiao_ble-zmk.uf2` → rechte Hälfte
- `settings_reset-seeeduino_xiao_ble-zmk.uf2` → setzt gespeicherte Einstellungen zurück

Änderungen aus ZMK Studio überschreiben die Keymap aus der Firmware. Nach dem Flashen einer neuen Keymap deshalb in ZMK Studio „Restore Stock Settings“ ausführen.
