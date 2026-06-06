#ifndef VIDEO_H
#define VIDEO_H
//定义视频端口号的端口号和地址

#define VIDEOPORT 8080
#define VIDEOADDR "127.0.0.1"

//公网服务器的端口号和地址
#define SERVER_PORT  8000
#define SERVER_ADDR "106.12.26.151"

#define BUFLEN  100*1024
#define UDP_MAGIC 0x53435631u
#define UDP_DATA_SIZE 1200

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
