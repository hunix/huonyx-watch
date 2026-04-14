<#
.SYNOPSIS
    Huonyx AI Smartwatch - One-Click Flash Script for Windows
    Automatically installs arduino-cli, configures ESP32-C3, installs libraries,
    patches TFT_eSPI, compiles, and uploads the firmware.

.DESCRIPTION
    This script handles the entire build and flash process:
    1. Downloads and installs arduino-cli (if not found)
    2. Installs ESP32 board support package
    3. Installs all required Arduino libraries
    4. Patches TFT_eSPI with the correct User_Setup.h for ESP32-2424S012
    5. Copies lv_conf.h to the correct location for LVGL
    6. Compiles the firmware with optimized settings
    7. Uploads to the connected ESP32-C3 board

.PARAMETER ComPort
    The COM port of the ESP32 board (e.g., COM3, COM5). Auto-detected if not specified.

.PARAMETER SkipInstall
    Skip the installation of arduino-cli and libraries (use if already set up).

.PARAMETER CompileOnly
    Only compile the firmware without uploading.

.PARAMETER Verbose
    Show detailed compilation output.

.EXAMPLE
    .\flash.ps1
    .\flash.ps1 -ComPort COM5
    .\flash.ps1 -CompileOnly
    .\flash.ps1 -SkipInstall -ComPort COM3
#>

param(
    [string]$ComPort = "",
    [switch]$SkipInstall,
    [switch]$CompileOnly,
    [switch]$Verbose
)

# ═══════════════════════════════════════════════════════════
#  CONFIGURATION
# ═══════════════════════════════════════════════════════════

$ErrorActionPreference = "Stop"

$BOARD_FQBN = "esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashFreq=80,FlashSize=4M,PartitionScheme=custom,UploadSpeed=921600"
$ESP32_CORE_URL = "https://espressif.github.io/arduino-esp32/package_esp32_index.json"
$ESP32_CORE_VERSION = "3.0.7"

$LIBRARIES = @(
    "TFT_eSPI@2.5.43",
    "lvgl@8.4.0",
    "ArduinoJson@7.2.1",
    "WebSockets@2.6.1",
    "NimBLE-Arduino@1.4.3"
)

# Build flags for NimBLE memory optimization
$BUILD_EXTRA_FLAGS = @(
    "-DUSER_SETUP_LOADED=1",
    "-DLV_CONF_INCLUDE_SIMPLE=1",
    "-DLV_TICK_CUSTOM=1",
    "-DCONFIG_BT_NIMBLE_ROLE_CENTRAL=1",
    "-DCONFIG_BT_NIMBLE_ROLE_PERIPHERAL=0",
    "-DCONFIG_BT_NIMBLE_ROLE_BROADCASTER=0",
    "-DCONFIG_BT_NIMBLE_ROLE_OBSERVER=1",
    "-DCONFIG_BT_NIMBLE_MAX_CONNECTIONS=1",
    "-DCONFIG_BT_NIMBLE_MAX_BONDS=1",
    "-DCONFIG_BT_NIMBLE_ATT_PREFERRED_MTU=128",
    "-DCORE_DEBUG_LEVEL=0",
    "-Os"
)

# ═══════════════════════════════════════════════════════════
#  HELPER FUNCTIONS
# ═══════════════════════════════════════════════════════════

function Write-Banner {
    Write-Host ""
    Write-Host "  ╔══════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "  ║     HUONYX AI SMARTWATCH FLASHER v2.0    ║" -ForegroundColor Cyan
    Write-Host "  ║     ESP32-2424S012 + Flipper Bridge      ║" -ForegroundColor Cyan
    Write-Host "  ╚══════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""
}

function Write-Step {
    param([string]$Message)
    Write-Host "  ► " -ForegroundColor Green -NoNewline
    Write-Host $Message
}

function Write-SubStep {
    param([string]$Message)
    Write-Host "    • " -ForegroundColor DarkGray -NoNewline
    Write-Host $Message -ForegroundColor Gray
}

