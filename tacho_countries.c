/**
 * @file tacho_countries.c
 * @author gabi
 * @date 10 Oct 2017
 *
 * Tachograph countries look-up table
 */

/******************************************************************************/
/*    INCLUDED FILES                                                          */
/******************************************************************************/

#include "std_types.h"
#include "tacho_countries.h"

/******************************************************************************/
/*    DEFINITIONS                                                             */
/******************************************************************************/

#define TACHO_COUNTRY_ENTRIES 60

/******************************************************************************/
/*    PRIVATE TYPES                                                           */
/******************************************************************************/

typedef struct
{
    uint8_t code;
    uint8_t country[TACHO_MAX_COUNTRY_CODE];
} Tacho_Country_t;

/******************************************************************************/
/*    PRIVATE DATA                                                            */
/******************************************************************************/

static const Tacho_Country_t Tacho_Countries[TACHO_COUNTRY_ENTRIES] =
{
    {0x00, "   "},  /* No information available */
    {0x01, "A  "},  /* Austria */
    {0x02, "AL "},  /* Albania */
    {0x03, "AND"},  /* Andorra */
    {0x04, "ARM"},  /* Armenia */
    {0x05, "AZ "},  /* Azerbaijan */
    {0x06, "B  "},  /* Belgium */
    {0x07, "BG "},  /* Bulgaria */
    {0x08, "BIH"},  /* Bosnia Herzegovina */
    {0x09, "BY "},  /* Belarus */
    {0x0A, "CH "},  /* Switzerland */
    {0x0B, "CY "},  /* Cyprus */
    {0x0C, "CZ "},  /* Czech Republic */
    {0x0D, "D  "},  /* Germany */
    {0x0E, "DK "},  /* Denmark */
    {0x0F, "E  "},  /* Spain */
    {0x10, "EST"},  /* Estonia */
    {0x11, "F  "},  /* France */
    {0x12, "FIN"},  /* Finland */
    {0x13, "FL "},  /* Liechtenstein */
    {0x14, "FO "},  /* Faroe Islands */
    {0x15, "UK "},  /* United Kingdom */
    {0x16, "GE "},  /* Georgia */
    {0x17, "GR "},  /* Greece */
    {0x18, "H  "},  /* Hungary */
    {0x19, "HR "},  /* Croatia */
    {0x1A, "I  "},  /* Italy */
    {0x1B, "IRL"},  /* Ireland */
    {0x1C, "IS "},  /* Iceland */
    {0x1D, "KZ "},  /* Kazakhstan */
    {0x1E, "L  "},  /* Luxembourg */
    {0x1F, "LT "},  /* Lithuania */
    {0x20, "LV "},  /* Latvia */
    {0x21, "M  "},  /* Malta */
    {0x22, "MC "},  /* Monaco */
    {0x23, "MD "},  /* Moldova */
    {0x24, "MK "},  /* Macedonia (FYROM) */
    {0x25, "N  "},  /* Norway */
    {0x26, "NL "},  /* Netherlands */
    {0x27, "P  "},  /* Portugal */
    {0x28, "PL "},  /* Poland */
    {0x29, "RO "},  /* Romania */
    {0x2A, "RSM"},  /* San Marino */
    {0x2B, "RUS"},  /* Russia */
    {0x2C, "S  "},  /* Sweden */
    {0x2D, "SK "},  /* Slovakia */
    {0x2E, "SLO"},  /* Slovenia */
    {0x2F, "TM "},  /* Turkmenistan */
    {0x30, "TR "},  /* Turkey */
    {0x31, "UA "},  /* Ukraine */
    {0x32, "V  "},  /* Vatican City */
    {0x33, "YU "},  /* Yugoslavia (Code no longer in use since 2003) */
    {0x34, "MNE"},  /* Montenegro */
    {0x35, "SRB"},  /* Serbia */
    {0x36, "UZ "},  /* Uzbekistan */
    {0x37, "TJ "},  /* Tajikistan */
    {0xFD, "EC "},  /* European Community */
    {0xFE, "EUR"},  /* Rest of Europe */
    {0xFF, "WLD"},  /* Rest of the World */

    {0x38, "RFU"}  /* Reserved for Future Use (also used for default return value) */
};

/******************************************************************************/
/*    IMPLEMENTATION                                                          */
/******************************************************************************/

uint8_t *Tacho_GetCountryCode(uint8_t code)
{
    uint8_t i = 0;

    for (i = 0; i < TACHO_COUNTRY_ENTRIES; i++)
    {
        if (Tacho_Countries[i].code == code)
        {
            return (uint8_t *) Tacho_Countries[i].country;
        }
    }

    return (uint8_t *) Tacho_Countries[TACHO_COUNTRY_ENTRIES - 1].country;
}
