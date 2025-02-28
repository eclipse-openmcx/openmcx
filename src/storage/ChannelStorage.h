/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_STORAGE_CHANNELSTORAGE_H
#define MCX_STORAGE_CHANNELSTORAGE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct ChannelStorage ChannelStorage;
typedef struct ComponentStorage ComponentStorage;

struct ChannelInfo;

typedef McxStatus (* fChannelStorageSetup)(ChannelStorage * channelStore, int fullStorage);
typedef McxStatus (* fChannelStorageRegisterChannel)(ChannelStorage * channelStore,
                                                     Channel * channel);
typedef McxStatus (* fChannelStorageStore)(ChannelStorage * channelStore, double time);
typedef size_t (* fChannelStorageGetChannelNum)(ChannelStorage * channelStore);
typedef ChannelValue (* fChannelStorageGetValueAt)(ChannelStorage * channelStore, size_t row, size_t col);
typedef ChannelValue * (* fChannelStorageGetValuesAtRow)(ChannelStorage * channelStore, size_t row);
typedef size_t (* fChannelStorageLength)(ChannelStorage * channelStore);
typedef struct ChannelInfo * (* fChannelStorageGetChannelInfo)(ChannelStorage * channelStore, size_t idx);

extern const struct ObjectClass _ChannelStorage;

typedef struct ChannelStorage {
    Object _; // super class first

    fChannelStorageSetup Setup;
    fChannelStorageRegisterChannel RegisterChannel;
    fChannelStorageStore Store;

    fChannelStorageGetChannelNum GetChannelNum;
    fChannelStorageGetValueAt GetValueAt;
    fChannelStorageGetValuesAtRow GetValuesAtRow;

    fChannelStorageLength Length;

    fChannelStorageGetChannelInfo GetChannelInfo;

    ObjectContainer * channels; /* of Channel */

    // the vector of values
    size_t numValues; /* number of rows used*/
    size_t numValuesAllocated; /* number of rows allocated */
    ChannelValue * values; /* of size numValuesAllocated * descriptions->Size() */

    double lastStored;

    int fullStorage;

    size_t storeCallNum;
} ChannelStorage;

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_STORAGE_CHANNELSTORAGE_H */