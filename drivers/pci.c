#include "../include/types.h"

extern void *kmalloc(size_t s);
extern void printk(const char *s);
extern void printk_info(const char *s);
extern void printk_ok(const char *s);
extern void printk_err(const char *s);
extern void printk_hex(uint64_t n);
extern void printk_dec(uint64_t n);

/* ============================================================
 * MINIOS PCI BUS DRIVER v4.0
 * PCI Configuration Space + Device Detection + BAR + IRQ
 * ============================================================ */

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC
#define PCI_MAX_DEVICES 256
#define PCI_MAX_BUSES 256
#define PCI_MAX_FUNCTIONS 8

/* PCI Configuration Space Offsets */
#define PCI_VENDOR_ID       0x00
#define PCI_DEVICE_ID       0x02
#define PCI_COMMAND         0x04
#define PCI_STATUS          0x06
#define PCI_REVISION_ID     0x08
#define PCI_PROG_IF         0x09
#define PCI_SUBCLASS        0x0A
#define PCI_CLASS_CODE      0x0B
#define PCI_CACHE_LINE_SIZE 0x0C
#define PCI_LATENCY_TIMER   0x0D
#define PCI_HEADER_TYPE     0x0E
#define PCI_BIST            0x0F
#define PCI_BAR0            0x10
#define PCI_BAR1            0x14
#define PCI_BAR2            0x18
#define PCI_BAR3            0x1C
#define PCI_BAR4            0x20
#define PCI_BAR5            0x24
#define PCI_CARDBUS_CIS     0x28
#define PCI_SUBSYSTEM_VENDOR 0x2C
#define PCI_SUBSYSTEM_ID    0x2E
#define PCI_ROM_ADDRESS     0x30
#define PCI_CAPABILITY_LIST 0x34
#define PCI_INTERRUPT_LINE  0x3C
#define PCI_INTERRUPT_PIN   0x3D
#define PCI_MIN_GRANT       0x3E
#define PCI_MAX_LATENCY     0x3F

/* PCI Class Codes */
#define PCI_CLASS_UNCLASSIFIED   0x00
#define PCI_CLASS_STORAGE        0x01
#define PCI_CLASS_NETWORK        0x02
#define PCI_CLASS_DISPLAY        0x03
#define PCI_CLASS_MULTIMEDIA     0x04
#define PCI_CLASS_MEMORY         0x05
#define PCI_CLASS_BRIDGE         0x06
#define PCI_CLASS_COMMUNICATION  0x07
#define PCI_CLASS_SYSTEM         0x08
#define PCI_CLASS_INPUT          0x09
#define PCI_CLASS_DOCKING        0x0A
#define PCI_CLASS_PROCESSOR      0x0B
#define PCI_CLASS_SERIAL_BUS     0x0C
#define PCI_CLASS_WIRELESS       0x0D
#define PCI_CLASS_INTELLIGENT    0x0E
#define PCI_CLASS_SATELLITE      0x0F
#define PCI_CLASS_ENCRYPTION     0x10
#define PCI_CLASS_SIGNAL         0x11
#define PCI_CLASS_ACCELERATOR    0x12

/* PCI Device Structure */
typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t revision;
    uint8_t prog_if;
    uint8_t subclass;
    uint8_t class_code;
    uint8_t header_type;
    uint32_t bar[6];
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    int present;
} pci_device_t;

/* Global device list */
static pci_device_t *pci_devices = NULL;
static int pci_device_count = 0;
static pci_device_t *device_list[PCI_MAX_DEVICES];

/* ============================================================
 * PCI CONFIG SPACE OKUMA/YAZMA
 * ============================================================ */

static uint32_t pci_read_config(uint8_t bus, uint8_t device, 
                                  uint8_t func, uint8_t offset) {
    uint32_t addr = (uint32_t)((bus << 16) | (device << 11) | 
                               (func << 8) | (offset & 0xFC) | 0x80000000);
    __outl(PCI_CONFIG_ADDR, addr);
    return __inl(PCI_CONFIG_DATA);
}

static void pci_write_config(uint8_t bus, uint8_t device, 
                               uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t addr = (uint32_t)((bus << 16) | (device << 11) | 
                               (func << 8) | (offset & 0xFC) | 0x80000000);
    __outl(PCI_CONFIG_ADDR, addr);
    __outl(PCI_CONFIG_DATA, value);
}

static uint16_t pci_read_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off) {
    uint32_t val = pci_read_config(bus, dev, func, off);
    return (uint16_t)((val >> ((off & 2) * 8)) & 0xFFFF);
}

static uint8_t pci_read_byte(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off) {
    uint32_t val = pci_read_config(bus, dev, func, off);
    return (uint8_t)((val >> ((off & 3) * 8)) & 0xFF);
}

/* ============================================================
 * PCI SINIF İSİMLERİ
 * ============================================================ */

static const char *pci_class_name(uint8_t class_code) {
    switch(class_code) {
        case 0x00: return "Unclassified";
        case 0x01: return "Mass Storage Controller";
        case 0x02: return "Network Controller";
        case 0x03: return "Display Controller";
        case 0x04: return "Multimedia Controller";
        case 0x05: return "Memory Controller";
        case 0x06: return "Bridge Device";
        case 0x07: return "Communication Controller";
        case 0x08: return "System Peripheral";
        case 0x09: return "Input Device";
        case 0x0A: return "Docking Station";
        case 0x0B: return "Processor";
        case 0x0C: return "Serial Bus Controller";
        case 0x0D: return "Wireless Controller";
        case 0x0E: return "Intelligent Controller";
        case 0x0F: return "Satellite Controller";
        case 0x10: return "Encryption Controller";
        case 0x11: return "Signal Processing Controller";
        case 0x12: return "Processing Accelerator";
        default: return "Unknown Device";
    }
}

static const char *pci_subclass_name(uint8_t class_code, uint8_t subclass) {
    if(class_code == 0x01 && subclass == 0x06) return "SATA Controller";
    if(class_code == 0x01 && subclass == 0x01) return "IDE Controller";
    if(class_code == 0x02 && subclass == 0x00) return "Ethernet Controller";
    if(class_code == 0x03 && subclass == 0x00) return "VGA Controller";
    if(class_code == 0x04 && subclass == 0x03) return "Audio Device";
    if(class_code == 0x06 && subclass == 0x04) return "PCI-to-PCI Bridge";
    if(class_code == 0x06 && subclass == 0x00) return "Host Bridge";
    if(class_code == 0x06 && subclass == 0x01) return "ISA Bridge";
    if(class_code == 0x0C && subclass == 0x03) return "USB Controller";
    if(class_code == 0x0C && subclass == 0x00) return "FireWire Controller";
    if(class_code == 0x0C && subclass == 0x05) return "SMBus";
    return NULL;
}

/* ============================================================
 * BAR (Base Address Register) Çözümleme
 * ============================================================ */

typedef enum {
    BAR_TYPE_MEMORY = 0,
    BAR_TYPE_IO = 1
} bar_type_t;

typedef struct {
    bar_type_t type;
    uint32_t base;
    uint32_t size;
    int prefetchable;
    int is_64bit;
} bar_info_t;

static void pci_decode_bar(uint32_t bar_value, bar_info_t *info) {
    info->type = (bar_value & 0x01) ? BAR_TYPE_IO : BAR_TYPE_MEMORY;
    
    if(info->type == BAR_TYPE_MEMORY) {
        info->base = bar_value & 0xFFFFFFF0;
        info->prefetchable = (bar_value >> 3) & 0x01;
        info->is_64bit = ((bar_value >> 1) & 0x03) == 0x02;
    } else {
        info->base = bar_value & 0xFFFFFFFC;
        info->prefetchable = 0;
        info->is_64bit = 0;
    }
    info->size = 0; /* BAR size probing sonra yapılır */
}

/* ============================================================
 * PCI CİHAZ TARAMA
 * ============================================================ */

static int pci_scan_device(uint8_t bus, uint8_t device, uint8_t func) {
    uint16_t vendor = pci_read_word(bus, device, func, PCI_VENDOR_ID);
    if(vendor == 0xFFFF) return 0; /* Device yok */
    
    /* Device var! */
    pci_device_t *dev = kmalloc(sizeof(pci_device_t));
    if(!dev) return 0;
    
    dev->vendor_id = vendor;
    dev->device_id = pci_read_word(bus, device, func, PCI_DEVICE_ID);
    dev->command = pci_read_word(bus, device, func, PCI_COMMAND);
    dev->status = pci_read_word(bus, device, func, PCI_STATUS);
    dev->revision = pci_read_byte(bus, device, func, PCI_REVISION_ID);
    dev->prog_if = pci_read_byte(bus, device, func, PCI_PROG_IF);
    dev->subclass = pci_read_byte(bus, device, func, PCI_SUBCLASS);
    dev->class_code = pci_read_byte(bus, device, func, PCI_CLASS_CODE);
    dev->header_type = pci_read_byte(bus, device, func, PCI_HEADER_TYPE);
    dev->interrupt_line = pci_read_byte(bus, device, func, PCI_INTERRUPT_LINE);
    dev->interrupt_pin = pci_read_byte(bus, device, func, PCI_INTERRUPT_PIN);
    dev->bus = bus;
    dev->device = device;
    dev->function = func;
    dev->present = 1;
    
    /* BAR'ları oku */
    for(int i=0; i<6; i++) {
        dev->bar[i] = pci_read_config(bus, device, func, PCI_BAR0 + i*4);
    }
    
    /* Listeye ekle */
    if(pci_device_count < PCI_MAX_DEVICES) {
        device_list[pci_device_count] = dev;
        pci_device_count++;
    }
    
    return 1;
}

static void pci_scan_bus(uint8_t bus);