function Write-Success {
    param([string]$Message)
    Write-Host "  ✓ " -ForegroundColor Green -NoNewline
    Write-Host $Message -ForegroundColor Green
}

function Write-Warn {
    param([string]$Message)
    Write-Host "  ⚠ " -ForegroundColor Yellow -NoNewline
    Write-Host $Message -ForegroundColor Yellow
}

function Write-Fail {
    param([string]$Message)
    Write-Host "  ✗ " -ForegroundColor Red -NoNewline
    Write-Host $Message -ForegroundColor Red
}

# ═══════════════════════════════════════════════════════════
#  LOCATE PATHS
# ═══════════════════════════════════════════════════════════

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$SketchDir = Join-Path $ScriptDir "arduino" "HuonyxWatch"
$ToolsDir = Join-Path $ScriptDir ".tools"
$ArduinoCliPath = ""

# Check if sketch directory exists
if (-not (Test-Path $SketchDir)) {
    Write-Fail "Sketch directory not found: $SketchDir"
    Write-Host "  Make sure you're running this script from the huonyx-watch repository root." -ForegroundColor Gray
    exit 1
}

# ═══════════════════════════════════════════════════════════
#  STEP 1: INSTALL / LOCATE ARDUINO-CLI
# ═══════════════════════════════════════════════════════════

Write-Banner

