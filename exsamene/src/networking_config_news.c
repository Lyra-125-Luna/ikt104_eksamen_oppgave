#include "network_config.h"

#include <zephyr/posix/fcntl.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <string.h>
#include <errno.h>


LOG_MODULE_REGISTER(network_config_news, LOG_LEVEL_INF);

#define HTTP_HOST  "10.130.51.252"
#define HTTP_PORT  5000
#define HTTP_PATH  "/news"

#define NEWS_ENOUGH_BYTES 2048

/* =========================
 * PRINT TITLES
 * ========================= */

static void print_titles(const char *json)
{
	printk("=== NEWS TITLES ===\n");

	/* Store up to 20 unique titles (pointers + lengths for comparison) */
#define MAX_TITLES 10
#define MAX_TITLE_LEN 120

	static char seen[MAX_TITLES][MAX_TITLE_LEN];
	int seen_count = 0;
	int printed = 0;

	const char *pos = json;

	while ((pos = strstr(pos, "\"title\":")) != NULL && printed < 10) {
		pos += 8;
		while (*pos == ' ' || *pos == '\t') pos++;

		if (strncmp(pos, "null", 4) == 0) {
			pos += 4;
			continue;
		}

		if (*pos == '"') pos++;

		/* Copy title into temp buffer */
		char title[MAX_TITLE_LEN];
		int i = 0;
		const char *start = pos;
		while (*pos && *pos != '"' && i < (int)sizeof(title) - 1) {
			title[i++] = *pos++;
		}
		title[i] = '\0';
		if (*pos == '"') pos++;

		/* Check for duplicate */
		bool duplicate = false;
		for (int j = 0; j < seen_count; j++) {
			if (strncmp(seen[j], title, MAX_TITLE_LEN) == 0) {
				duplicate = true;
				break;
			}
		}

		if (!duplicate) {
			printk("%d. %s\n", ++printed, title);
			if (seen_count < MAX_TITLES) {
				strncpy(seen[seen_count++], title, MAX_TITLE_LEN - 1);
			}
		}
	}

	printk("===================\n");
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

	size_t limetReturm = 100;

	while (total < limetReturm) {
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
		printk(body);
	} else {
		printk("No body found, raw: %.200s\n", shared_http_buf);
	}

	close(sock);
	return total > 0 ? 0 : -1;
}


/* =========================
 * MAIN NETWORK FUNCTION
 * ========================= */

void network_config_news(void)
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

		LOG_INF("Requesting /info");
		int ret = http_request(HTTP_PATH);
		if (ret < 0) {
			LOG_ERR("HTTP request failed (%d)", ret);
		}

		LOG_INF("mutex unloce");
		k_mutex_unlock(&http_mutex);

		LOG_INF("Slepping");
		k_sleep(K_FOREVER);

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
