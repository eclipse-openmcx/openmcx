/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_CORE_CONFIG_H
#define MCX_CORE_CONFIG_H

#include "CentralParts.h"

#include "reader/config/ConfigInput.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct Config Config;

typedef McxStatus (*fConfigSetupFromCmdLine)(Config * config, int argc, char ** argv);
typedef McxStatus (*fConfigSetupFromEnvironment)(Config * config);
typedef McxStatus (*fConfigSetupFromInput)(Config * self, ConfigInput * configInput);

extern const struct ObjectClass _Config;

/* \brief structure by which a config is identified. */
struct Config {
    Object _; // base class first

    fConfigSetupFromCmdLine SetupFromCmdLine;
    fConfigSetupFromEnvironment SetupFromEnvironment;
    fConfigSetupFromInput SetupFromInput;

    time_t timeStamp; // time of constructor call

    char * relPathToInputFile;
    char * absPathToInputFile;

    int outputModel;

    char * tempDir;
    char * localTempDir;
    int tempDirComandLine;

    int resultsEnabled;
    char * resultDir;

    char * logFile;

    char * modelFile;

    char * executable;

    int flushEveryStore;

    int sumTime;
    int sumTimeDefined;

    int writeAllLogFile;

    size_t interpolationBuffSize;
    size_t overrideInterpolationBuffSize;
    size_t interpolationBuffSizeLimit;
    size_t interpolationBuffSizeSafetyExt;

    int useMemFilter;
    size_t memFilterHistoryLimit;
    size_t memFilterHistoryExtra;

    int cosimInitEnabled;
    int patchWrongInitBehavior;

    int profilingMode;

    size_t maxNumTimeSnapWarnings;

    NaNCheckLevel nanCheck;
    int nanCheckNumMessages;
};

void CreateLogHeader(Config * config, LogSeverity sev);
void PrintConfig(const Config * config);

char * ConfigGetTempDir(const Config * config);
char * ConfigGetLocalTempDir(const Config * config);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */

#endif /* MCX_CORE_CONFIG_H */