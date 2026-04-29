#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "protocol.h"

#define SERVER_IP "106.12.26.151"
#define SERVER_PORT 8000
#define BUF_SIZE 1024

int main(void)
{
	int sockfd;
	struct sockaddr_in server_addr;
	char buf[BUF_SIZE];

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("socket");
		return -1;
	}

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);

	if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
		perror("inet_pton");
		close(sockfd);
		return -1;
	}

	printf("Connecting to %s:%d ...\n", SERVER_IP, SERVER_PORT);

	if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		perror("connect");
		close(sockfd);
		return -1;
	}

	printf("Connected.\n");

	char *json_str = protocol_build_online_info("0001");
	if (json_str != NULL) {
		if (send(sockfd, json_str, strlen(json_str), 0) < 0) {
			perror("send");
			protocol_free(json_str);
			return -1;
		}
		printf("Send: %s\n", json_str);
		protocol_free(json_str);
	}

	while (1) {
		protocol_msg_t message;
		protocol_parse_result_t parse_result;

		memset(buf, 0, sizeof(buf));
		int n = recv(sockfd, buf, sizeof(buf) - 1, 0);

		if (n < 0) {
			perror("recv");
			break;
		} else if (n == 0) {
			printf("Server closed connection.\n");
			break;
		}

		buf[n] = '\0';
		printf("Recv: %s\n", buf);

		parse_result = protocol_parse_message(buf, &message);
		if (parse_result == PROTOCOL_PARSE_INVALID_JSON) {
			printf("Invalid JSON\n");
			return -1;
		}
		if (parse_result == PROTOCOL_PARSE_INVALID_CMD) {
			printf("JSON has no valid cmd field\n");
			return -1;
		}
		if (parse_result == PROTOCOL_PARSE_INVALID_ACTION) {
			printf("control message has no valid action field\n");
			return -1;
		}

		if (message.type == PROTOCOL_MSG_ONLINE_OK) {
			if (message.deviceid[0] == '\0') {
				printf("Online success\n");
			} else {
				printf("Online success, deviceid=%s\n", message.deviceid);
			}
		} else if (message.type == PROTOCOL_MSG_CONTROL) {
			printf("Control action: %s\n", message.action);

			if (message.action_type == PROTOCOL_ACTION_LEFT) {
				printf("TODO: motor left\n");
			} else if (message.action_type == PROTOCOL_ACTION_RIGHT) {
				printf("TODO: motor right\n");
			} else if (message.action_type == PROTOCOL_ACTION_UP) {
				printf("TODO: motor up\n");
			} else if (message.action_type == PROTOCOL_ACTION_DOWN) {
				printf("TODO: motor down\n");
			} else {
				printf("Unknown action\n");
			}
		} else {
			printf("Unknown cmd: %s\n", message.cmd);
		}
	}

	close(sockfd);
	return 0;
}
