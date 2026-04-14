# Huonyx AI Smartwatch

> **An AI-era smartwatch firmware for the ESP32-2424S012 board** — chat with your Huonyx (House of Claws) autonomous AI agent directly from your wrist, with a beautiful dark-themed circular UI.

[![Platform](https://img.shields.io/badge/Platform-ESP32--C3-blue)](https://www.espressif.com/)
[![Framework](https://img.shields.io/badge/Framework-Arduino-teal)](https://www.arduino.cc/)
[![UI](https://img.shields.io/badge/UI-LVGL%208.4-purple)](https://lvgl.io/)
[![License: MIT](https://img.shields.io/badge/License-MIT-green)](LICENSE)

---

## Overview

This firmware transforms the ESP32-2424S012 round touchscreen development board into a fully functional AI smartwatch that connects to the **Huonyx / House of Claws (HoC)** gateway via WebSocket. It provides a seamless chat interface to interact with your autonomous AI agent, alongside standard smartwatch features like time display, battery monitoring, and WiFi status.

### What You Need to Configure

Only **two things** are required from you:

| Setting | Description |
|---------|-------------|
| **Gateway Host** | The IP address or hostname of your HoC gateway (e.g., `192.168.1.100` or `myserver.com`) |
| **Gateway Token** | Your `OPENCLAW_GATEWAY_TOKEN` for authentication |

Everything else is handled automatically.

---

## Hardware

### Board: ESP32-2424S012

| Component | Specification |
|-----------|--------------|
| **MCU** | ESP32-C3-MINI-1U (RISC-V, 160 MHz, single core) |
| **Flash** | 4 MB |
| **Display** | 1.28" IPS Round LCD, 240x240, GC9A01 driver (SPI) |
| **Touch** | CST816D capacitive touch controller (I2C) |
| **Battery** | IP5306 battery management IC |
| **Connectivity** | WiFi 802.11 b/g/n + Bluetooth LE 5.0 |
| **USB** | USB-C (programming + power) |
| **Buttons** | Boot (GPIO 9), Reset |

### Pin Map

| Function | GPIO |
|----------|------|
| SPI SCK (Display) | 6 |
| SPI MOSI (Display) | 7 |
| Display DC | 2 |
| Display CS | 10 |
| Backlight | 3 |
| I2C SDA (Touch) | 4 |
| I2C SCL (Touch) | 5 |
| Touch INT | 0 |
| Touch RST | 1 |
| Boot Button | 9 |

---

## Features

### Watch Face
The home screen displays a modern, minimalist digital clock with status indicators arranged around the circular display. A cyan outer ring frames the watch face, while the time is rendered in a large, clear Montserrat font. The date appears below in a dimmed tone. A battery arc in the bottom-left corner shows charge level with color-coded feedback (green for healthy, orange for low, red for critical). A small LED indicator in the top-right shows gateway connection status, and a WiFi icon in the top-left reflects signal strength.

### AI Chat Interface
Swipe left from the watch face to enter the chat screen. Messages appear as styled bubbles: user messages in blue on the right, agent responses in dark gray on the left. Since typing on a 1.28" screen is impractical, the interface provides six quick-reply buttons ("Status?", "Yes", "No", "Summarize", "Continue", "Help") that send pre-defined messages to your Huonyx agent. A spinning indicator appears while the agent is processing. The chat keeps the last 8 messages in memory to prevent out-of-memory conditions.

### Quick Settings
Swipe down from the watch face to access quick settings. This screen provides a brightness slider, WiFi toggle, and a shortcut to the full settings menu.

### Settings Menu
The settings screen provides access to WiFi Setup, Gateway Configuration, Session Management, and device information. Navigation uses smooth slide animations with gesture-based back navigation (swipe right).

### Web Configuration Portal
Since entering long strings (like tokens and hostnames) on a tiny screen is impractical, the watch runs a lightweight web server. Connect to the watch's IP address from any browser on your phone or computer to configure WiFi credentials, gateway host/token, brightness, and timezone through a beautiful dark-themed web interface.

### Gateway Protocol
The firmware implements the HoC Gateway WebSocket protocol, including the connect handshake with token authentication, chat.send for sending messages, streaming chat event handling for receiving responses, session management, and automatic reconnection with tick-based health monitoring.

---

## Getting Started

### Prerequisites

1. **PlatformIO** installed (via VS Code extension or CLI)
2. **ESP32-2424S012** board connected via USB-C
3. A running **Huonyx / HoC gateway** instance

### Build & Flash

```bash
# Clone the repository
git clone https://github.com/hunix/huonyx-watch.git
cd huonyx-watch

# Build the firmware
pio run

# Flash to the board
pio run --target upload

# Monitor serial output
pio device monitor
```

### First-Time Setup

1. **Power on** the watch via USB-C.
2. If no WiFi is configured, the watch automatically starts in **AP mode** with SSID `HuonyxWatch`.
3. Connect your phone to the `HuonyxWatch` WiFi network.
4. Open a browser and navigate to `http://192.168.4.1/setup`.
5. Enter your **WiFi credentials**, **Gateway Host**, **Port** (default: 18789), and **Gateway Token**.
6. Click **Save & Restart**.
7. The watch will connect to your WiFi and then to the Huonyx gateway.

### Subsequent Configuration

Once connected to your WiFi, the web portal remains accessible at the watch's IP address (check serial output or your router's DHCP table). Navigate to `http://<watch-ip>/setup` to modify settings at any time.

---

## Navigation

| Gesture | From | Action |
|---------|------|--------|
| Swipe Left | Watch Face | Open Chat |
| Swipe Right | Chat | Return to Watch Face |
| Swipe Down | Watch Face | Quick Settings |
| Swipe Up | Quick Settings | Return to Watch Face |
| Swipe Right | Any sub-screen | Go Back |
| Tap header | Any screen with header | Go Back |

---

## Architecture

```
┌─────────────────────────────────────────────┐
│              main.cpp (loop)                │
│  ┌──────────┐ ┌──────────┐ ┌────────────┐  │
│  │  LVGL    │ │ Gateway  │ │ Web Portal │  │
│  │  Timer   │ │  Loop    │ │   Loop     │  │
│  └────┬─────┘ └────┬─────┘ └─────┬──────┘  │
│       │             │              │         │
│  ┌────▼─────┐ ┌────▼─────┐ ┌─────▼──────┐  │
│  │UIManager │ │ Gateway  │ │ WebServer  │  │
│  │  (LVGL)  │ │  Client  │ │  (HTTP)    │  │
│  └────┬─────┘ └────┬─────┘ └─────┬──────┘  │
│       │             │              │         │
│  ┌────▼─────────────▼──────────────▼──────┐  │
│  │          ConfigManager (NVS)           │  │
│  └────────────────────────────────────────┘  │
│                                              │
│  ┌──────────────┐  ┌───────────────────┐     │
│  │  TFT_eSPI    │  │  CST816D Driver   │     │
│  │  (GC9A01)    │  │  (Touch I2C)      │     │
│  └──────┬───────┘  └────────┬──────────┘     │
│         │ SPI               │ I2C            │
└─────────┼───────────────────┼────────────────┘
          ▼                   ▼
    ┌──────────┐       ┌──────────┐
    │ GC9A01   │       │ CST816D  │
    │ Display  │       │  Touch   │
    └──────────┘       └──────────┘
```

---

## Project Structure

```
huonyx-watch/
├── platformio.ini          # PlatformIO configuration
├── partitions.csv          # Custom partition table (4MB)
├── README.md               # This file
├── include/
│   ├── hw_config.h         # Hardware pin definitions & constants
│   ├── touch_driver.h      # CST816D touch driver header
│   ├── config_manager.h    # NVS configuration manager
│   ├── gateway_client.h    # HoC Gateway WebSocket client
│   ├── ui_manager.h        # LVGL UI manager
│   └── web_portal.h        # Web configuration portal
├── src/
│   ├── main.cpp            # Entry point, setup & loop
│   ├── lv_conf.h           # LVGL configuration
│   ├── touch_driver.cpp    # CST816D driver implementation
│   ├── config_manager.cpp  # Config manager implementation
│   ├── gateway_client.cpp  # Gateway client implementation
│   ├── ui_manager.cpp      # UI screens & navigation
│   └── web_portal.cpp      # Web portal implementation
├── lib/                    # Local libraries (empty)
└── data/                   # SPIFFS data (empty)
```

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
| Orange | `#FF6B35` | Warning states |
| Red | `#FF3366` | Error states |
| Light Gray | `#E8E8F0` | Primary text |
| Dim Gray | `#6B6B8D` | Secondary text |

---

## Gateway Protocol

The firmware implements a subset of the HoC Gateway WebSocket protocol suitable for the ESP32-C3's resource constraints.

### Supported Methods

| Method | Direction | Description |
|--------|-----------|-------------|
| `connect` | Client -> Server | Authentication handshake with token |
| `chat.send` | Client -> Server | Send a chat message to the agent |
| `chat.history` | Client -> Server | Request chat history for a session |
| `chat.abort` | Client -> Server | Cancel an in-flight agent run |
| `sessions.list` | Client -> Server | List available chat sessions |

### Supported Events

| Event | Direction | Description |
|-------|-----------|-------------|
| `tick` | Server -> Client | Heartbeat (every ~30s) |
| `chat` | Server -> Client | Chat response deltas and finals |

---

## Memory Optimization

The ESP32-C3 has approximately 400 KB of SRAM. The firmware employs several strategies to stay within budget.

LVGL uses a single draw buffer of 240 x 24 pixels (11,520 bytes) rather than a full-frame buffer. Chat history is limited to 8 messages with automatic eviction of the oldest entry. JSON documents use ArduinoJson's stack-based allocation where possible. Unused LVGL widgets (canvas, chart, colorwheel, etc.) are disabled at compile time to reduce code size. The custom partition table allocates 1.75 MB to the application and 2.2 MB to SPIFFS for future use.

---

## Troubleshooting

**Watch shows "HuonyxWatch" AP but won't connect to my WiFi**: Double-check the SSID and password via the web portal. The ESP32-C3 only supports 2.4 GHz WiFi networks.

**Gateway LED stays red**: Verify that the gateway host and port are correct, the token matches your `OPENCLAW_GATEWAY_TOKEN`, and the gateway is running and reachable from the watch's network.

**Touch not responding**: The CST816D may need a power cycle. Disconnect USB and reconnect. If the issue persists, check the I2C connections.

**Display is too dim or too bright**: Access Quick Settings (swipe down from watch face) and adjust the brightness slider, or use the web portal.

---

## License

MIT License. See [LICENSE](LICENSE) for details.

---

## Credits

Built for the [House of Claws (HoC)](https://github.com/hunix/HoC) autonomous AI platform.
Hardware: ESP32-2424S012 by Shenzhen Jingcai Intelligent Co.
