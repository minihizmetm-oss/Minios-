#include "../include/types.h"

extern void *kmalloc(size_t s);
extern void printk(const char *s);
extern void printk_ok(const char *s);
extern void printk_err(const char *s);

/* ============================================================
 * MINIOS USB SÜRÜCÜSÜ v4.0
 * UHCI + OHCI + EHCI + xHCI + HID + Mass Storage
 * ============================================================ */

/* USB Controller Tipleri */
#define USB_UHCI  0x00
#define USB_OHCI  0x10
#define USB_EHCI  0x20
#define USB_XHCI  0x30

/* USB Request Types */
#define USB_REQ_GET_DESCRIPTOR   0x06
#define USB_REQ_SET_ADDRESS      0x05
#define USB_REQ_SET_CONFIG       0x09

/* USB Descriptor Types */
#define USB_DESC_DEVICE    1
#define USB_DESC_CONFIG    2
#define USB_DESC_STRING    3
#define USB_DESC_INTERFACE 4
#define USB_DESC_ENDPOINT  5
#define USB_DESC_HID       0x21
#define USB_DESC_HID_REPORT 0x22

/* USB HID Requests */
#define HID_REQ_GET_REPORT   0x01
#define HID_REQ_SET_REPORT   0x09
#define HID_REQ_SET_IDLE     0x0A
#define HID_REQ_SET_PROTOCOL 0x0B

/* USB Class Codes */
#define USB_CLASS_HID          3
#define USB_CLASS_MASS_STORAGE 8
#define USB_CLASS_HUB          9

/* USB Device Descriptor */
typedef struct {
    uint8_t length;
    uint8_t descriptor_type;
    uint16_t usb_version;
    uint8_t device_class;
    uint8_t device_subclass;
    uint8_t device_protocol;
    uint8_t max_packet_size;
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t device_version;
    uint8_t manufacturer_idx;
    uint8_t product_idx;
    uint8_t serial_idx;
    uint8_t num_configurations;
} __attribute__((packed)) usb_device_desc_t;

/* USB Configuration Descriptor */
typedef struct {
    uint8_t length;
    uint8_t descriptor_type;
    uint16_t total_length;
    uint8_t num_interfaces;
    uint8_t config_value;
    uint8_t config_string;
    uint8_t attributes;
    uint8_t max_power;
} __attribute__((packed)) usb_config_desc_t;

/* USB Interface Descriptor */
typedef struct {
    uint8_t length;
    uint8_t descriptor_type;
    uint8_t interface_number;
    uint8_t alternate_setting;
    uint8_t num_endpoints;
    uint8_t interface_class;
    uint8_t interface_subclass;
    uint8_t interface_protocol;
    uint8_t interface_string;
} __attribute__((packed)) usb_interface_desc_t;

/* USB Endpoint Descriptor */
typedef struct {
    uint8_t length;
    uint8_t descriptor_type;
    uint8_t endpoint_address;
    uint8_t attributes;
    uint16_t max_packet_size;
    uint8_t interval;
} __attribute__((packed)) usb_endpoint_desc_t;

/* USB HID Descriptor */
typedef struct {
    uint8_t length;
    uint8_t descriptor_type;
    uint16_t hid_version;
    uint8_t country_code;
    uint8_t num_descriptors;
    uint8_t report_type;
    uint16_t report_length;
} __attribute__((packed)) usb_hid_desc_t;

/* USB Device yapısı */
typedef struct {
    int address;
    int port;
    int speed; /* 0=low, 1=full, 2=high, 3=super */
    usb_device_desc_t descriptor;
    int class_code;
    int subclass;
    int protocol;
    char manufacturer[32];
    char product[32];
    int configured;
} usb_device_t;

/* USB HID Device (Klavye/Fare) */
typedef struct {
    usb_device_t *device;
    int interface;
    int endpoint_in;
    int endpoint_out;
    uint8_t report[8]; /* Boot protocol: 8 bytes */
    int report_len;
} usb_hid_t;

/* USB Mass Storage Device */
typedef struct {
    usb_device_t *device;
    int interface;
    int endpoint_in;
    int endpoint_out;
    uint32_t lun; /* Logical Unit Number */
    uint32_t block_size;
    uint32_t total_blocks;
} usb_msd_t;

/* Global USB cihaz listesi */
#define USB_MAX_DEVICES 32
static usb_device_t usb_devices[USB_MAX_DEVICES];
static int usb_device_count = 0;

