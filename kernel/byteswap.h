// Byte swapping routines to convert big-endian stuff to litle-endian

#ifndef __BYTESWAP_H__
#define __BYTESWAP_H__ 1

#define swap_u32(x) (((x & 0xff) << 24) | ((x & 0xff00) << 8) | \
            ((x & 0xff0000) >> 8) | ((x & 0xff000000) >> 24))

#define swap_u64(x) ( \
            ((x & 0xfful) << 56) | \
            ((x & 0xff00ul) << 40) | \
            ((x & 0xff0000ul) << 24) | \
            ((x & 0xff000000ul) << 8) | \
            ((x & 0xff00000000ul) >> 8) | \
            ((x & 0xff0000000000ul) >> 24) | \
            ((x & 0xff000000000000ul) >> 40) | \
            ((x & 0xff00000000000000ul) >> 56))

#endif
