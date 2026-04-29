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

static protocol_action_t protocol_parse_action(const char *action)
{
	if (strcmp(action, "left") == 0) {
		return PROTOCOL_ACTION_LEFT;
	}

	if (strcmp(action, "right") == 0) {
		return PROTOCOL_ACTION_RIGHT;
	}

	if (strcmp(action, "up") == 0) {
		return PROTOCOL_ACTION_UP;
	}

	if (strcmp(action, "down") == 0) {
		return PROTOCOL_ACTION_DOWN;
	}

	return PROTOCOL_ACTION_UNKNOWN;
}

char *protocol_build_online_info(const char *deviceid)
{
	cJSON *online = cJSON_CreateObject();
	if (online == NULL) {
		return NULL;
	}

	cJSON_AddStringToObject(online, "cmd", "info");
	cJSON_AddStringToObject(online, "deviceid", deviceid);

	char *json_str = cJSON_PrintUnformatted(online);
	cJSON_Delete(online);

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

	if (strcmp(cmd->valuestring, "online_ok") == 0) {
		cJSON *deviceid = cJSON_GetObjectItem(root, "deviceid");

		message->type = PROTOCOL_MSG_ONLINE_OK;
		if (cJSON_IsString(deviceid)) {
			protocol_copy_string(message->deviceid,
					     sizeof(message->deviceid),
					     deviceid->valuestring);
		}
	} else if (strcmp(cmd->valuestring, "control") == 0) {
		cJSON *action = cJSON_GetObjectItem(root, "action");

		if (!cJSON_IsString(action)) {
			cJSON_Delete(root);
			return PROTOCOL_PARSE_INVALID_ACTION;
		}

		message->type = PROTOCOL_MSG_CONTROL;
		message->action_type = protocol_parse_action(action->valuestring);
		protocol_copy_string(message->action, sizeof(message->action),
				     action->valuestring);
	} else {
		message->type = PROTOCOL_MSG_UNKNOWN;
	}

	cJSON_Delete(root);
	return PROTOCOL_PARSE_OK;
}
