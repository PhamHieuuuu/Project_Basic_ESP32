
#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "app";

//-------------------------- Form hệ thống yêu cầu ------------------
static unsigned int count_runs = 6;   // Chọn số lần vòng loop
static bool Start_System = false;

static void Script_System(const char* user_name)
{
    if (!Start_System) {
        Start_System = true;
        ESP_LOGI(TAG, "");  // dòng trống cho giống Serial.println("")
        ESP_LOGI(TAG, "____Start run partition_%s____", user_name);
    }

    if (count_runs > 0) {
        count_runs--;
    }

    if (count_runs == 0) {
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "____End run partition_%s____", user_name);
        esp_restart();  // tương đương ESP.restart()
    }
}
//-------------------------- Form hệ thống yêu cầu ------------------

// Chương trình ứng dụng viết trong đây: đặt trong vòng while của app_main

void app_main(void)
{
    // (Tùy chọn) In banner như setup()
    ESP_LOGI(TAG, "Booting... baud console mặc định 115200");

    // KHỞI TẠO ỨNG DỤNG (nếu cần) — tương đương phần trong setup()
    // ... (init peripheral, wifi, v.v.)

    // LOOP — tương đương loop() của Arduino
    while (1) {
        // Hàm yêu cầu phải có với tên user
        Script_System("hs4");

        // Chương trình ứng dụng viết trong đây bắt đầu
        ESP_LOGI(TAG, "p4");
        // Chương trình ứng dụng viết trong đây==== kết thúc

        vTaskDelay(pdMS_TO_TICKS(1000)); // delay(1000)
    }
}
