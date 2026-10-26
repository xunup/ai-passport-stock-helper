<p align="right">
  <strong>English</strong> · <a href="README.zh_CN.md">简体中文</a>
</p>

<h1 align="center">AI Passport Stock Helper</h1>

<p align="center">
  <strong>A wearable stock watchlist for FoloToy AI Passport.</strong><br>
  Three-button browsing, on-device settings, and a browser configuration page.
</p>

<p align="center">
  ESP32-C3 · 8 MB flash · 240 × 320 display · 2.4 GHz Wi-Fi
</p>

---

**AI Passport Stock Helper** is a self-contained firmware for the FoloToy AI Passport
wearable. It polls Sina Finance for up to eight A-share, index, or ETF codes and
renders them on a dark four-row quote board. An embedded web page handles Wi-Fi
provisioning, the watchlist, and device settings; screen brightness, auto screen-off,
and the provisioning hotspot are also available from the on-device menu. The device UI
and web page are in Simplified Chinese.

> This is a downstream application built on the FoloToy AI Passport baseline.
> Platform documentation remains upstream content under [`docs/`](/docs/README.md).

## Features

- **Live watchlist:** up to 8 A-share, index, or ETF codes, refreshed every 3–300 seconds (default 5). Press OK on the quote list to refresh immediately.
- **Readable at a glance:** four rows per screen with name, code, current price, change, and change percentage. Gains are red and losses are green, following the Chinese market convention. ETF prices use three decimals; other instruments use two.
- **Status chrome:** a Wi-Fi indicator and an optional CW2017 battery gauge in the header, plus a status line for network state, quote errors, and the active refresh interval.
- **On-device settings:** provisioning hotspot switch, five brightness levels (20–100% backlight), auto screen-off switch, and a 5–60 second timeout in 5-second steps.
- **Screen-off power saving:** after the idle timeout the backlight turns off and quotes drop to a 60-second poll. Any button wakes the screen; that press only wakes and does not trigger its normal action. Waking restores the configured interval and refreshes immediately.
- **Web configuration:** live device and quote status, Wi-Fi scan/save/forget, the watchlist editor, and device settings that stay in sync with the on-device menu.
- **Wi-Fi roaming:** when the current RSSI falls below -70 dBm, the device scans every 30 seconds for the same SSID and switches to an access point at least 8 dB stronger and no weaker than -72 dBm.
- **Persistent state:** Wi-Fi credentials, watchlist, refresh interval, brightness, auto screen-off, and hotspot state are stored in NVS.

## Controls

| Context | UP / DOWN | OK | Long OK |
| --- | --- | --- | --- |
| Quote list | Move selection | Refresh quotes now | Open settings |
| Settings menu | Select a row; adjust the value while editing | Toggle a switch, or enter/finish editing | Close settings |
| Setup screen | — | — | Open settings |
| Screen off | Wake the screen | Wake the screen | Wake the screen |

The settings menu has four rows: hotspot switch, screen brightness, auto screen-off
switch, and screen-off time. The first button press after the screen turns off is
consumed for the wake-up, including its release event.

## Getting started

1. Power on the device. On first boot, or after Wi-Fi has been cleared, the screen shows a setup card with the hotspot name and `http://192.168.4.1`.
2. From a phone or computer, join the `FoloStock-XXXX` hotspot. It is an open 2.4 GHz network on channel 6, accepts up to four clients, and has no password.
3. Open `http://192.168.4.1`. Select a nearby network or type its SSID, enter the password, and save. Then add up to eight stock codes and choose a refresh interval.
4. Once Wi-Fi connects and the watchlist is not empty, the quote list appears and refreshes automatically.

The hotspot is on by default. After provisioning, turn it off from the web page or the
settings menu; the configuration page then remains reachable through the device's LAN
address on the same network. Use the settings menu to turn the hotspot back on.

## Settings

| Setting | Range and default | Available from |
| --- | --- | --- |
| Screen brightness | 1–5, default 3 | Device menu, web page |
| Auto screen off | On/off, default on | Device menu, web page |
| Screen-off timeout | 5–60 s in 5 s steps, default 30 s | Device menu, web page |
| Provisioning hotspot | On/off, default on | Device menu, web page |
| Refresh interval | 3–300 s, default 5 s | Web page |
| Watchlist | 1–8 codes | Web page |

