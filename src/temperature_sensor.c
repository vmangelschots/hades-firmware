#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "temperature_sensor.h"
#include "owb.h"
#include "owb_rmt.h"
#include "ds18b20.h"
#include "esp_log.h"
#include "stdio.h"
#include "string.h"

#define MAX_DEVICES (4)
#define DS18B20_RESOLUTION   (DS18B20_RESOLUTION_10_BIT)
#define POLLING_INTERVAL 5000

#define TAG "temperature_sensor"

int unkown_name_counter=0;
float last_readings[4];
struct temp_sensors_t
{
    OneWireBus * owb;
    OneWireBus_ROMCode * rom_codes;   // a pointer to an *array* of ROM codes
    DS18B20_Info ** ds18b20_infos;
    char ** names;
    sensor_slot * slots;
    int num_ds18b20s;
};

typedef struct
{
    temp_sensors_t * sensors;
    QueueHandle_t queue;
} task_inputs_t;


static TaskHandle_t _task_handle = NULL;
/**
 * @brief Find all (or the first N) ROM codes for devices connected to the One Wire Bus.
 * @param[in] owb Pointer to initialised bus instance.
 * @param[in] devices A pre-existing array of ROM code structures, sufficient to hold max_devices entries.
 * @param[in] max_devices Maximum number of ROM codes to find.
 * @return Number of ROM codes found, or zero if no devices found.
 */
static int find_owb_rom_codes(const OneWireBus * owb, OneWireBus_ROMCode * rom_codes, int max_codes)
{
    ESP_LOGD(TAG, "%s", __FUNCTION__);
    int num_devices = 0;
    OneWireBus_SearchState search_state = {0};
    bool found = false;
    owb_search_first(owb, &search_state, &found);

    ESP_LOGI(TAG, "Find devices:");
    while (found && num_devices <= max_codes)
    {
        char rom_code_s[17];
        owb_string_from_rom_code(search_state.rom_code, rom_code_s, sizeof(rom_code_s));
        ESP_LOGI(TAG, "  %d : %s", num_devices, rom_code_s);
        rom_codes[num_devices] = search_state.rom_code;
        ++num_devices;
        owb_search_next(owb, &search_state, &found);
    }
    return num_devices;
}

static sensor_slot get_sensor_slot(OneWireBus_ROMCode code,char * name){
    sensor_slot slot = unkown_sensor;
    char * rom_code_str =calloc(sizeof(char),17);
    
    owb_string_from_rom_code(code, rom_code_str, sizeof(char)*17);
    if(strcmp(rom_code_str,water_in_rom)==0){
        strcpy(name,"water_in");
        slot = water_in;
    }
    else if(strcmp(rom_code_str,water_out_rom)==0){
        strcpy(name,"water_out");
        slot = water_out;
    }
    else if(strcmp(rom_code_str,inside_rom)==0){
        strcpy(name,"inside");
        slot = inside;
    }
    else if(strcmp(rom_code_str,outside_rom)==0){
        strcpy(name,"outside");
        slot = outside;
    }
    else{
        ESP_LOGD(TAG,"Could not map sensor with code %s",rom_code_str);
        sprintf(name,"UT%d",unkown_name_counter);
        unkown_name_counter++;

    }
    free(rom_code_str);
   
    return slot;
}
static void associate_ds18b20_devices(const OneWireBus * owb,
                                      const OneWireBus_ROMCode * rom_codes,
                                      DS18B20_Info ** device_infos,
                                      char ** names,
                                      sensor_slot * slots,
                                      int num_devices)
{
    ESP_LOGD(TAG, "%s", __FUNCTION__);
    for (int i = 0; 
    i < num_devices; ++i)
    {
        DS18B20_Info * ds18b20_info = ds18b20_malloc();
        device_infos[i] = ds18b20_info;
        names[i]  = malloc(sizeof(char)*32);
        slots[i] = get_sensor_slot(rom_codes[i],names[i]);
        ESP_LOGD(TAG,"Found sensor %s",names[i]);
        if (num_devices == 1)
        {
            ESP_LOGD(TAG, "Single device optimisations enabled");
            ds18b20_init_solo(ds18b20_info, owb);          // only one device on bus
            ds18b20_info->rom_code = rom_codes[0];
        }
        else
        {
            ds18b20_init(ds18b20_info, owb, rom_codes[i]); // associate with bus and device
        }
        ds18b20_use_crc(ds18b20_info, true);           // enable CRC check for temperature readings
        ds18b20_set_resolution(ds18b20_info, DS18B20_RESOLUTION);
    }
}

