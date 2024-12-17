/**
 * @file tacho.h
 * @author gabi
 * @date 17 Aug 2017
 *
 * Tachograph interpreter
 */

#ifndef TACHO_H
#define	TACHO_H

/******************************************************************************/
/*    DEFINITIONS                                                             */
/******************************************************************************/

/** Max driver ID size in bytes */
#define TACHO_MAX_DRIVER_ID 20

/** Size of DI field in bytes */
#define TACHO_MAX_DI_MSG (2 * TACHO_MAX_DRIVER_ID + 1)

/******************************************************************************/
/*    PUBLIC TYPES                                                            */
/******************************************************************************/

/** Data position in TCO1 message */
typedef enum
{
    TACHO_TCO1_WORKING_STATE,  /**< Driver working state */
    TACHO_TCO1_DRV1_STATE,  /**< DIN1 */
    TACHO_TCO1_DRV2_STATE,  /**< DIN2 */
    TACHO_TCO1_STATUS,  /**< Tachograph status */
    TACHO_TCO1_RB4,  /**< Reserved byte 4 */
    TACHO_TCO1_RB5,  /**< Reserved byte 5 */
    TACHO_TCO1_SPEED_LSB,  /**< Resolution: 1/256 km/h/bit */
    TACHO_TCO1_SPEED_MSB,  /**< Resolution: 1 km/h/bit */
    TACHO_TCO1_SIZE  /**< Total size of TCO1 message */
} Tacho_Tco1_Index_t;

/** Known types of Tachograph D8 serial output protocols */
typedef enum
{
    TACHO_STANDARD_VDO,
    TACHO_STANDARD_STONERIDGE,
    TACHO_STANDARD_MAX
} Tacho_Standard_t;

/******************************************************************************/
/*    PUBLIC FUNCTIONS                                                        */
/******************************************************************************/

void Tacho_process_j1939_event(uint8_t event);
void Tacho_process_j1939_di(uint8_t *di);
uint8_t *tacho_get_cached_tco1_content_p(void);
uint8_t *tacho_get_cached_di_content_p(void);

void Tacho_Init(void);
void Tacho_DeInit(void);
void Tacho_Task(void);
void Tacho_RxNotif(uint8_t rx_byte);
void Tacho_ErrorNotif(void);
Tacho_Standard_t Tacho_GetSelectedStandard(void);

#endif	/* TACHO_H */
