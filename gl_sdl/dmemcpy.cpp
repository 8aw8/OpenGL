#include <string.h>
#include "dmemcpy.h"

#define DEF_OPT_FLAG_NONE   0
#define DEF_OPT_FLAG_NOPT   1

#define DEF_OPT_FLAG_MMX    5
#define DEF_OPT_FLAG_SSE    6
#define DEF_OPT_FLAG_SSE2   7

static int opt_flag = DEF_OPT_FLAG_NONE;

void * _memcpy(void *to, const void *from, size_t len);

void *(* nmemcpy)(void *to, const void *from, size_t len) = _memcpy;

typedef struct {
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;
} cpuid_regs_t;

int check_opt_flag(void)
{
    cpuid_regs_t regs;

#define	CPUID	".byte 0x0f, 0xa2; "
    asm(CPUID
            : "=a" (regs.eax), "=b" (regs.ebx), "=c" (regs.ecx), "=d" (regs.edx)
            : "0" (1));

    if (regs.edx & 0x4000000)
        return (DEF_OPT_FLAG_SSE2);
    if (regs.edx & 0x2000000)
        return (DEF_OPT_FLAG_SSE);
    if (regs.edx & 0x800000)
        return (DEF_OPT_FLAG_MMX);
    return (DEF_OPT_FLAG_NONE);
}

#define small_memcpy(to,from,n)\
{\
    register unsigned long int dummy;\
    __asm__ __volatile__(\
            "rep; movsb"\
            :"=&D"(to), "=&S"(from), "=&c"(dummy)\
            :"0" (to), "1" (from),"2" (n)\
            : "memory");\
}

/* From Linux. */
static inline void * __memcpy(void * to, const void * from, size_t len)
{
    int d0, d1, d2;

    if (len < 4 ) {
        small_memcpy(to,from,len);
    } else
        __asm__ __volatile__(
                "rep ; movsl\n\t"
                "testb $2,%b4\n\t"
                "je 1f\n\t"
                "movsw\n"
                "1:\ttestb $1,%b4\n\t"
                "je 2f\n\t"
                "movsb\n"
                "2:"
                : "=&c" (d0), "=&D" (d1), "=&S" (d2)
                :"0" (len/4), "q" (len),"1" ((long) to),"2" ((long) from)
                : "memory");

    return(to);
}

#define MIN_LEN         0x40
#define SSE_MMREG_SIZE  16
#define MMX_MMREG_SIZE  8



void *sse_memcpy_32(void *to, const void *from, size_t len)
{
    void *const save = to;

    __asm__ __volatile__ (
            "prefetchnta (%0)\n"
            "prefetchnta 32(%0)\n"
            "prefetchnta 64(%0)\n"
            "prefetchnta 96(%0)\n"
            "prefetchnta 128(%0)\n"
            "prefetchnta 160(%0)\n"
            "prefetchnta 192(%0)\n"
            "prefetchnta 224(%0)\n"
            "prefetchnta 256(%0)\n"
            "prefetchnta 288(%0)\n"
            :: "r" (from) );

    if (len >= MIN_LEN) {
        register int i;
        register int j;
        register unsigned int delta;
       
	
        delta = (SSE_MMREG_SIZE-1);
        if (delta) {
            delta=SSE_MMREG_SIZE-delta;
            len -= delta;
            small_memcpy(to, from, delta);
        }
        j = len >> 6;
        len &= 63;

        for(i=0; i<j; i++) {
            __asm__ __volatile__ (
                    "prefetchnta 320(%0)\n"
                    "prefetchnta 352(%0)\n"
                    "movups (%0), %%xmm0\n"
                    "movups 16(%0), %%xmm1\n"
                    "movups 32(%0), %%xmm2\n"
                    "movups 48(%0), %%xmm3\n"
                    "movntps %%xmm0, (%1)\n"
                    "movntps %%xmm1, 16(%1)\n"
                    "movntps %%xmm2, 32(%1)\n"
                    "movntps %%xmm3, 48(%1)\n"
                    ::"r" (from), "r" (to) : "memory");
            from+=64;
            to+=64;
        }
        __asm__ __volatile__ ("sfence":::"memory");
    }
    if (len != 0)
        __memcpy(to, from, len);
    return save;
}

