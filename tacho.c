/**
 * @file tacho.c
 * @author gabi
 * @date 17 Aug 2017
 *
 * Tachograph interpreter
 */

/******************************************************************************/
/*    INCLUDED FILES                                                          */
/******************************************************************************/

#include "std_types.h"
#include "usart2.h"
#include "j1939app.h"
#include "fmi.h"
#include "tacho_countries.h"
#include "tacho.h"
#include "fram.h"

/******************************************************************************/
/*    DEFINITIONS                                                             */
/******************************************************************************/

/** Maximum number of framing errors per Task call before failing */
#define TACHO_MAX_FRAMING_ERRORS 5

/** Maximum number of failed attempts before switching to another protocol */
#define TACHO_MAX_FAILED_ATTEMPTS 2

#define TACHO_RX_QUEUE_SIZE 128  /**< Reception buffer size in bytes */
#define TACHO_MAX_DRIVERS 2  /**< Maximum number of drivers */
#define TACHO_MAX_CARD_NR 16  /**< Max driver card number in bytes */

/* VDO-related defines */
#define TACHO_VDO_SEQSZ 5  /**< VDO Start Sequence Size */
#define TACHO_VDO_CRC_INIT 0x49  /**< CRC-8 initialization value for VDO */
#define TACHO_VDO_CC_POS 1  /**< Country code byte position in VDO's DIN */

/* Stoneridge-related defines */
#define TACHO_SR_SEQSZ 3  /**< Stoneridge Start Sequence Size */
#define TACHO_SR_MSG_LEN_MIN 45  /**< Minimum Stoneridge SRE message length */
#define TACHO_SR_MSG_LEN_MAX 48  /**< Maximum Stoneridge SRE message length */

/******************************************************************************/
/*    PRIVATE TYPES                                                           */
/******************************************************************************/

/** Tachograph type handler callback function */
typedef bool_t (*Tacho_Handler_t)(uint8_t rx_byte);

/** Driver index */
typedef enum
{
    TACHO_DRIVER1,
    TACHO_DRIVER2
} Tacho_DriverIdx_t;

/** VDO-related field position in frame */
typedef enum
{
    TACHO_VDO_WORKING_STATE = 14,
    TACHO_VDO_DRV1_STATE = 15,
    TACHO_VDO_DRV2_STATE = 16,
    TACHO_VDO_STATUS = 17,
    TACHO_VDO_SPEED_LSB = 18,
    TACHO_VDO_SPEED_MSB = 19,
    TACHO_VDO_VIN_LENGTH = 34
} Tacho_VdoFields_t;

/** Stoneridge-related field position in frame */
typedef enum
{
    TACHO_SR_MSG_LEN = 3,
    TACHO_SR_MSG_ID = 4,
    TACHO_SR_WORKING_STATE = 9,
    TACHO_SR_DRV1_STATE = 10,
    TACHO_SR_DRV2_STATE = 11,
    TACHO_SR_STATUS = 12,
    TACHO_SR_SPEED_MSB = 13,
    TACHO_SR_SPEED_LSB = 14,
    TACHO_SR_CUSTOM = 30  /**< VIN, DIN1, DIN2 or VRN & RMS position (depends on message type) */
} Tacho_SrFields_t;

/** Stoneridge Message Identifier */
typedef enum
{
    TACHO_SR_MSG_VIN = 0x01,
    TACHO_SR_MSG_DIN1 = 0x02,
    TACHO_SR_MSG_DIN2 = 0x04,
    TACHO_SR_MSG_VRN = 0x08,
    TACHO_SR_MSG_TYPES = 4  /**< Total Stoneridge message types */
} Tacho_StoneridgeMsgID_t;

/** Circular buffer used for reception */
typedef struct
{
    uint8_t data[TACHO_RX_QUEUE_SIZE];
    uint8_t *head;
    uint8_t *tail;
    uint8_t *buffer_start;
    uint8_t *buffer_end;
    uint8_t count;  /**< Total number of bytes received and unprocessed, yet */
    uint16_t error_counter;  /**< Total number of framing errors */
    uint8_t failed_attempts;  /**< Total number of consecutive failed attempts */
} Tacho_RxQueue_t;

