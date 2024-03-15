/*
 * comms.h
 *
 *  Created on: 24 Feb 2024
 *      Author: mark
 *   Copyright  2024 Neostim
 */

#ifndef INC_COMMS_H_
#define INC_COMMS_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct _Comms Comms;                    // Opaque type.

// Class method.
Comms *Comms_new();

// Instance methods.
bool Comms_open(Comms *);
uint32_t Comms_write(Comms *, uint8_t const *, size_t);
void Comms_close(Comms *);
void Comms_delete(Comms *);

#endif
