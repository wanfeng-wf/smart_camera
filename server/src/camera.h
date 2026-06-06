#ifndef CAMERA_H
#define CAMERA_H

#include "cJSON.h"

#define BUFLEN 1024*200
#define CAMERA_VIDEO_PORT 9000
#define APP_VIDEO_PORT 9001
#define UDP_MAGIC 0x53435631u
#define UDP_DATA_SIZE 1200

struct jpeg_udp_head {
	unsigned int magic;
	unsigned int frame_id;
	unsigned int frame_size;
	unsigned int offset;
	unsigned int data_size;
};

void camera_online(cJSON *json,int fd);

#endif
