#ifndef status_led_h
#define status_led_h
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#define WIFI_CONNECTED_BIT BIT0
#define HEATER_ON_BIT BIT1
#define ERROR_BIT BIT2
#define MQTT_CONNECTED_BIT BIT3
#define TEMPERATURE_SENSOR_INIT BIT4


EventGroupHandle_t * get_status_bits_handle();
void init_status_led();
#endif