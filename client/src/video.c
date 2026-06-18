#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include "video.h"

static pid_t mjpg_pid = -1;

int start_mjpg_streamer(void)
{
	pid_t pid;

	if (mjpg_pid > 0) {
		return 0;
	}

	pid = fork();
	if (pid < 0) {
		perror("fork mjpg_streamer");
		return -1;
	}

	if (pid == 0) {
		execlp("mjpg_streamer", "mjpg_streamer", "-i",
		       "input_uvc.so -d /dev/video0 -r 320x240 -f 10", "-o",
		       "output_http.so -p 8080", NULL);
		perror("execlp mjpg_streamer");
		_exit(127);
	}

	mjpg_pid = pid;
	sleep(2);
	printf("mjpg_streamer pid = %d\n", mjpg_pid);
	return 0;
}

void stop_mjpg_streamer(void)
{
	if (mjpg_pid <= 0) {
		return;
	}

	kill(mjpg_pid, SIGTERM);
	waitpid(mjpg_pid, NULL, 0);
	printf("mjpg_streamer stopped\n");
	mjpg_pid = -1;
}

static int read_line(int fd, char *buf, int size)
{
	int i = 0;
	char c;

	while (i < size - 1) {
		int n = recv(fd, &c, 1, 0);
		if (n <= 0) {
			return n;
		}

		buf[i++] = c;
		if (c == '\n') {
			break;
		}
	}

	buf[i] = '\0';
	return i;
}

static int read_jpeg_frame(int fd, char *jpeg_buf, int jpeg_buf_size, int *jpeg_size)
{
	char line[256];
	int content_length = 0;
	int recv_size = 0;

	// 先读到一个空行：第一次是HTTP响应头，后面是上一帧后的分隔行
	while (read_line(fd, line, sizeof(line)) > 0) {
		if (strcmp(line, "\r\n") == 0) {
			break;
		}
	}

	// 再读这一帧的MJPEG头，找到Content-Length
	while (read_line(fd, line, sizeof(line)) > 0) {
		if (strncmp(line, "Content-Length:", 15) == 0) {
			content_length = atoi(line + 15);
		}

		if (strcmp(line, "\r\n") == 0) {
			break;
		}
	}

	if (content_length <= 0 || content_length > jpeg_buf_size) {
		return -1;
	}

	while (recv_size < content_length) {
		int n = recv(fd, jpeg_buf + recv_size, content_length - recv_size, 0);
		if (n <= 0) {
			return -1;
		}
		recv_size += n;
	}

	*jpeg_size = content_length;
	return 0;
}

void *send_video_data(void *arg)
{
	// 接收udp端口号
	int server_port = *(int *)arg;

	//创建tcp socket
	int video_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (video_sockfd == -1) {
		perror("socket error");
		exit(1);
	}

	struct sockaddr_in video_addr;
	bzero(&video_addr, sizeof(video_addr));
	video_addr.sin_family = AF_INET ;
	video_addr.sin_port = htons(VIDEOPORT);
    inet_pton(AF_INET, VIDEOADDR, &video_addr.sin_addr);

	//发起连接
	if (connect(video_sockfd, (struct sockaddr *)&video_addr, sizeof(video_addr)) < 0) {
		perror("connect error");
		exit(1);
	}
	printf("connect to mjpg-streamer success\n");

	// 创建udp socket 发送视频数据；
	int server_sockfd = socket(AF_INET,SOCK_DGRAM,0);
    if (server_sockfd == -1) {
	    perror("udp socket error ");
	    exit(1);
    }
    struct sockaddr_in server_addr;
    bzero(&server_addr ,sizeof(server_addr));
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(server_port);
    inet_pton(AF_INET, SERVER_ADDR, &server_addr.sin_addr);

	// 发送post;
    char *buf = (char *)malloc(BUFLEN);
    memset(buf, 0, BUFLEN);
    strcpy(buf, "GET /?action=stream\n");
    if (send(video_sockfd, buf, strlen(buf),0) < 0) {
	    perror("send post");
	    exit(1);
    }

	// 发送userinfo;任意两个字节
	if (send(video_sockfd,"f\n", 2, 0) < 0) {
		perror("send userinfo");
		exit(1);
	}

	int recv_size;
	unsigned int frame_id = 1;
	while (1) {
		// 读取一帧图片
		if (read_jpeg_frame(video_sockfd, buf, BUFLEN, &recv_size) < 0) {
			printf("read jpeg frame failed\n");
			break;
		}

		//将一帧图片分片发送给服务器
		int offset = 0;
		while (offset < recv_size) {
			unsigned char packet[sizeof(struct jpeg_udp_head) + UDP_DATA_SIZE];
			struct jpeg_udp_head head;
			int data_size = recv_size - offset;

			if (data_size > UDP_DATA_SIZE) {
				data_size = UDP_DATA_SIZE;
			}

			head.magic = htonl(UDP_MAGIC);
			head.frame_id = htonl(frame_id);
			head.frame_size = htonl(recv_size);
			head.offset = htonl(offset);
			head.data_size = htonl(data_size);

			memcpy(packet, &head, sizeof(head));
			memcpy(packet + sizeof(head), buf + offset, data_size);

			if (sendto(server_sockfd, packet, sizeof(head) + data_size, 0,
			           (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
				perror("sendto server error");
			}

			offset += data_size;
			usleep(1000);
		}
		frame_id++;
	}

	close(server_sockfd);
	close(video_sockfd);
	free(buf);
	return NULL;
}
