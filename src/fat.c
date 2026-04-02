#include "../include/fat.h"
#include "../include/ata.h"

#define FAT16_MAX_CLUSTERS 65525U

typedef struct {
    int mounted;
    int fat_type;
    u16 bytes_per_sector;
    u8 sectors_per_cluster;
    u16 reserved_sectors;
    u8 fat_count;
    u32 sectors_per_fat;
    u16 root_entry_count;
    u32 total_sectors;

    u32 root_cluster;
    u32 cluster_count;

    u32 fat_start;
    u32 root_start;
    u32 root_sectors;
    u32 data_start;
} fat_info_t;

static fat_info_t g_fat;

static u16 rd16(const u8* p) {
    return (u16)p[0] | ((u16)p[1] << 8);
}

static u32 rd32(const u8* p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static void wr16(u8* p, u16 v) {
    p[0] = (u8)(v & 0xFF);
    p[1] = (u8)((v >> 8) & 0xFF);
}

static void wr32(u8* p, u32 v) {
    p[0] = (u8)(v & 0xFF);
    p[1] = (u8)((v >> 8) & 0xFF);
    p[2] = (u8)((v >> 16) & 0xFF);
    p[3] = (u8)((v >> 24) & 0xFF);
}

static int fat_read_sector(u32 lba, u8* buf) {
    return ata_read28(lba, buf) == 0 ? FAT_OK : FAT_ERR_IO;
}

static int fat_write_sector(u32 lba, const u8* buf) {
    return ata_write28(lba, buf) == 0 ? FAT_OK : FAT_ERR_IO;
}

static u32 clus_to_lba(u32 cluster) {
    return g_fat.data_start + ((cluster - 2U) * g_fat.sectors_per_cluster);
}

static int fat_eoc(u32 cluster) {
    if (g_fat.fat_type == 16) return cluster >= 0xFFF8U;
    return cluster >= 0x0FFFFFF8U;
}

static u32 fat_eoc_value(void) {
    if (g_fat.fat_type == 16) return 0xFFFFU;
    return 0x0FFFFFFFU;
}

static int fat_calc_layout(const u8* bpb) {
    g_fat.bytes_per_sector = rd16(&bpb[11]);
    g_fat.sectors_per_cluster = bpb[13];
    g_fat.reserved_sectors = rd16(&bpb[14]);
    g_fat.fat_count = bpb[16];
    g_fat.root_entry_count = rd16(&bpb[17]);

    u16 total16 = rd16(&bpb[19]);
    u32 total32 = rd32(&bpb[32]);
    g_fat.total_sectors = total16 ? total16 : total32;

    u16 spf16 = rd16(&bpb[22]);
    u32 spf32 = rd32(&bpb[36]);
    g_fat.sectors_per_fat = spf16 ? (u32)spf16 : spf32;

    if (g_fat.bytes_per_sector != 512 || g_fat.sectors_per_cluster == 0 || g_fat.fat_count == 0) {
        return FAT_ERR_FORMAT;
    }

    g_fat.root_sectors = ((u32)g_fat.root_entry_count * 32U + (g_fat.bytes_per_sector - 1U)) / g_fat.bytes_per_sector;
    g_fat.fat_start = g_fat.reserved_sectors;

    if (spf16 != 0) {
        g_fat.fat_type = 16;
        g_fat.root_start = g_fat.fat_start + (u32)g_fat.fat_count * g_fat.sectors_per_fat;
        g_fat.data_start = g_fat.root_start + g_fat.root_sectors;
        g_fat.root_cluster = 0;
    } else {
        g_fat.fat_type = 32;
        g_fat.root_start = 0;
        g_fat.root_sectors = 0;
        g_fat.root_cluster = rd32(&bpb[44]) & 0x0FFFFFFFU;
        g_fat.data_start = g_fat.reserved_sectors + (u32)g_fat.fat_count * g_fat.sectors_per_fat;
    }

    u32 data_sectors = g_fat.total_sectors - g_fat.data_start;
    g_fat.cluster_count = data_sectors / g_fat.sectors_per_cluster;

    if (g_fat.cluster_count < 4085U) return FAT_ERR_UNSUP;
    return FAT_OK;
}

static int fat_get_entry(u32 cluster, u32* out_next) {
    u8 sec[512];
    u32 off = cluster * (g_fat.fat_type == 16 ? 2U : 4U);
    u32 lba = g_fat.fat_start + (off / 512U);
    u32 idx = off % 512U;
    if (fat_read_sector(lba, sec) != FAT_OK) return FAT_ERR_IO;

    if (g_fat.fat_type == 16) {
        *out_next = rd16(&sec[idx]);
    } else {
        *out_next = rd32(&sec[idx]) & 0x0FFFFFFFU;
    }
    return FAT_OK;
}

static int fat_set_entry_single(u32 fat_base, u32 cluster, u32 value) {
    u8 sec[512];
    u32 off = cluster * (g_fat.fat_type == 16 ? 2U : 4U);
    u32 lba = fat_base + (off / 512U);
    u32 idx = off % 512U;
    if (fat_read_sector(lba, sec) != FAT_OK) return FAT_ERR_IO;

    if (g_fat.fat_type == 16) {
        wr16(&sec[idx], (u16)value);
    } else {
        u32 old = rd32(&sec[idx]);
        u32 merged = (old & 0xF0000000U) | (value & 0x0FFFFFFFU);
        wr32(&sec[idx], merged);
    }
    return fat_write_sector(lba, sec);
}

static int fat_set_entry(u32 cluster, u32 value) {
    for (u8 i = 0; i < g_fat.fat_count; ++i) {
        int r = fat_set_entry_single(g_fat.fat_start + (u32)i * g_fat.sectors_per_fat, cluster, value);
        if (r != FAT_OK) return r;
    }
    return FAT_OK;
}

static int name83_equal(const u8* ent, const char name83[11]) {
    for (int i = 0; i < 11; ++i) if (ent[i] != (u8)name83[i]) return 0;
    return 1;
}

static int root_iter_find(const char name83[11], int want_free, u32* out_lba, u32* out_off) {
    u8 sec[512];

    if (g_fat.fat_type == 16) {
        for (u32 s = 0; s < g_fat.root_sectors; ++s) {
            u32 lba = g_fat.root_start + s;
            if (fat_read_sector(lba, sec) != FAT_OK) return FAT_ERR_IO;

            for (u32 off = 0; off < 512; off += 32) {
                u8 first = sec[off];
                if (want_free) {
                    if (first == 0x00 || first == 0xE5) {
                        *out_lba = lba; *out_off = off; return FAT_OK;
                    }
                } else {
                    if (first == 0x00) return FAT_ERR_NOTFOUND;
                    if (first == 0xE5 || sec[off + 11] == 0x0F) continue;
                    if (name83_equal(&sec[off], name83)) {
                        *out_lba = lba; *out_off = off; return FAT_OK;
                    }
                }
            }
        }
        return want_free ? FAT_ERR_NOSPACE : FAT_ERR_NOTFOUND;
    }

    u32 cl = g_fat.root_cluster;
    while (cl >= 2 && !fat_eoc(cl)) {
        u32 base = clus_to_lba(cl);
        for (u8 s = 0; s < g_fat.sectors_per_cluster; ++s) {
            u32 lba = base + s;
            if (fat_read_sector(lba, sec) != FAT_OK) return FAT_ERR_IO;

            for (u32 off = 0; off < 512; off += 32) {
                u8 first = sec[off];
                if (want_free) {
                    if (first == 0x00 || first == 0xE5) {
                        *out_lba = lba; *out_off = off; return FAT_OK;
                    }
                } else {
                    if (first == 0x00) return FAT_ERR_NOTFOUND;
                    if (first == 0xE5 || sec[off + 11] == 0x0F) continue;
                    if (name83_equal(&sec[off], name83)) {
                        *out_lba = lba; *out_off = off; return FAT_OK;
                    }
                }
            }
        }

        u32 next;
        if (fat_get_entry(cl, &next) != FAT_OK) return FAT_ERR_IO;
        if (next == cl) break;
        cl = next;
    }

    return want_free ? FAT_ERR_NOSPACE : FAT_ERR_NOTFOUND;
}

static int alloc_cluster_chain(u32 cluster_count, u32* first_cluster) {
    u32 prev = 0;
    *first_cluster = 0;

    for (u32 c = 2; c < g_fat.cluster_count + 2U && cluster_count; ++c) {
        u32 n;
        if (fat_get_entry(c, &n) != FAT_OK) return FAT_ERR_IO;
        if (n == 0x00000000U) {
            if (*first_cluster == 0) *first_cluster = c;
            if (prev) {
                if (fat_set_entry(prev, c) != FAT_OK) return FAT_ERR_IO;
            }
            prev = c;
            cluster_count--;
        }
    }

    if (cluster_count != 0 || prev == 0) return FAT_ERR_NOSPACE;
    if (fat_set_entry(prev, fat_eoc_value()) != FAT_OK) return FAT_ERR_IO;
    return FAT_OK;
}

static int free_cluster_chain(u32 first) {
    u32 cur = first;
    while (cur >= 2 && !fat_eoc(cur)) {
        u32 next;
        if (fat_get_entry(cur, &next) != FAT_OK) return FAT_ERR_IO;
        if (fat_set_entry(cur, 0x00000000U) != FAT_OK) return FAT_ERR_IO;
        if (next == cur) break;
        cur = next;
    }
    return FAT_OK;
}

int fat_mount(void) {
    u8 sec[512];
    g_fat.mounted = 0;

    if (!ata_is_ready()) return FAT_ERR_IO;
    if (fat_read_sector(0, sec) != FAT_OK) return FAT_ERR_IO;
    if (sec[510] != 0x55 || sec[511] != 0xAA) return FAT_ERR_FORMAT;

    int r = fat_calc_layout(sec);
    if (r != FAT_OK) return r;

    g_fat.mounted = 1;
    return FAT_OK;
}

int fat_format_fat16(u32 total_sectors) {
    if (!ata_is_ready() || total_sectors < 4096U) return FAT_ERR_IO;

    const u16 bps = 512;
    const u8 spc = 4;
    const u16 reserved = 1;
    const u8 fats = 2;
    const u16 root_entries = 512;
    const u16 root_secs = (u16)(((u32)root_entries * 32U + (bps - 1U)) / bps);

    u32 data_secs = total_sectors - reserved - root_secs;
    u16 spf = (u16)((data_secs / spc + 256U) / 257U);
    data_secs = total_sectors - reserved - ((u32)fats * spf) - root_secs;
    (void)data_secs;

    u8 sec[512];
    for (int i = 0; i < 512; ++i) sec[i] = 0;

    sec[0] = 0xEB; sec[1] = 0x3C; sec[2] = 0x90;
    sec[3] = 'N'; sec[4] = 'O'; sec[5] = 'I'; sec[6] = 'R'; sec[7] = 'O'; sec[8] = 'S'; sec[9] = ' '; sec[10] = ' ';
    wr16(&sec[11], bps);
    sec[13] = spc;
    wr16(&sec[14], reserved);
    sec[16] = fats;
    wr16(&sec[17], root_entries);
    if (total_sectors < 65536U) wr16(&sec[19], (u16)total_sectors);
    sec[21] = 0xF8;
    wr16(&sec[22], spf);
    wr16(&sec[24], 63);
    wr16(&sec[26], 255);
    wr32(&sec[28], 0);
    if (total_sectors >= 65536U) wr32(&sec[32], total_sectors);
    sec[36] = 0x80;
    sec[38] = 0x29;
    wr32(&sec[39], 0x12345678);
    sec[43] = 'N'; sec[44] = 'o'; sec[45] = 'i'; sec[46] = 'r'; sec[47] = 'O'; sec[48] = 'S';
    sec[54] = 'F'; sec[55] = 'A'; sec[56] = 'T'; sec[57] = '1'; sec[58] = '6';
    sec[510] = 0x55; sec[511] = 0xAA;
    if (fat_write_sector(0, sec) != FAT_OK) return FAT_ERR_IO;

    for (u32 s = 1; s < total_sectors; ++s) {
        for (int i = 0; i < 512; ++i) sec[i] = 0;
        if (fat_write_sector(s, sec) != FAT_OK) return FAT_ERR_IO;
    }

    u32 fat0 = reserved;
    u32 fat1 = reserved + spf;
    for (int i = 0; i < 512; ++i) sec[i] = 0;
    sec[0] = 0xF8; sec[1] = 0xFF; sec[2] = 0xFF; sec[3] = 0xFF;
    if (fat_write_sector(fat0, sec) != FAT_OK) return FAT_ERR_IO;
    if (fat_write_sector(fat1, sec) != FAT_OK) return FAT_ERR_IO;

    return fat_mount();
}

int fat_read_root_file_83(const char name83[11], u8* out, u32 max_len, u32* out_len) {
    if (!g_fat.mounted) return FAT_ERR_FORMAT;

    u32 lba, off;
    int f = root_iter_find(name83, 0, &lba, &off);
    if (f != FAT_OK) return f;

    u8 dir_sec[512];
    if (fat_read_sector(lba, dir_sec) != FAT_OK) return FAT_ERR_IO;

    u32 first_cluster;
    if (g_fat.fat_type == 16) {
        first_cluster = rd16(&dir_sec[off + 26]);
    } else {
        first_cluster = ((u32)rd16(&dir_sec[off + 20]) << 16) | rd16(&dir_sec[off + 26]);
        first_cluster &= 0x0FFFFFFFU;
    }
    u32 file_size = rd32(&dir_sec[off + 28]);

    if (out_len) *out_len = file_size;
    if (!out || max_len == 0) return FAT_OK;

    u32 copied = 0;
    u32 cur = first_cluster;
    u8 sec[512];

    while (cur >= 2 && !fat_eoc(cur) && copied < file_size) {
        u32 cl_lba = clus_to_lba(cur);
        for (u8 s = 0; s < g_fat.sectors_per_cluster && copied < file_size; ++s) {
            if (fat_read_sector(cl_lba + s, sec) != FAT_OK) return FAT_ERR_IO;
            for (u32 i = 0; i < 512 && copied < file_size; ++i) {
                if (copied < max_len) out[copied] = sec[i];
                copied++;
            }
        }
        u32 next;
        if (fat_get_entry(cur, &next) != FAT_OK) return FAT_ERR_IO;
        if (next == cur) break;
        cur = next;
    }

    return FAT_OK;
}

int fat_write_root_file_83(const char name83[11], const u8* data, u32 len) {
    if (!g_fat.mounted || !data) return FAT_ERR_FORMAT;

    u32 dir_lba, dir_off;
    int found = root_iter_find(name83, 0, &dir_lba, &dir_off);
    if (found != FAT_OK) {
        int fr = root_iter_find(name83, 1, &dir_lba, &dir_off);
        if (fr != FAT_OK) return fr;
    }

    u8 dir_sec[512];
    if (fat_read_sector(dir_lba, dir_sec) != FAT_OK) return FAT_ERR_IO;

    if (found == FAT_OK) {
        u32 old_first;
        if (g_fat.fat_type == 16) old_first = rd16(&dir_sec[dir_off + 26]);
        else old_first = (((u32)rd16(&dir_sec[dir_off + 20]) << 16) | rd16(&dir_sec[dir_off + 26])) & 0x0FFFFFFFU;

        if (old_first >= 2) {
            int r = free_cluster_chain(old_first);
            if (r != FAT_OK) return r;
        }
    }

    u32 cluster_bytes = (u32)g_fat.sectors_per_cluster * 512U;
    u32 need_clusters = (len + cluster_bytes - 1U) / cluster_bytes;
    if (need_clusters == 0) need_clusters = 1;

    u32 first_cluster = 0;
    int ar = alloc_cluster_chain(need_clusters, &first_cluster);
    if (ar != FAT_OK) return ar;

    u32 cur = first_cluster;
    u32 written = 0;
    u8 sec[512];

    while (cur >= 2 && !fat_eoc(cur)) {
        u32 cl_lba = clus_to_lba(cur);
        for (u8 s = 0; s < g_fat.sectors_per_cluster; ++s) {
            for (u32 i = 0; i < 512; ++i) {
                if (written < len) sec[i] = data[written++];
                else sec[i] = 0;
            }
            if (fat_write_sector(cl_lba + s, sec) != FAT_OK) return FAT_ERR_IO;
        }

        if (written >= len) break;

        u32 next;
        if (fat_get_entry(cur, &next) != FAT_OK) return FAT_ERR_IO;
        if (next == cur) break;
        cur = next;
    }

    for (int i = 0; i < 11; ++i) dir_sec[dir_off + i] = (u8)name83[i];
    dir_sec[dir_off + 11] = 0x20;
    for (int i = 12; i < 32; ++i) dir_sec[dir_off + i] = 0;

    if (g_fat.fat_type == 16) {
        wr16(&dir_sec[dir_off + 26], (u16)(first_cluster & 0xFFFFU));
    } else {
        wr16(&dir_sec[dir_off + 20], (u16)((first_cluster >> 16) & 0xFFFFU));
        wr16(&dir_sec[dir_off + 26], (u16)(first_cluster & 0xFFFFU));
    }
    wr32(&dir_sec[dir_off + 28], len);

    return fat_write_sector(dir_lba, dir_sec);
}
