/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/event_groups.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "mqtt_client.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
#include "esp_mac.h"
#else
#include "esp_system.h"
#endif

#if CONFIG_TEST_MQTT_QOS
#define ESP_TEST_QOS   1
#else 
#define ESP_TEST_QOS   0
#endif
static const char *TAG = "MQTT_TCP";
//ESP32-C3 AP-CH6
// USB 0/1/2 Qos0 + 60s + AMPDU RX Disable + AMPDU TX Disable
// USB 3/4 Qos0 + 60s + AMPDU RX Enable + AMPDU TX Enable
// USB 5/6/7 Qos0 + 60s + LE


/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define EXAMPLE_ESP_MAXIMUM_RETRY   5
static uint8_t ipv4[4];
static esp_netif_ip_info_t ip_info;
static wifi_config_t ap_info;
static char DeviceName[16];
static uint8_t s_keepalive = CONFIG_BROKER_KEEPALIVE;



#define DEFAULT_SCAN_LIST_SIZE 10

static void print_auth_mode(int authmode)
{
    switch (authmode) {
    case WIFI_AUTH_OPEN:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OPEN");
        break;
    case WIFI_AUTH_OWE:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OWE");
        break;
    case WIFI_AUTH_WEP:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WEP");
        break;
    case WIFI_AUTH_WPA_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_PSK");
        break;
    case WIFI_AUTH_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA2_ENTERPRISE:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_ENTERPRISE");
        break;
    case WIFI_AUTH_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_PSK");
        break;
    case WIFI_AUTH_WPA2_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_PSK");
        break;
    default:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_UNKNOWN");
        break;
    }
}

static void print_cipher_type(int pairwise_cipher, int group_cipher)
{
    switch (pairwise_cipher) {
    case WIFI_CIPHER_TYPE_NONE:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_NONE");
        break;
    case WIFI_CIPHER_TYPE_WEP40:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP40");
        break;
    case WIFI_CIPHER_TYPE_WEP104:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP104");
        break;
    case WIFI_CIPHER_TYPE_TKIP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP");
        break;
    case WIFI_CIPHER_TYPE_CCMP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_CCMP");
        break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
        break;
    default:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }

    switch (group_cipher) {
    case WIFI_CIPHER_TYPE_NONE:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_NONE");
        break;
    case WIFI_CIPHER_TYPE_WEP40:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP40");
        break;
    case WIFI_CIPHER_TYPE_WEP104:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP104");
        break;
    case WIFI_CIPHER_TYPE_TKIP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP");
        break;
    case WIFI_CIPHER_TYPE_CCMP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_CCMP");
        break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
        break;
    default:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }
}


static int s_retry_num = 0;
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            // ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        // ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        // ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        // ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

#if CONFIG_TEST_MQTT_PS

static const char *TAG = "MQTT_TEST_PS";

#if CONFIG_EXAMPLE_POWER_SAVE_MIN_MODEM
#define DEFAULT_PS_MODE WIFI_PS_MIN_MODEM
#elif CONFIG_EXAMPLE_POWER_SAVE_MAX_MODEM
#define DEFAULT_PS_MODE WIFI_PS_MAX_MODEM
#elif CONFIG_EXAMPLE_POWER_SAVE_NONE
#define DEFAULT_PS_MODE WIFI_PS_NONE
#else
#define DEFAULT_PS_MODE WIFI_PS_NONE
#endif /*CONFIG_POWER_SAVE_MODEM*/

/*init wifi as sta and set power save mode*/
static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_STA_SSID,
            #if CONFIG_TEST_AP_PASSWD
            .password = CONFIG_ESP_WIFI_STA_PASSWORD,
            #endif
            .listen_interval = CONFIG_TEST_WIFI_LISTEN_INTERVAL,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
#if CONFIG_PM_ENABLE
#if CONFIG_IDF_TARGET_ESP32
    esp_pm_config_esp32_t pm_config = {
#elif CONFIG_IDF_TARGET_ESP32S2
    esp_pm_config_esp32s2_t pm_config = {
#elif CONFIG_IDF_TARGET_ESP32C3
    esp_pm_config_esp32c3_t pm_config = {
#elif CONFIG_IDF_TARGET_ESP32S3
    esp_pm_config_esp32s3_t pm_config = {
#elif CONFIG_IDF_TARGET_ESP32C2
    esp_pm_config_esp32c2_t pm_config = {
#endif
            .max_freq_mhz = CONFIG_EXAMPLE_MAX_CPU_FREQ_MHZ,
            .min_freq_mhz = CONFIG_EXAMPLE_MIN_CPU_FREQ_MHZ,
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
            .light_sleep_enable = true
#endif
    };
    ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
#endif // CONFIG_PM_ENABLE
    ESP_LOGI(TAG, "esp_wifi_set_ps().");
    esp_wifi_set_ps(DEFAULT_PS_MODE);
}

#else 


static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    // wifi_ap_record_t ap_scan_info[DEFAULT_SCAN_LIST_SIZE];
    // uint16_t ap_count = 0;
    // memset(ap_scan_info, 0, sizeof(ap_scan_info));

    // ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // ESP_ERROR_CHECK(esp_wifi_start());
    // esp_wifi_scan_start(NULL, true);
    // ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_scan_info));
    // ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    // ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);
    // for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) {
    //     ESP_LOGI(TAG, "SSID \t\t%s", ap_scan_info[i].ssid);
    //     ESP_LOGI(TAG, "RSSI \t\t%d", ap_scan_info[i].rssi);
    //     print_auth_mode(ap_scan_info[i].authmode);
    //     if (ap_scan_info[i].authmode != WIFI_AUTH_WEP) {
    //         print_cipher_type(ap_scan_info[i].pairwise_cipher, ap_scan_info[i].group_cipher);
    //     }
    //     ESP_LOGI(TAG, "Channel \t\t%d\n", ap_scan_info[i].primary);
    // }

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_STA_SSID,
            #if CONFIG_TEST_AP_PASSWD
            .password = CONFIG_ESP_WIFI_STA_PASSWORD,
            #endif
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            8000);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        esp_wifi_get_config(WIFI_IF_STA, &ap_info);
        esp_netif_get_ip_info(sta_netif, &ip_info);
        ipv4[0] = (ip_info.ip.addr) & 0xff;
        ipv4[1] = (ip_info.ip.addr >>8) & 0xff;
        ipv4[2] = (ip_info.ip.addr >>16) & 0xff;
        ipv4[3] = ip_info.ip.addr >>24;
        ESP_LOGI(TAG, "connected to ap SSID:%s[%d.%d.%d.%d][chan %d]",CONFIG_ESP_WIFI_STA_SSID,ipv4[0],ipv4[1],ipv4[2],ipv4[3],ap_info.ap.channel);
        
        
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s",
                 CONFIG_ESP_WIFI_STA_SSID);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        esp_restart();
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}
#endif

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}


/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    char startup[28];
    sprintf(startup,"/%s/start",DeviceName);
    char subrssi[28];
    sprintf(subrssi,"/%s/rssi",DeviceName);

    wifi_ap_record_t ap_rssi;
    char rssi[8];
    esp_wifi_sta_get_ap_info(&ap_rssi);
    sprintf(rssi,"%ddBm",ap_rssi.rssi);
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_publish(client, "/dev/start", DeviceName, 0, ESP_TEST_QOS, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        msg_id = esp_mqtt_client_publish(client, startup, rssi, 0, ESP_TEST_QOS, 0);
        ESP_LOGI(TAG, "sent publish to %s, msg_id=%d",startup, msg_id);
        msg_id = esp_mqtt_client_subscribe(client, subrssi, 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        vTaskDelay(pdMS_TO_TICKS(10));
        msg_id = esp_mqtt_client_publish(client, subrssi, rssi, 0, ESP_TEST_QOS, 0);

        // msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        // ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGE(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        // msg_id = esp_mqtt_client_publish(client, DeviceName, DeviceName, 0, 0, 0);
        msg_id = esp_mqtt_client_publish(client,"/dev/alive", DeviceName, 0, ESP_TEST_QOS, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        // ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        // printf("RECV=%.*s\r\n", event->topic_len, event->topic);
        // ESP_LOGI("mqtt", "%.*s",  event->topic_len, event->topic);
        uint8_t data = rand()%80+20;
        vTaskDelay(pdMS_TO_TICKS(100*data));
        msg_id = esp_mqtt_client_publish(client,"/dev/alive", DeviceName, 0, ESP_TEST_QOS, 0);
        // vTaskDelay(pdMS_TO_TICKS(data));
        msg_id = esp_mqtt_client_publish(client, subrssi, rssi, 0, 0, 0);
        // printf("rssi", "%.*s -> %s", event->data_len, event->data, rssi);
        printf("[%d][%d.%d]%s[%d]: %.*s -> %s [%d.%d]\n",ap_info.ap.channel,ipv4[2],ipv4[3],DeviceName,s_keepalive,event->data_len, event->data,rssi,data/10,data%10);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGE(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGW(TAG, "Other event id:%d", event->event_id);
        break;
    }
}




static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
        .credentials.client_id = DeviceName,
        .credentials.username = CONFIG_BROKER_USER,
        .credentials.authentication.password = CONFIG_BROKER_PASSWORD,
        .session.keepalive = s_keepalive,
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

}

void app_main(void)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    sprintf(DeviceName,"%02x%02x%02x%02x%02x%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    ESP_LOGI("device", "[IDF]%s [MAC]%s", esp_get_idf_version(),DeviceName);
    uint32_t min_heap = esp_get_free_heap_size();
    ESP_LOGW("start free heap"," %luKB",min_heap/1024);  
    
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);
    esp_log_level_set("wifi", ESP_LOG_VERBOSE);

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    wifi_init_sta();
    mqtt_app_start();
}
