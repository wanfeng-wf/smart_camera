#include <stdio.h>
#include <sys/types.h>          /* See NOTES */
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

void *send_video_data(void *arg)
{
	//接收端口号·
	int server_port=*(int *)arg;

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
	//连接成功打印一个方便调试
	printf("connect to mjpg-streamer success\n");

	//创建udp socket 发送视频数据；
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

	//发送post;
    char *buf = (char *)malloc(BUFLEN);
    memset(buf, 0, BUFLEN);
    strcpy(buf, "GET /?action=stream\n");
    if (send(video_sockfd, buf, strlen(buf),0) < 0) {
	    perror("send post");
	    exit(1);
    }

	//发送userinfo;任意两个字节
	if (send(video_sockfd,"f\n", 2, 0) < 0) {
		perror("send userinfo");
		exit(1);
	}
	memset(buf, 0, BUFLEN);
	if (recv(video_sockfd, buf, BUFLEN, 0) < 0) {
		perror("recv head info");

	}
	int recv_size;
	char *begin, *end;
	char cont_len[10] = {0};
	unsigned int frame_id = 1;
	while (1) {
		//接收头部信息
		memset(buf, 0, BUFLEN);
		memset(cont_len, 0, sizeof(cont_len));
		recv_size = recv(video_sockfd, buf, BUFLEN, 0);
		if(recv_size == -1) {
			perror("recv");

		}
		//解析获取的信息
		if(strstr(buf, "Content-Length")) {
			begin=strstr(buf,"Content-Length");
			end=strstr(buf,"X-Timestamp");
			memcpy(cont_len,begin+16 ,end-2-begin-16);
		} else {
			continue;
		}

		memset(buf, 0, BUFLEN);
		//接收帧数据
		int picture_size = atoi(cont_len);
		recv_size = 0;
		while (recv_size < picture_size) {
			int n = recv(video_sockfd, buf + recv_size, picture_size - recv_size, 0);
			if (n <= 0) {
				break;
			}
			recv_size += n;
		}

		//将一帧图片分片发送给公网服务器
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

		//接收尾部信息
		memset(buf, 0, BUFLEN);
		recv_size = recv(video_sockfd, buf, 1024, 0);
	}

}
