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
    powershell -ExecutionPolicy Bypass -File .\flash.ps1
    powershell -ExecutionPolicy Bypass -File .\flash.ps1 -ComPort COM5
    powershell -ExecutionPolicy Bypass -File .\flash.ps1 -CompileOnly
    powershell -ExecutionPolicy Bypass -File .\flash.ps1 -SkipInstall -ComPort COM3
#>

param(
    [string]$ComPort = "",
    [switch]$SkipInstall,
    [switch]$CompileOnly,
    [switch]$Verbose
)

$ErrorActionPreference = "Continue"  # Must be Continue so NativeCommandError does not kill output capture

# ===================================================================
#  CONFIGURATION
# ===================================================================

# Use min_spiffs which gives maximum app space (~1.9MB) without needing a custom.csv file
$BOARD_FQBN = "esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashFreq=80,FlashSize=4M,PartitionScheme=min_spiffs,UploadSpeed=921600"
$ESP32_CORE_URL = "https://espressif.github.io/arduino-esp32/package_esp32_index.json"
$ESP32_CORE_VERSION = "3.0.7"

$LIBRARIES = @(
    "TFT_eSPI@2.5.43",
    "lvgl@9.2.2",
    "ArduinoJson@7.2.1",
    "WebSockets@2.6.1",
    "NimBLE-Arduino@2.1.1"
)

# NOTE: All -D defines have been moved into arduino/HuonyxWatch/build_config.h
# This avoids the arduino-cli bug where -D values in --build-property are
# incorrectly parsed as CLI flags. No --build-property flags are needed.

# ===================================================================
#  HELPER FUNCTIONS
# ===================================================================

function Write-Banner {
    Write-Host ""
    Write-Host "+------------------------------------------+" -ForegroundColor Cyan
    Write-Host "|   HUONYX AI SMARTWATCH FLASHER v2.1      |" -ForegroundColor Cyan
    Write-Host "|   ESP32-2424S012 + Flipper Bridge         |" -ForegroundColor Cyan
    Write-Host "+------------------------------------------+" -ForegroundColor Cyan
    Write-Host ""
}

function Write-Step {
    param([string]$Message)
    Write-Host "  >> " -ForegroundColor Green -NoNewline
    Write-Host $Message
}

function Write-SubStep {
    param([string]$Message)
    Write-Host "     - " -ForegroundColor DarkGray -NoNewline
    Write-Host $Message -ForegroundColor Gray
}

function Write-Success {
    param([string]$Message)
    Write-Host "  [OK] " -ForegroundColor Green -NoNewline
    Write-Host $Message -ForegroundColor Green
}

function Write-Warn {
    param([string]$Message)
    Write-Host "  [!!] " -ForegroundColor Yellow -NoNewline
    Write-Host $Message -ForegroundColor Yellow
}

function Write-Fail {
    param([string]$Message)
    Write-Host "  [XX] " -ForegroundColor Red -NoNewline
    Write-Host $Message -ForegroundColor Red
}

# ===================================================================
#  LOCATE PATHS
# ===================================================================

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$SketchDir = Join-Path (Join-Path $ScriptDir "arduino") "HuonyxWatch"
$ToolsDir  = Join-Path $ScriptDir ".tools"
$ArduinoCliPath = ""

if (-not (Test-Path $SketchDir)) {
    Write-Fail "Sketch directory not found: $SketchDir"
    Write-Host "  Make sure you are running this script from the huonyx-watch repository root." -ForegroundColor Gray
    exit 1
}

# ===================================================================
#  STEP 1: INSTALL / LOCATE ARDUINO-CLI
# ===================================================================

Write-Banner

