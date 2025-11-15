/*
 * Custom multi-app round-robin bootloader for ESP-IDF v5.3.x
 * - Stage byte stored at partition "chain" (offset 0x210000)
 * - Stage order: 0->app0(factory), 1->app1(ota_0), 2->app2(ota_1), 3->app3(ota_2)
 * - Validates target app by checking magic byte 0xE9
 * - Loads app via bootloader_utility_load_boot_image(&bs, boot_index)
 *
 * Factory index in IDF 5.x is -1, OTA slots start at 0.

 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "sdkconfig.h"

/* ROM APIs */
#include "esp_rom_spiflash.h"
#include "esp_rom_sys.h"

/* IDF bootloader headers */
#include "esp_log.h"
#include "bootloader_init.h"
#include "bootloader_utility.h"
#include "bootloader_common.h"
#include "bootloader_hooks.h"

#ifndef SPI_FLASH_SEC_SIZE
#define SPI_FLASH_SEC_SIZE 4096
#endif

static const char *TAG = "boot";

/* Vị trí byte STAGE (khớp CSV: partition 'chain' @0x210000, size 4KB) */
#define CHAIN_OFFSET   0x210000U
#define STAGE_ADDR     (CHAIN_OFFSET)   /* bảo đảm 4-byte aligned */
#define NUM_APPS       4

/* Hằng boot_index thực tế trong IDF 5.x */
#define BL_IDX_FACTORY   (-1)  /* app0 */
#define BL_IDX_OTA0      (0)   /* app1 */
#define BL_IDX_OTA1      (1)   /* app2 */
#define BL_IDX_OTA2      (2)   /* app3 */

/* ---------- Flash helpers: đọc/ghi theo 32-bit word để khỏi cảnh báo ---------- */

static uint8_t chain_read_stage(void)
{
    uint32_t word = 0xFFFFFFFFU;
    /* esp_rom_spiflash_read(dest = uint32_t*, len bytes, len nên là bội số 4) */
    if (esp_rom_spiflash_read(STAGE_ADDR, &word, sizeof(word)) != 0) {
        return 0;
    }
    uint8_t b = (uint8_t)(word & 0xFF);
    if (b == 0xFF || b >= NUM_APPS) return 0;
    return b;
}

static void chain_write_stage(uint8_t stage)
{
    /* Erase 1 sector rồi program 1 word: 0xFFFFFFSS (SS = stage) */
    esp_rom_spiflash_erase_sector(CHAIN_OFFSET / SPI_FLASH_SEC_SIZE);
    uint32_t word = 0xFFFFFF00U | (uint32_t)stage;
    (void) esp_rom_spiflash_write(STAGE_ADDR, &word, sizeof(word));
}

/* Đọc 1 byte đầu của image để kiểm tra magic = 0xE9 */
static bool image_magic_ok(uint32_t image_offset)
{
    uint32_t word = 0xFFFFFFFFU;
    if (esp_rom_spiflash_read(image_offset, &word, sizeof(word)) != 0) {
        return false;
    }
    return ((word & 0xFFU) == 0xE9U);
}

/* Map stage -> boot_index (IDF 5.x) */
static int stage_to_boot_index(uint8_t stage)
{
    switch (stage % NUM_APPS) {
        case 0: return BL_IDX_FACTORY; /* app0 */
        case 1: return BL_IDX_OTA0;    /* app1 */
        case 2: return BL_IDX_OTA1;    /* app2 */
        case 3: return BL_IDX_OTA2;    /* app3 */
        default: return BL_IDX_FACTORY;
    }
}

/* Lấy offset phân vùng theo stage (để check magic nhanh) từ bootloader_state_t */
static uint32_t stage_to_offset(const bootloader_state_t *bs, uint8_t stage)
{
    switch (stage % NUM_APPS) {
        case 0: return bs->factory.offset;
        case 1: return bs->ota[0].offset;
        case 2: return bs->ota[1].offset;
        case 3: return bs->ota[2].offset;
        default: return bs->factory.offset;
    }
}

void __attribute__((noreturn)) call_start_cpu0(void)
{
    if (bootloader_before_init) bootloader_before_init();

    if (bootloader_init() != ESP_OK) {
        bootloader_reset();
    }

    if (bootloader_after_init) bootloader_after_init();

#ifdef CONFIG_BOOTLOADER_SKIP_VALIDATE_IN_DEEP_SLEEP
    bootloader_utility_load_boot_image_from_deep_sleep();
#endif

    /* 1) Nạp bảng phân vùng */
    bootloader_state_t bs = {0};
    if (!bootloader_utility_load_partition_table(&bs)) {
        ESP_LOGE(TAG, "Partition table error!");
        bootloader_reset();
    }

    /* 2) Đọc stage và thử tối đa 4 lần (vòng tròn) để tìm phân vùng có image hợp lệ */
    uint8_t stage = chain_read_stage();
    ESP_LOGI(TAG, "Stage=%u (order: factory, ota_0, ota_1, ota_2)", stage);

    for (int i = 0; i < NUM_APPS; ++i) {
        uint8_t try_stage = (stage + i) % NUM_APPS;
        uint32_t off = stage_to_offset(&bs, try_stage);
        if (off == 0) {
            ESP_LOGW(TAG, "Stage %u: slot not present", try_stage);
            continue;
        }
        if (!image_magic_ok(off)) {
            ESP_LOGW(TAG, "Stage %u: image @0x%06x not bootable (bad magic)", try_stage, off);
            continue;
        }

        /* 3) Chọn boot_index đúng cho stage, ghi trước “next” để vòng quay tiếp */
        int boot_index = stage_to_boot_index(try_stage);
        uint8_t next   = (try_stage + 1) % NUM_APPS;
        chain_write_stage(next);

        ESP_LOGI(TAG, "Booting stage=%u -> boot_index=%d (off=0x%06x), next=%u",
                 try_stage, boot_index, off, next);

        /* 4) Nạp app qua API ổn định của IDF (sẽ nhảy vào app nếu OK) */
        bootloader_utility_load_boot_image(&bs, boot_index);

        /* Nếu quay lại được tới đây, tức load thất bại → thử stage kế */
        ESP_LOGW(TAG, "Load failed at stage %u (off=0x%06x). Trying next...", try_stage, off);
    }

    /* 5) Không app nào bootable → reset */
    ESP_LOGE(TAG, "No bootable app found. Resetting...");
    bootloader_reset();
}

/* newlib reent */
struct _reent *__getreent(void) { return _GLOBAL_REENT; }
