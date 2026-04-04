/**
 * @file lv_port_disp_template.c
 *
 */

/*Copy this file as "lv_port_disp.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp.h"
#include <stdbool.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspge.h>
#include <vram.h>
#include <pspkerneltypes.h>
#include <pspdmac.h>
#include <psputils.h>
#include <pspkernel.h>
#include <string.h>
#include "printk.h"


/*********************
 *      DEFINES
 *********************/
#define MY_DISP_HOR_RES    480
#define MY_DISP_VER_RES    272
#define BYTE_PER_PIXEL (LV_COLOR_DEPTH / 8)
#define PSP_BUF_WIDTH 512
#define FB_NUM_PIXEL (PSP_BUF_WIDTH * MY_DISP_VER_RES)
#define FBSIZE (FB_NUM_PIXEL * BYTE_PER_PIXEL)
#define PROFILE LV_USE_SYSMON

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);

static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);

/**********************
 *  STATIC VARIABLES
 **********************/
static char *disp_buf_0;
static char *disp_buf_1;
static char draw_buf[FBSIZE];
static char draw_buf_br_flip[FBSIZE];

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(void)
{
    // allocate buffers
    disp_buf_0 = vramalloc(FBSIZE);
    disp_buf_1 = vramalloc(FBSIZE);


    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init();

    /*------------------------------------
     * Create a display and set a flush_cb
     * -----------------------------------*/
    lv_display_t * disp = lv_display_create(PSP_BUF_WIDTH, MY_DISP_VER_RES);
    lv_display_set_flush_cb(disp, disp_flush);

    // render to draw buffer in direct mode
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_DIRECT);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*Initialize your display and the required peripherals.*/
static void disp_init(void)
{
    /*You code here*/
    sceDisplaySetMode(PSP_DISPLAY_MODE_LCD, MY_DISP_HOR_RES, MY_DISP_VER_RES);
    sceDisplayWaitVblankStart();
}

volatile bool disp_flush_enabled = true;

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

/* Disable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

static void dcache_writeback(uint32_t addr, int size){
    static const int alignment = 64;

    int addr_mod = (addr % alignment);
    size += addr_mod;
    addr -= addr_mod;

    int size_mod = size % alignment;
    if (size_mod != 0){
        size += (alignment - size_mod);
    }

    sceKernelDcacheWritebackRange((void *)addr, size);
}

// on no, this is gonna be very slow
static void swap_br_8888(char *dst, const char *src){
    #if PROFILE
    uint64_t begin = sceKernelGetSystemTimeWide();
    #endif

    for(int line = 0;line < MY_DISP_VER_RES;line++){
        for(int pixel = 0;pixel < MY_DISP_HOR_RES;pixel++){
            char *dst_pixel = &dst[(line * PSP_BUF_WIDTH + pixel) * 4];
            char *src_pixel = &src[(line * PSP_BUF_WIDTH + pixel) * 4];
            dst_pixel[3] = src_pixel[3];
            dst_pixel[2] = src_pixel[0];
            dst_pixel[1] = src_pixel[1];
            dst_pixel[0] = src_pixel[2];
        }
    }

    #if PROFILE
    uint32_t timespent = sceKernelGetSystemTimeWide() - begin;
    printk("%s: took %u us\n", __func__, timespent);
    #endif
}

static void swap_br_565(char *dst, const char *src){
    #if PROFILE
    uint64_t begin = sceKernelGetSystemTimeWide();
    #endif

    for(int line = 0;line < MY_DISP_VER_RES;line++){
        for(int pixel = 0;pixel < MY_DISP_HOR_RES;pixel++){
            uint8_t *dst_pixel = (uint8_t *)&dst[(line * PSP_BUF_WIDTH + pixel) * 2];
            uint8_t *src_pixel = (uint8_t *)&src[(line * PSP_BUF_WIDTH + pixel) * 2];

            uint8_t r = src_pixel[1] >> 3;
            uint8_t g_0 = src_pixel[1] & 0b111;
            uint8_t g_1 = src_pixel[0] & 0b11100000;
            uint8_t b = src_pixel[0] & 0b11111;

            dst_pixel[1] = (b << 3) | g_0;
            dst_pixel[0] = (r) | g_1;
        }
    }

    #if PROFILE
    uint32_t timespent = sceKernelGetSystemTimeWide() - begin;
    printk("%s: took %u us\n", __func__, timespent);
    #endif
}

static void dma_copy_and_set_framebuf(char *dst, const char *src, int size, int psp_pixel_format){
    #if PROFILE
    uint64_t begin = sceKernelGetSystemTimeWide();
    #endif

    dcache_writeback((uint32_t)src, size);
    sceDmacMemcpy(dst, src, size);
    sceDisplaySetFrameBuf(dst, PSP_BUF_WIDTH, psp_pixel_format, PSP_DISPLAY_SETBUF_NEXTVSYNC);

    #if PROFILE
    uint32_t timespent = sceKernelGetSystemTimeWide() - begin;
    printk("%s: took %u us\n", __func__, timespent);
    #endif
}

/*Flush the content of the internal buffer the specific area on the display.
 *`px_map` contains the rendered image as raw pixel map and it should be copied to `area` on the display.
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_display_flush_ready()' has to be called when it's finished.*/
static void disp_flush(lv_display_t * disp_drv, const lv_area_t * area, uint8_t * px_map)
{
    static int buffer_toggle = 0;
    char *fb = buffer_toggle ? disp_buf_1 : disp_buf_0;
    buffer_toggle = !buffer_toggle;

    #if LV_COLOR_DEPTH == 16
    int psp_pixel_format = PSP_DISPLAY_PIXEL_FORMAT_565;
    swap_br_565(draw_buf_br_flip, draw_buf);
    #endif

    #if LV_COLOR_DEPTH == 32
    int psp_pixel_format = PSP_DISPLAY_PIXEL_FORMAT_8888;
    swap_br_8888(draw_buf_br_flip, draw_buf);
    #endif

    dma_copy_and_set_framebuf(fb, draw_buf_br_flip, sizeof(draw_buf_br_flip), psp_pixel_format);

    sceDisplayWaitVblankCB();

    /*IMPORTANT!!!
     *Inform the graphics library that you are ready with the flushing*/
    lv_display_flush_ready(disp_drv);
}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
