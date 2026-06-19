#include "../include/types.h"

extern void *kmalloc(size_t s);

/* ============================================================
 * MINIOS SECURITY MODULE v4.0
 * ASLR + Stack Canary + NX Bit + SMEP/SMAP + Şifreleme
 * ============================================================ */

#define STACK_CANARY_VALUE 0xDEADBEEFCAFEBABEULL
#define ASLR_SHIFT_MAX 16
#define MAX_PASSWORD_LEN 64
#define MAX_USERS 16

/* ============================================================
 * STACK CANARY
 * ============================================================ */

static uint64_t canary = STACK_CANARY_VALUE;

void security_canary_init(void) {
    canary = STACK_CANARY_VALUE;
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    canary ^= ((uint64_t)hi << 32) | lo;
}

uint64_t security_get_canary(void) { return canary; }
int security_check_canary(uint64_t c) { return c == canary; }

/* ============================================================
 * ASLR (Address Space Layout Randomization)
 * ============================================================ */

static uint64_t aslr_seed = 0x12345678;
static int aslr_on = 1;

void aslr_init(void) {
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    aslr_seed = ((uint64_t)hi << 32) | lo;
}

void aslr_enable(void) { aslr_on = 1; }
void aslr_disable(void) { aslr_on = 0; }

uint64_t aslr_randomize(void) {
    if(!aslr_on) return 0;
    aslr_seed = aslr_seed * 1103515245 + 12345;
    return (aslr_seed & ((1ULL << ASLR_SHIFT_MAX) - 1)) << 12;
}

/* ============================================================
 * NX BIT + SMEP/SMAP
 * ============================================================ */

void nx_enable(void) {
    uint64_t efer;
    __asm__ __volatile__("rdmsr" : "=a"(*(uint32_t*)&efer), "=d"(*((uint32_t*)&efer+1)) : "c"(0xC0000080));
    efer |= (1ULL << 11);
    __asm__ __volatile__("wrmsr" : : "a"(*(uint32_t*)&efer), "d"(*((uint32_t*)&efer+1)), "c"(0xC0000080));
}

