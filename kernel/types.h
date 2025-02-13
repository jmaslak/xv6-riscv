typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int  uint32;
typedef unsigned long uint64;

typedef uint64 pde_t;

#define ALIGN4(x) (((uint64) x) % 4 == 0 ? (uint64) x : ((uint64) x) + (4 - (((uint64) x) % 4)))
#define ADVANCE4(x) (((uint64) x) + (4ul - (((uint64) x) % 4)))
