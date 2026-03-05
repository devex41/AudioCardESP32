#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <driver\uart.h>
#include <freertos\FreeRTOS.h>
#include <freertos\task.h>
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

inline void init();




BEGIN_EXTERN_C
void app_main() {
    init();

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
    adc_digi_init_config_t init_config = {
        .adc1_chan_mask = BIT(ADC1_CHANNEL_0),
        .max_store_buf_size = 1024,
        .conv_num_each_intr = SOC_ADC_DIGI_DATA_BYTES_PER_CONV * 8
    };
    adc_digi_initialize(&init_config);

    adc_digi_configuration_t config = {
        .pattern_num = 1,
        .sample_freq_hz = 48000,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1
    };

    adc_digi_controller_configure(&config);


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