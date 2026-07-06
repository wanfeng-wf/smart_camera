#ifndef VIDEO_H
#define VIDEO_H
//定义视频端口号的端口号和地址

#define VIDEOPORT 8080
#define VIDEOADDR "127.0.0.1"

//公网服务器的端口号和地址
#define SERVER_PORT  8000
#define SERVER_ADDR "106.12.26.151"

#define BUFLEN  (100 * 1024) // 100KB
#define MJPEG_RECV_BUF_SIZE (BUFLEN + 4096) // 104KB
#define UDP_MAGIC 0x53435631u // UDP 协议魔数,"SCV1"(Smart Camera Video v1)
#define UDP_DATA_SIZE 1200 // 1200 bytes

struct mjpeg_reader {
	int fd;
	char *buf;
	int len;
	int size;
};

struct jpeg_udp_head {
	unsigned int magic;
	unsigned int frame_id;
	unsigned int frame_size;
	unsigned int offset;
	unsigned int data_size;
};

int start_mjpg_streamer(void);
void stop_mjpg_streamer(void);
void *send_video_data(void *arg);


#endif
