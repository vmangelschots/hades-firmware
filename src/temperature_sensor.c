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


struct temp_sensors_t
{
    OneWireBus * owb;
    OneWireBus_ROMCode * rom_codes;   // a pointer to an *array* of ROM codes
    DS18B20_Info ** ds18b20_infos;
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

static void associate_ds18b20_devices(const OneWireBus * owb,
                                      const OneWireBus_ROMCode * rom_codes,
                                      DS18B20_Info ** device_infos,
                                      int num_devices)
{
    ESP_LOGD(TAG, "%s", __FUNCTION__);
    for (int i = 0; i < num_devices; ++i)
    {
        DS18B20_Info * ds18b20_info = ds18b20_malloc();
        device_infos[i] = ds18b20_info;

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

        associate_ds18b20_devices(owb, device_rom_codes, device_infos, num_devices);

        sensors = malloc(sizeof(*sensors));
        if (sensors)
        {
            memset(sensors, 0, sizeof(*sensors));
            sensors->owb = owb;
            sensors->rom_codes = device_rom_codes;
            sensors->ds18b20_infos = device_infos;
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

    /*wait a couple of seconds before we start measuring¨*/

    vTaskDelay(5000 / portTICK_PERIOD_MS);
    last_wake_time = xTaskGetTickCount();
    
    for(;;){
        ESP_LOGI(TAG,"reading sensors");
        float readings[MAX_DEVICES] = { 0 };
        DS18B20_ERROR errors[MAX_DEVICES] = { 0 };
        read_temperatures(task_inputs->sensors->ds18b20_infos, readings, errors, task_inputs->sensors->num_ds18b20s);
        for(int i = 0; i <MAX_DEVICES; i++){
            if(errors[i]== DS18B20_OK){
                sensor_readings_t * result = malloc(sizeof(sensor_readings_t));
                sprintf(result->name,"T%d",i);
                result->reading = readings[i];
                ESP_LOGI(TAG,"  T%d: %.1f",i,readings[i]);
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