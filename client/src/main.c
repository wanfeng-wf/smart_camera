#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "video.h"
#include "motor.h"
#include "cJSON.h"

int main()
{
	if (start_mjpg_streamer() < 0) {
		return -1;
	}

	// 第一步：创建客户端socket；
	int sockfd;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("sockfd error");
		return -1;
	}

	// 第二步：向服务器发起连接请求
	struct sockaddr_in myserveraddr;
	// int port = atoi(argv[2]);
	bzero(&myserveraddr, sizeof(myserveraddr));
	myserveraddr.sin_family = AF_INET;
	myserveraddr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_ADDR, &myserveraddr.sin_addr);
	if (connect(sockfd, (struct sockaddr *)&myserveraddr, sizeof(myserveraddr)) != 0) {
		perror("connect error");
		return -1;
	}
	// 创建json对象
	cJSON *obj = cJSON_CreateObject();
	cJSON_AddStringToObject(obj, "cmd", "info");
	cJSON_AddStringToObject(obj, "deviceid", "0001");// 简化直接写出
	char *s = cJSON_PrintUnformatted(obj);
	if (send(sockfd, s, strlen(s), 0) < 0) {
		perror("send error");
		cJSON_free(s);
		cJSON_Delete(obj);
		return -1;
	}
	cJSON_free(s);
	cJSON_Delete(obj);

	// 接收服务器的端口信息
	char buf[256] = {0};
	if (recv(sockfd, buf, sizeof(buf), 0) < 0) {
		perror("recv error");
		return -1;
	}
	cJSON *serverdata = cJSON_Parse(buf);

	cJSON *cmd = NULL;
	cJSON *port = NULL;

	int server_udp_port;
	cmd = cJSON_GetObjectItem(serverdata, "cmd");
	port = cJSON_GetObjectItem(serverdata, "port");
	if (cJSON_IsString(cmd) && !strcmp(cmd->valuestring, "port_info")) {
		server_udp_port = port->valueint;
		printf("recv port_info, udp port=%d\n", server_udp_port);
	} else {
		printf("recv error\n");
	}
	cJSON_Delete(serverdata);
	// 创建一个线程，通过send_video_data 函数发送视频数据；该函数需要知道udp的端口号
	pthread_t tid;
	pthread_create(&tid, NULL, send_video_data, &server_udp_port);
	pthread_detach(tid);
	while (1) {
		bzero(buf, sizeof(buf));
		if (recv(sockfd, buf, sizeof(buf), 0) < 0) {
			perror("recv error\n");
		}
		serverdata = cJSON_Parse(buf);

		cJSON *cmd = NULL;
		cJSON *action = NULL;

		cmd = cJSON_GetObjectItem(serverdata, "cmd");
		action = cJSON_GetObjectItem(serverdata, "action");

		if (cJSON_IsString(cmd) && !strcmp(cmd->valuestring, "control")) {
			const char *act = action->valuestring;
			if (!strcmp(act, "left")) {
				// 调动相应的函数让舵机转动起来
				motor_turn_left();
			} else if (!strcmp(act, "right")) {
				motor_turn_right();
			} else if (!strcmp(act, "up")) {
				motor_turn_up();
			} else if (!strcmp(act, "down")) {
				motor_turn_down();
			}
		}
		cJSON_Delete(serverdata);
	}
	stop_mjpg_streamer();
	return 0;
}
