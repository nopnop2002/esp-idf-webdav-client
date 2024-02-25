/*
	WebDav Example using plain POSIX sockets

	This example code is in the Public Domain (or CC0 licensed, at your option.)

	Unless required by applicable law or agreed to in writing, this
	software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
	CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <mbedtls/base64.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "sdkconfig.h"

#include "cmd.h"

extern QueueHandle_t xQueueCmd;
extern char *mount_point;

static const char *TAG = "HTTP";

/*
buffer:receive data from socket
buffer_length:receive length from socket
header:header data
header_length:length of header
content_length:content length
body_index:index of body
body_length:length of body
*/
bool http_header_analysis(bool *detected, char *buffer, int buffer_length, char *header, int *header_length, int *content_length, int *body_index, int *body_length) {
	*body_index = 0;
	*body_length = 0;
	if (*detected) {
		*body_length = buffer_length;
		return *detected;
	}

	// Search body separator
	int _header_length = *header_length;
	ESP_LOGD(__FUNCTION__, "_header_length=%d", _header_length);
	char SEPARATOR[5];
	SEPARATOR[0] = 0x0d;
	SEPARATOR[1] = 0x0a;
	SEPARATOR[2] = 0x0d;
	SEPARATOR[3] = 0x0a;
	SEPARATOR[4] = 0x00;
	int save_header_length = *header_length;
	memcpy(&header[_header_length], buffer, buffer_length);
	*header_length += buffer_length;
	char *pos = strstr(header, SEPARATOR);
	ESP_LOGD(__FUNCTION__, "pos=%p", pos);
	if (pos == 0) return *detected;

	// Search Content-Length
	char CRLF[3];
	CRLF[0] = 0x0d;
	CRLF[1] = 0x0a;
	CRLF[2] = 0x00;
	*body_index = pos - header - save_header_length + 4;
	*body_length = buffer_length - *body_index;
	ESP_LOGD(__FUNCTION__, "body_index=%d body_length=%d", *body_index, *body_length);
	char *pos1 = strstr(header, "Content-Length:");
	char *pos2 = strstr(pos1, CRLF);
	int pos12 = pos2-pos1;
	ESP_LOGD(__FUNCTION__, "pos12=%d", pos12);
	char work[32];
	memset(work, 0, sizeof(work));
	strncpy(work, pos1+16, pos12-16);
	ESP_LOGD(__FUNCTION__, "work=[%s]", work);
	long _content_length = strtol(work, NULL, 10);
	*content_length = _content_length;
	ESP_LOGD(__FUNCTION__, "content_length=%d", *content_length);
	*detected = true;
	return *detected;
}

size_t http_request_set(char *request, int request_length, char *verb, char *path, int content_length, char *extra_header) {
	//sprintf(request, "%s %s HTTP/1.1\r\nUser-Agent: ESP32 HTTP Client/1.0\r\nHost: %s:%d\r\n", 
	sprintf(request, "%s %s HTTP/1.1\r\nUser-Agent: ESP32 HTTP Client/1.0\r\nHost: %s:%d\r\nConnection: keep-alive\r\n", 
		verb, path, CONFIG_HTTP_ENDPOINT, CONFIG_HTTP_PORT);
	ESP_LOGD(__FUNCTION__, "strlen(extra_header)=%d", strlen(extra_header));
	if (strlen(extra_header)) {
		strcat(request, extra_header);
		strcat(request, "\r\n");
	}
#if CONFIG_HTTP_AUTH_BASIC
	ESP_LOGD(__FUNCTION__, "user=[%s] password=[%s]", CONFIG_HTTP_USER, CONFIG_HTTP_PASSWORD);
	unsigned char authorization[64];
	strcpy((char *)authorization, CONFIG_HTTP_USER);
	strcat((char *)authorization, ":");
	strcat((char *)authorization, CONFIG_HTTP_PASSWORD);
	ESP_LOGD(__FUNCTION__, "authorization=[%s]", (char *)authorization);
	unsigned char base64_buffer[64];
	size_t encord_len;
	size_t authorization_len = strlen((char *)authorization);
	esp_err_t ret = mbedtls_base64_encode(base64_buffer, sizeof(base64_buffer), &encord_len, authorization, authorization_len);
	ESP_LOGD(__FUNCTION__, "mbedtls_base64_encode=%d encord_len=%d base64_buffer=[%.*s]", ret, encord_len, encord_len, base64_buffer);
	strcat(request, "Authorization: Basic ");
	strcat(request, (char *)base64_buffer);
	strcat(request, "\r\n");
#endif
	char length_buffer[33];
	snprintf(length_buffer, sizeof(length_buffer)-1, "Content-Length: %d\r\n", content_length);
	strcat(request, length_buffer);
	strcat(request, "\r\n");
	if(strlen(request) > request_length) {
		ESP_LOGE(__FUNCTION__, "http_request_set buffer too small");
		while(1) { vTaskDelay(1); }
	}
	ESP_LOGD(__FUNCTION__, "request=[%s]", request);
	printf("%s\n", request);
	return strlen(request);
}