static void pci_scan_function(uint8_t bus, uint8_t device, uint8_t func) {
    if(pci_scan_device(bus, device, func)) {
        /* Çok fonksiyonlu mu? */
        uint8_t header_type = pci_read_byte(bus, device, func, PCI_HEADER_TYPE);
        
        if(func == 0 && !(header_type & 0x80)) {
            return; /* Tek fonksiyonlu */
        }
        
        /* Diğer fonksiyonları tara */
        for(int f=1; f<PCI_MAX_FUNCTIONS; f++) {
            if(pci_read_word(bus, device, f, PCI_VENDOR_ID) != 0xFFFF) {
                pci_scan_function(bus, device, f);
            }
        }
    }
}

static void pci_scan_bus(uint8_t bus) {
    for(int dev=0; dev<32; dev++) {
        pci_scan_function(bus, dev, 0);
    }
}

/* ============================================================
 * PCI BAŞLATMA
 * ============================================================ */

void pci_init(void) {
    pci_device_count = 0;
    
    printk_info("Scanning PCI bus...");
    
    /* Host bridge kontrolü */
    uint8_t header_type = pci_read_byte(0, 0, 0, PCI_HEADER_TYPE);
    
    if((header_type & 0x80) == 0) {
        /* Tek PCI host controller */
        pci_scan_bus(0);
    } else {
        /* Çoklu host bridge - her birini tara */
        for(int func=0; func<8; func++) {
            if(pci_read_word(0, 0, func, PCI_VENDOR_ID) == 0xFFFF) break;
            
            uint8_t ht = pci_read_byte(0, 0, func, PCI_HEADER_TYPE);
            if((ht & 0x7F) == 0x00) { /* Host bridge */
                pci_scan_bus(func);
            }
            
            if(!(header_type & 0x80)) break;
        }
    }
    
    printk_ok("PCI scan complete");
    printk_info("Devices found: ");
    /* Sayı yazdırma */
}

/* ============================================================
 * PCI CİHAZ SORGULAMA
 * ============================================================ */

pci_device_t *pci_find_device(uint16_t vendor, uint16_t device) {
    for(int i=0; i<pci_device_count; i++) {
        if(device_list[i]->vendor_id == vendor && 
           device_list[i]->device_id == device) {
            return device_list[i];
        }
    }
    return NULL;
}

pci_device_t *pci_find_class(uint8_t class_code, uint8_t subclass) {
    for(int i=0; i<pci_device_count; i++) {
        if(device_list[i]->class_code == class_code && 
           device_list[i]->subclass == subclass) {
            return device_list[i];
        }
    }
    return NULL;
}

/* ============================================================
 * PCI CİHAZ LİSTELEME
 * ============================================================ */

void pci_list_devices(void) {
    printk("\n=== PCI DEVICE LIST ===\n");
    printk("Bus:Dev:Func  Vendor:Device  Class  Name\n");
    printk("--------------------------------------------------\n");
    
    for(int i=0; i<pci_device_count; i++) {
        pci_device_t *dev = device_list[i];
        printk_hex((dev->bus << 8) | dev->device);
        printk(":");
        printk_hex(dev->function);
        printk("  ");
        printk_hex(dev->vendor_id);
        printk(":");
        printk_hex(dev->device_id);
        printk("  ");
        printk_hex(dev->class_code);
        printk(" ");
        printk(pci_class_name(dev->class_code));
        printk("\n");
    }
}

/* ============================================================
 * PCI KONFIGÜRASYON
 * ============================================================ */

void pci_enable_bus_master(pci_device_t *dev) {
    uint16_t cmd = pci_read_word(dev->bus, dev->device, dev->function, PCI_COMMAND);
    cmd |= 0x0004; /* Bus Master Enable */
    pci_write_config(dev->bus, dev->device, dev->function, PCI_COMMAND, 
                     (pci_read_config(dev->bus, dev->device, dev->function, PCI_COMMAND) & 0xFFFF0000) | cmd);
}

void pci_enable_mmio(pci_device_t *dev) {
    uint16_t cmd = pci_read_word(dev->bus, dev->device, dev->function, PCI_COMMAND);
    cmd |= 0x0002; /* Memory Space Enable */
    pci_write_config(dev->bus, dev->device, dev->function, PCI_COMMAND,
                     (pci_read_config(dev->bus, dev->device, dev->function, PCI_COMMAND) & 0xFFFF0000) | cmd);
}

void pci_set_irq(pci_device_t *dev, uint8_t irq) {
    pci_write_config(dev->bus, dev->device, dev->function, PCI_INTERRUPT_LINE,
                     (pci_read_config(dev->bus, dev->device, dev->function, PCI_INTERRUPT_LINE) & 0xFFFFFF00) | irq);
}

int pci_get_device_count(void) {
    return pci_device_count;
}

pci_device_t *pci_get_device(int index) {
    if(index < 0 || index >= pci_device_count) return NULL;
    return device_list[index];
}
