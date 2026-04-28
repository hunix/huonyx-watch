/**
 * LVGL Configuration for Huonyx Smartwatch
 * M5StickC Plus2 - 135x240 Portrait Display
 *
 * Compatible with LVGL 9.x (9.2+)
 * Based on lv_conf_template.h for v9.2
 *
 * IMPORTANT: Do NOT add #include <stdint.h> here!
 * This file is included by LVGL's .S assembly files (e.g. lv_blend_neon.S)
 * which go through the Xtensa assembler. The assembler cannot process
 * C 'typedef' keywords from stdint.h. LVGL handles stdint.h internally
 * via the LV_STDINT_INCLUDE define below — only from C/C++ code paths.
 */

/* clang-format off */
#if 1 /* Set to "1" to enable content */

#ifndef LV_CONF_H
#define LV_CONF_H

/* If you need to include anything here, do it inside the __ASSEMBLY__ guard */
#if 0 && !defined(__ASSEMBLY__)
#include <stdint.h>
#endif

/*====================
   COLOR SETTINGS
 *====================*/

/*Color depth: 1 (I1), 8 (L8), 16 (RGB565), 24 (RGB888), 32 (XRGB8888)*/
#define LV_COLOR_DEPTH 16

/*=========================
   STDLIB WRAPPER SETTINGS
 *=========================*/

#define LV_USE_STDLIB_MALLOC    LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING    LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_CLIB

#define LV_STDINT_INCLUDE       <stdint.h>
#define LV_STDDEF_INCLUDE       <stddef.h>
#define LV_STDBOOL_INCLUDE      <stdbool.h>
#define LV_INTTYPES_INCLUDE     <inttypes.h>
#define LV_LIMITS_INCLUDE       <limits.h>
#define LV_STDARG_INCLUDE       <stdarg.h>

/*====================
   HAL SETTINGS
 *====================*/

/*Default display refresh, input device read and animation step period.*/
#define LV_DEF_REFR_PERIOD  33      /*[ms]*/

/*Default Dot Per Inch.*/
#define LV_DPI_DEF 200     /*[px/inch]*/

/*=================
 * OPERATING SYSTEM
 *=================*/
#define LV_USE_OS   LV_OS_NONE

/*========================
 * RENDERING CONFIGURATION
 *========================*/

/*Align the stride of all layers and images to this bytes*/
#define LV_DRAW_BUF_STRIDE_ALIGN                1

/*Align the start address of draw_buf addresses to this bytes*/
#define LV_DRAW_BUF_ALIGN                       4

/*Using matrix for transformations.*/
#define LV_DRAW_TRANSFORM_USE_MATRIX            0

/*The target buffer size for simple layer chunks.*/
#define LV_DRAW_LAYER_SIMPLE_BUF_SIZE    (8 * 1024)   /*[bytes] reduced for IRAM savings */

/* The stack size of the drawing thread. */
#define LV_DRAW_THREAD_STACK_SIZE    (8 * 1024)   /*[bytes]*/

#define LV_USE_DRAW_SW 1
#if LV_USE_DRAW_SW == 1
    #define LV_DRAW_SW_SUPPORT_RGB565       1
    #define LV_DRAW_SW_SUPPORT_RGB565A8     0  /* unused — saves IRAM */
    #define LV_DRAW_SW_SUPPORT_RGB888       0  /* unused — saves IRAM */
    #define LV_DRAW_SW_SUPPORT_XRGB8888    0  /* unused — saves IRAM */
    #define LV_DRAW_SW_SUPPORT_ARGB8888    0  /* unused — saves IRAM */
    #define LV_DRAW_SW_SUPPORT_L8          0  /* unused — saves IRAM */
    #define LV_DRAW_SW_SUPPORT_AL88        0  /* unused — saves IRAM */
    #define LV_DRAW_SW_SUPPORT_A8          1  /* needed for font alpha */
    #define LV_DRAW_SW_SUPPORT_I1          0  /* unused — saves IRAM */

    #define LV_DRAW_SW_DRAW_UNIT_CNT    1

    /* MUST be 0 on Xtensa/ESP32 — these are ARM-only */
    #define LV_USE_DRAW_ARM2D_SYNC      0
    #define LV_USE_NATIVE_HELIUM_ASM    0

    /* 0: simple renderer, 1: complex renderer (rounded corners, shadow, arcs) */
    #define LV_DRAW_SW_COMPLEX          1

    #if LV_DRAW_SW_COMPLEX == 1
        #define LV_DRAW_SW_SHADOW_CACHE_SIZE 0
        #define LV_DRAW_SW_CIRCLE_CACHE_SIZE 4
    #endif

    /* CRITICAL: Must be LV_DRAW_SW_ASM_NONE on ESP32 (Xtensa).
     * ARM NEON/Helium assembly will NOT compile on Xtensa and causes
     * the "unknown opcode typedef" assembler error. */
    #define LV_USE_DRAW_SW_ASM     LV_DRAW_SW_ASM_NONE

    #define LV_USE_DRAW_SW_COMPLEX_GRADIENTS    0
#endif

/* GPU acceleration - disabled */
#define LV_USE_DRAW_VGLITE 0
#define LV_USE_PXP 0
#define LV_USE_DRAW_DAVE2D 0
#define LV_USE_DRAW_SDL 0
#define LV_USE_DRAW_VG_LITE 0

/*=======================
 * FEATURE CONFIGURATION
 *=======================*/

/*-------------
 * Logging
 *-----------*/
#define LV_USE_LOG 0

/*-------------
 * Asserts
 *-----------*/
#define LV_USE_ASSERT_NULL          0
#define LV_USE_ASSERT_MALLOC        0
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

/*-------------
 * Others
 *-----------*/

/*1: Show CPU usage and FPS count*/
#define LV_USE_SYSMON   0
#define LV_USE_PROFILER 0
#define LV_USE_MONKEY   0
#define LV_USE_GRIDNAV  0
#define LV_USE_FRAGMENT 0
#define LV_USE_IMGFONT  0
#define LV_USE_OBSERVER 0
#define LV_USE_IME_PINYIN 0
#define LV_USE_FILE_EXPLORER 0

/*1: Enable the built-in animations*/
#define LV_USE_ANIM 1

/*===================
 *  FONT USAGE
 *===================*/

/* Only enable fonts actually used in UI (10, 12, 14, 36) */
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 1  /* hints, status, battery, logs */
#define LV_FONT_MONTSERRAT_12 1  /* buttons, headers, chat text */
#define LV_FONT_MONTSERRAT_14 1  /* date, WiFi, titles, default font */
#define LV_FONT_MONTSERRAT_16 0  /* unused - saves ~7KB ROM */
#define LV_FONT_MONTSERRAT_18 0  /* unused - saves ~8KB ROM */
#define LV_FONT_MONTSERRAT_20 0  /* disabled — use 14 instead to save ~9KB ROM */
#define LV_FONT_MONTSERRAT_22 0  /* unused - saves ~9KB ROM */
#define LV_FONT_MONTSERRAT_24 0  /* unused - saves ~10KB ROM */
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 0  /* unused - saves ~11KB ROM */
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 1  /* time display */
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 0

#define LV_FONT_MONTSERRAT_28_COMPRESSED 0
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0
#define LV_FONT_SIMSUN_14_CJK            0
#define LV_FONT_SIMSUN_16_CJK            0

#define LV_FONT_UNSCII_8  0
#define LV_FONT_UNSCII_16 0

#define LV_FONT_CUSTOM_DECLARE

/*Always set a default font*/
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_FONT_FMT_TXT_LARGE 0
#define LV_USE_FONT_COMPRESSED 0
#define LV_USE_FONT_PLACEHOLDER 0  /* no missing-glyph placeholder needed */

/*=================
 *  TEXT SETTINGS
 *=================*/

#define LV_TXT_ENC LV_TXT_ENC_UTF8
#define LV_TXT_BREAK_CHARS " ,.;:-_)]}"
#define LV_TXT_LINE_BREAK_LONG_LEN 0
#define LV_TXT_LINE_BREAK_LONG_PRE_MIN_LEN 3
#define LV_TXT_LINE_BREAK_LONG_POST_MIN_LEN 3

#define LV_USE_BIDI 0
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*==================
 * WIDGETS
 *================*/

#define LV_WIDGETS_HAS_DEFAULT_VALUE  1

/* Only enable widgets actually used in the UI */
#define LV_USE_ANIMIMG    0
#define LV_USE_ARC        0  /* NOT used in code — saves ~3KB */
#define LV_USE_BAR        0  /* NOT used in code (slider also disabled) — saves ~2KB */
#define LV_USE_BUTTON     1  /* quick replies, settings, scan/disconnect */
#define LV_USE_BUTTONMATRIX  0  /* unused - saves ~5KB ROM */
#define LV_USE_CALENDAR   0
#define LV_USE_CANVAS     0
#define LV_USE_CHART      0
#define LV_USE_CHECKBOX   0  /* unused on watch - saves ~2KB ROM */
#define LV_USE_DROPDOWN   0  /* unused on watch - saves ~4KB ROM */
#define LV_USE_IMAGE      0  /* NOT used in code — saves ~3KB */
#define LV_USE_IMAGEBUTTON 0
#define LV_USE_KEYBOARD   0  /* no on-watch keyboard - saves ~6KB ROM */
#define LV_USE_LABEL      1  /* heavily used everywhere */
#if LV_USE_LABEL
    #define LV_LABEL_TEXT_SELECTION 0  /* no text selection on watch */
    #define LV_LABEL_LONG_TXT_HINT 0  /* saves RAM per long label */
    #define LV_LABEL_WAIT_CHAR_COUNT 3