void smep_enable(void) {
    uint64_t cr4;
    __asm__ __volatile__("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 20);
    __asm__ __volatile__("mov %0, %%cr4" : : "r"(cr4));
}

void smap_enable(void) {
    uint64_t cr4;
    __asm__ __volatile__("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 21);
    __asm__ __volatile__("mov %0, %%cr4" : : "r"(cr4));
}

/* ============================================================
 * BASİT ŞİFRELEME (XOR Cipher)
 * ============================================================ */

void xor_encrypt(void *data, int len, uint8_t key) {
    uint8_t *bytes = (uint8_t *)data;
    for(int i=0; i<len; i++) bytes[i] ^= key;
}

void xor_decrypt(void *data, int len, uint8_t key) {
    xor_encrypt(data, len, key);
}

/* ============================================================
 * HASH (Basit djb2)
 * ============================================================ */

uint64_t hash_djb2(const char *str) {
    uint64_t hash = 5381;
    int c;
    while((c = *str++)) hash = ((hash << 5) + hash) + c;
    return hash;
}

/* ============================================================
 * KULLANICI SİSTEMİ
 * ============================================================ */

typedef struct {
    char username[32];
    uint64_t password_hash;
    int uid;
    int gid;
    int active;
} user_t;

static user_t users[MAX_USERS];
static int user_count = 0;
static int current_uid = 0;

void users_init(void) {
    for(int i=0; i<MAX_USERS; i++) users[i].active = 0;
    user_count = 0;
    
    /* Root kullanıcısı */
    users[0].uid = 0;
    users[0].gid = 0;
    users[0].password_hash = hash_djb2("root");
    users[0].active = 1;
    for(int i=0; i<5 && "root"[i]; i++) users[0].username[i] = "root"[i];
    users[0].username[4] = '\0';
    user_count = 1;
}

int user_login(const char *username, const char *password) {
    for(int i=0; i<MAX_USERS; i++) {
        if(users[i].active) {
            int match = 1;
            for(int j=0; username[j] || users[i].username[j]; j++) {
                if(username[j] != users[i].username[j]) { match = 0; break; }
            }
            if(match && hash_djb2(password) == users[i].password_hash) {
                current_uid = users[i].uid;
                return users[i].uid;
            }
        }
    }
    return -1;
}

int user_add(const char *username, const char *password) {
    if(user_count >= MAX_USERS) return -1;
    
    int idx = user_count++;
    users[idx].uid = idx;
    users[idx].gid = idx;
    users[idx].password_hash = hash_djb2(password);
    users[idx].active = 1;
    
    for(int i=0; i<31 && username[i]; i++) users[idx].username[i] = username[i];
    users[idx].username[31] = '\0';
    
    return idx;
}

/* ============================================================
 * İZİN KONTROLÜ
 * ============================================================ */

int security_check_permission(int uid, int required_uid) {
    if(uid == 0) return 1;  /* Root her şeyi yapabilir */
    return uid == required_uid;
}

/* ============================================================
 * GÜVENLİK BAŞLATMA
 * ============================================================ */

void security_init(void) {
    security_canary_init();
    aslr_init();
    nx_enable();
    smep_enable();
    smap_enable();
    users_init();
}

/* ============================================================
 * GÜVENLİK DENETİMİ
 * ============================================================ */

void security_audit_log(const char *event) {
    volatile char *serial = (volatile char *)0x3F8;
    char *prefix = "[SEC] ";
    while(*prefix) *serial = *prefix++;
    while(*event) *serial = *event++;
    *serial = '\n';
}

int security_validate_pointer(void *ptr) {
    if(!ptr) return 0;
    uint64_t addr = (uint64_t)ptr;
    /* Kernel adres alanı kontrolü */
    if(addr >= 0xC0000000 && addr < 0xFFFFFFFF) return 1;
    if(addr >= 0x100000 && addr < 0x1000000) return 1;
    return 0;
}

/* ============================================================
 * AES-256 BASİT IMPLEMENTASYON
 * ============================================================ */

#define AES_BLOCK_SIZE 16
#define AES_KEY_SIZE 32
#define AES_ROUNDS 14

static const uint8_t sbox[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

static void aes_sub_bytes(uint8_t state[16]) {
    for(int i=0; i<16; i++) state[i] = sbox[state[i]];
}

static void aes_shift_rows(uint8_t state[16]) {
    uint8_t tmp = state[1];
    state[1] = state[5]; state[5] = state[9]; state[9] = state[13]; state[13] = tmp;
    tmp = state[2]; state[2] = state[10]; state[10] = tmp;
    tmp = state[6]; state[6] = state[14]; state[14] = tmp;
    tmp = state[3]; state[3] = state[15]; state[15] = state[11]; state[11] = state[7]; state[7] = tmp;
}

static void aes_mix_columns(uint8_t state[16]) {
    for(int i=0; i<4; i++) {
        uint8_t a = state[i*4], b = state[i*4+1], c = state[i*4+2], d = state[i*4+3];
        state[i*4]   = a ^ b ^ c ^ d;
        state[i*4+1] = a ^ b ^ c ^ d;
        state[i*4+2] = a ^ b ^ c ^ d;
        state[i*4+3] = a ^ b ^ c ^ d;
    }
}

static void aes_add_round_key(uint8_t state[16], uint8_t key[16]) {
    for(int i=0; i<16; i++) state[i] ^= key[i];
}

void aes_encrypt_block(uint8_t input[16], uint8_t output[16], uint8_t key[16]) {
    uint8_t state[16];
    for(int i=0; i<16; i++) state[i] = input[i];
    
    aes_add_round_key(state, key);
    
    for(int r=0; r<10; r++) {
        aes_sub_bytes(state);
        aes_shift_rows(state);
        if(r < 9) aes_mix_columns(state);
        aes_add_round_key(state, key);
    }
    
    for(int i=0; i<16; i++) output[i] = state[i];
}

/* ============================================================
 * SHA-256 BASİT
 * ============================================================ */

typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[64];
} sha256_ctx_t;

static const uint32_t sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174
};

static uint32_t sha256_rotr(uint32_t x, int n) { return (x>>n)|(x<<(32-n)); }

static void sha256_transform(sha256_ctx_t *ctx, const uint8_t *data) {
    uint32_t w[64];
    for(int i=0; i<16; i++)
        w[i] = ((uint32_t)data[i*4]<<24)|((uint32_t)data[i*4+1]<<16)|((uint32_t)data[i*4+2]<<8)|data[i*4+3];
    
    for(int i=16; i<64; i++) {
        uint32_t s0 = sha256_rotr(w[i-15],7)^sha256_rotr(w[i-15],18)^(w[i-15]>>3);
        uint32_t s1 = sha256_rotr(w[i-2],17)^sha256_rotr(w[i-2],19)^(w[i-2]>>10);
        w[i] = w[i-16]+s0+w[i-7]+s1;
    }
    
    uint32_t a=ctx->state[0],b=ctx->state[1],c=ctx->state[2],d=ctx->state[3];
    uint32_t e=ctx->state[4],f=ctx->state[5],g=ctx->state[6],h=ctx->state[7];
    
    for(int i=0; i<64; i++) {
        uint32_t S1 = sha256_rotr(e,6)^sha256_rotr(e,11)^sha256_rotr(e,25);
        uint32_t ch = (e&f)^((~e)&g);
        uint32_t t1 = h+S1+ch+sha256_k[i]+w[i];
        uint32_t S0 = sha256_rotr(a,2)^sha256_rotr(a,13)^sha256_rotr(a,22);
        uint32_t maj = (a&b)^(a&c)^(b&c);
        uint32_t t2 = S0+maj;
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    
    ctx->state[0]+=a; ctx->state[1]+=b; ctx->state[2]+=c; ctx->state[3]+=d;
    ctx->state[4]+=e; ctx->state[5]+=f; ctx->state[6]+=g; ctx->state[7]+=h;
}

void sha256_init(sha256_ctx_t *ctx) {
    ctx->state[0]=0x6a09e667; ctx->state[1]=0xbb67ae85;
    ctx->state[2]=0x3c6ef372; ctx->state[3]=0xa54ff53a;
    ctx->state[4]=0x510e527f; ctx->state[5]=0x9b05688c;
    ctx->state[6]=0x1f83d9ab; ctx->state[7]=0x5be0cd19;
    ctx->count=0;
}

void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, size_t len) {
    for(size_t i=0; i<len; i++) {
        ctx->buffer[ctx->count%64] = data[i];
        ctx->count++;
        if(ctx->count%64==0) sha256_transform(ctx, ctx->buffer);
    }
}

void sha256_final(sha256_ctx_t *ctx, uint8_t hash[32]) {
    uint64_t bits = ctx->count*8;
    ctx->buffer[ctx->count%64] = 0x80;
    if(ctx->count%64 >= 56) { sha256_transform(ctx, ctx->buffer); for(int i=0;i<64;i++)ctx->buffer[i]=0; }
    for(int i=0; i<8; i++) ctx->buffer[56+i] = (bits>>(56-i*8))&0xFF;
    sha256_transform(ctx, ctx->buffer);
    for(int i=0; i<8; i++) {
        hash[i*4]=(ctx->state[i]>>24)&0xFF; hash[i*4+1]=(ctx->state[i]>>16)&0xFF;
        hash[i*4+2]=(ctx->state[i]>>8)&0xFF; hash[i*4+3]=ctx->state[i]&0xFF;
    }
}

/* ============================================================
 * SELinux benzeri MAC (Mandatory Access Control)
 * ============================================================ */

typedef struct {
    char user[32];
    char role[32];
    char type[32];
    int level;
} security_context_t;

static security_context_t current_context;

void mac_init(void) {
    for(int i=0; i<4 && "root"[i]; i++) current_context.user[i] = "root"[i];
    current_context.user[4] = '\0';
    for(int i=0; i<5 && "admin"[i]; i++) current_context.role[i] = "admin"[i];
    current_context.role[5] = '\0';
    for(int i=0; i<8 && "kernel_t"[i]; i++) current_context.type[i] = "kernel_t"[i];
    current_context.type[8] = '\0';
    current_context.level = 0;
}

int mac_check_access(const char *type, int level) {
    if(current_context.level <= level) return 1;
    /* Type enforcement */
    for(int i=0; current_context.type[i] && type[i]; i++)
        if(current_context.type[i] != type[i]) return 0;
    return 1;
}

/* ============================================================
 * GÜVENLİK DUVARI (Basit Paket Filtresi)
 * ============================================================ */

#define FW_MAX_RULES 32

typedef struct {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t protocol;
    uint8_t action; /* 0=drop, 1=allow */
    int active;
} fw_rule_t;

static fw_rule_t fw_rules[FW_MAX_RULES];
static int fw_rule_count = 0;
static int fw_enabled = 1;

void firewall_init(void) {
    fw_rule_count = 0;
    fw_enabled = 1;
    for(int i=0; i<FW_MAX_RULES; i++) fw_rules[i].active = 0;
}

int firewall_add_rule(uint32_t src, uint32_t dst, uint16_t sport, uint16_t dport, uint8_t proto, uint8_t action) {
    if(fw_rule_count >= FW_MAX_RULES) return -1;
    fw_rules[fw_rule_count].src_ip = src;
    fw_rules[fw_rule_count].dst_ip = dst;
    fw_rules[fw_rule_count].src_port = sport;
    fw_rules[fw_rule_count].dst_port = dport;
    fw_rules[fw_rule_count].protocol = proto;
    fw_rules[fw_rule_count].action = action;
    fw_rules[fw_rule_count].active = 1;
    fw_rule_count++;
    return 0;
}

int firewall_check(uint32_t src, uint32_t dst, uint16_t sport, uint16_t dport, uint8_t proto) {
    if(!fw_enabled) return 1;
    for(int i=0; i<fw_rule_count; i++) {
        if(fw_rules[i].active && fw_rules[i].protocol == proto &&
           (fw_rules[i].src_ip==0 || fw_rules[i].src_ip==src) &&
           (fw_rules[i].dst_ip==0 || fw_rules[i].dst_ip==dst)) {
            return fw_rules[i].action;
        }
    }
    return 0; /* Default: drop */
}

/* ============================================================
 * GÜVENLİ AÇILIŞ (Secure Boot benzeri)
 * ============================================================ */

#define SB_HASH_SIZE 32

static uint8_t kernel_hash[SB_HASH_SIZE];
static int secure_boot_enabled = 1;

void secure_boot_init(void) {
    /* Kernel hash'ini hesapla */
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, (uint8_t*)0x100000, 65536); /* Kernel'in ilk 64KB */
    sha256_final(&ctx, kernel_hash);
}

