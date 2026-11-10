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
- **Display:** Stil 3 (HUD, `boards/shields/nice_view_gem/widgets/hud.c`), gewählt in `config/toucan_left.conf` über `CONFIG_TOUCAN_STATUS_SCREEN`.

### Trackpad-Anpassungen

Gegenüber der beekeeb-Vorlage geändert. Trackpad-Werte gehören zur **rechten** Hälfte, Verbindung zum PC und Maus-Ebene zur **linken**.

**Trackpad** (`boards/shields/toucan/toucan_right.overlay`):

| Einstellung | beekeeb | jetzt | Warum |
|---|---|---|---|
| `invert-scroll-y` | gesetzt | entfernt | Scrollrichtung (zwei Finger) umgedreht |
| `report-rate-active` | Chip-Standard | `4` ms (250 Hz) | Deutlich schneller als das 7,5-ms-Funkintervall zwischen den Hälften. Bei 125 Hz bekam manches Funkfenster keinen neuen Messwert, das nächste zwei. Das ruckelte bei schnellen, langen Bewegungen. |
| `report-rate-idle`, `-lp1`, `-lp2` | Chip-Standard, LP2 `640` ms | je `20` ms | Erste Berührung nach einer Pause wird sofort erkannt, wie bei einem Laptop. Kostet etwas mehr Akku. |
| `sensitivity` | `100` | `85` | Zeiger etwas langsamer |
| `filter-settings` | `0x0B` | `0x09` | Mittelwert-Filter (MAV) aus, er ließ den Zeiger nachziehen. Dynamischer IIR-Filter und ALP bleiben an. |

**Gesten und Scrollen** (`boards/shields/toucan/toucan.dtsi`):

- `#define TOUCAN_WIN_MODE` aktiv: Wischgesten als Windows-Kürzel.
- Zoom-Geste sendet Strg + `-` / `+` für das **deutsche** Layout (`LC(FSLH)`, `LC(RBKT)`) statt der US-Tasten.
- Scrollen per Ebene 1/2 + Wischen: `INPUT_TRANSFORM_Y_INVERT` ergänzt, damit es in dieselbe Richtung läuft wie das Zwei-Finger-Scrollen.

**Maus-Ebene** (`config/toucan.keymap`, Ebene 4): Mittel- und Rechtsklick getauscht. Daumentasten links Rechts / Links / Mitte, rechts Links / Mitte / Rechts.

**Bluetooth für flüssige Bewegung:**

| Datei | Einstellung | vorher | jetzt | Warum |
|---|---|---|---|---|
| `config/toucan_left.conf` | `BT_PERIPHERAL_PREF_MIN_INT` / `MAX_INT` | `6` / `12` | `6` / `6` | Verlangt vom PC das kürzeste Intervall (7,5 ms). Vorher nahm Windows oft 15 ms. |
| `config/toucan_left.conf` | `BT_CTLR_RX_BUFFERS` | `1` | `6` | Jede Trackpad-Bewegung sind zwei Pakete (X und Y). Mit einem Empfangspuffer kam nur eins pro Funkintervall durch. |
| `config/toucan_left.conf` | `BT_BUF_ACL_RX_COUNT` | `3` | `6` | wie oben |
| `config/toucan_left.conf` | `BT_BUF_ACL_TX_COUNT` | `3` | `10` | Puffer für Maus-Reports zum PC |
| `config/toucan_right.conf` | `BT_BUF_ACL_TX_COUNT`, `BT_L2CAP_TX_BUF_COUNT`, `BT_CONN_TX_MAX` | `3` | `10` | Ein ausgefallenes Funkintervall staut sonst alles, der Zeiger hakt kurz. |

Was beim Testen herauskam:

- Mit der linken Hälfte am USB-Kabel lief das Trackpad deutlich flüssiger. Der Engpass war die Bluetooth-Verbindung zum PC, daher die Intervall- und Puffer-Änderungen.
- Die Grenze per Bluetooth liegt bei etwa 133 Updates pro Sekunde (7,5 ms). Noch flüssiger ginge es nur mit einem USB-Dongle. Mit Dongle zeigt das Display auf der linken Hälfte allerdings keine Ebene und Verbindung mehr.
- Noch nicht ausprobiert: 2M-Funkmodus nur zwischen den Hälften. `CONFIG_BT_CTLR_PHY_2M=n` bleibt wegen des Windows-Fixes gesetzt.
- In Windows macht „Zeigerbeschleunigung verbessern“ kleine Ruckler bei schnellen Bewegungen stärker sichtbar.

Firmware bauen: GitHub → Actions → **Build Toucan2 firmware** → **Run workflow** (für die Piantor entsprechend **Build Piantor Pro BT firmware**). Gebaut wird nur manuell, nicht bei jedem Push. Dateien im Artefakt `toucan2-firmware`:

- `toucan_left rgbled_adapter nice_view_gem-seeeduino_xiao_ble-zmk.uf2` → linke Hälfte
- `toucan_right rgbled_adapter-seeeduino_xiao_ble-zmk.uf2` → rechte Hälfte
- `settings_reset-seeeduino_xiao_ble-zmk.uf2` → setzt gespeicherte Einstellungen zurück

Änderungen aus ZMK Studio überschreiben die Keymap aus der Firmware. Nach dem Flashen einer neuen Keymap deshalb in ZMK Studio „Restore Stock Settings“ ausführen.
