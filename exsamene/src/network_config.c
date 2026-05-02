#include "network_config.h"

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <stdlib.h>

#include <zephyr/data/json.h>

#include <inttypes.h>
#include <string.h>

#define _POSIX_C_SOURCE 200809L
#include <zephyr/net/socket.h>
#include <zephyr/net/http/client.h>
#include <errno.h>

LOG_MODULE_REGISTER(network_config, LOG_LEVEL_DBG);

#define WIFI_SSID     "JLV-21-23"
#define WIFI_PASSWORD "1522221300"

#define HTTP_HOST_UNIX "api.ipgeolocation.io"
#define HTTP_PORT_UNIX "80"
#define HTTP_PATH_BASE "/v3/timezone?apiKey=d481d36b33e249d4abe04a1c77428884&ip="

// Buffer for ip=
static char http_path_buf[256];

// Semaphore fore IP address
static K_SEM_DEFINE(got_ip_sem, 0, 1);

// JSON struct
struct joson_info {
    const char *fact;
    int32_t length;
};

static const struct json_obj_descr foo_descr[] = {
    JSON_OBJ_DESCR_PRIM(struct joson_info, fact, JSON_TOK_STRING),
    JSON_OBJ_DESCR_PRIM(struct joson_info, length, JSON_TOK_NUMBER),
};

static void event_handler(uint64_t mgmt_event, struct net_if *iface, void *info, size_t info_length, void *user_data)
{
    switch (mgmt_event) {
        case NET_EVENT_WIFI_CONNECT_RESULT: {
            const struct wifi_status *status = (const struct wifi_status *)info;
            if (status->status) {
                LOG_ERR("Connection failed (%d)", status->status);
            } else {
                LOG_INF("Connected to Wi-Fi");
            }
            break;
        }

        case NET_EVENT_IPV4_ADDR_ADD:
            LOG_INF("Got IPv4 address");
            k_sem_give(&got_ip_sem);
            break;

        default:
            break;
    }
}

static uint8_t recv_buf[512];

static int response_cb(struct http_response *rsp,
                       enum http_final_call final_data,
                       void *user_data)
{
    static char full_body[1024];
    static int full_body_len = 0;

    if (rsp->body_frag_len > 0) {
        memcpy(full_body + full_body_len,
               rsp->body_frag_start,
               rsp->body_frag_len);
        full_body_len += rsp->body_frag_len;
    }

    if (final_data == HTTP_DATA_FINAL) {
        full_body[full_body_len] = '\0';
        printk("Full body: %s\n", full_body);
    	printk("full_body_len: %d\n", full_body_len);

        struct joson_info data;
        memset(&data, 0, sizeof(data));

        int ret = json_obj_parse(full_body, full_body_len,
                                 foo_descr, ARRAY_SIZE(foo_descr),
                                 &data);
        if (ret > 0) {
            printk("fact: %s\n", data.fact);
            printk("length: %d\n", data.length);
        } else {
            printk("JSON parse error: %d\n", ret);
        }

        full_body_len = 0;
    }

    return 0;
}

static int http_request(const char *path)
{
    struct addrinfo hints;
    struct addrinfo *res;
    int sock;
    int ret;

    LOG_INF("Resolving %s...", HTTP_HOST_UNIX);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    ret = getaddrinfo(HTTP_HOST_UNIX, HTTP_PORT_UNIX, &hints, &res);
    if (ret != 0) {
        LOG_ERR("DNS lookup failed: %d", ret);
        return ret;
    }

    LOG_INF("DNS resolved, creating socket...");

    sock = socket(res->ai_family, res->ai_socktype, IPPROTO_TCP);
    if (sock < 0) {
        LOG_ERR("Socket creation failed: %d", errno);
        freeaddrinfo(res);
        return -errno;
    }

    LOG_INF("Connecting to %s:%s...", HTTP_HOST_UNIX, HTTP_PORT_UNIX);

    ret = connect(sock, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    if (ret < 0) {
        LOG_ERR("Connect failed: %d", errno);
        close(sock);
        return -errno;
    }

    LOG_INF("Connected, sending HTTP GET %s...", path);

    struct http_request req = {
        .method = HTTP_GET,
        .url = path,
        .host = HTTP_HOST_UNIX,
        .protocol = "HTTP/1.1",
        .response = response_cb,
        .recv_buf = recv_buf,
        .recv_buf_len = sizeof(recv_buf),
    };

    ret = http_client_req(sock, &req, 5 * MSEC_PER_SEC, NULL);

    if (ret < 0) {
        LOG_ERR("HTTP request failed: %d", ret);
    } else {
        LOG_INF("HTTP request completed (%d bytes sent)", ret);
    }

    close(sock);
    return ret;
}

NET_MGMT_REGISTER_EVENT_HANDLER(net_event_handler_cb, NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT, event_handler, NULL);
NET_MGMT_REGISTER_EVENT_HANDLER(ipv4_event_handler_cb, NET_EVENT_IPV4_ADDR_ADD, event_handler, NULL);

void network_config(void)
{
    LOG_INF("Waiting for networking to initialize...");
    k_sleep(K_SECONDS(5));

    LOG_INF("Getting default network interface...");
    struct net_if *iface = net_if_get_default();

    if (!iface) {
        LOG_ERR("No default interface found");
        return;
    }

    LOG_INF("Initializing connection parameters...");
    static struct wifi_connect_req_params params = {
        .ssid = WIFI_SSID,
        .ssid_length = strlen(WIFI_SSID),
        .psk = WIFI_PASSWORD,
        .psk_length = strlen(WIFI_PASSWORD),
        .channel = WIFI_CHANNEL_ANY,
        .security = WIFI_SECURITY_TYPE_PSK,
        .band = WIFI_FREQ_BAND_2_4_GHZ,
    };

    LOG_INF("Connecting to Wi-Fi...");
    int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));

    if (!ret) {
        LOG_INF("Connection request sent");
    } else {
        LOG_ERR("Connection request failed (%d)", ret);
        return;
    }

    LOG_INF("Waiting for IP address...");
    if (k_sem_take(&got_ip_sem, K_SECONDS(30)) != 0) {
        LOG_ERR("Timed out waiting for IP address");
        return;
    }

    // Get the assigned IP address from the interface
    struct net_if_ipv4 *ipv4 = iface->config.ip.ipv4;
    char ip_str[NET_IPV4_ADDR_LEN];

    net_addr_ntop(AF_INET,
                  &ipv4->unicast[0].ipv4.address.in_addr,
                  ip_str,
                  sizeof(ip_str));

    LOG_INF("Device IP: %s", ip_str);

    // Build the full request path with the device IP appended
    snprintk(http_path_buf, sizeof(http_path_buf),
             HTTP_PATH_BASE "%s", ip_str);

    LOG_INF("Request path: %s", http_path_buf);

    // Make exactly one HTTP request
    LOG_INF("Got IP, making HTTP request...");
    ret = http_request(http_path_buf);
    if (ret < 0) {
        LOG_ERR("Request failed (%d)", ret);
    }

    while (1) {
        k_sleep(K_FOREVER);
    }
}

void network_config_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    network_config();
}