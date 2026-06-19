extern void printk(const char *s);
#include "../include/types.h"

extern void *kmalloc(size_t s);
extern void kfree(void *p, size_t s);

/* ============================================================
 * MINIOS DISK SÜRÜCÜSÜ v4.0
 * ATA/IDE + AHCI/SATA + NVMe
 * ============================================================ */

/* Disk tipleri */
#define DISK_TYPE_IDE   0
#define DISK_TYPE_SATA  1
#define DISK_TYPE_NVME  2

/* ATA Portları */
#define ATA_PRIMARY_DATA     0x1F0
#define ATA_PRIMARY_ERROR    0x1F1
#define ATA_PRIMARY_SECCOUNT 0x1F2
#define ATA_PRIMARY_LBA_LO   0x1F3
#define ATA_PRIMARY_LBA_MID  0x1F4
#define ATA_PRIMARY_LBA_HI   0x1F5
#define ATA_PRIMARY_DRIVE    0x1F6
#define ATA_PRIMARY_COMMAND  0x1F7
#define ATA_PRIMARY_STATUS   0x1F7
#define ATA_PRIMARY_CONTROL  0x3F6

#define ATA_SECONDARY_DATA     0x170
#define ATA_SECONDARY_ERROR    0x171
#define ATA_SECONDARY_SECCOUNT 0x172
#define ATA_SECONDARY_LBA_LO   0x173
#define ATA_SECONDARY_LBA_MID  0x174
#define ATA_SECONDARY_LBA_HI   0x175
#define ATA_SECONDARY_DRIVE    0x176
#define ATA_SECONDARY_COMMAND  0x177
#define ATA_SECONDARY_STATUS   0x177
#define ATA_SECONDARY_CONTROL  0x376

/* ATA Komutları */
#define ATA_CMD_IDENTIFY    0xEC
#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_READ_DMA    0xC8
#define ATA_CMD_WRITE_DMA   0xCA
#define ATA_CMD_FLUSH       0xE7

/* ATA Status */
#define ATA_SR_BSY  0x80  /* Busy */
#define ATA_SR_DRDY 0x40  /* Drive ready */
#define ATA_SR_DRQ  0x08  /* Data request */
#define ATA_SR_ERR  0x01  /* Error */

/* ATA Identity */
typedef struct {
    uint16_t config;
    uint16_t cylinders;
    uint16_t reserved1;
    uint16_t heads;
    uint16_t unused1[2];
    uint16_t sectors_per_track;
    uint16_t unused2[3];
    char serial[20];
    uint16_t unused3[3];
    char firmware[8];
    char model[40];
    uint16_t sectors_per_int;
    uint16_t unused4;
    uint16_t capabilities[2];
    uint16_t unused5[2];
    uint16_t valid_ext;
    uint16_t unused6[5];
    uint16_t size_of_rw_mult;
    uint32_t sectors_28;
    uint16_t unused7[38];
    uint64_t sectors_48;
} __attribute__((packed)) ata_identify_t;

/* Disk yapısı */
typedef struct {
    int type;
    int port_base;
    int slave;
    char model[41];
    char serial[21];
    uint64_t total_sectors;
    uint32_t sector_size;
    int present;
} disk_device_t;

#define MAX_DISKS 4
static disk_device_t disks[MAX_DISKS];
static int disk_count = 0;

/* ============================================================
 * ATA PORT İŞLEMLERİ
 * ============================================================ */

static uint8_t ata_read8(uint16_t port) {
    uint8_t v; __asm__ __volatile__("inb %1, %0":"=a"(v):"d"(port)); return v;
}
static uint16_t ata_read16(uint16_t port) {
    uint16_t v; __asm__ __volatile__("inw %1, %0":"=a"(v):"d"(port)); return v;
}
static void ata_write8(uint16_t port, uint8_t v) {
    __asm__ __volatile__("outb %0, %1"::"a"(v),"d"(port));
}

/* Status bekle */
static int ata_wait_bsy(int base) {
    for(int i=0; i<100000; i++) {
        if(!(ata_read8(base + 7) & ATA_SR_BSY)) return 0;
    }
    return -1;
}

static int ata_wait_drq(int base) {
    for(int i=0; i<100000; i++) {
        if(ata_read8(base + 7) & ATA_SR_DRQ) return 0;
    }
    return -1;
}

/* ============================================================
 * ATA IDENTIFY (Disk bilgisi)
 * ============================================================ */

