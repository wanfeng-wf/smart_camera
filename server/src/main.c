#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "cJSON.h"

#define SERVER_PORT 8000
#define BUF_SIZE 1024

int main(void)
{
	int listenfd;
	int clientfd;
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	char buf[BUF_SIZE];

	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenfd < 0) {
		perror("socket");
		return -1;
	}

	int opt = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(listenfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		perror("bind");
		close(listenfd);
		return -1;
	}

	if (listen(listenfd, 10) < 0) {
		perror("listen");
		close(listenfd);
		return -1;
	}

	printf("Server listening on port %d ...\n", SERVER_PORT);

	clientfd = accept(listenfd, (struct sockaddr *)&client_addr, &client_len);
	if (clientfd < 0) {
		perror("accept");
		close(listenfd);
		return -1;
	}

	printf("Client connected: %s:%d\n", inet_ntoa(client_addr.sin_addr),
	       ntohs(client_addr.sin_port));

	while (1) {
        memset(buf, 0, sizeof(buf));
		int n = recv(clientfd, buf, sizeof(buf) - 1, 0);
		if (n < 0) {
			perror("recv");
			break;
		} else if (n == 0) {
			printf("Client disconnected.\n");
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

		if (strcmp(cmd->valuestring, "info") == 0) {
			cJSON *deviceid = cJSON_GetObjectItem(root, "deviceid");
			const char *deviceid_str = NULL;

			if (cJSON_IsString(deviceid)) {
				deviceid_str = deviceid->valuestring;
			}

			printf("Device online, deviceid=%s\n", deviceid_str);

			cJSON *reply = cJSON_CreateObject();
			cJSON_AddStringToObject(reply, "cmd", "online_ok");
			cJSON_AddStringToObject(reply, "deviceid", deviceid_str);

			char *reply_str = cJSON_PrintUnformatted(reply);
			if (reply_str) {
				if (send(clientfd, reply_str, strlen(reply_str), 0) < 0) {
					perror("send");
					cJSON_free(reply_str);
					continue;
				}
				printf("Send: %s\n", reply_str);
				cJSON_free(reply_str);
			}

			cJSON_Delete(reply);

			sleep(2);

			cJSON *control = cJSON_CreateObject();
			cJSON_AddStringToObject(control, "cmd", "control");
			cJSON_AddStringToObject(control, "action", "left");

			char *control_str = cJSON_PrintUnformatted(control);
			if (control_str) {
				if (send(clientfd, control_str, strlen(control_str), 0) < 0) {
					perror("send");
					cJSON_free(control_str);
					continue;
				}
				printf("Send: %s\n", control_str);
				cJSON_free(control_str);
			}

			cJSON_Delete(control);
		} else {
			printf("Unknown cmd: %s\n", cmd->valuestring);
		}

		cJSON_Delete(root);
	}

	close(clientfd);
	close(listenfd);
	return 0;
}