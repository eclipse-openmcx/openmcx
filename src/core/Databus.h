/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_DATABUS_H
#define MCX_CORE_DATABUS_H

#include "CentralParts.h"
#include "core/Config.h"
#include "core/connections/filters/Filter.h"
#include "objects/StringContainer.h"
#include "core/channels/Channel.h"
#include "core/connections/Connection.h"
#include "core/Component.h"
#include "reader/model/ports/PortsInput.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct DatabusInfo;
struct Databus;

typedef struct Databus Databus;
typedef struct DatabusInfo DatabusInfo;

// ----------------------------------------------------------------------
// DatabusInfo

int DatabusInfoGetChannelID(struct DatabusInfo * info, const char * name);

McxStatus DatabusInfoRead(struct DatabusInfo * info,
                          PortsInput * input,
                          fComponentChannelRead SpecificRead,
                          struct Component * comp,
                          ChannelMode mode);

struct ChannelInfo * DatabusInfoGetChannel(struct DatabusInfo * info, size_t i);
size_t DatabusInfoGetChannelNum(struct DatabusInfo * info);



// ----------------------------------------------------------------------
// Databus

/* public interface for components */

/**
 * \return The number of out channels of \a db or -1 if \a db is not initialized
 * correctly.
 */
size_t DatabusGetOutChannelsNum(struct Databus * db);

/**
 * \return The number of in channels of \a db or -1 if \a db is not initialized
 * correctly.
 */
size_t DatabusGetInChannelsNum(struct Databus * db);

/**
 * \return The number of local channels of \a db or -1 if \a db is not initialized
 * correctly.
 */
size_t DatabusGetLocalChannelsNum(struct Databus * db);

size_t DatabusGetInChannelsElemNum(Databus * db);
size_t DatabusGetOutChannelsElemNum(Databus * db);

/**
 * \return The number of rtfactor channels of \a db or -1 if \a db is not initialized
 * correctly.
 */
size_t DatabusGetRTFactorChannelsNum(struct Databus * db);

/**
 * Connects the variable of type \a type at \a reference to the out channel \a channel
 * in \a db. The value at \a reference will be copied to all connected channels at each
 * communication step.
 *
 * \return \c RETURN_OK on success, or \c RETURN_ERROR otherwise.
 */
McxStatus DatabusSetOutReference(struct Databus * db,
                                 size_t           channel,
                                 const void     * reference,
                                 ChannelType    * type);


McxStatus DatabusSetOutReferenceFunction(struct Databus * db,
                                         size_t           channel,
                                         const void     * reference,
                                         ChannelType    * type);

/**
 * Connects a variable of type \a type at \a reference to the in channel \a channel
 * in \a db. On all communication steps, the current value of the channel will be
 * copied to \a reference.
 *
 * \return \c RETURN_OK on success, or \c RETURN_ERROR otherwise.
 */
McxStatus DatabusSetInReference(struct Databus * db, size_t channel, void * reference, const ChannelType * type);

/**
 * Adds a local channel of type \a type at \a reference to the databus \a db.
 */
McxStatus DatabusAddLocalChannel(Databus * db,
                                 const char * name,
                                 const char * id,
                                 const char * unit,
                                 const void * reference,
                                 ChannelType * type);

/**
 * Adds a rtfactor channel of type \a type at \a reference to the databus \a db.
 */
McxStatus DatabusAddRTFactorChannel(Databus * db,
                                   const char * name,
                                   const char * id,
                                   const char * unit,
                                   const void * reference,
                                   ChannelType * type);


/**
 * \return The address of the value of the in channel \a channel in \a db or \c NULL
 * if \a db or \a channel are invalid.
 */
const void * DatabusGetInValueReference(struct Databus * db, size_t channel);

/**
 * \return The address of the value of the out channel \a channel in \a db or \c NULL
 * if \a db or \a channel are invalid.
 */
const void * DatabusGetOutValueReference(struct Databus * db, size_t channel);

/**
 * \return A pointer to the in channel \a channel of \a db or \c NULL if \a db or
 * \a channel are invalid.
 */
struct ChannelInfo * DatabusGetInChannelInfo (struct Databus * db, size_t channel);

/**
 * \return A pointer to the out channel \a channel of \a db or \c NULL if \a db or
 * \a channel are invalid.
 */
struct ChannelInfo * DatabusGetOutChannelInfo(struct Databus * db, size_t channel);


/** private interface for Component **/

