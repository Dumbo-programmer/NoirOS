#ifndef FAT_H
#define FAT_H

#include "common.h"

#define FAT_OK            0
#define FAT_ERR_IO       -1
#define FAT_ERR_FORMAT   -2
#define FAT_ERR_NOTFOUND -3
#define FAT_ERR_NOSPACE  -4
#define FAT_ERR_UNSUP    -5

int fat_mount(void);
int fat_format_fat16(u32 total_sectors);
int fat_read_root_file_83(const char name83[11], u8* out, u32 max_len, u32* out_len);
int fat_write_root_file_83(const char name83[11], const u8* data, u32 len);

#endif
