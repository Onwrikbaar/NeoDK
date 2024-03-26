/*
 *  net_frame.h
 *
 *  Created on: 29 Feb 2020
 *      Author: mark
 *   Copyright  2020..2024 Neostim
 */

#include <stdbool.h>
#include <stdint.h>

typedef enum { FT_NONE, FT_ACK, FT_NAK, FT_SYNC, FT_DATA } FrameType;
typedef enum { PROTO_BARE, PROTO_V2020 } ProtocolVersion;

typedef struct _PhysFrame PhysFrame;            // Opaque type.

#ifdef __cplusplus
extern "C" {
#endif

// Class methods.
uint16_t PhysFrame_headerSize(void);
char const *PhysFrame_typeName(FrameType);

// Instance methods.
PhysFrame *PhysFrame_initHeader(PhysFrame *, FrameType, uint8_t seq);
PhysFrame *PhysFrame_initHeaderWithAck(PhysFrame *, FrameType, uint8_t seq, uint8_t ack);
PhysFrame *PhysFrame_init(PhysFrame *, FrameType, uint8_t seq, uint8_t const *, uint16_t nb);
bool PhysFrame_hasValidHeader(PhysFrame const *);
ProtocolVersion PhysFrame_protocolVersion(PhysFrame const *);
uint8_t PhysFrame_seqNr(PhysFrame const *);
uint8_t PhysFrame_ackNr(PhysFrame const *);
FrameType PhysFrame_type(PhysFrame const *);
uint16_t PhysFrame_payloadSize(PhysFrame const *);
uint8_t const *PhysFrame_payload(PhysFrame const *);
bool PhysFrame_isIntact(PhysFrame const *);

#ifdef __cplusplus
}
#endif
