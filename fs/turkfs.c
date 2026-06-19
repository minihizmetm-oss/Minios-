#include "../include/types.h"

extern void *kmalloc(size_t s);
extern void kfree(void *p, size_t s);

/* ============================================================
 * TÜRK-FS v1.0 - Türkiye'nin Yerli Dosya Sistemi 🇹🇷
 * ============================================================ */

#define TURKFS_MAGIC      0x5455524B  /* "TURK" */
#define TURKFS_BLOCK_SIZE 4096
#define TURKFS_MAX_FILES  256
#define TURKFS_NAME_MAX   48

/* TÜRK-FS Süper Blok */
typedef struct {
    uint32_t magic;              /* 0x5455524B = "TURK" */
    char volume_name[32];        /* Disk adı */
    uint32_t block_size;         /* 4096 */
    uint32_t total_blocks;       /* Toplam blok */
    uint32_t free_blocks;        /* Boş blok */
    uint32_t total_inodes;       /* Toplam dosya */
    uint32_t free_inodes;        /* Boş dosya yuvası */
    uint32_t root_inode;         /* Kök dizin */
    uint32_t created_time;       /* Oluşturma zamanı */
    uint32_t mounted_count;      /* Kaç kez mount edildi */
    uint32_t last_check;         /* Son kontrol */
    char creator[32];            /* "MiniOS TÜRK-FS" */
    uint8_t reserved[4012];      /* 4096 byte */
} __attribute__((packed)) turkfs_superblock_t;

/* TÜRK-FS Inode (Dosya bilgisi) */
typedef struct {
    uint16_t mode;               /* Dosya tipi + izinler */
    uint16_t uid;                /* Sahip ID */
    uint32_t size;               /* Byte cinsinden boyut */
    uint32_t blocks[10];         /* Direkt veri blokları */
    uint32_t indirect;           /* Dolaylı blok */
    uint32_t double_indirect;    /* Çift dolaylı blok */
    uint32_t create_time;        /* Oluşturma zamanı */
    uint32_t modify_time;        /* Son değiştirme */
    uint32_t access_time;        /* Son erişim */
    char name[TURKFS_NAME_MAX];  /* Dosya adı */
} __attribute__((packed)) turkfs_inode_t;

/* TÜRK-FS Dizin Girişi */
typedef struct {
    uint32_t inode;              /* Inode numarası */
    uint16_t rec_len;            /* Kayıt uzunluğu */
    uint8_t name_len;            /* İsim uzunluğu */
    uint8_t file_type;           /* 1=file, 2=dir */
    char name[TURKFS_NAME_MAX];  /* Dosya adı */
} __attribute__((packed)) turkfs_dirent_t;

/* TÜRK-FS Ana Yapı */
typedef struct {
    turkfs_superblock_t *sb;
    turkfs_inode_t *inodes;
    uint8_t *block_bitmap;
    uint8_t *inode_bitmap;
    uint8_t *data_blocks;
    int mounted;
    char mount_point[64];
} turkfs_t;

static turkfs_t turkfs;

/* ============================================================
 * TÜRK-FS BAŞLATMA
 * ============================================================ */

int turkfs_init(void) {
    /* Bellekte dosya sistemi oluştur */
    turkfs.sb = kmalloc(sizeof(turkfs_superblock_t));
    if(!turkfs.sb) return -1;
    
    turkfs.sb->magic = TURKFS_MAGIC;
    turkfs.sb->block_size = TURKFS_BLOCK_SIZE;
    turkfs.sb->total_blocks = 1024;  /* 4MB */
    turkfs.sb->free_blocks = 1000;
    turkfs.sb->total_inodes = TURKFS_MAX_FILES;
    turkfs.sb->free_inodes = TURKFS_MAX_FILES;
    turkfs.sb->root_inode = 0;
    turkfs.sb->created_time = 0;
    turkfs.sb->mounted_count = 1;
    
    /* Volume name */
    turkfs.sb->volume_name[0] = 'M'; turkfs.sb->volume_name[1] = 'i';
    turkfs.sb->volume_name[2] = 'n'; turkfs.sb->volume_name[3] = 'i';
    turkfs.sb->volume_name[4] = 'O'; turkfs.sb->volume_name[5] = 'S';
    turkfs.sb->volume_name[6] = '\0';
    
    /* Creator */
    turkfs.sb->creator[0] = 'M'; turkfs.sb->creator[1] = 'i';
    turkfs.sb->creator[2] = 'n'; turkfs.sb->creator[3] = 'i';
    turkfs.sb->creator[4] = 'O'; turkfs.sb->creator[5] = 'S';
    turkfs.sb->creator[6] = ' '; turkfs.sb->creator[7] = 'T';
    turkfs.sb->creator[8] = 0xC3; turkfs.sb->creator[9] = 0x9C; /* Ü */
    turkfs.sb->creator[10] = 'R'; turkfs.sb->creator[11] = 'K';
    turkfs.sb->creator[12] = '-'; turkfs.sb->creator[13] = 'F';
    turkfs.sb->creator[14] = 'S'; turkfs.sb->creator[15] = '\0';
    
    /* Inode tablosu */
    turkfs.inodes = kmalloc(TURKFS_MAX_FILES * sizeof(turkfs_inode_t));
    
    /* Root inode */
    turkfs.inodes[0].mode = 0x41ED; /* Dizin + 0755 */
    turkfs.inodes[0].uid = 0;
    turkfs.inodes[0].size = 0;
    turkfs.inodes[0].create_time = 0;
    turkfs.inodes[0].name[0] = '/';
    turkfs.inodes[0].name[1] = '\0';
    
    /* Bitmap */
    turkfs.block_bitmap = kmalloc(128);
    turkfs.inode_bitmap = kmalloc(32);
    
    turkfs.mounted = 1;
    turkfs.mount_point[0] = '/';
    turkfs.mount_point[1] = '\0';
    
    return 0;
}

/* Dosya oluştur */
int turkfs_create(const char *name, int is_dir) {
    for(int i=1; i<TURKFS_MAX_FILES; i++) {
        if(turkfs.inodes[i].name[0] == '\0') {
            turkfs.inodes[i].mode = is_dir ? 0x41ED : 0x81A4; /* dir:0755, file:0644 */
            turkfs.inodes[i].uid = 0;
            turkfs.inodes[i].size = 0;
            turkfs.inodes[i].create_time = 0;
            
            int j;
            for(j=0; j<TURKFS_NAME_MAX-1 && name[j]; j++)
                turkfs.inodes[i].name[j] = name[j];
            turkfs.inodes[i].name[j] = '\0';
            
            turkfs.sb->free_inodes--;
            return i;
        }
    }
    return -1;
}

/* Dosya listele */
void turkfs_list(void) {
    volatile char *serial = (volatile char *)0x3F8;
    char *hdr = "\n=== TURK-FS DOSYA LISTESI ===\n";
    while(*hdr) *serial = *hdr++;
    
    for(int i=0; i<TURKFS_MAX_FILES; i++) {
        if(turkfs.inodes[i].name[0] != '\0') {
            if(turkfs.inodes[i].mode & 0x4000) {
                char *d = "[DIR]  ";
                while(*d) *serial = *d++;
            } else {
                char *f = "[FILE] ";
                while(*f) *serial = *f++;
            }
            char *n = turkfs.inodes[i].name;
            while(*n) *serial = *n++;
            *serial = '\n';
        }
    }
}