void *sse_memcpy_64(void *to, const void *from, size_t len)
{
    void *const save = to;

    __asm__ __volatile__ (
            "prefetchnta (%0)\n"
            "prefetchnta 64(%0)\n"
            "prefetchnta 128(%0)\n"
            "prefetchnta 192(%0)\n"
            "prefetchnta 256(%0)\n"
            :: "r" (from) );

    if (len >= MIN_LEN) {
        register int i;
        register int j;
        register unsigned int delta;

        delta = (SSE_MMREG_SIZE-1);
        if (delta) {
            delta=SSE_MMREG_SIZE-delta;
            len -= delta;
            small_memcpy(to, from, delta);
        }
        j = len >> 6;
        len &= 63;

        for(i=0; i<j; i++) {
            __asm__ __volatile__ (
                    "prefetchnta 320(%0)\n"
                    "movups (%0), %%xmm0\n"
                    "movups 16(%0), %%xmm1\n"
                    "movups 32(%0), %%xmm2\n"
                    "movups 48(%0), %%xmm3\n"
                    "movntps %%xmm0, (%1)\n"
                    "movntps %%xmm1, 16(%1)\n"
                    "movntps %%xmm2, 32(%1)\n"
                    "movntps %%xmm3, 48(%1)\n"
                    ::"r" (from), "r" (to) : "memory");
            from+=64;
            to+=64;
        }
        __asm__ __volatile__ ("sfence":::"memory");
    }
    if (len != 0)
        __memcpy(to, from, len);
    return save;
}

void *mmx_memcpy_32(void *to, const void *from, size_t len)
{
    void *const save = to;
    register int i;
    register int j;

    __asm__ __volatile__ (
            "prefetchnta (%0)\n"
            "prefetchnta 32(%0)\n"
            "prefetchnta 64(%0)\n"
            "prefetchnta 96(%0)\n"
            "prefetchnta 128(%0)\n"
            "prefetchnta 160(%0)\n"
            "prefetchnta 192(%0)\n"
            "prefetchnta 224(%0)\n"
            "prefetchnta 256(%0)\n"
            "prefetchnta 288(%0)\n"
            :: "r" (from) );

    j = len >> 6;
    len &= 63;
    for(i=0; i<j; i++) {
        __asm__ __volatile__ (
                "prefetchnta 320(%0)\n"
                "prefetchnta 352(%0)\n"
                "movq (%0), %%mm0\n"
                "movq 8(%0), %%mm1\n"
                "movq 16(%0), %%mm2\n"
                "movq 24(%0), %%mm3\n"
                "movq 32(%0), %%mm4\n"
                "movq 40(%0), %%mm5\n"
                "movq 48(%0), %%mm6\n"
                "movq 56(%0), %%mm7\n"
                "movq %%mm0, (%1)\n"
                "movq %%mm1, 8(%1)\n"
                "movq %%mm2, 16(%1)\n"
                "movq %%mm3, 24(%1)\n"
                "movq %%mm4, 32(%1)\n"
                "movq %%mm5, 40(%1)\n"
                "movq %%mm6, 48(%1)\n"
                "movq %%mm7, 56(%1)\n"
                :: "r" (from), "r" (to) : "memory");
        from+=64;
        to+=64;
    }
    __asm__ __volatile__ ("sfence":::"memory");
    __asm__ __volatile__ ("emms":::"memory");

    if (len != 0)
        __memcpy(to, from, len);

    return (save);
}


void *mmx_memcpy_64(void *to, const void *from, size_t len)
{
    void *const save = to;
    register int i;
    register int j;

    __asm__ __volatile__ (
            "prefetchnta (%0)\n"
            "prefetchnta 64(%0)\n"
            "prefetchnta 128(%0)\n"
            "prefetchnta 192(%0)\n"
            "prefetchnta 256(%0)\n"
            :: "r" (from) );

    j = len >> 6;
    len &= 63;
    for(i=0; i<j; i++) {
        __asm__ __volatile__ (
                "prefetchnta 320(%0)\n"
                "movq (%0), %%mm0\n"
                "movq 8(%0), %%mm1\n"
                "movq 16(%0), %%mm2\n"
                "movq 24(%0), %%mm3\n"
                "movq 32(%0), %%mm4\n"
                "movq 40(%0), %%mm5\n"
                "movq 48(%0), %%mm6\n"
                "movq 56(%0), %%mm7\n"
                "movq %%mm0, (%1)\n"
                "movq %%mm1, 8(%1)\n"
                "movq %%mm2, 16(%1)\n"
                "movq %%mm3, 24(%1)\n"
                "movq %%mm4, 32(%1)\n"
                "movq %%mm5, 40(%1)\n"
                "movq %%mm6, 48(%1)\n"
                "movq %%mm7, 56(%1)\n"
                :: "r" (from), "r" (to) : "memory");
        from+=64;
        to+=64;
    }
    __asm__ __volatile__ ("sfence":::"memory");
    __asm__ __volatile__ ("emms":::"memory");

    if (len != 0)
        __memcpy(to, from, len);

    return (save);
}


void *_memcpy(void *to, const void *from, size_t len)
{
    if (opt_flag == DEF_OPT_FLAG_NONE)
        opt_flag = check_opt_flag();
    if (opt_flag == DEF_OPT_FLAG_SSE2)
        nmemcpy = sse_memcpy_64;
    if (opt_flag == DEF_OPT_FLAG_SSE)
        nmemcpy = sse_memcpy_32;
    else if (opt_flag == DEF_OPT_FLAG_MMX)
        nmemcpy = mmx_memcpy_32;
    else
        nmemcpy = memcpy;
    return (nmemcpy(to, from, len));
}