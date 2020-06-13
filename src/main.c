#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "version.h"
#include "nvs_store.h"
#include "wifi_control.h"
#include "esp_sntp.h"
#include <time.h>
#include <sys/time.h>

#include "temperature_sensor.h"
#include "mqtt_control.h"

#include "freertos/queue.h"
#include "status_led.h"
#include "secrets.h"
#include "heater.h"
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
        esp_log_level_set("wpa",ESP_LOG_ERROR);
        /*Boot notification*/
        ESP_LOGI(TAG,"Hades Pool heater controller version %f",VERSION);

        
        /*initialize datastore*/
        nvs_store_init();
        init_status_led();
        wifi_init(SSID,PASSWORD);

        /* get the time of the day*/
        setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
        tzset();
        sntp_setoperatingmode(SNTP_OPMODE_POLL);
        sntp_setservername(0, "pool.ntp.org");
        sntp_init();
        while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET ) {
            ESP_LOGI(TAG, "Waiting for system time to be set... ");
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
        time_t now;
        struct tm timeinfo;
        char strftime_buf[64];
        time(&now);
        localtime_r(&now,&timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "The current date/time: %s", strftime_buf);
        /*create message queue between sensors and mqtt*/
        QueueHandle_t sensor_mqtt_queue = xQueueCreate( 12, sizeof(sensor_readings_t));
        QueueHandle_t heater_status_queue = xQueueCreate(12,sizeof(bool));
        init_temperature_sensors(OWB_PIN,sensor_mqtt_queue);
        init_mqtt(sensor_mqtt_queue,heater_status_queue);

        init_heater(heater_status_queue);

    }