void wait_enter(char *prompt) {
	if (strlen(prompt)) ESP_LOGW(TAG, "%s", prompt);
	CMD_t cmdBuf;
	xQueueReceive(xQueueCmd, &cmdBuf, portMAX_DELAY);
}

#define HEADER_SIZE 4096

esp_err_t parse_xml(char *buffer, int buffer_length);

int connect_server(struct addrinfo *res) {
	int sock = socket(res->ai_family, res->ai_socktype, 0);
	if(sock < 0) {
		ESP_LOGE(TAG, "... Failed to allocate socket.");
		return -1;
	}
	ESP_LOGD(TAG, "... allocated socket");

	if(connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
		ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
		close(sock);
		return -1;
	}
	ESP_LOGD(TAG, "... connected. sock=%d", sock);

#if 0
	struct timeval receiving_timeout;
	receiving_timeout.tv_sec = 5;
	receiving_timeout.tv_usec = 0;
	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout)) < 0) {
		ESP_LOGE(TAG, "... failed to set socket receiving timeout");
		close(sock);
		return -1;
	}
	ESP_LOGI(TAG, "... set socket receiving timeout success");
#endif
	return sock;
}

esp_err_t http_webdav_propfind(int sock, bool receive_print) {
	// Allocate header
	char *header_buffer = malloc(HEADER_SIZE);
	if (header_buffer == NULL) {
		ESP_LOGE(TAG, "header_buffer malloc fail");
		return ESP_FAIL;
	}
	memset(header_buffer, 0, HEADER_SIZE);

	// Setup request header
	char request[257];
	http_request_set(request, 256, "PROPFIND", "/", 0, "");

	// Send request
	if (write(sock, request, strlen(request)) < 0) {
		ESP_LOGE(TAG, "... socket send failed");
		return ESP_FAIL;
	}
	ESP_LOGD(TAG, "... socket send success");

	esp_err_t return_code = ESP_OK;
	int header_length = 0;
	int content_length = 0;
	char* content_buffer = NULL;
	int body_index = 0;
	int body_length = 0;
	int content_index = 0;
	bool header_detect = false;

	/* Read HTTP response */
	while(1) {
		char recv_buf[64];
		int recv_bytes;
		bzero(recv_buf, sizeof(recv_buf));
		recv_bytes = read(sock, recv_buf, sizeof(recv_buf)-1);
		if (receive_print) {
			for(int i = 0; i < recv_bytes; i++) {
				putchar(recv_buf[i]);
			}
			printf("\n");
		}

		http_header_analysis(&header_detect, recv_buf, recv_bytes, header_buffer, &header_length, &content_length, &body_index, &body_length);
		ESP_LOGD(TAG, "header_detect=%d content_length=%d body_index=%d body_length=%d", header_detect, content_length, body_index, body_length);
		if (header_detect && content_buffer == NULL) {
			content_buffer = malloc(content_length);
			if (content_buffer == NULL) {
				ESP_LOGE(TAG, "malloc fail. content_length %d", content_length);
				return ESP_FAIL;
			}
		}
		if (header_detect && body_length) {
			memcpy(&content_buffer[content_index], &recv_buf[body_index], body_length);
			content_index += body_length;
			ESP_LOGD(TAG, "content_index=%d", content_index);
			if (content_index == content_length) break;
		}
	} // end while

	ESP_LOGI(TAG, "... done reading from socket. header_length=%d content_length=%d", header_length, content_length);
#if 0
	for(int i = 0; i < content_length; i++) {
		putchar(content_buffer[i]);
	}
	printf("\n");
#endif
	parse_xml(content_buffer, (int)content_length);
	free(header_buffer);
	free(content_buffer);
	return return_code;
} // http_webdav_propfind

