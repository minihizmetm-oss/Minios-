#include "../include/types.h"

extern void *kmalloc(size_t s);
extern void kfree(void *p, size_t s);
extern void printk(const char *s);
extern void printk_ok(const char *s);
extern void printk_err(const char *s);

/* ============================================================
 * MINIOS VFS (Virtual File System) v4.0
 * ============================================================ */

#define VFS_MAX_FILES 256
#define VFS_MAX_MOUNTS 16
#define VFS_NAME_MAX 256
#define VFS_PATH_MAX 1024

/* Dosya tipleri */
#define VFS_FILE     0
#define VFS_DIR      1
#define VFS_SYMLINK  2
#define VFS_DEVICE   3

/* Dosya izinleri */
#define VFS_PERM_READ  0x04
#define VFS_PERM_WRITE 0x02
#define VFS_PERM_EXEC  0x01

/* Dosya açma flag'leri */
#define O_RDONLY  0x0001
#define O_WRONLY  0x0002
#define O_RDWR    0x0004
#define O_CREAT   0x0010
#define O_TRUNC   0x0020
#define O_APPEND  0x0040

/* VFS Inode */
typedef struct vfs_inode {
    uint32_t ino;
    uint32_t mode;
    uint64_t size;
    uint32_t uid;
    uint32_t gid;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    uint32_t blocks;
    void *private_data;
} vfs_inode_t;

/* VFS Dentry (Directory Entry) */
typedef struct vfs_dentry {
    char name[VFS_NAME_MAX];
    vfs_inode_t *inode;
    struct vfs_dentry *parent;
    struct vfs_dentry *child;
    struct vfs_dentry *next;
    int ref_count;
} vfs_dentry_t;

/* VFS File */
typedef struct vfs_file {
    vfs_dentry_t *dentry;
    vfs_inode_t *inode;
    uint64_t offset;
    uint32_t flags;
    int ref_count;
} vfs_file_t;

/* VFS Mount */
typedef struct vfs_mount {
    char mount_point[VFS_NAME_MAX];
    vfs_dentry_t *root;
    int mounted;
} vfs_mount_t;

/* Global VFS state */
static vfs_dentry_t *root_dentry = NULL;
static vfs_file_t *file_table[VFS_MAX_FILES];
static vfs_mount_t mount_table[VFS_MAX_MOUNTS];
static int mount_count = 0;
static int file_count = 0;

/* ============================================================
 * VFS BAŞLATMA
 * ============================================================ */

void vfs_init(void) {
    /* Root dizin oluştur */
    root_dentry = kmalloc(sizeof(vfs_dentry_t));
    if(!root_dentry) {
        printk_err("VFS: Cannot create root!");
        return;
    }
    
    root_dentry->name[0] = '/';
    root_dentry->name[1] = '\0';
    root_dentry->inode = kmalloc(sizeof(vfs_inode_t));
    if(root_dentry->inode) {
        root_dentry->inode->ino = 0;
        root_dentry->inode->mode = VFS_DIR | 0755;
        root_dentry->inode->size = 0;
        root_dentry->inode->uid = 0;
        root_dentry->inode->gid = 0;
    }
    root_dentry->parent = root_dentry;
    root_dentry->child = NULL;
    root_dentry->next = NULL;
    root_dentry->ref_count = 1;
    
    /* File table'ı temizle */
    for(int i=0; i<VFS_MAX_FILES; i++) file_table[i] = NULL;
    file_count = 0;
    
    printk_ok("VFS initialized");
}

/* ============================================================
 * PATH ÇÖZÜMLEME
 * ============================================================ */

/* Basit path çözümleme - /home/user/file.txt */
static vfs_dentry_t *vfs_lookup(const char *path) {
    if(!path || path[0] != '/') return NULL;
    if(path[1] == '\0') return root_dentry;
    
    vfs_dentry_t *current = root_dentry;
    char component[VFS_NAME_MAX];
    int ci = 0;
    
    for(int i=1; path[i]; i++) {
        if(path[i] == '/') {
            if(ci > 0) {
                component[ci] = '\0';
                
                /* Çocuklarda ara */
                vfs_dentry_t *child = current->child;
                int found = 0;
                while(child) {
                    int match = 1;
                    for(int j=0; component[j] || child->name[j]; j++) {
                        if(component[j] != child->name[j]) {
                            match = 0; break;
                        }
                    }
                    if(match) {
                        current = child;
                        found = 1;
                        break;
                    }
                    child = child->next;
                }
                if(!found) return NULL;
                ci = 0;
            }
        } else {
            if(ci < VFS_NAME_MAX-1) component[ci++] = path[i];
        }
    }
    
    /* Son component */
    if(ci > 0) {
        component[ci] = '\0';
        vfs_dentry_t *child = current->child;
        while(child) {
            int match = 1;
            for(int j=0; component[j] || child->name[j]; j++) {
                if(component[j] != child->name[j]) {
                    match = 0; break;
                }
            }
            if(match) return child;
            child = child->next;
        }
    }
    
    return current;
}

/* ============================================================
 * DOSYA İŞLEMLERİ
 * ============================================================ */

