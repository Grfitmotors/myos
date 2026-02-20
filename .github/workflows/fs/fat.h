// fat.h - FAT12/FAT16 Filesystem
#ifndef FAT_H
#define FAT_H
#include "../kernel/kernel.h"

#define FAT_EOF 0xFFFF

// BIOS Parameter Block (BPB)
typedef struct {
    uint8_t  jmp[3];
    uint8_t  oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors16;
    uint8_t  media_type;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors32;
} __attribute__((packed)) fat_bpb_t;

// 32-byte directory entry
typedef struct {
    char     name[11];         // 8.3 format
    uint8_t  attrs;            // file attributes
    uint8_t  reserved;
    uint8_t  create_time_ms;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t start_cluster_hi; // FAT32 only
    uint16_t modify_time;
    uint16_t modify_date;
    uint16_t start_cluster_lo;
    uint32_t file_size;
} __attribute__((packed)) fat_dir_entry_t;

#define FAT_ATTR_READONLY  0x01
#define FAT_ATTR_HIDDEN    0x02
#define FAT_ATTR_SYSTEM    0x04
#define FAT_ATTR_VOLID     0x08
#define FAT_ATTR_SUBDIR    0x10
#define FAT_ATTR_ARCHIVE   0x20

bool     fat_init(void);
bool     fat_is_mounted(void);
uint32_t fat_list_dir(fat_dir_entry_t* entries, uint32_t max);
uint32_t fat_read_file(const char* name83, uint8_t* buf, uint32_t buf_size);
void     fat_name_to_83(const char* name, char* out);
bool     ata_read_sectors(uint32_t lba, uint8_t count, uint8_t* buf);
#endif
