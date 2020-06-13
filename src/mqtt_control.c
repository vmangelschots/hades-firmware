#include "mqtt_control.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "temperature_sensor.h"
#include "owb.h"
#include "status_led.h"

#define TAG "mqtt"

bool mqtt_connected = false;

esp_mqtt_client_handle_t client = NULL;

typedef struct task_inputs_t{
    QueueHandle_t queue;
    QueueHandle_t status_queue;
} task_inputs_t;

static void parse_commands(char* data){
    ESP_LOGD(TAG,"i would now process %s",data);
    if(!strcmp(data,"set_error")){
        xEventGroupSetBits(get_status_bits_handle(), ERROR_BIT);
    }
    else if(!strcmp(data,"clear_error")){
        xEventGroupClearBits(get_status_bits_handle(), ERROR_BIT);
    }
    else if(!strcmp(data,"setmode remote")){
        
    }
    else if(!strcmp(data,"setmode local")){
        
    }
    else{
        ESP_LOGW(TAG,"Unkown command %s",data);
    }
}
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            msg_id = esp_mqtt_client_publish(client, "/hades/status", "online", 0, 1, 0);
            ESP_LOGD(TAG, "sent publish successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, "/hades/commands", 0);

            // msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
            // ESP_LOGD(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            // msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
            // ESP_LOGD(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
            mqtt_connected = true;
             xEventGroupSetBits(get_status_bits_handle(), MQTT_CONNECTED_BIT);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT disconnected");
            xEventGroupClearBits(get_status_bits_handle(), MQTT_CONNECTED_BIT);
            mqtt_connected = false;
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            char ltopic[256];
            char ldata[256];
            memcpy(&ltopic,event->topic,event->topic_len);
            memcpy(&ldata,event->data,event->data_len);
            ltopic[event->topic_len] = '\0';
            ldata[event->data_len] = '\0';
            if(strcmp(ltopic,"/hades/commands")==0){
                parse_commands(ldata);
            }
            else{
                ESP_LOGD(TAG,"Could not parse command %s",ltopic);
            }
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}
static void sensor_readings_sender_task(void * pvParameter){
    task_inputs_t * task_inputs = (task_inputs_t *)pvParameter;
    QueueHandle_t queue = task_inputs->queue;
    sensor_readings_t * readings = malloc(sizeof(sensor_readings_t));
    char  * message = calloc(256,sizeof(char));
    for(;;){
        switch(xQueueReceive(queue,readings,pdMS_TO_TICKS(10000))){
            case(pdPASS):
                if(!client || !mqtt_connected){
                    

                    ESP_LOGW(TAG,"MQTT not connected. Sensor reading discarded");
                }
                else{

                    char * rom_code_str =calloc(sizeof(char),17);
                    owb_string_from_rom_code(*readings->rom_code, rom_code_str, sizeof(char)*17);
                    sprintf(message,"%s,rom_code=%s temperature=%.1f",readings->name,rom_code_str,readings->reading);
                    free(rom_code_str);
                    esp_mqtt_client_publish(client, "/hades/temp_sensor",message , 0, 1, 0);
                    ESP_LOGD(TAG,"%s send",message); 
                    
                }

                break;
            
            case(errQUEUE_EMPTY):
                ESP_LOGD(TAG,"No readings to send");
            
        }
    }
}

static void heater_status_sender_task(void * pvParameter){
    task_inputs_t * task_inputs = (task_inputs_t *)pvParameter;
    QueueHandle_t status_queue= task_inputs->status_queue;
    int * heater_status = malloc(sizeof(int*));
    char  * message = calloc(256,sizeof(char));
    for(;;){
        switch(xQueueReceive(status_queue,heater_status,pdMS_TO_TICKS(10000))){
            case(pdPASS):
                if(!client || !mqtt_connected){
                    ESP_LOGW(TAG,"MQTT not connected. sensor status discared");
                }
                else{
                    if(*heater_status){
                        strcpy(message,"on");
                    }
                    else{
                        strcpy(message,"off");
                    }
                        esp_mqtt_client_publish(client, "/hades/heater_status",message , 0, 1, 0);
                        ESP_LOGD(TAG,"%s send",message);    
                }
                break;
            case(errQUEUE_EMPTY):
                ESP_LOGD(TAG,"No readings to send");
        }
    }
}
void init_mqtt(QueueHandle_t queue,QueueHandle_t heater_status_queue){
    ESP_LOGD(TAG,"Initializing mqtt");
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = "mqtt://192.168.5.24:1893"
    };
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
    TaskHandle_t _task_handle;
    TaskHandle_t _heater_status_task_handle;
    task_inputs_t * task_inputs = malloc(sizeof(*task_inputs));
    task_inputs->queue = queue;
    task_inputs->status_queue = heater_status_queue;
    xTaskCreate(&sensor_readings_sender_task,"sensor_reading_sender_task",4096,task_inputs,14,&_task_handle);
    xTaskCreate(&heater_status_sender_task,"heater_status_sender_task",4096,task_inputs,14,&_heater_status_task_handle);
}