/** Driver ID (DIN) = Issuing member state + CardNumber */
typedef struct
{
    uint8_t country[TACHO_MAX_COUNTRY_CODE];
    uint8_t cardnr[TACHO_MAX_CARD_NR];
} Tacho_DriverID_t;

/** Real-time data received from Tachograph */
typedef struct
{
    uint8_t working_state;
    uint8_t driver1_state;
    uint8_t driver2_state;
    uint8_t tacho_status;
    uint8_t speed_msb;
    uint8_t speed_lsb;
    Tacho_DriverID_t driver[TACHO_MAX_DRIVERS];
} Tacho_Frame_t;

/** Cached data */
typedef struct
{
    uint8_t tco1[TACHO_TCO1_SIZE];  /**< Reconstructed TCO1 */
    uint8_t tco1_cmn[TACHO_TCO1_SIZE];  /**< TCO1 common collected data from J1939 and D8 */
    uint8_t di[2 * TACHO_MAX_DRIVER_ID + 1];  /**< Cached DIN1 + DIN2 + delimiters (and zero terminator) */
} Tacho_CachedData_t;

/** Protocol configuration */
typedef struct
{
    uint8_t *start_seq;  /**< Start sequence */
    uint8_t start_sz;  /**< Start sequence size */
    uint16_t baudRate;  /**< UART baudrate for specified protocol */
} Tacho_Protocol_t;

/** VDO-specific internal data */
typedef struct
{
    uint8_t index;  /**< Current position in frame */
    uint8_t cstr_pos;  /**< Start of custom string byte position */
    uint8_t drv1_pos;  /**< Start of Driver1 ID byte position */
    uint8_t drv2_pos;  /**< Start of Driver2 ID byte position */
    uint8_t crc8_pos;  /**< CRC8 position */
    uint8_t crc8_value;  /**< CRC8 computed value */
} Tacho_VdoData_t;

/** Stoneridge-specific internal data */
typedef struct
{
    uint8_t index;  /**< Current position in frame */
    uint8_t drv1_pos;  /**< Start of Driver1 ID byte position */
    uint8_t drv2_pos;  /**< Start of Driver2 ID byte position */
    uint8_t crc8_pos;  /**< CRC8 position */
    uint8_t crc8_value;  /**< CRC8 computed value */
} Tacho_SrData_t;

/******************************************************************************/
/*    PRIVATE DATA                                                            */
/******************************************************************************/

static Tacho_RxQueue_t Tacho_RxQueue;  /**< Reception buffer */
static Tacho_Frame_t Tacho_Frame;  /**< Tacho frame data (TCO1 + DIN) */
static Tacho_CachedData_t Tacho_CachedData;  /**< Data storage after succesful read */
static Tacho_VdoData_t vdo;  /**< VDO-related internal data */
static Tacho_SrData_t sr;  /**< Stoneridge-related internal data */

/** Protocol configuration parameters */
static const Tacho_Protocol_t Tacho_Protocol[TACHO_STANDARD_MAX] =
{
    /* VDO */
    {
        (uint8_t[TACHO_VDO_SEQSZ]) {0x55, 0x44, 0x54, 0x43, 0x4F},
        TACHO_VDO_SEQSZ,
        10400  /**< Baudrate */
    },
    /* Stoneridge */
    {
        (uint8_t[TACHO_SR_SEQSZ]) {0xFF, 0xFF, 0xFF},
        TACHO_SR_SEQSZ,
        1200  /**< Baudrate */
    }
};

/** Current selected protocol */
static Tacho_Standard_t Tacho_SelectedStandard = TACHO_STANDARD_VDO;

/** Pointer to the currently selected protocol */
static const Tacho_Protocol_t *Tacho_Proto = NULL;

/** Pointer to the currently selected handler function */
static Tacho_Handler_t Tacho_Handler = NULL_PTR;

/******************************************************************************/
/*    PRIVATE FUNCTIONS                                                       */
/******************************************************************************/