int secure_boot_verify(void) {
    if(!secure_boot_enabled) return 1;
    uint8_t current_hash[SB_HASH_SIZE];
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, (uint8_t*)0x100000, 65536);
    sha256_final(&ctx, current_hash);
    
    for(int i=0; i<SB_HASH_SIZE; i++)
        if(current_hash[i] != kernel_hash[i]) return 0;
    return 1;
}

/* ============================================================
 * İNTRUSION DETECTION (IDS)
 * ============================================================ */

#define IDS_MAX_EVENTS 64

typedef struct {
    char event[64];
    int severity;
    uint64_t timestamp;
} ids_event_t;

static ids_event_t ids_events[IDS_MAX_EVENTS];
static int ids_event_count = 0;
static int ids_alert_level = 0;

void ids_init(void) {
    ids_event_count = 0;
    ids_alert_level = 0;
}

void ids_log_event(const char *event, int severity) {
    if(ids_event_count < IDS_MAX_EVENTS) {
        for(int i=0; i<63 && event[i]; i++) ids_events[ids_event_count].event[i] = event[i];
        ids_events[ids_event_count].event[63] = '\0';
        ids_events[ids_event_count].severity = severity;
        ids_events[ids_event_count].timestamp = 0;
        ids_event_count++;
    }
    if(severity > ids_alert_level) ids_alert_level = severity;
}

