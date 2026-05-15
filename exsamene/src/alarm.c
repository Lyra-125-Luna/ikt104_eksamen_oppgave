#include "alarm.h"

#include <stdio.h>
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>

// ================================
// Alarm configuration
// ================================

// 20 for testing. Change to 600 before final delivery.
#define ALARM_AUTO_MUTE_SECONDS 20
#define SNOOZE_SECONDS 300

#define DEFAULT_ALARM_HOUR   0
#define DEFAULT_ALARM_MINUTE 0
#define DEFAULT_ALARM_SECOND 3

// ================================
// GPIO pins
// ================================

#define SNOOZE_PIN   6   // PA6 / D12
#define MUTE_PIN     7   // PA7 / D11
#define ENABLE_PIN   1   // PB1 / D6
#define SPEAKER_PIN  0   // PB0 / D3

// ================================
// Clock state
// ================================

static int current_hour   = 0;
static int current_minute = 0;
static int current_second = 0;

// ================================
// Alarm state
// ================================

static int  alarm_hour   = DEFAULT_ALARM_HOUR;
static int  alarm_minute = DEFAULT_ALARM_MINUTE;
static int  alarm_second = DEFAULT_ALARM_SECOND;

static bool alarm_enabled  = true;
static bool alarm_active   = false;
static bool alarm_snoozed  = false;

static int active_seconds      = 0;
static int snooze_seconds_left = 0;

// ================================
// GPIO state
// ================================

static const struct device *gpioa;
static const struct device *gpiob;

static int last_snooze = 1;
static int last_mute   = 1;
static int last_enable = 1;

// ================================
// Clock update
// ================================

static void update_clock(void)
{
    current_second++;
    if (current_second >= 60) { current_second = 0; current_minute++; }
    if (current_minute >= 60) { current_minute = 0; current_hour++;   }
    if (current_hour   >= 24) { current_hour   = 0;                   }
}

// ================================
// Alarm check
// ================================

static void check_alarm_time(void)
{
    if (!alarm_enabled || alarm_active || alarm_snoozed) return;

    if (current_hour   == alarm_hour   &&
        current_minute == alarm_minute &&
        current_second == alarm_second) {
        alarm_active   = true;
        active_seconds = 0;
        printk("ALARM ACTIVE!\n");
    }
}

// ================================
// Snooze update
// ================================

static void update_snooze(void)
{
    if (!alarm_snoozed) return;
    snooze_seconds_left--;
    if (snooze_seconds_left <= 0) {
        alarm_snoozed  = false;
        alarm_active   = true;
        active_seconds = 0;
        printk("SNOOZE FINISHED - ALARM ACTIVE AGAIN!\n");
    }
}

// ================================
// Auto mute
// ================================

static void update_auto_mute(void)
{
    if (!alarm_active) return;
    active_seconds++;
    if (active_seconds >= ALARM_AUTO_MUTE_SECONDS) {
        alarm_active   = false;
        active_seconds = 0;
        printk("ALARM AUTO MUTED\n");
    }
}

// ================================
// Speaker thread
// ================================

/*
 * Runs in its own thread so the busy-toggle loop never overflows
 * the alarm thread's stack or blocks it for 100 ms at a time.
 */

#define SPEAKER_STACK_SIZE 512
#define SPEAKER_PRIORITY   4

K_THREAD_STACK_DEFINE(speaker_stack, SPEAKER_STACK_SIZE);
static struct k_thread speaker_thread_data;

static void speaker_thread_entry(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    while (1) {
        if (alarm_active && gpiob != NULL) {
            /* 200 half-cycles at 250 µs each ≈ one 100 ms beep burst */
            for (int i = 0; i < 200; i++) {
                gpio_pin_set(gpiob, SPEAKER_PIN, 1);
                k_busy_wait(250);
                gpio_pin_set(gpiob, SPEAKER_PIN, 0);
                k_busy_wait(250);
            }
            k_msleep(50);   /* short gap between bursts */
        } else {
            if (gpiob != NULL) {
                gpio_pin_set(gpiob, SPEAKER_PIN, 0);
            }
            k_msleep(100);
        }
    }
}

