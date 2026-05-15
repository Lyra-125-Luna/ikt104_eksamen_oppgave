#include "rgb_lcd.h"
#include "screen.h"
#include "alarm.h"
#include "tem_hum.h"
#include "network_config.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include <stdio.h>

static rgb_lcd_1602_t lcd;

// ================================
// LCD helper: write string
// ================================
static void lcd_write_string(const char *text)
{
    if (text == NULL) return;
    k_usleep(500);
    for (int i = 0; i < (int)strlen(text) && i < 16; i++) {
        rgb_lcd_1602_write_char(&lcd, text[i]);
        k_msleep(2);
    }
}

// ================================
// LCD helper: write two lines
// ================================
static void lcd_show(const char *line1, const char *line2)
{
    rgb_lcd_1602_clear(&lcd);
    k_msleep(5);
    rgb_lcd_1602_set_cursor(&lcd, 0, 0);
    lcd_write_string(line1);
    rgb_lcd_1602_set_cursor(&lcd, 0, 1);
    lcd_write_string(line2);
}

// ================================
// Boot screen
// ================================
void boot(void)
{
    const struct device *i2c1 = DEVICE_DT_GET(DT_NODELABEL(i2c1));

    if (!device_is_ready(i2c1)) {
        printk("I2C device not ready\n");
        return;
    }

    if (rgb_lcd_1602_init(&lcd, i2c1, 0x2D) != 0) {
        printk("rgb_lcd1602_init() failed\n");
        return;
    }

    rgb_lcd_1602_set_rgb(&lcd, 255, 0, 255);
    rgb_lcd_1602_clear(&lcd);
    k_msleep(10);

    rgb_lcd_1602_set_cursor(&lcd, 0, 0);
    lcd_write_string("System Loading");
    rgb_lcd_1602_set_cursor(&lcd, 0, 1);
    lcd_write_string("Please wait...");

    k_sleep(K_SECONDS(2));
}

// ================================
// Screen: alarm (clock + status)
// ================================
void screen_show_alarm(void)
{
    char line1[17] = {0};
    char line2[17] = {0};
    alarm_get_clock_text(line1, sizeof(line1));
    alarm_get_status_text(line2, sizeof(line2));
    lcd_show(line1, line2);
}

// ================================
// Screen: temperature and humidity
// ================================
void screen_show_temp_hum(void)
{
    char line1[17] = {0};
    char line2[17] = {0};

    float temp = (float)tem_hum_get_temp();
    float hum  = (float)tem_hum_get_hum();

    int temp_int = (int)temp;
    int temp_dec = (int)(temp * 10.0f) % 10;
    if (temp_dec < 0) temp_dec = -temp_dec;

    snprintf(line1, sizeof(line1), "Temp:  %d.%d C", temp_int, temp_dec);
    snprintf(line2, sizeof(line2), "Humid: %d%%", (int)hum);

    lcd_show(line1, line2);
}

// ================================
// Screen: weather
// ================================
static void screen_show_weather(void)
{
    char weather[INFO_STR_LEN] = {0};
    char temp[INFO_STR_LEN]    = {0};

    k_mutex_lock(&info_mutex, K_FOREVER);
    strncpy(weather, g_info.weather,     sizeof(weather) - 1);
    strncpy(temp,    g_info.temperature, sizeof(temp) - 1);
    k_mutex_unlock(&info_mutex);

    if (weather[0] == '\0') {
        lcd_show("Weather:", "No data yet");
        return;
    }

    char line2[17] = {0};
    snprintf(line2, sizeof(line2), "%.13s C", temp);
    lcd_show(weather, line2);
}

// ================================
// Screen: Unix epoch
// ================================
static void screen_show_epoch(void)
{
    char epoch[INFO_STR_LEN] = {0};

    k_mutex_lock(&info_mutex, K_FOREVER);
    strncpy(epoch, g_info.unix_epoch, sizeof(epoch) - 1);
    k_mutex_unlock(&info_mutex);

    if (epoch[0] == '\0') {
        lcd_show("Unix epoch:", "No data yet");
        return;
    }

    lcd_show("Unix epoch:", epoch);
}

// ================================
// Screen: latitude and longitude
// ================================
static void screen_show_latlon(void)
{
    char lat[INFO_STR_LEN] = {0};
    char lon[INFO_STR_LEN] = {0};

    k_mutex_lock(&info_mutex, K_FOREVER);
    strncpy(lat, g_info.latitude,  sizeof(lat) - 1);
    strncpy(lon, g_info.longitude, sizeof(lon) - 1);
    k_mutex_unlock(&info_mutex);

    if (lat[0] == '\0') {
        lcd_show("Lat/Lon:", "No data yet");
        return;
    }

    char line1[17] = {0};
    char line2[17] = {0};
    snprintf(line1, sizeof(line1), "Lat: %.11s", lat);
    snprintf(line2, sizeof(line2), "Lon: %.11s", lon);
    lcd_show(line1, line2);
}

// ================================
// Screen: city
// ================================
static void screen_show_city(void)
{
    char city[INFO_STR_LEN] = {0};

    k_mutex_lock(&info_mutex, K_FOREVER);
    strncpy(city, g_info.city, sizeof(city) - 1);
    k_mutex_unlock(&info_mutex);

    if (city[0] == '\0') {
        lcd_show("City:", "No data yet");
        return;
    }

    lcd_show("City:", city);
}

