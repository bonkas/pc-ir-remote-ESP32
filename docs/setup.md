# Software Setup Guide

See `assembly.md` for wiring before flashing.

---

## Option 1 — ESPHome (Home Assistant Integration)

### Prerequisites

- Home Assistant with the ESPHome add-on installed
- OR ESPHome CLI installed locally (`pip install esphome`)

### 1. Create your secrets file

Copy the template and fill in your values:

```
firmware/esphome/secrets.yaml.example  →  copy to  →  firmware/esphome/secrets.yaml
```

`secrets.yaml` contents:

```yaml
wifi_ssid: "YourNetworkName"
wifi_password: "YourWiFiPassword"
api_key: "your-32-byte-base64-key"   # generate: openssl rand -base64 32
ota_password: "your-ota-password"
```

`secrets.yaml` is excluded from git (see `.gitignore`) — it will never be committed.

### 2. Flash via USB (first time)

**ESPHome dashboard (Home Assistant):**
1. Open the ESPHome add-on in HA.
2. Click **+ New Device** and paste in `firmware/esphome/pc-ir-remote.yaml`.
3. Connect the ESP32-C3 via USB-C and click **Install → Plug into this computer**.

**ESPHome CLI:**
```bash
esphome run firmware/esphome/pc-ir-remote.yaml
```

### 3. Learn your IR codes

`dump: all` is enabled by default so all received codes are printed to the log.

1. Open the device logs (ESPHome dashboard → your device → **Logs**, or via HA).
2. Point your remote at the TSOP38238 and press each button you want to use.
3. The log shows the protocol and code for each press, for example:
   ```
   [D][remote.nec] Received NEC: address=0x04, command=0x08
   ```
4. Note the **protocol**, **address**, and **command** for each button.

### 4. Add IR buttons to the YAML

Open `firmware/esphome/pc-ir-remote.yaml` and find the commented IR button blocks
in the `binary_sensor:` section. Uncomment and fill in your address and command:

```yaml
binary_sensor:
  - platform: remote_receiver
    name: "IR Power Button"
    internal: true
    nec:
      address: 0x837C
      command: 0x7F80
    on_press:
      - button.press: pc_power_button

  - platform: remote_receiver
    name: "IR Reset Button"
    internal: true
    nec:
      address: 0x837C
      command: 0x????
    on_press:
      - button.press: pc_reset_button
```

> If your remote uses a different protocol the log will show the correct name
> (e.g. `samsung`, `rc5`, `sony`). Replace `nec:` with that protocol name.

### 5. Disable dumping and reflash

Once codes are captured, comment out `dump: all` in the YAML:

```yaml
remote_receiver:
  - pin:
      number: GPIO3
      inverted: true
    # dump: all
```

Then reflash — OTA from this point:
```bash
esphome run firmware/esphome/pc-ir-remote.yaml
```

### 6. Add to Home Assistant

If not auto-discovered: **Settings → Devices & Services → Add Integration → ESPHome**.

By default the HA device exposes only the **PC Power Button**. Optional entities are
hidden with `internal: true` or commented out until the required wiring is in place:

| Entity | Default | How to enable |
|--------|---------|---------------|
| PC Power Button | **Visible** | Always on |
| PC Reset Button | Hidden | Set `internal: false` in the button block |
| PC State (On/Sleeping/Off) | Commented out | Wire the voltage divider to GPIO4 first, then uncomment both the `binary_sensor` and `text_sensor` blocks |

---

## Option 2 — Standalone Arduino Sketch (No Home Assistant)

### Prerequisites

- [VS Code](https://code.visualstudio.com/) with the [PlatformIO extension](https://platformio.org/install/ide?install=vscode)

No secrets file needed — the standalone sketch has no WiFi or cloud connectivity.

### 1. Open the project

In VS Code: **File → Open Folder** → select `firmware/arduino/pc-ir-remote/`

PlatformIO will auto-install the required libraries (`IRremoteESP8266`) on first open.

### 2. Flash via USB

1. Connect the ESP32-C3 via USB-C.
2. Click the **→ Upload** arrow in the PlatformIO toolbar (bottom of VS Code).
3. If the upload fails with "Failed to connect", manually enter bootloader mode:
   - Hold **BOOT** → press and release **RESET** → release **BOOT**
   - Then click Upload again.
4. Click the **🔌 Serial Monitor** icon and set baud rate to **115200**.
5. You should see:
   ```
   PC IR Remote starting...
   WARNING: Power code not set — press Learn Power button.
   Ready.
   ```

### 3. Learn your IR codes

IR codes are learned at runtime using the onboard buttons — no reflash needed.

**Power button code:**
1. Press the **Learn Power** button (GPIO5).
2. Status LED flashes **3 times repeatedly** — board is waiting for IR.
3. Point your remote and press the button you want to use for power.
4. LED flashes **3 quick blinks** — saved.

**Reset button code:**
1. Press the **Learn Reset** button (GPIO6).
2. Status LED flashes **2 times repeatedly** — board is waiting for IR.
3. Press the desired button on your remote.
4. LED flashes **3 quick blinks** — saved.

Codes are stored in flash and survive power cycles. Repeat the above any time
you want to change to a different remote or button.

### LED feedback reference

| Pattern | Meaning |
|---------|---------|
| 3 repeating flashes | Learn power mode active — waiting for IR |
| 2 repeating flashes | Learn reset mode active — waiting for IR |
| 3 quick blinks | Code saved successfully |
| 2 slow blinks | Learn mode timed out (10 s, no IR received) |
| 1 blink | IR command received and relay fired |

### Debug mode

To print every received IR code to Serial (useful if learn buttons are not yet wired):

In `pc-ir-remote.ino`, change:
```cpp
#define DUMP_ALL_CODES  false
```
to:
```cpp
#define DUMP_ALL_CODES  true
```
Then reflash. All received codes will appear in the Serial Monitor.
