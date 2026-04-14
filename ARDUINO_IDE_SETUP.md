# Arduino IDE Setup Guide for Huonyx Watch

This guide explains how to manually configure the Arduino IDE to compile and flash the Huonyx AI Smartwatch firmware to the ESP32-2424S012 board. 

If you are on Windows, you can alternatively use the automated `flash.ps1` script included in this repository.

## Prerequisites

1. Download and install the latest [Arduino IDE 2.x](https://www.arduino.cc/en/software).
2. Ensure you have a USB-C cable capable of data transfer (not just charging).
3. If your computer doesn't recognize the board, you may need to install the CH340 or CP210X USB-to-Serial drivers.

---

## Step 1: Install ESP32 Board Support

1. Open Arduino IDE.
2. Go to **File > Preferences** (Windows/Linux) or **Arduino IDE > Settings** (macOS).
3. In the **Additional boards manager URLs** field, add the following URL:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
   *(If you already have URLs there, separate them with a comma).*
4. Click **OK**.
5. Go to **Tools > Board > Boards Manager...**
6. Search for `esp32` and install the **esp32 by Espressif Systems** package (version 3.x is recommended).

---

## Step 2: Configure the Board Settings

1. Connect your ESP32-2424S012 board to your computer.
2. Go to **Tools > Board > esp32** and select **ESP32C3 Dev Module**.
3. Go to **Tools > Port** and select the COM port corresponding to your board.
4. Configure the following settings under the **Tools** menu:
   - **USB CDC On Boot:** `Enabled`
   - **Flash Size:** `4MB (32Mb)`
   - **Partition Scheme:** `Custom` *(See Step 5 for details)*
   - **Flash Frequency:** `80MHz`
   - **Core Debug Level:** `None`

---

## Step 3: Install Required Libraries

Go to **Sketch > Include Library > Manage Libraries...** and install the following libraries (exact versions recommended if you face issues):

| Library Name | Version | Purpose |
|--------------|---------|---------|
| **TFT_eSPI** by Bodmer | `2.5.43` | Display driver |
| **lvgl** by kisvegabor | `9.2.x` | UI framework |
| **ArduinoJson** by Benoit Blanchon | `7.2.1` | JSON parsing for config |
| **WebSockets** by Markus Sattler | `2.6.1` | HoC Gateway connection |
| **NimBLE-Arduino** by h2zero | `2.1.x` | Memory-efficient BLE for Flipper |

---

## Step 4: Patch Library Configurations

Because this firmware uses a custom display and UI framework, you **must** configure the libraries manually before compiling.

### 1. Configure TFT_eSPI
1. Locate your Arduino libraries folder (usually `Documents/Arduino/libraries/TFT_eSPI`).
2. Copy the `User_Setup.h` file from the `arduino/HuonyxWatch/` folder in this repository.
3. Paste it into the `TFT_eSPI` library folder, **replacing** the existing `User_Setup.h` file.

### 2. Configure LVGL
1. Locate your Arduino libraries folder.
2. Copy the `lv_conf.h` file from the `arduino/HuonyxWatch/` folder.
3. Paste it **directly into the `libraries` folder** (next to the `lvgl` folder, not inside it).
   *(Path should look like: `Documents/Arduino/libraries/lv_conf.h`)*

---

## Step 5: Custom Partition Table

The ESP32-C3 only has 4MB of flash. The default partition table does not leave enough room for the firmware and the OTA (Over-The-Air) update space.

1. Locate the ESP32 hardware package directory. 
   - Windows: `%LOCALAPPDATA%\Arduino15\packages\esp32\hardware\esp32\3.x.x\tools\partitions`
   - macOS: `~/Library/Arduino15/packages/esp32/hardware/esp32/3.x.x/tools/partitions`
2. Copy the `partitions.csv` file from this repository into that folder and rename it to `custom.csv`.
3. In Arduino IDE, ensure **Tools > Partition Scheme** is set to `Custom`.

---

## Step 6: Compile and Upload

1. In Arduino IDE, go to **File > Open...** and select the `arduino/HuonyxWatch/HuonyxWatch.ino` file from this repository.
2. Click the **Verify** (checkmark) button to compile the code. This may take a few minutes the first time.
3. Click the **Upload** (right arrow) button to flash the firmware to the board.

> **Note on Uploading:** If the upload fails to connect, press and hold the **BOOT** button on the back of the board, click Upload in the IDE, and release the BOOT button once the "Connecting..." text appears.

---

## Step 7: Initial Setup

Once flashed successfully:
1. The watch will reboot and display the HUONYX watch face.
2. It will create a temporary WiFi hotspot named **HuonyxWatch** (Password: `huonyx2024`).
3. Connect to this network from your phone or computer.
4. Open a browser and go to `http://192.168.4.1/setup`.
5. Enter your local WiFi credentials, your HoC Gateway IP, and your Gateway Token.
6. Click **Save & Restart**. The watch will connect to your network and the Huonyx Gateway.
