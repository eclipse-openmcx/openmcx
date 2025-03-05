/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "CentralParts.h"

#include "core/channels/ChannelInfo.h"

#include "core/channels/ChannelValue.h"
#include "objects/Object.h"
#include "util/string.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


const char * ChannelInfoGetLogName(const ChannelInfo * info) {
    if (info->id) {
        return info->id;
    } else {
        return info->name;
    }
}

const char * ChannelInfoGetName(const ChannelInfo * info) {
    if (info->name) {
        return info->name;
    } else {
        return "";
    }
}

int ChannelInfoIsBinary(const ChannelInfo * info) {
    return ChannelTypeIsBinary(info->type);
}

static McxStatus ChannelInfoSetString(char ** dst, const char * src) {
    if (*dst) {
        mcx_free(*dst);
    }

    *dst = mcx_string_copy(src);

    if (src && !*dst) {
        mcx_log(LOG_DEBUG, "ChannelInfoSetString: Could not copy string \"%s\"", src);
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

McxStatus ChannelInfoSetName(ChannelInfo * info, const char * name) {
    return ChannelInfoSetString(&info->name, name);
}

McxStatus ChannelInfoSetNameInTool(ChannelInfo * info, const char * name) {
    return ChannelInfoSetString(&info->nameInTool, name);
}

McxStatus ChannelInfoSetID(ChannelInfo * info, const char * name) {
    return ChannelInfoSetString(&info->id, name);
}

McxStatus ChannelInfoSetDescription(ChannelInfo * info, const char * name) {
    return ChannelInfoSetString(&info->description, name);
}

McxStatus ChannelInfoSetUnit(ChannelInfo * info, const char * name) {
    return ChannelInfoSetString(&info->unitString, name);
}

McxStatus ChannelInfoSetType(ChannelInfo * info, ChannelType * type) {
    if (ChannelTypeIsValid(info->type)) {
        mcx_log(LOG_ERROR, "Port %s: Type already set", ChannelInfoGetLogName(info));
        return RETURN_ERROR;
    }

    info->type = ChannelTypeClone(type);

    if (ChannelInfoIsBinary(info)) {
        // the default for binary is off
        info->writeResult = FALSE;
    }

    return RETURN_OK;
}

McxStatus ChannelInfoSetup(ChannelInfo * info,
                           const char * name,
                           const char * nameInModel,
                           const char * descr,
                           const char * unit,
                           ChannelType * type,
                           const char * id) {
    if (name && RETURN_OK != ChannelInfoSetName(info, name)) {
        mcx_log(LOG_DEBUG, "Port %s: Could not set name", name);
        return RETURN_ERROR;
    }
    if (nameInModel && RETURN_OK != ChannelInfoSetNameInTool(info, nameInModel)) {
        mcx_log(LOG_DEBUG, "Port %s: Could not set name in tool", name);
        return RETURN_ERROR;
    }
    if (descr && RETURN_OK != ChannelInfoSetDescription(info, descr)) {
        mcx_log(LOG_DEBUG, "Port %s: Could not set description", name);
        return RETURN_ERROR;
    }
    if (unit && RETURN_OK != ChannelInfoSetUnit(info, unit)) {
        mcx_log(LOG_DEBUG, "Port %s: Could not set unit", name);
        return RETURN_ERROR;
    }
    if (id && RETURN_OK != ChannelInfoSetID(info, id)) {
        mcx_log(LOG_DEBUG, "Port %s: Could not set ID", name);
        return RETURN_ERROR;
    }
    if (RETURN_OK != ChannelInfoSetType(info, type)) {
        mcx_log(LOG_DEBUG, "Port %s: Could not set type", name);
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static void FreeChannelValue(ChannelValue ** value) {
    if (*value) {
        ChannelValueDestructor(*value);
        mcx_free(*value);
        *value = NULL;
    }
}

McxStatus ChannelInfoSetFrom(ChannelInfo * info, const ChannelInfo * other) {
    McxStatus retVal = RETURN_OK;

    retVal = ChannelInfoSetName(info, other->name);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "ChannelInfoSetFrom: Failed to set name");
        return RETURN_ERROR;
    }

    retVal = ChannelInfoSetNameInTool(info, other->nameInTool);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "ChannelInfoSetFrom: Failed to set name in model");
        return RETURN_ERROR;
    }

    retVal = ChannelInfoSetDescription(info, other->description);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "ChannelInfoSetFrom: Failed to set description");
        return RETURN_ERROR;
    }

    retVal = ChannelInfoSetUnit(info, other->unitString);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "ChannelInfoSetFrom: Failed to set unit");
        return RETURN_ERROR;
    }

    retVal = ChannelInfoSetID(info, other->id);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "ChannelInfoSetFrom: Failed to set name");
        return RETURN_ERROR;
    }

    info->type = ChannelTypeClone(other->type);
    info->mode = other->mode;
    info->writeResult = other->writeResult;
    info->connected = other->connected;
    info->initialValueIsExact = other->initialValueIsExact;

    info->channel = other->channel;  // weak reference

    FreeChannelValue(&info->min);
    FreeChannelValue(&info->max);
    FreeChannelValue(&info->scale);
    FreeChannelValue(&info->offset);
    FreeChannelValue(&info->defaultValue);
    FreeChannelValue(&info->initialValue);

    if (other->min) {
        info->min = ChannelValueClone(other->min);
        if (!info->min) {
            mcx_log(LOG_ERROR, "ChannelInfoSetFrom: Failed to set min");
            return RETURN_ERROR;
        }
    }

    if (other->max) {
        info->max = ChannelValueClone(other->max);
        if (!info->max) {
            mcx_log(LOG_ERROR, "ChannelInfoSetFrom: Failed to set max");
            return RETURN_ERROR;
        }
    }

    if (other->scale) {
        info->scale = ChannelValueClone(other->scale);
        if (!info->scale) {
            mcx_log(LOG_ERROR, "ChannelInfoSetFrom: Failed to set scale");
            return RETURN_ERROR;
        }
    }

    if (other->offset) {
        info->offset = ChannelValueClone(other->offset);
        if (!info->offset) {
            mcx_log(LOG_ERROR, "ChannelInfoSetFrom: Failed to set offset");
            return RETURN_ERROR;
        }
    }

    if (other->defaultValue) {
        info->defaultValue = ChannelValueClone(other->defaultValue);
        if (!info->defaultValue) {
            mcx_log(LOG_ERROR, "ChannelInfoSetFrom: Failed to set default value");
            return RETURN_ERROR;
        }
    }

    if (other->initialValue) {
        info->initialValue = ChannelValueClone(other->initialValue);
        if (!info->initialValue) {
            mcx_log(LOG_ERROR, "ChannelInfoSetFrom: Failed to set initial value");
            return RETURN_ERROR;
        }
    }

    object_destroy(info->dimension);
    if (other->dimension) {
        info->dimension = CloneChannelDimension(other->dimension);
        if (!info->dimension) {
            mcx_log(LOG_ERROR, "ChannelInfoSetFrom: Failed to set dimension");
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

static void FreeStr(char ** str) {
    if (*str) {
        mcx_free(*str);
        *str = NULL;
    }
}

void ChannelInfoDestroy(ChannelInfo * info) {
    FreeStr(&info->name);
    FreeStr(&info->nameInTool);
    FreeStr(&info->description);
    FreeStr(&info->unitString);
    FreeStr(&info->id);

    FreeChannelValue(&info->min);
    FreeChannelValue(&info->max);
    FreeChannelValue(&info->scale);
    FreeChannelValue(&info->offset);
    FreeChannelValue(&info->defaultValue);
    FreeChannelValue(&info->initialValue);

    if (info->type) {
        ChannelTypeDestructor(info->type);
    }

    if (info->dimension) {
        object_destroy(info->dimension);
    }

    if (info->connectionStatus) {
        DestroyConnectionStatus(info->connectionStatus);
    }

    info->channel = NULL;
    info->initialValueIsExact = FALSE;
    info->type = ChannelTypeClone(&ChannelTypeUnknown);
    info->connected = FALSE;
    info->writeResult = TRUE;
}

McxStatus ChannelInfoInit(ChannelInfo * info) {
    info->dimension = NULL;

    info->name        = NULL;
    info->nameInTool  = NULL;
    info->description = NULL;
    info->unitString  = NULL;
    info->id          = NULL;
    info->min         = NULL;
    info->max         = NULL;

    info->writeResult = TRUE;

    info->connected = FALSE;

    info->scale = NULL;
    info->offset = NULL;

    info->type = ChannelTypeClone(&ChannelTypeUnknown);
    info->defaultValue = NULL;
    info->initialValue = NULL;

    info->initialValueIsExact = FALSE;

    info->channel = NULL;

    info->connectionStatus = NULL;

    return RETURN_OK;
}


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */