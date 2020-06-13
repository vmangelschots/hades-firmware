#ifndef temperature_sensor_h
#define temperature_sensor_h
#include "stdint.h"
#include "owb.h"

#define inside_rom " "
#define outside_rom " "
#define water_in_rom "f60315a279325628"
#define water_out_rom "a600000c43b9de28"


typedef struct temp_sensors_t temp_sensors_t;
typedef struct sensor_readings_t{
    char name[15];
    float reading;
    OneWireBus_ROMCode * rom_code;
} sensor_readings_t;

typedef enum sensor_slot {inside,outside,water_in,water_out,unkown_sensor} sensor_slot ;



void init_temperature_sensors(uint8_t owb_pin,QueueHandle_t queue);
float get_sensor_reading(sensor_slot slot);
#endif