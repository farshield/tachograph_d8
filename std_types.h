/**
 * @file std_types.h
 * @author Gabi Pascalau
 * @date 2 Sep 2017
 *
 * Standard Types
 */

#ifndef STD_TYPES_H
#define STD_TYPES_H

/******************************************************************************/
/*    INCLUDED FILES                                                          */
/******************************************************************************/

#include <stdint.h>

/******************************************************************************/
/*    DEFINITIONS                                                             */
/******************************************************************************/

#ifndef NULL
#define NULL 0
#endif

#ifndef NULL_PTR
#define NULL_PTR ( (void *) 0)
#endif

typedef enum
{
    FALSE = 0,
    TRUE = 1
} bool_t;

typedef uint8_t Std_ReturnType;

#define E_OK (Std_ReturnType) 0
#define E_NOT_OK (Std_ReturnType) 1
#define E_PENDING (Std_ReturnType) 10

#ifndef CALLBACK_T
#define CALLBACK_T
typedef void (*callback_t)(void);
#endif

#ifndef MIN
#define MIN(_x,_y) (((_x) < (_y)) ? (_x) : (_y))
#endif
#ifndef MAX
#define MAX(_x,_y) (((_x) > (_y)) ? (_x) : (_y))
#endif

#define STD_HIGH        0x01
#define STD_LOW         0x00

#define STD_ACTIVE      0x01
#define STD_IDLE        0x00

#define STD_ON          0x01
#define STD_OFF         0x00

/* bit definitions */
#define B0  (0x000000001)
#define B1  (0x000000002)
#define B2  (0x000000004)
#define B3  (0x000000008)
#define B4  (0x000000010)
#define B5  (0x000000020)
#define B6  (0x000000040)
#define B7  (0x000000080)
#define B8  (0x000000100)
#define B9  (0x000000200)
#define B10 (0x000000400)
#define B11 (0x000000800)
#define B12 (0x000001000)
#define B13 (0x000002000)
#define B14 (0x000004000)
#define B15 (0x000008000)
#define B16 (0x000010000)
#define B17 (0x000020000)
#define B18 (0x000040000)
#define B19 (0x000080000)
#define B20 (0x000100000)
#define B21 (0x000200000)
#define B22 (0x000400000)
#define B23 (0x000800000)
#define B24 (0x001000000)
#define B25 (0x002000000)
#define B26 (0x004000000)
#define B27 (0x008000000)
#define B28 (0x010000000)
#define B29 (0x020000000)
#define B30 (0x040000000)
#define B31 (0x080000000)

#endif /* STD_TYPES_H */
