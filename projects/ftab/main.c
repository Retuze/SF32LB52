/**
 * @file main.c
 * @brief Generate flash partition table (ftab.bin) for SF32LB52
 *
 * Layout (matching SDK ptab.json):
 *   ftab        @ 0x12000000  32 KB  (sec_config + image headers)
 *   bootloader  @ 0x12010000  64 KB  (ROM copies to RAM 0x20020000)
 *   firmware    @ 0x12020000  ~14 MB (XIP from flash)
 *
 * ROM bootloader reads sec_config at 0x12000000, finds bootloader entry
 * at ftab[3], copies it to 0x20020000, and jumps to it.
 *
 * Build: This is a *host* executable (not cross-compiled). Output is ftab.bin.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ---- Constants from SDK dfu.h ---- */
#define SEC_CONFIG_MAGIC       0x53454346U  /* "SECF" */
#define DFU_FLASH_PARTITION    16
#define DFU_FLASH_IMG_IDX_MAX  (DFU_FLASH_PARTITION - 2)
#define DFU_FLAG_AUTO          2
#define DFU_SIG_KEY_SIZE       256

/* SDK core IDs */
#define CORE_LCPU  0
#define CORE_BL    1
#define CORE_HCPU  2
#define CORE_BOOT  3
#define CORE_MAX   4

/* ---- Flash table entry (16 bytes) ---- */
typedef struct {
    uint32_t base;       /* Flash base address */
    uint32_t size;       /* Partition size in bytes */
    uint32_t xip_base;   /* XIP address (or RAM load addr for bootloader) */
    uint32_t flags;      /* Reserved */
} flash_table_t;

/* ---- Image header (512 bytes on flash) ---- */
typedef struct {
    uint32_t length;     /* Image size */
    uint16_t blksize;    /* Block size (typically 512) */
    uint16_t flags;      /* DFU flags (e.g., DFU_FLAG_AUTO) */
    uint8_t  reserved[504];
} image_header_t;

/* ---- sec_config structure ---- */
typedef struct {
    uint32_t        magic;                              /* 0x000: "SECF" */
    flash_table_t   ftab[DFU_FLASH_PARTITION];          /* 0x004: 16 entries × 16 bytes = 256 bytes */
    uint8_t         sig_pub_key[DFU_SIG_KEY_SIZE];      /* 0x104: 256 bytes (all zero = no secure boot) */
    uint8_t         padding[4096 - 4 - 256 - 256];      /* 0x204: pad to 4096 bytes */
    image_header_t  imgs[DFU_FLASH_IMG_IDX_MAX];        /* 0x1000: 14 image headers × 512 bytes */
    uint32_t        running_imgs[CORE_MAX];             /* Pointers to active image headers */
} sec_config_t;

/* ---- Build sec_config statically ---- */
static sec_config_t sec_config = {
    .magic = SEC_CONFIG_MAGIC,

    .ftab = {
        /* ftab[0]: flash table itself */
        { .base = 0x12000000, .size = 0x00008000, .xip_base = 0, .flags = 0 },
        /* ftab[1]: calibration table */
        { .base = 0x12008000, .size = 0x00008000, .xip_base = 0, .flags = 0 },
        /* ftab[2]: unused */
        { .base = 0, .size = 0, .xip_base = 0, .flags = 0 },
        /* ftab[3]: bootloader (flash → RAM 0x20020000) */
        { .base = 0x12010000, .size = 0x00010000, .xip_base = 0x20020000, .flags = 0 },
        /* ftab[4]: main firmware (XIP from flash) */
        { .base = 0x12020000, .size = 0x00700000, .xip_base = 0x12020000, .flags = 0 },
        /* ftab[5]: bootloader patch (unused) */
        { .base = 0, .size = 0, .xip_base = 0, .flags = 0 },
        /* ftab[6]: unused */
        { .base = 0, .size = 0, .xip_base = 0, .flags = 0 },
        /* ftab[7]: bootloader backup (same as ftab[3]) */
        { .base = 0x12010000, .size = 0x00010000, .xip_base = 0x20020000, .flags = 0 },
        /* ftab[8]: firmware backup (same as ftab[4]) */
        { .base = 0x12020000, .size = 0x00700000, .xip_base = 0x12020000, .flags = 0 },
        /* ftab[9..15]: unused */
        { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, { 0 },
    },

    .sig_pub_key = { 0 },  /* All zeros = no secure boot */

    .imgs = {
        /* imgs[0]: LCPU (unused) */
        { .length = 0xFFFFFFFF, .blksize = 512, .flags = 0 },
        /* imgs[1]: bootloader */
        { .length = 0x00010000, .blksize = 512, .flags = DFU_FLAG_AUTO },
        /* imgs[2]: HCPU firmware */
        { .length = 0x00700000, .blksize = 512, .flags = DFU_FLAG_AUTO },
        /* imgs[3]: boot (unused) */
        { .length = 0xFFFFFFFF, .blksize = 512, .flags = 0 },
        /* imgs[4..5]: LCPU2/BCPU2 unused */
        { .length = 0xFFFFFFFF, .blksize = 512, .flags = 0 },
        { .length = 0xFFFFFFFF, .blksize = 512, .flags = 0 },
        /* imgs[6]: HCPU2 backup firmware */
        { .length = 0x00700000, .blksize = 512, .flags = DFU_FLAG_AUTO },
        /* imgs[7..13]: unused */
        { .length = 0xFFFFFFFF, .blksize = 512, .flags = 0 },
        { .length = 0xFFFFFFFF, .blksize = 512, .flags = 0 },
        { .length = 0xFFFFFFFF, .blksize = 512, .flags = 0 },
        { .length = 0xFFFFFFFF, .blksize = 512, .flags = 0 },
        { .length = 0xFFFFFFFF, .blksize = 512, .flags = 0 },
        { .length = 0xFFFFFFFF, .blksize = 512, .flags = 0 },
        { .length = 0xFFFFFFFF, .blksize = 512, .flags = 0 },
    },

    /* running_imgs: pointers to active image headers */
    .running_imgs = {
        [CORE_LCPU] = 0xFFFFFFFF,                          /* LCPU unused */
        [CORE_BL]   = 0x12000000 + 4096 + 1 * 512,        /* bootloader */
        [CORE_HCPU] = 0x12000000 + 4096 + 2 * 512,        /* firmware */
        [CORE_BOOT] = 0xFFFFFFFF,                          /* boot unused */
    },
};

int main(int argc, char *argv[])
{
    const char *outfile = (argc > 1) ? argv[1] : "ftab.bin";

    FILE *fp = fopen(outfile, "wb");
    if (!fp) {
        fprintf(stderr, "[ftab] Error: cannot open %s for writing\n", outfile);
        return 1;
    }

    size_t written = fwrite(&sec_config, 1, sizeof(sec_config), fp);
    fclose(fp);

    if (written != sizeof(sec_config)) {
        fprintf(stderr, "[ftab] Error: write failed (expected %zu, wrote %zu)\n",
                sizeof(sec_config), written);
        return 1;
    }

    printf("[ftab] Wrote %zu bytes to %s\n", sizeof(sec_config), outfile);
    printf("  sec_config: %zu bytes\n", sizeof(sec_config));
    printf("  ftab[0]: flash_table @ 0x12000000\n");
    printf("  ftab[3]: bootloader  @ 0x12010000 -> RAM 0x20020000\n");
    printf("  ftab[4]: firmware    @ 0x12020000 (XIP)\n");

    return 0;
}