#endif
#define LV_USE_LED        0  /* NOT used in code — saves ~2KB */
#define LV_USE_LINE       0  /* unused - saves ~2KB ROM */
#define LV_USE_LIST       1  /* settings menu, sessions */
#define LV_USE_LOTTIE     0
#define LV_USE_MENU       0
#define LV_USE_MSGBOX     0  /* unused - saves ~3KB ROM */
#define LV_USE_ROLLER     0  /* unused - saves ~3KB ROM */
#define LV_USE_SCALE      0  /* unused - saves ~2KB ROM */
#define LV_USE_SLIDER     0  /* NOT used in code — saves ~3KB */
#define LV_USE_SPAN       0  /* unused - saves ~2KB ROM */
#define LV_USE_SPINBOX    0
#define LV_USE_SPINNER    0  /* NOT used in code — saves ~2KB */
#define LV_USE_SWITCH     0  /* unused on watch - saves ~2KB ROM */
#define LV_USE_TEXTAREA   0  /* unused on watch (web portal instead) - saves ~5KB ROM */
#define LV_USE_TABLE      0
#define LV_USE_TABVIEW    0
#define LV_USE_TILEVIEW   0  /* unused - saves ~2KB ROM */
#define LV_USE_WIN        0

/*==================
 * THEMES
 *==================*/

#define LV_USE_THEME_DEFAULT 1
#if LV_USE_THEME_DEFAULT
    #define LV_THEME_DEFAULT_DARK 1
    #define LV_THEME_DEFAULT_GROW 0  /* disable grow animation - saves CPU */
    #define LV_THEME_DEFAULT_TRANSITION_TIME 80
#endif

#define LV_USE_THEME_SIMPLE 0  /* unused - saves ~5KB ROM */
#define LV_USE_THEME_MONO 0

/*==================
 * LAYOUTS
 *==================*/

#define LV_USE_FLEX 1
#define LV_USE_GRID 0

/*====================
 * 3RD PARTS LIBRARIES
 *====================*/

#define LV_FS_DEFAULT_DRIVE_LETTER '\0'
#define LV_USE_FS_STDIO 0
#define LV_USE_FS_POSIX 0
#define LV_USE_FS_WIN32 0
#define LV_USE_FS_FATFS 0
#define LV_USE_FS_MEMFS 0
#define LV_USE_FS_LITTLEFS 0
#define LV_USE_FS_ARDUINO_ESP_LITTLEFS 0
#define LV_USE_FS_ARDUINO_SD 0

#define LV_USE_LODEPNG 0
#define LV_USE_LIBPNG 0
#define LV_USE_BMP 0
#define LV_USE_TJPGD 0
#define LV_USE_LIBJPEG_TURBO 0
#define LV_USE_GIF 0
#define LV_BIN_DECODER_RAM_LOAD 0
#define LV_USE_RLE 0
#define LV_USE_QRCODE 0
#define LV_USE_BARCODE 0
#define LV_USE_FREETYPE 0
#define LV_USE_TINY_TTF 0
#define LV_USE_RLOTTIE 0
#define LV_USE_VECTOR_GRAPHIC  0
#define LV_USE_THORVG_INTERNAL 0
#define LV_USE_THORVG_EXTERNAL 0
#define LV_USE_LZ4_INTERNAL  0
#define LV_USE_LZ4_EXTERNAL  0
#define LV_USE_FFMPEG 0

#define LV_USE_SNAPSHOT 0

/*==================
 * DEVICES
 *==================*/

#define LV_USE_SDL              0
#define LV_USE_X11              0
#define LV_USE_WAYLAND          0
#define LV_USE_LINUX_FBDEV      0
#define LV_USE_NUTTX            0
#define LV_USE_LINUX_DRM        0
#define LV_USE_TFT_ESPI         0
#define LV_USE_EVDEV            0
#define LV_USE_LIBINPUT         0
#define LV_USE_ST7735           0
#define LV_USE_ST7789           0
#define LV_USE_ST7796           0
#define LV_USE_ILI9341          0
#define LV_USE_RENESAS_GLCDC    0
#define LV_USE_WINDOWS          0
#define LV_USE_OPENGLES         0
#define LV_USE_QNX              0

/*==================
 * EXAMPLES
 *==================*/
#define LV_BUILD_EXAMPLES 0

/*==================
 * DEMO USAGE
 *==================*/
#define LV_USE_DEMO_WIDGETS 0
#define LV_USE_DEMO_KEYPAD_AND_ENCODER 0
#define LV_USE_DEMO_BENCHMARK 0
#define LV_USE_DEMO_RENDER 0
#define LV_USE_DEMO_STRESS 0
#define LV_USE_DEMO_MUSIC 0
#define LV_USE_DEMO_FLEX_LAYOUT     0
#define LV_USE_DEMO_MULTILANG       0
#define LV_USE_DEMO_TRANSFORM       0
#define LV_USE_DEMO_SCROLL          0
#define LV_USE_DEMO_VECTOR_GRAPHIC  0

/*--END OF LV_CONF_H--*/

#endif /*LV_CONF_H*/

#endif /*End of "Content enable"*/
