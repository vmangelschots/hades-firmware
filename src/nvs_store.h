
#ifndef NVS_STORE_H
#define NVS_STORE_H


#include "nvs.h"
#include "nvs_flash.h"



    void nvs_store_init();
    int32_t nvs_store_getValue(char* key);
    void nvs_store_storeInt(char* key, int32_t value);
    void nvs_store_commit();
        

#endif
