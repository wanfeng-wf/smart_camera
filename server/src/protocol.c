#include "protocol.h"
#include <string.h>
#include "cJSON.h"

static void protocol_copy_string(char *dst, size_t dst_size, const char *src)
{
	if (dst_size == 0) {
		return;
	}

	if (src == NULL) {
		dst[0] = '\0';
		return;
	}

	strncpy(dst, src, dst_size - 1);
	dst[dst_size - 1] = '\0';
}

char *protocol_build_message(const char *cmd, const char *key,
				    const char *value)
{
	cJSON *root = cJSON_CreateObject();
	if (root == NULL) {
		return NULL;
	}

	cJSON_AddStringToObject(root, "cmd", cmd);
	if (key != NULL) {
		cJSON_AddStringToObject(root, key, value);
	}

	char *json_str = cJSON_PrintUnformatted(root);
	cJSON_Delete(root);

	return json_str;
}

void protocol_free(char *message)
{
	cJSON_free(message);
}

protocol_parse_result_t protocol_parse_message(const char *json,
					       protocol_msg_t *message)
{
	cJSON *root;
	cJSON *cmd;

	if (json == NULL || message == NULL) {
		return PROTOCOL_PARSE_INVALID_JSON;
	}

	memset(message, 0, sizeof(*message));

	root = cJSON_Parse(json);
	if (root == NULL) {
		return PROTOCOL_PARSE_INVALID_JSON;
	}

	cmd = cJSON_GetObjectItem(root, "cmd");
	if (!cJSON_IsString(cmd)) {
		cJSON_Delete(root);
		return PROTOCOL_PARSE_INVALID_CMD;
	}

	protocol_copy_string(message->cmd, sizeof(message->cmd), cmd->valuestring);

	if (strcmp(cmd->valuestring, "info") == 0) {
		cJSON *deviceid = cJSON_GetObjectItem(root, "deviceid");

		message->type = PROTOCOL_MSG_INFO;
		if (cJSON_IsString(deviceid)) {
			protocol_copy_string(message->deviceid,
					     sizeof(message->deviceid),
					     deviceid->valuestring);
		}
	} else {
		message->type = PROTOCOL_MSG_UNKNOWN;
	}

	cJSON_Delete(root);
	return PROTOCOL_PARSE_OK;
}
