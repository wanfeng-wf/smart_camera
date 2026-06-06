#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include "camera.h"
#include <unistd.h>
#include "node.h"

extern pthread_mutex_t mutex;
extern char *pic_data;
extern int pic_length;
extern unsigned int pic_id;
extern Node *head;

void *app_video_data(void *arg)
{
	int app_port = *(int *)arg;
	unsigned int frame_id = 1;
	unsigned int last_pic_id = 0;
	free(arg);

	// 创建udp socket
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (-1 == sockfd) {
		perror("app udp socket error");
		return NULL;
	}

	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(app_port);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		perror("bind error app aliserver");
		return NULL;
	}

	// 获取app发送过来的数据
	char *buf = (char *)malloc(BUFLEN);
	struct sockaddr_in app_addr;
	int length = sizeof(app_addr);
	printf("等待app UDP hello, port=%d\n", app_port);
	recvfrom(sockfd, buf, 16, 0, (struct sockaddr *)&app_addr, (socklen_t *)&length);
	printf("收到app UDP hello: %s:%d\n", inet_ntoa(app_addr.sin_addr), ntohs(app_addr.sin_port));
	while (1) {
		int send_len;
		unsigned int current_pic_id;

		// 共享内存上锁
		pthread_mutex_lock(&mutex);
		send_len = pic_length;
		current_pic_id = pic_id;
		if (send_len > 0 && current_pic_id != last_pic_id) {
			memcpy(buf, pic_data, send_len);
		}
		pthread_mutex_unlock(&mutex);

		if (send_len > 0 && current_pic_id != last_pic_id) {
			int offset = 0;

			while (offset < send_len) {
				unsigned char packet[sizeof(struct jpeg_udp_head) + UDP_DATA_SIZE];
				struct jpeg_udp_head head;
				int data_size = send_len - offset;

				if (data_size > UDP_DATA_SIZE) {
					data_size = UDP_DATA_SIZE;
				}

				head.magic = htonl(UDP_MAGIC);
				head.frame_id = htonl(frame_id);
				head.frame_size = htonl(send_len);
				head.offset = htonl(offset);
				head.data_size = htonl(data_size);

				memcpy(packet, &head, sizeof(head));
				memcpy(packet + sizeof(head), buf + offset, data_size);

				if (sendto(sockfd, packet, sizeof(head) + data_size, 0,
				           (struct sockaddr *)&app_addr, length) < 0) {
					perror("send picture to app error");
				}

				offset += data_size;
				usleep(1000);
			}

			frame_id++;
			last_pic_id = current_pic_id;
		}
		usleep(150000);
	}
}

void app_send_video_data(int fd)
{
	int *app_port = malloc(sizeof(int));
	*app_port = APP_VIDEO_PORT;

	// 启动线程 创建udp socket
	pthread_t tid;
	pthread_create(&tid, NULL, app_video_data, app_port);

	// 返回端口信息，json类型
	cJSON *obj = cJSON_CreateObject();
	cJSON_AddStringToObject(obj, "cmd", "reply_port_info");
	cJSON_AddNumberToObject(obj, "port", APP_VIDEO_PORT);
	char *s = cJSON_PrintUnformatted(obj);

	if (send(fd, s, strlen(s), 0) < 0) {
		perror("send to app error");
		cJSON_free(s);
		cJSON_Delete(obj);
		return;
	}
	cJSON_free(s);
	cJSON_Delete(obj);
}

void app_send_control_info(cJSON *obj)
{
	cJSON *val = cJSON_GetObjectItem(obj, "deviceid");
	const char *deviceid = cJSON_GetStringValue(val);

	int fd = TraverseLink(deviceid);
	if (-1 == fd) {
		printf("摄像头不存在\n");
		return;
	}

	// 服务器将接收到的json格式控制命令解析后转发到对应的摄像头；
	char *s = cJSON_PrintUnformatted(obj);
	if (send(fd, s, strlen(s), 0) < 0) {
		printf("服务器转发控制指令失败\n");
	}
	cJSON_free(s);
}