esp_err_t http_webdav_mkcol(int sock, char * path, bool receive_print) {
	// Allocate header
	char *header_buffer = malloc(HEADER_SIZE);
	if (header_buffer == NULL) {
		ESP_LOGE(TAG, "header_buffer malloc fail");
		return ESP_FAIL;
	}
	memset(header_buffer, 0, HEADER_SIZE);

	// Setup request header
	char request[257];
	http_request_set(request, 256, "MKCOL", path, 0, "");

	// Send request
	if (write(sock, request, strlen(request)) < 0) {
		ESP_LOGE(TAG, "... socket send failed");
		return ESP_FAIL;
	}
	ESP_LOGD(TAG, "... socket send success");

	esp_err_t return_code = ESP_OK;
	int header_length = 0;
	int content_length = 0;
	int body_index = 0;
	int body_length = 0;
	int content_index = 0;
	bool header_detect = false;

	/* Read HTTP response */
	while(1) {
		char recv_buf[64];
		int recv_bytes;
		bzero(recv_buf, sizeof(recv_buf));
		recv_bytes = read(sock, recv_buf, sizeof(recv_buf)-1);
		if (receive_print) {
			for(int i = 0; i < recv_bytes; i++) {
				putchar(recv_buf[i]);
			}
			printf("\n");
		}

		http_header_analysis(&header_detect, recv_buf, recv_bytes, header_buffer, &header_length, &content_length, &body_index, &body_length);
		ESP_LOGD(TAG, "header_detect=%d content_length=%d body_index=%d body_length=%d", header_detect, content_length, body_index, body_length);
		if (header_detect) {
			if (content_length) {
				if (body_length > 0) {
					content_index += body_length;
					ESP_LOGD(TAG, "content_index=%d", content_index);
					if (content_index == content_length) break;
				}
			} else {
				break;
			}
		}
	} // end while

	ESP_LOGI(TAG, "... done reading from socket. header_length=%d content_length=%d", header_length, content_length);
#if 0
	for(int i = 0; i < content_length; i++) {
		putchar(content_buffer[i]);
	}
	printf("\n");
#endif
	free(header_buffer);
	return return_code;
} // http_webdav_mkcol

int getFileSize(char *fullPath);
void printDirectory(char * path);

