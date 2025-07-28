#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <esp_http_client.h>
#include <string.h>
#include <time.h>
#include "cJSON.h" 

#include "mfrc522.h"
#include "esp_crt_bundle.h"
#include "lcd_i2c.h"
#include "sntp_time.h"
#include "wifi_connect.h"
#include "esp_sntp.h"

static const char *TAG = "PONTO_ELETRONICO";
const char *scriptURL = "https://script.google.com/macros/s/AKfycby86pnetJZ1SdcUlivKSr4a-XhnbjjLH_jjFYc-42WoNiy37ztx3xKkpl-xi5SPbA9i/exec"; 
static const uint32_t readInterval = 2000;                                                                                                    
#define MASTER_CARD_UID "03B98756"                                                                                                            

#define DISPLAY_LINE_LEN 16
#define DISPLAY_BUFFER_SIZE (DISPLAY_LINE_LEN + 1)

typedef enum
{
    DISPLAY_MODE_CONNECTING_WIFI,
    DISPLAY_MODE_CONNECTING_SNTP,
    DISPLAY_MODE_DEFAULT,
    DISPLAY_MODE_PROCESSANDO,
    DISPLAY_MODE_ENTRADA,
    DISPLAY_MODE_SAIDA,
    DISPLAY_MODE_NAO_CADASTRADO,
    DISPLAY_MODE_ERRO,
    DISPLAY_MODE_REGISTRATION,
    DISPLAY_MODE_REG_SUCCESS,
    DISPLAY_MODE_REG_FAIL
} display_state_t;

static volatile display_state_t current_display_state = DISPLAY_MODE_CONNECTING_WIFI;
static bool g_registration_mode = false;
static char g_response_buffer[512];
static char g_display_message_buffer[DISPLAY_BUFFER_SIZE];
static volatile bool g_http_request_in_progress = false;

static char lastCardID[20] = "";
static uint32_t lastReadTime = 0;

esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
        g_response_buffer[0] = '\0';
        break;
    case HTTP_EVENT_ON_DATA:
        if (evt->data_len > 0 && (strlen(g_response_buffer) + evt->data_len) < sizeof(g_response_buffer))
        {
            strncat(g_response_buffer, (char *)evt->data, evt->data_len);
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        g_http_request_in_progress = false;
        break;
    default:
        break;
    }
    return ESP_OK;
}

void perform_registration(const char *card_id)
{
    g_http_request_in_progress = true;
    g_response_buffer[0] = '\0';

    char url_buffer[256];
    snprintf(url_buffer, sizeof(url_buffer), "%s?action=register&uid=%s", scriptURL, card_id);

    esp_http_client_config_t config = {
        .url = url_buffer,
        .event_handler = http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        cJSON *json = cJSON_Parse(g_response_buffer);
        if (json)
        {
            const cJSON *status = cJSON_GetObjectItem(json, "status");
            if (cJSON_IsString(status) && strcmp(status->valuestring, "registered") == 0)
            {
                current_display_state = DISPLAY_MODE_REG_SUCCESS;
            }
            else
            {
                current_display_state = DISPLAY_MODE_REG_FAIL;
            }
            cJSON_Delete(json);
        }
        else
        {
            current_display_state = DISPLAY_MODE_REG_FAIL;
        }
    }
    else
    {
        current_display_state = DISPLAY_MODE_REG_FAIL;
    }
    esp_http_client_cleanup(client);
}

