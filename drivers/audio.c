#include "../include/types.h"

extern void *kmalloc(size_t s);
extern void printk(const char *s);
extern void printk_ok(const char *s);
extern void printk_err(const char *s);

/* ============================================================
 * MINIOS SES SÜRÜCÜSÜ v4.0
 * Intel HD Audio / AC'97 / Sound Blaster 16
 * ============================================================ */

/* Ses kartı tipleri */
#define AUDIO_NONE      0
#define AUDIO_AC97      1
#define AUDIO_HDA       2
#define AUDIO_SB16      3

/* AC'97 Portları */
#define AC97_RESET      0x00
#define AC97_MASTER_VOL 0x02
#define AC97_PCM_VOL    0x18
#define AC97_EXT_AUDIO  0x28
#define AC97_PCM_FRONT  0x1C
#define AC97_MIC_VOL    0x0E
#define AC97_LINE_IN    0x10
#define AC97_RECORD_SEL 0x1A
#define AC97_RECORD_GAIN 0x1C
#define AC97_POWERDOWN  0x26

/* Intel HDA Registers */
#define HDA_GCAP        0x00
#define HDA_GCTL        0x08
#define HDA_WAKEEN      0x0C
#define HDA_INTSTS      0x20
#define HDA_INTCTL      0x24
#define HDA_CORB_BASE   0x40
#define HDA_RIRB_BASE   0x50
#define HDA_CORB_WP     0x48
#define HDA_RIRB_WP     0x58
#define HDA_STATESTS    0x0E

/* Ses formatları */
#define SAMPLE_RATE_8000  8000
#define SAMPLE_RATE_11025 11025
#define SAMPLE_RATE_16000 16000
#define SAMPLE_RATE_22050 22050
#define SAMPLE_RATE_44100 44100
#define SAMPLE_RATE_48000 48000

#define BITS_8   8
#define BITS_16  16
#define BITS_24  24

#define CHANNELS_MONO   1
#define CHANNELS_STEREO 2

/* Ses buffer */
#define AUDIO_BUFFER_SIZE 65536 /* 64KB */

/* Ses cihazı yapısı */
typedef struct {
    int type;
    uint16_t base_port;
    uint32_t base_mmio;
    char name[32];
    int sample_rate;
    int bits;
    int channels;
    int volume;
    int muted;
    uint8_t *buffer;
    int buffer_pos;
    int buffer_size;
    int playing;
} audio_device_t;

/* Ses efektleri */
typedef enum {
    EFFECT_NONE = 0,
    EFFECT_ECHO,
    EFFECT_REVERB,
    EFFECT_CHORUS,
    EFFECT_DISTORTION,
    EFFECT_BASS_BOOST
} audio_effect_t;

static audio_device_t audio_dev;

/* ============================================================
 * TEMEL PORT GİRİŞ/ÇIKIŞ
 * ============================================================ */

static uint8_t port_in8(uint16_t port) { 
    uint8_t v; __asm__ __volatile__("inb %1, %0":"=a"(v):"d"(port)); return v; 
}
static uint16_t port_in16(uint16_t port) { 
    uint16_t v; __asm__ __volatile__("inw %1, %0":"=a"(v):"d"(port)); return v; 
}
static uint32_t port_in32(uint16_t port) { 
    uint32_t v; __asm__ __volatile__("inl %1, %0":"=a"(v):"d"(port)); return v; 
}
static void port_out8(uint16_t port, uint8_t v) { 
    __asm__ __volatile__("outb %0, %1"::"a"(v),"d"(port)); 
}
static void port_out16(uint16_t port, uint16_t v) { 
    __asm__ __volatile__("outw %0, %1"::"a"(v),"d"(port)); 
}
static void port_out32(uint16_t port, uint32_t v) { 
    __asm__ __volatile__("outl %0, %1"::"a"(v),"d"(port)); 
}

/* ============================================================
 * AC'97 SES KARTI
 * ============================================================ */

static void ac97_write(uint16_t reg, uint16_t val) {
    port_out16(audio_dev.base_port + reg, val);
}

static uint16_t ac97_read(uint16_t reg) {
    return port_in16(audio_dev.base_port + reg);
}

int ac97_init(uint16_t base) {
    audio_dev.type = AUDIO_AC97;
    audio_dev.base_port = base;
    audio_dev.sample_rate = SAMPLE_RATE_44100;
    audio_dev.bits = BITS_16;
    audio_dev.channels = CHANNELS_STEREO;
    audio_dev.volume = 75;
    audio_dev.muted = 0;
    
    /* Reset */
    ac97_write(AC97_RESET, 0);
    
    /* Ses seviyesi */
    uint16_t vol = (audio_dev.volume * 63) / 100;
    ac97_write(AC97_MASTER_VOL, (vol << 8) | vol);
    ac97_write(AC97_PCM_VOL, (vol << 8) | vol);
    
    /* Extended audio: enable variable rate */
    ac97_write(AC97_EXT_AUDIO, 0x0001);
    
    /* Sample rate */
    ac97_write(0x2C, audio_dev.sample_rate);
    
    audio_dev.buffer = kmalloc(AUDIO_BUFFER_SIZE);
    audio_dev.buffer_pos = 0;
    audio_dev.buffer_size = AUDIO_BUFFER_SIZE;
    audio_dev.playing = 0;
    
    return 0;
}

/* AC'97'ye ses verisi yaz */
void ac97_play(uint8_t *data, int len) {
    if(audio_dev.type != AUDIO_AC97) return;
    
    for(int i=0; i<len && audio_dev.buffer_pos < audio_dev.buffer_size; i++) {
        audio_dev.buffer[audio_dev.buffer_pos++] = data[i];
    }
    audio_dev.playing = 1;
}

