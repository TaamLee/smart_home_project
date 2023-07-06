#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "driver/gptimer.h"
#include "nvs_flash.h"
#include "coap3/coap.h"


#define COAP_DEFAULT_DEMO_URI "coap://192.168.0.2:5683/control_motor"


void GetTemperature_Coap(void);
void ControlMotor_Coap (char *cmd);