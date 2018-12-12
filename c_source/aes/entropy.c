#ifdef _WIN32
/* `RtlGenRandom` is used over `CryptGenRandom` on Microsoft Windows based systems:
 *  - `CryptGenRandom` requires pulling in `CryptoAPI` which causes unnecessary
 *     memory overhead if this API is not being used for other purposes
 *  - `RtlGenRandom` is thus called directly instead. A detailed explanation
 *     can be found here: https://blogs.msdn.microsoft.com/michael_howard/2005/01/14/cryptographically-secure-random-number-on-windows-without-using-cryptoapi/
 *
 * In spite of the disclaimer on the `RtlGenRandom` documentation page that was
 * written back in the Windows XP days, this function is here to stay. The CRT
 * function `rand_s()` directly depends on it, so touching it would break many
 * applications released since Windows XP.
 *
 * Also note that Rust, Firefox and BoringSSL (thus, Google Chrome and everything
 * based on Chromium) also depend on it, and that libsodium allows the RNG to be
 * replaced without patching nor recompiling the library.
 */
# include <windows.h>
# define RtlGenRandom SystemFunction036
#else
#include <stdio.h>
#include <fcntl.h>
#endif
#include "entropy.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#ifdef _WIN32
BOOLEAN NTAPI RtlGenRandom(PVOID RandomBuffer, ULONG RandomBufferLength);
int entropy_fun(unsigned char buf[], unsigned int len)
{
	unsigned int i;
    unsigned __int64 pentium_tsc[1];
    if (RtlGenRandom(buf, len))
		return len;

    QueryPerformanceCounter((LARGE_INTEGER *)pentium_tsc);

    for(i = 0; i < 8 && i < len; ++i)
        buf[i] = ((unsigned char*)pentium_tsc)[i];

    return i;
}
#else
int entropy_fun(unsigned char buf[], unsigned int len)
{
    FILE *frand = fopen("/dev/random", "r");
    int rlen = 0;
    if (frand != NULL)
    {
        rlen = fread(buf, sizeof(unsigned char), len, frand);
        fclose(frand);
    }
    return rlen;
}
#endif

#if defined(__cplusplus)
}
#endif