static void Tacho_SelectStandard(Tacho_Standard_t standard, bool_t updateMemory);
static void Tacho_CopyToCache(void);
static void Tacho_NotifyFrameReceived(uint8_t *tco1_data);
static bool_t Tacho_QueueAddByte(uint8_t rx_byte);
static bool_t Tacho_FetchByte(uint8_t *byte_val);
static void Tacho_ClearRxQueue(void);
static Std_ReturnType Tacho_ReadMemory(Tacho_Standard_t *protocol);
static Std_ReturnType Tacho_SetMemory(Tacho_Standard_t protocol);

/* VDO-specific functions*/
static void Tacho_VdoInitHandler(void);
static bool_t Tacho_VdoHandler(uint8_t rx_byte);
static void Tacho_VdoCheckDIN(uint8_t rx_byte);
static void Tacho_VdoCopyDIN(uint8_t pos, uint8_t rx_byte, uint8_t *country, uint8_t *cardnr);

/* Stoneridge-specific functions */
static void Tacho_StoneridgeInitHandler(void);
static bool_t Tacho_StoneridgeHandler(uint8_t rx_byte);
static bool_t Tacho_StoneridgeMsgProcess(uint8_t rx_byte);
static void Tacho_StoneridgeCheckDIN(uint8_t rx_byte);
static void Tacho_StoneridgeCopyDIN(uint8_t pos, uint8_t rx_byte, uint8_t *country, uint8_t *cardnr);

/******************************************************************************/
/*    IMPLEMENTATION                                                          */
/******************************************************************************/

/**
 * Initialization function called by the main initialization routine
 */
void Tacho_Init(void)
{
    Std_ReturnType op_status = E_NOT_OK;
    Tacho_Standard_t protocol = TACHO_STANDARD_MAX;

    Tacho_RxQueue.buffer_start = &Tacho_RxQueue.data[0];
    Tacho_RxQueue.buffer_end = &Tacho_RxQueue.data[TACHO_RX_QUEUE_SIZE - 1];

    USART2_init(Tacho_RxNotif, Tacho_ErrorNotif);

    op_status = Tacho_ReadMemory(&protocol);
    if (E_OK == op_status)
    {
        /* Select last saved Tachograph protocol */
        Tacho_SelectStandard(protocol, FALSE);
    }
    else
    {
        /* Default to VDO */
        Tacho_SelectStandard(TACHO_STANDARD_VDO, TRUE);
    }
}

/**
 * Initialization function called by the main initialization routine
 */
void Tacho_DeInit(void)
{
    USART2_close();
}

/**
 * Get most recent TCO1 data
 * @return Pointer to an array of 8 bytes representing a reconstructed TCO1 message
 */
uint8_t *tacho_get_cached_tco1_content_p(void)
{
    return (uint8_t *) Tacho_CachedData.tco1_cmn;
}

/**
 * Get most recent driver ID data
 * @return Pointer to an array where the 2 driver IDs are stored
 */
uint8_t *tacho_get_cached_di_content_p(void)
{
    return (uint8_t *) Tacho_CachedData.di;
}

/**
 * Current selected D8 protocol
 * @return VDO or Stoneridge
 */
Tacho_Standard_t Tacho_GetSelectedStandard(void)
{
    return Tacho_SelectedStandard;
}

/**
 * Task called by Scheduler periodically
 */
void Tacho_Task(void)
{
    static bool_t perform_sync = TRUE;
    static uint8_t index = 0;
    uint8_t rx_byte = 0xFF;

    /* Check for framing errors and select another standard if needed */
    if (TACHO_MAX_FRAMING_ERRORS <= Tacho_RxQueue.error_counter)
    {
        Tacho_RxQueue.failed_attempts++;
        if (TACHO_MAX_FAILED_ATTEMPTS <= Tacho_RxQueue.failed_attempts)
        {
            if (TACHO_STANDARD_VDO == Tacho_SelectedStandard)
            {
                Tacho_SelectStandard(TACHO_STANDARD_STONERIDGE, TRUE);
            }
            else
            {
                Tacho_SelectStandard(TACHO_STANDARD_VDO, TRUE);
            }
            /* Terminate Task for now and wait for a new set of data */
            return;
        }
    }
    Tacho_RxQueue.error_counter = 0;

    while (Tacho_FetchByte(&rx_byte))
    {
        if (perform_sync)
        {
            /* Search for start of frame */
            if (Tacho_Proto->start_seq[index] == rx_byte)
            {
                index++;
                if (Tacho_Proto->start_sz <= index)
                {
                    perform_sync = FALSE;
                }
            }
            else
            {
                index = 0;
            }
        }
        else
        {
             /* Sync OK - take next step: run handler (if not null :) */
            if (NULL_PTR != Tacho_Handler)
            {
                if ( (*Tacho_Handler)(rx_byte) )
                {
                    /* Frame done or frame error - must re-sync */
                    perform_sync = TRUE;
                    index = 0;
                }
            }
        }
    }
}

/**
 * Reads last known Tachograph protocol from FRAM memory
 * @param protocol[out] Pointer to protocol type info
 * @return E_OK if data was read from FRAM successfully, E_NOT_OK otherwise
 */
static Std_ReturnType Tacho_ReadMemory(Tacho_Standard_t *protocol)
{
    Std_ReturnType op_status = E_NOT_OK;
    uint8_t data;

    if (NULL != protocol)
    {
        *protocol = TACHO_STANDARD_MAX;
        if (E_OK == FRAM_ReadByte(FRAM_MEMADDR_TACHO_PROTO, &data))
        {
            if (data < TACHO_STANDARD_MAX)
            {
                *protocol = (Tacho_Standard_t) data;
                op_status = E_OK;
            }
        }
    }
    return op_status;
}

/**
 * Saves determined Tachograph protocol in FRAM memory
 * @param protocol[in] Protocol type to be written in memory
 * @return E_OK if data was saved in FRAM successfully, E_NOT_OK otherwise
 */
static Std_ReturnType Tacho_SetMemory(Tacho_Standard_t protocol)
{
    Std_ReturnType op_status = E_NOT_OK;

    if (protocol < TACHO_STANDARD_MAX)
    {
        op_status = FRAM_WriteByte(FRAM_MEMADDR_TACHO_PROTO, (uint8_t) protocol);
    }
    return op_status;
}

/**
 * Copies the DIN fields from the D8 VDO frame to the corresponding buffers
 *
 * @param pos Position in the DIN field
 * @param rx_byte Received byte from D8 serial output
 * @param country Pointer to country code buffer
 * @param cardnr Pointer to card number buffer
 */
static void Tacho_VdoCopyDIN(uint8_t pos, uint8_t rx_byte, uint8_t *country, uint8_t *cardnr)
{
    uint8_t *readCode;
    uint8_t i;

    if (pos == TACHO_VDO_CC_POS)
    {
        readCode = Tacho_GetCountryCode(rx_byte);
        for (i = 0; i < TACHO_MAX_COUNTRY_CODE; i++)
        {
            country[i] = readCode[i];
        }
    }
    else if (pos > TACHO_VDO_CC_POS)
    {
        pos -= (TACHO_VDO_CC_POS + 1);
        if (pos < TACHO_MAX_CARD_NR)
        {
            cardnr[pos] = rx_byte;
        }
    }
}

/**
 * Checks if received byte contains data from a VDO DIN field
 * @param rx_byte Received byte from D8 serial output
 */
static void Tacho_VdoCheckDIN(uint8_t rx_byte)
{
    uint8_t pos;

    if (vdo.drv1_pos == vdo.index)
    {
        if (rx_byte == 0)
        {
            /* DIN1 field is empty */
            Tacho_Frame.driver[TACHO_DRIVER1].cardnr[0] = '\0';
        }
    }
    else if ( (vdo.drv1_pos < vdo.index) && (vdo.index < vdo.drv2_pos) )
    {
        /* Index points within the boundaries of the DIN1 field */
        pos = vdo.index - vdo.drv1_pos - 1;
        Tacho_VdoCopyDIN(
            pos,
            rx_byte,
            Tacho_Frame.driver[TACHO_DRIVER1].country,
            Tacho_Frame.driver[TACHO_DRIVER1].cardnr
        );
    }
    else if (vdo.drv2_pos == vdo.index)
    {
        if (rx_byte == 0)
        {
            /* DIN2 field is empty */
            Tacho_Frame.driver[TACHO_DRIVER2].cardnr[0] = '\0';
        }
    }
    else if ( (vdo.drv2_pos < vdo.index) && (vdo.index < vdo.crc8_pos) )
    {
        /* Index points within the boundaries of the DIN2 field */
        pos = vdo.index - vdo.drv2_pos - 1;
        Tacho_VdoCopyDIN(
            pos,
            rx_byte,
            Tacho_Frame.driver[TACHO_DRIVER2].country,
            Tacho_Frame.driver[TACHO_DRIVER2].cardnr
        );
    }
}

