#include "zephyr/posix/sys/stat.h"
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <stdio.h>
#include <zephyr/sys/__assert.h>
#include "alarm.h"
#include "rgb_lcd.h"
#include "screen.h"
#include "tem_hum.h"
#include "network_config.h"

// ================================
// Stack sizes and priorities
// ================================
#define STACKSIZE           1024
#define STACKSIZE_NETWORK   3072   /* news thread parses a 5 KB buffer — give it room */
#define STACKSIZE_SCREEN    2048
#define STACKSIZE_ALARM     2048
#define PRIORITY_HIGH       3
#define PRIORITY            2
#define PRIORITY_min        1
#define START_FIRST_REQEST  0
#define START_SECOND_REQEST 0

// ================================
// Thread stacks
// ================================
K_THREAD_STACK_DEFINE(stack_1,     STACKSIZE_SCREEN);
K_THREAD_STACK_DEFINE(stack_2,     STACKSIZE);
K_THREAD_STACK_DEFINE(alarm_stack, STACKSIZE_ALARM);

// ================================
// Thread structs
// ================================
struct k_thread t1, t2;
struct k_thread alarm_thread;

// ================================
// WiFi + network threads
// ================================

K_THREAD_DEFINE(t_wifi,     STACKSIZE_NETWORK, wifi_connect,
                NULL, NULL, NULL, 1,        0, 0);
K_THREAD_DEFINE(t_info_web, STACKSIZE_NETWORK, network_config_thread_entry,
                NULL, NULL, NULL, PRIORITY, 0, START_FIRST_REQEST);
K_THREAD_DEFINE(t_news_web, STACKSIZE_NETWORK, network_config_thread_entry_news,
                NULL, NULL, NULL, PRIORITY, 0, START_SECOND_REQEST);

// ================================
// Temperature / humidity thread
// ================================
void temp_hum_thread_entry(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);
    tem_hum_init();
    while (1) {
        tem_hum_update();
        k_sleep(K_SECONDS(2));
    }
}

// ================================
// Main
// ================================
int main(void)
{
    /* Screen thread — boot splash + info loop */
    k_thread_create(&t1, stack_1, STACKSIZE_SCREEN,
                    screen_thread_entry, NULL, NULL, NULL,
                    PRIORITY, 0, K_NO_WAIT);

    /* Alarm thread (spawns its own speaker sub-thread internally) */
    k_thread_create(&alarm_thread, alarm_stack, STACKSIZE_ALARM,
                    alarm_thread_entry, NULL, NULL, NULL,
                    PRIORITY_HIGH, 0, K_NO_WAIT);

    /* Temperature / humidity thread */
    k_thread_create(&t2, stack_2, STACKSIZE,
                    temp_hum_thread_entry, NULL, NULL, NULL,
                    PRIORITY, 0, K_NO_WAIT);

    return 0;
}






