void process_card_lookup(const char *card_id)
{
    g_http_request_in_progress = true;
    g_response_buffer[0] = '\0';

    char url_buffer[256];
    snprintf(url_buffer, sizeof(url_buffer), "%s?uid=%s", scriptURL, card_id);

    esp_http_client_config_t config = {
        .url = url_buffer,
        .event_handler = http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        ESP_LOGE(TAG, "Falha ao inicializar cliente HTTP");
        current_display_state = DISPLAY_MODE_ERRO;
        snprintf(g_display_message_buffer, sizeof(g_display_message_buffer), "Erro HTTP");
        g_http_request_in_progress = false;
        return;
    }

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        int http_status = esp_http_client_get_status_code(client);
        if (http_status == 200)
        {
            cJSON *json = cJSON_Parse(g_response_buffer);
            if (json == NULL)
            {
                current_display_state = DISPLAY_MODE_ERRO;
                snprintf(g_display_message_buffer, sizeof(g_display_message_buffer), "Erro Resposta");
            }
            else
            {
                const cJSON *status = cJSON_GetObjectItem(json, "status");
                if (cJSON_IsString(status) && (strcmp(status->valuestring, "success") == 0))
                {
                    const cJSON *name = cJSON_GetObjectItem(json, "name");
                    const cJSON *action = cJSON_GetObjectItem(json, "action");
                    if (cJSON_IsString(name))
                    {
                        snprintf(g_display_message_buffer, sizeof(g_display_message_buffer), "%s", name->valuestring);
                    }
                    if (cJSON_IsString(action) && (strcmp(action->valuestring, "saida") == 0))
                    {
                        current_display_state = DISPLAY_MODE_SAIDA;
                    }
                    else
                    {
                        current_display_state = DISPLAY_MODE_ENTRADA;
                    }
                }
                else if (cJSON_IsString(status) && (strcmp(status->valuestring, "not_found") == 0))
                {
                    current_display_state = DISPLAY_MODE_NAO_CADASTRADO;
                }
                else
                {
                    current_display_state = DISPLAY_MODE_ERRO;
                    snprintf(g_display_message_buffer, sizeof(g_display_message_buffer), "Erro Servidor");
                }
                cJSON_Delete(json);
            }
        }
        else
        {
            current_display_state = DISPLAY_MODE_ERRO;
            snprintf(g_display_message_buffer, sizeof(g_display_message_buffer), "Erro HTTP %d", http_status);
        }
    }
    else
    {
        current_display_state = DISPLAY_MODE_ERRO;
        snprintf(g_display_message_buffer, sizeof(g_display_message_buffer), "Erro Conexao");
    }

    esp_http_client_cleanup(client);
}

static void format_display_line_centered(char *buffer, const char *text)
{
    int text_len = strlen(text);
    int padding_total = DISPLAY_LINE_LEN - text_len;
    if (padding_total < 0)
        padding_total = 0;
    int padding_start = padding_total / 2;

    memset(buffer, ' ', DISPLAY_LINE_LEN);
    memcpy(buffer + padding_start, text, text_len > DISPLAY_LINE_LEN ? DISPLAY_LINE_LEN : text_len);
    buffer[DISPLAY_LINE_LEN] = '\0';
}

