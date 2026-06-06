#ifndef APP_H
#define APP_H

#include "cJSON.h"

void app_send_video_data(int fd);
void app_send_control_info(cJSON *obj);

#endif