/**
 * Initializes internal data for the VDO protocol
 */
static void Tacho_VdoInitHandler(void)
{
    vdo.index = TACHO_VDO_SEQSZ;
    vdo.crc8_value = TACHO_VDO_CRC_INIT;
    vdo.cstr_pos = 0xFF;
    vdo.drv1_pos = 0xFF;
    vdo.drv2_pos = 0xFF;
    vdo.crc8_pos = 0xFF;
}

/**
 * Handles data coming from a VDO-type Tachograph
 * @param rx_byte Received byte from D8 serial output
 * @return TRUE if end of frame detected
 *  FALSE if frame is still being processed
 */
static bool_t Tacho_VdoHandler(uint8_t rx_byte)
{
    switch (vdo.index)
    {
    case TACHO_VDO_WORKING_STATE:
        Tacho_Frame.working_state = rx_byte;
        break;

    case TACHO_VDO_DRV1_STATE:
        Tacho_Frame.driver1_state = rx_byte;
        break;

    case TACHO_VDO_DRV2_STATE:
        Tacho_Frame.driver2_state = rx_byte;
        break;

    case TACHO_VDO_STATUS:
        Tacho_Frame.tacho_status = rx_byte;
        break;

    case TACHO_VDO_SPEED_LSB:
        Tacho_Frame.speed_lsb = rx_byte;
        break;

    case TACHO_VDO_SPEED_MSB:
        Tacho_Frame.speed_msb = rx_byte;
        break;

    case TACHO_VDO_VIN_LENGTH:
        vdo.cstr_pos = TACHO_VDO_VIN_LENGTH + rx_byte + 1;
        break;

    default:
        break;
    }

    Tacho_VdoCheckDIN(rx_byte);

    if (vdo.cstr_pos == vdo.index)
    {
        vdo.drv1_pos = vdo.cstr_pos + rx_byte + 1;
    }
    else if (vdo.drv1_pos == vdo.index)
    {
        vdo.drv2_pos = vdo.drv1_pos + rx_byte + 1;
    }
    else if (vdo.drv2_pos == vdo.index)
    {
        vdo.crc8_pos = vdo.drv2_pos + rx_byte + 1;
    }
    else if (vdo.crc8_pos == vdo.index)
    {
        /* End of frame detected */
        if (rx_byte == vdo.crc8_value)
        {
            /* Checksum OK - frame received correctly */
            Tacho_CopyToCache();
            Tacho_NotifyFrameReceived(Tacho_CachedData.tco1);
        }
        Tacho_VdoInitHandler();
        return TRUE;
    }

    /* Frame is still being processed */
    vdo.crc8_value ^= rx_byte;
    vdo.index++;
    return FALSE;
}

/**
 * Copies the DIN fields from the D8 Stoneridge frame to the corresponding buffers
 *
 * @param pos Position in the DIN field
 * @param rx_byte Received byte from D8 serial output
 * @param country Pointer to country code buffer
 * @param cardnr Pointer to card number buffer
 */
static void Tacho_StoneridgeCopyDIN(uint8_t pos, uint8_t rx_byte, uint8_t *country, uint8_t *cardnr)
{
    if ( (rx_byte == 0xFF) && (pos == 0) )
    {
        /* DIN field is empty, so skip the field entirely */
        cardnr[0] = '\0';
        sr.drv1_pos = 0xFF;
        sr.drv2_pos = 0xFF;
        return;
    }

    if (pos < TACHO_MAX_COUNTRY_CODE)
    {
        /* Country Code */
        country[pos] = rx_byte;
    }
    else
    {
        /* Card Number */
        cardnr[pos - TACHO_MAX_COUNTRY_CODE] = rx_byte;
    }
}

