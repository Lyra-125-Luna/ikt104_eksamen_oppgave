#include "rgb_lcd.h"
#include "screen.h"

#include <zephyr/kernel.h>

#include <zephyr/drivers/rtc.h>

const struct device *i2c1 = DEVICE_DT_GET(DT_NODELABEL(i2c1));

static rgb_lcd_1602_t lcd;

void boot()
{

	// Use address 0x60 for V1.0 LCDs, 0x2D for V2.0 LCDs
	if (rgb_lcd_1602_init(&lcd, i2c1, 0x2D) != 0)
	{
		printk("rgb_lcd1602_init() failed\n");
	}

	printk("rgb_lcd1602_init() succeeded\n");

	printk("Setting RGB\n");
	rgb_lcd_1602_set_rgb(&lcd, 255, 0, 255);

	printk("Setting cursor\n");
	rgb_lcd_1602_set_cursor(&lcd, 0, 0);

	const char lowding_1[] = "setting up boot";

	printk("Writing text\n");
	for (int i = 0; i < strlen(lowding_1); i++)
	{
		rgb_lcd_1602_write_char(&lcd, lowding_1[i]);
	}
	printk("Setting curosor\n");
	rgb_lcd_1602_set_cursor(&lcd, 0, 1);

	char const lowding_2[] = "lowding . . .";

	for (int i = 0; i < strlen(lowding_2); i++)
	{
		rgb_lcd_1602_write_char(&lcd, lowding_2[i]);
	}

};
