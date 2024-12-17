/**
 * @file tacho_countries.h
 * @author gabi
 * @date 10 Oct 2017
 *
 * Tachograph countries look-up table
 */

#ifndef TACHO_COUNTRIES_H
#define	TACHO_COUNTRIES_H

/******************************************************************************/
/*    DEFINITIONS                                                             */
/******************************************************************************/

#define TACHO_MAX_COUNTRY_CODE 3  /**< Size of country code */

/******************************************************************************/
/*    PUBLIC FUNCTIONS                                                        */
/******************************************************************************/

uint8_t *Tacho_GetCountryCode(uint8_t code);

#endif	/* TACHO_COUNTRIES_H */
