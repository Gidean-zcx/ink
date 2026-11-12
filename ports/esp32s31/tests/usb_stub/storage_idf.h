#ifndef TEST_STORAGE_IDF_H
#define TEST_STORAGE_IDF_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE 1
#define ESP_ERR_NOT_SUPPORTED 2
#define ESP_PARTITION_TYPE_DATA 1
#define ESP_PARTITION_SUBTYPE_ANY 255
#define ESP_PARTITION_MMAP_DATA 0
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_DMA 4
#define MMC_WRITE_BLOCK_SINGLE 24
#define MMC_WRITE_BLOCK_MULTIPLE 25
#define MMC_ERASE 38
typedef int esp_err_t;
typedef unsigned esp_partition_mmap_handle_t;
typedef struct { unsigned address, size; } esp_partition_t;
typedef struct { unsigned opcode; } sdmmc_command_t;
typedef struct { int slot; bool (*check_buffer_alignment)(int, const void *, size_t);
    int (*do_transaction)(int, sdmmc_command_t *); void *dma_aligned_buffer;
    int (*init)(void); unsigned unaligned_multi_block_rw_max_chunk_size; } sdmmc_host_t;
typedef struct { int clk, cmd, d0, d1, d2, d3, width, flags, gpio_cd; } sdmmc_slot_config_t;
typedef struct { unsigned tag; sdmmc_host_t host; } sdmmc_card_t;
typedef struct { int max_files; size_t allocation_unit_size; bool disk_status_check_enable, format_if_mount_failed; } esp_vfs_fat_mount_config_t;
int sdmmc_host_init(void);
#define SDMMC_HOST_DEFAULT() {.init = sdmmc_host_init}
#define SDMMC_SLOT_CONFIG_DEFAULT() {.gpio_cd = -1}
typedef const char *esp_event_base_t;
#define ESP_EVENT_DEFINE_BASE(name) const char *name = #name
typedef void (*test_event_fn)(void *, esp_event_base_t, int32_t, void *);
typedef struct { void (*callback)(void *); const char *name; } esp_timer_create_args_t;
typedef void *esp_timer_handle_t;
const esp_partition_t *esp_partition_find_first(int type, int subtype, const char *name);
int esp_partition_read(const esp_partition_t *, size_t off, void *buf, size_t n);
int esp_partition_write(const esp_partition_t *, size_t off, const void *buf, size_t n);
int esp_partition_erase_range(const esp_partition_t *, size_t off, size_t n);
int esp_partition_mmap(const esp_partition_t *, size_t, size_t, int, const void **, esp_partition_mmap_handle_t *);
void *heap_caps_calloc(size_t, size_t, unsigned);
void *heap_caps_aligned_alloc(size_t, size_t, unsigned);
void heap_caps_free(void *);
int esp_cache_get_alignment(unsigned, size_t *);
bool esp_ptr_internal(const void *);
bool sdmmc_host_check_buffer_alignment(int, const void *, size_t);
int sdmmc_host_do_transaction(int, sdmmc_command_t *);
int sdmmc_get_status(sdmmc_card_t *);
void gpio_hold_dis(int);
int esp_vfs_fat_sdmmc_mount(const char *, const sdmmc_host_t *, const sdmmc_slot_config_t *, const esp_vfs_fat_mount_config_t *, sdmmc_card_t **);
int esp_vfs_fat_sdcard_unmount(const char *, sdmmc_card_t *);
int esp_vfs_fat_info(const char *, uint64_t *, uint64_t *);
int esp_event_loop_create_default(void);
int esp_event_handler_register(esp_event_base_t, int32_t, test_event_fn, void *);
int esp_event_post(esp_event_base_t, int32_t, void *, size_t, unsigned);
int esp_timer_create(const esp_timer_create_args_t *, esp_timer_handle_t *);
int esp_timer_start_periodic(esp_timer_handle_t, uint64_t);
void esp_fill_random(void *, size_t);
#endif

typedef test_event_fn esp_event_handler_t;
int sdmmc_host_deinit(void);
int sdmmc_host_init_slot(int, const sdmmc_slot_config_t *);
int sdmmc_card_init(const sdmmc_host_t *, sdmmc_card_t *);
