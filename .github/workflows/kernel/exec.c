// exec.c - Simple EXE/Binary loader
// Supports flat 32-bit binaries (.bin) loaded from FAT filesystem
// For proper PE/EXE support the loader detects the MZ header.
//
// NOTE: Full PE parsing is complex. This loader handles:
//   1. Flat binary (.bin) - just executes at load address
//   2. MZ/PE detection    - extracts entry point and jumps
//
// Programs run in RING 0 (kernel mode) for simplicity.
// A proper OS would use ring 3 + paging for user programs.

#include "exec.h"
#include "../kernel/kernel.h"
#include "../kernel/vga.h"
#include "../fs/fat.h"

// Load address for user programs
#define PROG_LOAD_ADDR 0x400000    // 4 MB

// MZ DOS header magic
#define MZ_MAGIC   0x5A4D

// PE signature
#define PE_MAGIC   0x00004550

typedef struct {
    uint16_t magic;         // MZ
    uint16_t e_cblp;
    uint16_t e_cp;
    uint16_t e_crlc;
    uint16_t e_cparhdr;
    uint16_t e_minalloc;
    uint16_t e_maxalloc;
    uint16_t e_ss;
    uint16_t e_sp;
    uint16_t e_csum;
    uint16_t e_ip;
    uint16_t e_cs;
    uint16_t e_lfarlc;
    uint16_t e_ovno;
    uint16_t e_res[4];
    uint16_t e_oemid;
    uint16_t e_oeminfo;
    uint16_t e_res2[10];
    uint32_t e_lfanew;      // Offset to PE header
} __attribute__((packed)) mz_header_t;

typedef struct {
    uint32_t signature;     // PE\0\0
    uint16_t machine;       // 0x014C = x86
    uint16_t num_sections;
    uint32_t timestamp;
    uint32_t symbol_table;
    uint32_t num_symbols;
    uint16_t opt_header_size;
    uint16_t characteristics;
} __attribute__((packed)) pe_file_header_t;

typedef struct {
    uint16_t magic;         // 0x010B = PE32
    uint8_t  major_linker;
    uint8_t  minor_linker;
    uint32_t code_size;
    uint32_t init_data_size;
    uint32_t uninit_data_size;
    uint32_t entry_point;   // RVA of entry point
    uint32_t base_of_code;
    uint32_t base_of_data;
    uint32_t image_base;    // Default load address
    uint32_t section_align;
    uint32_t file_align;
    // ... more fields follow
} __attribute__((packed)) pe_opt_header_t;

#define MAX_PROG_SIZE (1024 * 1024)   // 1 MB max program

exec_result_t exec_load(const char* filename) {
    exec_result_t result = {0};
    static uint8_t prog_buf[MAX_PROG_SIZE];

    if (!fat_is_mounted()) {
        vga_set_color(VGA_COLOR_RED, VGA_COLOR_BLACK);
        vga_print("[EXEC] Filesystem not mounted!\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        result.error = EXEC_ERR_NO_FS;
        return result;
    }

    // Convert filename to 8.3 format
    char name83[12];
    fat_name_to_83(filename, name83);

    // Load file from FAT
    uint32_t size = fat_read_file(name83, prog_buf, MAX_PROG_SIZE);
    if (size == 0) {
        vga_set_color(VGA_COLOR_RED, VGA_COLOR_BLACK);
        vga_print("[EXEC] File not found: ");
        vga_print(filename);
        vga_print("\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        result.error = EXEC_ERR_NOT_FOUND;
        return result;
    }

    vga_print("[EXEC] Loaded ");
    vga_print(filename);
    vga_print(" (");
    vga_print_dec(size);
    vga_print(" bytes)\n");

    // Check for MZ/PE header
    mz_header_t* mz = (mz_header_t*)prog_buf;

    if (mz->magic == MZ_MAGIC && size >= sizeof(mz_header_t)) {
        // Check for PE
        uint32_t pe_offset = mz->e_lfanew;
        if (pe_offset + sizeof(pe_file_header_t) + 4 < size) {
            uint32_t* pe_sig = (uint32_t*)(prog_buf + pe_offset);
            if (*pe_sig == PE_MAGIC) {
                pe_file_header_t* pef = (pe_file_header_t*)(prog_buf + pe_offset);
                pe_opt_header_t* peo = (pe_opt_header_t*)(prog_buf + pe_offset + sizeof(pe_file_header_t));

                if (pef->machine != 0x014C) {
                    vga_print("[EXEC] Not an x86 PE binary!\n");
                    result.error = EXEC_ERR_BAD_FORMAT;
                    return result;
                }

                // Copy to load address
                uint8_t* load_addr = (uint8_t*)PROG_LOAD_ADDR;
                memcpy(load_addr, prog_buf, size);

                uint32_t entry = peo->image_base + peo->entry_point;

                vga_print("[EXEC] PE entry point: ");
                vga_print_hex(entry);
                vga_print("\n[EXEC] Executing...\n");

                // Execute the program (call as function)
                typedef int (*prog_func_t)(void);
                prog_func_t prog = (prog_func_t)entry;
                result.exit_code = prog();
                result.error = EXEC_OK;
                return result;
            }
        }
        vga_print("[EXEC] MZ but no PE header - not supported\n");
        result.error = EXEC_ERR_BAD_FORMAT;
        return result;
    }

    // Treat as flat binary - copy and execute
    uint8_t* load_addr = (uint8_t*)PROG_LOAD_ADDR;
    memcpy(load_addr, prog_buf, size);

    vga_print("[EXEC] Flat binary, executing at ");
    vga_print_hex(PROG_LOAD_ADDR);
    vga_print("\n");

    typedef int (*prog_func_t)(void);
    prog_func_t prog = (prog_func_t)PROG_LOAD_ADDR;
    result.exit_code = prog();
    result.error = EXEC_OK;
    return result;
}
