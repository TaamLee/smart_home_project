#include <string.h>
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <arpa/inet.h>
#include "lwip/opt.h"


#include "lwip/err.h"
#include "lwip/sys.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#define ON_COMMAND    "on"
#define OFF_COMMAND    "off"

typedef struct 
{
    uint8_t temperature_value;
    uint8_t ID_DEVICE;
} Data_Information_t;
typedef struct 
{
    uint8_t CMD;
    uint8_t ID_DEVICE;
} Command_Information_t;

extern int wifi_connected;
extern Data_Information_t temp_value_indoor;
extern Data_Information_t temp_value_outdoor;
extern QueueHandle_t data_queue;
extern QueueHandle_t cmd_queue;
extern TaskHandle_t cmd_handler_task_Handle;