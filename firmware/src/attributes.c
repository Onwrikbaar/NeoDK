/*
 * attributes.c
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 15 Oct 2024
 *      Author: mark
 *   Copyright  2024 Neostim™
 */

#include "bsp_dbg.h"
#include "convenience.h"

// This module implements:
#include "attributes.h"


typedef struct {
    AttributeId ai;                             // Key.
    ElementEncoding encoding;
    AttrNotifier notify;
    void *target;
} Subscription;


static Subscription subscriptions[4];
static uint8_t nr_of_subs = 0;


static Subscription const *findSubForId(AttributeId ai)
{
    for (uint8_t i = 0; i < M_DIM(subscriptions); i++) {
        Subscription const *sub = &subscriptions[i];
        if (ai == sub->ai) return sub;
    }
    return NULL;
}

/*
 * Below are the functions implementing this module's interface.
 */

bool Attribute_subscribe(AttributeId ai, ElementEncoding enc, AttrNotifier notify, void *target)
{
    BSP_logf("%s for id=%hu\n", __func__, ai);
    if (nr_of_subs == M_DIM(subscriptions)) return false;

    Subscription *sub = &subscriptions[nr_of_subs++];
    sub->ai = ai;
    sub->encoding = enc;
    sub->notify = notify;
    sub->target = target;
    return true;
}


void Attribute_changed(AttributeId ai, uint8_t const *data, uint16_t size)
{
    // BSP_logf("%s(%hu)\n", __func__, ai);
    Subscription const *sub = findSubForId(ai);
    if (sub != NULL) {
        sub->notify(sub->target, ai, sub->encoding, data, size);
    }
}
