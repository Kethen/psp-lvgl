//#include "../lvgl/examples/lv_examples.h"
#include "../lvgl/demos/lv_demos.h"
#include "../lvgl/demos/keypad_encoder/lv_demo_keypad_encoder.h"
#include "../lv_port_disp.h"
#include "../lv_port_indev.h"

#include <pspkernel.h>

PSP_MODULE_INFO("LVGL Sample", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

static uint32_t tick_cb(){
	return sceKernelGetSystemTimeWide() / 1000;
}

int main(int argc, char** argv)
{
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();

    static lv_style_t container_style;
    lv_style_init(&container_style);
    lv_obj_t * container = lv_obj_create(lv_screen_active());
    //lv_obj_add_style(container, &container_style, 0);
    lv_obj_remove_style_all(container);
    lv_obj_set_content_width(container, 480);
    lv_obj_set_content_height(container, 272);
    lv_obj_set_size(container, 480, 272);
    //lv_obj_set_layout(container, LV_LAYOUT_FLEX);

    lv_demo_args_t demo_arg = {
        .parent = container,
    };

    //lv_example_get_started_3();
    lv_demo_music_with_args(&demo_arg);

    //lv_demo_keypad_encoder();
    lv_tick_set_cb(tick_cb);
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
