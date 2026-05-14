#include "alarm.h"

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <stdbool.h>

// ================================
// Alarm configuration
// ================================

// bytt tilbake til 600
#define ALARM_AUTO_MUTE_SECONDS 20
#define SNOOZE_SECONDS 300

// Test alarm: går av etter 30 sekunder
#define DEFAULT_ALARM_HOUR 0
#define DEFAULT_ALARM_MINUTE 0
#define DEFAULT_ALARM_SECOND 30

// ================================
// Clock state
// ================================

static int current_hour = 0;
static int current_minute = 0;
static int current_second = 0;

// ================================
// Alarm state
// ================================

static int alarm_hour = DEFAULT_ALARM_HOUR;
static int alarm_minute = DEFAULT_ALARM_MINUTE;
static int alarm_second = DEFAULT_ALARM_SECOND;

static bool alarm_enabled = true;
static bool alarm_active = false;
static bool alarm_snoozed = false;

static int active_seconds = 0;
static int snooze_seconds_left = 0;

// ================================
// Internal clock update
// ================================

static void update_clock(void)
{
    current_second++;

    if (current_second >= 60) {
        current_second = 0;
        current_minute++;
    }

    if (current_minute >= 60) {
        current_minute = 0;
        current_hour++;
    }

    if (current_hour >= 24) {
        current_hour = 0;
    }
}

// ================================
// Alarm trigger check
// ================================

static void check_alarm_time(void)
{
    if (!alarm_enabled || alarm_active || alarm_snoozed) {
        return;
    }

    if (current_hour == alarm_hour &&
        current_minute == alarm_minute &&
        current_second == alarm_second) {

        alarm_active = true;
        active_seconds = 0;

        printk("ALARM ACTIVE!\n");
    }
}

// ================================
// Snooze countdown
// ================================

static void update_snooze(void)
{
    if (!alarm_snoozed) {
        return;
    }

    snooze_seconds_left--;

    if (snooze_seconds_left <= 0) {
        alarm_snoozed = false;
        alarm_active = true;
        active_seconds = 0;

        printk("SNOOZE FINISHED - ALARM ACTIVE AGAIN!\n");
    }
}

// ================================
// Auto mute after 10 minutes
// ================================

static void update_auto_mute(void)
{
    if (!alarm_active) {
        return;
    }

    active_seconds++;

    if (active_seconds >= ALARM_AUTO_MUTE_SECONDS) {
        alarm_active = false;
        active_seconds = 0;

        printk("ALARM AUTO MUTED\n");
    }
}

// ================================
// Placeholder for speaker/buzzer
// ================================

static void update_speaker(void)
{
    if (alarm_active) {
        printk("BEEP BEEP BEEP\n");
    }
}

// ================================
// Public alarm thread
// ================================

void alarm_thread_entry(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    printk("Alarm thread started\n");

    while (1) {
        update_clock();
        check_alarm_time();
        update_snooze();
        update_auto_mute();
        update_speaker();

        printk("Clock: %02d:%02d:%02d | Alarm: %02d:%02d:%02d | Enabled: %d | Active: %d | Snoozed: %d\n",
               current_hour,
               current_minute,
               current_second,
               alarm_hour,
               alarm_minute,
               alarm_second,
               alarm_enabled,
               alarm_active,
               alarm_snoozed);

        k_sleep(K_SECONDS(1));
    }
}

// ================================
// Alarm control functions
// These will be connected to buttons later
// ================================
void alarm_snooze(void)
{
    if (alarm_active) {
        alarm_active = false;
        alarm_snoozed = true;
        snooze_seconds_left = SNOOZE_SECONDS;
        active_seconds = 0;

        printk("Alarm snoozed\n");
    }
}

void alarm_mute(void)
{
    if (alarm_active || alarm_snoozed) {
        alarm_active = false;
        alarm_snoozed = false;
        active_seconds = 0;
        snooze_seconds_left = 0;

        printk("Alarm muted\n");
    }
}

void alarm_toggle_enabled(void)
{
    alarm_enabled = !alarm_enabled;

    if (!alarm_enabled) {
        alarm_active = false;
        alarm_snoozed = false;
    }

    printk("Alarm enabled: %d\n", alarm_enabled);
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