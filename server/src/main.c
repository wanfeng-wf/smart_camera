#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include "cJSON.h"
#include "node.h"
#include "camera.h"
#include "app.h"

#define SERVERPORT 8000

Node *head = NULL;

int pic_length;
unsigned int pic_id;
pthread_mutex_t mutex;
char *pic_data;

void *ClientHandler(void *arg)
{
	int fd = *(int *)arg;
	int recv_size;
	char buf[256] = {0};

	free(arg);
	while (1) {
		memset(buf, 0, sizeof(buf));
		recv_size = recv(fd, buf, sizeof(buf) - 1, 0);
		if (-1 == recv_size) {
			perror("error");
			break;
		} else if (recv_size == 0) {
			printf("客户端%d 异常下线\n", fd);
			break;
		}

		printf("TCP收到数据 %s \n", buf);
		cJSON *obj = cJSON_Parse(buf);
		cJSON *val = cJSON_GetObjectItem(obj, "cmd");
		const char *cmd = cJSON_GetStringValue(val);

		if (!strcmp(cmd, "info")) {
			camera_online(obj, fd);
		} else if (!strcmp(cmd, "get_video_data")) {
			app_send_video_data(fd);
		} else if (!strcmp(cmd, "control")) {
			app_send_control_info(obj);
		}

		cJSON_Delete(obj);
	}

	return NULL;
}

int main()
{
	InitLink();
	pic_data = (char *)malloc(BUFLEN);
	pthread_mutex_init(&mutex, NULL);
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == sockfd) {
		perror("socket");
		exit(1);
	}

	// 地址复用
	int opt = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVERPORT);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
		perror("bind error");
		exit(1);
	}

	if (listen(sockfd, 10) != 0) {
		perror("listen error");
		exit(1);
	}

	struct sockaddr_in client_addr;
	socklen_t length = sizeof(client_addr);
	while (1) {
		int *fd = malloc(sizeof(int));
		*fd = accept(sockfd, (struct sockaddr *)&client_addr, &length);
		if (*fd == -1) {
			perror("accept error");
			free(fd);
			continue;
		}

		pthread_t tid;
		pthread_create(&tid, NULL, ClientHandler, (void *)fd);
		pthread_detach(tid);
		usleep(10000);
	}

	return 0;
}