// ================================
// GPIO init
// ================================

void alarm_gpio_init(void)
{
    gpioa = DEVICE_DT_GET(DT_NODELABEL(gpioa));
    gpiob = DEVICE_DT_GET(DT_NODELABEL(gpiob));

    if (!device_is_ready(gpioa) || !device_is_ready(gpiob)) {
        printk("GPIO not ready\n");
        return;
    }

    gpio_pin_configure(gpioa, SNOOZE_PIN,  GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_configure(gpioa, MUTE_PIN,    GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_configure(gpiob, ENABLE_PIN,  GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_configure(gpiob, SPEAKER_PIN, GPIO_OUTPUT_INACTIVE);

    printk("Alarm GPIO ready\n");

    k_thread_create(&speaker_thread_data,
                    speaker_stack, SPEAKER_STACK_SIZE,
                    speaker_thread_entry,
                    NULL, NULL, NULL,
                    SPEAKER_PRIORITY, 0, K_NO_WAIT);
}

// ================================
// Button check
// ================================

void alarm_check_buttons(void)
{
    if (gpioa == NULL || gpiob == NULL) return;

    int snooze_now = gpio_pin_get(gpioa, SNOOZE_PIN);
    int mute_now   = gpio_pin_get(gpioa, MUTE_PIN);
    int enable_now = gpio_pin_get(gpiob, ENABLE_PIN);

    if (snooze_now == 0 && last_snooze == 1) alarm_snooze();
    if (mute_now   == 0 && last_mute   == 1) alarm_mute();
    if (enable_now == 0 && last_enable == 1) alarm_toggle_enabled();

    last_snooze = snooze_now;
    last_mute   = mute_now;
    last_enable = enable_now;
}

// ================================
// Alarm thread
// ================================

void alarm_thread_entry(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    printk("Alarm thread started\n");

    alarm_gpio_init();

    while (1) {
        update_clock();
        alarm_check_buttons();
        check_alarm_time();
        update_snooze();
        update_auto_mute();
        /* Speaker handled by its own thread */

        printk("Clock: %02d:%02d:%02d | Alarm: %02d:%02d | Enabled: %d | Active: %d | Snoozed: %d\n",
               current_hour, current_minute, current_second,
               alarm_hour, alarm_minute,
               alarm_enabled, alarm_active, alarm_snoozed);

        k_sleep(K_SECONDS(1));
    }
}

// ================================
// Alarm control
// ================================

void alarm_snooze(void)
{
    if (alarm_active) {
        alarm_active        = false;
        alarm_snoozed       = true;
        snooze_seconds_left = SNOOZE_SECONDS;
        active_seconds      = 0;
        printk("Alarm snoozed\n");
    }
}

void alarm_mute(void)
{
    if (alarm_active || alarm_snoozed) {
        alarm_active        = false;
        alarm_snoozed       = false;
        active_seconds      = 0;
        snooze_seconds_left = 0;
        printk("Alarm muted\n");
    }
}

void alarm_toggle_enabled(void)
{
    alarm_enabled = !alarm_enabled;
    if (!alarm_enabled) {
        alarm_active        = false;
        alarm_snoozed       = false;
        active_seconds      = 0;
        snooze_seconds_left = 0;
    }
    printk("Alarm enabled: %d\n", alarm_enabled);
}

// ================================
// LCD text helpers
// ================================

void alarm_get_clock_text(char *buffer, int buffer_size)
{
    snprintf(buffer, buffer_size, "%02d:%02d:%02d",
             current_hour, current_minute, current_second);
}

void alarm_get_status_text(char *buffer, int buffer_size)
{
    if (!alarm_enabled) {
        snprintf(buffer, buffer_size, "                ");
    } else if (alarm_active) {
        snprintf(buffer, buffer_size, "Alarm active");
    } else if (alarm_snoozed) {
        snprintf(buffer, buffer_size, "Alarm snoozed");
    } else {
        snprintf(buffer, buffer_size, "Alarm %02d:%02d", alarm_hour, alarm_minute);
    }
}