/* ============================================================
 * INTEL HDA (High Definition Audio)
 * ============================================================ */

int hda_init(uint32_t mmio_base) {
    audio_dev.type = AUDIO_HDA;
    audio_dev.base_mmio = mmio_base;
    audio_dev.sample_rate = SAMPLE_RATE_44100;
    audio_dev.bits = BITS_16;
    audio_dev.channels = CHANNELS_STEREO;
    audio_dev.volume = 75;
    audio_dev.muted = 0;
    
    /* HDA Controller reset */
    volatile uint32_t *gctl = (volatile uint32_t *)(mmio_base + HDA_GCTL);
    *gctl = 0x00; /* Reset */
    for(volatile int i=0; i<1000; i++); /* Bekle */
    *gctl = 0x01; /* Enable */
    
    audio_dev.buffer = kmalloc(AUDIO_BUFFER_SIZE);
    audio_dev.buffer_pos = 0;
    audio_dev.buffer_size = AUDIO_BUFFER_SIZE;
    audio_dev.playing = 0;
    
    return 0;
}

/* ============================================================
 * SES KONTROL
 * ============================================================ */

void audio_set_volume(int vol) {
    if(vol < 0) vol = 0;
    if(vol > 100) vol = 100;
    audio_dev.volume = vol;
    
    if(audio_dev.type == AUDIO_AC97) {
        uint16_t v = (vol * 63) / 100;
        ac97_write(AC97_MASTER_VOL, (v << 8) | v);
    }
}

void audio_mute(void) {
    audio_dev.muted = 1;
    if(audio_dev.type == AUDIO_AC97) {
        ac97_write(AC97_MASTER_VOL, 0x8000); /* Mute bit */
    }
}

void audio_unmute(void) {
    audio_dev.muted = 0;
    audio_set_volume(audio_dev.volume);
}

/* ============================================================
 * SES EFECTLERİ
 * ============================================================ */

void audio_echo(uint8_t *samples, int len, int delay_ms, float feedback) {
    int delay_samples = (delay_ms * audio_dev.sample_rate) / 1000;
    for(int i=delay_samples; i<len; i++) {
        int16_t *s = (int16_t *)samples;
        s[i] = s[i] + (int16_t)(s[i-delay_samples] * feedback);
    }
}

void audio_reverb(uint8_t *samples, int len) {
    /* Basit reverb: birkaç echo katmanı */
    audio_echo(samples, len, 50, 0.3f);
    audio_echo(samples, len, 100, 0.2f);
    audio_echo(samples, len, 150, 0.1f);
}

/* ============================================================
 * TON ÜRETİCİ (Bip sesi)
 * ============================================================ */

void audio_beep(int frequency, int duration_ms) {
    /* PC Speaker üzerinden bip */
    uint16_t divisor = 1193180 / frequency;
    port_out8(0x43, 0xB6);
    port_out8(0x42, divisor & 0xFF);
    port_out8(0x42, (divisor >> 8) & 0xFF);
    
    uint8_t tmp = port_in8(0x61);
    port_out8(0x61, tmp | 0x03);
    
    /* Bekle */
    for(volatile int i=0; i<duration_ms*1000; i++);
    
    port_out8(0x61, tmp & 0xFC);
}

/* ============================================================
 * SES ÇALMA
 * ============================================================ */

void audio_play_tone(int freq, int ms) {
    audio_beep(freq, ms);
}

void audio_play_melody(void) {
    /* MiniOS açılış melodisi */
    int notes[] = {523, 587, 659, 698, 784, 880, 988, 1047}; /* Do-Re-Mi... */
    int durations[] = {100, 100, 100, 100, 100, 100, 100, 200};
    
    for(int i=0; i<8; i++) {
        audio_play_tone(notes[i], durations[i]);
    }
}

/* ============================================================
 * SES BAŞLATMA
 * ============================================================ */

void audio_init(void) {
    audio_dev.type = AUDIO_NONE;
    audio_dev.volume = 75;
    audio_dev.muted = 0;
    audio_dev.playing = 0;
    
    /* AC'97 tespit etmeye çalış */
    /* Basit: varsayılan portları dene */
    if(ac97_init(0x1000) == 0) {
        printk_ok("AC'97 Audio initialized");
        return;
    }
    
    /* HDA dene */
    if(hda_init(0xFEB00000) == 0) {
        printk_ok("Intel HDA initialized");
        return;
    }
    
    printk_err("No audio device found - PC Speaker only");
}

/* ============================================================
 * WAV DOSYA OKUMA (Basit)
 * ============================================================ */

typedef struct {
    char riff[4];
    uint32_t file_size;
    char wave[4];
    char fmt[4];
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data[4];
    uint32_t data_size;
} __attribute__((packed)) wav_header_t;

int audio_play_wav(uint8_t *wav_data, int len) {
    if(len < (int)sizeof(wav_header_t)) return -1;
    
    wav_header_t *hdr = (wav_header_t *)wav_data;
    
    if(hdr->riff[0]!='R' || hdr->riff[1]!='I' || hdr->riff[2]!='F' || hdr->riff[3]!='F')
        return -1;
    
    audio_dev.sample_rate = hdr->sample_rate;
    audio_dev.channels = hdr->channels;
    audio_dev.bits = hdr->bits_per_sample;
    
    uint8_t *samples = wav_data + sizeof(wav_header_t);
    int samples_len = hdr->data_size;
    
    if(audio_dev.type == AUDIO_AC97) {
        ac97_play(samples, samples_len);
    }
    
    return 0;
}
