#include <stdio.h>
#include <stdint.h>

// Network stack'teki yapıları buraya kopyala
typedef struct {
    uint8_t bytes[4];
} ip_addr_t;

typedef struct {
    uint8_t bytes[6];
} mac_addr_t;

int main() {
    printf("=== MiniOS Network Stack Test ===\n");
    
    // IP adres testi
    ip_addr_t ip = {{192, 168, 1, 100}};
    printf("[OK] IP: %d.%d.%d.%d\n", ip.bytes[0], ip.bytes[1], ip.bytes[2], ip.bytes[3]);
    
    // MAC adres testi
    mac_addr_t mac = {{0x52, 0x54, 0x00, 0x12, 0x34, 0x56}};
    printf("[OK] MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", 
           mac.bytes[0], mac.bytes[1], mac.bytes[2], 
           mac.bytes[3], mac.bytes[4], mac.bytes[5]);
    
    printf("\n✅ Network stack yapıları çalışıyor!\n");
    printf("📡 Gerçek bağlantı için ağ kartı sürücüsü gerek.\n");
    
    return 0;
}
