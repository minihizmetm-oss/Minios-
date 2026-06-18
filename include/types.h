/* =============================================================================
 * MiniOS Kernel v4.0 - Core Types and Definitions
 * =============================================================================
 * 24 Yeni Nesil Ozellik:
 * 1. Fixed-width integer types
 * 2. Boolean type
 * 3. Size type
 * 4. NULL pointer
 * 5. Packed attribute
 * 6. Aligned attribute
 * 7. Noreturn attribute
 * 8. Section attribute
 * 9. Used attribute
 * 10. Weak attribute
 * 11. Inline assembly macros
 * 12. Memory barriers
 * 13. Atomic operations
 * 14. Compiler hints (likely/unlikely)
 * 15. Array size macro
 * 16. Offsetof macro
 * 17. Container_of macro
 * 18. Min/max macros
 * 19. Swap macro
 * 20. Round up/down macros
 * 21. Alignment macros
 * 22. Static assert
 * 23. Type checking
 * 24. Endianness conversion
 * ============================================================================= */

#ifndef __MINIOS_TYPES_H__
#define __MINIOS_TYPES_H__

/* ============================================================
 * STANDART TIPLER
 * ============================================================ */
typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned long long  uint64_t;

typedef signed char         int8_t;
typedef signed short        int16_t;
typedef signed int          int32_t;
typedef signed long long    int64_t;

typedef uint64_t            size_t;
typedef int64_t             ssize_t;
typedef uint64_t            uintptr_t;
typedef int64_t             intptr_t;

typedef uint64_t            phys_addr_t;
typedef uint64_t            virt_addr_t;
typedef uint64_t            paddr_t;
typedef uint64_t            vaddr_t;

typedef int                 bool;
#define true    1
#define false   0

#define NULL    ((void*)0)

/* ============================================================
 * OZNITELIKLER (ATTRIBUTES)
 * ============================================================ */
#define __packed        __attribute__((packed))
#define __aligned(x)    __attribute__((aligned(x)))
#define __noreturn      __attribute__((noreturn))
#define __section(x)    __attribute__((section(x)))
#define __used          __attribute__((used))
#define __unused        __attribute__((unused))
#define __weak          __attribute__((weak))
#define __noinline      __attribute__((noinline))
#define __always_inline __attribute__((always_inline))
#define __cold          __attribute__((cold))
#define __hot           __attribute__((hot))
#define __const         __attribute__((const))
#define __pure          __attribute__((pure))
#define __malloc        __attribute__((malloc))
#define __nonnull       __attribute__((nonnull))
#define __returns_twice __attribute__((returns_twice))
#define __warn_unused   __attribute__((warn_unused_result))
#define __deprecated    __attribute__((deprecated))
#define __interrupt     __attribute__((interrupt))
#define __no_instrument __attribute__((no_instrument_function))

/* ============================================================
 * SATIR ICI ASSEMBLY
 * ============================================================ */
#define __cli()         __asm__ __volatile__ ("cli" ::: "memory")
#define __sti()         __asm__ __volatile__ ("sti" ::: "memory")
#define __hlt()         __asm__ __volatile__ ("hlt" ::: "memory")
#define __pause()       __asm__ __volatile__ ("pause" ::: "memory")
#define __nop()         __asm__ __volatile__ ("nop" ::: "memory")
#define __fence()       __asm__ __volatile__ ("mfence" ::: "memory")
#define __sfence()      __asm__ __volatile__ ("sfence" ::: "memory")
#define __lfence()      __asm__ __volatile__ ("lfence" ::: "memory")

#define __invlpg(addr)  __asm__ __volatile__ ("invlpg (%0)" :: "r"(addr) : "memory")
#define __wbinvd()      __asm__ __volatile__ ("wbinvd" ::: "memory")
#define __cpuid(level, a, b, c, d) \
    __asm__ __volatile__ ("cpuid" \
        : "=a"(a), "=b"(b), "=c"(c), "=d"(d) \
        : "a"(level) \
        : "memory")

#define __rdmsr(msr, lo, hi) \
    __asm__ __volatile__ ("rdmsr" \
        : "=a"(lo), "=d"(hi) \
        : "c"(msr))

#define __wrmsr(msr, lo, hi) \
    __asm__ __volatile__ ("wrmsr" \
        : \
        : "c"(msr), "a"(lo), "d"(hi) \
        : "memory")

