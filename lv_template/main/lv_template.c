#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "driver/gpio.h"

#include "lvgl.h"
#include "lvgl_helpers.h"

/*********************
 *      DEFINES
 *********************/
#define TAG "demo"
#define LV_TICK_PERIOD_MS 1

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_tick_task(void *arg);
static void guiTask(void *pvParameter);
static void create_demo_application(void);

/**********************
 *   APPLICATION MAIN
 **********************/
void app_main() {

    /* If you want to use a task to create the graphic, you NEED to create a Pinned task
     * Otherwise there can be problem such as memory corruption and so on.
     * NOTE: When not using Wi-Fi nor Bluetooth you can pin the guiTask to core 0 */
    xTaskCreatePinnedToCore(guiTask, "gui", 4096*2, NULL, 0, NULL, 1);
}

/* Creates a semaphore to handle concurrent call to lvgl stuff
 * If you wish to call *any* lvgl function from other threads/tasks
 * you should lock on the very same semaphore! */
SemaphoreHandle_t xGuiSemaphore;

static void guiTask(void *pvParameter) {

    (void) pvParameter;
    xGuiSemaphore = xSemaphoreCreateMutex();

    lv_init();

    /* Initialize SPI or I2C bus used by the drivers */
    lvgl_driver_init();

    lv_color_t* buf1 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1 != NULL);

    /* Use double buffered when not working with monochrome displays */
    lv_color_t* buf2 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2 != NULL);

    static lv_disp_buf_t disp_buf;

    uint32_t size_in_px = DISP_BUF_SIZE;

    /* Initialize the working buffer depending on the selected display.
     * NOTE: buf2 == NULL when using monochrome displays. */
    lv_disp_buf_init(&disp_buf, buf1, buf2, size_in_px);

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = disp_driver_flush;

    disp_drv.buffer = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    /* Create and start a periodic timer interrupt to call lv_tick_inc */
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "periodic_gui"
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));

    /* Create the demo application */
    create_demo_application();

    while (1) {
        /* Delay 1 tick (assumes FreeRTOS tick is 10ms */
        vTaskDelay(pdMS_TO_TICKS(10));

        /* Try to take the semaphore, call lvgl related function on success */
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
       }
    }

    /* A task should NEVER return */
    free(buf1);
    free(buf2);
    vTaskDelete(NULL);
}

static void create_demo_application(void)
{
        lv_obj_t *base_obj = lv_obj_create(lv_scr_act(), NULL);
        lv_obj_set_size(base_obj, 100, 100);
        lv_obj_set_style_local_bg_color(base_obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xFF00));
/*
        lv_obj_t *base_obj_1 = lv_obj_create(lv_scr_act(), base_obj);
        lv_obj_set_pos(base_obj_1, 100, 100);
        lv_obj_set_style_local_bg_color(base_obj_1, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x5FCD));
        lv_obj_t *base_obj_2 = lv_obj_create(lv_scr_act(), base_obj);
        lv_obj_set_pos(base_obj_2, 200, 200);
        lv_obj_set_style_local_bg_color(base_obj_2, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x100D));
*/
/*
        uint16_t  i=0;
        while(i<0xFFFF){
        lv_obj_set_style_local_bg_color(base_obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x0+i++));
        vTaskDelay(pdTICKS_TO_MS(10));
        i += 100;
        }
*/
    //    lv_label_set_text(base_obj, "ssu");

        //lv_obj_set_pos(base_obj, 4, 4);
}

static void lv_tick_task(void *arg) {
    (void) arg;

    lv_tick_inc(LV_TICK_PERIOD_MS);
}