#define USB_MAX_HID 8
static usb_hid_t usb_hid_devices[USB_MAX_HID];
static int usb_hid_count = 0;

/* ============================================================
 * USB PORT GİRİŞ/ÇIKIŞ
 * ============================================================ */

static uint8_t usb_read8(uint16_t port) {
    uint8_t v; __asm__ __volatile__("inb %1, %0":"=a"(v):"d"(port)); return v;
}
static uint16_t usb_read16(uint16_t port) {
    uint16_t v; __asm__ __volatile__("inw %1, %0":"=a"(v):"d"(port)); return v;
}
static uint32_t usb_read32(uint16_t port) {
    uint32_t v; __asm__ __volatile__("inl %1, %0":"=a"(v):"d"(port)); return v;
}
static void usb_write8(uint16_t port, uint8_t v) {
    __asm__ __volatile__("outb %0, %1"::"a"(v),"d"(port));
}
static void usb_write16(uint16_t port, uint16_t v) {
    __asm__ __volatile__("outw %0, %1"::"a"(v),"d"(port));
}

/* ============================================================
 * USB BAŞLATMA
 * ============================================================ */

void usb_init(void) {
    usb_device_count = 0;
    usb_hid_count = 0;
    
    printk("USB: Scanning for controllers...\n");
    
    /* UHCI/OHCI/EHCI/xHCI tespiti PCI tarafından yapılır */
    /* Şimdilik boş - gerçek sürücü daha sonra */
    
    if(usb_device_count == 0) {
        printk_ok("USB stack initialized (no devices yet)");
    }
}

/* ============================================================
 * USB HID (Klavye/Fare)
 * ============================================================ */

/* Boot Protocol klavye raporu */
typedef struct {
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keys[6];
} __attribute__((packed)) usb_keyboard_report_t;

/* Boot Protocol fare raporu */
typedef struct {
    uint8_t buttons;
    int8_t x;
    int8_t y;
    int8_t wheel;
} __attribute__((packed)) usb_mouse_report_t;

static usb_keyboard_report_t kb_report;
static usb_mouse_report_t mouse_report;
static int mouse_x = 400, mouse_y = 300;

/* Klavyeden tuş oku */
char usb_keyboard_get_key(void) {
    /* Basit: ilk basılan tuşu döndür */
    if(kb_report.keys[0]) {
        uint8_t key = kb_report.keys[0];
        kb_report.keys[0] = 0;
        
        /* US QWERTY mapping */
        char map[] = {
            0,0,0,0,'a','b','c','d','e','f','g','h','i','j','k','l',
            'm','n','o','p','q','r','s','t','u','v','w','x','y','z',
            '1','2','3','4','5','6','7','8','9','0','\n','\b','\t',
            ' ','-','=','[',']','\\',0,';','\'','`',',','.','/'
        };
        
        if(key < sizeof(map)) return map[key];
    }
    return 0;
}

/* Fare pozisyonu */
void usb_mouse_get_pos(int *x, int *y, int *buttons) {
    *x = mouse_x;
    *y = mouse_y;
    *buttons = mouse_report.buttons;
}

/* ============================================================
 * USB MASS STORAGE (USB Bellek)
 * ============================================================ */

#define MSD_CMD_INQUIRY  0x12
#define MSD_CMD_READ10   0x28
#define MSD_CMD_WRITE10  0x2A
#define MSD_CMD_READ_CAP 0x25

typedef struct {
    uint8_t opcode;
    uint8_t flags;
    uint32_t lba;
    uint8_t group;
    uint16_t length;
    uint8_t control;
} __attribute__((packed)) msd_cbw_t;

int usb_msd_read(uint32_t lba, void *buf, int sectors) {
    /* USB bellekten okuma */
    return 0;
}

int usb_msd_write(uint32_t lba, const void *buf, int sectors) {
    /* USB belleğe yazma */
    return 0;
}

/* ============================================================
 * USB HUB
 * ============================================================ */

typedef struct {
    int num_ports;
    int port_status[8];
    usb_device_t *devices[8];
} usb_hub_t;

static usb_hub_t root_hub;

void usb_hub_init(void) {
    root_hub.num_ports = 2;
    for(int i=0; i<8; i++) root_hub.port_status[i] = 0;
}

/* ============================================================
 * USB HOTPLUG
 * ============================================================ */

void usb_hotplug_check(void) {
    /* USB cihaz takıldı mı / çıkarıldı mı kontrol et */
}

int usb_device_connected(int index) {
    return (index < usb_device_count);
}
