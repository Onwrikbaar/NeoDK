/*
 *  net_frame.h
 *
 *  Created on: 29 Feb 2020
 *      Author: mark
 *   Copyright  2020..2024 Neostim™
 */

#include <stdbool.h>
#include <stdint.h>

#define FRAME_HEADER_SIZE       8
#define NR_OF_FRAME_SEQ_NRS     8

// ProtocolVersion (2 bits) determines size and interpretation of the frame header.
typedef enum { PROTO_FIXED, PROTO_VAR } ProtocolVersion;
typedef enum { FT_NONE, FT_ACK, FT_NAK, FT_SYNC, FT_DATA, FT_OPTIONS, FT_RESERVED_1, FT_RESERVED_2 } FrameType;
typedef enum { NST_DEBUG, NST_DATAGRAM, NST_VIRTUAL_CIRCUIT, NST_RESERVED } NetworkServiceType;

typedef struct _PhysFrame PhysFrame;            // Opaque type.

#ifdef __cplusplus
extern "C" {
#endif

// Class methods.
char const *PhysFrame_frameTypeName(FrameType);
char const *PhysFrame_serviceTypeName(NetworkServiceType);

// Instance methods.
PhysFrame *PhysFrame_initHeader(PhysFrame *, FrameType, uint8_t seq, NetworkServiceType);
PhysFrame *PhysFrame_initHeaderWithAck(PhysFrame *, FrameType, uint8_t seq, uint8_t ack, NetworkServiceType);
PhysFrame *PhysFrame_init(PhysFrame *, FrameType, uint8_t seq, NetworkServiceType, uint8_t const *, uint16_t nb);
bool PhysFrame_hasValidHeader(PhysFrame const *);

ProtocolVersion PhysFrame_protocolVersion(PhysFrame const *);
FrameType PhysFrame_type(PhysFrame const *);
NetworkServiceType PhysFrame_serviceType(PhysFrame const *);

uint8_t PhysFrame_seqNr(PhysFrame const *);
uint8_t PhysFrame_ackNr(PhysFrame const *);
uint16_t PhysFrame_payloadSize(PhysFrame const *);
uint8_t const *PhysFrame_payload(PhysFrame const *);
bool PhysFrame_isIntact(PhysFrame const *);

#ifdef __cplusplus
}
#endif
