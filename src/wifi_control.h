#ifndef wifi_control_h
#define wifi_control_h


#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <stdio.h>
#include <string.h>

#include <freertos/event_groups.h>

void wifi_init(char* ssid, char* password);


#endif