/********************************************************************************
 * Copyright (c) 2021 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_PARAMETERS_PARAMETER_PROXIES_H
#define MCX_CORE_PARAMETERS_PARAMETER_PROXIES_H

#include "CentralParts.h"

#include "fmu/Fmu2Value.h"
#include "objects/ObjectContainer.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


struct ScalarParameterProxy;
typedef struct ScalarParameterProxy ScalarParameterProxy;

typedef void (*fScalarParameterProxySetValue)(ScalarParameterProxy * proxy, Fmu2Value * value);
typedef ChannelType * (*fScalarParameterProxyGetType)(ScalarParameterProxy * proxy);
typedef Fmu2Value * (*fScalarParameterProxyGetValue)(ScalarParameterProxy * proxy);

extern const struct ObjectClass _ScalarParameterProxy;

struct ScalarParameterProxy {
    Object _;

    fScalarParameterProxySetValue SetValue;
    fScalarParameterProxyGetType GetType;
    fScalarParameterProxyGetValue GetValue;

    Fmu2Value * value_;
};


struct ArrayParameterProxy;
typedef struct ArrayParameterProxy ArrayParameterProxy;

typedef McxStatus(*fArrayParameterProxyAddValue)(ArrayParameterProxy * proxy, Fmu2Value * value);
typedef McxStatus(*fArrayParameterProxySetup)(ArrayParameterProxy * proxy, const char * name, size_t numDims, size_t * dims);
typedef const char * (*fArrayParameterProxyGetName)(ArrayParameterProxy * proxy);
typedef const char * (*fArrayParameterProxyGetDesc)(ArrayParameterProxy * proxy);
typedef const char * (*fArrayParameterProxyGetUnit)(ArrayParameterProxy * proxy);
typedef size_t (*fArrayParameterProxyGetNumDims)(ArrayParameterProxy * proxy);
typedef size_t (*fArrayParameterProxyGetDim)(ArrayParameterProxy * proxy, size_t idx);
typedef size_t (*fArrayParameterProxyGetSize)(ArrayParameterProxy * proxy);
typedef ObjectContainer * (*fArrayParameterProxyGetValues)(ArrayParameterProxy * proxy);
typedef ChannelType * (*fArrayParameterProxyGetType)(ArrayParameterProxy * proxy);
typedef fmi2_value_reference_t (*fArrayParameterProxyGetValueReference)(ArrayParameterProxy * proxy, size_t idx);
typedef ChannelValueData * (*fArrayParameterProxyGetMin)(ArrayParameterProxy * proxy);
typedef ChannelValueData * (*fArrayParameterProxyGetMax)(ArrayParameterProxy * proxy);

extern const struct ObjectClass _ArrayParameterProxy;

struct ArrayParameterProxy {
    Object _;

    fArrayParameterProxySetup Setup;
    fArrayParameterProxyAddValue AddValue;
    fArrayParameterProxyGetValues GetValues;
    fArrayParameterProxyGetName GetName;
    fArrayParameterProxyGetDesc GetDesc;
    fArrayParameterProxyGetUnit GetUnit;
    fArrayParameterProxyGetNumDims GetNumDims;
    fArrayParameterProxyGetDim GetDim;
    fArrayParameterProxyGetSize GetSize;
    fArrayParameterProxyGetType GetType;
    fArrayParameterProxyGetValueReference GetValueReference;
    fArrayParameterProxyGetMin GetMin;
    fArrayParameterProxyGetMax GetMax;

    size_t numDims_;
    size_t * dims_;

    char * name_;

    ObjectContainer * values_;       // of Fmu2Value
};


#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_PARAMETERS_PARAMETER_PROXIES_H */