static temp_sensors_t * detect_sensors(uint8_t gpio)
{
    // set up the One Wire Bus
    OneWireBus * owb = NULL;
    temp_sensors_t * sensors = NULL;
    char ** names = malloc(sizeof(char*)* MAX_DEVICES);
    sensor_slot * slots = malloc(sizeof(sensor_slot)*MAX_DEVICES);
    owb_rmt_driver_info * rmt_driver_info = malloc(sizeof(*rmt_driver_info));

    // TODO: Need to keep hold of rmt_driver_info so we can free it later

    if (rmt_driver_info)
    {
        owb = owb_rmt_initialize(rmt_driver_info, gpio, RMT_CHANNEL_1, RMT_CHANNEL_0);
        owb_use_crc(owb, true);       // enable CRC check for ROM code and measurement readings

        // locate attached devices
        OneWireBus_ROMCode * device_rom_codes = calloc(MAX_DEVICES, sizeof(*device_rom_codes));

        int num_devices = find_owb_rom_codes(owb, device_rom_codes, MAX_DEVICES);

        // free up unused space
        device_rom_codes = realloc(device_rom_codes, num_devices * sizeof(*device_rom_codes));

        // associate devices on bus with DS18B20 device driver
        DS18B20_Info ** device_infos = calloc(num_devices, sizeof(*device_infos));

        associate_ds18b20_devices(owb, device_rom_codes, device_infos, names, slots, num_devices);

        sensors = malloc(sizeof(*sensors));
        if (sensors)
        {
            memset(sensors, 0, sizeof(*sensors));
            sensors->owb = owb;
            sensors->rom_codes = device_rom_codes;
            sensors->ds18b20_infos = device_infos;
            sensors->names = names;
            sensors->slots = slots;
            sensors->num_ds18b20s = num_devices;
        }
        else
        {
            ESP_LOGE(TAG, "malloc failed");
        }
    }
    else
    {
        ESP_LOGE(TAG, "malloc failed");
    }
    return sensors;
}

static void read_temperatures(DS18B20_Info ** device_infos, float * readings, DS18B20_ERROR * errors, int num_devices)
{
    assert(readings);
    assert(errors);

    ds18b20_convert_all(device_infos[0]->bus);

    // in this application all devices use the same resolution,
    // so use the first device to determine the delay
    ds18b20_wait_for_conversion(device_infos[0]);

    // read the results immediately after conversion otherwise it may fail
    // (using printf before reading may take too long)
    for (int i = 0; i < num_devices; ++i)
    {
        errors[i] = ds18b20_read_temp(device_infos[i], &readings[i]);
    }
}


static void sensor_task(void * pvParameter){
    assert(pvParameter);
    TickType_t last_wake_time;
    
    task_inputs_t * task_inputs = (task_inputs_t *)pvParameter;

    /*get the rom_code_match*/

    /*wait a couple of seconds before we start measuring¨*/
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    last_wake_time = xTaskGetTickCount();
    sensor_readings_t * result = malloc(sizeof(sensor_readings_t));
    for(;;){
        ESP_LOGI(TAG,"reading sensors");
        float readings[MAX_DEVICES] = { 0 };
        DS18B20_ERROR errors[MAX_DEVICES] = { 0 };
        read_temperatures(task_inputs->sensors->ds18b20_infos, readings, errors, task_inputs->sensors->num_ds18b20s);
        for(int i = 0; i <task_inputs->sensors->num_ds18b20s; i++){
            if(errors[i]== DS18B20_OK){
                /*keep the readings for internal use*/
                if(task_inputs->sensors->slots[i] != unkown_sensor){
                    last_readings[task_inputs->sensors->slots[i]] = readings[i];
                }

                /* send to mqtt for processing*/
                strcpy(result->name, task_inputs->sensors->names[i]);
                result->reading = readings[i];
                result->rom_code = &task_inputs->sensors->ds18b20_infos[i]->rom_code;
                ESP_LOGI(TAG,"%s: %.1f",task_inputs->sensors->names[i],readings[i]);
                xQueueSend(task_inputs->queue,result,pdMS_TO_TICKS(10000));
            }
            else{
                ESP_LOGI(TAG,"Error while reading sensor %d",i);
            }
        }
        vTaskDelayUntil(&last_wake_time, POLLING_INTERVAL / portTICK_PERIOD_MS);
    }

}
/*
This function detects the sensors available and starts a task to read the temperature every POLLING_INTERVAL time
*/
void init_temperature_sensors(uint8_t owb_pin,QueueHandle_t queue){
    /*get the sensors*/
    temp_sensors_t * sensors = detect_sensors(owb_pin);
    //TODO: doe iets om de sensors te koppelen
    task_inputs_t * task_inputs = malloc(sizeof(*task_inputs));
    task_inputs->sensors = sensors;
    task_inputs->queue= queue;
    xTaskCreate(&sensor_task,"temp_sensor_task",4096,task_inputs,15,&_task_handle);
}

float get_sensor_reading(sensor_slot slot){
    return last_readings[slot];
}

 