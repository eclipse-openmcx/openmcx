/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "fmu/Fmu1Value.h"

#include "fmu/common_fmu1.h"

#include "util/string.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static McxStatus Fmu1ValueSetFromChannelValue(Fmu1Value * v, ChannelValue * val) {
    return ChannelValueSet(&v->val, val);
}

void Fmu1ValueDestructor(Fmu1Value * v) {
    if (v->name) {
        mcx_free(v->name);
        v->name = NULL;
    }

    ChannelValueDestructor(&v->val);
}

Fmu1Value * Fmu1ValueCreate(Fmu1Value * v) {
    v->SetFromChannelValue = Fmu1ValueSetFromChannelValue;

    v->name = NULL;
    v->var = NULL;
    v->channel = NULL;
    ChannelValueInit(&v->val, ChannelTypeUnknown);

    return v;
}

OBJECT_CLASS(Fmu1Value, Object);

Fmu1Value * Fmu1ValueMake(const char * name, fmi1_import_variable_t * var, Channel * channel) {
    Fmu1Value * value = (Fmu1Value *)object_create(Fmu1Value);

    if (value) {
        McxStatus retVal = RETURN_OK;
        fmi1_base_type_enu_t t;

        if (!name || !var) {
            mcx_log(LOG_ERROR, "Fmu1Value: Setup failed: Name or data missing");
            mcx_free(value);
            return NULL;
        }

        value->channel = channel;

        t = fmi1_import_get_variable_base_type(var);

        value->name = mcx_string_copy(name);
        value->vr = fmi1_import_get_variable_vr(var);
        value->var = var;
        ChannelValueInit(&value->val, Fmi1TypeToChannelType(t));

        if (!value->name) {
            mcx_log(LOG_ERROR, "Fmu1Value: Setup failed: Cannot copy name");
            mcx_free(value);
            return NULL;
        }
    }

    return value;
}

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */