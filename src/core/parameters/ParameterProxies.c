/********************************************************************************
 * Copyright (c) 2021 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "core/parameters/ParameterProxies.h"
#include "util/string.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


static void ScalarParameterProxySetValue(ScalarParameterProxy * proxy, Fmu2Value * value) {
    proxy->value_ = value;
}

static ChannelType ScalarParameterProxyGetType(ScalarParameterProxy * proxy) {
    if (proxy->value_) {
        return ChannelValueType(&proxy->value_->val);
    }

    return CHANNEL_UNKNOWN;
}

static Fmu2Value * ScalarParameterProxyGetValue(ScalarParameterProxy * proxy) {
    return proxy->value_;
}

static void ScalarParameterProxyDestructor(ScalarParameterProxy * proxy) {
    // don't destroy value as it is just a weak reference
}

static ScalarParameterProxy * ScalarParameterProxyCreate(ScalarParameterProxy * proxy) {
    proxy->SetValue = ScalarParameterProxySetValue;
    proxy->GetType = ScalarParameterProxyGetType;
    proxy->GetValue = ScalarParameterProxyGetValue;

    proxy->value_ = NULL;

    return proxy;
}

OBJECT_CLASS(ScalarParameterProxy, Object);



static McxStatus ArrayParameterProxyAddValue(ArrayParameterProxy * proxy, Fmu2Value * value) {
    McxStatus retVal = RETURN_OK;

    if (proxy->values_->Size(proxy->values_) > 0) {
        Fmu2Value * val = (Fmu2Value *)proxy->values_->At(proxy->values_, 0);

        // check that data types match
        if (ChannelValueType(&val->val) != ChannelValueType(&value->val)) {
            mcx_log(LOG_ERROR, "Adding value of '%s' to array proxy '%s' failed: Data type mismatch", value->name, proxy->name_);
            return RETURN_ERROR;
        }

        // check that units match
        if (!val->unit && value->unit || val->unit && !value->unit || val->unit && value->unit && strcmp(val->unit, value->unit) != 0) {
            mcx_log(LOG_ERROR, "Adding value of '%s' to array proxy '%s' failed: Unit mismatch", value->name, proxy->name_);
            return RETURN_ERROR;
        }
    }

    // add the value to the internal container
    retVal = proxy->values_->PushBackNamed(proxy->values_, (Object *)value, value->name);
    if (retVal == RETURN_ERROR) {
        mcx_log(LOG_ERROR, "Adding value of '%s' to array proxy '%s' failed", value->name, proxy->name_);
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus ArrayParameterProxySetup(ArrayParameterProxy * proxy, const char * name, size_t numDims, size_t * dims) {
    proxy->name_ = mcx_string_copy(name);
    if (name && !proxy->name_) {
        mcx_log(LOG_ERROR, "Array proxy setup failed: Not enough memory");
        return RETURN_ERROR;
    }

    proxy->numDims_ = numDims;
    proxy->dims_ = (size_t *)mcx_calloc(numDims, sizeof(size_t));
    if (!proxy->dims_) {
        mcx_log(LOG_ERROR, "Array proxy setup failed: Not enough memory");
        return RETURN_ERROR;
    }

    memcpy((void *)proxy->dims_, (void *)dims, numDims * sizeof(size_t));

    return RETURN_OK;
}

static const char * ArrayParameterProxyGetName(ArrayParameterProxy * proxy) {
    return proxy->name_;
}

static const char * ArrayParameterProxyGetDesc(ArrayParameterProxy * proxy) {
    if (proxy->values_->Size(proxy->values_) > 0) {
        Fmu2Value * value = (Fmu2Value *)proxy->values_->At(proxy->values_, 0);
        return value->info->desc;
    }

    return NULL;
}

static const char * ArrayParameterProxyGetUnit(ArrayParameterProxy * proxy) {
    if (proxy->values_->Size(proxy->values_) > 0) {
        Fmu2Value * value = (Fmu2Value *)proxy->values_->At(proxy->values_, 0);
        return value->unit;
    }

    return NULL;
}

static ObjectContainer * ArrayParameterProxyGetValues(ArrayParameterProxy * proxy) {
    return proxy->values_;
}

static size_t ArrayParameterProxyGetNumDims(ArrayParameterProxy * proxy) {
    return proxy->numDims_;
}

static size_t ArrayParameterProxyGetDim(ArrayParameterProxy * proxy, size_t idx) {
    if (idx >= proxy->numDims_) {
        return (size_t)(-1);
    }

    return proxy->dims_[idx];
}

static ChannelType ArrayParameterProxyGetType(ArrayParameterProxy * proxy) {
    if (proxy->values_->Size(proxy->values_) > 0) {
        Fmu2Value * value = (Fmu2Value *)proxy->values_->At(proxy->values_, 0);
        return ChannelValueType(&value->val);
    }

    return CHANNEL_UNKNOWN;
}

static void ArrayParameterProxyDestructor(ArrayParameterProxy * proxy) {
    if (proxy->name_) { mcx_free(proxy->name_); }
    if (proxy->dims_) { mcx_free(proxy->dims_); }

    proxy->numDims_ = 0;

    // don't destroy the contained objects as they are just weak references
    object_destroy(proxy->values_);
}

static fmi2_value_reference_t ArrayParameterProxyGetValueReference(ArrayParameterProxy * proxy, size_t idx) {
    if (idx < proxy->values_->Size(proxy->values_)) {
        Fmu2Value * value = (Fmu2Value *)proxy->values_->At(proxy->values_, idx);
        return value->data->vr.scalar;
    }

    return (fmi2_value_reference_t)(-1);;
}

static ChannelValueData * ArrayParameterProxyGetMin(ArrayParameterProxy * proxy) {
    if (proxy->values_->Size(proxy->values_) > 0) {
        Fmu2Value * value = (Fmu2Value *)proxy->values_->At(proxy->values_, 0);
        return value->info->min;
    }

    return NULL;
}

static ChannelValueData * ArrayParameterProxyGetMax(ArrayParameterProxy * proxy) {
    if (proxy->values_->Size(proxy->values_) > 0) {
        Fmu2Value * value = (Fmu2Value *)proxy->values_->At(proxy->values_, 0);
        return value->info->max;
    }

    return NULL;
}

static ArrayParameterProxy * ArrayParameterProxyCreate(ArrayParameterProxy * proxy) {
    proxy->Setup = ArrayParameterProxySetup;
    proxy->AddValue = ArrayParameterProxyAddValue;

    proxy->GetName = ArrayParameterProxyGetName;
    proxy->GetDesc = ArrayParameterProxyGetDesc;
    proxy->GetUnit = ArrayParameterProxyGetUnit;
    proxy->GetValues = ArrayParameterProxyGetValues;
    proxy->GetDim = ArrayParameterProxyGetDim;
    proxy->GetNumDims = ArrayParameterProxyGetNumDims;
    proxy->GetType = ArrayParameterProxyGetType;
    proxy->GetValueReference = ArrayParameterProxyGetValueReference;
    proxy->GetMin = ArrayParameterProxyGetMin;
    proxy->GetMax = ArrayParameterProxyGetMax;

    proxy->values_ = (ObjectContainer *)object_create(ObjectContainer);
    if (!proxy->values_) {
        return NULL;
    }

    proxy->name_ = NULL;
    proxy->dims_ = NULL;
    proxy->numDims_ = 0;

    return proxy;
}

OBJECT_CLASS(ArrayParameterProxy, Object);


#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */