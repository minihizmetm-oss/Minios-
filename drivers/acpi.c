#include "../include/types.h"

extern void *kmalloc(size_t s);
extern void printk(const char *s);
extern void printk_ok(const char *s);
extern void printk_err(const char *s);
extern void printk_info(const char *s);
extern void printk_hex(uint64_t n);

/* ============================================================
 * MINIOS ACPI v4.0 - Güç Yönetimi
 * Shutdown, Reboot, Sleep, CPU States
 * ============================================================ */

/* ACPI Tablo imzaları */
#define ACPI_RSDP_SIG     "RSD PTR "
#define ACPI_RSDT_SIG     "RSDT"
#define ACPI_XSDT_SIG     "XSDT"
#define ACPI_FACP_SIG     "FACP"
#define ACPI_FACS_SIG     "FACS"
#define ACPI_DSDT_SIG     "DSDT"
#define ACPI_SSDT_SIG     "SSDT"
#define ACPI_MADT_SIG     "APIC"
#define ACPI_MCFG_SIG     "MCFG"
#define ACPI_HPET_SIG     "HPET"

/* ACPI Sleep States */
#define ACPI_S0  0  /* Çalışıyor */
#define ACPI_S1  1  /* Uyku (CPU durur) */
#define ACPI_S2  2  /* Derin uyku */
#define ACPI_S3  3  /* Askıda (RAM) */
#define ACPI_S4  4  /* Hazırda beklet (Disk) */
#define ACPI_S5  5  /* Kapalı */

/* RSDP (Root System Description Pointer) */
typedef struct {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_addr;
    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t ext_checksum;
    uint8_t reserved[3];
} __attribute__((packed)) acpi_rsdp_t;

/* ACPI Tablo Başlığı */
typedef struct {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_header_t;

/* FADT (Fixed ACPI Description Table) */
typedef struct {
    acpi_header_t header;
    uint32_t firmware_ctrl;
    uint32_t dsdt;
    uint8_t reserved1;
    uint8_t preferred_pm_profile;
    uint16_t sci_int;
    uint32_t smi_cmd;
    uint8_t acpi_enable;
    uint8_t acpi_disable;
    uint8_t s4bios_req;
    uint8_t pstate_cnt;
    uint32_t pm1a_evt_blk;
    uint32_t pm1b_evt_blk;
    uint32_t pm1a_cnt_blk;
    uint32_t pm1b_cnt_blk;
    uint32_t pm2_cnt_blk;
    uint32_t pm_tmr_blk;
    uint32_t gpe0_blk;
    uint32_t gpe1_blk;
    uint8_t pm1_evt_len;
    uint8_t pm1_cnt_len;
    uint8_t pm2_cnt_len;
    uint8_t pm_tmr_len;
    uint8_t gpe0_blk_len;
    uint8_t gpe1_blk_len;
    uint8_t gpe1_base;
    uint8_t cstate_cnt;
    uint16_t p_lvl2_lat;
    uint16_t p_lvl3_lat;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t duty_offset;
    uint8_t duty_width;
    uint8_t day_alrm;
    uint8_t mon_alrm;
    uint8_t century;
} __attribute__((packed)) acpi_fadt_t;

/* ACPI durumu */
static acpi_rsdp_t *rsdp = NULL;
static acpi_fadt_t *fadt = NULL;
static int acpi_enabled = 0;
static uint32_t pm1a_cnt = 0;
static uint32_t smi_cmd = 0;

/* ============================================================
 * ACPI TABLO BULMA
 * ============================================================ */

/* RSDP bul */
static acpi_rsdp_t *acpi_find_rsdp(void) {
    /* EBDA (Extended BIOS Data Area) */
    uint16_t ebda_seg = *((uint16_t *)0x40E);
    uint32_t ebda_addr = ebda_seg << 4;
    
    /* BIOS ROM alanı: 0xE0000 - 0xFFFFF */
    uint32_t bios_start = 0xE0000;
    uint32_t bios_end = 0xFFFFF;
    
    /* EBDA'da ara */
    for(uint32_t addr = ebda_addr; addr < ebda_addr + 1024; addr += 16) {
        char *sig = (char *)addr;
        if(sig[0]=='R' && sig[1]=='S' && sig[2]=='D' && sig[3]==' ' &&
           sig[4]=='P' && sig[5]=='T' && sig[6]=='R' && sig[7]==' ') {
            return (acpi_rsdp_t *)addr;
        }
    }
    
    /* BIOS'ta ara */
    for(uint32_t addr = bios_start; addr < bios_end; addr += 16) {
        char *sig = (char *)addr;
        if(sig[0]=='R' && sig[1]=='S' && sig[2]=='D' && sig[3]==' ' &&
           sig[4]=='P' && sig[5]=='T' && sig[6]=='R' && sig[7]==' ') {
            return (acpi_rsdp_t *)addr;
        }
    }
    
    return NULL;
}

/* ============================================================
 * ACPI GÜÇ İŞLEMLERİ
 * ============================================================ */

/* Shutdown */
void acpi_shutdown(void) {
    if(!acpi_enabled || !fadt) {
        /* ACPI yoksa klavye controller ile kapat */
        __outb(0x64, 0xFE);
        return;
    }
    
    printk_info("ACPI Shutdown...");
    
    /* SLP_TYP = 5 (S5) ve SLP_EN */
    uint16_t val = (5 << 10) | (1 << 13);
    __outl(fadt->pm1a_cnt_blk, val);
    
    /* Bekle */
    while(1) __hlt();
}

/* Reboot */
void acpi_reboot(void) {
    if(!acpi_enabled) {
        /* Klavye controller reset */
        uint8_t good = 0x02;
        while(good & 0x02) good = __inb(0x64);
        __outb(0x64, 0xFE);
        while(1) __hlt();
    }
    
    printk_info("ACPI Reboot...");
    
    /* Reset register */
    if(fadt) {
        __outb(fadt->pm1a_cnt_blk, 0x06);
    }
    
    /* Alternatif: CPU reset */
    uint64_t cr0;
    __asm__ __volatile__("mov %%cr0, %0":"=r"(cr0));
    cr0 &= ~(1ULL << 31); /* Disable paging */
    __asm__ __volatile__("mov %0, %%cr0"::"r"(cr0));
    
    /* Triple fault */
    uint8_t *zero = (uint8_t *)0;
    __asm__ __volatile__("ljmp $0x08, $0x1000");
}

/* Uyku */
void acpi_sleep(int state) {
    if(!acpi_enabled || !fadt) return;
    
    printk_info("ACPI Sleep...");
    
    uint16_t val = (state << 10) | (1 << 13);
    __outl(fadt->pm1a_cnt_blk, val);
}

/* ============================================================
 * CPU GÜÇ YÖNETİMİ
 * ============================================================ */

/* CPU'yu HLT ile bekle */
void cpu_idle(void) {
    __asm__ __volatile__("hlt");
}

/* CPU frekansını değiştir (basit) */
void cpu_set_performance(int level) {
    /* 0-100 arası */
    if(level < 0) level = 0;
    if(level > 100) level = 100;
    
    /* P-state değişimi (ACPI gerektirir) */
    /* Şimdilik sadece HLT oranını ayarla */
}

/* ============================================================
 * ACPI BAŞLATMA
 * ============================================================ */

void acpi_init(void) {
    rsdp = acpi_find_rsdp();
    
    if(!rsdp) {
        printk_err("ACPI not found - using legacy power management");
        acpi_enabled = 0;
        return;
    }
    
    printk_ok("ACPI found: OEM=");
    for(int i=0; i<6; i++) {
        char tmp[2] = {rsdp->oem_id[i], 0};
        printk(tmp);
    }
    printk("\n");
    
    /* RSDT bul */
    acpi_header_t *rsdt = (acpi_header_t *)(uint64_t)rsdp->rsdt_addr;
    
    /* FADT bul */
    int entries = (rsdt->length - sizeof(acpi_header_t)) / 4;
    uint32_t *ptr = (uint32_t *)(rsdt + 1);
    
    for(int i=0; i<entries; i++) {
        acpi_header_t *hdr = (acpi_header_t *)(uint64_t)ptr[i];
        if(hdr->signature[0]=='F' && hdr->signature[1]=='A' &&
           hdr->signature[2]=='C' && hdr->signature[3]=='P') {
            fadt = (acpi_fadt_t *)hdr;
            break;
        }
    }
    
    if(fadt) {
        pm1a_cnt = fadt->pm1a_cnt_blk;
        smi_cmd = fadt->smi_cmd;
        
        /* ACPI'yi etkinleştir */
        if(smi_cmd) {
            __outb(smi_cmd, fadt->acpi_enable);
        }
        
        acpi_enabled = 1;
        printk_ok("ACPI enabled: SMI=0x");
        printk_hex(smi_cmd);
        printk(" PM1a=0x");
        printk_hex(pm1a_cnt);
        printk("\n");
    } else {
        printk_err("FADT not found");
        acpi_enabled = 0;
    }
}

/* ============================================================
 * BATARYA DURUMU (Basit)
 * ============================================================ */

typedef struct {
    int present;
    int charging;
    int level;  /* 0-100 */
    int voltage;
    int temperature;
} battery_status_t;

static battery_status_t battery;

void battery_init(void) {
    battery.present = 0;
    battery.charging = 0;
    battery.level = 100;
}

void battery_update(void) {
    /* Gerçek batarya okuması ACPI gerektirir */
    /* Şimdilik sabit değer */
    battery.present = 1;
    battery.level = 75;
    battery.charging = 0;
}

int battery_get_level(void) { return battery.level; }
int battery_is_charging(void) { return battery.charging; }
