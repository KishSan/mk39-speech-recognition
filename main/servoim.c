/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "servoim.h"
#include "esp_system.h"
#include "iot_servo.h"

static const char *TAG = "MK39";

#define SERVO1_PULSE_GPIO             4        // GPIO connects to the PWM signal line
#define SERVO2_PULSE_GPIO             5        // GPIO connects to the PWM signal line

// Configure the servos
servo_config_t servo_helmet_cfg = {
    .max_angle = 180,
    .min_width_us = 500,
    .max_width_us = 2500,
    .freq = 50,
    .timer_number = LEDC_TIMER_0,
    .channels = {
        .servo_pin = {
            SERVO1_PULSE_GPIO,
            SERVO2_PULSE_GPIO,
        },
        .ch = {
            LEDC_CHANNEL_0,
            LEDC_CHANNEL_1,
        },
    },
    .channel_number = 2,
};

void servo_init(void)
{
    ESP_LOGI(TAG, "Servo Setup");

    //Initialize the servos
    iot_servo_init(LEDC_LOW_SPEED_MODE, &servo_helmet_cfg);
}

void helmet_open(void *arg)
{
    // servos will be disabled after each motion to 
    // avoid overheating due to misalignment in printed parts
    // initialize on each action
    servo_init();

    ESP_LOGI(TAG, "Open Sequence");

    // no smoothing function will be needed for this application
    // direct servo angle write
    iot_servo_write_angle(LEDC_LOW_SPEED_MODE, 0, 0);
    // motors are mounted in opposite orientations
    // one clockwise and the other counterclockwise for the same angle
    iot_servo_write_angle(LEDC_LOW_SPEED_MODE, 1, 180);
    // delay while waiting for action to complete
    vTaskDelay(300 /  portTICK_PERIOD_MS);
    // to avoid excessive strain on motors, kill the signal
    iot_servo_deinit(LEDC_LOW_SPEED_MODE);
}

void helmet_close(void *arg)
{
    // servos will be disabled after each motion to 
    // avoid overheating due to misalignment in printed parts
    // initialize on each action
    servo_init();

    ESP_LOGI(TAG, "Close Sequence");

    // no smoothing function will be needed for this application
    // direct servo angle write
    iot_servo_write_angle(LEDC_LOW_SPEED_MODE, 0, 150);
    // motors are mounted in opposite orientations
    // one clockwise and the other counterclockwise for the same angle
    iot_servo_write_angle(LEDC_LOW_SPEED_MODE, 1, 30);
    // delay while waiting for action to complete
    vTaskDelay(300 /  portTICK_PERIOD_MS);
    // to avoid excessive strain on motors, kill the signal
    iot_servo_deinit(LEDC_LOW_SPEED_MODE);
}