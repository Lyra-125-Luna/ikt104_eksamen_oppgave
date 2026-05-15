#ifndef SCREEN_H
#define SCREEN_H

void boot(void);
void screen_show_alarm(void);
void screen_show_temp_hum(void);
void screen_thread_entry(void *arg1, void *arg2, void *arg3);

#endif // SCREEN_H