#define WIFI_SSID     "JLV-21-23"
#define WIFI_PASSWORD "1522221300"

#include "network_config.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>

K_MUTEX_DEFINE(http_mutex);

LOG_MODULE_REGISTER(wifi_connect, LOG_LEVEL_INF);

// This semaphore is shared with both network files
K_SEM_DEFINE(wifi_ready_sem, 0, 1);
uint8_t shared_http_buf[3072];
uint8_t shared_http_chunk[256];

static bool ip_handled = false;

static void event_handler(uint64_t mgmt_event,
                          struct net_if *iface,
                          void *info,
                          size_t info_length,
                          void *user_data)
{
    switch (mgmt_event) {

    case NET_EVENT_WIFI_CONNECT_RESULT: {
        const struct wifi_status *status = (const struct wifi_status *)info;
        if (status->status) {
            LOG_ERR("WiFi connection failed (%d)", status->status);
        } else {
            LOG_INF("Connected to WiFi");
        }
        break;
    }

    case NET_EVENT_IPV4_ADDR_ADD:
        if (!ip_handled) {
            ip_handled = true;
            LOG_INF("Got IPv4 address");
            k_sem_give(&wifi_ready_sem);
        }
        break;

    default:
        break;
    }
}

NET_MGMT_REGISTER_EVENT_HANDLER(
    wifi_handler,
    NET_EVENT_WIFI_CONNECT_RESULT,
    event_handler,
    NULL
);

NET_MGMT_REGISTER_EVENT_HANDLER(
    ipv4_handler,
    NET_EVENT_IPV4_ADDR_ADD,
    event_handler,
    NULL
);

void wifi_connect(void)
{
    LOG_INF("Waiting 5 seconds for networking stack");
    k_sleep(K_SECONDS(5));

    struct net_if *iface = net_if_get_default();
    if (!iface) {
        LOG_ERR("No network interface found");
        return;
    }

    static struct wifi_connect_req_params params = {
        .ssid        = WIFI_SSID,
        .ssid_length = sizeof(WIFI_SSID) - 1,
        .psk         = WIFI_PASSWORD,
        .psk_length  = sizeof(WIFI_PASSWORD) - 1,
        .channel     = WIFI_CHANNEL_ANY,
        .security    = WIFI_SECURITY_TYPE_PSK,
        .band        = WIFI_FREQ_BAND_2_4_GHZ,
    };

    LOG_INF("Connecting to WiFi: %s", WIFI_SSID);

    int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
    if (ret) {
        LOG_ERR("WiFi connect request failed (%d)", ret);
        return;
    }

    LOG_INF("Waiting for IP address...");
    if (k_sem_take(&wifi_ready_sem, K_SECONDS(30)) != 0) {
        LOG_ERR("Timeout waiting for IP address");
        return;
    }

    LOG_INF("WiFi ready - giving semaphore to both network threads");

    // Release twice so both waiting threads can proceed
    k_sem_give(&wifi_ready_sem);
    k_sem_give(&wifi_ready_sem);
}