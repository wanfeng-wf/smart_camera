#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include "node.h"
#include "camera.h"

extern pthread_mutex_t mutex;
extern char *pic_data;
extern int pic_length;
extern unsigned int pic_id;

static long long get_time_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// 接收摄像头数据
void *camera_video_data(void *arg)
{
	int video_port = *(int *)arg;
	free(arg);

	// 同时启动udp socket,接收摄像头数据
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (-1 == sockfd) {
		perror("server udp socket error");
		return NULL;
	}

	struct sockaddr_in server_udp_addr;
	memset(&server_udp_addr, 0, sizeof(server_udp_addr));
	server_udp_addr.sin_family = AF_INET;
	server_udp_addr.sin_port = htons(video_port);
	server_udp_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	// 绑定
	if (bind(sockfd, (struct sockaddr *)&server_udp_addr, sizeof(server_udp_addr)) < 0) {
		perror("bind error");
		return NULL;
	}

	struct sockaddr_in camera_addr;
	int length = sizeof(camera_addr);
	int recv_size;
	int count = 0;
	unsigned char packet[sizeof(struct jpeg_udp_head) + UDP_DATA_SIZE];
	char *frame_buf = (char *)malloc(BUFLEN);
	unsigned char *packet_recv = (unsigned char *)calloc((BUFLEN + UDP_DATA_SIZE - 1) / UDP_DATA_SIZE, 1);
	unsigned int cur_frame_id = 0;
	unsigned int cur_frame_size = 0;
	unsigned int frame_recv_size = 0;
	unsigned int packet_count = 0;
	unsigned int recv_packet_count = 0;
	long long frame_start_ms = 0;

	printf("准备接收视频数据, port=%d\n", video_port);
	while (1) {
		recv_size =
		    recvfrom(sockfd, packet, sizeof(packet), 0, (struct sockaddr *)&camera_addr,
		             (socklen_t *)&length);
		if (recv_size < (int)sizeof(struct jpeg_udp_head)) {
			perror("recvfrom camera error");
			continue;
		}

		struct jpeg_udp_head head;
		memcpy(&head, packet, sizeof(head));
		head.magic = ntohl(head.magic);
		head.frame_id = ntohl(head.frame_id);
		head.frame_size = ntohl(head.frame_size);
		head.offset = ntohl(head.offset);
		head.data_size = ntohl(head.data_size);

		if (head.magic != UDP_MAGIC || head.frame_size > BUFLEN ||
		    head.data_size > UDP_DATA_SIZE ||
		    recv_size != (int)(sizeof(head) + head.data_size) ||
		    head.offset + head.data_size > head.frame_size ||
		    head.offset % UDP_DATA_SIZE != 0) {
			continue;
		}

		long long now_ms = get_time_ms();
		if (cur_frame_size > 0 && head.frame_id == cur_frame_id &&
		    now_ms - frame_start_ms > FRAME_TIMEOUT_MS) {
			cur_frame_size = 0;
			frame_recv_size = 0;
			recv_packet_count = 0;
			continue;
		}

		if (head.frame_id != cur_frame_id) {
			if (head.frame_id < cur_frame_id) {
				continue;
			}
			cur_frame_id = head.frame_id;
			cur_frame_size = head.frame_size;
			frame_recv_size = 0;
			packet_count = (head.frame_size + UDP_DATA_SIZE - 1) / UDP_DATA_SIZE;
			recv_packet_count = 0;
			memset(packet_recv, 0, (BUFLEN + UDP_DATA_SIZE - 1) / UDP_DATA_SIZE);
			frame_start_ms = now_ms;
		}

		unsigned int packet_index = head.offset / UDP_DATA_SIZE;
		if (head.frame_size != cur_frame_size || packet_index >= packet_count) {
			continue;
		}

		if (packet_recv[packet_index] == 0) {
			memcpy(frame_buf + head.offset, packet + sizeof(head), head.data_size);
			packet_recv[packet_index] = 1;
			frame_recv_size += head.data_size;
			recv_packet_count++;
		}

		if (frame_recv_size < cur_frame_size || recv_packet_count < packet_count) {
			continue;
		}

		pthread_mutex_lock(&mutex);
		memcpy(pic_data, frame_buf, cur_frame_size);
		pic_length = cur_frame_size;
		pic_id++;
		pthread_mutex_unlock(&mutex);

		count++;
		if (count % 30 == 0) {
			printf("收到完整图片 %u\n", cur_frame_size);
		}
		cur_frame_size = 0;
		frame_recv_size = 0;
		recv_packet_count = 0;
	}

	return NULL;
}

// 解析json数据得到deviceid;
// 操作链表，插入deviceid和fd;
// 返回摄像头端口信息
void camera_online(cJSON *json, int fd)
{
	int video_port = CAMERA_VIDEO_PORT;

	printf("进入摄像头上线处理程序\n");
	cJSON *val = cJSON_GetObjectItem(json, "deviceid");
	const char *dev_id = cJSON_GetStringValue(val);
	InsertLink(dev_id, fd);

	// 返回板端上传摄像头数据的UDP端口信息
	cJSON *obj = cJSON_CreateObject();
	cJSON_AddStringToObject(obj, "cmd", "port_info");
	cJSON_AddNumberToObject(obj, "port", video_port);

	char *s = cJSON_PrintUnformatted(obj);
	if (send(fd, s, strlen(s), 0) < 0) {
		perror("send to camera error");
		cJSON_free(s);
		cJSON_Delete(obj);
		exit(1);
	}
	cJSON_free(s);
	cJSON_Delete(obj);

	int *arg = malloc(sizeof(int));
	*arg = video_port;

	pthread_t tid;
	if (pthread_create(&tid, NULL, camera_video_data, arg) != 0) {
		perror("pthread_create camera_video_data");
		free(arg);
		return;
	}
	pthread_detach(tid);
}
