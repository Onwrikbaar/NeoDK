/*
 * datalink.h
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 24 Feb 2024
 *      Author: mark
 *   Copyright  2024 Neostim™
 */

#ifndef INC_COMMS_H_
#define INC_COMMS_H_

#include <stdbool.h>
#include <stdint.h>

typedef void (*PacketCallback)(void *, uint8_t const *packet, uint16_t nb);
typedef struct _DataLink DataLink;              // Opaque type.

// Class method.
DataLink *DataLink_new();

// Instance methods.
bool DataLink_open(DataLink *, void *packet_handler, PacketCallback);
void DataLink_waitForSync(DataLink *);
bool DataLink_sendPacket(DataLink *, uint8_t const *, uint16_t);
void DataLink_close(DataLink *);
void DataLink_delete(DataLink *);

#endif