esp_err_t http_webdav_text_put(int sock, char *local, char *path, bool receive_print) {
	// Allocate header
	char *header_buffer = malloc(HEADER_SIZE);
	if (header_buffer == NULL) {
		ESP_LOGE(TAG, "header_buffer malloc fail");
		return ESP_FAIL;
	}
	memset(header_buffer, 0, HEADER_SIZE);

	// Get local file size
	int file_size = getFileSize(local);
	ESP_LOGI(TAG, "local=[%s] file_size=%d", local, file_size);
	if (file_size < 0) {
		ESP_LOGE(TAG, "invalid file size");
		return ESP_FAIL;
	}

	// Open local file
	FILE* fp = fopen(local, "rb");
	if (fp == NULL) {
		ESP_LOGE(TAG, "Failed to open file for reading");
		return ESP_FAIL;
	}

	// Setup request header
	char request[257];
	http_request_set(request, 256, "PUT", path, file_size, "Content-Type: application/x-www-form-urlencoded");

	// Send request
	if (write(sock, request, strlen(request)) < 0) {
		ESP_LOGE(TAG, "... socket send failed");
		return ESP_FAIL;
	}
	ESP_LOGD(TAG, "... socket send success");

	// Send body
	uint8_t buffer[512];
	int total_read = 0;
	while(1) {
		size_t read_bytes = fread(buffer, 1, sizeof(buffer), fp);
		ESP_LOGD(TAG, "fread read_bytes=%d", read_bytes);
		if (read_bytes == 0) break;
		if (write(sock, buffer, read_bytes) < 0) {
			ESP_LOGE(TAG, "... socket send failed");
			return ESP_FAIL;
		}
		total_read += read_bytes;
	}
	fclose(fp);
	ESP_LOGI(TAG, "File read done. total_read=%d", total_read);

	esp_err_t return_code = ESP_OK;
	int header_length = 0;
	int content_length = 0;
	int body_index = 0;
	int body_length = 0;
	int content_index = 0;
	bool header_detect = false;

	/* Read HTTP response */
	while(1) {
		char recv_buf[64];
		int recv_bytes;
		bzero(recv_buf, sizeof(recv_buf));
		recv_bytes = read(sock, recv_buf, sizeof(recv_buf)-1);
		if (receive_print) {
			for(int i = 0; i < recv_bytes; i++) {
				putchar(recv_buf[i]);
			}
			printf("\n");
		}

		http_header_analysis(&header_detect, recv_buf, recv_bytes, header_buffer, &header_length, &content_length, &body_index, &body_length);
		ESP_LOGD(TAG, "header_detect=%d content_length=%d body_index=%d body_length=%d", header_detect, content_length, body_index, body_length);
		if (header_detect) {
			if (content_length) {
				if (body_length > 0) {
					content_index += body_length;
					ESP_LOGD(TAG, "content_index=%d", content_index);
					if (content_index == content_length) break;
				}
			} else {
				break;
			}
		}
	} // end while

	ESP_LOGI(TAG, "... done reading from socket. header_length=%d content_length=%d", header_length, content_length);
#if 0
	for(int i = 0; i < content_length; i++) {
		putchar(content_buffer[i]);
	}
	printf("\n");
#endif
	free(header_buffer);
	return return_code;
} // http_webdav_text_put


