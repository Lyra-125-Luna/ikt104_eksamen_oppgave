#ifndef BLINKY_NETWORK_CONFIG_H
#define BLINKY_NETWORK_CONFIG_H

#include <zephyr/kernel.h>


extern struct k_sem   wifi_ready_sem;
extern struct k_mutex http_mutex;
extern uint8_t        shared_http_buf[3072];
extern uint8_t        shared_http_chunk[256];

void wifi_connect(void);

void network_config(void);
void network_config_thread_entry(void *p1, void *p2, void *p3);

void network_config_news(void);
void network_config_thread_entry_news(void *p1, void *p2, void *p3);


#endif // BLINKY_NETWORK_CONFIG_H
