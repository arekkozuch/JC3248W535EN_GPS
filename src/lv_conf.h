#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/

/* Color depth: 1 (1 bpp), 8 (RGB332), 16 (RGB565), 32 (ARGB8888) */
#define LV_COLOR_DEPTH 16

/* Swap the 2 bytes of RGB565 color. Useful if the display has a 8 bit interface (e.g. SPI)*/
#define LV_COLOR_16_SWAP 0

/* Enable features to draw on transparent background.
 * It's required if opa, and transform_* style properties are used.
 * Can be also used if the UI is above another layer, e.g. an OSD menu or video player.*/
#define LV_COLOR_SCREEN_TRANSP 0

/* Images pixels with this color will not be drawn if they are chroma keyed) */
#define LV_COLOR_CHROMA_KEY lv_color_hex(0x00ff00)         /* pure green */

/*=========================
   MEMORY SETTINGS
 *=========================*/

/* 1: use custom malloc/free, 0: use the built-in `lv_mem_alloc()` and `lv_mem_free()` */
#define LV_MEM_CUSTOM 1
#if LV_MEM_CUSTOM == 0
    /* Size of the memory available for `lv_mem_alloc()` in bytes (>= 2kB)*/
    #define LV_MEM_SIZE (48U * 1024U)          /* [bytes] */
    /* Set an address for the memory pool instead of allocating it as a normal array. */
    #define LV_MEM_ADR 0     /* 0: unused */
#else       /* LV_MEM_CUSTOM */
    #define LV_MEM_CUSTOM_INCLUDE <stdlib.h>   /* Header for the dynamic memory function */
    #define LV_MEM_CUSTOM_ALLOC malloc
    #define LV_MEM_CUSTOM_FREE free
    #define LV_MEM_CUSTOM_REALLOC realloc
#endif     /* LV_MEM_CUSTOM */

/* Number of the intermediate memory buffer used during rendering and other internal processing.
 * You will see an error log message if there wasn't enough buffers. */
#define LV_MEM_BUF_MAX_NUM 16

/* Use the standard `memcpy` and `memset` instead of LVGL's own functions. (Might or might not be faster). */
#define LV_MEMCPY_MEMSET_STD 1

/*====================
   HAL SETTINGS
 *====================*/

/* Default display refresh period. LVG will redraw changed areas with this period time*/
#define LV_DISP_DEF_REFR_PERIOD 10      /* [ms] */

/* Input device read period in milliseconds */
#define LV_INDEV_DEF_READ_PERIOD 10     /* [ms] */

/* Use a custom tick source that tells the elapsed time in milliseconds.
 * It removes the need to manually update the tick with `lv_tick_inc()`) */
#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM
    #define LV_TICK_CUSTOM_INCLUDE "Arduino.h"         /* Header for the system time function */
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())    /* Expression evaluating to current system time in ms */
#endif   /* LV_TICK_CUSTOM */

/* Default Dot Per Inch. Used to initialize default sizes such as widgets sized, style paddings.
 * (Not so important, you can adjust it to modify default sizes and spaces)*/
#define LV_DPI_DEF 130     /* [px/inch] */

/*======================
   FEATURE CONFIGURATION
 *======================*/

/* Enable/disable the built in printf buffer */
#define LV_SPRINTF_CUSTOM 1
#if LV_SPRINTF_CUSTOM
    #define LV_SPRINTF_INCLUDE <stdio.h>
    #define lv_snprintf  snprintf
    #define lv_vsnprintf vsnprintf
#else   /* LV_SPRINTF_CUSTOM */
    #define LV_SPRINTF_USE_FLOAT 0
#endif  /* LV_SPRINTF_CUSTOM */

/* Enable animations */
#define LV_USE_ANIMATION 1
#if LV_USE_ANIMATION
    /* Declare the type of the user data of animations (can be `void *`, `int`, pointer etc) */
    #define LV_ANIM_USER_DATA_TYPE void *
#endif