esp_err_t http_webdav_binary_put(int sock, char *local, char *path, bool receive_print) {
	// Allocate header
	char *header_buffer = malloc(HEADER_SIZE);
	if (header_buffer == NULL) {
		ESP_LOGE(TAG, "header_buffer malloc fail");
		return ESP_FAIL;
	}
	memset(header_buffer, 0, HEADER_SIZE);

	// Get local file size
	int file_size = getFileSize(local);
	ESP_LOGI(TAG, "local=[%s] file_size=%d", local, file_size);
	if (file_size < 0) {
		ESP_LOGE(TAG, "invalid file size");
		return ESP_FAIL;
	}

	// Setup request header
	char request[513];
	char extra_header[128];
	strcpy(extra_header, "Accept: */*\r\nContent-Type: application/x-www-form-urlencoded\r\nExpect: 100-continue");
	http_request_set(request, 512, "PUT", path, file_size, extra_header);

	// Send request
	if (write(sock, request, strlen(request)) < 0) {
		ESP_LOGE(TAG, "... socket send failed");
		return ESP_FAIL;
	}
	ESP_LOGD(TAG, "... socket send success");

	/* Read HTTP response */
	char recv_buf[128];
	int recv_bytes;
	bzero(recv_buf, sizeof(recv_buf));
	recv_bytes = read(sock, recv_buf, sizeof(recv_buf)-1);
	if (receive_print) {
		for(int i = 0; i < recv_bytes; i++) {
			putchar(recv_buf[i]);
		}
		printf("\n");
	}

	ESP_LOGI(TAG, "recv_buf=[%s]", recv_buf);
	if (strncmp(recv_buf, "HTTP/1.1 100 Continue", 21) !=0) {
		return ESP_FAIL;
	}

	// Open local file
	FILE* fp = fopen(local, "rb");
	if (fp == NULL) {
		ESP_LOGE(TAG, "Failed to open file for reading");
		return ESP_FAIL;
	}

	// Send body
	uint8_t buffer[512];
	int total_read = 0;
	while(1) {
		size_t read_bytes = fread(buffer, 1, sizeof(buffer), fp);
		ESP_LOGD(TAG, "fread read_bytes=%d", read_bytes);
		if (read_bytes == 0) break;
		if (write(sock, buffer, read_bytes) < 0) {
			ESP_LOGE(TAG, "... socket send failed");
			return ESP_FAIL;
		}
		total_read += read_bytes;
	}
	fclose(fp);
	ESP_LOGI(TAG, "File read done. total_read=%d", total_read);

	esp_err_t return_code = ESP_OK;
	int header_length = 0;
	int content_length = 0;
	int body_index = 0;
	int body_length = 0;
	bool header_detect = false;

	/* Read HTTP response */
	while(1) {
		char recv_buf[64];
		int recv_bytes;
		bzero(recv_buf, sizeof(recv_buf));
		recv_bytes = read(sock, recv_buf, sizeof(recv_buf)-1);
		if (receive_print) {
			for(int i = 0; i < recv_bytes; i++) {
				putchar(recv_buf[i]);
			}
			printf("\n");
		}

		http_header_analysis(&header_detect, recv_buf, recv_bytes, header_buffer, &header_length, &content_length, &body_index, &body_length);
		ESP_LOGI(TAG, "header_detect=%d content_length=%d body_index=%d body_length=%d", header_detect, content_length, body_index, body_length);
		if (header_detect) break;
	} // end while

	free(header_buffer);
	return return_code;
} // http_webdav_binary_put



esp_err_t http_webdav_get(int sock, char *local, char *path, bool receive_print) {
	// Allocate header
	char *header_buffer = malloc(HEADER_SIZE);
	if (header_buffer == NULL) {
		ESP_LOGE(TAG, "header_buffer malloc fail");
		return ESP_FAIL;
	}
	memset(header_buffer, 0, HEADER_SIZE);

	// Open local file
	FILE* fp = fopen(local, "wb");
	if (fp == NULL) {
		ESP_LOGE(TAG, "Failed to open file for writing");
		return ESP_FAIL;
	}

	// Setup request header
	char request[257];
	http_request_set(request, 256, "GET", path, 0, "");

	// Send request
	if (write(sock, request, strlen(request)) < 0) {
		ESP_LOGE(TAG, "... socket send failed");
		return ESP_FAIL;
	}
	ESP_LOGD(TAG, "... socket send success");

	esp_err_t return_code = ESP_OK;
	int header_length = 0;
	int content_length = 0;
	int body_index = 0;
	int body_length = 0;
	int content_index = 0;
	bool header_detect = false;

	/* Read HTTP response */
	while(1) {
		char recv_buf[64];
		int recv_bytes;
		bzero(recv_buf, sizeof(recv_buf));
		recv_bytes = read(sock, recv_buf, sizeof(recv_buf)-1);
		if (receive_print) {
			for(int i = 0; i < recv_bytes; i++) {
				putchar(recv_buf[i]);
			}
			printf("\n");
		}

		http_header_analysis(&header_detect, recv_buf, recv_bytes, header_buffer, &header_length, &content_length, &body_index, &body_length);
		ESP_LOGD(TAG, "header_detect=%d content_length=%d body_index=%d body_length=%d", header_detect, content_length, body_index, body_length);
		if (header_detect) {
			if (body_length > 0) {
				size_t write_bytes = fwrite(&recv_buf[body_index], 1, body_length, fp);
				ESP_LOGD(TAG, "write_bytes=%d body_length=%d", write_bytes, body_length);
				if (write_bytes != body_length) {
					ESP_LOGE(TAG, "fwrite fail. write_bytes=%d body_length=%d", write_bytes, body_length);
					return_code = ESP_FAIL;
					break;
				}
				content_index += body_length;
				ESP_LOGD(TAG, "content_index=%d", content_index);
				if (content_index == content_length) break;
			}
		}
	} // end while

	ESP_LOGI(TAG, "... done reading from socket. header_length=%d content_length=%d", header_length, content_length);
	free(header_buffer);
	fclose(fp);
	printDirectory(mount_point);
	return return_code;
} // http_webdav_get

