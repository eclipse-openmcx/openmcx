/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "reader/ssp/Units.h"

#include "util/compare.h"
#include "util/string.h"
#include "units/Units.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

SSDUnit* SSDUnitClone(SSDUnit * unit) {
    SSDUnit * clone = (SSDUnit*)object_create(SSDUnit);

    if (!clone) {
        goto cleanup;
    }

    clone->siDefinition = unit->siDefinition;

    clone->name = mcx_string_copy(unit->name);
    if (unit->name && !clone->name) {
        goto cleanup;
    }

    return clone;

cleanup:
    object_destroy(clone);
    return NULL;
}

static void SSDUnitDestructor(SSDUnit * unit) {
    if (unit->name) { mcx_free(unit->name); }
}

static SSDUnit * SSDUnitCreate(SSDUnit * unit) {
    unit->Clone = SSDUnitClone;

    unit->name = NULL;
    unit->siDefinition.kg = 0;
    unit->siDefinition.m = 0;
    unit->siDefinition.s = 0;
    unit->siDefinition.A = 0;
    unit->siDefinition.K = 0;
    unit->siDefinition.mol = 0;
    unit->siDefinition.cd = 0;
    unit->siDefinition.rad = 0;

    unit->siDefinition.factor = 1.0;
    unit->siDefinition.offset = 0.0;

    return unit;
}

OBJECT_CLASS(SSDUnit, Object);


ObjectContainer * SSDReadUnits(xmlNodePtr unitsNode) {
    ObjectContainer * units = (ObjectContainer*)object_create(ObjectContainer);

    size_t i = 0;
    size_t numUnits = 0;

    McxStatus retVal = RETURN_OK;

    if (!units) {
        goto cleanup_0;
    }

    if (!unitsNode) {
        goto cleanup_0;
    }

    numUnits = xml_num_children(unitsNode);
    for (i = 0; i < numUnits; i++) {
        xmlNodePtr unitNode = xml_child_by_index(unitsNode, i);
        SSDUnit * unit = (SSDUnit *)object_create(SSDUnit);
        int ret = 0;

        if (!unit) {
            retVal = RETURN_ERROR;
            goto cleanup_1;
        }

        retVal = xml_attr_string(unitNode, "name", &unit->name, SSD_MANDATORY);
        if (retVal == RETURN_ERROR) {
            goto cleanup_1;
        }

        {
            xmlNodePtr baseUnitNode = xml_child(unitNode, "BaseUnit");
            si_def unitSIDef = { 0, 0, 0, 0, 0, 0, 0, 0, 1.0, 0.0 };

            retVal = xml_attr_int32(baseUnitNode, "kg", &unitSIDef.kg, SSD_OPTIONAL);
            if (retVal == RETURN_ERROR) {
                goto cleanup_1;
            }

            retVal = xml_attr_int32(baseUnitNode, "m", &unitSIDef.m, SSD_OPTIONAL);
            if (retVal == RETURN_ERROR) {
                goto cleanup_1;
            }

            retVal = xml_attr_int32(baseUnitNode, "s", &unitSIDef.s, SSD_OPTIONAL);
            if (retVal == RETURN_ERROR) {
                goto cleanup_1;
            }

            retVal = xml_attr_int32(baseUnitNode, "A", &unitSIDef.A, SSD_OPTIONAL);
            if (retVal == RETURN_ERROR) {
                goto cleanup_1;
            }

            retVal = xml_attr_int32(baseUnitNode, "K", &unitSIDef.K, SSD_OPTIONAL);
            if (retVal == RETURN_ERROR) {
                goto cleanup_1;
            }

            retVal = xml_attr_int32(baseUnitNode, "mol", &unitSIDef.mol, SSD_OPTIONAL);
            if (retVal == RETURN_ERROR) {
                goto cleanup_1;
            }

            retVal = xml_attr_int32(baseUnitNode, "cd", &unitSIDef.cd, SSD_OPTIONAL);
            if (retVal == RETURN_ERROR) {
                goto cleanup_1;
            }

            retVal = xml_attr_int32(baseUnitNode, "rad", &unitSIDef.rad, SSD_OPTIONAL);
            if (retVal == RETURN_ERROR) {
                goto cleanup_1;
            }

            retVal = xml_attr_double(baseUnitNode, "factor", &unitSIDef.factor, SSD_OPTIONAL);
            if (retVal == RETURN_ERROR) {
                goto cleanup_1;
            }

            retVal = xml_attr_double(baseUnitNode, "offset", &unitSIDef.offset, SSD_OPTIONAL);
            if (retVal == RETURN_ERROR) {
                goto cleanup_1;
            }

            ret = mcx_units_add_si_def(unit->name, &unitSIDef);
            if (ret) {
                mcx_log(LOG_ERROR, "Could not add SI definition %s", unit->name);
                goto cleanup_1;
            }
        }

        // unit duplicates are not allowed
        if (units->GetByName(units, unit->name)) {
            retVal = xml_error_generic(unitNode, "Redefinition of unit %s", unit->name);
            goto cleanup_1;
        }

        retVal = units->PushBackNamed(units, (Object*)unit, unit->name);
        if (retVal == RETURN_ERROR) {
            goto cleanup_1;
        }

cleanup_1:
        if (retVal == RETURN_ERROR) {
            object_destroy(unit);
            goto cleanup_0;
        }
    }

cleanup_0:
    if (retVal == RETURN_ERROR) {
        object_destroy(units);
    }

    return units;
}


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */