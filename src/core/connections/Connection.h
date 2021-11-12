/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_CONNECTIONS_CONNECTION_H
#define MCX_CORE_CONNECTIONS_CONNECTION_H

#include "CentralParts.h"
#include "core/connections/ConnectionInfo.h"
#include "objects/Vector.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct Model;
struct ChannelIn;
struct ChannelOut;
struct ChannelFilter;
struct Component;

typedef enum {
    InCouplingStepMode,
    InCommunicationMode,
    InInitializationMode
} ConnectionState;

McxStatus CheckConnectivity(Vector * connections);
McxStatus MakeOneConnection(ConnectionInfo * info, InterExtrapolatingType isInterExtrapolating);


// ----------------------------------------------------------------------
// Connection

struct Connection;
typedef struct Connection Connection;

typedef McxStatus (* fConnectionSetup)(Connection * channel,
                                       struct ChannelOut        * out,
                                       struct ChannelIn         * in,
                                       struct ConnectionInfo    * info);

typedef struct ChannelOut * (* fConnectionGetSource)(Connection * connection);
typedef struct ChannelIn  * (* fConnectionGetTarget)(Connection * connection);

typedef void * (* fConnectionGetValueReference)(Connection * connection);
typedef void (* fConnectionSetValueReference)(Connection * connection, void * reference);
typedef ChannelDimension * (*fConnectionGetValueDimension)(Connection * connection);
typedef ChannelType * (*fConnectionGetValueType)(Connection * connection);

typedef ConnectionInfo * (* fConnectionGetInfo)(Connection * connection);

typedef int (* fConnectionGetBool)(Connection * connection);

typedef void (* fConnectionSetBool)(Connection * connection, int value);

typedef void (* fConnectionSetVoid)(Connection * connection);

typedef void (* fConnectionUpdateFromInput)(Connection * connection, TimeInterval * time);

typedef McxStatus (* fConnectionUpdateToOutput)(Connection * connection, TimeInterval * time);

typedef McxStatus (* fConnectionUpdateInitialValue)(Connection * connection);

typedef McxStatus (* fConnectionEnterInitializationMode)(Connection * connection);
typedef McxStatus (* fConnectionExitInitializationMode)(Connection * connection, double time);
typedef McxStatus (* fConnectionEnterCommunicationMode)(Connection * connection, double time);
typedef McxStatus (* fConnectionEnterCouplingMode)(Connection * connection
    , double communicationTimeStepSize, double sourceTimeStepSize, double targetTimeStepSize);

typedef McxStatus (* fConnectionAddFilter)(Connection * connection);

extern const struct ObjectClass _Connection;


struct Connection {
    Object _; // base class

    // source
    struct ChannelOut * out_;

    // target
    struct ChannelIn * in_;


    // ----------------------------------------------------------------------
    // Value on channel

    const void * value_;
    int useInitialValue_;

    ChannelValue store_;

    int isActiveDependency_;

    // Meta Data
    ConnectionInfo info;

    // Current state of the connection in state machine
    ConnectionState state_;

    // Temporary save functions during initialization mode
    fConnectionUpdateFromInput NormalUpdateFrom_;
    fConnectionUpdateToOutput NormalUpdateTo_;
    const void * normalValue_;

    /**
     * Virtual Method.
     *
     * Setup connection from ConnectionInfo struct and register with out
     * channel.
     */
    fConnectionSetup Setup;

    /**
     * Returns the source out channel.
     */
    fConnectionGetSource GetSource;

    /**
     * Returns the target in channel.
     */
    fConnectionGetTarget GetTarget;

    /**
     * Returns a reference to the value of the connection. This value will be
     * updated on each call to UpdateToOutput().
     */
    fConnectionGetValueReference GetValueReference;

    fConnectionGetValueDimension GetValueDimension;

    fConnectionGetValueType GetValueType;

    /**
     * Set the reference to the value of the connection. This value will be
     * updated on each call to UpdateToOutput().
     */
    fConnectionSetValueReference SetValueReference;

    /**
     * Returns the connection info struct.
     */
    fConnectionGetInfo GetInfo;

    /**
     * Returns if connection is set decoupled.
     */
    fConnectionGetBool IsDecoupled;

    /**
     * Getter for the flag `isDefinedDuringInit` of target channel
     */
    fConnectionGetBool IsDefinedDuringInit;
    /**
     * Setter for the flag `isDefinedDuringInit` of target channel
     */
    fConnectionSetVoid SetDefinedDuringInit;

    fConnectionGetBool IsActiveDependency;
    fConnectionSetBool SetActiveDependency;

    /**
     * Change state to InitializationMode and modifies UpdateFrom/To functions
     * for initialization mode.
     */
    fConnectionEnterInitializationMode EnterInitializationMode;

    /**
     * Change state to InitializationMode and modifies UpdateFrom/To functions
     * for normal mode.
     */
    fConnectionExitInitializationMode ExitInitializationMode;

    /**
     * Change state to CommunicationMode.
     */
    fConnectionEnterCommunicationMode EnterCommunicationMode;

    /**
     * Change state to CouplingStepMode.
     */
    fConnectionEnterCouplingMode EnterCouplingStepMode;


    /**
     * UpdateFromInput is the interface of the connection to the out channel in
     * which the connection is told that a value at a time is available.
     */
    fConnectionUpdateFromInput UpdateFromInput;

    /**
     * UpdateToOutput is the interface of the connection to the in channel. This
     * sets the value at the value reference for the specified time to the
     * updated value of the connection.
     */
    fConnectionUpdateToOutput UpdateToOutput;

    /**
     * Updates the initial value of the component based on the initial values
     * of the connected channels. Returns RETURN_ERROR if the initial value
     * could not be set or if the function was called outside of
     * InitializationMode.
     */
    fConnectionUpdateInitialValue UpdateInitialValue;

    fConnectionAddFilter AddFilter;
} ;

//------------------------------------------------------------------------------
// Common Functionality for Subclasses
McxStatus ConnectionSetup(Connection * connection, struct ChannelOut * out, struct ChannelIn * in, ConnectionInfo * info);

struct ChannelFilter *FilterFactory(ConnectionState *state,
                                    InterExtrapolationType extrapolation_type,
                                    InterExtrapolationParams *extrapolation_params,
                                    ChannelType *channel_type,
                                    InterExtrapolatingType inter_extrapolating_type,
                                    int is_decoupled,
                                    Component * sourceComp,
                                    Component * targetComp,
                                    const char * connString);


#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_CONNECTIONS_CONNECTION_H */