if (-not $SkipInstall) {
    Write-Step "Checking for arduino-cli..."

    # Check if arduino-cli is in PATH
    $existing = Get-Command "arduino-cli" -ErrorAction SilentlyContinue
    if ($existing) {
        $ArduinoCliPath = $existing.Source
        Write-Success "Found arduino-cli at: $ArduinoCliPath"
    }
    else {
        # Check in local .tools directory
        $localCli = Join-Path $ToolsDir "arduino-cli.exe"
        if (Test-Path $localCli) {
            $ArduinoCliPath = $localCli
            Write-Success "Found local arduino-cli at: $ArduinoCliPath"
        }
        else {
            Write-Step "Downloading arduino-cli..."
            New-Item -ItemType Directory -Force -Path $ToolsDir | Out-Null

            $cliUrl = "https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Windows_64bit.zip"
            $zipPath = Join-Path $ToolsDir "arduino-cli.zip"

            try {
                [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
                Invoke-WebRequest -Uri $cliUrl -OutFile $zipPath -UseBasicParsing
                Expand-Archive -Path $zipPath -DestinationPath $ToolsDir -Force
                Remove-Item $zipPath -Force
                $ArduinoCliPath = $localCli
                Write-Success "arduino-cli downloaded to: $ArduinoCliPath"
            }
            catch {
                Write-Fail "Failed to download arduino-cli: $_"
                Write-Host ""
                Write-Host "  Manual install:" -ForegroundColor Yellow
                Write-Host "    1. Download from https://arduino.github.io/arduino-cli/installation/" -ForegroundColor Gray
                Write-Host "    2. Place arduino-cli.exe in: $ToolsDir" -ForegroundColor Gray
                Write-Host "    3. Or add it to your PATH" -ForegroundColor Gray
                exit 1
            }
        }
    }

    # ═══════════════════════════════════════════════════════════
    #  STEP 2: INSTALL ESP32 BOARD SUPPORT
    # ═══════════════════════════════════════════════════════════

    Write-Step "Configuring ESP32 board support..."

    # Add ESP32 board URL
    & $ArduinoCliPath config init --overwrite 2>$null
    & $ArduinoCliPath config add board_manager.additional_urls $ESP32_CORE_URL 2>$null
    & $ArduinoCliPath core update-index 2>$null

    # Check if ESP32 core is installed
    $installedCores = & $ArduinoCliPath core list --format json 2>$null | ConvertFrom-Json
    $esp32Installed = $false
    if ($installedCores) {
        foreach ($core in $installedCores) {
            if ($core.id -eq "esp32:esp32") {
                $esp32Installed = $true
                Write-Success "ESP32 core already installed (v$($core.installed))"
                break
            }
        }
    }

    if (-not $esp32Installed) {
        Write-SubStep "Installing ESP32 Arduino core v${ESP32_CORE_VERSION} (this may take a few minutes)..."
        $installResult = & $ArduinoCliPath core install "esp32:esp32@${ESP32_CORE_VERSION}" 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Warn "Specific version failed, trying latest..."
            & $ArduinoCliPath core install "esp32:esp32" 2>&1
        }
        Write-Success "ESP32 core installed"
    }

    # ═══════════════════════════════════════════════════════════
    #  STEP 3: INSTALL LIBRARIES
    # ═══════════════════════════════════════════════════════════

    Write-Step "Installing Arduino libraries..."

    foreach ($lib in $LIBRARIES) {
        $libName = ($lib -split "@")[0]
        Write-SubStep "Installing $lib..."
        & $ArduinoCliPath lib install $lib 2>$null
        if ($LASTEXITCODE -ne 0) {
            # Try without version constraint
            Write-Warn "Exact version failed, trying latest $libName..."
            & $ArduinoCliPath lib install $libName 2>$null
        }
    }
    Write-Success "All libraries installed"

    # ═══════════════════════════════════════════════════════════
    #  STEP 4: PATCH TFT_eSPI User_Setup.h
    # ═══════════════════════════════════════════════════════════

    Write-Step "Patching TFT_eSPI configuration for ESP32-2424S012..."

    # Find TFT_eSPI library path
    $libListJson = & $ArduinoCliPath lib list --format json 2>$null | ConvertFrom-Json
    $tftEspiPath = ""

    if ($libListJson -and $libListJson.installed_libraries) {
        foreach ($entry in $libListJson.installed_libraries) {
            if ($entry.library.name -eq "TFT_eSPI") {
                $tftEspiPath = $entry.library.install_dir
                break
            }
        }
    }

    # Fallback: search common Arduino library paths
    if (-not $tftEspiPath -or -not (Test-Path $tftEspiPath)) {
        $searchPaths = @(
            "$env:USERPROFILE\Documents\Arduino\libraries\TFT_eSPI",
            "$env:LOCALAPPDATA\Arduino15\libraries\TFT_eSPI",
            "$env:USERPROFILE\Arduino\libraries\TFT_eSPI"
        )
        foreach ($p in $searchPaths) {
            if (Test-Path $p) {
                $tftEspiPath = $p
                break
            }
        }
    }

    if ($tftEspiPath -and (Test-Path $tftEspiPath)) {
        $userSetupSrc = Join-Path $SketchDir "User_Setup.h"
        $userSetupDst = Join-Path $tftEspiPath "User_Setup.h"

        # Backup original
        $backupPath = Join-Path $tftEspiPath "User_Setup.h.backup"
        if (-not (Test-Path $backupPath)) {
            Copy-Item $userSetupDst $backupPath -Force -ErrorAction SilentlyContinue
            Write-SubStep "Original User_Setup.h backed up"
        }

        Copy-Item $userSetupSrc $userSetupDst -Force
        Write-Success "TFT_eSPI patched with GC9A01 config at: $tftEspiPath"
    }
    else {
        Write-Warn "Could not find TFT_eSPI library path. You may need to manually copy User_Setup.h"
    }

    # ═══════════════════════════════════════════════════════════
    #  STEP 5: PLACE lv_conf.h FOR LVGL
    # ═══════════════════════════════════════════════════════════

    Write-Step "Configuring LVGL..."

    # LVGL expects lv_conf.h one level above its library folder
    $lvglPath = ""
    if ($libListJson -and $libListJson.installed_libraries) {
        foreach ($entry in $libListJson.installed_libraries) {
            if ($entry.library.name -eq "lvgl") {
                $lvglPath = $entry.library.install_dir
                break
            }
        }
    }

    if ($lvglPath -and (Test-Path $lvglPath)) {
        $lvglParent = Split-Path -Parent $lvglPath
        $lvConfSrc = Join-Path $SketchDir "lv_conf.h"
        $lvConfDst = Join-Path $lvglParent "lv_conf.h"
        Copy-Item $lvConfSrc $lvConfDst -Force
        Write-Success "lv_conf.h placed at: $lvConfDst"

        # Also copy into the lvgl/src directory as a fallback
        $lvConfDst2 = Join-Path $lvglPath "src" "lv_conf.h"
        Copy-Item $lvConfSrc $lvConfDst2 -Force -ErrorAction SilentlyContinue
        Write-SubStep "Also copied to lvgl/src/ as fallback"
    }
    else {
        Write-Warn "Could not find LVGL library path. lv_conf.h may need manual placement."
    }
}
else {
    Write-Step "Skipping installation (--SkipInstall)"
    # Find arduino-cli
    $existing = Get-Command "arduino-cli" -ErrorAction SilentlyContinue
    if ($existing) {
        $ArduinoCliPath = $existing.Source
    }
    else {
        $localCli = Join-Path $ToolsDir "arduino-cli.exe"
        if (Test-Path $localCli) {
            $ArduinoCliPath = $localCli
        }
        else {
            Write-Fail "arduino-cli not found. Run without -SkipInstall first."
            exit 1
        }
    }
}

