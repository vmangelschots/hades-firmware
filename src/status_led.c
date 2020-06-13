#include "status_led.h"
#include "esp_log.h"
#include "driver/gpio.h"


#define TAG "STATUS_LED"

static EventGroupHandle_t * status_bits_handle;

static void led_control_task(){
     /*turn led on*/
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = 1ULL<< 18;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    EventBits_t bits = {0}; 
    int counter = 0;
    for(;;){
        counter ++;
        counter = counter %20;
        bits = xEventGroupGetBits( status_bits_handle );
        if((bits & ERROR_BIT)!= 0){
            if(counter%4<2){
                gpio_set_level(18,1);
            }
            else{
                gpio_set_level(18,0);
            }
        }
        else if((bits & WIFI_CONNECTED_BIT) ==0 || (bits & MQTT_CONNECTED_BIT) == 0){
            if(counter<10){
                gpio_set_level(18,1);
            }
            else{
                gpio_set_level(18,0);
            }
        }
        else if ((bits & HEATER_ON_BIT) != 0)
        {
            gpio_set_level(18,1);
        }
        
        else{
            gpio_set_level(18,0);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }   
}
void init_status_led(){
    status_bits_handle = xEventGroupCreate();
    TaskHandle_t * task_handle = malloc(sizeof(* task_handle));
    xTaskCreate(&led_control_task,"led_control_task",4096,NULL,3,task_handle);
}

EventGroupHandle_t * get_status_bits_handle(){
    assert(status_bits_handle);
    return status_bits_handle;
}