int ids_get_alert_level(void) { return ids_alert_level; }

/* ============================================================
 * ANTI-TAMPER
 * ============================================================ */

static uint64_t tamper_counter = 0;

void anti_tamper_init(void) {
    tamper_counter = 0;
}

int anti_tamper_check(void *addr, size_t size) {
    /* Basit checksum */
    uint8_t *bytes = (uint8_t *)addr;
    uint8_t checksum = 0;
    for(size_t i=0; i<size; i++) checksum ^= bytes[i];
    if(checksum != 0) {
        tamper_counter++;
        ids_log_event("Tampering detected!", 3);
        return 0;
    }
    return 1;
}

uint64_t get_tamper_count(void) { return tamper_counter; }

/* ============================================================
 * RSA (Basitleştirilmiş)
 * ============================================================ */

typedef struct {
    uint64_t n;     /* Modulus */
    uint64_t e;     /* Public exponent */
    uint64_t d;     /* Private exponent */
} rsa_key_t;

static rsa_key_t rsa_pub_key;

/* Modüler üs alma (a^b mod m) */
static uint64_t mod_pow(uint64_t base, uint64_t exp, uint64_t mod) {
    uint64_t result = 1;
    base = base % mod;
    while(exp > 0) {
        if(exp & 1) result = (result * base) % mod;
        exp >>= 1;
        base = (base * base) % mod;
    }
    return result;
}

