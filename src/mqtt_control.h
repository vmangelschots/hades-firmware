#ifndef mqtt_control_h
#define mqtt_control_h
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

void init_mqtt(QueueHandle_t queue);
#endif