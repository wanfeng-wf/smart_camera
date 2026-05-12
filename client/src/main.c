#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "cJSON.h"

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

	cJSON *online = cJSON_CreateObject();
	cJSON_AddStringToObject(online, "cmd", "info");
	cJSON_AddStringToObject(online, "deviceid", "0001");

	char *json_str = cJSON_PrintUnformatted(online);
	if (json_str != NULL) {
        if (send(sockfd, json_str, strlen(json_str), 0) < 0) {
            perror("send");
            cJSON_free(json_str);
            return -1;
        }
		printf("Send: %s\n", json_str);
		cJSON_free(json_str);
	}

	cJSON_Delete(online);

	while (1) {
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

		cJSON *root = cJSON_Parse(buf);
		if (root == NULL) {
			printf("Invalid JSON\n");
			return -1;
		}

		cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
		if (!cJSON_IsString(cmd)) {
			printf("JSON has no valid cmd field\n");
			cJSON_Delete(root);
			return -1;
		}

		if (strcmp(cmd->valuestring, "online_ok") == 0) {
			cJSON *deviceid = cJSON_GetObjectItem(root, "deviceid");

			if (cJSON_IsString(deviceid)) {
				printf("Online success, deviceid=%s\n", deviceid->valuestring);
			} else {
				printf("Online success\n");
			}
		} else if (strcmp(cmd->valuestring, "control") == 0) {
			cJSON *action = cJSON_GetObjectItem(root, "action");

			if (!cJSON_IsString(action)) {
				printf("control message has no valid action field\n");
				cJSON_Delete(root);
				return -1;
			}

			printf("Control action: %s\n", action->valuestring);

			if (strcmp(action->valuestring, "left") == 0) {
				printf("TODO: motor left\n");
			} else if (strcmp(action->valuestring, "right") == 0) {
				printf("TODO: motor right\n");
			} else if (strcmp(action->valuestring, "up") == 0) {
				printf("TODO: motor up\n");
			} else if (strcmp(action->valuestring, "down") == 0) {
				printf("TODO: motor down\n");
			} else {
				printf("Unknown action\n");
			}
		} else {
			printf("Unknown cmd: %s\n", cmd->valuestring);
		}

		cJSON_Delete(root);
	}

	close(sockfd);
	return 0;
}
