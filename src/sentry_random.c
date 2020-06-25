#include "sentry_boot.h"

#include "sentry_random.h"
#include "sentry_utils.h"

#ifdef SENTRY_PLATFORM_DARWIN
#    include <stdlib.h>

#if defined(__MAC_OS_X_VERSION_MIN_REQUIRED)                                   \
    && (__MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_7)
void arc4random_buf(void* dst, size_t bytes ) {
    size_t whole = bytes / 4;
    size_t remainder = bytes % 4;
    uint8_t*  buf8 = (uint8_t*)dst;
    uint32_t* buf32 = (uint32_t*)(buf8 + remainder);
    uint32_t  rand = arc4random(); 

    switch( remainder ) {
    case 3:
       buf8[2] = 0xFF & (rand>>16);
    case 2:
       buf8[1] = 0xFF & (rand>>8);
    case 1:
       buf8[0] = 0xFF & rand;
       break;
    case 0:
    default:
        buf32[0] = rand;
        buf32++;
        whole--;
        break;
    }

    for(size_t n = 0; n < whole; ++n) {
        buf32[n] = arc4random();
    }
}
#endif

static int
getrandom_arc4random(void *dst, size_t bytes)
{
    arc4random_buf(dst, bytes);
    return 0;
}
#    define HAVE_ARC4RANDOM

#endif
#ifdef SENTRY_PLATFORM_UNIX
#    include <errno.h>
#    include <fcntl.h>
#    include <unistd.h>

static int
getrandom_devurandom(void *dst, size_t bytes)
{
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        return 1;
    }

    size_t to_read = bytes;
    char *d = dst;
    while (to_read > 0) {
        ssize_t n = read(fd, d, to_read);
        if (n < 0 && (errno == EAGAIN || errno == EINTR)) {
            continue;
        } else if (n <= 0) {
            break;
        }
        d += n;
        to_read -= n;
    }

    close(fd);
    return to_read > 0;
}

#    define HAVE_URANDOM
#endif
#ifdef SENTRY_PLATFORM_WINDOWS
typedef BOOLEAN(WINAPI *sRtlGenRandom)(PVOID Buffer, ULONG BufferLength);

static sRtlGenRandom pRtlGenRandom = NULL;
static bool sRtlGenRandomLoaded = false;

static int
getrandom_rtlgenrandom(void *dst, size_t bytes)
{
    if (sRtlGenRandomLoaded == false) {
        HANDLE advapi32_module = LoadLibraryA("advapi32.dll");
        if (advapi32_module != NULL) {
            pRtlGenRandom = (sRtlGenRandom)GetProcAddress(
                advapi32_module, "SystemFunction036");
        }
        sRtlGenRandomLoaded = true;
    }
    if (pRtlGenRandom == NULL) {
        return 1;
    }
    if (pRtlGenRandom(dst, (ULONG)bytes) == FALSE) {
        return 1;
    }
    return 0;
}

#    define HAVE_RTLGENRANDOM
#endif

int
sentry__getrandom(void *dst, size_t len)
{
#ifdef HAVE_ARC4RANDOM
    if (getrandom_arc4random(dst, len) == 0) {
        return 0;
    }
#endif
#ifdef HAVE_URANDOM
    if (getrandom_devurandom(dst, len) == 0) {
        return 0;
    }
#endif
#ifdef HAVE_RTLGENRANDOM
    if (getrandom_rtlgenrandom(dst, len) == 0) {
        return 0;
    }
#endif
    return 1;
}
