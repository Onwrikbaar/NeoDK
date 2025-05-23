/*
 * matter.h -- some Matter definitions
 * 
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 11 October 2024
 *      Author: Mark
 *   Copyright  2024, 2025 Neostim™
 */

#ifndef INC_MATTER_H_
#define INC_MATTER_H_

#include <stdint.h>

typedef enum {
    OC_NONE, OC_STATUS_RESPONSE, OC_READ_REQUEST, OC_SUBSCRIBE_REQUEST, OC_SUBSCRIBE_RESPONSE, OC_REPORT_DATA,
    OC_WRITE_REQUEST, OC_WRITE_RESPONSE, OC_INVOKE_REQUEST, OC_INVOKE_RESPONSE, OC_TIMED_REQUEST
} Opcode;

typedef enum {
    SC_SUCCESS, SC_FAILURE,
    SC_INVALID_SUBSCRIPTION = 0x7d, SC_UNSUPPORTED_ACCESS, SC_UNSUPPORTED_ENDPOINT, SC_INVALID_ACTION, SC_UNSUPPORTED_COMMAND,
    SC_INVALID_COMMAND = 0x85, SC_UNSUPPORTED_ATTRIBUTE, SC_CONSTRAINT_ERROR, SC_UNSUPPORTED_WRITE, SC_RESOURCE_EXHAUSTED,
    SC_NOT_FOUND = 0x8B, SC_UNREPORTABLE_ATTRIBUTE, SC_INVALID_DATA_TYPE,
    SC_UNSUPPORTED_READ = 0x8f, SC_TIMEOUT = 0x94, SC_BUSY = 0x9c
} StatusCode;

typedef enum {
    EE_SIGNED_INT_1, EE_SIGNED_INT = EE_SIGNED_INT_1, EE_SIGNED_INT_2, EE_SIGNED_INT_4, EE_SIGNED_INT_8,
    EE_UNSIGNED_INT_1, EE_UNSIGNED_INT = EE_UNSIGNED_INT_1, EE_UNSIGNED_INT_2, EE_UNSIGNED_INT_4, EE_UNSIGNED_INT_8,
    EE_BOOLEAN_FALSE, EE_BOOLEAN_TRUE, EE_FLOAT_4, EE_FLOAT = EE_FLOAT_4, EE_FLOAT_8,
    EE_UTF8_1LEN, EE_UTF8_2LEN, EE_UTF8_4LEN, EE_UTF8_8LEN,
    EE_BYTES_1LEN, EE_BYTES_2LEN, EE_BYTES_4LEN, EE_BYTES_8LEN,
    EE_NULL, EE_STRUCT, EE_ARRAY, EE_LIST, EE_END_OF_CONTAINER
} ElementEncoding;

typedef uint16_t TransactionId;
typedef uint16_t SubscriptionId;

#ifdef __cplusplus
extern "C" {
#endif

uint16_t Matter_encodeUnsignedInteger(uint8_t dst[], uint8_t const *src, uint8_t nr_of_octets);
uint16_t Matter_encodedStringLength(char const *str);
uint16_t Matter_encodeString(uint8_t dst[], char const *str);
uint16_t Matter_encodedStringArrayLength(char const *strings[], uint8_t nr_of_strings);
uint16_t Matter_encodeStringArray(uint8_t dst[], char const *strings[], uint8_t nr_of_strings);
uint16_t Matter_encodedDataLength(ElementEncoding enc, uint16_t nr_of_octets);
uint16_t Matter_encode(uint8_t dst[], ElementEncoding, uint8_t const *src, uint16_t nr_of_octets);

#ifdef __cplusplus
}
#endif

#endif
