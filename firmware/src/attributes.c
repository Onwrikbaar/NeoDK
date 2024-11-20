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
    uint16_t active;
    AttributeId ai;                             // Key.
    AttrNotifier notify;
    void *target;
} Subscription;


static Subscription subscriptions[10];
static uint8_t nr_of_subs = 0;


static Subscription *findSubForId(AttributeId ai)
{
    for (uint8_t i = 0; i < M_DIM(subscriptions); i++) {
        Subscription *sub = &subscriptions[i];
        if (ai == sub->ai) return sub;
    }

    return NULL;
}


static SubscriptionId setSubForId(AttributeId ai, AttrNotifier notify, void *target, uint16_t count)
{
    Subscription *sub = findSubForId(ai);
    if (sub == NULL) {
        if (nr_of_subs == M_DIM(subscriptions)) return 0;
        sub = &subscriptions[nr_of_subs++];
        sub->ai = ai;
    }
    sub->notify = notify;
    sub->target = target;
    sub->active = count;
    return 256 + (sub - subscriptions);
}

/*
 * Below are the functions implementing this module's interface.
 */

SubscriptionId Attribute_awaitRead(AttributeId ai, AttrNotifier notify, void *target)
{
    BSP_logf("%s for id=%hu)\n", __func__, ai);
    return setSubForId(ai, notify, target, 1);
}


SubscriptionId Attribute_subscribe(AttributeId ai, AttrNotifier notify, void *target)
{
    // BSP_logf("%s for id=%hu\n", __func__, ai);
    return setSubForId(ai, notify, target, ~0);
}


void Attribute_changed(AttributeId ai, ElementEncoding enc, uint8_t const *data, uint16_t size)
{
    BSP_logf("%s(%hu)\n", __func__, ai);
    Subscription *sub = findSubForId(ai);
    if (sub != NULL && sub->active) {
        sub->notify(sub->target, ai, enc, data, size);
        sub->active -= 1;
    }
}
