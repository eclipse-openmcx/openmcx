/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "core/channels/VectorChannelInfo.h"
#include "core/channels/ChannelInfo.h"
#include "util/string.h"
#include "objects/ObjectContainer.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static McxStatus VectorChannelInfoSetup(
    VectorChannelInfo * info,
    const char        * name,
    const char        * nameInModel,
    int                 isScalar,
    size_t              startIndex,
    size_t              endIndex)
{
    if (!(info->name = mcx_string_copy(name))) {
        mcx_log(LOG_ERROR, "Vector port: Could not copy name %s", name);
        return RETURN_ERROR;
    }
    if (nameInModel) {
        if (!(info->nameInTool = mcx_string_copy(nameInModel))) {
            mcx_log(LOG_ERROR, "Vector port: Could not copy name in tool %s", nameInModel);
            return RETURN_ERROR;
        }
    }

    info->isScalar = isScalar;
    if (info->isScalar) {
        // scalar lifted to vector
        info->startIndex = 0;
        info->endIndex = 0;
    } else {
        // proper vector
        if (SIZE_T_ERROR == startIndex) {
            mcx_log(LOG_ERROR, "Vector port: %s: Start index not defined", name);
            return RETURN_ERROR;
        }
        info->startIndex = startIndex;

        if (endIndex < startIndex) {
            mcx_log(LOG_ERROR, "Vector port: %s: End index %zu < Start index %zu", name, endIndex, startIndex);
            return RETURN_ERROR;
        }
        info->endIndex = endIndex;
    }

    return RETURN_OK;
}

static McxStatus VectorChannelInfoAddElement(
    VectorChannelInfo * info,
    ChannelInfo       * channel,
    size_t              index)
{
    McxStatus retVal = RETURN_OK;

    if (index < info->startIndex || index > info->endIndex) {
        mcx_log(LOG_ERROR, "Vector port: %s: Index %zu out of bounds [%d, %d]", info->GetName(info), index, info->startIndex, info->endIndex);
        return RETURN_ERROR;
    }

    retVal = info->channels->SetAt(info->channels, index - info->startIndex, &channel);
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "Vector port: %s: Could not add port info with index %zu", info->GetName(info), index);
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static size_t VectorChannelInfoGetStartIndex(VectorChannelInfo * info) {
    return info->startIndex;
}

static size_t VectorChannelInfoGetEndIndex(VectorChannelInfo * info) {
    return info->endIndex;
}

static const char * VectorChannelInfoGetName(VectorChannelInfo * info) {
    return (const char *) info->name;
}

static const char * VectorChannelInfoGetNameInTool(VectorChannelInfo * info) {
    return (const char *) info->nameInTool;
}

static ChannelInfo * VectorChannelInfoGetElement(VectorChannelInfo * info, size_t index) {
    return *(ChannelInfo **) info->channels->At(info->channels, index - info->startIndex);
}

static int VectorChannelInfoIsScalar(VectorChannelInfo * info) {
    return info->isScalar;
}

static void VectorChannelInfoDestructor(VectorChannelInfo * info) {
    if (info->name) {
        mcx_free(info->name);
    }

    if (info->nameInTool) {
        mcx_free(info->nameInTool);
    }

    object_destroy(info->channels);
}

static VectorChannelInfo * VectorChannelInfoCreate(VectorChannelInfo * info) {
    info->Setup         = VectorChannelInfoSetup;
    info->AddElement    = VectorChannelInfoAddElement;
    info->GetStartIndex = VectorChannelInfoGetStartIndex;
    info->GetEndIndex   = VectorChannelInfoGetEndIndex;
    info->GetName       = VectorChannelInfoGetName;
    info->GetNameInTool = VectorChannelInfoGetNameInTool;
    info->GetElement    = VectorChannelInfoGetElement;
    info->IsScalar      = VectorChannelInfoIsScalar;

    info->name = NULL;
    info->nameInTool = NULL;

    info->isScalar = FALSE;

    info->startIndex = SIZE_T_ERROR;
    info->endIndex = SIZE_T_ERROR;

    info->channels = (Vector *) object_create(Vector);
    if (!info->channels) {
        return NULL;
    }

    info->channels->Setup(info->channels, sizeof(ChannelInfo *), NULL, NULL, NULL);

    return info;
}

OBJECT_CLASS(VectorChannelInfo, Object);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */