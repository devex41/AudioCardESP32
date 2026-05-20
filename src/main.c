#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <driver\uart.h>
#include <freertos\FreeRTOS.h>
#include <freertos\task.h>
#include <freertos\semphr.h>
#include <driver\gpio.h>
#include <stdarg.h>
#include <driver/dac_cosine.h>
#include <esp_adc/adc_continuous.h>
#include <esp_cpu.h>


#ifdef __cplusplus
#define BEGIN_EXTERN_C  extern "C" {
#define END_EXTERN_C }
#else
#define BEGIN_EXTERN_C 
#define END_EXTERN_C 
#endif


#define UART_BUFFER_SIZE 1024*4
#define UART_PORT UART_NUM_0
#define UART_BAUDRATE 1200000
#define DELAY_MS 1000


#define ADC_FRAME_VAL 64
#define ADC_FREQUENCY 48000
#define ADC_UART_SAMPLE_BYTEWIDTH 2   // 1 or 2 byte for sample
#define MEASURE_PER_SAMPLE 8          // must be multiple of two
#if (0b1 == MEASURE_PER_SAMPLE & 0b1 &&  MEASURE_PER_SAMPLE != 1)
#error "MEASURE_PER_SAMPLE must be multiple of two"
#endif
#define ADC_FREQ_TO_SAMPLE_COEF (117345.f/48000.f)
#define ADC_SAMPLE_RATE ((unsigned long) (ADC_FREQUENCY * ADC_FREQ_TO_SAMPLE_COEF * MEASURE_PER_SAMPLE))

#define FILTR_COUNT 6


void init();

adc_continuous_handle_t adc_handle = NULL;
TaskHandle_t adc_task_handle = NULL;
dac_cosine_handle_t dac_cosine_handle = NULL;
SemaphoreHandle_t mutex;


uint32_t adc_ret_num = 0;
adc_digi_output_data_t adc_buf[(ADC_FRAME_VAL * SOC_ADC_DIGI_DATA_BYTES_PER_CONV * MEASURE_PER_SAMPLE) / sizeof(adc_digi_output_data_t)] = { 0 };

int16_t val[((sizeof(adc_buf) / SOC_ADC_DIGI_DATA_BYTES_PER_CONV) / MEASURE_PER_SAMPLE + 1) + (FILTR_COUNT - 1)];

uint16_t pack_header = 0xFFFF;

static bool IRAM_ATTR adc_conv_done_callback(adc_continuous_handle_t handle, const adc_continuous_evt_data_t* edata, void* user_data) {
    BaseType_t mustYield = pdFALSE;
    //Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(adc_task_handle, &mustYield);
    portYIELD_FROM_ISR(mustYield);
    return (mustYield == pdTRUE);
}

void adc_task(void*) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        adc_continuous_read(adc_handle, (uint8_t*)adc_buf, sizeof(adc_buf), &adc_ret_num, 0);
        if (sizeof(adc_buf) != (int)adc_ret_num) {
            continue;
            // printf("blyat");
        }


#if (ADC_UART_SAMPLE_BYTEWIDTH == 1)

        uint8_t val[sizeof(adc_buf) / SOC_ADC_DIGI_DATA_BYTES_PER_CONV];
        for (int i = 0; i < sizeof(val); ++i) {
            val[i] = adc_buf[i].type1.data >> 4;
        }
        uart_write_bytes(UART_PORT, (const void*)&val, sizeof(val));

