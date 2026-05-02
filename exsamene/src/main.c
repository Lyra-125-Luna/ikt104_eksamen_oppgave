#include "zephyr/posix/sys/stat.h"

#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <stdio.h>
#include <zephyr/sys/__assert.h>

//filer
#include "rgb_lcd.h"
#include "screen.h"
#include "tem_hum.h"
#include "network_config.h"


// defines
#define STACKSIZE 1024
#define PRIORITY_HIGH 3
#define PRIORITY 2
#define PRIORITY_min 1

//trå
K_THREAD_STACK_DEFINE(stack_1, STACKSIZE);
K_THREAD_STACK_DEFINE(stack_2, STACKSIZE);
K_THREAD_STACK_DEFINE(stack_3, STACKSIZE);
K_THREAD_STACK_DEFINE(stack_4, STACKSIZE);


//structs
	//trå
struct k_thread t1, t2, t3, t4;

	//mutexes:
typedef struct
{
	struct k_mutex writing_mutex;
	struct k_sem reading_mutex;
}mutex;

mutex m;

void funks()
{

}

int main(void)
{

	//boot
	k_thread_create(&t1, stack_1,STACKSIZE, boot, &m, NULL, NULL,
		PRIORITY, 0, K_NO_WAIT);

	//Temp -> hum
	k_thread_create(&t2, stack_2,STACKSIZE, temp, &m, NULL, NULL, PRIORITY, 0, K_NO_WAIT);
	k_thread_create(&t3, stack_2,STACKSIZE, hum, &m, NULL, NULL, PRIORITY, 0, K_NO_WAIT);

	// network
	k_thread_create(&t4, stack_2,STACKSIZE, network_config, &m, NULL, NULL, PRIORITY, 0, K_NO_WAIT);

	while (1)
	{
		k_sleep(K_SECONDS(10));
	}

};