#define __read_cr0()    ({ uint64_t __cr0; __asm__ __volatile__ ("mov %%cr0, %0" : "=r"(__cr0)); __cr0; })
#define __read_cr2()    ({ uint64_t __cr2; __asm__ __volatile__ ("mov %%cr2, %0" : "=r"(__cr2)); __cr2; })
#define __read_cr3()    ({ uint64_t __cr3; __asm__ __volatile__ ("mov %%cr3, %0" : "=r"(__cr3)); __cr3; })
#define __read_cr4()    ({ uint64_t __cr4; __asm__ __volatile__ ("mov %%cr4, %0" : "=r"(__cr4)); __cr4; })
#define __read_cr8()    ({ uint64_t __cr8; __asm__ __volatile__ ("mov %%cr8, %0" : "=r"(__cr8)); __cr8; })
#define __read_rflags() ({ uint64_t __rflags; __asm__ __volatile__ ("pushfq; pop %0" : "=r"(__rflags)); __rflags; })

#define __write_cr0(x)  __asm__ __volatile__ ("mov %0, %%cr0" :: "r"((uint64_t)(x)) : "memory")
#define __write_cr3(x)  __asm__ __volatile__ ("mov %0, %%cr3" :: "r"((uint64_t)(x)) : "memory")
#define __write_cr4(x)  __asm__ __volatile__ ("mov %0, %%cr4" :: "r"((uint64_t)(x)) : "memory")
#define __write_cr8(x)  __asm__ __volatile__ ("mov %0, %%cr8" :: "r"((uint64_t)(x)) : "memory")

#define __inb(port)     ({ uint8_t __v; __asm__ __volatile__ ("inb %w1, %b0" : "=a"(__v) : "Nd"(port)); __v; })
#define __inw(port)     ({ uint16_t __v; __asm__ __volatile__ ("inw %w1, %w0" : "=a"(__v) : "Nd"(port)); __v; })
#define __inl(port)     ({ uint32_t __v; __asm__ __volatile__ ("inl %w1, %k0" : "=a"(__v) : "Nd"(port)); __v; })
#define __outb(port, val)   __asm__ __volatile__ ("outb %b0, %w1" :: "a"((uint8_t)(val)), "Nd"(port))
#define __outw(port, val)   __asm__ __volatile__ ("outw %w0, %w1" :: "a"((uint16_t)(val)), "Nd"(port))
#define __outl(port, val)   __asm__ __volatile__ ("outl %k0, %w1" :: "a"((uint32_t)(val)), "Nd"(port))

/* ============================================================
 * BELLEK BARIYERLERI
 * ============================================================ */
#define barrier()       __asm__ __volatile__ ("" ::: "memory")
#define mb()            __fence()
#define rmb()           __lfence()
#define wmb()           __sfence()
#define smp_mb()        mb()
#define smp_rmb()       rmb()
#define smp_wmb()       wmb()

/* ============================================================
 * ATOMIK ISLEMLER
 * ============================================================ */
#define atomic_read(v)      (*(volatile typeof(*(v)) *)(v))
#define atomic_set(v, i)    (*(volatile typeof(*(v)) *)(v) = (i))

static __always_inline int atomic_add(int *v, int i) {
    int __i = i;
    __asm__ __volatile__ (
        "lock; xaddl %0, %1"
        : "+r"(__i), "+m"(*v)
        : : "memory", "cc"
    );
    return __i + i;
}

static __always_inline int atomic_sub(int *v, int i) {
    return atomic_add(v, -i);
}

static __always_inline int atomic_inc(int *v) {
    return atomic_add(v, 1);
}

static __always_inline int atomic_dec(int *v) {
    return atomic_add(v, -1);
}

static __always_inline int atomic_xchg(int *v, int new) {
    int old;
    __asm__ __volatile__ (
        "lock; xchgl %0, %1"
        : "=r"(old), "+m"(*v)
        : "0"(new)
        : "memory"
    );
    return old;
}

static __always_inline int atomic_cmpxchg(int *v, int old, int new) {
    int prev;
    __asm__ __volatile__ (
        "lock; cmpxchgl %2, %1"
        : "=a"(prev), "+m"(*v)
        : "r"(new), "0"(old)
        : "memory", "cc"
    );
    return prev;
}

/* ============================================================
 * DERLEYICI IPUCLARI
 * ============================================================ */
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

/* ============================================================
 * YARDIMCI MAKROLAR
 * ============================================================ */
#define ARRAY_SIZE(arr)     (sizeof(arr) / sizeof((arr)[0]))
#define offsetof(type, member)  __builtin_offsetof(type, member)
#define container_of(ptr, type, member) ({          \
    const typeof(((type *)0)->member) *__mptr = (ptr);  \
    (type *)((char *)__mptr - offsetof(type, member)); })