# ═══════════════════════════════════════════════════════════
#  STEP 6: COPY CUSTOM PARTITIONS
# ═══════════════════════════════════════════════════════════

Write-Step "Setting up custom partition table..."

# Find the ESP32 core hardware directory to place custom partitions
$esp32HwPath = ""
$configDump = & $ArduinoCliPath config dump --format json 2>$null | ConvertFrom-Json
$dataDir = ""
if ($configDump -and $configDump.directories -and $configDump.directories.data) {
    $dataDir = $configDump.directories.data
}
if (-not $dataDir) {
    $dataDir = "$env:LOCALAPPDATA\Arduino15"
}

# Search for the ESP32-C3 boards.txt to find the hardware path
$boardsTxt = Get-ChildItem -Path $dataDir -Recurse -Filter "boards.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match "esp32[/\\]esp32" } |
    Select-Object -First 1

if ($boardsTxt) {
    $esp32HwPath = Split-Path -Parent $boardsTxt.FullName
    $partitionsDst = Join-Path $esp32HwPath "tools" "partitions"

    if (Test-Path $partitionsDst) {
        $customPartFile = Join-Path $partitionsDst "custom.csv"
        $partitionsSrc = Join-Path $SketchDir "partitions.csv"
        Copy-Item $partitionsSrc $customPartFile -Force
        Write-Success "Custom partition table installed"
    }
    else {
        Write-Warn "Partitions directory not found. Using default partitions."
    }
}
else {
    Write-Warn "ESP32 hardware path not found. Using default partitions."
}

# ═══════════════════════════════════════════════════════════
#  STEP 7: DETECT COM PORT
# ═══════════════════════════════════════════════════════════

if (-not $CompileOnly) {
    if (-not $ComPort) {
        Write-Step "Auto-detecting ESP32 COM port..."

        $boards = & $ArduinoCliPath board list --format json 2>$null | ConvertFrom-Json
        $detectedPort = ""

        if ($boards) {
            foreach ($board in $boards) {
                $portAddr = ""
                $boardName = ""

                # Handle different JSON structures
                if ($board.port -and $board.port.address) {
                    $portAddr = $board.port.address
                }
                elseif ($board.address) {
                    $portAddr = $board.address
                }

                if ($board.matching_boards) {
                    foreach ($mb in $board.matching_boards) {
                        if ($mb.name -match "ESP32" -or $mb.fqbn -match "esp32") {
                            $boardName = $mb.name
                            break
                        }
                    }
                }

                # Also check by port protocol (serial) and common USB-serial chips
                if ($portAddr -match "^COM\d+$") {
                    if ($boardName) {
                        $detectedPort = $portAddr
                        Write-Success "Detected: $boardName on $portAddr"
                        break
                    }
                    elseif (-not $detectedPort) {
                        # Keep first COM port as fallback
                        $detectedPort = $portAddr
                    }
                }
            }
        }

        if (-not $detectedPort) {
            # Fallback: check Windows serial ports
            $serialPorts = [System.IO.Ports.SerialPort]::GetPortNames()
            if ($serialPorts.Count -gt 0) {
                $detectedPort = $serialPorts[0]
                Write-Warn "No ESP32 specifically detected. Using first serial port: $detectedPort"
            }
        }

        if ($detectedPort) {
            $ComPort = $detectedPort
        }
        else {
            Write-Fail "No serial port detected!"
            Write-Host ""
            Write-Host "  Troubleshooting:" -ForegroundColor Yellow
            Write-Host "    1. Connect the ESP32-2424S012 via USB-C" -ForegroundColor Gray
            Write-Host "    2. Install CH340/CP2102 USB driver if needed" -ForegroundColor Gray
            Write-Host "    3. Check Device Manager for COM port" -ForegroundColor Gray
            Write-Host "    4. Run with: .\flash.ps1 -ComPort COM5" -ForegroundColor Gray
            Write-Host ""
            $CompileOnly = $true
            Write-Warn "Switching to compile-only mode"
        }
    }
    else {
        Write-Step "Using specified COM port: $ComPort"
    }
}