/**
 * Checks if received byte contains data from a Stoneridge DIN field
 * @param rx_byte Received byte from D8 serial output
 */
static void Tacho_StoneridgeCheckDIN(uint8_t rx_byte)
{
    uint8_t pos;

    if ( (sr.drv1_pos == 0xFF) && (sr.drv2_pos == 0xFF) )
    {
        /* Message doesn't contain DIN1 or DIN2 info or DIN field is empty */
        return;
    }

    /* Handle DIN1 */
    if ( (sr.drv1_pos <= sr.index) && (sr.index < sr.crc8_pos - 1) )
    {
        pos = sr.index - sr.drv1_pos;
        Tacho_StoneridgeCopyDIN(
            pos,
            rx_byte,
            Tacho_Frame.driver[TACHO_DRIVER1].country,
            Tacho_Frame.driver[TACHO_DRIVER1].cardnr
        );
    }
    /* Handle DIN2 */
    else if ( (sr.drv2_pos <= sr.index) && (sr.index < sr.crc8_pos - 1) )
    {
        pos = sr.index - sr.drv2_pos;
        Tacho_StoneridgeCopyDIN(
            pos,
            rx_byte,
            Tacho_Frame.driver[TACHO_DRIVER2].country,
            Tacho_Frame.driver[TACHO_DRIVER2].cardnr
        );
    }
}

/**
 * Initializes internal data for the Stoneridge protocol
 */
static void Tacho_StoneridgeInitHandler(void)
{
    sr.index = TACHO_SR_SEQSZ;
    sr.crc8_value = 0;
    sr.drv1_pos = 0xFF;
    sr.drv2_pos = 0xFF;
    sr.crc8_pos = 0xFF;
}

/**
 * Handles data coming from a Stoneridge-type Tachograph
 * @param rx_byte Received byte from D8 serial output
 * @return TRUE if end of frame detected
 *  FALSE if frame is still being processed
 */
static bool_t Tacho_StoneridgeHandler(uint8_t rx_byte)
{
    switch (sr.index)
    {
    case TACHO_SR_MSG_LEN:
        if ( (rx_byte < TACHO_SR_MSG_LEN_MIN) || (rx_byte > TACHO_SR_MSG_LEN_MAX) )
        {
            /* Message length not in valid range - discard frame */
            Tacho_StoneridgeInitHandler();
            return TRUE;
        }
        /* Message length OK - compute position of last byte (CRC byte) */
        sr.crc8_pos = TACHO_SR_MSG_LEN + rx_byte - 1;
        break;

    case TACHO_SR_MSG_ID:
        if (FALSE == Tacho_StoneridgeMsgProcess(rx_byte))
        {
            /* Message ID not valid - discard frame */
            Tacho_StoneridgeInitHandler();
            return TRUE;
        }
        break;

    case TACHO_SR_WORKING_STATE:
        Tacho_Frame.working_state = rx_byte;
        break;

    case TACHO_SR_DRV1_STATE:
        Tacho_Frame.driver1_state = rx_byte;
        break;

    case TACHO_SR_DRV2_STATE:
        Tacho_Frame.driver2_state = rx_byte;
        break;

    case TACHO_SR_STATUS:
        Tacho_Frame.tacho_status = rx_byte;
        break;

    case TACHO_SR_SPEED_LSB:
        Tacho_Frame.speed_lsb = rx_byte;
        break;

    case TACHO_SR_SPEED_MSB:
        Tacho_Frame.speed_msb = rx_byte;
        break;

    default:
        break;
    }

    Tacho_StoneridgeCheckDIN(rx_byte);

    if (sr.crc8_pos == sr.index)
    {
        /* End of frame detected */
        sr.crc8_value = ~sr.crc8_value + 1;
        if (sr.crc8_value == rx_byte)
        {
            /* Checksum OK - frame received correctly */
            Tacho_CopyToCache();
            Tacho_NotifyFrameReceived(Tacho_CachedData.tco1);
        }
        Tacho_StoneridgeInitHandler();
        return TRUE;
    }

    sr.crc8_value += rx_byte;
    sr.index++;
    return FALSE;
}

