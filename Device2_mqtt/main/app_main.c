
#include "app_main.h"
#include "wifi_conection.h"
#include "mqtt.h"
#include "ds18b20.h"

float temp_value;
QueueHandle_t data_queue;

void control_sock_handler(char* cmd)
{
    //check cmd
    if (!strncmp(cmd,"on",2))
    {
        gpio_set_level(GPIO_NUM_2,1);
    }
    else if (!strncmp(cmd,"off",3))
    {
        gpio_set_level(GPIO_NUM_2,0);
    }

}

void gpio_init (void)
{
    // init for led
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1<<GPIO_NUM_2);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    //init for ds18b20
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = (1<<GPIO_NUM_4);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

}
void read_data_ds18b20_task (void * p)
{
    char data_enqueue[10];
    while(1)
    {
        temp_value = ds18b20_get_temp();
        sprintf(data_enqueue,"%f",temp_value);
        //send to queue
        xQueueSend( data_queue, ( void * ) data_enqueue, portMAX_DELAY );
        printf("%f\n",temp_value);
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }

}
void send_data_to_homecenter_task(void *p)
{
    char payload[10];
    while(1)
    {
        if( xQueueReceive( data_queue,payload,portMAX_DELAY ) == pdPASS )
        {
            publish_temperature_value(payload);
        }

    }

}
void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    //init gpio
    gpio_init ();

    //init ds18b20
    ds18b20_init(GPIO_NUM_4);

    //create a queue
    data_queue = xQueueCreate( 10, 10 );

    //conect to HOME Central 
    wifi_init_sta();

    //start a mqtt client
    mqtt_app_start();

    xTaskCreate(read_data_ds18b20_task, "read_data_ds18b20_task", 4 * 1024, NULL, 5, NULL);
    xTaskCreate(send_data_to_homecenter_task, "send_data_to_homecenter_task", 4 * 1024, NULL, 5, NULL);
}
