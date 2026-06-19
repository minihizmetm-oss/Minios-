#include "../include/types.h"

extern void *kmalloc(size_t s);
extern void kfree(void *p, size_t s);

/* ============================================================
 * MINIOS ELF LOADER v4.0
 * Executable and Linkable Format yükleyici
 * ============================================================ */

/* ELF Header */
#define ELF_MAGIC "\x7F""ELF"

#define ELF_CLASS_32 1
#define ELF_CLASS_64 2

#define ELF_DATA_LSB 1  /* Little Endian */
#define ELF_DATA_MSB 2  /* Big Endian */

#define ELF_TYPE_NONE 0
#define ELF_TYPE_REL  1  /* Relocatable */
#define ELF_TYPE_EXEC 2  /* Executable */
#define ELF_TYPE_DYN  3  /* Shared object */
#define ELF_TYPE_CORE 4

#define ELF_MACH_X86    3   /* Intel 80386 */
#define ELF_MACH_X86_64 62  /* AMD x86-64 */
#define ELF_MACH_ARM    40  /* ARM */

/* ELF Header (32-bit) */
typedef struct {
    uint8_t  magic[4];        /* 0x7F, 'E', 'L', 'F' */
    uint8_t  class;           /* 1=32bit, 2=64bit */
    uint8_t  data;            /* 1=little, 2=big */
    uint8_t  version;         /* 1=current */
    uint8_t  os_abi;          /* 0=UNIX System V */
    uint8_t  abi_version;
    uint8_t  padding[7];
    uint16_t type;            /* 1=reloc, 2=exec, 3=shared */
    uint16_t machine;         /* 3=x86, 62=x86-64 */
    uint32_t version2;
    uint32_t entry;           /* Giriş noktası */
    uint32_t phoff;           /* Program header offset */
    uint32_t shoff;           /* Section header offset */
    uint32_t flags;
    uint16_t ehsize;          /* ELF header size */
    uint16_t phentsize;       /* Program header entry size */
    uint16_t phnum;           /* Program header entry count */
    uint16_t shentsize;       /* Section header entry size */
    uint16_t shnum;           /* Section header entry count */
    uint16_t shstrndx;        /* Section name string table index */
} __attribute__((packed)) elf_header_t;

/* ELF Program Header */
typedef struct {
    uint32_t type;            /* 1=LOAD, 2=DYNAMIC, 3=INTERP, 4=NOTE */
    uint32_t offset;          /* Dosyadaki offset */
    uint32_t vaddr;           /* Sanal adres */
    uint32_t paddr;           /* Fiziksel adres */
    uint32_t filesz;          /* Dosyadaki boyut */
    uint32_t memsz;           /* Bellekteki boyut */
    uint32_t flags;           /* PF_R=4, PF_W=2, PF_X=1 */
    uint32_t align;           /* Hizalama */
} __attribute__((packed)) elf_program_header_t;

/* ELF Section Header */
typedef struct {
    uint32_t name;            /* Section name index */
    uint32_t type;            /* 1=PROGBITS, 2=SYMTAB, 3=STRTAB, 8=NOBITS */
    uint32_t flags;           /* SHF_WRITE=1, SHF_ALLOC=2, SHF_EXEC=4 */
    uint32_t addr;            /* Sanal adres */
    uint32_t offset;          /* Dosyadaki offset */
    uint32_t size;            /* Boyut */
    uint32_t link;
    uint32_t info;
    uint32_t addralign;
    uint32_t entsize;
} __attribute__((packed)) elf_section_header_t;

/* ELF Symbol */
typedef struct {
    uint32_t name;            /* İsim index */
    uint32_t value;           /* Değer/adres */
    uint32_t size;            /* Boyut */
    uint8_t  info;            /* Type + Binding */
    uint8_t  other;           /* Visibility */
    uint16_t shndx;           /* Section index */
} __attribute__((packed)) elf_symbol_t;

/* Program Header Tipleri */
#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_SHLIB   5
#define PT_PHDR    6

/* Program Header Flags */
#define PF_X 1  /* Execute */
#define PF_W 2  /* Write */
#define PF_R 4  /* Read */

/* ============================================================
 * ELF DOĞRULAMA
 * ============================================================ */

static int elf_validate(elf_header_t *hdr) {
    /* Magic kontrol */
    if(hdr->magic[0]!=0x7F || hdr->magic[1]!='E' || 
       hdr->magic[2]!='L' || hdr->magic[3]!='F') {
        return -1;
    }
    
    /* 32-bit mi? */
    if(hdr->class != ELF_CLASS_32) {
        return -2; /* Sadece 32-bit destekleniyor */
    }
    
    /* Little endian mı? */
    if(hdr->data != ELF_DATA_LSB) {
        return -3;
    }
    
    /* x86 mi? */
    if(hdr->machine != ELF_MACH_X86) {
        return -4;
    }
    
    return 0;
}

