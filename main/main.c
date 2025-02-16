/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_board_init.h"
#include "model_path.h"
#include "esp_process_sdkconfig.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include <string.h>

// specific includes for iron man suit control
#include "led_im.h"
#include "servo_im.h"

//led control
#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define RMT_LED_STRIP_GPIO_NUM      21

#define EXAMPLE_LED_NUMBERS         16
#define EXAMPLE_CHASE_SPEED_MS      10

static const char *TAG = "MK39";

static uint8_t led_strip_pixels[EXAMPLE_LED_NUMBERS * 3];

uint32_t red = 0;
uint32_t green = 0;
uint32_t blue = 0;
uint16_t hue = 0;
uint16_t start_rgb = 0;

rmt_channel_handle_t led_chan = NULL;
rmt_tx_channel_config_t tx_chan_config;
rmt_encoder_handle_t led_encoder = NULL;
led_strip_encoder_config_t encoder_config;
rmt_transmit_config_t tx_config;

//
int detect_flag = 0;
static esp_afe_sr_iface_t *afe_handle = NULL;
static volatile int task_flag = 0;
srmodel_list_t *models = NULL;
static int play_voice = -2;

void led_set(){
    tx_chan_config.clk_src = RMT_CLK_SRC_DEFAULT; // select source clock
    tx_chan_config.gpio_num = RMT_LED_STRIP_GPIO_NUM;
    tx_chan_config.mem_block_symbols = 256; // increase the block size can make the LED less flickering
    tx_chan_config.resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ;
    tx_chan_config.trans_queue_depth = 4; // set the number of transactions that can be pending in the background
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

    encoder_config.resolution = RMT_LED_STRIP_RESOLUTION_HZ;
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));

    ESP_ERROR_CHECK(rmt_enable(led_chan));

    tx_config.loop_count = 0; // no transfer loop
}

void led_reset(){
        //use memset instead of loop for setting pixels
        memset(led_strip_pixels, 0, sizeof(led_strip_pixels)); 
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
        vTaskDelay(pdMS_TO_TICKS(EXAMPLE_CHASE_SPEED_MS));
        memset(led_strip_pixels, 0, sizeof(led_strip_pixels));     
}

void led_color(uint8_t g, uint8_t r, uint8_t b){
    //RGB Color
    led_reset();
    for (int j = 0; j < 48; j += 1) {
        led_strip_pixels[j] = g;
        led_strip_pixels[j+1] = r;
        led_strip_pixels[j+2] = b;
        j+=2;
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
        vTaskDelay(pdMS_TO_TICKS(EXAMPLE_CHASE_SPEED_MS));
    }
}

void led_process(void *arg){
    led_reset();

    for (int i = 0; i < 5; i++) {
        for (int j = 1; j < 48; j += 3) {
            // Build RGB pixels
            led_strip_pixels[j] = 255;
            // Flush RGB values to LEDs
            ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
            ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
            vTaskDelay(pdMS_TO_TICKS(EXAMPLE_CHASE_SPEED_MS));
            memset(led_strip_pixels, 0, sizeof(led_strip_pixels));     
        }
        gpio_reset_pin(38);
        gpio_set_direction(38, GPIO_MODE_OUTPUT);
        gpio_set_level(38, (i%2));
    }
    led_color(0, 100 , 0);
    vTaskDelete(NULL);
}

void feed_Task(void *arg)
{
    esp_afe_sr_data_t *afe_data = arg;
    int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
    int nch = afe_handle->get_channel_num(afe_data);
    int feed_channel = esp_get_feed_channel();
    assert(nch <= feed_channel);
    int16_t *i2s_buff = malloc(audio_chunksize * sizeof(int16_t) * feed_channel);
    assert(i2s_buff);

    while (task_flag) {
        esp_get_feed_data(false, i2s_buff, audio_chunksize * sizeof(int16_t) * feed_channel);

        afe_handle->feed(afe_data, i2s_buff);
    }
    if (i2s_buff) {
        free(i2s_buff);
        i2s_buff = NULL;
    }
    vTaskDelete(NULL);
}