static void display_manager_task(void *pvParameters)
{
    char time_str_buffer[9];
    char line0[DISPLAY_BUFFER_SIZE];
    char line1[DISPLAY_BUFFER_SIZE];
    uint32_t temp_message_end_time = 0;
    display_state_t last_displayed_state = -1;

    while (1)
    {
        uint32_t current_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

        if (current_display_state == DISPLAY_MODE_CONNECTING_SNTP && sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED)
        {
            ESP_LOGI(TAG, "SNTP Sincronizado! Mudando para a tela padrão.");
            current_display_state = DISPLAY_MODE_DEFAULT;
        }

        if (temp_message_end_time != 0 && current_time_ms >= temp_message_end_time)
        {
            temp_message_end_time = 0; 
            if (g_registration_mode)
            {
                current_display_state = DISPLAY_MODE_REGISTRATION;
            }
            else
            {
                current_display_state = DISPLAY_MODE_DEFAULT;
            }
        }

        if (current_display_state != last_displayed_state || current_display_state == DISPLAY_MODE_DEFAULT)
        {
            switch (current_display_state)
            {
            case DISPLAY_MODE_CONNECTING_WIFI:
                format_display_line_centered(line0, "Conectando");
                format_display_line_centered(line1, "ao WiFi...");
                break;
            case DISPLAY_MODE_CONNECTING_SNTP:
                format_display_line_centered(line0, "Sincronizando");
                format_display_line_centered(line1, "Relogio...");
                break;
            case DISPLAY_MODE_PROCESSANDO:
                format_display_line_centered(line0, "Processando...");
                format_display_line_centered(line1, "Aguarde");
                break;
            case DISPLAY_MODE_NAO_CADASTRADO:
                format_display_line_centered(line0, "Cartao");
                format_display_line_centered(line1, "Nao Cadastrado");
                temp_message_end_time = current_time_ms + 3000;
                break;
            case DISPLAY_MODE_ENTRADA:
                format_display_line_centered(line0, "ENTRADA");
                format_display_line_centered(line1, g_display_message_buffer);
                temp_message_end_time = current_time_ms + 3000;
                break;
            case DISPLAY_MODE_SAIDA:
                format_display_line_centered(line0, "SAIDA");
                format_display_line_centered(line1, g_display_message_buffer);
                temp_message_end_time = current_time_ms + 3000;
                break;
            case DISPLAY_MODE_ERRO:
                format_display_line_centered(line0, "ERRO");
                format_display_line_centered(line1, g_display_message_buffer);
                temp_message_end_time = current_time_ms + 3000;
                break;
            case DISPLAY_MODE_REGISTRATION:
                format_display_line_centered(line0, "MODO CADASTRO");
                format_display_line_centered(line1, "Aproxime cartao");
                break;
            case DISPLAY_MODE_REG_SUCCESS:
                format_display_line_centered(line0, "Sucesso!");
                format_display_line_centered(line1, "Cadastrado");
                temp_message_end_time = current_time_ms + 3000;
                break;
            case DISPLAY_MODE_REG_FAIL:
                format_display_line_centered(line0, "Falha");
                format_display_line_centered(line1, "no Cadastro");
                temp_message_end_time = current_time_ms + 3000;
                break;
            case DISPLAY_MODE_DEFAULT:
            default:
                format_display_line_centered(line0, "PONTO ELETRONICO");
                get_formatted_time(time_str_buffer, sizeof(time_str_buffer));
                format_display_line_centered(line1, time_str_buffer);
                break;
            }
            lcd_set_cursor(0, 0);
            lcd_print_str(line0);
            lcd_set_cursor(1, 0);
            lcd_print_str(line1);
            last_displayed_state = current_display_state;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

static void tag_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    rc522_event_data_t *rfid_data = (rc522_event_data_t *)event_data;
    rc522_tag_t *tag = (rc522_tag_t *)rfid_data->ptr;
    char cardID_str[20];
    snprintf(cardID_str, sizeof(cardID_str), "%08lX", (unsigned long)tag->serial_number);

    uint32_t currentTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (strcmp(cardID_str, lastCardID) == 0 && (currentTime - lastReadTime < readInterval))
    {
        ESP_LOGI(TAG, "Cartao %s lido novamente dentro do intervalo. Ignorando.", cardID_str);
        return;
    }
    lastReadTime = currentTime;
    strncpy(lastCardID, cardID_str, sizeof(lastCardID));

    if (strcmp(cardID_str, MASTER_CARD_UID) == 0)
    {
        g_registration_mode = !g_registration_mode;
        if (g_registration_mode)
        {
            ESP_LOGI(TAG, "MODO CADASTRO ATIVADO");
            current_display_state = DISPLAY_MODE_REGISTRATION;
        }
        else
        {
            ESP_LOGI(TAG, "MODO CADASTRO DESATIVADO");
            current_display_state = DISPLAY_MODE_DEFAULT;
        }
        return;
    }

    if (g_registration_mode)
    {
        ESP_LOGI(TAG, "Cadastrando novo cartao: %s", cardID_str);
        current_display_state = DISPLAY_MODE_PROCESSANDO;
        perform_registration(cardID_str);
        return;
    }

    ESP_LOGI(TAG, "Cartao de ponto detectado: %s. Processando...", cardID_str);
    current_display_state = DISPLAY_MODE_PROCESSANDO;
    process_card_lookup(cardID_str);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(lcd_module_init());
    ESP_LOGI(TAG, "Módulo LCD inicializado.");

    xTaskCreate(display_manager_task, "display_manager_task", 4096, NULL, 5, NULL);

    wifi_connect_init("eduarda", "12345678");

    current_display_state = DISPLAY_MODE_CONNECTING_SNTP;
    sntp_time_init();

    ESP_LOGI(TAG, "Inicializando leitor MFRC522...");
    rc522_config_t mfrc_config = {
        .transport = RC522_TRANSPORT_SPI,
        .spi = {
            .host = SPI2_HOST,
            .miso_gpio = 19,
            .mosi_gpio = 23,
            .sck_gpio = 18,
            .sda_gpio = 15,
        },
    };
    rc522_handle_t scanner;
    ESP_ERROR_CHECK(rc522_create(&mfrc_config, &scanner));
    ESP_ERROR_CHECK(rc522_register_events(scanner, RC522_EVENT_TAG_SCANNED, tag_handler, NULL));
    ESP_ERROR_CHECK(rc522_start(scanner));
    ESP_LOGI(TAG, "Leitor MFRC522 pronto.");

    ESP_LOGI(TAG, "Sistema principal pronto e aguardando cartões.");
}