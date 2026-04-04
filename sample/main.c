//#include "../lvgl/examples/lv_examples.h"
#include "../lvgl/demos/lv_demos.h"
#include "../lvgl/demos/keypad_encoder/lv_demo_keypad_encoder.h"
#include "../lv_port_disp.h"
#include "../lv_port_indev.h"

#include <pspkernel.h>

PSP_MODULE_INFO("LVGL Sample", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

int main(int argc, char** argv)
{
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();
    //lv_example_get_started_3();
    lv_demo_music();
    //lv_demo_keypad_encoder();
    while (true) {
        uint64_t begin = sceKernelGetSystemTimeWide();
        lv_timer_handler();
        uint32_t timespent = sceKernelGetSystemTimeWide() - begin;
        if (timespent < 5000){
            // we yield in display driver anyway
            sceKernelDelayThread(5000 - timespent);
        }
    }
}