void detect_Task(void *arg)
{
    esp_afe_sr_data_t *afe_data = arg;
    int afe_chunksize = afe_handle->get_fetch_chunksize(afe_data);
    char *mn_name = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_ENGLISH);
    printf("multinet:%s\n", mn_name);
    esp_mn_iface_t *multinet = esp_mn_handle_from_name(mn_name);
    model_iface_data_t *model_data = multinet->create(mn_name, 6000);
    int mu_chunksize = multinet->get_samp_chunksize(model_data);
    esp_mn_commands_update_from_sdkconfig(multinet, model_data); // Add speech commands from sdkconfig
    assert(mu_chunksize == afe_chunksize);
    //print active speech commands
    multinet->print_active_speech_commands(model_data);

    printf("------------detect start------------\n");
    while (task_flag) {
        afe_fetch_result_t* res = afe_handle->fetch(afe_data); 
        if (!res || res->ret_value == ESP_FAIL) {
            printf("fetch error!\n");
            break;
        }

        if (res->wakeup_state == WAKENET_DETECTED) {
            printf("WAKEWORD DETECTED\n");
            xTaskCreatePinnedToCore(&led_process, "led", 8 * 1024, NULL, 5, NULL, 1);
	    multinet->clean(model_data);
        } else if (res->wakeup_state == WAKENET_CHANNEL_VERIFIED) {
            play_voice = -1;
            detect_flag = 1;
            printf("AFE_FETCH_CHANNEL_VERIFIED, channel index: %d\n", res->trigger_channel_id);
            // afe_handle->disable_wakenet(afe_data);
            // afe_handle->disable_aec(afe_data);
        }

        if (detect_flag == 1) {
            esp_mn_state_t mn_state = multinet->detect(model_data, res->data);

            if (mn_state == ESP_MN_STATE_DETECTING) {
                continue;
            }

            if (mn_state == ESP_MN_STATE_DETECTED) {
                esp_mn_results_t *mn_result = multinet->get_results(model_data);
                for (int i = 0; i < mn_result->num; i++) {
                    printf("TOP %d, command_id: %d, phrase_id: %d, string: %s, prob: %f\n", 
                    i+1, mn_result->command_id[i], mn_result->phrase_id[i], mn_result->string, mn_result->prob[i]);
                }
                if(mn_result->num >0){
                    printf("processing\n");
                    switch (mn_result->command_id[0])
                    {
                        case 0:
                            //open helmet
                            helmet_open();
                            detect_flag = 2;
                            printf("Open Sequence\n");
                            vTaskDelay(70);
                            gpio_reset_pin(38);
                            gpio_set_direction(38, GPIO_MODE_OUTPUT);
                            gpio_set_level(38, 0);
                            led_color(50, 50, 0);
                            // servo_reset();
                            // esp_restart();
                            break;
                        
                        case 1:
                            //hulk out
                            led_color(100, 0, 0);
                            break;

                        case 2:
                            //close helmet
                            helmet_close();
                            detect_flag = 2;
                            printf("Close Sequence\n");
                            vTaskDelay(50);
                            gpio_reset_pin(38);
                            gpio_set_direction(38, GPIO_MODE_OUTPUT);
                            gpio_set_level(38, 1);
                            led_color(0, 0, 100);
                            // servo_reset();
                            break;

                        case 3:
                            //
                            break;

                        default:
                            break;
                    }
                }
                printf("-----------listening-----------\n");

                
            }

            if (mn_state == ESP_MN_STATE_TIMEOUT) {
                esp_mn_results_t *mn_result = multinet->get_results(model_data);
                printf("timeout, string:%s\n", mn_result->string);
                afe_handle->enable_wakenet(afe_data);
                detect_flag = 0;
                printf("\n-----------awaits to be waken up-----------\n");
                continue;
            }
        }
    }
    if (model_data) {
        multinet->destroy(model_data);
        model_data = NULL;
    }
    printf("detect exit\n");
    vTaskDelete(NULL);
}

void app_main()
{
    led_set();
    servo_init();

    models = esp_srmodel_init("model"); // partition label defined in partitions.csv
    ESP_ERROR_CHECK(esp_board_init(AUDIO_HAL_16K_SAMPLES, 1, 16));
    // ESP_ERROR_CHECK(esp_sdcard_init("/sdcard", 10));
#if defined CONFIG_ESP32_KORVO_V1_1_BOARD
    led_init();
#endif

#if CONFIG_IDF_TARGET_ESP32
    printf("This demo only support ESP32S3\n");
    return;
#else 
    afe_handle = (esp_afe_sr_iface_t *)&ESP_AFE_SR_HANDLE;
#endif

    afe_config_t afe_config = AFE_CONFIG_DEFAULT();
    afe_config.wakenet_model_name = esp_srmodel_filter(models, ESP_WN_PREFIX, NULL);;
#if defined CONFIG_ESP32_S3_BOX_BOARD || defined CONFIG_ESP32_S3_EYE_BOARD || CONFIG_ESP32_S3_DEVKIT_C
    afe_config.aec_init = false;
    #if defined CONFIG_ESP32_S3_EYE_BOARD || CONFIG_ESP32_S3_DEVKIT_C
        afe_config.pcm_config.total_ch_num = 2;
        afe_config.pcm_config.mic_num = 1;
        afe_config.pcm_config.ref_num = 1;
    #endif
#endif
    esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(&afe_config);

    task_flag = 1;
    xTaskCreatePinnedToCore(&detect_Task, "detect", 8 * 1024, (void*)afe_data, 5, NULL, 1);
    xTaskCreatePinnedToCore(&feed_Task, "feed", 8 * 1024, (void*)afe_data, 5, NULL, 0);

    // // You can call afe_handle->destroy to destroy AFE.
    // task_flag = 0;

    // printf("destroy\n");
    // afe_handle->destroy(afe_data);
    // afe_data = NULL;
    // printf("successful\n");
}
