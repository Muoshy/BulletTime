#include "esp_log.h"
#include "driver/i2c.h"
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_spi_flash.h"

#include <time.h>
#include <sys/time.h>
#include <esp_wifi.h>
#include <esp_netif.h>

#include "esp_sntp.h"
#include "wifi_manager.h"


#define TAG "i2c_bus"
#define ack_en false

#define GPIO_INPUT_IO_0     26
#define GPIO_INPUT_IO_1     13
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_0) | (1ULL<<GPIO_INPUT_IO_1))
#define ESP_INTR_FLAG_DEFAULT 0

static uint8_t b = 5;
static uint8_t prevb = 5;

static xQueueHandle gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task_example(void* arg)
{
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            if(io_num == 26) {b++;}
            if(io_num == 13) {b--;}
            if(b>20) {b = 20;}
            if(b<1) {b = 1;}
            printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
            printf("Brightness: %i \n", b);

        }
    }
}



const uint16_t digits[10][8] = {

{0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E, 0x00},

{0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E, 0x00},

{0x0E, 0x11, 0x11, 0x02, 0x04, 0x08, 0x1F, 0x00},

{0x1F, 0x02, 0x04, 0x0E, 0x01, 0x11, 0x0E, 0x00},

{0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02, 0x00}, 

{0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E, 0x00},

{0x0E, 0x11, 0x10, 0x1E, 0x11, 0x11, 0x0E, 0x00},

{0x1F, 0x01, 0x02, 0x04, 0x04, 0x04, 0x04, 0x00},

{0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E, 0x00},

{0x0E, 0x11, 0x11, 0x0F, 0x01, 0x11, 0x0E, 0x00}

};


void set_oscillator(uint8_t addr) {
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);

    i2c_master_write_byte(handle, addr << 1 | I2C_MASTER_WRITE, ack_en);
    i2c_master_write_byte(handle, 0x20 | 1, ack_en);

    i2c_master_stop(handle);

    esp_err_t error = i2c_master_cmd_begin(I2C_NUM_0, handle, 1000 / portTICK_RATE_MS);
    if (error != ESP_OK) {
    ESP_LOGI(TAG, "Failed to set oscillator: %s", esp_err_to_name(error));
    }

    i2c_cmd_link_delete(handle);
}

void set_blink(uint8_t b, uint8_t addr){
	if(b > 3) return;
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
	i2c_master_write_byte(handle, addr << 1 | I2C_MASTER_WRITE, ack_en);
	i2c_master_write_byte(handle, 0x80 | b << 1 | 1, ack_en); // Blinking / blanking command
    i2c_master_stop(handle);

    esp_err_t error = i2c_master_cmd_begin(I2C_NUM_0, handle, 1000 / portTICK_RATE_MS);
    if (error != ESP_OK) {
    ESP_LOGI(TAG, "Failed to set blink mode: %s", esp_err_to_name(error));
    }

    i2c_cmd_link_delete(handle);	
}

void set_brightness(uint8_t b, uint8_t addr) {
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);

    i2c_master_write_byte(handle, addr << 1 | I2C_MASTER_WRITE, ack_en);
    i2c_master_write_byte(handle, 0xE0 | b , ack_en);

    i2c_master_stop(handle);

    esp_err_t error = i2c_master_cmd_begin(I2C_NUM_0, handle, 1000 / portTICK_RATE_MS);
    if (error != ESP_OK) {
    ESP_LOGI(TAG, "Failed to set brightness: %s", esp_err_to_name(error));
    }

    i2c_cmd_link_delete(handle);	
}

void clear_matrix(uint8_t addr) {
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, addr << 1 | I2C_MASTER_WRITE, ack_en);
    i2c_master_write_byte(handle, 0x00, ack_en);

    for(int i = 0; i < 16; i++) {
            i2c_master_write_byte(handle, 0x00, ack_en);
        }

    i2c_master_stop(handle);

    esp_err_t error = i2c_master_cmd_begin(I2C_NUM_0, handle, 1000 / portTICK_RATE_MS);
    if (error != ESP_OK) {
    ESP_LOGI(TAG, "Failed to clear matrix: %s", esp_err_to_name(error));
    }

    i2c_cmd_link_delete(handle);
}

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}


static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();
}

static void obtain_time(void)
{

    initialize_sntp();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    time(&now);
    localtime_r(&now, &timeinfo);

}



void cb_connection_ok(){

    vTaskDelay( pdMS_TO_TICKS(500) );

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    obtain_time();
    time(&now);

    char strftime_buf[64];

    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in Stockholm is: %s", strftime_buf);

    wifi_manager_send_message(WM_ORDER_DISCONNECT_STA, NULL);
    
}


