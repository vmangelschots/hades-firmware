#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "version.h"
#include "nvs_store.h"
#include "wifi_control.h"
#include "driver/gpio.h"
#include "temperature_sensor.h"
#include "mqtt_control.h"

#include "freertos/queue.h"

#include "secrets.h"
#define DEFAULT_LOG_LEVEL ESP_LOG_DEBUG
#define OWB_PIN 15

static const char* TAG = "Main";



    void app_main() {
        /* Set log levels*/
        esp_log_level_set("*",DEFAULT_LOG_LEVEL);
        esp_log_level_set("nvs_store",DEFAULT_LOG_LEVEL);
        esp_log_level_set("owb",ESP_LOG_ERROR);
        esp_log_level_set("ds18b20",ESP_LOG_ERROR);
        esp_log_level_set("MQTT_CLIENT",ESP_LOG_ERROR);
        /*Boot notification*/
        ESP_LOGI(TAG,"Hades Pool heater controller version %f",VERSION);

        
        /*initialize datastore*/
        nvs_store_init();
        wifi_init(SSID,PASSWORD);


        /*turn led on*/
        gpio_config_t io_conf;
        io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = 1ULL<< 18;
        io_conf.pull_down_en = 0;
        io_conf.pull_up_en = 0;
        gpio_config(&io_conf);
        gpio_set_level(18,1);

        /*create message queue between sensors and mqtt*/
        QueueHandle_t sensor_mqtt_queue = xQueueCreate( 12, sizeof(sensor_readings_t));
        init_temperature_sensors(OWB_PIN,sensor_mqtt_queue);
        init_mqtt(sensor_mqtt_queue);

    }
