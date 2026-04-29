#ifndef PROTOCOL_H
#define PROTOCOL_H

#define PROTOCOL_DEVICE_ID_SIZE 32
#define PROTOCOL_CMD_SIZE 32

typedef enum {
	PROTOCOL_PARSE_OK = 0,
	PROTOCOL_PARSE_INVALID_JSON,
	PROTOCOL_PARSE_INVALID_CMD,
} protocol_parse_result_t;

typedef enum {
	PROTOCOL_MSG_UNKNOWN = 0,
	PROTOCOL_MSG_INFO,
} protocol_msg_type_t;

typedef struct {
	protocol_msg_type_t type;
	char cmd[PROTOCOL_CMD_SIZE];
	char deviceid[PROTOCOL_DEVICE_ID_SIZE];
} protocol_msg_t;

char *protocol_build_message(const char *cmd, const char *key,
				    const char *value);
void protocol_free(char *message);

protocol_parse_result_t protocol_parse_message(const char *json,
					       protocol_msg_t *message);

#endif