On the web page, brightness, auto screen-off, and hotspot changes are written to the
device only when **Save device settings** is pressed. The page polls `/api/status`
every 2 seconds and refreshes the quote table and status cards from that response.

## Stock codes

Codes can be entered as comma-, semicolon-, or whitespace-separated lists. Accepted
forms are a bare six-digit code (`600519`) or a six-digit code with an exchange prefix
(`sh600519`, `sh000001`). Bare codes infer the exchange: `6`/`5` to Shanghai,
`0`/`1`/`3` to Shenzhen, and `4`/`8` to Beijing. Duplicates are removed and the list is
limited to eight entries.

Indices need an explicit exchange prefix. For example, `000001` is a Shenzhen stock;
the Shanghai Composite is `sh000001`.

## Local HTTP API

The embedded page is served from flash and uses these endpoints:

| Method and path | Purpose |
| --- | --- |
| `GET /api/status` | Device, Wi-Fi, quote, and configuration snapshot |
| `GET /api/config` | Current watchlist and device settings |
| `POST /api/config` | Save `codes` and `interval` |
| `POST /api/settings` | Save brightness, auto screen-off, and hotspot state |
| `GET /api/scan` | Scan nearby 2.4 GHz networks |
| `POST /api/wifi` | Save Wi-Fi credentials and reconnect |
| `POST /api/wifi/forget` | Clear stored Wi-Fi credentials |
| `POST /api/portal/close` | Dismiss the setup screen after provisioning |

## Quotes and limitations

- Quote data comes from the Sina Finance endpoint at `hq.sinajs.cn`. An internet connection and 2.4 GHz Wi-Fi are required; the ESP32-C3 does not support 5 GHz.
- Quotes may be delayed and are provided for reference only. This firmware does not place orders and is not investment advice.
- The UI fonts cover GB2312 level-one Hanzi and 23 UI punctuation marks. Stock names containing other characters display those characters as `?`.
- Long names and codes are shortened with an ellipsis to fit their rows.
- The two embedded fonts occupy roughly 768 KB of flash; the board has no PSRAM.
- The provisioning hotspot is open. Turn it off when it is not needed, and do not leave the configuration page open on a shared device: the browser caches form values in local storage, though the device never returns the stored Wi-Fi password.

## Build, test, and flash

The firmware targets ESP-IDF 5.5.3 and the `esp32c3` target.

```bash
source <path-to-esp-idf-v5.5.3>/export.sh
idf.py set-target esp32c3
./tools/validate.sh --firmware
```

`validate.sh --firmware` builds in a clean temporary directory, verifies the firmware
layout, and writes the merged image to `build/FoloToy-AI-Passport-full.bin` (also
archived under `build/firmware/<sha256>/`). Flash that merged image at offset `0x0`.

A merged flash rewrites the whole image and can reset NVS, clearing Wi-Fi credentials,
the watchlist, and display settings. To keep existing NVS data, use a segmented
`idf.py flash` with a compatible partition layout instead. Always confirm the exact
image and offset before flashing.

Run host-side checks with:

```bash
./tools/validate.sh --static
```

## Project layout

```text
main/                    Application logic, LVGL UI, Wi-Fi, quotes, and HTTP server
main/web/index.html      Embedded configuration page
components/bsp/          Board support package and hardware facts
assets/fonts/            Generated Chinese fonts
tests/                   Host-runnable logic and contract tests
tools/                   Validation, font generation, and firmware archiving
docs/                    Upstream platform documentation
```

## Documentation

| Resource | Contents |
| --- | --- |
| [Platform overview](/docs/README.md) | FoloToy AI Passport product and documentation index |
| [Hardware guide](/docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md) | Pin map, panel, audio, buttons, and acceptance checklists |
| [Build and test](/docs/development/engineering/build-and-test.md) | Environment, validation, and firmware gates |
| [Firmware layout](/docs/development/engineering/firmware-layout.md) | Partitions, flashing, and stored data |
| [Changelog](/docs/CHANGELOG.md) | Release history |

## License

The repository is released under the [MIT License](/LICENSE). Fonts are generated from
Source Han Sans SC under the SIL Open Font License; see
[`assets/fonts/LICENSE-OFL.txt`](/assets/fonts/LICENSE-OFL.txt). Market data is provided
by Sina Finance and is subject to its terms.
