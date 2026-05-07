/**
 * Huonyx Watch – Hardware Configuration
 * Board: Waveshare ESP32-S3-Touch-AMOLED-2.06
 * Display: CO5300 412x412 AMOLED (QSPI)
 * Touch: FT3168 (I2C)
 * PMU: AXP2101
 * IMU: QMI8658
 * RTC: PCF85063
 * Audio: ES8311 codec + mic
 */
#ifndef HW_CONFIG_H
#define HW_CONFIG_H

/* ── Firmware Info ─────────────────────────────────────── */
#define FIRMWARE_VERSION    "2.0.0"
#define DEVICE_NAME         "Huonyx Watch"
#define DEVICE_PLATFORM     "esp32-s3"
#define DEVICE_FAMILY       "smartwatch"

/* ── Display: CO5300 AMOLED via QSPI ──────────────────── */
#define LCD_WIDTH           412
#define LCD_HEIGHT          412
#define LCD_CS              12
#define LCD_SCLK            11
#define LCD_SDIO0           4
#define LCD_SDIO1           5
#define LCD_SDIO2           6
#define LCD_SDIO3           7
#define LCD_RESET           8
#define LCD_COL_OFFSET      22
#define LCD_BRIGHTNESS_DEF  180

/* ── Touch: FT3168 via I2C ────────────────────────────── */
#define TOUCH_SDA           15
#define TOUCH_SCL           14
#define TOUCH_INT           38
#define TOUCH_RST           9
#define TOUCH_I2C_ADDR      0x38

/* ── PMU: AXP2101 via I2C ────────────────────────────── */
#define PMU_SDA             15
#define PMU_SCL             14
#define PMU_INT             40
#define PMU_I2C_ADDR        0x34

/* ── IMU: QMI8658 via I2C ────────────────────────────── */
#define IMU_SDA             15
#define IMU_SCL             14
#define IMU_INT1            39
#define IMU_I2C_ADDR        0x6B

/* ── RTC: PCF85063 via I2C ───────────────────────────── */
#define RTC_I2C_ADDR        0x51

/* ── Audio: ES8311 codec ─────────────────────────────── */
#define AUDIO_I2S_MCLK      42
#define AUDIO_I2S_SCLK      41
#define AUDIO_I2S_LRCK      43
#define AUDIO_I2S_DIN       44
#define AUDIO_I2S_DOUT      2
#define AUDIO_PA_EN         46
#define AUDIO_ES8311_ADDR   0x18

/* ── SD Card (SPI) ───────────────────────────────────── */
#define SD_CS               13
#define SD_MOSI             48
#define SD_MISO             47
#define SD_SCLK             21

/* ── Buttons ─────────────────────────────────────────── */
#define BTN_1               0   /* Boot button */

/* ── Vibration Motor ─────────────────────────────────── */
#define MOTOR_PIN           3

/* ── Gateway Configuration ───────────────────────────── */
#define GATEWAY_HOST        "hanis-mac-mini.tailfbdfbb.ts.net"
#define GATEWAY_PORT        443
#define GATEWAY_USE_SSL     true
#define GATEWAY_PASSWORD    "Hu5321806"
#define GATEWAY_RECONNECT_MS  5000
#define GATEWAY_TICK_TIMEOUT_MS  90000

/* ── UI Configuration ────────────────────────────────── */
#define SCREEN_CENTER_X     206
#define SCREEN_CENTER_Y     206
#define SCREEN_RADIUS       206
#define CHAT_MAX_MSG_LEN    256
#define CHAT_MAX_MESSAGES   20
#define SLEEP_TIMEOUT_MS    30000
#define UI_FPS              30

/* ── WiFi ────────────────────────────────────────────── */
#define WIFI_CONNECT_TIMEOUT_MS  15000
#define WIFI_MAX_SSID_LEN   32
#define WIFI_MAX_PASS_LEN   64

#endif /* HW_CONFIG_H */