# ═══════════════════════════════════════════════════════════
#  STEP 8: COMPILE
# ═══════════════════════════════════════════════════════════

Write-Step "Compiling firmware (this may take 2-5 minutes on first build)..."
Write-Host ""

$buildDir = Join-Path $ScriptDir ".build"
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

# Build the extra flags string
$extraFlagsStr = $BUILD_EXTRA_FLAGS -join " "

$compileArgs = @(
    "compile",
    "--fqbn", $BOARD_FQBN,
    "--build-path", $buildDir,
    "--build-property", "build.extra_flags=$extraFlagsStr",
    "--build-property", "compiler.cpp.extra_flags=-w",
    "--warnings", "none"
)

if ($Verbose) {
    $compileArgs += "--verbose"
}

$compileArgs += $SketchDir

Write-SubStep "FQBN: $BOARD_FQBN"
Write-SubStep "Sketch: $SketchDir"
Write-SubStep "Build dir: $buildDir"
Write-Host ""

$compileOutput = & $ArduinoCliPath @compileArgs 2>&1
$compileExitCode = $LASTEXITCODE

if ($Verbose) {
    $compileOutput | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }
}

if ($compileExitCode -ne 0) {
    Write-Fail "Compilation failed!"
    Write-Host ""

    # Show last 30 lines of output for error diagnosis
    $errorLines = ($compileOutput | Select-Object -Last 30)
    foreach ($line in $errorLines) {
        $lineStr = "$line"
        if ($lineStr -match "error:") {
            Write-Host "    $lineStr" -ForegroundColor Red
        }
        elseif ($lineStr -match "warning:") {
            Write-Host "    $lineStr" -ForegroundColor Yellow
        }
        else {
            Write-Host "    $lineStr" -ForegroundColor DarkGray
        }
    }

    Write-Host ""
    Write-Host "  Common fixes:" -ForegroundColor Yellow
    Write-Host "    • Ensure all libraries are installed (run without -SkipInstall)" -ForegroundColor Gray
    Write-Host "    • Check that User_Setup.h was copied to TFT_eSPI library folder" -ForegroundColor Gray
    Write-Host "    • Check that lv_conf.h was placed next to the lvgl library folder" -ForegroundColor Gray
    Write-Host "    • Try: .\flash.ps1 -Verbose for full output" -ForegroundColor Gray
    exit 1
}

# Parse size info from output
$sizeLines = $compileOutput | Where-Object { "$_" -match "Sketch uses|Global variables" }
foreach ($sl in $sizeLines) {
    Write-SubStep "$sl"
}

Write-Success "Compilation successful!"
Write-Host ""

# ═══════════════════════════════════════════════════════════
#  STEP 9: UPLOAD
# ═══════════════════════════════════════════════════════════

