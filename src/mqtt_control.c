#include "mqtt_control.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "temperature_sensor.h"
#define TAG "mqtt"


esp_mqtt_client_handle_t client = NULL;

typedef struct task_inputs_t{
    QueueHandle_t queue;
} task_inputs_t;

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

            // msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
            // ESP_LOGD(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            // msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
            // ESP_LOGD(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            // msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
            // ESP_LOGD(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT disconnected");
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
                
                sprintf(message,"%s temperature=%.1f",readings->name,readings->reading);
                //sprintf(message,"%s","A somewhat long test message to see where it goes wrong");
                esp_mqtt_client_publish(client, "/hades/temp_sensor",message , 0, 1, 0);
                ESP_LOGD(TAG,"%s send",message); 
                break;
            
            case(errQUEUE_EMPTY):
                ESP_LOGD(TAG,"No readings to send");
            
        }
    }
}
void init_mqtt(QueueHandle_t queue){
    ESP_LOGD(TAG,"Initializing mqtt");
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = "mqtt://192.168.5.24"
    };
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
    TaskHandle_t _task_handle;
    task_inputs_t * task_inputs = malloc(sizeof(*task_inputs));
    task_inputs->queue = queue;
    xTaskCreate(&sensor_readings_sender_task,"sensor_reading_sender_task",4096,task_inputs,14,&_task_handle);
}