/* ============================================================
 * ELF YÜKLEME
 * ============================================================ */

typedef struct {
    elf_header_t *header;
    void *entry_point;
    void *base_addr;
    uint32_t size;
    int loaded;
} elf_process_t;

elf_process_t *elf_load(uint8_t *elf_data, uint32_t size) {
    elf_header_t *hdr = (elf_header_t *)elf_data;
    
    /* Doğrula */
    if(elf_validate(hdr) != 0) {
        return NULL;
    }
    
    /* Process yapısı oluştur */
    elf_process_t *proc = kmalloc(sizeof(elf_process_t));
    if(!proc) return NULL;
    
    proc->header = hdr;
    proc->loaded = 0;
    
    /* Program header'ları işle */
    elf_program_header_t *phdr = (elf_program_header_t *)(elf_data + hdr->phoff);
    
    for(int i=0; i<hdr->phnum; i++) {
        if(phdr[i].type == PT_LOAD) {
            /* Bellek ayır ve segmenti yükle */
            uint32_t mem_size = phdr[i].memsz;
            void *mem = kmalloc(mem_size);
            
            if(mem) {
                /* Dosyadan belleğe kopyala */
                uint8_t *src = elf_data + phdr[i].offset;
                uint8_t *dst = (uint8_t *)mem;
                uint32_t copy_size = phdr[i].filesz;
                
                for(uint32_t j=0; j<copy_size; j++) {
                    dst[j] = src[j];
                }
                
                /* Kalan kısmı sıfırla (BSS) */
                for(uint32_t j=copy_size; j<mem_size; j++) {
                    dst[j] = 0;
                }
                
                if(proc->base_addr == NULL) {
                    proc->base_addr = mem;
                    proc->size = mem_size;
                }
            }
        }
    }
    
    /* Giriş noktasını ayarla */
    proc->entry_point = (void *)((uint32_t)proc->base_addr + (hdr->entry - 0x1000));
    proc->loaded = 1;
    
    return proc;
}

/* ============================================================
 * ELF ÇALIŞTIRMA
 * ============================================================ */

typedef int (*elf_entry_t)(int argc, char **argv);

int elf_execute(elf_process_t *proc, int argc, char **argv) {
    if(!proc || !proc->loaded) return -1;
    
    /* Giriş noktasını çağır */
    elf_entry_t entry = (elf_entry_t)proc->entry_point;
    
    /* Programı çalıştır */
    int result = entry(argc, argv);
    
    return result;
}

/* ============================================================
 * ELF TEMİZLEME
 * ============================================================ */

void elf_unload(elf_process_t *proc) {
    if(!proc) return;
    if(proc->base_addr) {
        kfree(proc->base_addr, proc->size);
    }
    kfree(proc, sizeof(elf_process_t));
}

/* ============================================================
 * ELF BİLGİ
 * ============================================================ */

void elf_print_info(elf_header_t *hdr) {
    volatile char *serial = (volatile char *)0x3F8;
    char *msg;
    
    msg = "\n=== ELF DOSYA BILGISI ===\n"; while(*msg) *serial = *msg++;
    msg = "Type: "; while(*msg) *serial = *msg++;
    
    switch(hdr->type) {
        case ELF_TYPE_REL: msg = "Relocatable\n"; break;
        case ELF_TYPE_EXEC: msg = "Executable\n"; break;
        case ELF_TYPE_DYN: msg = "Shared Object\n"; break;
        default: msg = "Unknown\n";
    }
    while(*msg) *serial = *msg++;
    
    msg = "Machine: "; while(*msg) *serial = *msg++;
    if(hdr->machine == ELF_MACH_X86) msg = "x86\n";
    else if(hdr->machine == ELF_MACH_X86_64) msg = "x86-64\n";
    else msg = "Unknown\n";
    while(*msg) *serial = *msg++;
    
    msg = "Entry: 0x"; while(*msg) *serial = *msg++;
    /* Hex yaz */
    uint32_t entry = hdr->entry;
    char hex[] = "0123456789ABCDEF";
    *serial = hex[(entry>>28)&0xF]; *serial = hex[(entry>>24)&0xF];
    *serial = hex[(entry>>20)&0xF]; *serial = hex[(entry>>16)&0xF];
    *serial = hex[(entry>>12)&0xF]; *serial = hex[(entry>>8)&0xF];
    *serial = hex[(entry>>4)&0xF]; *serial = hex[entry&0xF];
    *serial = '\n';
}
