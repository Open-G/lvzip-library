#ifdef _WIN32
#include <windows.h>
#else
#include <stdio.h>
#include <fcntl.h>
#endif

#if defined(__cplusplus)
extern "C"
{
#endif

#ifdef _WIN32
int entropy_fun(unsigned char buf[], unsigned int len)
{
    HCRYPTPROV provider;
    unsigned __int64 pentium_tsc[1];
    unsigned int i;
    int result = 0;

    if (CryptAcquireContext(&provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
    {
        result = CryptGenRandom(provider, len, buf);
        CryptReleaseContext(provider, 0);
        if (result)
            return len;
    }

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