#elif (ADC_UART_SAMPLE_BYTEWIDTH == 2)

        // unsigned int aaa = (unsigned int)esp_cpu_get_cycle_count();
        for (int i = 0; i < (sizeof(val) / 2 - 1 - (FILTR_COUNT - 1)); ++i) {
            // val[i] = (adc_buf[i * 8].type1.data + adc_buf[(i * 8) + 2].type1.data + adc_buf[(i * 8) + 4].type1.data + adc_buf[(i * 8) + 6].type1.data >> 2;
            val[i + (FILTR_COUNT - 1)] = (adc_buf[i * 16].type1.data + adc_buf[(i * 16) + 2].type1.data + adc_buf[(i * 16) + 4].type1.data + adc_buf[(i * 16) + 6].type1.data
                + adc_buf[i * 16 + 8].type1.data + adc_buf[(i * 16) + 10].type1.data + adc_buf[(i * 16) + 12].type1.data + adc_buf[(i * 16) + 14].type1.data) >> (3);
            // val[i] = adc_buf[i].type1.data;
            if ((val[i + (FILTR_COUNT - 1)] & 0x00FF) == 0xFF) {
                val[i + (FILTR_COUNT - 1)] -= 1;
            }
        }
        // unsigned int bbb = (unsigned int)esp_cpu_get_cycle_count();
        // printf("-------------------------\n");
        // printf("%d \n", (unsigned int)aaa);
        // printf("%d \n", (unsigned int)bbb);
        // printf("%d \n", (unsigned int)(bbb - aaa));

        // скользящий средний фильтр
        for (int i = 0; i < ((sizeof(val) / 2 - 1) - (FILTR_COUNT - 1)); ++i) {
            for (int l = 1; l < FILTR_COUNT; ++l) {
                val[i] += val[i + l];
            }
            val[i] = val[i] / FILTR_COUNT;
        }

        // val[(sizeof(val) / 2 - 1)] = 0xFFFF;

        uart_write_bytes(UART_PORT, (const void*)&val, (sizeof(val) - 2 - (FILTR_COUNT - 1) * sizeof(*val)));
        uart_write_bytes(UART_PORT, (const void*)&pack_header, sizeof(pack_header));
        for (int i = 0; i < (FILTR_COUNT - 1); ++i) {
            val[i] = val[(sizeof(val) / 2 - FILTR_COUNT) + i];
        }

#else 
#error "ADC_UART_SAMPLE_BYTEWIDTH must be 1 or 2"
#endif
    }
}

BEGIN_EXTERN_C
void app_main() {
    init();
    mutex = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(adc_task, "adc_task", 4096, NULL, 5, &adc_task_handle, 0);
    // dac_cosine_start(dac_cosine_handle);
    adc_continuous_start(adc_handle);
    for (;;) {
        // xSemaphoreTake(mutex, portMAX_DELAY);
        // if (ff == 3 + 3) {
        //     ff = 4 + 3;
        //     adc_continuous_stop(adc_handle);
        //     for (int i = 0; i < 3 + 3; ++i) {
        //         for (int j = 0; j < sizeof(*buf) / 2; j++) {
        //             // printf("%d, %d\n", buf[i][j], (i + 1) * j);
        //             printf("%d\n", buf[i][j]);
        //         }
        //     }
        // }
        // xSemaphoreGive(mutex);
        //printf("hel\n");
        vTaskDelay(pdMS_TO_TICKS(DELAY_MS));
    }
}
END_EXTERN_C


void init() {
    adc_continuous_handle_cfg_t adc_handle_config = {
        .max_store_buf_size = 1024 * 4,
        .conv_frame_size = (SOC_ADC_DIGI_DATA_BYTES_PER_CONV * ADC_FRAME_VAL * MEASURE_PER_SAMPLE)
    };

    adc_digi_pattern_config_t adc_pattern[1];
    adc_pattern[0].atten = ADC_ATTEN_DB_12;
    adc_pattern[0].channel = ADC_CHANNEL_0;
    adc_pattern[0].unit = ADC_UNIT_1;
    adc_pattern[0].bit_width = ADC_BITWIDTH_12;

    adc_continuous_config_t adc_config = {
        .pattern_num = 1,
        .adc_pattern = adc_pattern,
        .sample_freq_hz = (ADC_SAMPLE_RATE),
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
    };
    adc_continuous_evt_cbs_t adc_callback = {
        .on_conv_done = adc_conv_done_callback
    };

    adc_continuous_new_handle(&adc_handle_config, &adc_handle);
    adc_continuous_config(adc_handle, &adc_config);
    adc_continuous_register_event_callbacks(adc_handle, &adc_callback, NULL);
    //adc_continuous_read(adc_handle, adc_buf, sizeof(adc_buf), &adc_ret_num, 0);

    static QueueHandle_t uart_queue;
    uart_driver_install(UART_PORT, UART_BUFFER_SIZE, UART_BUFFER_SIZE, 10, &uart_queue, 0);
    uart_config_t uart_config = {
        .baud_rate = UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122
    };
    // Configure UART parameters
    uart_param_config(UART_PORT, &uart_config);
    // dac_cosine_config_t dac_cosine_config = {
    //     .chan_id = DAC_CHAN_0,
    //     .freq_hz = 380,
    //     .atten = DAC_COSINE_ATTEN_DB_6,
    //     .offset = 67
    // };
    // dac_cosine_new_channel(&dac_cosine_config, &dac_cosine_handle);

}