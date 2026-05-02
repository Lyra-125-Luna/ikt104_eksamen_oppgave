#include "rgb_lcd.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>

// Device I2C address
#define LCD_ADDRESS        (0x7c>>1)

// Payload type
#define LCD_COMMAND        0x80
#define LCD_DATA           0x40

// Commands
#define LCD_CLEARDISPLAY   0x01
#define LCD_RETURNHOME     0x02
#define LCD_ENTRYMODESET   0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT    0x10
#define LCD_FUNCTIONSET    0x20
#define LCD_SETCGRAMADDR   0x40
#define LCD_SETDDRAMADDR   0x80

// Flags for display on/off control
#define LCD_DISPLAYON      0x04
#define LCD_DISPLAYOFF     0x00
#define LCD_CURSORON       0x02
#define LCD_CURSOROFF      0x00
#define LCD_BLINKON        0x01
#define LCD_BLINKOFF       0x00

// Flags for function set
#define LCD_8BITMODE       0x10
#define LCD_4BITMODE       0x00
#define LCD_2LINE          0x08
#define LCD_1LINE          0x00
#define LCD_5x10DOTS       0x04
#define LCD_5x8DOTS        0x00

// RGB register addresses
#define REG_MODE1          0x00
#define REG_MODE2          0x01
#define REG_OUTPUT         0x08

// Internal functions

int lcd_function_set(rgb_lcd_1602_t *lcd)
{
    const uint8_t cmd[2] = {LCD_COMMAND, LCD_FUNCTIONSET | LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS};
    return i2c_write(lcd->dev, cmd, sizeof(cmd), lcd->lcd_addr);
}

int lcd_display_control(const rgb_lcd_1602_t *lcd)
{
    const uint8_t cmd[2] = {LCD_COMMAND, LCD_DISPLAYCONTROL | lcd->control};
    return i2c_write(lcd->dev, cmd, sizeof(cmd), lcd->lcd_addr);
}

int rgb_reg_write(const rgb_lcd_1602_t *lcd, uint8_t reg, uint8_t data)
{
    const uint8_t cmd[2] = {reg, data};
    return i2c_write(lcd->dev, cmd, sizeof(cmd), lcd->rgb_addr);
}

int rgb_init(const rgb_lcd_1602_t *lcd)
{
    if (lcd->rgb_addr == (0xc0 >> 1))
    {
        rgb_reg_write(lcd, REG_MODE1, 0);
        rgb_reg_write(lcd, REG_OUTPUT, 0xFF);
        rgb_reg_write(lcd, REG_MODE2, 0x20);
    } else if (lcd->rgb_addr == (0x60 >> 1))
    {
        rgb_reg_write(lcd, 0x01, 0x00);
        rgb_reg_write(lcd, 0x02, 0xfF);
        rgb_reg_write(lcd, 0x04, 0x15);
    } else if (lcd->rgb_addr == 0x6B)
    {
        rgb_reg_write(lcd, 0x2F, 0x00);
        rgb_reg_write(lcd, 0x00, 0x20);
        rgb_reg_write(lcd, 0x01, 0x00);
        rgb_reg_write(lcd, 0x02, 0x01);
        rgb_reg_write(lcd, 0x03, 4);
    }

    // We're assuming it's fine for now...
    return 0;
}

// Public functions

int rgb_lcd_1602_init(rgb_lcd_1602_t *lcd, const struct device *device, const uint8_t rgb_addr)
{
    if (!device_is_ready(device))
        return -ENODEV;

    lcd->dev = (struct device *) device;

    lcd->cols = 16;
    lcd->rows = 2;

    lcd->control = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;

    lcd->lcd_addr = LCD_ADDRESS;
    lcd->rgb_addr = rgb_addr;

    if (rgb_addr == (0x60))
    {
        lcd->reg_red = 0x04;
        lcd->reg_green = 0x03;
        lcd->reg_blue = 0x02;
    } else if (rgb_addr == (0x60 >> 1))
    {
        lcd->reg_red = 0x06;
        lcd->reg_green = 0x07;
        lcd->reg_blue = 0x08;
    } else if (rgb_addr == (0x6B))
    {
        lcd->reg_red = 0x06;
        lcd->reg_green = 0x05;
        lcd->reg_blue = 0x04;
    } else if (rgb_addr == (0x2D))
    {
        lcd->reg_red = 0x01;
        lcd->reg_green = 0x02;
        lcd->reg_blue = 0x03;
    }

    // Wait for display to be ready on boot
    k_msleep(50);

    // Send the function set command three times (WHY???)
    for (int i = 0; i < 3; i++)
    {
        lcd_function_set(lcd);
        k_msleep(5);
    }

    lcd_display_control(lcd);
    rgb_lcd_1602_clear(lcd);
    rgb_init(lcd);

    return 0;
}

int rgb_lcd_1602_clear(const rgb_lcd_1602_t *lcd)
{
    const uint8_t cmd[2] = {LCD_COMMAND, LCD_CLEARDISPLAY};
    return i2c_write(lcd->dev, cmd, sizeof(cmd), lcd->lcd_addr);
}

int rgb_lcd_1602_set_cursor(const rgb_lcd_1602_t *lcd, uint8_t col, uint8_t row)
{
    // Row is encoded together with column to one byte
    uint8_t data = (row == 0 ? col | 0x80 : col | 0xc0);

    const uint8_t cmd[2] = {LCD_COMMAND, data};
    return i2c_write(lcd->dev, cmd, sizeof(cmd), lcd->lcd_addr);
}

int rgb_lcd_1602_write_char(const rgb_lcd_1602_t *lcd, uint8_t data)
{
    const uint8_t buf[2] = {LCD_DATA, data};
    return i2c_write(lcd->dev, buf, sizeof(buf), lcd->lcd_addr);
}

int rgb_lcd_1602_set_rgb(const rgb_lcd_1602_t *lcd, uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t buf[2];
    int ret;

    buf[0] = lcd->reg_red;
    buf[1] = r;
    ret = i2c_write(lcd->dev, buf, 2, lcd->rgb_addr);
    if (ret) return ret;

    buf[0] = lcd->reg_green;
    buf[1] = g;
    ret = i2c_write(lcd->dev, buf, 2, lcd->rgb_addr);
    if (ret) return ret;

    buf[0] = lcd->reg_blue;
    buf[1] = b;
    return i2c_write(lcd->dev, buf, 2, lcd->rgb_addr);
}
