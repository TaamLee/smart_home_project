
#include "http.h"
#include "app_main.h"

static const char *TAG = "HTTP_CLIENT";
char cmd_data[100];
uint8_t len_cmd_data;
Command_Information_t cmd_enqueue;
esp_err_t client_event_post_handler(esp_http_client_event_handle_t evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        printf("HTTP_EVENT_ON_DATA: %.*s\n", evt->data_len, (char *)evt->data);
        sprintf(cmd_data,"%.*s\n", evt->data_len, (char *)evt->data);
        len_cmd_data = evt->data_len;
        for (int i =0 ; i<len_cmd_data;i++)
        {
            if (cmd_data[i] == ',')
            {
                if (cmd_data[i+2] == 'm')
                {
                    if (cmd_data[i+13] == '1') 
                    {
                        cmd_enqueue.ID_DEVICE = 1;
                        printf("device 1");
                    }
                    else if (cmd_data[i+13] == '2')
                    {
                        cmd_enqueue.ID_DEVICE = 2;
                        printf("device 2");
                    }
                }
                if (cmd_data[i+2] == 'p')
                {
                    if (cmd_data[i+10] == 't') 
                    {
                        cmd_enqueue.CMD = 1;
                        //send to data_queue
                        xQueueSend( cmd_queue,&cmd_enqueue,( TickType_t ) 10 );
                        printf(": on\n");
                    }
                    else if (cmd_data[i+10] == 'f')
                    {
                        cmd_enqueue.CMD = 0;
                        //send to data_queue
                        xQueueSend( cmd_queue,&cmd_enqueue,( TickType_t ) 10 );
                        printf(": off\n");
                    } 
                }

            }
        }
        break;

    default:
        break;
    }
    return ESP_OK;
}


void SendtoThingsboard_Device1(uint8_t data)
{
    char output_buffer[512] = {0};
    char post_data [25];
    esp_http_client_config_t config_post = {
        .url = "http://demo.thingsboard.io/api/v1/OSXXVzXfeO4v6V5jyk8j/telemetry",
        .method = HTTP_METHOD_POST,
        .cert_pem = NULL,
        .event_handler = client_event_post_handler};
        
    esp_http_client_handle_t client = esp_http_client_init(&config_post);

    sprintf(post_data,"{\"temperature\":%d}",data);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_http_client_set_header(client, "Content-Type", "application/json");

    esp_http_client_perform(client);

    int data_read = esp_http_client_read_response(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
    if (data_read >= 0) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %lld",
        esp_http_client_get_status_code(client),
        esp_http_client_get_content_length(client));
        ESP_LOG_BUFFER_HEX(TAG, output_buffer, strlen(output_buffer));
    } else {
        ESP_LOGE(TAG, "Failed to read response");
    }

    esp_http_client_cleanup(client);

}
void SendtoThingsboard_Device2(uint8_t data)
{
    char output_buffer[512] = {0};
    char post_data [25];
    esp_http_client_config_t config_post = {
        .url = "http://demo.thingsboard.io/api/v1/GqZXgmkTMB7hGNRcOCer/telemetry",
        .method = HTTP_METHOD_POST,
        .cert_pem = NULL,
        .event_handler = client_event_post_handler};
        
    esp_http_client_handle_t client = esp_http_client_init(&config_post);

    sprintf(post_data,"{\"temperature\":%d}",data);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_http_client_set_header(client, "Content-Type", "application/json");

    esp_http_client_perform(client);

    int data_read = esp_http_client_read_response(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
    if (data_read >= 0) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %lld",
        esp_http_client_get_status_code(client),
        esp_http_client_get_content_length(client));
        ESP_LOG_BUFFER_HEX(TAG, output_buffer, strlen(output_buffer));
    } else {
        ESP_LOGE(TAG, "Failed to read response");
    }

    esp_http_client_cleanup(client);

}
void GetRequestToThingsboard_Device1()
{

    esp_http_client_config_t config_get = {
        .url = "http://demo.thingsboard.io/api/v1/OSXXVzXfeO4v6V5jyk8j/rpc",
        .method = HTTP_METHOD_GET,
        .cert_pem = NULL,
        .event_handler = client_event_post_handler,
        .keep_alive_enable = 1,
        .timeout_ms = 60000,
        };
    
    esp_http_client_handle_t client_get = esp_http_client_init(&config_get);
    esp_http_client_set_header(client_get, "Content-Type", "application/json");
    esp_http_client_perform(client_get);
    esp_http_client_cleanup(client_get);

}
void GetRequestToThingsboard_Device2()
{
    esp_http_client_config_t config_get = {
        .url = "http://demo.thingsboard.io/api/v1/GqZXgmkTMB7hGNRcOCer/rpc",
        .method = HTTP_METHOD_GET,
        .cert_pem = NULL,
        .event_handler = client_event_post_handler,
        .keep_alive_enable = 1,
        .timeout_ms = 60000,
        };
    
    esp_http_client_handle_t client_get = esp_http_client_init(&config_get);
    esp_http_client_set_header(client_get, "Content-Type", "application/json");
    esp_http_client_perform(client_get);
    esp_http_client_cleanup(client_get);

}