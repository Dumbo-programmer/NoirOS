#include "../include/ata.h"
#include "../include/io.h"

#define ATA_IO_BASE      0x1F0
#define ATA_REG_DATA     (ATA_IO_BASE + 0)
#define ATA_REG_ERROR    (ATA_IO_BASE + 1)
#define ATA_REG_SECCNT   (ATA_IO_BASE + 2)
#define ATA_REG_LBA0     (ATA_IO_BASE + 3)
#define ATA_REG_LBA1     (ATA_IO_BASE + 4)
#define ATA_REG_LBA2     (ATA_IO_BASE + 5)
#define ATA_REG_HDDEVSEL (ATA_IO_BASE + 6)
#define ATA_REG_STATUS   (ATA_IO_BASE + 7)
#define ATA_REG_COMMAND  (ATA_IO_BASE + 7)

#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_READ28   0x20
#define ATA_CMD_WRITE28  0x30
#define ATA_CMD_FLUSH    0xE7

static int g_ready = 0;
static u32 g_sectors = 0;

static int ata_wait_not_busy(void) {
    for (int t = 0; t < 1000000; ++t) {
        if ((io_in8(ATA_REG_STATUS) & 0x80) == 0) return 0;
    }
    return -1;
}

static int ata_wait_drq_or_err(void) {
    for (int t = 0; t < 1000000; ++t) {
        u8 s = io_in8(ATA_REG_STATUS);
        if (s & 0x01) return -1;
        if (s & 0x08) return 0;
    }
    return -1;
}

int ata_init(void) {
    g_ready = 0;
    g_sectors = 0;

    io_out8(ATA_REG_HDDEVSEL, 0xA0);
    io_wait();

    io_out8(ATA_REG_SECCNT, 0);
    io_out8(ATA_REG_LBA0, 0);
    io_out8(ATA_REG_LBA1, 0);
    io_out8(ATA_REG_LBA2, 0);
    io_out8(ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    u8 status = io_in8(ATA_REG_STATUS);
    if (status == 0) return -1;

    if (ata_wait_not_busy() != 0) return -1;

    if (io_in8(ATA_REG_LBA1) != 0 || io_in8(ATA_REG_LBA2) != 0) return -1;

    if (ata_wait_drq_or_err() != 0) return -1;

    u16 identify[256];
    for (int i = 0; i < 256; ++i) identify[i] = io_in16(ATA_REG_DATA);

    g_sectors = ((u32)identify[61] << 16) | identify[60];
    if (g_sectors == 0) return -1;

    g_ready = 1;
    return 0;
}

int ata_is_ready(void) {
    return g_ready;
}

u32 ata_sector_count(void) {
    return g_sectors;
}

int ata_read28(u32 lba, u8* out512) {
    if (!g_ready || !out512) return -1;
    if (lba >= g_sectors || lba > 0x0FFFFFFFU) return -1;

    if (ata_wait_not_busy() != 0) return -1;

    io_out8(ATA_REG_HDDEVSEL, (u8)(0xE0 | ((lba >> 24) & 0x0F)));
    io_out8(ATA_REG_SECCNT, 1);
    io_out8(ATA_REG_LBA0, (u8)(lba & 0xFF));
    io_out8(ATA_REG_LBA1, (u8)((lba >> 8) & 0xFF));
    io_out8(ATA_REG_LBA2, (u8)((lba >> 16) & 0xFF));
    io_out8(ATA_REG_COMMAND, ATA_CMD_READ28);

    if (ata_wait_drq_or_err() != 0) return -1;

    for (int i = 0; i < 256; ++i) {
        u16 w = io_in16(ATA_REG_DATA);
        out512[i * 2] = (u8)(w & 0xFF);
        out512[i * 2 + 1] = (u8)(w >> 8);
    }

    return 0;
}

int ata_write28(u32 lba, const u8* in512) {
    if (!g_ready || !in512) return -1;
    if (lba >= g_sectors || lba > 0x0FFFFFFFU) return -1;

    if (ata_wait_not_busy() != 0) return -1;

    io_out8(ATA_REG_HDDEVSEL, (u8)(0xE0 | ((lba >> 24) & 0x0F)));
    io_out8(ATA_REG_SECCNT, 1);
    io_out8(ATA_REG_LBA0, (u8)(lba & 0xFF));
    io_out8(ATA_REG_LBA1, (u8)((lba >> 8) & 0xFF));
    io_out8(ATA_REG_LBA2, (u8)((lba >> 16) & 0xFF));
    io_out8(ATA_REG_COMMAND, ATA_CMD_WRITE28);

    if (ata_wait_drq_or_err() != 0) return -1;

    for (int i = 0; i < 256; ++i) {
        u16 w = (u16)in512[i * 2] | ((u16)in512[i * 2 + 1] << 8);
        io_out16(ATA_REG_DATA, w);
    }

    io_out8(ATA_REG_COMMAND, ATA_CMD_FLUSH);
    if (ata_wait_not_busy() != 0) return -1;

    return 0;
}
