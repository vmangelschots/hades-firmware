#ifndef mqtt_control_h
#define mqtt_control_h
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"


typedef struct status_message_t{
    char  topic[256];
    char  data[256];
} status_message_t;


void init_mqtt(QueueHandle_t queue,QueueHandle_t heater_status_queue);
#endif