#define min(a, b)       ({ typeof(a) _a = (a); typeof(b) _b = (b); _a < _b ? _a : _b; })
#define max(a, b)       ({ typeof(a) _a = (a); typeof(b) _b = (b); _a > _b ? _a : _b; })
#define clamp(x, lo, hi)    min(max(x, lo), hi)
#define swap(a, b)      do { typeof(a) __tmp = (a); (a) = (b); (b) = __tmp; } while (0)

#define DIV_ROUND_UP(n, d)  (((n) + (d) - 1) / (d))
#define ALIGN_UP(x, a)      (((x) + (a) - 1) & ~((a) - 1))
#define ALIGN_DOWN(x, a)    ((x) & ~((a) - 1))
#define IS_ALIGNED(x, a)    (((x) & ((a) - 1)) == 0)
#define PAGE_ALIGN(x)       ALIGN_UP(x, 4096)
#define PAGE_SIZE           4096
#define PAGE_SHIFT          12
#define PAGE_MASK           (~(PAGE_SIZE - 1))

#define KB(x)               ((x) * 1024ULL)
#define MB(x)               ((x) * 1024ULL * 1024ULL)
#define GB(x)               ((x) * 1024ULL * 1024ULL * 1024ULL)
#define TB(x)               ((x) * 1024ULL * 1024ULL * 1024ULL * 1024ULL)

#define BITS_PER_BYTE       8
#define BITS_PER_LONG       64

/* ============================================================
 * STATIK ASSERT
 * ============================================================ */
#define static_assert(expr, msg)    _Static_assert(expr, msg)
#define STATIC_ASSERT(expr)         static_assert(expr, #expr)

/* ============================================================
 * ENDIANNESS
 * ============================================================ */
#define cpu_to_le16(x)      (x)
#define cpu_to_le32(x)      (x)
#define cpu_to_le64(x)      (x)
#define le16_to_cpu(x)      (x)
#define le32_to_cpu(x)      (x)
#define le64_to_cpu(x)      (x)
#define cpu_to_be16(x)      __builtin_bswap16(x)
#define cpu_to_be32(x)      __builtin_bswap32(x)
#define cpu_to_be64(x)      __builtin_bswap64(x)
#define be16_to_cpu(x)      __builtin_bswap16(x)
#define be32_to_cpu(x)      __builtin_bswap32(x)
#define be64_to_cpu(x)      __builtin_bswap64(x)

/* ============================================================
 * TIP KONTROLU
 * ============================================================ */
#define __same_type(a, b)   __builtin_types_compatible_p(typeof(a), typeof(b))
#define BUILD_BUG_ON(condition)     static_assert(!(condition), "BUG: " #condition)
#define BUILD_BUG_ON_ZERO(e)        (sizeof(struct { int:-!!(e); }))
#define BUILD_BUG_ON_MSG(cond, msg) static_assert(!(cond), msg)

/* ============================================================
 * ZAMAN
 * ============================================================ */
typedef uint64_t ktime_t;
typedef uint64_t jiffies_t;

#define HZ                  1000
#define TICK_NS             (1000000000ULL / HZ)
#define MS_TO_NS(ms)        ((ms) * 1000000ULL)
#define US_TO_NS(us)        ((us) * 1000ULL)
#define NS_TO_MS(ns)        ((ns) / 1000000ULL)
#define NS_TO_US(ns)        ((ns) / 1000ULL)

/* ============================================================
 * HATA KODLARI
 * ============================================================ */
#define OK                  0
#define ERROR               -1
#define ENOMEM              -2
#define EINVAL              -3
#define EAGAIN              -4
#define EBUSY               -5
#define EIO                 -6
#define ENOENT              -7
#define EACCES              -8
#define EEXIST              -9
#define ENOSPC              -10
#define EPIPE               -11
#define EINTR               -12
#define EFAULT              -13
#define ENODEV              -14
#define EOPNOTSUPP          -15
#define ETIMEDOUT           -16
#define ECONNREFUSED        -17
#define EADDRINUSE          -18
#define EADDRNOTAVAIL       -19
#define ENETUNREACH         -20
#define ECONNRESET          -21
#define ECONNABORTED        -22
#define ENOBUFS             -23
#define EPROTONOSUPPORT     -24
#define EAFNOSUPPORT        -25
#define EALREADY            -26
#define EINPROGRESS         -27
#define ENOTSOCK            -28
#define EDESTADDRREQ        -29
#define EMSGSIZE            -30
#define EPROTOTYPE          -31
#define ENOPROTOOPT         -32
#define ESHUTDOWN           -33
#define ETOOMANYREFS        -34
#define EHOSTDOWN           -35
#define EHOSTUNREACH        -36
#define ELOOP               -37
#define ENAMETOOLONG        -38
#define ENOTEMPTY           -39
#define EUSERS              -40
#define EDQUOT              -41
#define ESTALE              -42
#define EREMOTE             -43
#define EBADFD              -44
#define EREMCHG             -45
#define EILSEQ              -46
#define EOVERFLOW           -47
#define ENOTUNIQ            -48
#define EBADMSG             -49
#define EMULTIHOP           -50
#define ENOLINK             -51
#define EPROTO              -52

