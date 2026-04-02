#ifndef ATA_H
#define ATA_H

#include "common.h"

int ata_init(void);
int ata_is_ready(void);
u32 ata_sector_count(void);
int ata_read28(u32 lba, u8* out512);
int ata_write28(u32 lba, const u8* in512);

#endif