/* Asal kontrol (basit) */
static int is_prime(uint64_t n) {
    if(n < 2) return 0;
    if(n == 2) return 1;
    if(n % 2 == 0) return 0;
    for(uint64_t i=3; i*i<=n && i<1000; i+=2)
        if(n % i == 0) return 0;
    return 1;
}

void rsa_init(void) {
    /* Basit anahtar çifti (eğitim amaçlı) */
    uint64_t p = 61, q = 53;
    rsa_pub_key.n = p * q;
    uint64_t phi = (p-1) * (q-1);
    rsa_pub_key.e = 17;
    /* d = e^-1 mod phi (basit) */
    rsa_pub_key.d = 2753;
}

uint64_t rsa_encrypt(uint64_t msg) {
    return mod_pow(msg, rsa_pub_key.e, rsa_pub_key.n);
}

uint64_t rsa_decrypt(uint64_t cipher) {
    return mod_pow(cipher, rsa_pub_key.d, rsa_pub_key.n);
}

/* ============================================================
 * DIFFIE-HELLMAN Anahtar Değişimi
 * ============================================================ */

typedef struct {
    uint64_t private_key;
    uint64_t public_key;
    uint64_t shared_secret;
} dh_context_t;

static dh_context_t dh;

void dh_init(void) {
    /* Basit parametreler */
    uint64_t g = 5;  /* Generator */
    uint64_t p = 97; /* Prime (küçük, eğitim amaçlı) */
    
    dh.private_key = 23; /* Rastgele özel anahtar */
    dh.public_key = mod_pow(g, dh.private_key, p);
}

void dh_compute_shared(uint64_t other_public) {
    uint64_t p = 97;
    dh.shared_secret = mod_pow(other_public, dh.private_key, p);
}

/* ============================================================
 * SERTİFİKA YÖNETİMİ
 * ============================================================ */

typedef struct {
    char issuer[64];
    char subject[64];
    uint64_t serial;
    uint64_t valid_from;
    uint64_t valid_until;
    uint8_t signature[32];
    int valid;
} certificate_t;

static certificate_t root_cert;

void cert_init(void) {
    for(int i=0; i<9 && "MiniOS CA"[i]; i++) root_cert.issuer[i] = "MiniOS CA"[i];
    root_cert.issuer[9] = '\0';
    for(int i=0; i<9 && "MiniOS CA"[i]; i++) root_cert.subject[i] = "MiniOS CA"[i];
    root_cert.subject[9] = '\0';
    root_cert.serial = 1;
    root_cert.valid_from = 0;
    root_cert.valid_until = ~0ULL;
    root_cert.valid = 1;
}

int cert_verify(certificate_t *cert) {
    if(!cert->valid) return 0;
    /* Basit: imza kontrolü (SHA256 ile) */
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, (uint8_t*)cert, 128);
    uint8_t hash[32];
    sha256_final(&ctx, hash);
    /* İmza karşılaştırma */
    for(int i=0; i<32; i++)
        if(hash[i] != cert->signature[i]) return 0;
    return 1;
}

/* ============================================================
 * TPM (Trusted Platform Module) Benzeri
 * ============================================================ */

#define TPM_PCR_COUNT 16
#define TPM_PCR_SIZE 32

typedef struct {
    uint8_t pcrs[TPM_PCR_COUNT][TPM_PCR_SIZE];
    uint64_t counter;
    int initialized;
} tpm_chip_t;