/* ============================================================
 * GUVENLIK
 * ============================================================ */
#define __stack_chk_guard   (*(volatile uint64_t *)0xFFFFFFFF800FF000)
#define STACK_CANARY_VALUE  0xDEADBEEFCAFEBABEULL

/* ============================================================
 * VERSIYON
 * ============================================================ */
#define KERNEL_VERSION_MAJOR    4
#define KERNEL_VERSION_MINOR    0
#define KERNEL_VERSION_PATCH    0
#define KERNEL_VERSION_STRING   "MiniOS Kernel v4.0 - AI-Powered Secure Kernel"
#define KERNEL_BUILD_DATE       __DATE__
#define KERNEL_BUILD_TIME       __TIME__
#define KERNEL_COMPILER         "GCC " __VERSION__

/* ============================================================
 * LIMITLER
 * ============================================================ */
#define PID_MAX                 32768
#define FD_MAX                  1024
#define PATH_MAX                4096
#define NAME_MAX                255
#define ARG_MAX                 131072
#define OPEN_MAX                1024
#define NGROUPS_MAX             32
#define MAX_CPUS                256
#define MAX_PROCESSES           8192
#define MAX_THREADS             32768
#define MAX_FILES               65536
#define MAX_VMA                 65536
#define MAX_DEVICES             256
#define MAX_DRIVERS             128
#define MAX_FILESYSTEMS         32
#define MAX_MOUNT_POINTS        64
#define MAX_NET_INTERFACES      16
#define MAX_SOCKETS             65536
#define MAX_PIPES               1024
#define MAX_MSG_QUEUES          1024
#define MAX_SHM_SEGMENTS        1024
#define MAX_SEM_SETS            1024

/* ============================================================
 * KERNEL ADRES ALANI
 * ============================================================ */
#define KERNEL_VADDR_BASE       0xFFFFFFFF80000000ULL
#define KERNEL_VADDR_END        0xFFFFFFFFFFFFFFFFULL
#define KERNEL_HEAP_START       0xFFFFFFFF80100000ULL
#define KERNEL_HEAP_END         0xFFFFFFFF8F000000ULL
#define KERNEL_STACK_TOP        0xFFFFFFFF90000000ULL
#define KERNEL_STACK_SIZE       0x20000         /* 128KB */
#define USER_VADDR_BASE         0x0000000000400000ULL
#define USER_VADDR_END          0x00007FFFFFFFFFFFULL
#define USER_STACK_TOP          0x00007FFFFFFFF000ULL
#define USER_STACK_SIZE         0x100000        /* 1MB */

/* ============================================================
 * PAGE FLAGS
 * ============================================================ */
#define PAGE_PRESENT            (1ULL << 0)
#define PAGE_RW                 (1ULL << 1)
#define PAGE_USER               (1ULL << 2)
#define PAGE_PWT                (1ULL << 3)
#define PAGE_PCD                (1ULL << 4)
#define PAGE_ACCESSED           (1ULL << 5)
#define PAGE_DIRTY              (1ULL << 6)
#define PAGE_PAT                (1ULL << 7)
#define PAGE_GLOBAL             (1ULL << 8)
#define PAGE_NX                 (1ULL << 63)
#define PAGE_HUGE               (1ULL << 7)     /* For PD/PDPT entries */

#define PAGE_KERNEL             (PAGE_PRESENT | PAGE_RW | PAGE_GLOBAL)
#define PAGE_KERNEL_RO          (PAGE_PRESENT | PAGE_GLOBAL)
#define PAGE_KERNEL_NX          (PAGE_PRESENT | PAGE_RW | PAGE_GLOBAL | PAGE_NX)
#define PAGE_USER_RW            (PAGE_PRESENT | PAGE_RW | PAGE_USER)
#define PAGE_USER_RO            (PAGE_PRESENT | PAGE_USER)
#define PAGE_USER_NX            (PAGE_PRESENT | PAGE_RW | PAGE_USER | PAGE_NX)

#endif /* __MINIOS_TYPES_H__ */