/* Enable Asserts */
#define LV_USE_ASSERT_NULL          1   /* Check if the parameter is NULL. (Very fast, recommended) */
#define LV_USE_ASSERT_MALLOC        1   /* Checks if the memory is successfully allocated or no. (Very fast, recommended) */
#define LV_USE_ASSERT_STYLE         0   /* Check if the style is properly initialized. (Very fast, recommended) */
#define LV_USE_ASSERT_MEM_INTEGRITY 0   /* Check the integrity of `lv_mem` after critical operations. (Slow) */
#define LV_USE_ASSERT_OBJ           0   /* Check the object's type and existence (e.g. not deleted). (Slow) */

/* Enable/disable Log module */
#define LV_USE_LOG 1
#if LV_USE_LOG
    /* How important log should be added:
     * LV_LOG_LEVEL_TRACE       A lot of logs to give detailed information
     * LV_LOG_LEVEL_INFO        Log important events
     * LV_LOG_LEVEL_WARN        Log if something unwanted happened but didn't cause a problem
     * LV_LOG_LEVEL_ERROR       Only critical issue, when the system may fail
     * LV_LOG_LEVEL_USER        Only logs added by the user
     * LV_LOG_LEVEL_NONE        Do not log anything */
    #define LV_LOG_LEVEL LV_LOG_LEVEL_WARN

    /* 1: Print the log with 'printf';
     * 0: User need to register a callback with `lv_log_register_print_cb()`*/
    #define LV_LOG_PRINTF 1

    /* Enable/disable LV_LOG_TRACE in modules that produces a huge number of logs */
    #define LV_LOG_TRACE_MEM        1
    #define LV_LOG_TRACE_TIMER      1
    #define LV_LOG_TRACE_INDEV      1
    #define LV_LOG_TRACE_DISP_REFR  1
    #define LV_LOG_TRACE_EVENT      1
    #define LV_LOG_TRACE_OBJ_CREATE 1
    #define LV_LOG_TRACE_LAYOUT     1
    #define LV_LOG_TRACE_ANIM       1
#endif  /* LV_USE_LOG */

/*=================
   WIDGET USAGE
 *================*/

/* Documentation of the widgets: https://docs.lvgl.io/latest/en/html/widgets/index.html */

#define LV_USE_ARC          1
#define LV_USE_BAR          1
#define LV_USE_BTN          1
#define LV_USE_BTNMATRIX    1
#define LV_USE_CANVAS       1
#define LV_USE_CHECKBOX     1
#define LV_USE_DROPDOWN     1   /* Requires: lv_label */
#define LV_USE_IMG          1   /* Requires: lv_label */
#define LV_USE_LABEL        1
#define LV_USE_LINE         1
#define LV_USE_ROLLER       1   /* Requires: lv_label */
#define LV_USE_SLIDER       1   /* Requires: lv_bar */
#define LV_USE_SWITCH       1
#define LV_USE_TEXTAREA     1   /* Requires: lv_label */
#define LV_USE_TABLE        1   /* Requires: lv_label */

/*==================
 * THEME USAGE
 *================*/

/* A simple, impressive and very complete theme */
#define LV_USE_THEME_DEFAULT 1
#if LV_USE_THEME_DEFAULT
    /* 0: Light mode; 1: Dark mode */
    #define LV_THEME_DEFAULT_DARK 1
    /* 1: Enable grow on press */
    #define LV_THEME_DEFAULT_GROW 1
    /* Default transition time in [ms] */
    #define LV_THEME_DEFAULT_TRANSITION_TIME 80
#endif /* LV_USE_THEME_DEFAULT */

/* A very simple theme that is a good starting point for a custom theme */
#define LV_USE_THEME_BASIC 1

/* A theme designed for monochrome displays */
#define LV_USE_THEME_MONO 1

/*==================
 * LAYOUTS
 *================*/

/* A layout similar to Flexbox in CSS. */
#define LV_USE_FLEX 1

/* A layout similar to Grid in CSS. */
#define LV_USE_GRID 1

/*==================
 * 3RD PARTS LIBRARIES
 *================*/

/* File system interfaces for common APIs */

/* API for fopen, fread, etc */
#define LV_USE_FS_STDIO 0
#if LV_USE_FS_STDIO
    #define LV_FS_STDIO_LETTER '\0'     /* Set an upper cased letter on which the drive will accessible (e.g. 'A') */
    #define LV_FS_STDIO_PATH ""         /* Set the working directory. File/directory paths will be appended to it. */
    #define LV_FS_STDIO_CACHE_SIZE 0    /* >0 to cache this number of bytes in lv_fs_read() */