esp_err_t http_webdav_copy(int sock, char *src, char *dst, bool overwrite, bool receive_print) {
	// Allocate header
	char *header_buffer = malloc(HEADER_SIZE);
	if (header_buffer == NULL) {
		ESP_LOGE(TAG, "header_buffer malloc fail");
		return ESP_FAIL;
	}
	memset(header_buffer, 0, HEADER_SIZE);

	// Setup request header
	char request[257];
	char extra_header[65];
	snprintf(extra_header, sizeof(extra_header)-1, "Destination: %s", dst);
	if (overwrite) {
		strcat(extra_header, "\r\nOverwrite: T");
	}
	http_request_set(request, 256, "COPY", src, 0, extra_header);

	// Send request
	if (write(sock, request, strlen(request)) < 0) {
		ESP_LOGE(TAG, "... socket send failed");
		return ESP_FAIL;
	}
	ESP_LOGD(TAG, "... socket send success");

	esp_err_t return_code = ESP_OK;
	int header_length = 0;
	int content_length = 0;
	int body_index = 0;
	int body_length = 0;
	int content_index = 0;
	bool header_detect = false;

	/* Read HTTP response */
	while(1) {
		char recv_buf[64];
		int recv_bytes;
		bzero(recv_buf, sizeof(recv_buf));
		recv_bytes = read(sock, recv_buf, sizeof(recv_buf)-1);
		if (receive_print) {
			for(int i = 0; i < recv_bytes; i++) {
				putchar(recv_buf[i]);
			}
			printf("\n");
		}

		http_header_analysis(&header_detect, recv_buf, recv_bytes, header_buffer, &header_length, &content_length, &body_index, &body_length);
		ESP_LOGD(TAG, "header_detect=%d content_length=%d body_index=%d body_length=%d", header_detect, content_length, body_index, body_length);
		if (header_detect) {
			if (content_length) {
				if (body_length > 0) {
					content_index += body_length;
					ESP_LOGD(TAG, "content_index=%d", content_index);
					if (content_index == content_length) break;
				}
			} else {
				break;
			}
		}
	} // end while

	ESP_LOGI(TAG, "... done reading from socket. header_length=%d content_length=%d", header_length, content_length);
#if 0
	for(int i = 0; i < content_length; i++) {
		putchar(content_buffer[i]);
	}
	printf("\n");
#endif
	free(header_buffer);
	return return_code;
} // http_webdav_copy

