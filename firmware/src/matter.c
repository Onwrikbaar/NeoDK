/*
 * matter.h -- Matter data encoding and decoding
 * 
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 12 October 2024
 *      Author: Mark
 *   Copyright  2024 Neostim™
 */

#include <string.h>
#include "matter.h"


uint16_t Matter_encodedIntegerLength(uint8_t nr_of_octets)
{
    return 1 + nr_of_octets;
}


uint16_t Matter_encodeUnsignedInteger(uint8_t dst[], uint8_t const *src, uint8_t nr_of_octets)
{
    switch (nr_of_octets)
    {
        case 1: dst[0] = EE_UNSIGNED_INT_1; break;
        case 2: dst[0] = EE_UNSIGNED_INT_2; break;
        case 4: dst[0] = EE_UNSIGNED_INT_4; break;
        case 8: dst[0] = EE_UNSIGNED_INT_8; break;
        default:
            dst[0] = EE_UNSIGNED_INT_1;
            dst[1] = 0;
            return 2;
    }
    uint16_t nbe = 1;
    for (uint8_t i = 0; i < nr_of_octets; i++) {
        dst[nbe++] = src[i];
    }
    return nbe;
}


uint16_t Matter_encodedStringLength(char const *str)
{
    uint16_t nbe = strlen(str);
    return nbe + (nbe < 256 ? 2 : 3);
}


uint16_t Matter_encodeString(uint8_t dst[], char const *str)
{
    uint16_t nbe = 0;
    uint16_t str_len = strlen(str);
    if (str_len < 256) {
        dst[nbe++] = EE_UTF8_1LEN;
        dst[nbe++] = str_len;
    } else {
        dst[nbe++] = EE_UTF8_2LEN;
        dst[nbe++] = str_len;
        dst[nbe++] = str_len >> 8;
    }
    memcpy(dst + nbe, str, str_len);
    return nbe + str_len;
}


uint16_t Matter_encodedStringArrayLength(char const *strings[], uint8_t nr_of_strings)
{
    uint16_t nbe = 2;                           // 2 bytes to Matter_encode array.
    for (uint8_t i = 0; i < nr_of_strings; i++) {
        nbe += Matter_encodedStringLength(strings[i]);
    }
    return nbe;
}


uint16_t Matter_encodeStringArray(uint8_t dst[], char const *strings[], uint8_t nr_of_strings)
{
    uint16_t nbe = 0;
    dst[nbe++] = EE_ARRAY;
    for (uint8_t i = 0; i < nr_of_strings; i++) {
        nbe += Matter_encodeString(dst + nbe, strings[i]);
    }
    dst[nbe++] = EE_END_OF_CONTAINER;
    return nbe;
}
