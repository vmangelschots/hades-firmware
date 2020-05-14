#include "esp_log.h"
#include "version.h"

#define DEFAULT_LOG_LEVEL ESP_LOG_DEBUG

static const char* TAG = "Main";

void app_main() {
    /* Set log levels*/
    esp_log_level_set("*",DEFAULT_LOG_LEVEL);

    /*Boot notification*/
    ESP_LOGI(TAG,"Hades Pool heater controller version %f",VERSION);


}