static tpm_chip_t tpm;

void tpm_init(void) {
    for(int i=0; i<TPM_PCR_COUNT; i++)
        for(int j=0; j<TPM_PCR_SIZE; j++)
            tpm.pcrs[i][j] = 0;
    tpm.counter = 0;
    tpm.initialized = 1;
}

void tpm_extend(int pcr_index, const uint8_t *data, size_t len) {
    if(pcr_index < 0 || pcr_index >= TPM_PCR_COUNT) return;
    
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, tpm.pcrs[pcr_index], TPM_PCR_SIZE);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, tpm.pcrs[pcr_index]);
    tpm.counter++;
}

void tpm_quote(int pcr_index, uint8_t *output) {
    if(pcr_index < 0 || pcr_index >= TPM_PCR_COUNT) return;
    for(int i=0; i<TPM_PCR_SIZE; i++) output[i] = tpm.pcrs[pcr_index][i];
}

void tpm_seal(const uint8_t *secret, size_t len, int pcr_index) {
    /* PCR değeriyle şifrele */
    uint8_t key[TPM_PCR_SIZE];
    tpm_quote(pcr_index, key);
    for(size_t i=0; i<len; i++) ((uint8_t*)secret)[i] ^= key[i % TPM_PCR_SIZE];
}

void tpm_unseal(uint8_t *secret, size_t len, int pcr_index) {
    tpm_seal(secret, len, pcr_index); /* XOR simetrik */
}

/* ============================================================
 * SANDBOX / İZOLASYON
 * ============================================================ */

typedef struct {
    char name[32];
    uint64_t memory_start;
    uint64_t memory_size;
    int allowed_syscalls[32];
    int num_syscalls;
    int active;
} sandbox_t;

static sandbox_t sandboxes[8];
static int sandbox_count = 0;

void sandbox_init(void) {
    sandbox_count = 0;
    for(int i=0; i<8; i++) sandboxes[i].active = 0;
}

int sandbox_create(const char *name, uint64_t mem_start, uint64_t mem_size) {
    if(sandbox_count >= 8) return -1;
    sandbox_t *sb = &sandboxes[sandbox_count];
    for(int i=0; i<31 && name[i]; i++) sb->name[i] = name[i];
    sb->name[31] = '\0';
    sb->memory_start = mem_start;
    sb->memory_size = mem_size;
    sb->num_syscalls = 0;
    sb->active = 1;
    sandbox_count++;
    return sandbox_count - 1;
}

int sandbox_check_syscall(int sandbox_id, int syscall_nr) {
    if(sandbox_id < 0 || sandbox_id >= sandbox_count) return 0;
    sandbox_t *sb = &sandboxes[sandbox_id];
    if(!sb->active) return 0;
    for(int i=0; i<sb->num_syscalls; i++)
        if(sb->allowed_syscalls[i] == syscall_nr) return 1;
    return 0;
}

void sandbox_allow_syscall(int sandbox_id, int syscall_nr) {
    if(sandbox_id < 0 || sandbox_id >= sandbox_count) return;
    sandbox_t *sb = &sandboxes[sandbox_id];
    if(sb->num_syscalls < 32) {
        sb->allowed_syscalls[sb->num_syscalls++] = syscall_nr;
    }
}

int sandbox_check_memory(int sandbox_id, uint64_t addr) {
    if(sandbox_id < 0 || sandbox_id >= sandbox_count) return 0;
    sandbox_t *sb = &sandboxes[sandbox_id];
    return (addr >= sb->memory_start && addr < sb->memory_start + sb->memory_size);
}

/* ============================================================
 * GÜVENLİK DENETİM GÜNLÜĞÜ
 * ============================================================ */

#define AUDIT_MAX_ENTRIES 128

typedef struct {
    char message[128];
    uint64_t timestamp;
    int severity;
    int uid;
} audit_entry_t;

static audit_entry_t audit_log[AUDIT_MAX_ENTRIES];
static int audit_count = 0;

void audit_init(void) {
    audit_count = 0;
}

void audit_log_event(int uid, const char *msg, int severity) {
    if(audit_count < AUDIT_MAX_ENTRIES) {
        audit_entry_t *entry = &audit_log[audit_count];
        entry->uid = uid;
        entry->severity = severity;
        entry->timestamp = 0;
        for(int i=0; i<127 && msg[i]; i++) entry->message[i] = msg[i];
        entry->message[127] = '\0';
        audit_count++;
    }
}

