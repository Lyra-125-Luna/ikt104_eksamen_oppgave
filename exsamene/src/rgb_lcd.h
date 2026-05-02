//
// Created by luna on 4/22/26.
//

#ifndef BLINKY_RGB_LCD_H
#define BLINKY_RGB_LCD_H

#include <zephyr/device.h>
#include <stdint.h>

typedef struct
{
	uint8_t cols;
	uint8_t rows;

	uint8_t lcd_addr;
	uint8_t rgb_addr;

	uint8_t reg_red;
	uint8_t reg_green;
	uint8_t reg_blue;

	uint8_t control;

	struct device *dev;
} rgb_lcd_1602_t;


int rgb_lcd_1602_init(rgb_lcd_1602_t *lcd, const struct device *device, uint8_t rgb_addr);

int rgb_lcd_1602_clear(const rgb_lcd_1602_t *lcd);
int rgb_lcd_1602_set_cursor(const rgb_lcd_1602_t *lcd, uint8_t col, uint8_t row);
int rgb_lcd_1602_write_char(const rgb_lcd_1602_t *lcd, uint8_t data);

int rgb_lcd_1602_set_rgb(const rgb_lcd_1602_t *lcd, uint8_t r, uint8_t g, uint8_t b);

#endif //BLINKY_RGB_LCD_H
