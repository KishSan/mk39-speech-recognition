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

#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_board_init.h"
#include "esp_log.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_process_sdkconfig.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"

#include "model_path.h"

#include "driver/gpio.h"

#include <string.h>

// specific includes for iron man suit control
#include "led_im.h"
#include "servo_im.h"

static const char *TAG = "MK39 Master Control";

//
int detect_flag = 0;
static esp_afe_sr_iface_t *afe_handle = NULL;
static volatile int task_flag = 0;
srmodel_list_t *models = NULL;
static int play_voice = -2;

void feed_Task(void *arg)
{
    afe_task_into_t *afe_task_info = (afe_task_into_t *)arg;
    esp_afe_sr_iface_t *afe_handle = afe_task_info->afe_handle;
    esp_afe_sr_data_t *afe_data = afe_task_info->afe_data;

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

void fetch_Task(void *arg)
{
    afe_task_into_t *afe_task_info = (afe_task_into_t *)arg;
    esp_afe_sr_iface_t *afe_handle = afe_task_info->afe_handle;
    esp_afe_sr_data_t *afe_data = afe_task_info->afe_data;

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
                            break;

                        default:
                            break;
                    }
                }
                printf("-----------Awaiting Order-----------\n");

                
            }

            if (mn_state == ESP_MN_STATE_TIMEOUT) {
                esp_mn_results_t *mn_result = multinet->get_results(model_data);
                printf("timeout, string:%s\n", mn_result->string);
                afe_handle->enable_wakenet(afe_data);
                detect_flag = 0;
                printf("\n-----------Awaiting Order-----------\n");
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
    // M - Microphone channel
    // R - Playback reference channel
    // N - Unused or unknown channel
    afe_config_t *afe_config = afe_config_init("MMNR", models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
    afe_config_print(afe_config); // print all configurations
    esp_afe_sr_iface_t *afe_handle = esp_afe_handle_from_config(afe_config);
#endif

    afe_config->wakenet_model_name = esp_srmodel_filter(models, ESP_WN_PREFIX, NULL);;
#if defined CONFIG_ESP32_S3_BOX_BOARD || defined CONFIG_ESP32_S3_EYE_BOARD || CONFIG_ESP32_S3_DEVKIT_C
    afe_config->aec_init = false;
    #if defined CONFIG_ESP32_S3_EYE_BOARD || CONFIG_ESP32_S3_DEVKIT_C
        afe_config->pcm_config.total_ch_num = 2;
        afe_config->pcm_config.mic_num = 1;
        afe_config->pcm_config.ref_num = 1;
    #endif
#endif
    esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(afe_config);
    afe_task_into_t task_info;
    task_info.afe_data = afe_data;
    task_info.afe_handle = afe_handle;
    task_info.feed_task = NULL;
    task_info.fetch_task = NULL;
    task_flag = 1;
    xTaskCreatePinnedToCore(&fetch_Task, "fetch", 8 * 1024, (void*)&task_info, 5, NULL, 1);
    xTaskCreatePinnedToCore(&feed_Task, "feed", 8 * 1024, (void*)&task_info, 5, NULL, 0);

    // // You can call afe_handle->destroy to destroy AFE.
    // task_flag = 0;

    // printf("destroy\n");
    // afe_handle->destroy(afe_data);
    // afe_data = NULL;
    // printf("successful\n");
}
