/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"
#include "servoim.h"
#include "esp_system.h"

static const char *TAG = "MK39";

// Please consult the datasheet of your servo before changing the following parameters
#define SERVO_MIN_PULSEWIDTH_US 500  // Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH_US 2500  // Maximum pulse width in microsecond
#define SERVO_MIN_DEGREE        -90   // Minimum angle
#define SERVO_MAX_DEGREE        90    // Maximum angle

#define SERVO1_PULSE_GPIO             4        // GPIO connects to the PWM signal line
#define SERVO2_PULSE_GPIO             5        // GPIO connects to the PWM signal line
#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000  // 1MHz, 1us per tick
#define SERVO_TIMEBASE_PERIOD        20000    // 20000 ticks, 20ms

static inline uint32_t example_angle_to_compare(int angle)
{
    return (angle - SERVO_MIN_DEGREE) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) / (SERVO_MAX_DEGREE - SERVO_MIN_DEGREE) + SERVO_MIN_PULSEWIDTH_US;
}

mcpwm_timer_handle_t timer;
mcpwm_timer_handle_t timer2;
mcpwm_oper_handle_t oper;
mcpwm_oper_handle_t oper2;
mcpwm_cmpr_handle_t comparator;
mcpwm_cmpr_handle_t comparator2;
mcpwm_gen_handle_t generator;
mcpwm_gen_handle_t generator2;

mcpwm_timer_config_t timer_config;
mcpwm_timer_config_t timer2_config;

servo_init(void){
    timer = NULL;
    timer2 = NULL;
    oper = NULL;
    oper2 = NULL;
    comparator = NULL;
    comparator2 = NULL;
    generator = NULL;
    generator2 = NULL;

    ESP_LOGI(TAG, "Create timer and operator");
    // mcpwm_timer_handle_t timer = NULL;
    // timer_config = {
        timer_config.group_id = 0;
        timer_config.clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT;
        timer_config.resolution_hz = SERVO_TIMEBASE_RESOLUTION_HZ;
        timer_config.period_ticks = SERVO_TIMEBASE_PERIOD;
        timer_config.count_mode = MCPWM_TIMER_COUNT_MODE_UP;
    // };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    // mcpwm_timer_handle_t timer2 = NULL;
        timer2_config.group_id = 1;
        timer2_config.clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT;
        timer2_config.resolution_hz = SERVO_TIMEBASE_RESOLUTION_HZ;
        timer2_config.period_ticks = SERVO_TIMEBASE_PERIOD;
        timer2_config.count_mode = MCPWM_TIMER_COUNT_MODE_UP;
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer2_config, &timer2));

    ///////////////////////////////////////
    // mcpwm_oper_handle_t oper = NULL;
    mcpwm_operator_config_t operator_config = {
        .group_id = 0, // operator must be in the same group to the timer
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper));

    ESP_LOGI(TAG, "Connect timer and operator");
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, timer));

    // mcpwm_oper_handle_t oper2 = NULL;
    mcpwm_operator_config_t operator2_config = {
        .group_id = 1, // operator must be in the same group to the timer
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator2_config, &oper2));

    ESP_LOGI(TAG, "Connect timer and operator");
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper2, timer2));

    ///////////////////////////////////////
    ESP_LOGI(TAG, "Create comparator and generator from the operator");
    // mcpwm_cmpr_handle_t comparator = NULL;
    mcpwm_comparator_config_t comparator_config = {
        .flags.update_cmp_on_tez = true,
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &comparator_config, &comparator));

    ESP_LOGI(TAG, "Create comparator and generator from the operator");
    // mcpwm_cmpr_handle_t comparator2 = NULL;
    mcpwm_comparator_config_t comparator2_config = {
        .flags.update_cmp_on_tez = true,
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper2, &comparator2_config, &comparator2));

    ///////////////////////////////////////
    // mcpwm_gen_handle_t generator = NULL;
    mcpwm_generator_config_t generator_config = {
        .gen_gpio_num = SERVO1_PULSE_GPIO,
    };
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &generator_config, &generator));

        // mcpwm_gen_handle_t generator2 = NULL;
    mcpwm_generator_config_t generator2_config = {
        .gen_gpio_num = SERVO2_PULSE_GPIO,
    };
    ESP_ERROR_CHECK(mcpwm_new_generator(oper2, &generator2_config, &generator2));

    ESP_LOGI(TAG, "Set generator action on timer and compare event");
    // go high on counter empty
        ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator,
                        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    // go low on compare threshold
        ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator,
                        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW)));

    // go high on counter empty
        ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator2,
                        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    // go low on compare threshold
        ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator2,
                        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator2, MCPWM_GEN_ACTION_LOW)));

                        

    ESP_LOGI(TAG, "Enable and start timer");
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));

    ESP_LOGI(TAG, "Enable and start timer");
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer2));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer2, MCPWM_TIMER_START_NO_STOP));
}

servo_reset(){
    mcpwm_timer_start_stop(timer, MCPWM_TIMER_STOP_FULL);
    mcpwm_timer_start_stop(timer2, MCPWM_TIMER_STOP_FULL);
    mcpwm_timer_disable(timer);
    mcpwm_timer_disable(timer2);

    mcpwm_del_generator(generator);
    mcpwm_del_generator(generator2);
    mcpwm_del_comparator(comparator);
    mcpwm_del_comparator(comparator2);
    mcpwm_del_operator(oper);
    mcpwm_del_operator(oper2);
    mcpwm_del_timer(timer);
    mcpwm_del_timer(timer2);
    gpio_reset_pin(SERVO1_PULSE_GPIO);
    gpio_reset_pin(SERVO2_PULSE_GPIO);
}

void servo_close(void)
{
    servo_init();

    //close
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer2, MCPWM_TIMER_START_NO_STOP));

    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, example_angle_to_compare(60)));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator2, example_angle_to_compare(-60)));
}

void servo_open(void)
{
    servo_init();

    //open
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer2, MCPWM_TIMER_START_NO_STOP));
    
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, example_angle_to_compare(-90)));
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator2, example_angle_to_compare(90)));
}