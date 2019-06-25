#pragma once
#include <fstream>
#include <stddef.h>
#include <stdlib.h>
#include <inttypes.h>

//#define UINT64_MAX ((1<<64)-1)

#ifndef HAVE_BSWAP_16
#       define bswap_16(num) \
                (((num) << 8) | ((num) >> 8))
#endif

#ifndef HAVE_BSWAP_32
#       define bswap_32(num) \
                ( (((num) << 24)                       ) \
                | (((num) <<  8) & UINT32_C(0x00FF0000)) \
                | (((num) >>  8) & UINT32_C(0x0000FF00)) \
                | (((num) >> 24)                       ) )
#endif

#ifndef HAVE_BSWAP_64
#       define bswap_64(num) \
                ( (((num) << 56)                               ) \
                | (((num) << 40) & UINT64_C(0x00FF000000000000)) \
                | (((num) << 24) & UINT64_C(0x0000FF0000000000)) \
                | (((num) <<  8) & UINT64_C(0x000000FF00000000)) \
                | (((num) >>  8) & UINT64_C(0x00000000FF000000)) \
                | (((num) >> 24) & UINT64_C(0x0000000000FF0000)) \
                | (((num) >> 40) & UINT64_C(0x000000000000FF00)) \
                | (((num) >> 56)                               ) )
#endif

//#define WORDS_BIGENDIAN 1
#ifdef WORDS_BIGENDIAN
#       define integer_le_16(n) bswap_16(n)
#       define integer_le_32(n) bswap_32(n)
#       define integer_le_64(n) bswap_64(n)
#else
#       define integer_le_16(n) (n)
#       define integer_le_32(n) (n)
#       define integer_le_64(n) (n)
#endif

uint32_t crc32_table[256];
uint64_t crc64_table[256];

void initcrc(void)
{
    static const uint32_t poly32 = 0xEDB88320;
    static const uint64_t poly64 = 0xC96C5795D7870F42;

    for (size_t i = 0; i < 256; ++i) {
        uint32_t crc32 = i;
        uint64_t crc64 = i;

        for (size_t j = 0; j < 8; ++j) {
            if (crc32 & 1)
                crc32 = (crc32 >> 1) ^ poly32;
            else
                crc32 >>= 1;

            if (crc64 & 1)
                crc64 = (crc64 >> 1) ^ poly64;
            else
                crc64 >>= 1;
        }

        crc32_table[i] = crc32;
        crc64_table[i] = crc64;
    }
}

uint32_t
crc32(const uint8_t *buf, size_t size, uint32_t crc)
{
    crc = ~crc;
    for (size_t i = 0; i < size; ++i)
        crc = crc32_table[buf[i] ^ (crc & 0xFF)]
                ^ (crc >> 8);
    return ~crc;
}

uint64_t
crc64(const uint8_t *buf, size_t size, uint64_t crc)
{
    crc = ~crc;
    for (size_t i = 0; i < size; ++i)
        crc = crc64_table[buf[i] ^ (crc & 0xFF)]
                ^ (crc >> 8);
    return ~crc;
}

struct CRC {
	union {
		uint8_t u8[64];
		uint32_t u32[16];
		uint64_t u64[8];
	} buffer;
};

size_t encode(uint8_t buf[9], uint64_t num)
{
    if (num > (uint64_t)UINT64_MAX / 2)
        return 0;

    size_t i = 0;
    while (num >= 0x80) {
        buf[i++] = (uint8_t)(num) | 0x80;
        num >>= 7;
    }

    buf[i++] = (uint8_t)(num);

    return i;
}

size_t decode(const uint8_t buf[], size_t size_max, uint64_t *num)
{
    if (size_max == 0)
        return 0;

    if (size_max > 9)
        size_max = 9;

    *num = buf[0] & 0x7F;
    size_t i = 0;

    while (buf[i++] & 0x80) {
        if (i >= size_max || buf[i] == 0x00)
            return 0;

        *num |= (uint64_t)(buf[i] & 0x7F) << (i * 7);
    }

    return i;
}

void fileWrite(std::ofstream *ofs, uint8_t *buf, size_t size, uint8_t* out,uint64_t outindex)
{
    if(ofs)
	    ofs->write((char*)buf,size);
    if(out)
        memcpy(&out[outindex],buf,size);
}

void fileWrite(std::ofstream *ofs, uint8_t data, uint8_t *out, uint64_t outindex)
{
    if(ofs)
	    (*ofs) << data;
    if(out)
        out[outindex] = data;
    
}
