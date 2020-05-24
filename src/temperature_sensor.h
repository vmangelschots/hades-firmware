#ifndef temperature_sensor_h
#define temperature_sensor_h
#include "stdint.h"



typedef struct temp_sensors_t temp_sensors_t;
typedef struct sensor_readings_t{
    char name[15];
    float reading;

} sensor_readings_t;
void init_temperature_sensors(uint8_t owb_pin,QueueHandle_t queue);

#endif