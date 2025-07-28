#include "sntp_time.h" 
#include "esp_log.h"
#include "esp_sntp.h"
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h" 
#include "freertos/task.h"    

#define TIME_ZONE "BRT3" 
static const char *TAG_SNTP = "SNTP_MODULE";

void sntp_time_init(void) {
    ESP_LOGI(TAG_SNTP, "Inicializando SNTP...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    
    esp_sntp_setservername(0, "br.pool.ntp.org");

    esp_sntp_init(); 

    ESP_LOGI(TAG_SNTP, "Configurando fuso horário para: %s", TIME_ZONE);
    setenv("TZ", TIME_ZONE, 1);
    tzset();
}

void print_current_time(void) {
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    time(&now);
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year < (2024 - 1900)) { 
         ESP_LOGI(TAG_SNTP, "Hora ainda não sincronizada ou inválida.");
    } else {
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG_SNTP, "Hora atual: %s", strftime_buf);
    }
}

void get_formatted_time(char *time_str, size_t max_len) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year < (2024 - 1900)) {
        snprintf(time_str, max_len, "--:--");
    } else {
        strftime(time_str, max_len, "%H:%M", &timeinfo);
    }
}

void get_full_datetime(char *datetime_str, size_t max_len) { 
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year < (2024 - 1900)) {
         snprintf(datetime_str, max_len, "DD/MM/YYYY-HH:MM");
    } else {
        strftime(datetime_str, max_len, "%d/%m/%Y-%H:%M", &timeinfo);
    }
}