/**
 * Initializes the in and out channels according to in and out.
 * db, connection have to be != NULL
 *
 * \return \c RETURN_OK on success, or \c RETURN_ERROR otherwise.
 */
McxStatus DatabusSetup(struct Databus * db, struct DatabusInfo * in, struct DatabusInfo * out, Config * config);


/**
 * Updates the values of all out channels of \a db. The values will
 * be converted according to the specified unit and min/max values. First, the
 * values will be set capped to the min/max values, then if a unit
 * is specified, the values are converted to their respective SI unit.
 *
 * Only after a call to this function, the connections from the out channels
 * have access to the values at the specified start time in \a time.
 *
 * \return \c RETURN_OK on success, or \c RETURN_ERROR otherwise.
 */
McxStatus DatabusTriggerOutChannels(struct Databus * db, TimeInterval * time);


/* private interface for Model */

/**
 * Creates a \a Connection and connectes it to the channel in \a db specified
 * in \a info.
 *
 * \return The created \c Connection or \c NULL if \a db or \a info are invalid.
 */
struct Connection * DatabusCreateConnection(struct Databus * db, struct ConnectionInfo * info);


/* private interface for Component and Task */

/**
 * For all in channels in \a db updates the connection filter for the start time
 * in time \a consumerTime and updates the values of all registered references in
 * the in channels.
 *
 * \return \c RETURN_OK on success, or \c RETURN_ERROR otherwise.
 */
McxStatus DatabusTriggerInConnections(struct Databus * db, TimeInterval * consumerTime);
McxStatus DatabusTriggerConnectedInConnections(struct Databus * db, TimeInterval * consumerTime);

McxStatus DatabusUpdateInConnected(Databus * db);

McxStatus DatabusCollectModeSwitchData(Databus * db);
McxStatus DatabusEnterCouplingStepMode(struct Databus * db, double timeStepSize);
McxStatus DatabusEnterCommunicationMode(struct Databus * db, double time);
McxStatus DatabusEnterCommunicationModeForConnections(Databus * db, ObjectList * connections, double time);

/* private interface for Component, Model, Task */

/**
 * Accessor for \a inInfo.
 *
 * \return \c NULL if \a db is invalid.
 */
struct DatabusInfo * DatabusGetInInfo(struct Databus * db);

/**
 * Accessor for \a outInfo.
 *
 * \return \c NULL if \a db is invalid.
 */
struct DatabusInfo * DatabusGetOutInfo(struct Databus * db);

/**
 * Accessor for \a localInfo.
 *
 * \return \c NULL if \a db is invalid.
 */
struct DatabusInfo * DatabusGetLocalInfo(Databus * db);

/**
 * Accessor for \a rtfactorInfo.
 *
 * \return \c NULL if \a db is invalid.
 */
struct DatabusInfo * DatabusGetRTFactorInfo(Databus * db);

/**
 * \return the number of \c ChannelInfo where \c GetWriteResultFlag is \c true.
 */
size_t DatabusInfoGetNumWriteChannels(DatabusInfo * dbInfo);

/* internal */

int DatabusInChannelsDefined(Databus * db);
int DatabusOutChannelsDefined(Databus * db);

/**
 * Accessor function for the \a i-th in channel of \a db.
 *
 * \return \a NULL if \a db or \a i are invalid.
 */
struct ChannelIn * DatabusGetInChannel(struct Databus * db, size_t i);

/**
 * Accessor function for the \a i-th out channel of \a db.
 *
 * \return \a NULL if \a db or \a i are invalid.
 */
struct ChannelOut * DatabusGetOutChannel(struct Databus * db, size_t i);

/**
 * Accessor function for the \a i-th local channel of \a db.
 *
 * \return \a NULL if \a db or \a i are invalid.
 */
struct Channel * DatabusGetLocalChannel(Databus * db, size_t i);

/**
 * Accessor function for the \a i-th rtfactor channel of \a db.
 *
 * \return \a NULL if \a db or \a i are invalid.
 */
struct Channel * DatabusGetRTFactorChannel(Databus * db, size_t i);

extern const struct ObjectClass _Databus;


typedef struct {
    Connection * connection;
    double sourceTimeStepSize;
    double targetTimeStepSize;
} ModeSwitchData;

typedef struct Databus {
    Object _; // base class

    struct DatabusData * data;

    ModeSwitchData * modeSwitchData;
    size_t modeSwitchDataSize;
} Databus;

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_DATABUS_H */