esp_err_t http_webdav_move(int sock, char *src, char *dst, bool overwrite, bool receive_print) {
	// Allocate header
	char *header_buffer = malloc(HEADER_SIZE);
	if (header_buffer == NULL) {
		ESP_LOGE(TAG, "header_buffer malloc fail");
		return ESP_FAIL;
	}
	memset(header_buffer, 0, HEADER_SIZE);

	// Setup request header
	char request[257];
	char extra_header[65];
	snprintf(extra_header, sizeof(extra_header)-1, "Destination: %s", dst);
	if (overwrite) {
		strcat(extra_header, "\r\nOverwrite: T");
	}
	http_request_set(request, 256, "MOVE", src, 0, extra_header);

	// Send request
	if (write(sock, request, strlen(request)) < 0) {
		ESP_LOGE(TAG, "... socket send failed");
		return ESP_FAIL;
	}
	ESP_LOGD(TAG, "... socket send success");

	esp_err_t return_code = ESP_OK;
	int header_length = 0;
	int content_length = 0;
	int body_index = 0;
	int body_length = 0;
	int content_index = 0;
	bool header_detect = false;

	/* Read HTTP response */
	while(1) {
		char recv_buf[64];
		int recv_bytes;
		bzero(recv_buf, sizeof(recv_buf));
		recv_bytes = read(sock, recv_buf, sizeof(recv_buf)-1);
		if (receive_print) {
			for(int i = 0; i < recv_bytes; i++) {
				putchar(recv_buf[i]);
			}
			printf("\n");
		}

		http_header_analysis(&header_detect, recv_buf, recv_bytes, header_buffer, &header_length, &content_length, &body_index, &body_length);
		ESP_LOGD(TAG, "header_detect=%d content_length=%d body_index=%d body_length=%d", header_detect, content_length, body_index, body_length);
		if (header_detect) {
			if (content_length) {
				if (body_length > 0) {
					content_index += body_length;
					ESP_LOGD(TAG, "content_index=%d", content_index);
					if (content_index == content_length) break;
				}
			} else {
				break;
			}
		}
	} // end while

	ESP_LOGI(TAG, "... done reading from socket. header_length=%d content_length=%d", header_length, content_length);
#if 0
	for(int i = 0; i < content_length; i++) {
		putchar(content_buffer[i]);
	}
	printf("\n");
#endif
	free(header_buffer);
	return return_code;
} // http_webdav_move

esp_err_t http_webdav_delete(int sock, char * path, bool receive_print) {
	// Allocate header
	char *header_buffer = malloc(HEADER_SIZE);
	if (header_buffer == NULL) {
		ESP_LOGE(TAG, "header_buffer malloc fail");
		return ESP_FAIL;
	}
	memset(header_buffer, 0, HEADER_SIZE);

	// Setup request header
	char request[257];
	http_request_set(request, 256, "DELETE", path, 0, "");

	// Send request
	if (write(sock, request, strlen(request)) < 0) {
		ESP_LOGE(TAG, "... socket send failed");
		return ESP_FAIL;
	}
	ESP_LOGD(TAG, "... socket send success");

	esp_err_t return_code = ESP_OK;
	int header_length = 0;
	int content_length = 0;
	int body_index = 0;
	int body_length = 0;
	bool header_detect = false;

	/* Read HTTP response */
	while(1) {
		char recv_buf[64];
		int recv_bytes;
		bzero(recv_buf, sizeof(recv_buf));
		recv_bytes = read(sock, recv_buf, sizeof(recv_buf)-1);
		if (receive_print) {
			for(int i = 0; i < recv_bytes; i++) {
				putchar(recv_buf[i]);
			}
			printf("\n");
		}

		http_header_analysis(&header_detect, recv_buf, recv_bytes, header_buffer, &header_length, &content_length, &body_index, &body_length);
		ESP_LOGD(TAG, "header_detect=%d content_length=%d body_index=%d body_length=%d", header_detect, content_length, body_index, body_length);
		if (header_detect) break;
	} // end while

	ESP_LOGI(TAG, "... done reading from socket. header_length=%d content_length=%d", header_length, content_length);
	free(header_buffer);
	return return_code;
} // http_webdav_delete

