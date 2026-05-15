#ifndef ALARM_H
#define ALARM_H

// ================================
// Alarm thread
// ================================
void alarm_thread_entry(void *arg1, void *arg2, void *arg3);

// ================================
// TODO: kall alarm_snooze() når snooze-knappen trykkes
// TODO: kall alarm_mute() når mute-knappen trykkes
// TODO: kall alarm_toggle_enabled() når enable-knappen trykkes
// ================================

void alarm_get_clock_text(char *buffer, int buffer_size);

void alarm_gpio_init(void);
void alarm_check_buttons(void);

void alarm_snooze(void);
void alarm_mute(void);
void alarm_toggle_enabled(void);
void alarm_get_status_text(char *buffer, int buffer_size);


#endif