/**
 * Determines message type and prepares DIN1 and DIN2 to be read when needed
 * @param rx_byte Received byte from D8 serial output
 * @return TRUE if message ID is valid; FALSE otherwise
 */
static bool_t Tacho_StoneridgeMsgProcess(uint8_t rx_byte)
{
    bool_t opSuccess = TRUE;

    switch (rx_byte)
    {
    case TACHO_SR_MSG_DIN1:
        sr.drv1_pos = TACHO_SR_CUSTOM;
        break;

    case TACHO_SR_MSG_DIN2:
        sr.drv2_pos = TACHO_SR_CUSTOM;
        break;

    case TACHO_SR_MSG_VIN:
    case TACHO_SR_MSG_VRN:
        /* Ignore - VIN and VRN not needed (for now) */
        break;

    default:
        /* Invalid message ID */
        opSuccess = FALSE;
        break;
    }

    return opSuccess;
}

/**
 * Called when data was successfully read
 * Copies the data received from D8 to a cache for future use
 */
static void Tacho_CopyToCache(void)
{
    uint8_t dindex = 0;
    uint8_t i, j;

    /* Create cached TCO1 message */
    Tacho_CachedData.tco1[TACHO_TCO1_WORKING_STATE] = Tacho_Frame.working_state;
    Tacho_CachedData.tco1[TACHO_TCO1_DRV1_STATE] = Tacho_Frame.driver1_state;
    Tacho_CachedData.tco1[TACHO_TCO1_DRV2_STATE] = Tacho_Frame.driver2_state;
    Tacho_CachedData.tco1[TACHO_TCO1_STATUS] = Tacho_Frame.tacho_status;
    Tacho_CachedData.tco1[TACHO_TCO1_RB4] = 0xFF;
    Tacho_CachedData.tco1[TACHO_TCO1_RB5] = 0xFF;
    Tacho_CachedData.tco1[TACHO_TCO1_SPEED_LSB] = Tacho_Frame.speed_lsb;
    Tacho_CachedData.tco1[TACHO_TCO1_SPEED_MSB] = Tacho_Frame.speed_msb;

    /* Copy driver ID data */
    for (i = 0; i < TACHO_MAX_DRIVERS; i++)
    {
        if (Tacho_Frame.driver[i].cardnr[0])
        {
            for (j = 0; j < TACHO_MAX_COUNTRY_CODE; j++)
            {
                Tacho_CachedData.di[dindex++] = Tacho_Frame.driver[i].country[j];
            }
            for (j = 0; j < TACHO_MAX_CARD_NR; j++)
            {
                Tacho_CachedData.di[dindex++] = Tacho_Frame.driver[i].cardnr[j];
            }
        }
        Tacho_CachedData.di[dindex++] = '*';
    }
    Tacho_CachedData.di[dindex++] = '\0';
}

/**
 * Common notification function called whenever TCO1-related data is
 * received either on CAN or on the D8 serial output.
 * In turn will call the FMI notification function if new data is available.
 *
 * @param tco1_data[in] This is the TCO1 8-byte buffer
 */
static void Tacho_NotifyFrameReceived(uint8_t *tco1_data)
{
    bool_t dataChanged = FALSE;
    uint8_t i;

    if (NULL != tco1_data)
    {
        /* Only check if the first 4 bytes have changed */
        for (i = 0; i < TACHO_TCO1_RB4; i++)
        {
            if (Tacho_CachedData.tco1_cmn[i] != tco1_data[i])
            {
                dataChanged = TRUE;
            }
        }

        if (dataChanged)
        {
            /* Copy to common buffer and fire event */
            for (i = 0; i < TACHO_TCO1_SIZE; i++)
            {
                Tacho_CachedData.tco1_cmn[i] = tco1_data[i];
            }
            FMI_process_j1939_event(J1939_EVENT_TCO1_AVAILABLE);
        }
    }
}

/**
 * Called by the J1939 when a TCO1 message has been read on CAN
 * @param event Should always be J1939_EVENT_TCO1_AVAILABLE
 */
void Tacho_process_j1939_event(uint8_t event)
{
    uint8_t *p_data;

    switch (event)
    {
    case (J1939_EVENT_TCO1_AVAILABLE):
        p_data = j1939_get_cached_tco1_content_p();
        Tacho_NotifyFrameReceived(p_data);
        break;

    default:
        break;
    }
}

