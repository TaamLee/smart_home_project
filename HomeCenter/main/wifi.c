
#include "wifi.h"
#include "app_main.h"

static const char *TAG = "wifi softAP";
// WiFi application settings
#define WIFI_AP_SSID				"ESP32_AP"			// AP name
#define WIFI_AP_PASSWORD			"password"			// AP password
#define WIFI_AP_CHANNEL				1					// AP channel
#define WIFI_AP_SSID_HIDDEN			0					// AP visibility
#define WIFI_AP_MAX_CONNECTIONS		5					// AP max clients
#define WIFI_AP_BEACON_INTERVAL		100					// AP beacon: 100 milliseconds recommended
#define WIFI_AP_IP					"192.168.0.1"		// AP default IP
#define WIFI_AP_GATEWAY				"192.168.0.1"		// AP default Gateway (should be the same as the IP)
#define WIFI_AP_NETMASK				"255.255.255.0"		// AP netmask
#define WIFI_AP_BANDWIDTH			WIFI_BW_HT20		// AP bandwidth 20 MHz (40 MHz is the other option)
#define WIFI_STA_POWER_SAVE			WIFI_PS_NONE		// Power save not used
#define MAX_SSID_LENGTH				32					// IEEE standard maximum
#define MAX_PASSWORD_LENGTH			64					// IEEE standard maximum
#define MAX_CONNECTION_RETRIES		3					// Retry number on disconnect

#define WIFI_STA_WIFI_SSID			"VIP1_5G"
#define WIFI_STA_WIFI_PASSWORD		"12345679"


// netif objects for the station and access point
esp_netif_t* esp_netif_sta = NULL;
esp_netif_t* esp_netif_ap  = NULL;

int s_retry_num =0;

static void wifi_ap_handler(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data)
{
    if (base == WIFI_EVENT)
    {
        switch (id)
		{
			case WIFI_EVENT_AP_START:
				ESP_LOGI(TAG, "WIFI_MODE_AP started. SSID:%s password:%s channel:%d",
							WIFI_AP_SSID, WIFI_AP_PASSWORD, WIFI_AP_CHANNEL);
				break;

			case WIFI_EVENT_AP_STOP:
				ESP_LOGI(TAG, "WIFI_EVENT_AP_STOP");
				break;

			case WIFI_EVENT_AP_STACONNECTED:
				ESP_LOGI(TAG, "WIFI_EVENT_AP_STACONNECTED");
				break;

			case WIFI_EVENT_AP_STADISCONNECTED:
				ESP_LOGI(TAG, "WIFI_EVENT_AP_STADISCONNECTED");
				break;

			case WIFI_EVENT_STA_START:
				ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
				// connect to moderm AP
				ESP_ERROR_CHECK( esp_wifi_connect() );
				break;

			case WIFI_EVENT_STA_CONNECTED:
				ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");
				wifi_connected = 1;
				break;

			case WIFI_EVENT_STA_DISCONNECTED:
				ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");
				//reconect to AP 
				if (s_retry_num < MAX_CONNECTION_RETRIES) {
					esp_wifi_connect();
					s_retry_num++;
					ESP_LOGI(TAG, "retry to connect to the AP");
				}
				if (s_retry_num == MAX_CONNECTION_RETRIES)
				{	
					Command_Information_t dummy_data = {0,0};
					// change wifi status
					wifi_connected = 0;
					// send a dummy data to unblock cmd handler task 
					xQueueSend( cmd_queue,&dummy_data,( TickType_t ) 10 );
				}
				break;
		}


    }
    else if (base == IP_EVENT)
    {
        switch (id)
		{
			case IP_EVENT_STA_GOT_IP:
				ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP");
				ESP_LOGI(TAG, "WIFI_MODE_STA connected. SSID:%s password:%s",
			              WIFI_STA_WIFI_SSID, WIFI_STA_WIFI_PASSWORD);
				s_retry_num=0;
				break;
		}
    }
}

void ap_sta_wifi_start()
{
    // // init flash
    // esp_err_t ret = nvs_flash_init();
    // if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    //   ESP_ERROR_CHECK(nvs_flash_erase());
    //   ret = nvs_flash_init();
    // }
    //ESP_ERROR_CHECK(ret);

    // // init event loop
    // ESP_ERROR_CHECK(esp_event_loop_create_default());
    //register event
    esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,&wifi_ap_handler,NULL,NULL);
    esp_event_handler_instance_register(IP_EVENT,ESP_EVENT_ANY_ID,&wifi_ap_handler,NULL,NULL);

    // // init LWip
    // ESP_ERROR_CHECK(esp_netif_init());
	// this is create default lw stack for wifi
	//in AP/STA we have to create for both ap and sta
	esp_netif_ap = esp_netif_create_default_wifi_ap();
    esp_netif_sta =esp_netif_create_default_wifi_sta();

    //config some parameter for wifi
    wifi_init_config_t cfg_wifi = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg_wifi));

    //config for ap mode 
	wifi_config_t ap_config =
	{
		.ap = {
				.ssid = WIFI_AP_SSID,
				.ssid_len = strlen(WIFI_AP_SSID),
				.password = WIFI_AP_PASSWORD,
				.channel = WIFI_AP_CHANNEL,
				.ssid_hidden = WIFI_AP_SSID_HIDDEN,
				.authmode = WIFI_AUTH_WPA2_PSK,
				.max_connection = WIFI_AP_MAX_CONNECTIONS,
				.beacon_interval = WIFI_AP_BEACON_INTERVAL,
		},
	};
	//config for sta mode
	wifi_config_t sta_config = {
		.sta= {
			.ssid = WIFI_STA_WIFI_SSID,
			.password = WIFI_STA_WIFI_PASSWORD,
		},
	 };

	// Configure DHCP server for the gateway in AP mode
	esp_netif_ip_info_t ap_ip_info;
	memset(&ap_ip_info, 0x00, sizeof(ap_ip_info));

	esp_netif_dhcps_stop(esp_netif_ap);					///> must call this first
	ap_ip_info.ip.addr = ipaddr_addr(WIFI_AP_IP);
	ap_ip_info.gw.addr = ipaddr_addr(WIFI_AP_GATEWAY);
	ap_ip_info.netmask.addr = ipaddr_addr(WIFI_AP_NETMASK);
	ESP_ERROR_CHECK(esp_netif_set_ip_info(esp_netif_ap, &ap_ip_info));			///> Statically configure the network interface
	ESP_ERROR_CHECK(esp_netif_dhcps_start(esp_netif_ap));						///> Start the AP DHCP server (for connecting stations e.g. your mobile device)

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));						///> Setting the mode as Access Point / Station Mode
	ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_AP_BANDWIDTH));		///> Our default bandwidth 20 MHz
	ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_STA_POWER_SAVE));						///> Power save set to "NONE"

	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));			///> Set AP configuration

	ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_config) );		///> Set STA configuration


	 // Start AP WiFi
	ESP_ERROR_CHECK(esp_wifi_start());


}
