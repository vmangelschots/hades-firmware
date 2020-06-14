#include "heater.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "temperature_sensor.h"
#include "driver/gpio.h"
#include "status_led.h"
#include "esp_log.h"
#include "mqtt_control.h"
#include "string.h"
#include "stdio.h"
#define TAG "heater"

static TaskHandle_t _task_handle = NULL;
static bool remote_controlled = false;
static int ticks;

typedef struct
{
} task_inputs_t;

QueueHandle_t heater_queue;
void setMode(bool mode){
    if(mode){
        remote_controlled = true;
    }
    else{
        remote_controlled = false;
    }
}

static void heater_task(void * pvParameter){
    EventBits_t bits = {0};
    for(;;){
    float water_in_temp = get_sensor_reading(water_in);
    float water_out_temp = get_sensor_reading(water_out);
    float diff = water_in_temp - water_out_temp;
    if(diff < 0 ){
        diff = diff *-1;
    }
    ESP_LOGD(TAG,"temperature difference: %f",diff);
    if(diff> MAX_SENSOR_DIFF){
        xEventGroupSetBits(get_status_bits_handle(), ERROR_BIT);
        //strcpy(message,"temperature difference found");
        //strcpy(topic,"/hades/heater/error/reason");
        //xQueueSend(heater_queue,status_message,pdMS_TO_TICKS(10000));
        //strcpy(message,"on");
        //strcpy(topic,"/hades/heater/error/status");
        //xQueueSend(heater_queue,status_message,pdMS_TO_TICKS(10000));
        set_heater(false);
    }
    else{
        //strcpy(message,"off");
        //strcpy(topic,"/hades/heater/error/status");
        //xQueueSend(heater_queue,status_message,pdMS_TO_TICKS(10000));
        bits = xEventGroupClearBits(get_status_bits_handle(), ERROR_BIT); 
        if(remote_controlled && !((bits & WIFI_CONNECTED_BIT) ==0 || (bits & MQTT_CONNECTED_BIT) == 0)){

        }
        else{
            if(water_in_temp<26.0){
                set_heater(true);

            }
            else{
                set_heater(false);
            }
        }
    }

    vTaskDelay(pdMS_TO_TICKS(5000));
    }

}

void init_heater(QueueHandle_t queue){

    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = 1ULL<< RELAY_PIN;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    /* wait for the temperature sensors to be initialized*/
    xEventGroupWaitBits(get_status_bits_handle(),TEMPERATURE_SENSOR_INIT,pdFALSE,pdFALSE,pdMS_TO_TICKS(10000));
    task_inputs_t * task_inputs = malloc(sizeof(*task_inputs));
    heater_queue = queue;
    xTaskCreate(&heater_task,"heater_task",4096,task_inputs,15,&_task_handle);
}

static void set_heater(int on){

    status_message_t status_message;
    
    if(on){
        gpio_set_level(RELAY_PIN,1);
        strcpy(status_message.data,"on");
    }
    else{
        gpio_set_level(RELAY_PIN,0);
        strcpy(status_message.data,"off");
        
    }
    strcpy(status_message.topic,"/hades/heater/status");
    
    xQueueSend(heater_queue,&status_message,pdMS_TO_TICKS(10000));
}