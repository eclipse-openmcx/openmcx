/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_CHANNELS_CHANNEL_H
#define MCX_CORE_CHANNELS_CHANNEL_H

#include "CentralParts.h"
#include "core/channels/ChannelInfo.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct Config;
struct Component;

struct ChannelInData;
struct ChannelOutData;
struct Connection;


// ----------------------------------------------------------------------
// Channel

typedef struct Channel Channel;

typedef const void * (* fChannelGetValueReference)(Channel * channel);

typedef int (* fChannelIsValid)(Channel * channel);

typedef int (* fChannelIsConnected)(Channel * channel);

typedef int (* fChannelIsDefinedDuringInit)(Channel * channel);

typedef void (* fChannelSetDefinedDuringInit)(Channel * channel);

typedef McxStatus (* fChannelSetup)(Channel * channel, struct ChannelInfo * info);

typedef McxStatus (* fChannelUpdate)(Channel * channel, TimeInterval * time);

extern const struct ObjectClass _Channel;

struct Channel {
    Object _; // base class

    ChannelInfo info;

    // ----------------------------------------------------------------------
    // Value

    // NOTE: This flag gets set if there is a defined value for the
    // channel during initialization.
    int isDefinedDuringInit;

    const void * internalValue;
    ChannelValue value;

    /**
     * Virtual method.
     *
     * Returns a reference to the value of the channel. This value will be
     * updated on each call to Update().
     */
    fChannelGetValueReference GetValueReference;

    /**
     * Virtual method.
     *
     * Update the value of the channel to the specified start time in time.
     */
    fChannelUpdate Update;

    /**
     * Virtual method.
     *
     * Returns true if a channel provides a value (connected or default value)
     */
    fChannelIsValid IsValid;

    /**
     * Returns true if a channel is connected
     */
    fChannelIsConnected IsConnected;

    /**
     * Getter for the flag data->isDefinedDuringInit
     */
    fChannelIsDefinedDuringInit IsDefinedDuringInit;
    /**
     * Setter for the flag data->isDefinedDuringInit
     */
    fChannelSetDefinedDuringInit SetDefinedDuringInit;

    /**
     * Initialize channel with info struct.
     */
    fChannelSetup Setup;
};

// ----------------------------------------------------------------------
// ChannelIn

typedef struct ChannelIn ChannelIn;

typedef McxStatus (* fChannelInSetup)(ChannelIn * in, struct ChannelInfo * info);

typedef McxStatus  (* fChannelInSetReference) (ChannelIn   * in,
                                               void        * reference,
                                               ChannelType * type);

typedef struct ObjectContainer * (* fChannelInGetConnectionInfos)(ChannelIn * in);

typedef struct ObjectContainer * (* fChannelInGetConnections)(ChannelIn * in);

typedef McxStatus (*fChannelInRegisterConnection)(ChannelIn * in, struct Connection * connection, const char * unit, ChannelType * type);

typedef int (*fChannelInIsDiscrete)(ChannelIn * in);
typedef void (*fChannelInSetDiscrete)(ChannelIn * in);

extern const struct ObjectClass _ChannelIn;

struct ChannelIn {
    Channel _; // base class

    /**
     * Returns true if and only if a connection has been set into the in
     * channel or if the channel has a default value.
     */
    // Channel::IsValid

    fChannelInSetup Setup;

    /**
     * Sets the reference inside the component to which the value of the in
     * channel is written on every channel in Update(). Only one reference can be
     * registered and subsequent calls will fail.
     */
    fChannelInSetReference SetReference;

    /**
     * Returns the ConnectionInfo of the incoming connection.
     */
    fChannelInGetConnectionInfos GetConnectionInfos;

    fChannelInGetConnections GetConnections;

    /**
     * Set the connection from which the channel retrieves the values in the
     * specified unit.
     */
    fChannelInRegisterConnection RegisterConnection;

    /**
    * Returns true if a channel value is discrete
    */
    fChannelInIsDiscrete IsDiscrete;

    /**
    * sets a channel to discrete
    */
    fChannelInSetDiscrete SetDiscrete;

    struct ChannelInData * data;
};


// ----------------------------------------------------------------------
// ChannelOut
typedef struct ChannelOut ChannelOut;

typedef McxStatus (* fChannelOutSetup)(ChannelOut * out,
                                       struct ChannelInfo * info,
                                       struct Config * config);

typedef McxStatus (* fChannelOutSetReference) (ChannelOut * out,
                                               const void * reference,
                                               ChannelType * type);
typedef McxStatus (* fChannelOutSetReferenceFunction) (ChannelOut * out,
                                                       const proc * reference,
                                                       ChannelType * type);

typedef McxStatus (* fChannelOutRegisterConnection)(struct ChannelOut * out,
                                                    struct Connection * connection);

typedef const proc * (* fChannelOutGetFunction)(ChannelOut * out);

typedef ObjectList * (* fChannelOutGetConnections)(ChannelOut * out);

extern const struct ObjectClass _ChannelOut;

struct ChannelOut {
    Channel _; // base class

    /**
     * The type of the channel cannot be CHANNEL_UNKNOWN.
     */
    fChannelOutSetup Setup;

    /**
     * Returns true if and only if a connection has been registered with the
     * out channel.
     */
    // Channel::IsValid

    /**
     * Add a connection to the list of connections in the out channel whose SetValue
     * methods are called after each out channel Update().
     */
    fChannelOutRegisterConnection RegisterConnection;

    /**
     * Connect the out channel to the internal value of the component. The specified
     * type has to match the type of the channel.
     */
    fChannelOutSetReference SetReference;

    /**
     * Connect the out channel to the internal function of the component that supplies
     * the values for each time. The specified type hast to match the type of the channel.
     */
    fChannelOutSetReferenceFunction SetReferenceFunction;

    /**
     * Returns the function that was set with SetReferenceFunction or NULL if no function
     * was set.
     */
    fChannelOutGetFunction GetFunction;

    /**
     * Returns the ObjectContainer with all outgoing connections.
     */
    fChannelOutGetConnections GetConnections;

    struct ChannelOutData * data;
};

// ----------------------------------------------------------------------
// ChannelLocal
typedef struct ChannelLocal ChannelLocal;

typedef McxStatus (* fChannelLocalSetup)(ChannelLocal * local, struct ChannelInfo * info);

typedef McxStatus (* fChannelLocalSetReference) (ChannelLocal * local,
                                                 const void * reference,
                                                 ChannelType * type);

extern const struct ObjectClass _ChannelLocal;

struct ChannelLocal {
    Channel _; // base class

    /**
     * The type of the channel cannot be CHANNEL_UNKNOWN.
     */
    fChannelLocalSetup Setup;

    /**
     * Returns true if and only if a reference has been registered with the
     * local channel.
     */
    // Channel::IsValid

    /**
     * Connect the local channel to the internal value of the component. The specified
     * type has to match the type of the channel.
     */
    fChannelLocalSetReference SetReference;

    struct ChannelLocalData * data;
};

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_CHANNELS_CHANNEL_H */