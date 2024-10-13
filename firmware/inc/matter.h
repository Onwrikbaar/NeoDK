/*
 * matter.h -- some Matter definitions
 * 
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 11 October 2024
 *      Author: Mark
 *   Copyright  2024 Neostim™
 */

#ifndef INC_MATTER_H_
#define INC_MATTER_H_

#include <stdint.h>

typedef enum {
    OC_NONE, OC_STATUS_RESPONSE, OC_READ_REQUEST, OC_SUBSCRIBE_REQUEST, OC_SUBSCRIBE_RESPONSE, OC_REPORT_DATA,
    OC_WRITE_REQUEST, OC_WRITE_RESPONSE, OC_INVOKE_REQUEST, OC_INVOKE_RESPONSE, OC_TIMED_REQUEST
} Opcode;

typedef enum {
    EE_SIGNED_INT_1, EE_SIGNED_INT_2, EE_SIGNED_INT_4, EE_SIGNED_INT_8,
    EE_UNSIGNED_INT_1, EE_UNSIGNED_INT_2, EE_UNSIGNED_INT_4, EE_UNSIGNED_INT_8,
    EE_BOOLEAN_FALSE, EE_BOOLEAN_TRUE, EE_FLOAT_4, EE_FLOAT_8,
    EE_UTF8_1LEN, EE_UTF8_2LEN, EE_UTF8_4LEN, EE_UTF8_8LEN,
    EE_BYTE_1LEN, EE_BYTE_2LEN, EE_BYTE_4LEN, EE_BYTE_8LEN,
    EE_NULL, EE_STRUCT, EE_ARRAY, EE_LIST, EE_END_OF_CONTAINER
} ElementEncoding;

#ifdef __cplusplus
extern "C" {
#endif

uint16_t Matter_encodedIntegerLength(uint8_t nr_of_octets);
uint16_t Matter_encodeUnsignedInteger(uint8_t dst[], uint8_t const *src, uint8_t nr_of_octets);
uint16_t Matter_encodedStringLength(char const *str);
uint16_t Matter_encodeString(uint8_t dst[], char const *str);
uint16_t Matter_encodedStringArrayLength(char const *strings[], uint8_t nr_of_strings);
uint16_t Matter_encodeStringArray(uint8_t dst[], char const *strings[], uint8_t nr_of_strings);

#ifdef __cplusplus
}
#endif

#endif