void print_task() {
	uint8_t prev_sec = 10;
    char ss[3];
    char mm[3];
    char hh[3];
    time_t now;
    struct tm timeinfo;

    uint8_t s1, s2, m1, m2, h1, h2;

    for(;;){

        time(&now);
        //char strftime_buf[64];

        localtime_r(&now, &timeinfo);
        //strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        //ESP_LOGI(TAG, "The current date/time in Stockholm is: %s", strftime_buf);

        strftime(ss, 3, "%S", &timeinfo);
        strftime(mm, 3, "%M", &timeinfo);
        strftime(hh, 3, "%H", &timeinfo);

        s1 = ss[0] - '0';
        s2 = ss[1] - '0';

        m1 = mm[0] - '0';
        m2 = mm[1] - '0';

        h1 = hh[0] - '0';
        h2 = hh[1] - '0';

        if(s2 != prev_sec) {
            i2c_cmd_handle_t handle = i2c_cmd_link_create();
            i2c_master_start(handle);
            i2c_master_write_byte(handle, 0x70 << 1 | I2C_MASTER_WRITE, ack_en);
            i2c_master_write_byte(handle, 0x00, ack_en);
            for(int i = 0; i < 8; i++) {
                uint16_t row = (digits[h1][i] << 10) + (digits[h2][i] << 5) + digits[m1][i];
                i2c_master_write_byte(handle, row & 0xFF, ack_en);
                i2c_master_write_byte(handle, row >> 8, ack_en);
            }
            i2c_master_stop(handle);

            i2c_master_cmd_begin(I2C_NUM_0, handle, 1000 / portTICK_RATE_MS);
            i2c_cmd_link_delete(handle);

            handle = i2c_cmd_link_create();
            i2c_master_start(handle);
            i2c_master_write_byte(handle, 0x71 << 1 | I2C_MASTER_WRITE, ack_en);
            i2c_master_write_byte(handle, 0x00, ack_en);
            for(int i = 0; i < 8; i++) {
                uint16_t row = (digits[m2][i] << 10) + (digits[s1][i] << 5) + digits[s2][i];
                i2c_master_write_byte(handle, row & 0xFF, ack_en);
                i2c_master_write_byte(handle, row >> 8, ack_en);
            }
            i2c_master_stop(handle);

            i2c_master_cmd_begin(I2C_NUM_0, handle, 1000 / portTICK_RATE_MS);
            i2c_cmd_link_delete(handle);
            prev_sec = s2;
        }

        if(b != prevb) {
            set_brightness(b, 0x70);
            set_brightness(b, 0x71);
            prevb = b;
        }

		vTaskDelay( pdMS_TO_TICKS(100) );
	}

}

void cb_disconnect(){
    ESP_LOGI(TAG, "Disconnected from WiFi");
    xTaskCreatePinnedToCore(&print_task, "print_task", 2048, NULL, 1, NULL, 1);
	esp_wifi_stop();
	wifi_manager_destroy();
}


void app_main(void)
{   

    //------------ interrupt config
    gpio_config_t io_conf;

    io_conf.pull_down_en = 0;

    io_conf.pull_up_en = 0;

    io_conf.intr_type = GPIO_INTR_POSEDGE;

    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;

    io_conf.mode = GPIO_MODE_INPUT;
    
    gpio_config(&io_conf);

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void*) GPIO_INPUT_IO_0);

    gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_isr_handler, (void*) GPIO_INPUT_IO_1);

    //------------ i2c config
    i2c_config_t config;
    config.sda_io_num = 32;
    config.sda_pullup_en = GPIO_PULLUP_ENABLE;
    config.scl_io_num = 33;
    config.scl_pullup_en = GPIO_PULLUP_ENABLE;
    config.mode = I2C_MODE_MASTER;
    config.master.clk_speed = 40000;

    esp_err_t error = i2c_param_config(I2C_NUM_0, &config);
        if (error != ESP_OK) {
            ESP_LOGI(TAG, "Failed to configure shared i2c bus: %s", esp_err_to_name(error));
            return;
        }

    error = i2c_set_timeout(I2C_NUM_0, 100000);
        if (error != ESP_OK) {
            ESP_LOGI(TAG, "Failed set timeout for i2c bus: %s", esp_err_to_name(error));
            return;
        }


    error = i2c_driver_install(I2C_NUM_0, config.mode, 512, 512, 0);
        if (error != ESP_OK) {
            ESP_LOGI(TAG, "Failed to install driver for i2c bus: %s", esp_err_to_name(error));
        }

        
    set_oscillator(0x70);
    set_oscillator(0x71);

    set_brightness(5, 0x70);
    set_brightness(5, 0x71);

    set_blink(0, 0x70);
    set_blink(0, 0x71);

    clear_matrix(0x70);
    clear_matrix(0x71);

    wifi_manager_start();

    
    //wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, &cb_connection_ok);
    wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, &cb_connection_ok);

    wifi_manager_set_callback(WM_EVENT_STA_DISCONNECTED, &cb_disconnect);

}