int vfs_open(const char *path, int flags) {
    vfs_dentry_t *dentry = vfs_lookup(path);
    
    if(!dentry && (flags & O_CREAT)) {
        /* Dosya oluştur */
        /* Basit: şimdilik sadece root'ta oluştur */
        dentry = kmalloc(sizeof(vfs_dentry_t));
        if(!dentry) return -1;
        
        /* İsmi path'ten al */
        int last = 0;
        for(int i=0; path[i]; i++) if(path[i] == '/') last = i+1;
        int j=0;
        for(int i=last; path[i] && j<VFS_NAME_MAX-1; i++, j++)
            dentry->name[j] = path[i];
        dentry->name[j] = '\0';
        
        dentry->inode = kmalloc(sizeof(vfs_inode_t));
        dentry->inode->ino = file_count + 1;
        dentry->inode->mode = VFS_FILE | 0644;
        dentry->inode->size = 0;
        dentry->parent = root_dentry;
        dentry->child = NULL;
        dentry->next = root_dentry->child;
        dentry->ref_count = 1;
        root_dentry->child = dentry;
    }
    
    if(!dentry) return -1;
    
    /* Boş slot bul */
    for(int i=0; i<VFS_MAX_FILES; i++) {
        if(!file_table[i]) {
            vfs_file_t *file = kmalloc(sizeof(vfs_file_t));
            file->dentry = dentry;
            file->inode = dentry->inode;
            file->offset = 0;
            file->flags = flags;
            file->ref_count = 1;
            file_table[i] = file;
            file_count++;
            return i; /* File descriptor */
        }
    }
    return -1;
}

int vfs_close(int fd) {
    if(fd < 0 || fd >= VFS_MAX_FILES || !file_table[fd]) return -1;
    file_table[fd]->ref_count--;
    if(file_table[fd]->ref_count <= 0) {
        kfree(file_table[fd], sizeof(vfs_file_t));
        file_table[fd] = NULL;
        file_count--;
    }
    return 0;
}

int vfs_read(int fd, void *buf, int count) {
    if(fd < 0 || fd >= VFS_MAX_FILES || !file_table[fd]) return -1;
    /* Basit: şimdilik 0 döndür */
    return 0;
}

int vfs_write(int fd, const void *buf, int count) {
    if(fd < 0 || fd >= VFS_MAX_FILES || !file_table[fd]) return -1;
    vfs_file_t *file = file_table[fd];
    if(file->inode) file->inode->size += count;
    file->offset += count;
    return count;
}

int vfs_seek(int fd, uint64_t offset) {
    if(fd < 0 || fd >= VFS_MAX_FILES || !file_table[fd]) return -1;
    file_table[fd]->offset = offset;
    return 0;
}

/* ============================================================
 * DİZİN İŞLEMLERİ
 * ============================================================ */

int vfs_mkdir(const char *path) {
    /* Basit dizin oluşturma */
    vfs_dentry_t *dentry = kmalloc(sizeof(vfs_dentry_t));
    if(!dentry) return -1;
    
    int last = 0;
    for(int i=0; path[i]; i++) if(path[i] == '/') last = i+1;
    int j=0;
    for(int i=last; path[i] && j<VFS_NAME_MAX-1; i++, j++)
        dentry->name[j] = path[i];
    dentry->name[j] = '\0';
    
    dentry->inode = kmalloc(sizeof(vfs_inode_t));
    dentry->inode->ino = file_count + 1;
    dentry->inode->mode = VFS_DIR | 0755;
    dentry->parent = root_dentry;
    dentry->child = NULL;
    dentry->next = root_dentry->child;
    root_dentry->child = dentry;
    
    return 0;
}

/* ============================================================
 * DOSYA LİSTELEME
 * ============================================================ */

void vfs_list_dir(const char *path) {
    vfs_dentry_t *dir = vfs_lookup(path);
    if(!dir) {
        printk_err("Directory not found");
        return;
    }
    
    printk("\nDirectory: ");
    printk(path);
    printk("\n");
    printk("----------------------------------------\n");
    
    vfs_dentry_t *child = dir->child;
    int count = 0;
    while(child) {
        if(child->inode) {
            if(child->inode->mode & VFS_DIR) {
                printk("[DIR]  ");
            } else {
                printk("[FILE] ");
            }
            printk(child->name);
            printk("\n");
            count++;
        }
        child = child->next;
    }
    printk("----------------------------------------\n");
    printk("Total: ");
    /* Sayı yazdırma */
    printk(" items\n");
}

/* ============================================================
 * MOUNT İŞLEMLERİ
 * ============================================================ */

int vfs_mount(const char *device, const char *mount_point) {
    if(mount_count >= VFS_MAX_MOUNTS) return -1;
    
    mount_table[mount_count].mount_point[0] = '/';
    mount_table[mount_count].mount_point[1] = '\0';
    mount_table[mount_count].root = root_dentry;
    mount_table[mount_count].mounted = 1;
    mount_count++;
    
    printk_ok("Mounted ");
    printk(device);
    printk(" on ");
    printk(mount_point);
    printk("\n");
    return 0;
}