if (-not $SkipInstall) {
    Write-Step "Checking for arduino-cli..."

    $existing = Get-Command "arduino-cli" -ErrorAction SilentlyContinue
    if ($existing) {
        $ArduinoCliPath = $existing.Source
        Write-Success "Found arduino-cli at: $ArduinoCliPath"
    }
    else {
        $localCli = Join-Path $ToolsDir "arduino-cli.exe"
        if (Test-Path $localCli) {
            $ArduinoCliPath = $localCli
            Write-Success "Found local arduino-cli at: $ArduinoCliPath"
        }
        else {
            Write-Step "Downloading arduino-cli..."
            New-Item -ItemType Directory -Force -Path $ToolsDir | Out-Null

            $cliUrl  = "https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Windows_64bit.zip"
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
                Write-Host "    3. Or add it to your PATH and re-run" -ForegroundColor Gray
                exit 1
            }
        }
    }

    # ===================================================================
    #  STEP 2: INSTALL ESP32 BOARD SUPPORT
    # ===================================================================

    Write-Step "Configuring ESP32 board support..."

    & $ArduinoCliPath config init --overwrite 2>$null
    & $ArduinoCliPath config add board_manager.additional_urls $ESP32_CORE_URL 2>$null
    & $ArduinoCliPath core update-index 2>$null

    $installedCores = & $ArduinoCliPath core list --format json 2>$null | ConvertFrom-Json
    $esp32Installed = $false
    $esp32InstalledVersion = ""
    if ($installedCores) {
        foreach ($core in $installedCores) {
            if ($core.id -eq "esp32:esp32") {
                $esp32Installed = $true
                $esp32InstalledVersion = $core.installed
                break
            }
        }
    }

    # If wrong version is installed (e.g. old Arduino IDE version with broken toolchain),
    # uninstall it first so we get a clean install with the correct esp-rv32 toolchain.
    if ($esp32Installed -and $esp32InstalledVersion -ne $ESP32_CORE_VERSION) {
        Write-Warn "Wrong ESP32 core version installed (v$esp32InstalledVersion). Reinstalling v${ESP32_CORE_VERSION}..."
        Write-SubStep "Uninstalling old ESP32 core..."
        & $ArduinoCliPath core uninstall "esp32:esp32" 2>$null
        $esp32Installed = $false
    }

    if ($esp32Installed) {
        Write-Success "ESP32 core v${ESP32_CORE_VERSION} already installed"
    } else {
        Write-SubStep "Installing ESP32 Arduino core v${ESP32_CORE_VERSION} (this may take 5-10 minutes)..."
        $installResult = & $ArduinoCliPath core install "esp32:esp32@${ESP32_CORE_VERSION}" 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Warn "Specific version failed, trying latest..."
            & $ArduinoCliPath core install "esp32:esp32" 2>&1
        }
        Write-Success "ESP32 core installed"
    }

    # ---------------------------------------------------------------
    # CRITICAL: Delete the old esp-rv32/2302 toolchain if it exists.
    # When Arduino IDE has previously installed an older ESP32 core,
    # the old assembler (esp-rv32/2302) remains on disk even after
    # reinstalling. arduino-cli then picks it up instead of the new
    # compiler (esp-rv32/2601), treating C headers as assembly code.
    # Physically removing the old folder forces use of the new toolchain.
    # ---------------------------------------------------------------
    $oldToolchain = "$env:LOCALAPPDATA\Arduino15\packages\esp32\tools\esp-rv32\2302"
    if (Test-Path $oldToolchain) {
        Write-Warn "Found old esp-rv32/2302 toolchain - removing it to prevent conflicts..."
        Remove-Item -Recurse -Force $oldToolchain -ErrorAction SilentlyContinue
        if (-not (Test-Path $oldToolchain)) {
            Write-Success "Old toolchain removed. New esp-rv32/2601 will be used."
        } else {
            Write-Warn "Could not remove old toolchain. Try running PowerShell as Administrator."
            Write-Host "  Manual fix: Delete this folder and retry:" -ForegroundColor Yellow
            Write-Host "  $oldToolchain" -ForegroundColor Gray
        }
    }

    # ===================================================================
    #  STEP 3: INSTALL LIBRARIES
    # ===================================================================

    Write-Step "Installing Arduino libraries..."

    foreach ($lib in $LIBRARIES) {
        $libName = ($lib -split "@")[0]
        Write-SubStep "Installing $lib..."
        & $ArduinoCliPath lib install $lib 2>$null
        if ($LASTEXITCODE -ne 0) {
            Write-Warn "Exact version failed, trying latest $libName..."
            & $ArduinoCliPath lib install $libName 2>$null
        }
    }
    Write-Success "All libraries installed"

    # ===================================================================
    #  STEP 4: PATCH TFT_eSPI User_Setup.h
    # ===================================================================

    Write-Step "Patching TFT_eSPI configuration for ESP32-2424S012..."

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

    if (-not $tftEspiPath -or -not (Test-Path $tftEspiPath)) {
        $searchPaths = @(
            "$env:USERPROFILE\Documents\Arduino\libraries\TFT_eSPI",
            "$env:LOCALAPPDATA\Arduino15\libraries\TFT_eSPI",
            "$env:USERPROFILE\Arduino\libraries\TFT_eSPI"
        )
        foreach ($p in $searchPaths) {
            if (Test-Path $p) { $tftEspiPath = $p; break }
        }
    }

    if ($tftEspiPath -and (Test-Path $tftEspiPath)) {
        $userSetupSrc = Join-Path $SketchDir "User_Setup.h"
        $userSetupDst = Join-Path $tftEspiPath "User_Setup.h"
        $backupPath   = Join-Path $tftEspiPath "User_Setup.h.backup"

        if (-not (Test-Path $backupPath)) {
            Copy-Item $userSetupDst $backupPath -Force -ErrorAction SilentlyContinue
            Write-SubStep "Original User_Setup.h backed up"
        }

        Copy-Item $userSetupSrc $userSetupDst -Force
        Write-Success "TFT_eSPI patched at: $tftEspiPath"
    }
    else {
        Write-Warn "Could not find TFT_eSPI library path. Copy User_Setup.h manually."
    }

    # ===================================================================
    #  STEP 5: PLACE lv_conf.h FOR LVGL
    # ===================================================================

    Write-Step "Configuring LVGL..."

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
        $lvglParent  = Split-Path -Parent $lvglPath
        $lvConfSrc   = Join-Path $SketchDir "lv_conf.h"
        $lvConfDst   = Join-Path $lvglParent "lv_conf.h"
        Copy-Item $lvConfSrc $lvConfDst -Force
        Write-Success "lv_conf.h placed at: $lvConfDst"

        $lvConfDst2 = Join-Path (Join-Path $lvglPath "src") "lv_conf.h"
        Copy-Item $lvConfSrc $lvConfDst2 -Force -ErrorAction SilentlyContinue
        Write-SubStep "Also copied to lvgl/src/ as fallback"
    }
    else {
        Write-Warn "Could not find LVGL library path. Place lv_conf.h manually next to the lvgl folder."
    }
}
else {
    Write-Step "Skipping installation (-SkipInstall)"
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

# ===================================================================
#  STEP 6: COPY CUSTOM PARTITIONS
# ===================================================================

Write-Step "Setting up custom partition table..."

$configDump = & $ArduinoCliPath config dump --format json 2>$null | ConvertFrom-Json
$dataDir = ""
if ($configDump -and $configDump.directories -and $configDump.directories.data) {
    $dataDir = $configDump.directories.data
}
if (-not $dataDir) { $dataDir = "$env:LOCALAPPDATA\Arduino15" }

$boardsTxt = Get-ChildItem -Path $dataDir -Recurse -Filter "boards.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match "esp32[/\\]esp32" } |
    Select-Object -First 1

if ($boardsTxt) {
    $esp32HwPath   = Split-Path -Parent $boardsTxt.FullName
    $partitionsDst = Join-Path (Join-Path $esp32HwPath "tools") "partitions"

    if (Test-Path $partitionsDst) {
        $customPartFile = Join-Path $partitionsDst "custom.csv"
        $partitionsSrc  = Join-Path $SketchDir "partitions.csv"
        if (Test-Path $partitionsSrc) {
            Copy-Item $partitionsSrc $customPartFile -Force
            Write-Success "Custom partition table installed"
        }
        else {
            Write-Warn "partitions.csv not found in sketch folder. Using default partitions."
        }
    }
    else {
        Write-Warn "Partitions directory not found. Using default partitions."
    }
}
else {
    Write-Warn "ESP32 hardware path not found. Using default partitions."
}

# ===================================================================
#  STEP 7: DETECT COM PORT
# ===================================================================

if (-not $CompileOnly) {
    if (-not $ComPort) {
        Write-Step "Auto-detecting ESP32 COM port..."

        $boards = & $ArduinoCliPath board list --format json 2>$null | ConvertFrom-Json
        $detectedPort = ""

        if ($boards) {
            foreach ($board in $boards) {
                $portAddr  = ""
                $boardName = ""

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

                if ($portAddr -match "^COM\d+$") {
                    if ($boardName) {
                        $detectedPort = $portAddr
                        Write-Success "Detected: $boardName on $portAddr"
                        break
                    }
                    elseif (-not $detectedPort) {
                        $detectedPort = $portAddr
                    }
                }
            }
        }

        if (-not $detectedPort) {
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
            Write-Host "    4. Run with: powershell -ExecutionPolicy Bypass -File .\flash.ps1 -ComPort COM5" -ForegroundColor Gray
            Write-Host ""
            $CompileOnly = $true
            Write-Warn "Switching to compile-only mode"
        }
    }
    else {
        Write-Step "Using specified COM port: $ComPort"
    }
}

# ===================================================================
#  STEP 8: COMPILE
# ===================================================================

Write-Step "Compiling firmware (this may take 2-5 minutes on first build)..."
Write-Host ""

$buildDir = Join-Path $ScriptDir ".build"
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

# Build the compile arguments array
$compileArgs = @(
    "compile",
    "--fqbn", $BOARD_FQBN,
    "--build-path", $buildDir,
    "--warnings", "none"
)

# No --build-property needed: all defines are in build_config.h
if ($Verbose) { $compileArgs += "--verbose" }
$compileArgs += $SketchDir

Write-SubStep "FQBN   : $BOARD_FQBN"
Write-SubStep "Sketch : $SketchDir"
Write-SubStep "Build  : $buildDir"
Write-Host ""

# Capture all output - use a temp file to bypass PowerShell NativeCommandError
$compileLogFile = Join-Path $buildDir "compile_output.txt"
$compileProcess = Start-Process -FilePath $ArduinoCliPath `
    -ArgumentList $compileArgs `
    -RedirectStandardOutput $compileLogFile `
    -RedirectStandardError "$compileLogFile.err" `
    -Wait -PassThru -NoNewWindow
$compileExitCode = $compileProcess.ExitCode

# Merge stdout and stderr
$compileOutput = @()
if (Test-Path $compileLogFile) { $compileOutput += Get-Content $compileLogFile }
if (Test-Path "$compileLogFile.err") { $compileOutput += Get-Content "$compileLogFile.err" }

if ($Verbose) {
    $compileOutput | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }
}

