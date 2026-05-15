#ifndef BLINKY_NETWORK_CONFIG_H
#define BLINKY_NETWORK_CONFIG_H
#include <zephyr/kernel.h>

extern struct k_sem   wifi_ready_sem;
extern struct k_mutex http_mutex;
extern uint8_t        shared_http_buf[3072];
extern uint8_t        shared_http_chunk[256];

#define INFO_STR_LEN   32
#define NEWS_MAX       10
#define NEWS_TITLE_LEN 80

typedef struct {
	char city[INFO_STR_LEN];
	char country[INFO_STR_LEN];
	char weather[INFO_STR_LEN];
	char temperature[INFO_STR_LEN];
	char latitude[INFO_STR_LEN];
	char longitude[INFO_STR_LEN];
	char unix_epoch[INFO_STR_LEN];
	bool ready;
} info_data_t;

typedef struct {
	char titles[NEWS_MAX][NEWS_TITLE_LEN];
	int  count;
	bool ready;
} news_data_t;

extern info_data_t    g_info;
extern struct k_mutex info_mutex;

extern news_data_t    g_news;
extern struct k_mutex news_mutex;

void wifi_connect(void);
void network_config(void);
void network_config_thread_entry(void *p1, void *p2, void *p3);
void network_config_news(void);
void network_config_thread_entry_news(void *p1, void *p2, void *p3);

#endif // BLINKY_NETWORK_CONFIG_H