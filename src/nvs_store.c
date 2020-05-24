#include "nvs_store.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char* TAG = "nvs_store";

static nvs_handle_t myhandle;

void nvs_store_init(){
    // Initialize NVS
    ESP_LOGD(TAG,"Initializing flash");
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_LOGD(TAG,"NVS partition was truncated and needs to be erased");
        ESP_ERROR_CHECK(nvs_flash_erase()); 
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    ESP_LOGD(TAG,"Opening store");
    err = nvs_open("storage",NVS_READWRITE,&myhandle);
    if(err!=ESP_OK){
        ESP_LOGE(TAG,"Could not open storage");
    }
    ESP_ERROR_CHECK(err);
}

int32_t nvs_store_getValue(char* key){
    int32_t value;
    esp_err_t err;
    err=nvs_get_i32(myhandle,key,&value);
        switch (err) {
            case ESP_OK:
                ESP_LOGD(TAG,"found value for %s",key);
                break;
                
            case ESP_ERR_NVS_NOT_FOUND:
                ESP_LOGD(TAG,"%s not found in store",key);
                return -1;
                break;
            default :
                printf("Error (%s) reading!\n", esp_err_to_name(err));
        }
    return value;

}

void nvs_store_storeInt(char* key, int32_t value){
    esp_err_t err;
    ESP_LOGD(TAG,"Writing %i to %s",value,key);
    err = nvs_set_i32(myhandle, key, value);
    if (err != ESP_OK){
        ESP_LOGE(TAG,"Could not write %i to %s",value,key);
        ESP_ERROR_CHECK(err);
    }


    
}

void nvs_store_commit(){
    esp_err_t err;
    ESP_LOGD(TAG,"Commiting to flash");
    err = nvs_commit(myhandle);
    if (err != ESP_OK){
        ESP_LOGE(TAG,"Commit failed");
        ESP_ERROR_CHECK(err);
    }   

}