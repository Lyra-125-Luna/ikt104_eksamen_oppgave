#include "network_config.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <string.h>
#include <errno.h>

LOG_MODULE_REGISTER(network_config, LOG_LEVEL_INF);

#define HTTP_HOST  "10.130.51.252"
#define HTTP_PORT  5000
#define HTTP_PATH  "/info"

/* =========================
 * PRINT INFO
 * ========================= */

static void print_info(const char *json)
{
    const char *fields[] = {"city", "country", "weather",
                            "temperature_celsius", NULL};

    printk("=== INFO ===\n");

    for (int i = 0; fields[i] != NULL; i++) {
        char key[32];
        snprintk(key, sizeof(key), "\"%s\":", fields[i]);

        char *pos = strstr(json, key);
        if (pos) {
            pos += strlen(key);
            while (*pos == ' ' || *pos == '\t') pos++;
            printk("%s: ", fields[i]);
            while (*pos && *pos != ',' && *pos != '}') {
                printk("%c", *pos++);
            }
            printk("\n");
        }
    }

    printk("============\n");
}

/* =========================
 * HTTP REQUEST
 * ========================= */

static int http_request(const char *path)
{
	LOG_INF("http_request() started: %s", path);

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

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
		LOG_ERR("Socket creation failed (%d)", errno);
		return -errno;
	}

	//struct zsock_timeval tv = { .tv_sec = 3, .tv_usec = 0 };
	//zsock_setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));



	ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0) {
		LOG_ERR("Connect failed (%d)", errno);
		close(sock);
		return -errno;
	}

	LOG_INF("Connected, sending raw HTTP GET");

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
		return -errno;
	}

	LOG_INF("Request sent, reading response");

	memset(shared_http_buf, 0, sizeof(shared_http_buf));
	int total = 0;
	int content_length = -1;

	struct zsock_pollfd fds = {
		.fd     = sock,
		.events = ZSOCK_POLLIN,
	    };

	while (total < (int)sizeof(shared_http_buf) - 1) {
		int r = zsock_poll(&fds, 1, 8000);
		if (r == 0) {
			LOG_WRN("poll timeout");
			break;
		}
		if (r < 0 && !(fds.revents & ZSOCK_POLLIN)) {
			LOG_WRN("poll error, no data: errno=%d revents=0x%x", errno, fds.revents);
			break;
		}

		int n = recv(sock, shared_http_chunk,
			     sizeof(shared_http_chunk) - 1, 0);
		LOG_INF("recv returned %d", n);
		if (n <= 0) break;
		memcpy(shared_http_buf + total, shared_http_chunk, n);
		total += n;

		if (content_length < 0) {
			char *cl = strstr((char *)shared_http_buf, "Content-Length: ");
			char *hdr_end = strstr((char *)shared_http_buf, "\r\n\r\n");
			if (cl && hdr_end) {
				content_length = atoi(cl + 16);
				int header_size = (hdr_end + 4) - (char *)shared_http_buf;
				LOG_INF("Content-Length: %d, header: %d", content_length, header_size);
				if (total >= header_size + content_length) break;
			}
		} else {
			char *hdr_end = strstr((char *)shared_http_buf, "\r\n\r\n");
			if (hdr_end) {
				int header_size = (hdr_end + 4) - (char *)shared_http_buf;
				if (total >= header_size + content_length) break;
			}
		}
	}

	LOG_INF("Total bytes received: %d", total);

	char *body = strstr((char *)shared_http_buf, "\r\n\r\n");
	if (body) {
		body += 4;
		print_info(body);
	} else {
		printk("No body found, raw: %.200s\n", shared_http_buf);
	}

	close(sock);
	return total > 0 ? 0 : -1;
}


/* =========================
 * MAIN NETWORK FUNCTION
 * ========================= */

void network_config(void)
{
	k_mutex_lock(&http_mutex, K_FOREVER);
    LOG_INF("Waiting for WiFi...");
	LOG_INF("Slepping");

    if (k_sem_take(&wifi_ready_sem, K_SECONDS(60)) != 0) {
        LOG_ERR("Timed out waiting for WiFi");
        return;
    }

    while (1) {
    	LOG_INF("Waiting for HTTP slot...");
        k_mutex_lock(&http_mutex, K_FOREVER);

        LOG_INF("Requesting /info");
        int ret = http_request(HTTP_PATH);
        if (ret < 0) {
            LOG_ERR("HTTP request failed (%d)", ret);
        }

		k_mutex_unlock(&http_mutex);
    		LOG_INF("mutex unloce");
    		LOG_INF("Slepping");
		k_sleep(K_FOREVER);
    }
}

/* =========================
 * THREAD ENTRY
 * ========================= */


void network_config_thread_entry(void *p1, void *p2, void *p3)
{
    printk("=== /info THREAD STARTED ===\n");
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    network_config();
}