#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "protocol.h"

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
		protocol_msg_t message;
		protocol_parse_result_t parse_result;

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

		parse_result = protocol_parse_message(buf, &message);
		if (parse_result == PROTOCOL_PARSE_INVALID_JSON) {
			printf("Invalid JSON\n");
			return -1;
		}
		if (parse_result == PROTOCOL_PARSE_INVALID_CMD) {
			printf("JSON has no valid cmd field\n");
			return -1;
		}

		if (message.type == PROTOCOL_MSG_INFO) {
			const char *deviceid = message.deviceid;

			printf("Device online, deviceid=%s\n", deviceid);

			char *reply_str = protocol_build_message("online_ok", "deviceid", deviceid);
			if (reply_str) {
				if (send(clientfd, reply_str, strlen(reply_str), 0) < 0) {
					perror("send");
					protocol_free(reply_str);
					continue;
				}
				printf("Send: %s\n", reply_str);
				protocol_free(reply_str);
			}

			sleep(2);

			char *control_str = protocol_build_message("control", "action", "left");
			if (control_str) {
				if (send(clientfd, control_str, strlen(control_str), 0) < 0) {
					perror("send");
					protocol_free(control_str);
					continue;
				}
				printf("Send: %s\n", control_str);
				protocol_free(control_str);
			}
		} else {
			printf("Unknown cmd: %s\n", message.cmd);
		}
	}

	close(clientfd);
	close(listenfd);
	return 0;
}
