#ifndef HEATER_H
#define HEATER_H
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
// the maximum temperature difference between water_in and water_out
#define MAX_SENSOR_DIFF 5.0
#define RELAY_PIN 25



void init_heater(QueueHandle_t queue);
static void set_heater(int on);
#endif