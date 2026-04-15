# HUONYX AI Smartwatch

**ESP32-2424S012 Firmware | Huonyx Agent Chat + Flipper Zero BLE Bridge**

[![Platform](https://img.shields.io/badge/Platform-ESP32--C3-blue)](https://www.espressif.com/)
[![Framework](https://img.shields.io/badge/Framework-Arduino-teal)](https://www.arduino.cc/)
[![UI](https://img.shields.io/badge/UI-LVGL%209.2-purple)](https://lvgl.io/)
[![License: MIT](https://img.shields.io/badge/License-MIT-green)](LICENSE)

A production-quality firmware for the ESP32-2424S012 circular smartwatch board that transforms it into an AI-era wearable controller for the [Huonyx/House of Claws](https://github.com/hunix/HoC) autonomous AI platform, with integrated Flipper Zero remote control via BLE.

---

## Overview

This firmware turns a $15 ESP32-C3 round display board into a powerful AI agent interface and hardware bridge. Wear it on your wrist, talk to your Huonyx agent through the chat interface or WhatsApp, and remotely control your Flipper Zero from anywhere in the world.

### The Scenario

You are driving your car wearing the watch. The watch is connected to your car's WiFi hotspot. Your Huonyx agent is running on your workstation at the office, connected to the internet. You message the agent through WhatsApp: *"Unlock my garage door"*. The agent processes the request, sends a Flipper Zero command through the Supabase Realtime bridge to your watch, which forwards it via BLE to the Flipper Zero clipped to your belt. Done.

### Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    COMMAND FLOW                          │
│                                                         │
│  WhatsApp ──► Huonyx Agent ──► Supabase Realtime ──┐   │
│                    │                                │   │
│  Watch Chat ──► HoC Gateway ──► Agent ──────────┐  │   │
│                                                  │  │   │
│                                                  ▼  ▼   │
│                                          ┌────────────┐ │
│                                          │  ESP32-C3   │ │
│                                          │  Smartwatch │ │
│                                          └──────┬─────┘ │
│                                                 │ BLE   │
│                                          ┌──────▼─────┐ │
│                                          │ Flipper Zero│ │
│                                          │  (CLI/IR/   │ │
│                                          │  SubGHz/NFC)│ │
│                                          └────────────┘ │
└─────────────────────────────────────────────────────────┘
```

### Two Command Paths

| Path | Flow | Best For |
|------|------|----------|
| **HoC Gateway** | Watch ↔ Gateway WebSocket ↔ Agent | Direct chat, low latency when on same network |
| **Supabase Bridge** | Agent → Supabase Broadcast → Watch → Flipper | Remote control from anywhere, WhatsApp integration |

---

## Hardware

### Board: ESP32-2424S012

| Component | Specification |
|-----------|--------------|
| **MCU** | ESP32-C3-MINI-1U (RISC-V, 160MHz, 400KB SRAM) |
| **Flash** | 4MB |
| **Display** | 1.28" GC9A01 240x240 IPS Round LCD |
| **Touch** | CST816D Capacitive Touch (I2C) |
| **Battery IC** | IP5306 (I2C charge management) |
| **Connectivity** | WiFi 802.11 b/g/n + BLE 5.0 |
| **USB** | USB-C (programming + charging) |
| **Buttons** | RST + GPIO0 (boot) |
| **Battery** | JST connector for LiPo |

### Pin Map

| Function | GPIO |
|----------|------|
| SPI SCK (Display) | 6 |
| SPI MOSI (Display) | 7 |
| Display DC | 2 |
| Display CS | 10 |
| Backlight | 3 |
| I2C SDA (Touch/IP5306) | 4 |
| I2C SCL (Touch/IP5306) | 5 |
| Touch INT | 0 |
| Touch RST | 1 |
| Boot Button | 9 |

---

## Features

### Watch Face
- Large digital clock with NTP sync and configurable timezone
- Date and day-of-week display
- Color-coded battery arc (green/orange/red + charging indicator)
- WiFi signal strength icon with color-coded RSSI
- Three status LEDs: Gateway (green), Flipper (orange), Bridge (purple)
- "HUONYX" branding

### AI Chat Interface (Swipe Left)
- Message bubbles with user/agent distinction (blue/dark gray)
- 6 quick-reply buttons: Status?, Yes, No, Summarize, Continue, Help
- Typing spinner during agent response
- Auto-scroll, 8-message buffer with eviction
- Streaming delta support for real-time responses

### Flipper Zero Control (Swipe Right)
- BLE connection status with device name and RSSI
- Scan / Disconnect buttons
- Command log with color-coded entries (orange = commands, green = results)
- Real-time command forwarding from agent (via Gateway or Supabase)
- 6-entry scrollable log

### Quick Settings (Swipe Down)
- Brightness slider with live preview
- WiFi and Settings shortcuts

### Settings Menu
- WiFi Setup
- Gateway Configuration
- Supabase Bridge Configuration
- Flipper BLE Configuration (name filter, auto-connect)
- Session Management
- Firmware version info

### Web Configuration Portal
- Beautiful dark-themed responsive web UI
- Accessible at `http://<watch-ip>/setup`
- Configure WiFi, Gateway, Supabase, and Flipper settings
- Status API at `/status` (JSON)

---

## Configuration

Only **two things** are required to get started:

| Setting | Description |
|---------|-------------|
| **Gateway Host** | Your HoC gateway IP address or hostname |
| **Gateway Token** | Your `OPENCLAW_GATEWAY_TOKEN` |

### First Boot Setup

1. Flash the firmware (see below)
2. The watch creates a WiFi AP: **HuonyxWatch** (password: `huonyx2024`)
3. Connect from your phone and open `http://192.168.4.1/setup`
4. Enter your WiFi credentials and Gateway host/token
5. Save — the watch restarts and connects

### Optional: Supabase Bridge

For remote Flipper Zero control via your agent (e.g., from WhatsApp):

1. Create a [Supabase](https://supabase.com) project (free tier works)
2. Copy the Project URL and anon API key
3. Enter them in the web portal under "Supabase Realtime Bridge"
4. The watch joins the `flipper-bridge` broadcast channel automatically

### Optional: Flipper Zero BLE

1. Enable Bluetooth on your Flipper Zero
2. In the web portal, optionally set a specific Flipper device name
3. Enable "Auto-connect on boot" if desired
4. On the Flipper screen (swipe right from watch face), tap "Scan" to find and connect

---

## Building & Flashing

### Prerequisites

- [PlatformIO](https://platformio.org/) (CLI or VS Code extension)
- USB-C cable
- ESP32-2424S012 board

### Build & Upload

```bash
git clone https://github.com/hunix/huonyx-watch.git
cd huonyx-watch

# Build
pio run

# Upload (hold BOOT button if needed)
pio run --target upload

# Monitor serial output
pio device monitor
```

### Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| TFT_eSPI | ^2.5.43 | GC9A01 display driver |
| LVGL | ^9.2.0 | UI framework |
| ArduinoJson | ^7.2.1 | JSON protocol handling |
| WebSockets | ^2.6.1 | Gateway & Supabase connections |
| NimBLE-Arduino | ^2.1.1 | BLE central (Flipper connection) |

---

## Navigation Map

```
                    ┌──────────────┐
                    │ Quick Settings│
                    │  (Brightness) │
                    └──────┬───────┘
                           │ Swipe Up
                           ▼
┌──────────┐  Swipe  ┌──────────┐  Swipe  ┌──────────┐
│  Flipper │◄────────│  Watch   │────────►│   Chat   │
│  Control │  Right  │   Face   │  Left   │ Interface│
└──────────┘         └──────────┘         └──────────┘
                           │
                      Settings ──► WiFi
                           │  ──► Gateway
                           │  ──► Supabase
                           │  ──► Flipper BLE
                           │  ──► Sessions
```

---

## Agent-Side Tools

### Python Bridge Client

The `tools/flipper_bridge.py` script lets your Huonyx agent send Flipper commands remotely:

```bash
# Install dependency
pip install websockets

# Set credentials
export SUPABASE_URL="https://your-project.supabase.co"
export SUPABASE_KEY="your-anon-key"

# Send a single Flipper command
python tools/flipper_bridge.py --command "ir tx NEC 0x04 0x08"

# Interactive mode
python tools/flipper_bridge.py --interactive

# Listen for events
python tools/flipper_bridge.py --listen
```

### Supabase SQL Setup (Optional)

For command logging and device registry persistence:

```bash
# Run in Supabase SQL Editor
cat tools/supabase_setup.sql
```

---

## Protocol Details

### HoC Gateway Protocol

The watch connects via WebSocket and authenticates using the gateway token:

```json
{
  "type": "req",
  "id": "esp32-...",
  "method": "connect",
  "params": {
    "minProtocol": 1,
    "maxProtocol": 1,
    "client": {
      "id": "huonyx-watch",
      "displayName": "Huonyx Watch",
      "platform": "esp32-c3",
      "deviceFamily": "smartwatch",
      "mode": "chat"
    },
    "auth": { "token": "..." },
    "caps": ["chat"],
    "role": "operator"
  }
}
```

Chat messages use `chat.send` and responses arrive as `delta`/`final` events. The agent can also send `flipper` events directly through the gateway to trigger Flipper commands.

### Supabase Realtime Protocol

The watch joins a Phoenix broadcast channel `flipper-bridge` and exchanges these events:

| Event | Direction | Payload |
|-------|-----------|---------|
| `flipper_command` | Agent → Watch | `{ command, cmd_id, source }` |
| `flipper_result` | Watch → Agent | `{ cmd_id, result, success }` |
| `flipper_status` | Watch → Agent | `{ state, device, rssi }` |
| `watch_status` | Watch → Agent | `{ battery, wifi, gateway, flipper, free_heap }` |
| `agent_message` | Agent → Watch | `{ message, source }` |

### Flipper Zero BLE Serial

The watch connects as a BLE central to the Flipper's Serial Profile:

| UUID | Direction | Purpose |
|------|-----------|---------|
| `00000001-cc7a-482a-984a-7f2ed5b3e58f` | Service | Flipper BLE Serial |
| `...003...` | Watch → Flipper | TX (write commands) |
| `...002...` | Flipper → Watch | RX (receive responses via notify) |

Commands are sent as CLI text with `\r\n` terminator. Responses are accumulated until a prompt character is detected.

---

## Color Palette

The UI uses a carefully crafted dark theme designed for OLED/IPS displays and the AI era aesthetic.

| Color | Hex | Usage |
|-------|-----|-------|
| Deep Black | `#0D0D0F` | Background |
| Dark Navy | `#1A1A2E` | Surface elements |
| Card Blue | `#16213E` | Card backgrounds |
| Cyan | `#00D4FF` | Primary accent, branding |
| Purple | `#7B2FFF` | Secondary accent |
| Green | `#00FF88` | Success, battery healthy |
| Orange | `#FF6B35` | Warning, Flipper accent |
| Red | `#FF3366` | Error states |
| Light Gray | `#E8E8F0` | Primary text |
| Dim Gray | `#6B6B8D` | Secondary text |

---

## Memory Optimization

The ESP32-C3 has only 400KB SRAM. Key optimizations:

| Technique | Savings |
|-----------|---------|
| Single 240x24 LVGL draw buffer | ~11KB vs 57KB dual full-frame |
| NimBLE (vs Bluedroid) | ~60KB RAM savings |
| 8-message chat limit with eviction | Bounded memory |
| 6-entry Flipper log limit | Bounded memory |
| Disabled unused LVGL widgets | ~20KB flash savings |
| ArduinoJson dynamic allocation | On-demand only |
| Fixed-size command queue (4 slots) | Predictable memory |

---

## Project Structure

```
huonyx-watch/
├── platformio.ini          # Build configuration
├── partitions.csv          # Custom partition table (4MB)
├── include/
│   ├── hw_config.h         # Hardware pins & constants
│   ├── touch_driver.h      # CST816D touch driver
│   ├── config_manager.h    # NVS configuration
│   ├── gateway_client.h    # HoC Gateway WebSocket
│   ├── flipper_ble.h       # Flipper Zero BLE client
│   ├── supabase_bridge.h   # Supabase Realtime bridge
│   ├── ui_manager.h        # LVGL UI screens
│   └── web_portal.h        # Web config portal
├── src/
│   ├── main.cpp            # Entry point & main loop
│   ├── lv_conf.h           # LVGL configuration
│   ├── touch_driver.cpp    # CST816D implementation
│   ├── config_manager.cpp  # NVS persistence
│   ├── gateway_client.cpp  # Gateway protocol
│   ├── flipper_ble.cpp     # NimBLE Flipper client
│   ├── supabase_bridge.cpp # Supabase protocol
│   ├── ui_manager.cpp      # All UI screens
│   └── web_portal.cpp      # HTTP server & HTML
├── tools/
│   ├── flipper_bridge.py   # Agent-side bridge client
│   └── supabase_setup.sql  # Optional DB schema
├── LICENSE
└── README.md
```

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Display blank | Check TFT_BL pin 3, verify SPI connections |
| Touch not working | Check I2C address 0x15, verify SDA/SCL |
| WiFi won't connect | Only 2.4GHz supported. Reset config: hold BOOT 10s on startup |
| Gateway auth fails | Verify token matches `OPENCLAW_GATEWAY_TOKEN` |
| Flipper not found | Ensure Flipper BT is enabled, check range (<10m) |
| Supabase timeout | Verify URL format, check anon key |
| Out of memory | Monitor via serial, reduce chat/log limits |
| Upload fails | Hold BOOT button while pressing RST, then upload |

---

## License

MIT License. See [LICENSE](LICENSE) for details.

---

**Built for the AI era.** Wear your agent on your wrist.
