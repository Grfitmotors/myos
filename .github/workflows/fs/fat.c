// fat.c - FAT12/FAT16 Filesystem Driver
// Uses LBA28 ATA PIO for disk access

#include "fat.h"
#include "../kernel/kernel.h"
#include "../kernel/vga.h"

// ATA PIO ports (Primary channel)
#define ATA_DATA        0x1F0
#define ATA_ERROR       0x1F1
#define ATA_SECT_COUNT  0x1F2
#define ATA_LBA_LO      0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HI      0x1F5
#define ATA_DRIVE       0x1F6
#define ATA_STATUS      0x1F7
#define ATA_CMD         0x1F7

#define ATA_STATUS_BSY  0x80
#define ATA_STATUS_RDY  0x40
#define ATA_STATUS_DRQ  0x08
#define ATA_STATUS_ERR  0x01
#define ATA_CMD_READ    0x20

static fat_bpb_t bpb;
static bool fat_mounted = false;
static uint8_t fat_table[512 * 18]; // FAT12: max 18 sectors
static uint32_t fat_root_dir_lba;
static uint32_t fat_data_lba;
static uint8_t fat_type = 0;

// Wait for ATA drive to be ready
static bool ata_wait(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        uint8_t s = inb(ATA_STATUS);
        if (s & ATA_STATUS_ERR) return false;
        if (!(s & ATA_STATUS_BSY) && (s & ATA_STATUS_RDY)) return true;
    }
    return false;
}

// Read sectors via LBA28 PIO
bool ata_read_sectors(uint32_t lba, uint8_t count, uint8_t* buf) {
    if (!ata_wait()) return false;

    outb(ATA_DRIVE,      0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_ERROR,      0x00);
    outb(ATA_SECT_COUNT, count);
    outb(ATA_LBA_LO,     (lba & 0xFF));
    outb(ATA_LBA_MID,    (lba >> 8) & 0xFF);
    outb(ATA_LBA_HI,     (lba >> 16) & 0xFF);
    outb(ATA_CMD,        ATA_CMD_READ);

    for (int s = 0; s < count; s++) {
        // Wait for DRQ
        uint32_t timeout = 100000;
        while (timeout--) {
            uint8_t st = inb(ATA_STATUS);
            if (st & ATA_STATUS_ERR) return false;
            if (st & ATA_STATUS_DRQ) break;
        }
        // Read 256 words
        for (int i = 0; i < 256; i++) {
            uint16_t data = inw(ATA_DATA);
            buf[s * 512 + i * 2]     = data & 0xFF;
            buf[s * 512 + i * 2 + 1] = (data >> 8) & 0xFF;
        }
    }
    return true;
}

bool fat_init(void) {
    uint8_t boot_sector[512];

    if (!ata_read_sectors(0, 1, boot_sector)) {
        vga_print("[FAT] ATA read failed\n");
        return false;
    }

    memcpy(&bpb, boot_sector, sizeof(fat_bpb_t));

    // Validate
    if (bpb.bytes_per_sector != 512) {
        vga_print("[FAT] Invalid BPB\n");
        return false;
    }

    // Calculate locations
    uint32_t fat_lba = bpb.reserved_sectors;
    fat_root_dir_lba = fat_lba + (bpb.num_fats * bpb.sectors_per_fat);
    uint32_t root_dir_sectors = (bpb.root_entry_count * 32 + 511) / 512;
    fat_data_lba = fat_root_dir_lba + root_dir_sectors;

    uint32_t total_sectors = bpb.total_sectors16 ? bpb.total_sectors16 : bpb.total_sectors32;
    uint32_t data_sectors = total_sectors - fat_data_lba;
    uint32_t cluster_count = data_sectors / bpb.sectors_per_cluster;

    if (cluster_count < 4085)       fat_type = 12;
    else if (cluster_count < 65525) fat_type = 16;
    else                            fat_type = 32;

    // Load FAT table
    uint8_t fat_sectors = (fat_type == 12) ? 
        (bpb.sectors_per_fat < 18 ? bpb.sectors_per_fat : 18) : 
        (bpb.sectors_per_fat < 36 ? bpb.sectors_per_fat : 36);
    
    ata_read_sectors(fat_lba, fat_sectors, fat_table);

    fat_mounted = true;
    return true;
}