#endif

/* API for open, read, etc */
#define LV_USE_FS_POSIX 0
#if LV_USE_FS_POSIX
    #define LV_FS_POSIX_LETTER '\0'     /* Set an upper cased letter on which the drive will accessible (e.g. 'A') */
    #define LV_FS_POSIX_PATH ""         /* Set the working directory. File/directory paths will be appended to it. */
    #define LV_FS_POSIX_CACHE_SIZE 0    /* >0 to cache this number of bytes in lv_fs_read() */
#endif

/* API for CreateFile, ReadFile, etc */
#define LV_USE_FS_WIN32 0
#if LV_USE_FS_WIN32
    #define LV_FS_WIN32_LETTER '\0'     /* Set an upper cased letter on which the drive will accessible (e.g. 'A') */
    #define LV_FS_WIN32_PATH ""         /* Set the working directory. File/directory paths will be appended to it. */
    #define LV_FS_WIN32_CACHE_SIZE 0    /* >0 to cache this number of bytes in lv_fs_read() */
#endif

/* API for FATFS (needs to be added separately). Uses f_open, f_read, etc */
#define LV_USE_FS_FATFS 0
#if LV_USE_FS_FATFS
    #define LV_FS_FATFS_LETTER '\0'     /* Set an upper cased letter on which the drive will accessible (e.g. 'A') */
    #define LV_FS_FATFS_CACHE_SIZE 0    /* >0 to cache this number of bytes in lv_fs_read() */
#endif

/* PNG decoder library */
#define LV_USE_PNG 0

/* BMP decoder library */
#define LV_USE_BMP 0

/* JPG + split JPG decoder library.
 * Split JPG is a custom format optimized for embedded systems. */
#define LV_USE_SJPG 0

/* GIF decoder library */
#define LV_USE_GIF 0

/* QR code library */
#define LV_USE_QRCODE 0

/* FreeType library */
#define LV_USE_FREETYPE 0

/* FFmpeg library for image decoding and playing videos
 * Supports all major image formats so do not enable other image decoder with it */
#define LV_USE_FFMPEG 0

/* Rlottie library */
#define LV_USE_RLOTTIE 0

/*==================
 * OTHERS
 *================*/

/* 1: Enable API to take snapshot for object */
#define LV_USE_SNAPSHOT 0

/* 1: Enable Monkey test */
#define LV_USE_MONKEY 0

/* 1: Enable grid navigation */
#define LV_USE_GRIDNAV 0

/* 1: Enable lv_obj fragment */
#define LV_USE_FRAGMENT 0

/* 1: Support using images as font in label or span widgets */
#define LV_USE_IMGFONT 0

/* 1: Enable a published subscriber based messaging system */
#define LV_USE_MSG 0

/* 1: Enable Pinyin input method */
/* Requires: lv_keyboard */
#define LV_USE_IME_PINYIN 0

/*==================
 * EXAMPLES
 *================*/

/* Enable the examples to be built with the library */
#define LV_BUILD_EXAMPLES 0

/*==================
 * DEMO USAGE
 *================*/

/* Show some widget. You might need to tune the `LV_MEM_SIZE` above to include the demos */
#define LV_USE_DEMO_WIDGETS 0
#if LV_USE_DEMO_WIDGETS
    #define LV_DEMO_WIDGETS_SLIDESHOW 0
#endif

/* Demonstrate the usage of encoder and keyboard */
#define LV_USE_DEMO_KEYPAD_AND_ENCODER 0

/* Benchmark your system */
#define LV_USE_DEMO_BENCHMARK 0

/* Stress test for LVGL */
#define LV_USE_DEMO_STRESS 0

/* Music player demo */
#define LV_USE_DEMO_MUSIC 0
#if LV_USE_DEMO_MUSIC
    #define LV_DEMO_MUSIC_SQUARE       0
    #define LV_DEMO_MUSIC_LANDSCAPE    0
    #define LV_DEMO_MUSIC_ROUND        0
    #define LV_DEMO_MUSIC_LARGE        0
    #define LV_DEMO_MUSIC_AUTO_PLAY    0
#endif

#endif /*LV_CONF_H*/