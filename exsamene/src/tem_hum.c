#include "tem_hum.h"

#include <stdbool.h>

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/printk.h>

// ================================
// Global sensor variables
// ================================

static const struct device *sensor;

static double temperature = 0.0;
static double humidity = 0.0;

// ================================
// Initialize sensor
// ================================

void tem_hum_init(void)
{
	sensor = DEVICE_DT_GET_ANY(st_hts221);

	if (sensor == NULL) {
		printk("HTS221 sensor not found\n");
		return;
	}

	if (!device_is_ready(sensor)) {
		printk("HTS221 sensor not ready\n");
		return;
	}

	printk("HTS221 sensor ready\n");
}

// ================================
// Read temperature and humidity
// ================================

void tem_hum_update(void)
{
	struct sensor_value temp;
	struct sensor_value hum;

	if (!device_is_ready(sensor)) {
		printk("Sensor not ready\n");
		return;
	}

	sensor_sample_fetch(sensor);

	sensor_channel_get(sensor, SENSOR_CHAN_AMBIENT_TEMP, &temp);
	sensor_channel_get(sensor, SENSOR_CHAN_HUMIDITY, &hum);

	temperature = sensor_value_to_double(&temp);
	humidity = sensor_value_to_double(&hum);

	printk("Temp: %d.%d C | Humidity: %d.%d %%\n",
	    (int)temperature,
	    (int)(temperature * 10) % 10,
	    (int)humidity,
	    (int)(humidity * 10) % 10);
}

// ================================
// Get latest temperature
// ================================

double tem_hum_get_temp(void)
{
	return temperature;
}

// ================================
// Get latest humidity
// ================================

double tem_hum_get_hum(void)
{
	return humidity;
}