if (-not $CompileOnly) {
    Write-Step "Uploading firmware to $ComPort..."
    Write-Host ""
    Write-Host "    ┌──────────────────────────────────────────────────┐" -ForegroundColor DarkYellow
    Write-Host "    │  If upload fails, hold the BOOT button on the   │" -ForegroundColor DarkYellow
    Write-Host "    │  board while pressing RST, then try again.      │" -ForegroundColor DarkYellow
    Write-Host "    └──────────────────────────────────────────────────┘" -ForegroundColor DarkYellow
    Write-Host ""

    $uploadArgs = @(
        "upload",
        "--fqbn", $BOARD_FQBN,
        "--port", $ComPort,
        "--input-dir", $buildDir
    )

    if ($Verbose) {
        $uploadArgs += "--verbose"
    }

    $uploadArgs += $SketchDir

    $uploadOutput = & $ArduinoCliPath @uploadArgs 2>&1
    $uploadExitCode = $LASTEXITCODE

    if ($Verbose) {
        $uploadOutput | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }
    }

    if ($uploadExitCode -ne 0) {
        Write-Fail "Upload failed!"
        Write-Host ""

        $errorLines = ($uploadOutput | Select-Object -Last 15)
        foreach ($line in $errorLines) {
            Write-Host "    $line" -ForegroundColor DarkGray
        }

        Write-Host ""
        Write-Host "  Troubleshooting:" -ForegroundColor Yellow
        Write-Host "    1. Hold BOOT button, press RST, release BOOT, then retry" -ForegroundColor Gray
        Write-Host "    2. Check that the correct COM port is selected" -ForegroundColor Gray
        Write-Host "    3. Close any Serial Monitor that might be using the port" -ForegroundColor Gray
        Write-Host "    4. Try a different USB cable (some are charge-only)" -ForegroundColor Gray
        Write-Host "    5. Install CH340 or CP2102 USB-to-Serial driver" -ForegroundColor Gray
        exit 1
    }

    Write-Success "Firmware uploaded successfully!"
    Write-Host ""

    # ═══════════════════════════════════════════════════════════
    #  STEP 10: OPEN SERIAL MONITOR
    # ═══════════════════════════════════════════════════════════

    Write-Host "  ╔══════════════════════════════════════════╗" -ForegroundColor Green
    Write-Host "  ║         FLASH COMPLETE!                  ║" -ForegroundColor Green
    Write-Host "  ╚══════════════════════════════════════════╝" -ForegroundColor Green
    Write-Host ""
    Write-Host "  Next steps:" -ForegroundColor Cyan
    Write-Host "    1. The watch will boot and show the HUONYX watch face" -ForegroundColor White
    Write-Host "    2. Connect to WiFi AP: HuonyxWatch (pass: huonyx2024)" -ForegroundColor White
    Write-Host "    3. Open http://192.168.4.1/setup in your browser" -ForegroundColor White
    Write-Host "    4. Enter your WiFi, Gateway Host, and Gateway Token" -ForegroundColor White
    Write-Host "    5. Save & Restart - you're connected!" -ForegroundColor White
    Write-Host ""

    $openMonitor = Read-Host "  Open Serial Monitor? (y/n)"
    if ($openMonitor -eq "y" -or $openMonitor -eq "Y") {
        Write-Step "Opening Serial Monitor on $ComPort at 115200 baud..."
        Write-Host "  (Press Ctrl+C to exit)" -ForegroundColor Gray
        Write-Host ""
        & $ArduinoCliPath monitor -p $ComPort -c baudrate=115200
    }
}
else {
    Write-Host ""
    Write-Host "  ╔══════════════════════════════════════════╗" -ForegroundColor Green
    Write-Host "  ║       COMPILATION COMPLETE!              ║" -ForegroundColor Green
    Write-Host "  ╚══════════════════════════════════════════╝" -ForegroundColor Green
    Write-Host ""
    Write-Host "  Binary location: $buildDir\HuonyxWatch.ino.bin" -ForegroundColor Gray
    Write-Host ""
    Write-Host "  To upload later:" -ForegroundColor Cyan
    Write-Host "    .\flash.ps1 -SkipInstall -ComPort COM5" -ForegroundColor White
    Write-Host ""
}