static uint32_t fat_next_cluster(uint32_t cluster) {
    if (fat_type == 12) {
        uint32_t offset = cluster + (cluster / 2);
        uint16_t val = *(uint16_t*)&fat_table[offset];
        if (cluster & 1) val >>= 4;
        else             val &= 0x0FFF;
        return (val >= 0xFF8) ? FAT_EOF : val;
    } else {
        uint16_t val = *(uint16_t*)&fat_table[cluster * 2];
        return (val >= 0xFFF8) ? FAT_EOF : val;
    }
}

static bool fat_read_cluster(uint32_t cluster, uint8_t* buf) {
    uint32_t lba = fat_data_lba + (cluster - 2) * bpb.sectors_per_cluster;
    return ata_read_sectors(lba, bpb.sectors_per_cluster, buf);
}

// List directory entries
uint32_t fat_list_dir(fat_dir_entry_t* entries, uint32_t max) {
    if (!fat_mounted) return 0;

    uint8_t sector[512];
    uint32_t count = 0;
    uint32_t root_sectors = (bpb.root_entry_count * 32 + 511) / 512;

    for (uint32_t s = 0; s < root_sectors && count < max; s++) {
        if (!ata_read_sectors(fat_root_dir_lba + s, 1, sector)) break;

        fat_dir_entry_t* entry = (fat_dir_entry_t*)sector;
        for (uint32_t i = 0; i < 16 && count < max; i++, entry++) {
            if (entry->name[0] == 0x00) goto done;        // End of dir
            if ((uint8_t)entry->name[0] == 0xE5) continue; // Deleted
            if (entry->attrs & 0x08) continue;             // Volume label
            if (entry->attrs & 0x0F) continue;             // LFN
            entries[count++] = *entry;
        }
    }
done:
    return count;
}

// Read a file by name (8.3 format, uppercase, space-padded)
uint32_t fat_read_file(const char* name83, uint8_t* buf, uint32_t buf_size) {
    if (!fat_mounted) return 0;

    uint8_t sector[512];
    uint32_t root_sectors = (bpb.root_entry_count * 32 + 511) / 512;

    for (uint32_t s = 0; s < root_sectors; s++) {
        if (!ata_read_sectors(fat_root_dir_lba + s, 1, sector)) break;

        fat_dir_entry_t* entry = (fat_dir_entry_t*)sector;
        for (uint32_t i = 0; i < 16; i++, entry++) {
            if (entry->name[0] == 0x00) return 0;
            if ((uint8_t)entry->name[0] == 0xE5) continue;
            if (entry->attrs & 0x08) continue;

            if (memcmp(entry->name, name83, 11) == 0) {
                // Found! Read clusters
                uint32_t cluster = entry->start_cluster_lo;
                uint32_t bytes_read = 0;
                uint32_t file_size = entry->file_size;
                uint8_t cluster_buf[512 * 8]; // max 8 sect/cluster

                while (cluster != FAT_EOF && bytes_read < buf_size && bytes_read < file_size) {
                    fat_read_cluster(cluster, cluster_buf);
                    uint32_t chunk = bpb.sectors_per_cluster * 512;
                    if (bytes_read + chunk > buf_size) chunk = buf_size - bytes_read;
                    if (bytes_read + chunk > file_size) chunk = file_size - bytes_read;
                    memcpy(buf + bytes_read, cluster_buf, chunk);
                    bytes_read += chunk;
                    cluster = fat_next_cluster(cluster);
                }
                return bytes_read;
            }
        }
    }
    return 0; // Not found
}

// Convert "FILENAME.EXT" to 8.3 padded format
void fat_name_to_83(const char* name, char* out) {
    memset(out, ' ', 11);
    int ni = 0, oi = 0;
    while (name[ni] && name[ni] != '.' && oi < 8) {
        char c = name[ni++];
        if (c >= 'a' && c <= 'z') c -= 32;
        out[oi++] = c;
    }
    if (name[ni] == '.') {
        ni++;
        int ei = 0;
        while (name[ni] && ei < 3) {
            char c = name[ni++];
            if (c >= 'a' && c <= 'z') c -= 32;
            out[8 + ei++] = c;
        }
    }
}

bool fat_is_mounted(void) {
    return fat_mounted;
}
