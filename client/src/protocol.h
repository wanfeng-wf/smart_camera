#ifndef PROTOCOL_H
#define PROTOCOL_H

#define PROTOCOL_DEVICE_ID_SIZE 32
#define PROTOCOL_CMD_SIZE 32
#define PROTOCOL_ACTION_SIZE 32

typedef enum {
	PROTOCOL_PARSE_OK = 0,
	PROTOCOL_PARSE_INVALID_JSON,
	PROTOCOL_PARSE_INVALID_CMD,
	PROTOCOL_PARSE_INVALID_ACTION,
} protocol_parse_result_t;

typedef enum {
	PROTOCOL_MSG_UNKNOWN = 0,
	PROTOCOL_MSG_ONLINE_OK,
	PROTOCOL_MSG_CONTROL,
} protocol_msg_type_t;

typedef enum {
	PROTOCOL_ACTION_UNKNOWN = 0,
	PROTOCOL_ACTION_LEFT,
	PROTOCOL_ACTION_RIGHT,
	PROTOCOL_ACTION_UP,
	PROTOCOL_ACTION_DOWN,
} protocol_action_t;

typedef struct {
	protocol_msg_type_t type;
	protocol_action_t action_type;
	char cmd[PROTOCOL_CMD_SIZE];
	char deviceid[PROTOCOL_DEVICE_ID_SIZE];
	char action[PROTOCOL_ACTION_SIZE];
} protocol_msg_t;

char *protocol_build_online_info(const char *deviceid);
void protocol_free(char *message);

protocol_parse_result_t protocol_parse_message(const char *json,
					       protocol_msg_t *message);

#endif
