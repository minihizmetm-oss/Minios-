#include "../include/types.h"

/* ============================================================
 * INTEL E1000 AĞ KARTI SÜRÜCÜSÜ
 * QEMU'da test edilebilir!
 * ============================================================ */

/* E1000 Register adresleri */
#define E1000_REG_CTRL    0x0000
#define E1000_REG_STATUS  0x0008
#define E1000_REG_EERD    0x0014
#define E1000_REG_IMS     0x00D0
#define E1000_REG_IMC     0x00D8
#define E1000_REG_RCTL    0x0100
#define E1000_REG_TCTL    0x0400
#define E1000_REG_TIPG    0x0410
#define E1000_REG_RDBAL   0x2800
#define E1000_REG_RDBAH   0x2804
#define E1000_REG_RDLEN   0x2808
#define E1000_REG_RDH     0x2810
#define E1000_REG_RDT     0x2818
#define E1000_REG_RXDCTL  0x3828
#define E1000_REG_TDBAL   0x3800
#define E1000_REG_TDBAH   0x3804
#define E1000_REG_TDLEN   0x3808
#define E1000_REG_TDH     0x3810
#define E1000_REG_TDT     0x3818
#define E1000_REG_TXDCTL  0x3828
#define E1000_REG_RA      0x5400
#define E1000_REG_MTA     0x5200

#define E1000_RCTL_EN     (1 << 1)
#define E1000_TCTL_EN     (1 << 1)
#define E1000_TCTL_PSP    (1 << 3)

/* PCI BAR0 adresi - QEMU'da genelde 0xFEB00000 */
static uint32_t e1000_base = 0xFEB00000;

/* Register okuma/yazma */
static uint32_t e1000_read(uint32_t reg) {
    volatile uint32_t *ptr = (volatile uint32_t *)(e1000_base + reg);
    return *ptr;
}

static void e1000_write(uint32_t reg, uint32_t val) {
    volatile uint32_t *ptr = (volatile uint32_t *)(e1000_base + reg);
    *ptr = val;
}

/* MAC adresini oku */
void e1000_get_mac(uint8_t mac[6]) {
    uint32_t low = e1000_read(E1000_REG_RA);
    uint32_t high = e1000_read(E1000_REG_RA + 4);
    
    mac[0] = low & 0xFF;
    mac[1] = (low >> 8) & 0xFF;
    mac[2] = (low >> 16) & 0xFF;
    mac[3] = (low >> 24) & 0xFF;
    mac[4] = high & 0xFF;
    mac[5] = (high >> 8) & 0xFF;
}

/* E1000 başlat */
int e1000_init(void) {
    uint32_t status = e1000_read(E1000_REG_STATUS);
    
    /* Kart var mı? */
    if(status == 0 || status == 0xFFFFFFFF) {
        return -1;
    }
    
    /* Reset */
    e1000_write(E1000_REG_CTRL, e1000_read(E1000_REG_CTRL) | (1 << 26));
    while(e1000_read(E1000_REG_CTRL) & (1 << 26));
    
    /* Alıcıyı etkinleştir */
    e1000_write(E1000_REG_RCTL, E1000_RCTL_EN);
    
    /* Vericiyi etkinleştir */
    e1000_write(E1000_REG_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP);
    
    return 0;
}

/* Paket gönder */
int e1000_send(void *data, uint16_t len) {
    /* TX descriptor'a yaz */
    volatile uint8_t *tx = (volatile uint8_t *)0x200000;
    for(int i=0; i<len; i++) tx[i] = ((uint8_t*)data)[i];
    
    /* Gönder */
    e1000_write(E1000_REG_TDT, 1);
    
    return len;
}

/* Paket al */
int e1000_recv(void *buf, uint16_t max_len) {
    volatile uint8_t *rx = (volatile uint8_t *)0x300000;
    
    /* Paket var mı? */
    uint32_t rdt = e1000_read(E1000_REG_RDT);
    uint32_t rdh = e1000_read(E1000_REG_RDH);
    
    if(rdt == rdh) return 0;
    
    /* Paketi kopyala */
    uint16_t len = 1514;
    if(len > max_len) len = max_len;
    for(int i=0; i<len; i++) ((uint8_t*)buf)[i] = rx[i];
    
    /* Alındı olarak işaretle */
    e1000_write(E1000_REG_RDT, (rdt + 1) % 256);
    
    return len;
}
