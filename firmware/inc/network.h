/*
 * network.h
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 5 Jan 2026
 *      Author: mark
 *   Copyright  2026 Neostim™
 */

#ifndef INC_NETWORK_H_
#define INC_NETWORK_H_

#include <stdint.h>

typedef struct {
    uint8_t  flags;                             // Version, type, etc.
    uint8_t  reserved;                          // Hop count, etc.
    uint16_t src_address;
    uint16_t dst_address;
    uint8_t  message[0];
} PacketHeader;

typedef enum { PT_ATTRIBUTE_ACTION } PacketType;


void PacketHeader_init(PacketHeader *);
PacketType PacketHeader_type(PacketHeader const *);

#endif