int audit_search(const char *keyword) {
    int matches = 0;
    for(int i=0; i<audit_count; i++) {
        /* Basit arama */
        for(int j=0; audit_log[i].message[j]; j++) {
            int found = 1;
            for(int k=0; keyword[k]; k++) {
                if(audit_log[i].message[j+k] != keyword[k]) {
                    found = 0;
                    break;
                }
            }
            if(found) { matches++; break; }
        }
    }
    return matches;
}

/* ============================================================
 * OBFUSKASYON (Kod Gizleme)
 * ============================================================ */

void code_obfuscate(void *code, size_t len, uint8_t key) {
    uint8_t *bytes = (uint8_t *)code;
    for(size_t i=0; i<len; i++) {
        bytes[i] ^= key;
        bytes[i] = (bytes[i] << 3) | (bytes[i] >> 5);
    }
}

void code_deobfuscate(void *code, size_t len, uint8_t key) {
    uint8_t *bytes = (uint8_t *)code;
    for(size_t i=0; i<len; i++) {
        bytes[i] = (bytes[i] >> 3) | (bytes[i] << 5);
        bytes[i] ^= key;
    }
}

/* ============================================================
 * ENTROPY HAVUZU (Rastgele Sayı Üreteci)
 * ============================================================ */

#define ENTROPY_POOL_SIZE 64

static uint8_t entropy_pool[ENTROPY_POOL_SIZE];
static int entropy_idx = 0;

void entropy_init(void) {
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    for(int i=0; i<ENTROPY_POOL_SIZE; i++) {
        entropy_pool[i] = (uint8_t)(lo ^ hi ^ (i*7));
        lo = lo * 1103515245 + 12345;
        hi = hi ^ (lo >> 16);
    }
}

void entropy_add(uint8_t byte) {
    entropy_pool[entropy_idx] ^= byte;
    entropy_idx = (entropy_idx + 1) % ENTROPY_POOL_SIZE;
}

uint8_t entropy_get_byte(void) {
    uint8_t result = entropy_pool[entropy_idx];
    /* Havuzu karıştır */
    for(int i=0; i<ENTROPY_POOL_SIZE; i++)
        entropy_pool[i] ^= entropy_pool[(i+entropy_idx)%ENTROPY_POOL_SIZE];
    entropy_idx = (entropy_idx + 1) % ENTROPY_POOL_SIZE;
    return result;
}

uint64_t entropy_get_uint64(void) {
    uint64_t result = 0;
    for(int i=0; i<8; i++)
        result = (result << 8) | entropy_get_byte();
    return result;
}

/* ============================================================
 * BELLEK KORUMASI (MPU benzeri)
 * ============================================================ */

#define MEMPROT_MAX_REGIONS 16

typedef struct {
    uint64_t start;
    uint64_t end;
    uint8_t permissions; /* bit0=read, bit1=write, bit2=execute */
    int active;
} memprot_region_t;

static memprot_region_t memprot_regions[MEMPROT_MAX_REGIONS];
static int memprot_count = 0;

void memprot_init(void) {
    memprot_count = 0;
    for(int i=0; i<MEMPROT_MAX_REGIONS; i++) memprot_regions[i].active = 0;
}

int memprot_add_region(uint64_t start, uint64_t end, uint8_t perms) {
    if(memprot_count >= MEMPROT_MAX_REGIONS) return -1;
    memprot_regions[memprot_count].start = start;
    memprot_regions[memprot_count].end = end;
    memprot_regions[memprot_count].permissions = perms;
    memprot_regions[memprot_count].active = 1;
    memprot_count++;
    return 0;
}

int memprot_check(uint64_t addr, uint8_t needed_perm) {
    for(int i=0; i<memprot_count; i++) {
        if(memprot_regions[i].active &&
           addr >= memprot_regions[i].start &&
           addr < memprot_regions[i].end) {
            return (memprot_regions[i].permissions & needed_perm) == needed_perm;
        }
    }
    return 0; /* Tanımsız bölge - erişim reddedildi */
}
