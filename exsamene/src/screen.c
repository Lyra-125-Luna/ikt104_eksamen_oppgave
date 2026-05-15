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

/* Timeout for all info/news mutex locks — never block forever from the
   screen thread, so a stuck network thread cannot freeze the display. */
#define MUTEX_TIMEOUT_MS  200

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

/* --------------------------------
 * Safe info snapshot helper.
 * Tries to lock info_mutex with a timeout; on failure returns false
 * and the caller shows "No data yet" without hanging.
 * -------------------------------- */
static bool get_info_snapshot(info_data_t *out)
{
    if (k_mutex_lock(&info_mutex, K_MSEC(MUTEX_TIMEOUT_MS)) != 0) {
        return false;
    }
    *out = g_info;   /* struct copy under the lock */
    k_mutex_unlock(&info_mutex);
    return true;
}

// ================================
// Screen: weather
// ================================
static void screen_show_weather(void)
{
    info_data_t info = {0};
    if (!get_info_snapshot(&info) || info.weather[0] == '\0') {
        lcd_show("Weather:", "No data yet");
        return;
    }
    char line2[17] = {0};
    snprintf(line2, sizeof(line2), "%.13s C", info.temperature);
    lcd_show(info.weather, line2);
}

// ================================
// Screen: Unix epoch
// ================================
static void screen_show_epoch(void)
{
    info_data_t info = {0};
    if (!get_info_snapshot(&info) || info.unix_epoch[0] == '\0') {
        lcd_show("Unix epoch:", "No data yet");
        return;
    }
    lcd_show("Unix epoch:", info.unix_epoch);
}

// ================================
// Screen: latitude and longitude
// ================================
static void screen_show_latlon(void)
{
    info_data_t info = {0};
    if (!get_info_snapshot(&info) || info.latitude[0] == '\0') {
        lcd_show("Lat/Lon:", "No data yet");
        return;
    }
    char line1[17] = {0};
    char line2[17] = {0};
    snprintf(line1, sizeof(line1), "Lat: %.11s", info.latitude);
    snprintf(line2, sizeof(line2), "Lon: %.11s", info.longitude);
    lcd_show(line1, line2);
}

// ================================
// Screen: city
// ================================
static void screen_show_city(void)
{
    info_data_t info = {0};
    if (!get_info_snapshot(&info) || info.city[0] == '\0') {
        lcd_show("City:", "No data yet");
        return;
    }
    lcd_show("City:", info.city);
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
    *wday = (int)((days_total + 4) % 7);

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
    info_data_t info = {0};
    if (!get_info_snapshot(&info) || info.unix_epoch[0] == '\0') {
        char line1[17] = {0};
        alarm_get_clock_text(line1, sizeof(line1));
        lcd_show(line1, "No date yet");
        return;
    }

    long epoch = 0;
    for (int i = 0; info.unix_epoch[i] >= '0' && info.unix_epoch[i] <= '9'; i++) {
        epoch = epoch * 10 + (long)(info.unix_epoch[i] - '0');
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
// Screen: news titles
// ================================

/* Static so the 10*80 = 800-byte snapshot never sits on the stack */
static char news_snapshot[NEWS_MAX][NEWS_TITLE_LEN];

static void screen_show_news(void)
{
    int count = 0;

    if (k_mutex_lock(&news_mutex, K_MSEC(MUTEX_TIMEOUT_MS)) == 0) {
        if (g_news.ready) {
            count = g_news.count;
            for (int i = 0; i < count; i++) {
                strncpy(news_snapshot[i], g_news.titles[i], NEWS_TITLE_LEN - 1);
                news_snapshot[i][NEWS_TITLE_LEN - 1] = '\0';
            }
        }
        k_mutex_unlock(&news_mutex);
    }

    if (count == 0) {
        lcd_show("News:", "No data yet");
        k_sleep(K_SECONDS(2));
        return;
    }

    for (int i = 0; i < count; i++) {
        char line1[17] = {0};
        char line2[17] = {0};

        snprintf(line1, sizeof(line1), "News %d/%d", i + 1, count);
        snprintf(line2, sizeof(line2), "%.16s", news_snapshot[i]);
        lcd_show(line1, line2);
        k_sleep(K_SECONDS(3));

        if ((int)strlen(news_snapshot[i]) > 16) {
            snprintf(line2, sizeof(line2), "%.16s", news_snapshot[i] + 16);
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
        screen_show_alarm();
        k_sleep(K_SECONDS(2));

        screen_show_temp_hum();
        k_sleep(K_SECONDS(2));

        // screen_show_news();

        screen_show_weather();
        k_sleep(K_SECONDS(2));

        screen_show_epoch();
        k_sleep(K_SECONDS(2));

        screen_show_latlon();
        k_sleep(K_SECONDS(2));

        screen_show_city();
        k_sleep(K_SECONDS(2));

        screen_show_datetime();
        k_sleep(K_SECONDS(2));
    }
}