void http_task(void *pvParameters)
{
	const struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};
	struct addrinfo *res;
	struct in_addr *addr;

	char port[11];
	snprintf(port, sizeof(port)-1, "%d", CONFIG_HTTP_PORT);
	
	int err = getaddrinfo(CONFIG_HTTP_ENDPOINT, port, &hints, &res);
	if(err != 0 || res == NULL) {
		ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
		vTaskDelete(NULL);
	}

	/* Code to print the resolved IP.
	   Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
	addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
	ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

#if 0
	int sock = socket(res->ai_family, res->ai_socktype, 0);
	if(sock < 0) {
		ESP_LOGE(TAG, "... Failed to allocate socket.");
		vTaskDelete(NULL);
	}
	ESP_LOGI(TAG, "... allocated socket");

	if(connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
		ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
		close(sock);
		vTaskDelete(NULL);
	}
	ESP_LOGI(TAG, "... connected. sock=%d", sock);
#endif

	int sock;

	// Perfome MKCOL
	wait_enter("Creating new foder on Webdav Server. Press Enter when ready.");
	sock = connect_server(res);
	http_webdav_mkcol(sock, "/new_folder", true);
	http_webdav_propfind(sock, false);
	close(sock);

	// Perfome Text PUT
	wait_enter("Creating new text file on Webdav Server. Press Enter when ready.");
	char local[32];
	strcpy(local, "/spiffs/test.txt");
	sock = connect_server(res);
	http_webdav_text_put(sock, local, "/new_folder/file.txt", true);
	http_webdav_propfind(sock, false);
	close(sock);

	// Perfome Binary PUT
	wait_enter("Creating new binary file on Webdav Server. Press Enter when ready.");
	strcpy(local, "/spiffs/esp32.jpeg");
	sock = connect_server(res);
	http_webdav_binary_put(sock, local, "/new_folder/esp32.jpeg", true);
	http_webdav_propfind(sock, false);
	close(sock);

	// Perfome GET
	wait_enter("Geting file on Webdav Server. Press Enter when ready.");
	strcpy(local, "/spiffs/test2.txt");
	sock = connect_server(res);
	http_webdav_get(sock, local, "/new_folder/file.txt", true);
	close(sock);

	// Perfome COPY file
	wait_enter("Copying file on Webdav Server. Press Enter when ready.");
	bool overwrite = false;
	sock = connect_server(res);
	http_webdav_copy(sock, "/new_folder/file.txt", "/new_folder/file2.txt", overwrite, true);
	http_webdav_propfind(sock, false);
	close(sock);

	// Perfome MOVE file
	wait_enter("Moveing file on Webdav Server. Press Enter when ready.");
	sock = connect_server(res);
	http_webdav_move(sock, "/new_folder/file.txt", "/new_folder/file3.txt", overwrite, true);
	http_webdav_propfind(sock, false);
	close(sock);

	// Perfome COPY folder
	wait_enter("Copying folder on Webdav Server. Press Enter when ready.");
	sock = connect_server(res);
	http_webdav_copy(sock, "/new_folder", "/copy_folder", overwrite, true);
	http_webdav_propfind(sock, false);
	close(sock);

	// Perfome DELETE file
	wait_enter("Deleting file on Webdav Server. Press Enter when ready.");
	sock = connect_server(res);
	http_webdav_delete(sock, "/new_folder/file3.txt", true);
	http_webdav_propfind(sock, false);
	close(sock);

	// Perfome DELETE folder
	wait_enter("Deleting folder on Webdav Server. Press Enter when ready.");
	sock = connect_server(res);
	http_webdav_delete(sock, "/new_folder", true);
	http_webdav_delete(sock, "/copy_folder", true);
	http_webdav_propfind(sock, false);
	close(sock);

	freeaddrinfo(res);
	ESP_LOGI(TAG, "All done");

	while(1) {
		wait_enter("");
	}
	vTaskDelete(NULL);
}
