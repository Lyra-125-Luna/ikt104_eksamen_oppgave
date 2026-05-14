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

//filer
#include "rgb_lcd.h"
#include "screen.h"
#include "tem_hum.h"
#include "network_config.h"


// defines
#define STACKSIZE        2048
#define PRIORITY_HIGH    3
#define PRIORITY         2
#define PRIORITY_min     1
#define STACKSIZE_NETWORK 12288


//trå
K_THREAD_STACK_DEFINE(stack_1, STACKSIZE);
K_THREAD_STACK_DEFINE(stack_2, STACKSIZE);
K_THREAD_STACK_DEFINE(stack_3, STACKSIZE);

// nettworking:
#define STACKSIZE_NETWORK 4096
#define PRIORITY 2

#define START_FIRST_REQEST 0
#define START_SECOND_REQEST 0

// WiFi
K_THREAD_DEFINE(t_wifi, STACKSIZE_NETWORK,
    wifi_connect, NULL, NULL, NULL, 1, 0, 0);

// first reqest:
K_THREAD_DEFINE(t_info_web, STACKSIZE_NETWORK, network_config_thread_entry, NULL, NULL, NULL, PRIORITY, 0, START_FIRST_REQEST);

// secend reqest:
K_THREAD_DEFINE(t_news_web, STACKSIZE_NETWORK, network_config_thread_entry_news, NULL, NULL, NULL, PRIORITY, 0, START_SECOND_REQEST);

K_THREAD_STACK_DEFINE(alarm_stack, STACKSIZE);
struct k_thread alarm_thread;



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

//structs
	//trå
struct k_thread t1, t2, t3;

	//mutexes:
typedef struct
{
	struct k_mutex writing_mutex;
	struct k_sem reading_mutex;
}mutex;

mutex m;

int main(void)
{

	//boot
	// k_thread_create(&t1, stack_1,STACKSIZE, boot, &m, NULL, NULL, PRIORITY, 0, K_NO_WAIT);

	// Alarm
	k_thread_create(&alarm_thread, alarm_stack, STACKSIZE,
					alarm_thread_entry, NULL, NULL, NULL,
					PRIORITY, 0, K_NO_WAIT);


	// Temperature / humidity
	k_thread_create(&t2, stack_2, STACKSIZE,
					temp_hum_thread_entry, NULL, NULL, NULL,
					PRIORITY, 0, K_NO_WAIT);
	//k_thread_create(&t2, stack_2,STACKSIZE, temp, &m, NULL, NULL, PRIORITY, 0, K_NO_WAIT);
	//k_thread_create(&t3, stack_2,STACKSIZE, hum, &m, NULL, NULL, PRIORITY, 0, K_NO_WAIT);


	while (1)
	{
		k_sleep(K_SECONDS(10));
	}

};