// ================================
// Unix epoch → calendar
// ================================

static const char *weekday_name(int wd)
{
    static const char *days[] = {
        "Sunday", "Monday", "Tuesday",
        "Wednesday", "Thursday", "Friday", "Saturday"
    };
    if (wd < 0 || wd > 6) return "???";
    return days[wd];
}

static const char *month_name(int m)
{
    static const char *months[] = {
        "", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    if (m < 1 || m > 12) return "???";
    return months[m];
}

static void epoch_to_datetime(long epoch,
                               int *year, int *month, int *day,
                               int *hour, int *min,  int *sec,
                               int *wday)
{
    if (epoch < 0) epoch = 0;

    *sec  = (int)(epoch % 60);
    *min  = (int)((epoch / 60) % 60);
    *hour = (int)((epoch / 3600) % 24);

    long days_total = epoch / 86400L;
    *wday = (int)((days_total + 4) % 7);   /* epoch day 0 = Thu = 4, shift to Sun=0 */

    long d = days_total;
    *year  = 1970;
    while (*year < 2100) {
        int leap = ((*year % 4 == 0 && *year % 100 != 0) ||
                    (*year % 400 == 0));
        int days_in_year = leap ? 366 : 365;
        if (d < (long)days_in_year) break;
        d -= days_in_year;
        (*year)++;
    }

    static const int mdays[13] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    int leap = ((*year % 4 == 0 && *year % 100 != 0) ||
                (*year % 400 == 0));
    for (*month = 1; *month <= 12; (*month)++) {
        int md = mdays[*month] + (*month == 2 && leap ? 1 : 0);
        if (d < (long)md) break;
        d -= md;
    }
    if (*month > 12) *month = 12;
    *day = (int)d + 1;
}

// ================================
// Screen: date + time
// ================================
static void screen_show_datetime(void)
{
    char epoch_str[INFO_STR_LEN] = {0};

    k_mutex_lock(&info_mutex, K_FOREVER);
    strncpy(epoch_str, g_info.unix_epoch, sizeof(epoch_str) - 1);
    k_mutex_unlock(&info_mutex);

    if (epoch_str[0] == '\0') {
        char line1[17] = {0};
        alarm_get_clock_text(line1, sizeof(line1));
        lcd_show(line1, "No date yet");
        return;
    }

    long epoch = 0;
    for (int i = 0; epoch_str[i] >= '0' && epoch_str[i] <= '9'; i++) {
        epoch = epoch * 10 + (long)(epoch_str[i] - '0');
    }

    int year, month, day, hour, min, sec, wday;
    epoch_to_datetime(epoch, &year, &month, &day, &hour, &min, &sec, &wday);

    char line1[17] = {0};
    char line2[17] = {0};
    snprintf(line1, sizeof(line1), "%.3s %d %s",
             weekday_name(wday), day, month_name(month));
    snprintf(line2, sizeof(line2), "%02d:%02d:%02d", hour, min, sec);

    lcd_show(line1, line2);
}

// ================================
// Screen: news titles — reads from g_news
// ================================
static void screen_show_news(void)
{
    /* Snapshot count and titles under the mutex */
    int   count = 0;
    char  titles[NEWS_MAX][NEWS_TITLE_LEN];

    k_mutex_lock(&news_mutex, K_FOREVER);
    if (g_news.ready) {
        count = g_news.count;
        for (int i = 0; i < count; i++) {
            strncpy(titles[i], g_news.titles[i], NEWS_TITLE_LEN - 1);
            titles[i][NEWS_TITLE_LEN - 1] = '\0';
        }
    }
    k_mutex_unlock(&news_mutex);

    if (count == 0) {
        lcd_show("News:", "No data yet");
        k_sleep(K_SECONDS(2));
        return;
    }

    for (int i = 0; i < count; i++) {
        char line1[17] = {0};
        char line2[17] = {0};

        snprintf(line1, sizeof(line1), "News %d/%d", i + 1, count);

        /* First 16 chars of title */
        snprintf(line2, sizeof(line2), "%.16s", titles[i]);
        lcd_show(line1, line2);
        k_sleep(K_SECONDS(3));

        /* Second slide if title longer than 16 chars */
        if ((int)strlen(titles[i]) > 16) {
            snprintf(line2, sizeof(line2), "%.16s", titles[i] + 16);
            lcd_show(line1, line2);
            k_sleep(K_SECONDS(2));
        }
    }
}

// ================================
// Main Thread Entry
// ================================
void screen_thread_entry(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    boot();

    while (1) {
        /* a) Alarm: clock + status (~3 s) */
        screen_show_alarm();
        k_sleep(K_SECONDS(3));

        /* b) Temperature + humidity (~3 s) */
        screen_show_temp_hum();
        k_sleep(K_SECONDS(3));

        /* c) News titles (~3 s each) */
        screen_show_news();

        /* d) Weather (~3 s) */
        screen_show_weather();
        k_sleep(K_SECONDS(3));

        /* e) Unix epoch (~2 s) */
        screen_show_epoch();
        k_sleep(K_SECONDS(2));

        /* f) Lat / Lon (~2 s) */
        screen_show_latlon();
        k_sleep(K_SECONDS(2));

        /* g) City (~2 s) */
        screen_show_city();
        k_sleep(K_SECONDS(2));

        /* h) Date + time (~3 s) */
        screen_show_datetime();
        k_sleep(K_SECONDS(3));
    }
}