/**
 * Called whenever a DI message from J1939 is received
 * @param di[in] Driver identification from J1939
 */
void Tacho_process_j1939_di(uint8_t *di)
{
    uint8_t index = 0;
    while ( (di[index] != 0) && (index < TACHO_MAX_DI_MSG) )
    {
        Tacho_CachedData.di[index] = di[index];
        index++;
    }
}

/**
 * Switches between tachograph standards
 * @param standard Specified tachograph standard
 * @param updateMemory Update FRAM memory with new Tachograph type
 */
static void Tacho_SelectStandard(Tacho_Standard_t standard, bool_t updateMemory)
{
    Tacho_ClearRxQueue();

    switch (standard)
    {
    case TACHO_STANDARD_VDO:
        Tacho_SelectedStandard = TACHO_STANDARD_VDO;
        Tacho_Proto = &Tacho_Protocol[TACHO_STANDARD_VDO];
        Tacho_Handler = &Tacho_VdoHandler;
        Tacho_VdoInitHandler();
        break;

    case TACHO_STANDARD_STONERIDGE:
        Tacho_SelectedStandard = TACHO_STANDARD_STONERIDGE;
        Tacho_Proto = &Tacho_Protocol[TACHO_STANDARD_STONERIDGE];
        Tacho_Handler = &Tacho_StoneridgeHandler;
        Tacho_StoneridgeInitHandler();
        break;

    default:
        break;
    }

    if ( (NULL != Tacho_Proto) && (standard < TACHO_STANDARD_MAX) )
    {
        USART2_set_baudrate(Tacho_Proto->baudRate);
        if (updateMemory)
        {
            Tacho_SetMemory(standard);
        }
    }
}

/**
 * Called each time a byte is received
 * @param rx_byte Byte value
 */
void Tacho_RxNotif(uint8_t rx_byte)
{
    Tacho_QueueAddByte(rx_byte);
}

/**
 * Callead each time a framing error occurs
 */
void Tacho_ErrorNotif(void)
{
    Tacho_RxQueue.error_counter++;
}

/**
 * Clears the reception buffer
 */
static void Tacho_ClearRxQueue(void)
{
    Tacho_RxQueue.count = 0;
    Tacho_RxQueue.error_counter = 0;
    Tacho_RxQueue.failed_attempts = 0;
    Tacho_RxQueue.head = Tacho_RxQueue.buffer_start;
    Tacho_RxQueue.tail = Tacho_RxQueue.buffer_start;
}

/**
 * Add byte to reception buffer
 * @param rx_byte Byte value
 * @return TRUE if byte has been added successfully, FALSE otherwise
 */
static bool_t Tacho_QueueAddByte(uint8_t rx_byte)
{
    bool_t opSuccess = FALSE;

    if (TACHO_RX_QUEUE_SIZE > Tacho_RxQueue.count)
    {
        opSuccess = TRUE;
        Tacho_RxQueue.count++;
        *Tacho_RxQueue.tail = rx_byte;
        if (Tacho_RxQueue.buffer_end == Tacho_RxQueue.tail)
        {
            Tacho_RxQueue.tail = Tacho_RxQueue.buffer_start;
        }
        else
        {
            Tacho_RxQueue.tail++;
        }
    }

    return opSuccess;
}

/**
 * Read & remove byte from reception buffer
 * @param byte_val[out] Holds the popped byte if dequeue is successful
 * @return TRUE if byte has been read/removed successfully, FALSE otherwise
 */
static bool_t Tacho_FetchByte(uint8_t *byte_val)
{
    bool_t opSuccess = FALSE;

    if (0 < Tacho_RxQueue.count)
    {
        opSuccess = TRUE;
        Tacho_RxQueue.count--;
        *byte_val = *Tacho_RxQueue.head;

        if (Tacho_RxQueue.buffer_end == Tacho_RxQueue.head)
        {
            Tacho_RxQueue.head = Tacho_RxQueue.buffer_start;
        }
        else
        {
            Tacho_RxQueue.head++;
        }
    }

    return opSuccess;
}
