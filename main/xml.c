/* The example of ESP-IDF
 *
 * This sample code is in the public domain.
 */

#include <expat.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "XML";

typedef struct {
	int depth; // XML depth
	char tag[128]; // XML tag
} USER_DATA_t;

static void XMLCALL start_element(void *userData, const XML_Char *name, const XML_Char **atts)
{
	ESP_LOGD(TAG, "start_element name=%s", name);
	USER_DATA_t *user_data = (USER_DATA_t *) userData;
	int depth = user_data->depth;
	if (depth == 0) {
		strcpy(user_data->tag, name);
	} else {
		strcat(user_data->tag, "/");
		strcat(user_data->tag, name);
	}
	++user_data->depth;
}

static void XMLCALL end_element(void *userData, const XML_Char *name)
{
	ESP_LOGD(TAG, "end_element name[%d]=%s", strlen(name), name);
	USER_DATA_t *user_data = (USER_DATA_t *) userData;
	int tagLen = strlen(user_data->tag);
	int offset = tagLen - strlen(name) -1;
	user_data->tag[offset] = 0;
	ESP_LOGD(TAG, "tag=[%s]", user_data->tag);
	//int depth = user_data->depth;
	--user_data->depth;
}

static void data_handler(void *userData, const XML_Char *s, int len)
{
	USER_DATA_t *user_data = (USER_DATA_t *) userData;
	//int depth = user_data->depth;
	if (len == 1 && s[0] == 0x0a) return;
	
	int nonSpace = 0;
	for (int i=0;i<len; i++) {
		if (s[i] != 0x20) nonSpace++;
	}
	if (nonSpace == 0) return;

	ESP_LOGD(TAG, "tag=[%s]", user_data->tag);
	ESP_LOGD(TAG, "depth=%d len=%d s=[%.*s]", user_data->depth, len, len, s);
	ESP_LOGD(TAG, "element=[%.*s]", len, s);
	ESP_LOGI(TAG, "[%s]=[%.*s]", user_data->tag, len, s);
	ESP_LOG_BUFFER_HEXDUMP(TAG, s, len, ESP_LOG_DEBUG);
}

esp_err_t parse_xml(char *buffer, int buffer_length) {
	ESP_LOGI(TAG, "buffer_length=%d", buffer_length);
	USER_DATA_t userData;
	userData.depth = 0;
	memset(userData.tag, 0, sizeof(userData.tag));

	// Parse XML
	XML_Parser parser = XML_ParserCreate(NULL);
	XML_SetUserData(parser, &userData);
	XML_SetElementHandler(parser, start_element, end_element);
	XML_SetCharacterDataHandler(parser, data_handler);
	if (XML_Parse(parser, buffer, buffer_length, 1) != XML_STATUS_OK) {
		ESP_LOGE(TAG, "XML_Parse fail");
		XML_ParserFree(parser);
		return ESP_FAIL;
	}
	XML_ParserFree(parser);
	return ESP_OK;
}
