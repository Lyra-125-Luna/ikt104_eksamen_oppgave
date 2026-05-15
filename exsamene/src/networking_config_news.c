#include "network_config.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <string.h>
#include <errno.h>

LOG_MODULE_REGISTER(network_config_news, LOG_LEVEL_INF);

#define HTTP_HOST  "10.130.51.252"
#define HTTP_PORT  5000
#define HTTP_PATH  "/news"

/* =========================
 * SHARED NEWS DATA
 * ========================= */

news_data_t    g_news     = {0};
struct k_mutex news_mutex = Z_MUTEX_INITIALIZER(news_mutex);

/* =========================
 * OWN RECEIVE BUFFER
 * Content-Length 3703 + ~186 header = 3889 bytes.
 * 5120 gives comfortable headroom.
 * Static globals — never on the stack.
 * ========================= */

#define NEWS_BUF_SIZE   5120
#define NEWS_CHUNK_SIZE  256

static uint8_t news_buf[NEWS_BUF_SIZE];
static uint8_t news_chunk[NEWS_CHUNK_SIZE];

/* =========================
 * PARSE TITLES
 * ========================= */

static void parse_news_titles(const char *json)
{
    k_mutex_lock(&news_mutex, K_FOREVER);

    g_news.count = 0;
    g_news.ready = false;

    const char *pos = json;

    while (g_news.count < NEWS_MAX) {
        pos = strstr(pos, "\"title\":");
        if (pos == NULL) break;

        pos += 8;
        while (*pos == ' ' || *pos == '\t') pos++;

        if (strncmp(pos, "null", 4) == 0) {
            pos += 4;
            continue;
        }

        if (*pos == '"') pos++;

        int i = 0;
        while (*pos && *pos != '"' && i < NEWS_TITLE_LEN - 1) {
            g_news.titles[g_news.count][i++] = *pos++;
        }
        g_news.titles[g_news.count][i] = '\0';
        if (*pos == '"') pos++;

        if (i > 0) {
            LOG_INF("title %d: %.60s", g_news.count + 1,
                    g_news.titles[g_news.count]);
            g_news.count++;
        }
    }

    if (g_news.count > 0) {
        g_news.ready = true;
    }

    k_mutex_unlock(&news_mutex);

    LOG_INF("Parsed %d news titles", g_news.count);
}

/* =========================
 * HTTP REQUEST
 *
 * KEY DESIGN: http_mutex is held only while connecting and sending.
 * It is released BEFORE the recv loop so the /info thread is never
 * blocked during the slow multi-packet download, which was causing
 * the network stack to drop packets ("Cannot allocate rx packet").
 * ========================= */

static int http_request(const char *path)
{
    LOG_INF("news http_request(): %s", path);

    int sock;
    int ret;

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(HTTP_PORT);

    ret = net_addr_pton(AF_INET, HTTP_HOST, &addr.sin_addr);
    if (ret < 0) {
        LOG_ERR("Invalid IP address");
        return -EINVAL;
    }

    /* --- MUTEX ON: connect + send only --- */
    k_mutex_lock(&http_mutex, K_FOREVER);

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        LOG_ERR("Socket creation failed (%d)", errno);
        k_mutex_unlock(&http_mutex);
        return -errno;
    }

    ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        LOG_ERR("Connect failed (%d)", errno);
        close(sock);
        k_mutex_unlock(&http_mutex);
        return -errno;
    }

    char request[256];
    snprintk(request, sizeof(request),
        "GET %s HTTP/1.0\r\n"
        "Host: %s\r\n"
        "Accept: application/json\r\n"
        "Connection: close\r\n"
        "\r\n",
        path, HTTP_HOST);

    ret = send(sock, request, strlen(request), 0);
    if (ret < 0) {
        LOG_ERR("Send failed (%d)", errno);
        close(sock);
        k_mutex_unlock(&http_mutex);
        return -errno;
    }

    /* --- MUTEX OFF: release before recv so /info thread can run --- */
    k_mutex_unlock(&http_mutex);

    LOG_INF("Request sent, receiving (mutex released)");

    memset(news_buf, 0, sizeof(news_buf));
    int total          = 0;
    int content_length = -1;

    struct zsock_pollfd fds = {
        .fd     = sock,
        .events = ZSOCK_POLLIN,
    };

    while (total < (int)(sizeof(news_buf) - 1)) {
        int r = zsock_poll(&fds, 1, 8000);
        if (r == 0) {
            LOG_WRN("poll timeout");
            break;
        }
        if (r < 0 && !(fds.revents & ZSOCK_POLLIN)) {
            LOG_WRN("poll error: errno=%d revents=0x%x", errno, fds.revents);
            break;
        }

        int n = recv(sock, news_chunk, sizeof(news_chunk) - 1, 0);
        if (n <= 0) break;

        memcpy(news_buf + total, news_chunk, n);
        total += n;

        if (content_length < 0) {
            char *cl      = strstr((char *)news_buf, "Content-Length: ");
            char *hdr_end = strstr((char *)news_buf, "\r\n\r\n");
            if (cl && hdr_end) {
                content_length = atoi(cl + 16);
                int header_size = (hdr_end + 4) - (char *)news_buf;
                LOG_INF("Content-Length: %d  header: %d bytes",
                        content_length, header_size);
                if (total >= header_size + content_length) break;
            }
        } else {
            char *hdr_end = strstr((char *)news_buf, "\r\n\r\n");
            if (hdr_end) {
                int header_size = (hdr_end + 4) - (char *)news_buf;
                if (total >= header_size + content_length) break;
            }
        }
    }

    LOG_INF("Total bytes received: %d / Content-Length: %d",
            total, content_length);

    char *body = strstr((char *)news_buf, "\r\n\r\n");
    if (body) {
        body += 4;
        parse_news_titles(body);
    } else {
        LOG_WRN("No HTTP body found");
    }

    close(sock);

    /* Success only if we received the full content */
    if (content_length > 0) {
        char *hdr_end  = strstr((char *)news_buf, "\r\n\r\n");
        int   hdr_size = hdr_end ? (hdr_end + 4 - (char *)news_buf) : 0;
        return (total >= hdr_size + content_length) ? 0 : -1;
    }
    return total > 0 ? 0 : -1;
}

/* =========================
 * MAIN NETWORK FUNCTION
 * ========================= */

void network_config_news(void)
{
    LOG_INF("Waiting for WiFi...");

    if (k_sem_take(&wifi_ready_sem, K_SECONDS(60)) != 0) {
        LOG_ERR("Timed out waiting for WiFi");
        return;
    }

    while (1) {
        LOG_INF("Fetching /news");
        int ret = http_request(HTTP_PATH);

        if (ret == 0 && g_news.ready) {
            LOG_INF("News fetched OK (%d titles) — sleeping forever",
                    g_news.count);
            k_sleep(K_FOREVER);
        } else {
            LOG_WRN("News fetch incomplete (got %d titles), retrying in 15 s",
                    g_news.count);
            k_sleep(K_SECONDS(15));
        }
    }
}

/* =========================
 * THREAD ENTRY
 * ========================= */

void network_config_thread_entry_news(void *p1, void *p2, void *p3)
{
    printk("=== /news THREAD STARTED ===\n");
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    network_config_news();
}