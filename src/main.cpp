#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <driver\uart.h>
#include <freertos\FreeRTOS.h>
#include <freertos\task.h>
#include <freertos\semphr.h>
#include <driver\gpio.h>
#include <string>
#include <stdarg.h>
#include <driver/dac_cosine.h>
#include <esp_adc/adc_continuous.h>


#ifdef __cplusplus
#define BEGIN_EXTERN_C  extern "C" {
#define END_EXTERN_C }
#else
#define BEGIN_EXTERN_C 
#define END_EXTERN_C 
#endif


#define UART_BUFFER_SIZE 1024
#define UART_PORT UART_NUM_0
#define DELAY_MS 1000

#define ADC_FRAME_VAL 10
#define ADC_FREQUENCY 48000


inline void init();

static adc_continuous_handle_t adc_handle;
static TaskHandle_t adc_task_handle;

static bool IRAM_ATTR adc_conv_done_callback(adc_continuous_handle_t handle, const adc_continuous_evt_data_t* edata, void* user_data) {
    BaseType_t mustYield = pdFALSE;
    //Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(adc_task_handle, &mustYield);

    return (mustYield == pdTRUE);
}

BEGIN_EXTERN_C
void app_main() {
    init();
    adc_continuous_start(adc_handle);
    // uart_write_bytes(UART_PORT, data, strlen(data));
    // printf(data);
    // xTaskCreatePinnedToCore(task_hi, "hi", 4096, NULL, 5, NULL, 0);
    // xTaskCreatePinnedToCore(task_bye, "bye", 4096, NULL, 10, NULL, 1);
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(DELAY_MS));
    }
}
END_EXTERN_C


inline void init() {
    adc_continuous_handle_cfg_t adc_handle_config = {
        .max_store_buf_size = 4 * ADC_FRAME_VAL * 5,
        .conv_frame_size = 4 * ADC_FRAME_VAL
    };

    adc_digi_pattern_config_t adc_pattern[1] = {
        .channel = ADC_CHANNEL_0
    };

    adc_continuous_config_t adc_config = {
        .adc_pattern = adc_pattern,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
        .pattern_num = 1,
        .sample_freq_hz = ADC_FREQUENCY
    };
    adc_continuous_evt_cbs_t adc_callback = {
        .on_conv_done = adc_conv_done_callback
    };

    adc_continuous_new_handle(&adc_handle_config, &adc_handle);
    adc_continuous_config(adc_handle, &adc_config);
    adc_continuous_register_event_callbacks(adc_handle, &adc_callback, NULL);


    static QueueHandle_t uart_queue;
    uart_driver_install(UART_PORT, UART_BUFFER_SIZE, UART_BUFFER_SIZE, 10, &uart_queue, 0);
    uart_config_t uart_config = {
        .baud_rate = 250000,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122
    };
    // Configure UART parameters
    uart_param_config(UART_PORT, &uart_config);



}