static int ata_identify(disk_device_t *disk) {
    int base = disk->port_base;
    
    /* Drive/Head register */
    ata_write8(base + 6, disk->slave ? 0xB0 : 0xA0);
    
    /* Sector count, LBA */
    ata_write8(base + 2, 0);
    ata_write8(base + 3, 0);
    ata_write8(base + 4, 0);
    ata_write8(base + 5, 0);
    
    /* IDENTIFY komutu */
    ata_write8(base + 7, ATA_CMD_IDENTIFY);
    
    /* Status kontrol */
    uint8_t status = ata_read8(base + 7);
    if(status == 0) return -1;
    
    ata_wait_bsy(base);
    
    /* Identify verisini oku */
    uint16_t identify[256];
    for(int i=0; i<256; i++) {
        identify[i] = ata_read16(base);
    }
    
    ata_identify_t *id = (ata_identify_t *)identify;
    
    /* Model (byte swap gerekli) */
    for(int i=0; i<40; i+=2) {
        disk->model[i] = id->model[i+1];
        disk->model[i+1] = id->model[i];
    }
    disk->model[40] = '\0';
    
    /* Serial */
    for(int i=0; i<20; i+=2) {
        disk->serial[i] = id->serial[i+1];
        disk->serial[i+1] = id->serial[i];
    }
    disk->serial[20] = '\0';
    
    /* Sektör sayısı */
    disk->total_sectors = id->sectors_28;
    disk->sector_size = 512;
    
    if(id->valid_ext & 0x0400) {
        /* 48-bit LBA desteği */
        disk->total_sectors = id->sectors_48;
    }
    
    disk->present = 1;
    return 0;
}

/* ============================================================
 * ATA OKUMA
 * ============================================================ */

int ata_read_sectors(disk_device_t *disk, uint64_t lba, 
                     uint8_t *buf, uint32_t count) {
    int base = disk->port_base;
    
    /* LBA28 modu */
    ata_write8(base + 6, 0xE0 | (disk->slave ? 0x10 : 0) | ((lba >> 24) & 0x0F));
    ata_write8(base + 2, count);
    ata_write8(base + 3, lba & 0xFF);
    ata_write8(base + 4, (lba >> 8) & 0xFF);
    ata_write8(base + 5, (lba >> 16) & 0xFF);
    ata_write8(base + 7, ATA_CMD_READ_PIO);
    
    /* Her sektör için */
    for(uint32_t s=0; s<count; s++) {
        ata_wait_bsy(base);
        ata_wait_drq(base);
        
        /* 256 word (512 byte) oku */
        uint16_t *dst = (uint16_t *)(buf + s * 512);
        for(int i=0; i<256; i++) {
            dst[i] = ata_read16(base);
        }
    }
    
    return count * 512;
}

/* ============================================================
 * ATA YAZMA
 * ============================================================ */

int ata_write_sectors(disk_device_t *disk, uint64_t lba,
                      const uint8_t *buf, uint32_t count) {
    int base = disk->port_base;
    
    ata_write8(base + 6, 0xE0 | (disk->slave ? 0x10 : 0) | ((lba >> 24) & 0x0F));
    ata_write8(base + 2, count);
    ata_write8(base + 3, lba & 0xFF);
    ata_write8(base + 4, (lba >> 8) & 0xFF);
    ata_write8(base + 5, (lba >> 16) & 0xFF);
    ata_write8(base + 7, ATA_CMD_WRITE_PIO);
    
    for(uint32_t s=0; s<count; s++) {
        ata_wait_bsy(base);
        ata_wait_drq(base);
        
        uint16_t *src = (uint16_t *)(buf + s * 512);
        for(int i=0; i<256; i++) {
            ata_write8(base, src[i] & 0xFF);
            ata_write8(base, (src[i] >> 8) & 0xFF);
        }
    }
    
    /* Cache flush */
    ata_write8(base + 7, ATA_CMD_FLUSH);
    
    return count * 512;
}

/* ============================================================
 * DISK TARAMA
 * ============================================================ */

void disk_init(void) {
    disk_count = 0;
    
    /* Primary Master */
    disks[disk_count].type = DISK_TYPE_IDE;
    disks[disk_count].port_base = ATA_PRIMARY_DATA;
    disks[disk_count].slave = 0;
    if(ata_identify(&disks[disk_count]) == 0) {
        printk("IDE Primary Master: ");
        printk(disks[disk_count].model);
        printk("\n");
        disk_count++;
    }
    
    /* Primary Slave */
    disks[disk_count].type = DISK_TYPE_IDE;
    disks[disk_count].port_base = ATA_PRIMARY_DATA;
    disks[disk_count].slave = 1;
    if(ata_identify(&disks[disk_count]) == 0) {
        printk("IDE Primary Slave: ");
        printk(disks[disk_count].model);
        printk("\n");
        disk_count++;
    }
}

int disk_read(int disk_idx, uint64_t lba, void *buf, uint32_t count) {
    if(disk_idx < 0 || disk_idx >= disk_count) return -1;
    return ata_read_sectors(&disks[disk_idx], lba, buf, count);
}

int disk_write(int disk_idx, uint64_t lba, const void *buf, uint32_t count) {
    if(disk_idx < 0 || disk_idx >= disk_count) return -1;
    return ata_write_sectors(&disks[disk_idx], lba, buf, count);
}