if ($compileExitCode -ne 0) {
    Write-Fail "Compilation failed!"
    Write-Host ""

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
    Write-Host "    - Run without -SkipInstall to reinstall all libraries" -ForegroundColor Gray
    Write-Host "    - Check that User_Setup.h was copied to the TFT_eSPI library folder" -ForegroundColor Gray
    Write-Host "    - Check that lv_conf.h was placed next to the lvgl library folder" -ForegroundColor Gray
    Write-Host "    - Run with -Verbose for full compiler output" -ForegroundColor Gray
    exit 1
}

$sizeLines = $compileOutput | Where-Object { "$_" -match "Sketch uses|Global variables" }
foreach ($sl in $sizeLines) { Write-SubStep "$sl" }

Write-Success "Compilation successful!"
Write-Host ""

# ===================================================================
#  STEP 9: UPLOAD
# ===================================================================

if (-not $CompileOnly) {
    Write-Step "Uploading firmware to $ComPort..."
    Write-Host ""
    Write-Host "    +--------------------------------------------------+" -ForegroundColor DarkYellow
    Write-Host "    |  If upload fails, hold BOOT button on the board  |" -ForegroundColor DarkYellow
    Write-Host "    |  while pressing RST, then release BOOT and retry.|" -ForegroundColor DarkYellow
    Write-Host "    +--------------------------------------------------+" -ForegroundColor DarkYellow
    Write-Host ""

    $uploadArgs = @(
        "upload",
        "--fqbn", $BOARD_FQBN,
        "--port", $ComPort,
        "--input-dir", $buildDir
    )

    if ($Verbose) { $uploadArgs += "--verbose" }
    $uploadArgs += $SketchDir

    $uploadLogFile = Join-Path $buildDir "upload_output.txt"
    $uploadProcess = Start-Process -FilePath $ArduinoCliPath `
        -ArgumentList $uploadArgs `
        -RedirectStandardOutput $uploadLogFile `
        -RedirectStandardError "$uploadLogFile.err" `
        -Wait -PassThru -NoNewWindow
    $uploadExitCode = $uploadProcess.ExitCode

    $uploadOutput = @()
    if (Test-Path $uploadLogFile) { $uploadOutput += Get-Content $uploadLogFile }
    if (Test-Path "$uploadLogFile.err") { $uploadOutput += Get-Content "$uploadLogFile.err" }

    if ($Verbose) {
        $uploadOutput | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }
    }

    if ($uploadExitCode -ne 0) {
        Write-Fail "Upload failed!"
        Write-Host ""

        $errorLines = ($uploadOutput | Select-Object -Last 15)
        foreach ($line in $errorLines) { Write-Host "    $line" -ForegroundColor DarkGray }

        Write-Host ""
        Write-Host "  Troubleshooting:" -ForegroundColor Yellow
        Write-Host "    1. Hold BOOT, press RST, release BOOT, then retry" -ForegroundColor Gray
        Write-Host "    2. Check that the correct COM port is selected" -ForegroundColor Gray
        Write-Host "    3. Close any Serial Monitor that may be using the port" -ForegroundColor Gray
        Write-Host "    4. Try a different USB cable (some are charge-only)" -ForegroundColor Gray
        Write-Host "    5. Install CH340 or CP2102 USB-to-Serial driver" -ForegroundColor Gray
        exit 1
    }

    Write-Success "Firmware uploaded successfully!"
    Write-Host ""

    Write-Host "+------------------------------------------+" -ForegroundColor Green
    Write-Host "|          FLASH COMPLETE!                  |" -ForegroundColor Green
    Write-Host "+------------------------------------------+" -ForegroundColor Green
    Write-Host ""
    Write-Host "  Next steps:" -ForegroundColor Cyan
    Write-Host "    1. The watch will boot and show the HUONYX watch face" -ForegroundColor White
    Write-Host "    2. Connect to WiFi AP: HuonyxWatch  (pass: huonyx2024)" -ForegroundColor White
    Write-Host "    3. Open http://192.168.4.1/setup in your browser" -ForegroundColor White
    Write-Host "    4. Enter your WiFi, Gateway Host, and Gateway Token" -ForegroundColor White
    Write-Host "    5. Save & Restart - you are connected!" -ForegroundColor White
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
    Write-Host "+------------------------------------------+" -ForegroundColor Green
    Write-Host "|       COMPILATION COMPLETE!               |" -ForegroundColor Green
    Write-Host "+------------------------------------------+" -ForegroundColor Green
    Write-Host ""
    Write-Host "  Binary location: $buildDir\HuonyxWatch.ino.bin" -ForegroundColor Gray
    Write-Host ""
    Write-Host "  To upload later:" -ForegroundColor Cyan
    Write-Host "    powershell -ExecutionPolicy Bypass -File .\flash.ps1 -SkipInstall -ComPort COM5" -ForegroundColor White
    Write-Host ""
}
