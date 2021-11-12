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
#include "core/connections/Connection.h"
#include "core/channels/Channel.h"
#include "core/channels/ChannelInfo.h"
#include "core/connections/ConnectionInfo.h"
#include "core/Conversion.h"

#include "core/Databus.h"
#include "core/Component.h"
#include "core/Model.h"

// Filter
#include "core/connections/filters/DiscreteFilter.h"
#include "core/connections/filters/MemoryFilter.h"
#include "core/connections/filters/IntExtFilter.h"
#include "core/connections/filters/ExtFilter.h"
#include "core/connections/filters/IntFilter.h"

#include "util/compare.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


static void UpdateInChannelInfo(Component * comp, size_t idx) {
    Databus * db = comp->GetDatabus(comp);

    if (DatabusInChannelsDefined(db)) {
        Channel * channel = (Channel *) DatabusGetInChannel(db, idx);
        if (channel) {
            channel->info.connected = 1;
        }
    }
}

static void UpdateOutChannelInfo(Component * comp, size_t idx) {
    Databus * db = comp->GetDatabus(comp);

    if (DatabusOutChannelsDefined(db)) {
        Channel * channel = (Channel *) DatabusGetOutChannel(db, idx);
        if (channel) {
            channel->info.connected = 1;
        }
    }
}

McxStatus CheckConnectivity(Vector * connections) {
    size_t i = 0;

    size_t connSize = connections->Size(connections);

    for (i = 0; i < connSize; i++) {
        ConnectionInfo * connInfo = (ConnectionInfo *) connections->At(connections, i);
        ChannelInfo * info = NULL;
        Component * target = connInfo->targetComponent;
        int targetId = connInfo->targetChannel;

        Component * source = connInfo->sourceComponent;
        int sourceId = connInfo->sourceChannel;

        info = DatabusInfoGetChannel(DatabusGetInInfo(target->GetDatabus(target)), targetId);
        if (info) {
            info->connected = 1;
            UpdateInChannelInfo(target, targetId);
        }

        info = DatabusInfoGetChannel(DatabusGetOutInfo(source->GetDatabus(source)), sourceId);
        if (info) {
            info->connected = 1;
            UpdateOutChannelInfo(source, sourceId);
        }
    }

    return RETURN_OK;
}

McxStatus MakeOneConnection(ConnectionInfo * info, InterExtrapolatingType isInterExtrapolating) {
    Component * source = NULL;
    Component * target = NULL;

    Connection * connection = NULL;

    ChannelInfo * outInfo = NULL;
    ChannelInfo * inInfo = NULL;

    source = info->sourceComponent;
    target = info->targetComponent;

    // Get data types of involved channels
    outInfo = DatabusInfoGetChannel(DatabusGetOutInfo(source->GetDatabus(source)), info->sourceChannel);
    inInfo = DatabusInfoGetChannel(DatabusGetInInfo(target->GetDatabus(target)), info->targetChannel);

    if (!outInfo || !inInfo) {
        mcx_log(LOG_ERROR, "Connection: Make connection: Invalid arguments");
        return RETURN_ERROR;
    }

    InterExtrapolationParams * params = &info->interExtrapolationParams;

    if (EXTRAPOLATING == isInterExtrapolating) {
        if (params->extrapolationOrder != params->interpolationOrder) {
            isInterExtrapolating = INTEREXTRAPOLATING;
        }
    } else if (INTERPOLATING == isInterExtrapolating) {
        isInterExtrapolating = INTEREXTRAPOLATING;
    }

    info->isInterExtrapolating = isInterExtrapolating;

    connection = DatabusCreateConnection(source->GetDatabus(source), info);
    if (!connection) {
        mcx_log(LOG_ERROR, "Connection: Make connection: Could not create connection");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static void LogStepRatios(double sourceStep, double targetStep, double synchStep, const char * connString) {
    if (sourceStep <= synchStep && targetStep <= synchStep) {
        MCX_DEBUG_LOG("CONN %s: source <= synch && target <= synch", connString);
    } else if (sourceStep <= synchStep && targetStep > synchStep) {
        MCX_DEBUG_LOG("CONN %s: source <= synch && target > synch", connString);
    } else if (sourceStep > synchStep && targetStep <= synchStep) {
        MCX_DEBUG_LOG("CONN %s: source > synch && target <= synch", connString);
    } else {
        MCX_DEBUG_LOG("CONN %s: source > synch && target > synch", connString);
    }
}

static int ComponentMightNotRespectStepSize(Component * comp) {
    return FALSE;
}

static size_t DetermineFilterBufferSize(Component * source, Component * target, const char * connString) {
    Model * model = source->GetModel(source);
    Task * task = model->GetTask(model);

    double synchStep = task->GetTimeStep(task);
    double sourceStep = source->GetTimeStep(source) > 0 ? source->GetTimeStep(source) : synchStep;
    double targetStep = target->GetTimeStep(target) > 0 ? target->GetTimeStep(target) : synchStep;

    size_t buffSize = 0;

    if (model->config->overrideInterpolationBuffSize > 0) {
        buffSize = model->config->overrideInterpolationBuffSize;
    }
    else if (ComponentMightNotRespectStepSize(source) || ComponentMightNotRespectStepSize(target)) {
        buffSize = model->config->interpolationBuffSize;
    }
    else {
        buffSize = (size_t) ceil(synchStep / sourceStep) + 1;
    }

    buffSize += model->config->interpolationBuffSizeSafetyExt;

    if (buffSize > model->config->interpolationBuffSizeLimit) {
        mcx_log(LOG_WARNING, "%s: buffer limit exceeded (%zu > &zu). Limit can be changed via MC_INTERPOLATION_BUFFER_SIZE_LIMIT.",
                connString, buffSize, model->config->interpolationBuffSizeLimit);

        buffSize = model->config->interpolationBuffSizeLimit;
    }

    return buffSize;
}

static size_t MemoryFilterHistorySize(Component * sourceComp, Component * targetComp, int extDegree, const char * connString) {
    size_t size = 0;

    Model * model = sourceComp->GetModel(sourceComp);
    Task * task = model->GetTask(model);

    size_t limit = model->config->memFilterHistoryLimit;

    double syncStep = task->GetTimeStep(task);

    double sourceStep = sourceComp->GetTimeStep(sourceComp) ? sourceComp->GetTimeStep(sourceComp) : syncStep;
    double targetStep = targetComp->GetTimeStep(targetComp) ? targetComp->GetTimeStep(targetComp) : syncStep;

    double syncToSrcRatio = syncStep / sourceStep;
    double syncToTrgRatio = syncStep / targetStep;

    double trgToSyncRatio = targetStep / syncStep;
    double trgToSrcRatio = targetStep / sourceStep;

    double srcToSyncRatio = sourceStep / syncStep;
    double srcToTrgRatio = sourceStep / targetStep;

    double syncToSrc = round(syncToSrcRatio);
    double syncToTrg = round(syncToTrgRatio);

    double trgToSync = round(trgToSyncRatio);
    double trgToSrc = round(trgToSrcRatio);

    double srcToSync = round(srcToSyncRatio);
    double srcToTrg = round(srcToTrgRatio);

    int useInputsAtEndTime = task->useInputsAtEndTime;

    StepTypeType stepType = task->GetStepTypeType(task);

    if (!model->config->useMemFilter) {
        return 0;
    }

    if (ComponentMightNotRespectStepSize(sourceComp) || ComponentMightNotRespectStepSize(targetComp)) {
        return 0;
    }

    if (STEP_TYPE_PARALLEL_MT == stepType) {
        if (useInputsAtEndTime && extDegree == 0) {
            // CASE 1: T = t_a && t_a = t_b
            if (double_eq(syncStep, sourceStep) && double_eq(sourceStep, targetStep)) {
                size = 2;
            }
            // CASE 2: T = t_a && t_a > t_b && t_a = n * t_b
            else if (double_eq(syncStep, sourceStep) && srcToTrgRatio > 1.0 && double_eq(srcToTrgRatio, srcToTrg)) {
                size = 2;
            }
            // CASE 3: T = t_a && t_a > t_b && t_a = m * t_b
            else if (double_eq(syncStep, sourceStep) && srcToTrgRatio > 1.0 && !double_eq(srcToTrgRatio, srcToTrg)) {
                size = 2;
            }
            // CASE 4: T > t_a && T = n * t_a && t_a = t_b
            else if (syncToSrcRatio > 1.0 && double_eq(syncToSrcRatio, syncToSrc) && double_eq(sourceStep, targetStep)) {
                size = (size_t) syncToSrc + 1;
            }
            // CASE 5: T > t_a && T = n * t_a && t_a > t_b && t_a = k * t_b
            else if (syncToSrcRatio > 1.0 && double_eq(syncToSrcRatio, syncToSrc) && srcToTrgRatio > 1.0 && double_eq(srcToTrgRatio, srcToTrg)) {
                size = (size_t) syncToSrc + 1;
            }
            // CASE 6: T > t_a && T = n * t_a && t_a > t_b && t_a = m * t_b
            else if (syncToSrcRatio > 1.0 && double_eq(syncToSrcRatio, syncToSrc) && srcToTrgRatio > 1.0 && !double_eq(srcToTrgRatio, srcToTrg)) {
                size = (size_t) syncToSrc + 1;
            }
            // CASE 7: T > t_a && T = m * t_a && t_a = t_b
            else if (syncToSrcRatio > 1.0 && !double_eq(syncToSrcRatio, syncToSrc) && double_eq(sourceStep, targetStep)) {
                size = (size_t) ceil(syncToSrcRatio) + 1;
            }
            // CASE 10: T < t_a && T = t_b && n * T = t_a && n = 2
            else if (double_eq(syncStep, targetStep) && double_eq(srcToSyncRatio, 2.0)) {
                size = 2;
            }
            // CASE 12: T < t_a && T = t_b && m * T = t_a && m <= 1.5
            else if (double_eq(syncStep, targetStep) && srcToSyncRatio > 1.0 && !double_eq(srcToSyncRatio, srcToSync) && (double_eq(srcToSyncRatio, 1.5) || srcToSyncRatio < 1.5)) {
                size = 2;
            }
            // CASE 14: T < t_a && T < t_b && n * T = t_b && k * T = t_a && k / n = 2
            else if (trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync) && srcToSyncRatio > 1.0 && double_eq(srcToSyncRatio, srcToSync)) {
                double factor = srcToSync / trgToSync;
                if (double_eq(factor, 2.0)) {
                    size = 2;
                }
            }
            // CASE 16: T < t_a && T < t_b && n * T = t_b && k * T = t_a && k / n in (1,2)
            else if (trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync) && srcToSyncRatio > 1.0 && double_eq(srcToSyncRatio, srcToSync)) {
                double factor = srcToSync / trgToSync;
                if (!double_eq(factor, round(factor)) && factor > 1.0 && factor < 2.0) {
                    size = 2;
                }
            }
            // CASE 18: T < t_a && T < t_b && n * T = t_b && p * T = t_a && p / n in (1,2)
            else if (trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync) && srcToSyncRatio > 1.0 && !double_eq(srcToSyncRatio, srcToSync)) {
                double factor = srcToSyncRatio / trgToSync;
                if (!double_eq(factor, round(factor)) && factor > 1.0 && factor < 2.0) {
                    size = 2;
                }
            }
            // CASE 20: T < t_a && T < t_b && m * T = t_b && k * T = t_a && k / m in (1,2)
            else if (trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync) && srcToSyncRatio > 1.0 && double_eq(srcToSyncRatio, srcToSync)) {
                double factor = srcToSync / trgToSyncRatio;
                if (!double_eq(factor, round(factor)) && factor > 1.0 && factor < 2.0) {
                    size = 2;
                }
            }
            // CASE 22: T < t_a && T < t_b && m * T = t_b && k * T = t_a && k / m = 2
            else if (trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync) && srcToSyncRatio > 1.0 && double_eq(srcToSyncRatio, srcToSync)) {
                double factor = srcToSync / trgToSyncRatio;
                if (double_eq(factor, 2.0)) {
                    size = 2;
                }
            }
            // CASE 24: T < t_a && T < t_b && m * T = t_b && p * T = t_a && p / m = 2
            else if (trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync) && srcToSyncRatio > 1.0 && !double_eq(srcToSyncRatio, srcToSync)) {
                double factor = srcToSyncRatio / trgToSyncRatio;
                if (double_eq(factor, 2.0)) {
                    size = 2;
                }
            }
            // CASE 26: T < t_a && T < t_b && m * T = t_b && p * T = t_a && p / m in (1,2)
            else if (trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync) && srcToSyncRatio > 1.0 && !double_eq(srcToSyncRatio, srcToSync)) {
                double factor = srcToSyncRatio / trgToSyncRatio;
                if (!double_eq(factor, round(factor)) && factor > 1.0 && factor < 2.0) {
                    size = 2;
                }
            }
            // CASE 36: T = t_b && t_b > t_a && t_b = n * t_a
            else if (double_eq(syncStep, targetStep) && trgToSrcRatio > 1.0 && double_eq(trgToSrcRatio, trgToSrc)) {
                size = (size_t) trgToSrc + 1;
            }
            // CASE 37: T = t_b && t_b > t_a && t_b = m * t_a
            else if (double_eq(syncStep, targetStep) && trgToSrcRatio > 1.0 && !double_eq(trgToSrcRatio, trgToSrc)) {
                size = (size_t) ceil(trgToSrcRatio) + 1;
            }
            // CASE 38: T > t_b && T = n * t_b && t_b > t_a && t_b = k * t_a
            else if (syncToTrgRatio > 1.0 && double_eq(syncToTrgRatio, syncToTrg) && trgToSrcRatio > 1.0 && double_eq(trgToSrcRatio, trgToSrc)) {
                size = (size_t) syncToTrg * (size_t) trgToSrc + 1;
            }
            // CASE 39: T > t_b && T = n * t_b && t_b > t_a && t_b = m * t_a
            else if (syncToTrgRatio > 1.0 && double_eq(syncToTrgRatio, syncToTrg) && trgToSrcRatio > 1.0 && !double_eq(trgToSrcRatio, trgToSrc)) {
                size = (size_t) syncToTrg * (size_t) ceil(trgToSrcRatio) + 1;
            }
            // CASE 40: T > t_b && t = m * t_b && t_b > t_a && t_b = k * t_a
            else if (syncToTrgRatio > 1.0 && !double_eq(syncToTrgRatio, syncToTrg) && trgToSrcRatio > 1.0 && double_eq(trgToSrcRatio, trgToSrc)) {
                size = (size_t) ceil(syncToTrgRatio) * (size_t) trgToSrc + 1;
            }
            // CASE 41: T > t_b && t = m * t_b && t_b > t_a && t_b = p * t_a
            else if (syncToTrgRatio > 1.0 && !double_eq(syncToTrgRatio, syncToTrg) && trgToSrcRatio > 1.0 && !double_eq(trgToSrcRatio, trgToSrc)) {
                size = (size_t) ceil(syncToTrgRatio) * (size_t) ceil(trgToSrcRatio) + 1;
            }
            // CASE 42: T < t_b && T = t_a && n * T = t_b
            else if (double_eq(syncStep, sourceStep) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                size = 2;
            }
            // CASE 43: T < t_b && T = t_a && m * T = t_b
            else if (double_eq(syncStep, sourceStep) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync)) {
                size = 2;
            }
            // CASE 44: T < t_b && T < t_a &&& n * T = t_a && k * T = t_b && k / n in {2,3,...}
            else if (srcToSyncRatio > 1.0 && double_eq(srcToSyncRatio, srcToSync) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                double factor = trgToSync / srcToSync;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = 2;
                }
            }
            // CASE 45: T < t_b && T < t_a &&& n * T = t_a && k * T = t_b && k / n not in {2,3,...} && k > n
            else if (srcToSyncRatio > 1.0 && double_eq(srcToSyncRatio, srcToSync) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync) && trgToSyncRatio > srcToSyncRatio) {
                double factor = trgToSync / srcToSync;
                if (factor > 1.0 && !double_eq(factor, round(factor))) {
                    size = 2;
                }
            }
            // CASE 46: T < t_b && T < t_a && n * T = t_a && p * T = t_b && p > n
            else if (srcToSyncRatio > 1.0 && double_eq(srcToSyncRatio, srcToSync) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync) && trgToSyncRatio > srcToSyncRatio) {
                size = 2;
            }
            // CASE 47: T < t_b && T < t_a &&& m * T = t_a && k * T = t_b && k / m not in {2,3,...} && k > m
            else if (srcToSyncRatio > 1.0 && !double_eq(srcToSyncRatio, srcToSync) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync) && trgToSyncRatio > srcToSyncRatio) {
                double factor = trgToSync / srcToSyncRatio;
                if (factor > 1.0 && !double_eq(factor, round(factor))) {
                    size = 2;
                }
            }
            // CASE 48: T < t_b && T < t_a &&& m * T = t_a && k * T = t_b && k / m in {2,3,...}
            else if (srcToSyncRatio > 1.0 && !double_eq(srcToSyncRatio, srcToSync) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                double factor = trgToSync / srcToSyncRatio;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = 2;
                }
            }
            // CASE 49: T < t_b && T < t_a &&& m * T = t_a && p * T = t_b && p / m in {2,3,...}
            else if (srcToSyncRatio > 1.0 && !double_eq(srcToSyncRatio, srcToSync) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync)) {
                double factor = trgToSyncRatio / srcToSyncRatio;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = 2;
                }
            }
            // CASE 50: T < t_b && T < t_a &&& m * T = t_a && p * T = t_b && p / m not in {2,3,...} && p > m
            else if (srcToSyncRatio > 1.0 && !double_eq(srcToSyncRatio, srcToSync) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync) && trgToSyncRatio > srcToSyncRatio) {
                double factor = trgToSyncRatio / srcToSyncRatio;
                if (factor > 1.0 && !double_eq(factor, round(factor))) {
                    size = 2;
                }
            }
            // CASE 51: T < t_b && T > t_a && T = n * t_a && k * T = t_b
            else if (syncToSrcRatio > 1.0 && double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                size = (size_t) syncToSrc + 1;
            }
            // CASE 52: T < t_b && T > t_a && T = n * t_a && p * T = t_b && n * p in {2,3,...}
            else if (syncToSrcRatio > 1.0 && double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync)) {
                double factor = syncToSrc * trgToSyncRatio;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = (size_t) syncToSrc + 1;
                }
            }
            // CASE 53: T < t_b && T > t_a && T = n * t_a && p * T = t_b && n * p not in {2,3,...}
            else if (syncToSrcRatio > 1.0 && double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync)) {
                double factor = syncToSrc * trgToSyncRatio;
                if (factor > 1.0 && !double_eq(factor, round(factor))) {
                    size = (size_t) syncToSrc + 1;
                }
            }
            // CASE 54: T < t_b && T > t_a && T = m * t_a && k * T = t_b && k * m not in {2,3,...}
            else if (syncToSrcRatio > 1.0 && !double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                double factor = syncToSrcRatio * trgToSync;
                if (factor > 1.0 && !double_eq(factor, round(factor))) {
                    size = (size_t) ceil(syncToSrcRatio) + 1;
                }
            }
            // CASE 55: T < t_b && T > t_a && T = m * t_a && k * T = t_b && k * m in {2,3,...}
            else if (syncToSrcRatio > 1.0 && !double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                double factor = syncToSrcRatio * trgToSync;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = (size_t) ceil(syncToSrcRatio) + 1;
                }
            }
            // CASE 56: T < t_b && T > t_a && T = m * t_a && p * T = t_b && p * m in {2,3,...}
            else if (syncToSrcRatio > 1.0 && !double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync)) {
                double factor = syncToSrcRatio * trgToSyncRatio;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = (size_t) ceil(syncToSrcRatio) + 1;
                }
            }
            // CASE 57: T < t_b && T > t_a && T = m * t_a && p * T = t_b && p * m < 2
            else if (syncToSrcRatio > 1.0 && !double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync)) {
                double factor = syncToSrcRatio * trgToSyncRatio;
                if (factor < 2.0 && !double_eq(factor, round(factor))) {
                    size = (size_t) ceil(syncToSrcRatio) + 1;
                }
            }
            // CASE 58: T < t_b && T > t_a && T = m * t_a && p * T = t_b && p * m > 2 && p * m not in {3,4,...}
            else if (syncToSrcRatio > 1.0 && !double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync)) {
                double factor = syncToSrcRatio * trgToSyncRatio;
                if (factor > 2.0 && !double_eq(factor, round(factor))) {
                    size = (size_t) ceil(syncToSrcRatio) + 1;
                }
            }
        }
        else if (!useInputsAtEndTime && extDegree == 0) {
            // CASE 1: T = t_a && t_a = t_b
            if (double_eq(syncStep, sourceStep) && double_eq(sourceStep, targetStep)) {
                size = 2;
            }
            // CASE 2: T = t_a && t_a > t_b && t_a = n * t_b
            else if (double_eq(syncStep, sourceStep) && srcToTrgRatio > 1.0 && double_eq(srcToTrgRatio, srcToTrg)) {
                size = 2;
            }
            // CASE 3: T = t_a && t_a > t_b && t_a = m * t_b
            else if (double_eq(syncStep, sourceStep) && srcToTrgRatio > 1.0 && !double_eq(srcToTrgRatio, srcToTrg)) {
                size = 2;
            }
            // CASE 4: T > t_a && T = n * t_a && t_a = t_b
            else if (syncToSrcRatio > 1.0 && double_eq(syncToSrcRatio, syncToSrc) && double_eq(sourceStep, targetStep)) {
                size = (size_t) syncToSrc + 1;
            }
            // CASE 5: T > t_a && T = n * t_a && t_a > t_b && t_a = k * t_b
            else if (syncToSrcRatio > 1.0 && double_eq(syncToSrcRatio, syncToSrc) && srcToTrgRatio > 1.0 && double_eq(srcToTrgRatio, srcToTrg)) {
                size = (size_t) syncToSrc + 1;
            }
            // CASE 6: T > t_a && T = n * t_a && t_a > t_b && t_a = m * t_b
            else if (syncToSrcRatio > 1.0 && double_eq(syncToSrcRatio, syncToSrc) && srcToTrgRatio > 1.0 && !double_eq(srcToTrgRatio, srcToTrg)) {
                size = (size_t) syncToSrc + 1;
            }
            // CASE 7: T > t_a && T = m * t_a && t_a = t_b
            else if (syncToSrcRatio > 1.0 && !double_eq(syncToSrcRatio, syncToSrc) && double_eq(sourceStep, targetStep)) {
                size = (size_t) ceil(syncToSrcRatio) + 1;
            }
            // CASE 12: T < t_a && T = t_b && m * T = t_a && m <= 1.5
            else if (double_eq(syncStep, targetStep) && srcToSyncRatio > 1.0 && !double_eq(srcToSyncRatio, srcToSync) && (double_eq(srcToSyncRatio, 1.5) || srcToSyncRatio < 1.5)) {
                size = 2;
            }
            // CASE 36: T = t_b && t_b > t_a && t_b = n * t_a
            else if (double_eq(syncStep, targetStep) && trgToSrcRatio > 1.0 && double_eq(trgToSrcRatio, trgToSrc)) {
                size = (size_t) trgToSrc + 1;
            }
            // CASE 38: T > t_b && T = n * t_b && t_b > t_a && t_b = k * t_a
            else if (syncToTrgRatio > 1.0 && double_eq(syncToTrgRatio, syncToTrg) && trgToSrcRatio > 1.0 && double_eq(trgToSrcRatio, trgToSrc)) {
                size = (size_t) syncToTrg * (size_t) trgToSrc + 1;
            }
            // CASE 40: T > t_b && t = m * t_b && t_b > t_a && t_b = k * t_a
            else if (syncToTrgRatio > 1.0 && !double_eq(syncToTrgRatio, syncToTrg) && trgToSrcRatio > 1.0 && double_eq(trgToSrcRatio, trgToSrc)) {
                size = (size_t) ceil(syncToTrgRatio) * (size_t) trgToSrc + 1;
            }
            // CASE 42: T < t_b && T = t_a && n * T = t_b
            else if (double_eq(syncStep, sourceStep) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                size = 2;
            }
            // CASE 43: T < t_b && T = t_a && m * T = t_b
            else if (double_eq(syncStep, sourceStep) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync)) {
                size = 2;
            }
            // CASE 44: T < t_b && T < t_a &&& n * T = t_a && k * T = t_b && k / n in {2,3,...}
            else if (srcToSyncRatio > 1.0 && double_eq(srcToSyncRatio, srcToSync) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                double factor = trgToSync / srcToSync;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = 2;
                }
            }
            // CASE 48: T < t_b && T < t_a &&& m * T = t_a && k * T = t_b && k / m in {2,3,...}
            else if (srcToSyncRatio > 1.0 && !double_eq(srcToSyncRatio, srcToSync) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                double factor = trgToSync / srcToSyncRatio;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = 2;
                }
            }
            // CASE 49: T < t_b && T < t_a &&& m * T = t_a && p * T = t_b && p / m in {2,3,...}
            else if (srcToSyncRatio > 1.0 && !double_eq(srcToSyncRatio, srcToSync) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync)) {
                double factor = trgToSyncRatio / srcToSyncRatio;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = 2;
                }
            }
            // CASE 51: T < t_b && T > t_a && T = n * t_a && k * T = t_b
            else if (syncToSrcRatio > 1.0 && double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                size = (size_t) syncToSrc + 1;
            }
            // CASE 52: T < t_b && T > t_a && T = n * t_a && p * T = t_b && n * p in {2,3,...}
            else if (syncToSrcRatio > 1.0 && double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync)) {
                double factor = syncToSrc * trgToSyncRatio;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = (size_t) syncToSrc + 1;
                }
            }
            // CASE 53: T < t_b && T > t_a && T = n * t_a && p * T = t_b && n * p not in {2,3,...}
            else if (syncToSrcRatio > 1.0 && double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync)) {
                double factor = syncToSrc * trgToSyncRatio;
                if (factor > 1.0 && !double_eq(factor, round(factor))) {
                    size = (size_t) syncToSrc + 1;
                }
            }
            // CASE 55: T < t_b && T > t_a && T = m * t_a && k * T = t_b && k * m in {2,3,...}
            else if (syncToSrcRatio > 1.0 && !double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                double factor = syncToSrcRatio * trgToSync;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = (size_t) ceil(syncToSrcRatio) + 1;
                }
            }
            // CASE 56: T < t_b && T > t_a && T = m * t_a && p * T = t_b && p * m in {2,3,...}
            else if (syncToSrcRatio > 1.0 && !double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync)) {
                double factor = syncToSrcRatio * trgToSyncRatio;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = (size_t) ceil(syncToSrcRatio) + 1;
                }
            }
        }
        else if (useInputsAtEndTime) {
            // not applicable
        }
        else if (!useInputsAtEndTime) {
            // CASE 1: T = t_a && t_a = t_b
            if (double_eq(syncStep, sourceStep) && double_eq(sourceStep, targetStep)) {
                size = 2;
            }
            // CASE 36: T = t_b && t_b > t_a && t_b = n * t_a
            else if (double_eq(syncStep, targetStep) && trgToSrcRatio > 1.0 && double_eq(trgToSrcRatio, trgToSrc)) {
                size = (size_t) trgToSrc + 1;
            }
            // CASE 42: T < t_b && T = t_a && n * T = t_b
            else if (double_eq(syncStep, sourceStep) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                size = 2;
            }
            // CASE 44: T < t_b && T < t_a &&& n * T = t_a && k * T = t_b && k / n in {2,3,...}
            else if (srcToSyncRatio > 1.0 && double_eq(srcToSyncRatio, srcToSync) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                double factor = trgToSync / srcToSync;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = 2;
                }
            }
            // CASE 48: T < t_b && T < t_a &&& m * T = t_a && k * T = t_b && k / m in {2,3,...}
            else if (srcToSyncRatio > 1.0 && !double_eq(srcToSyncRatio, srcToSync) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                double factor = trgToSync / srcToSyncRatio;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = 2;
                }
            }
            // CASE 49: T < t_b && T < t_a &&& m * T = t_a && p * T = t_b && p / m in {2,3,...}
            else if (srcToSyncRatio > 1.0 && !double_eq(srcToSyncRatio, srcToSync) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync)) {
                double factor = trgToSyncRatio / srcToSyncRatio;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = 2;
                }
            }
            // CASE 51: T < t_b && T > t_a && T = n * t_a && k * T = t_b
            else if (syncToSrcRatio > 1.0 && double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                size = (size_t) syncToSrc + 1;
            }
            // CASE 55: T < t_b && T > t_a && T = m * t_a && k * T = t_b && k * m in {2,3,...}
            else if (syncToSrcRatio > 1.0 && !double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                double factor = syncToSrcRatio * trgToSync;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = (size_t) ceil(syncToSrcRatio) + 1;
                }
            }
        }
    } else if (STEP_TYPE_SEQUENTIAL == stepType) {
        if (useInputsAtEndTime && extDegree == 0) {
            // CASE 1: T = t_a && t_a = t_b
            if (double_eq(syncStep, sourceStep) && double_eq(sourceStep, targetStep)) {
                size = 2;
            }
            // CASE 4: T > t_a && T = n * t_a && t_a = t_b
            else if (syncToSrcRatio > 1.0 && double_eq(syncToSrcRatio, syncToSrc) && double_eq(sourceStep, targetStep)) {
                size = (size_t) syncToSrc + 1;
            }
            // CASE 7: T > t_a && T = m * t_a && t_a = t_b
            else if (syncToSrcRatio > 1.0 && !double_eq(syncToSrcRatio, syncToSrc) && double_eq(sourceStep, targetStep)) {
                size = (size_t) ceil(syncToSrcRatio) + 1;
            }
            // CASE 36: T = t_b && t_b > t_a && t_b = n * t_a
            else if (double_eq(syncStep, targetStep) && trgToSrcRatio > 1.0 && double_eq(trgToSrcRatio, trgToSrc)) {
                size = (size_t) trgToSrc + 1;
            }
            // CASE 38: T > t_b && T = n * t_b && t_b > t_a && t_b = k * t_a
            else if (syncToTrgRatio > 1.0 && double_eq(syncToTrgRatio, syncToTrg) && trgToSrcRatio > 1.0 && double_eq(trgToSrcRatio, trgToSrc)) {
                size = (size_t) syncToTrg * (size_t) trgToSrc + 1;
            }
            // CASE 40: T > t_b && t = m * t_b && t_b > t_a && t_b = k * t_a
            else if (syncToTrgRatio > 1.0 && !double_eq(syncToTrgRatio, syncToTrg) && trgToSrcRatio > 1.0 && double_eq(trgToSrcRatio, trgToSrc)) {
                size = (size_t) ceil(syncToTrgRatio) * (size_t) trgToSrc + 1;
            }
            // CASE 41: T > t_b && t = m * t_b && t_b > t_a && t_b = p * t_a
            else if (syncToTrgRatio > 1.0 && !double_eq(syncToTrgRatio, syncToTrg) && trgToSrcRatio > 1.0 && !double_eq(trgToSrcRatio, trgToSrc)) {
                size = (size_t) ceil(syncToTrgRatio) * (size_t) ceil(trgToSrcRatio) + 1;
            }
            // CASE 42: T < t_b && T = t_a && n * T = t_b
            else if (double_eq(syncStep, sourceStep) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                size = 2;
            }
            // CASE 43: T < t_b && T = t_a && m * T = t_b
            else if (double_eq(syncStep, sourceStep) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync)) {
                size = 2;
            }
            // CASE 44: T < t_b && T < t_a &&& n * T = t_a && k * T = t_b && k / n in {2,3,...}
            else if (srcToSyncRatio > 1.0 && double_eq(srcToSyncRatio, srcToSync) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                double factor = trgToSync / srcToSync;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = 2;
                }
            }
            // CASE 45: T < t_b && T < t_a &&& n * T = t_a && k * T = t_b && k / n not in {2,3,...} && k > n
            else if (srcToSyncRatio > 1.0 && double_eq(srcToSyncRatio, srcToSync) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync) && trgToSyncRatio > srcToSyncRatio) {
                double factor = trgToSync / srcToSync;
                if (factor > 1.0 && !double_eq(factor, round(factor))) {
                    size = 2;
                }
            }
            // CASE 46: T < t_b && T < t_a && n * T = t_a && p * T = t_b && p > n
            else if (srcToSyncRatio > 1.0 && double_eq(srcToSyncRatio, srcToSync) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync) && trgToSyncRatio > srcToSyncRatio) {
                size = 2;
            }
            // CASE 47: T < t_b && T < t_a &&& m * T = t_a && k * T = t_b && k / m not in {2,3,...} && k > m
            else if (srcToSyncRatio > 1.0 && !double_eq(srcToSyncRatio, srcToSync) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync) && trgToSyncRatio > srcToSyncRatio) {
                double factor = trgToSync / srcToSyncRatio;
                if (factor > 1.0 && !double_eq(factor, round(factor))) {
                    size = 2;
                }
            }
            // CASE 48: T < t_b && T < t_a &&& m * T = t_a && k * T = t_b && k / m in {2,3,...}
            else if (srcToSyncRatio > 1.0 && !double_eq(srcToSyncRatio, srcToSync) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                double factor = trgToSync / srcToSyncRatio;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = 2;
                }
            }
            // CASE 49: T < t_b && T < t_a &&& m * T = t_a && p * T = t_b && p / m in {2,3,...}
            else if (srcToSyncRatio > 1.0 && !double_eq(srcToSyncRatio, srcToSync) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync)) {
                double factor = trgToSyncRatio / srcToSyncRatio;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = 2;
                }
            }
            // CASE 50: T < t_b && T < t_a &&& m * T = t_a && p * T = t_b && p / m not in {2,3,...} && p > m
            else if (srcToSyncRatio > 1.0 && !double_eq(srcToSyncRatio, srcToSync) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync) && trgToSyncRatio > srcToSyncRatio) {
                double factor = trgToSyncRatio / srcToSyncRatio;
                if (factor > 1.0 && !double_eq(factor, round(factor))) {
                    size = 2;
                }
            }
            // CASE 51: T < t_b && T > t_a && T = n * t_a && k * T = t_b
            else if (syncToSrcRatio > 1.0 && double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                size = (size_t)syncToSrc + 1;
            }
            // CASE 52: T < t_b && T > t_a && T = n * t_a && p * T = t_b && n * p in {2,3,...}
            else if (syncToSrcRatio > 1.0 && double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync)) {
                double factor = syncToSrc * trgToSyncRatio;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = (size_t) syncToSrc + 1;
                }
            }
            // CASE 53: T < t_b && T > t_a && T = n * t_a && p * T = t_b && n * p not in {2,3,...}
            else if (syncToSrcRatio > 1.0 && double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync)) {
                double factor = syncToSrc * trgToSyncRatio;
                if (factor > 1.0 && !double_eq(factor, round(factor))) {
                    size = (size_t) syncToSrc + 1;
                }
            }
            // CASE 54: T < t_b && T > t_a && T = m * t_a && k * T = t_b && k * m not in {2,3,...}
            else if (syncToSrcRatio > 1.0 && !double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                double factor = syncToSrcRatio * trgToSync;
                if (factor > 1.0 && !double_eq(factor, round(factor))) {
                    size = (size_t) ceil(syncToSrcRatio) + 1;
                }
            }
            // CASE 55: T < t_b && T > t_a && T = m * t_a && k * T = t_b && k * m in {2,3,...}
            else if (syncToSrcRatio > 1.0 && !double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                double factor = syncToSrcRatio * trgToSync;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = (size_t) ceil(syncToSrcRatio) + 1;
                }
            }
            // CASE 56: T < t_b && T > t_a && T = m * t_a && p * T = t_b && p * m in {2,3,...}
            else if (syncToSrcRatio > 1.0 && !double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync)) {
                double factor = syncToSrcRatio * trgToSyncRatio;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = (size_t) ceil(syncToSrcRatio) + 1;
                }
            }
            // CASE 58: T < t_b && T > t_a && T = m * t_a && p * T = t_b && p * m > 2 && p * m not in {3,4,...}
            else if (syncToSrcRatio > 1.0 && !double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync)) {
                double factor = syncToSrcRatio * trgToSyncRatio;
                if (factor > 2.0 && !double_eq(factor, round(factor))) {
                    size = (size_t) ceil(syncToSrcRatio) + 1;
                }
            }
        }
        else if (!useInputsAtEndTime && extDegree == 0) {
            // CASE 1: T = t_a && t_a = t_b
            if (double_eq(syncStep, sourceStep) && double_eq(sourceStep, targetStep)) {
                size = 2;
            }
            // CASE 4: T > t_a && T = n * t_a && t_a = t_b
            else if (syncToSrcRatio > 1.0 && double_eq(syncToSrcRatio, syncToSrc) && double_eq(sourceStep, targetStep)) {
                size = (size_t) syncToSrc + 1;
            }
            // CASE 7: T > t_a && T = m * t_a && t_a = t_b
            else if (syncToSrcRatio > 1.0 && !double_eq(syncToSrcRatio, syncToSrc) && double_eq(sourceStep, targetStep)) {
                size = (size_t) ceil(syncToSrcRatio) + 1;
            }
            // CASE 36: T = t_b && t_b > t_a && t_b = n * t_a
            else if (double_eq(syncStep, targetStep) && trgToSrcRatio > 1.0 && double_eq(trgToSrcRatio, trgToSrc)) {
                size = (size_t) trgToSrc + 1;
            }
            // CASE 38: T > t_b && T = n * t_b && t_b > t_a && t_b = k * t_a
            else if (syncToTrgRatio > 1.0 && double_eq(syncToTrgRatio, syncToTrg) && trgToSrcRatio > 1.0 && double_eq(trgToSrcRatio, trgToSrc)) {
                size = (size_t)syncToTrg * (size_t)trgToSrc + 1;
            }
            // CASE 40: T > t_b && t = m * t_b && t_b > t_a && t_b = k * t_a
            else if (syncToTrgRatio > 1.0 && !double_eq(syncToTrgRatio, syncToTrg) && trgToSrcRatio > 1.0 && double_eq(trgToSrcRatio, trgToSrc)) {
                size = (size_t) ceil(syncToTrgRatio) * (size_t) trgToSrc + 1;
            }
            // CASE 41: T > t_b && t = m * t_b && t_b > t_a && t_b = p * t_a
            else if (syncToTrgRatio > 1.0 && !double_eq(syncToTrgRatio, syncToTrg) && trgToSrcRatio > 1.0 && !double_eq(trgToSrcRatio, trgToSrc)) {
                size = (size_t) ceil(syncToTrgRatio) * (size_t) ceil(trgToSrcRatio) + 1;
            }
            // CASE 42: T < t_b && T = t_a && n * T = t_b
            else if (double_eq(syncStep, sourceStep) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                size = 2;
            }
            // CASE 44: T < t_b && T < t_a &&& n * T = t_a && k * T = t_b && k / n in {2,3,...}
            else if (srcToSyncRatio > 1.0 && double_eq(srcToSyncRatio, srcToSync) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                double factor = trgToSync / srcToSync;
                if (factor > 1 && double_eq(factor, round(factor))) {
                    size = 2;
                }
            }
            // CASE 48: T < t_b && T < t_a &&& m * T = t_a && k * T = t_b && k / m in {2,3,...}
            else if (srcToSyncRatio > 1.0 && !double_eq(srcToSyncRatio, srcToSync) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                double factor = trgToSync / srcToSyncRatio;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = 2;
                }
            }
            // CASE 49: T < t_b && T < t_a &&& m * T = t_a && p * T = t_b && p / m in {2,3,...}
            else if (srcToSyncRatio > 1.0 && !double_eq(srcToSyncRatio, srcToSync) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync)) {
                double factor = trgToSyncRatio / srcToSyncRatio;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = 2;
                }
            }
            // CASE 51: T < t_b && T > t_a && T = n * t_a && k * T = t_b
            else if (syncToSrcRatio > 1.0 && double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                size = (size_t)syncToSrc + 1;
            }
            // CASE 52: T < t_b && T > t_a && T = n * t_a && p * T = t_b && n * p in {2,3,...}
            else if (syncToSrcRatio > 1.0 && double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync)) {
                double factor = syncToSrc * trgToSyncRatio;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = (size_t) syncToSrc + 1;
                }
            }
            // CASE 55: T < t_b && T > t_a && T = m * t_a && k * T = t_b && k * m in {2,3,...}
            else if (syncToSrcRatio > 1.0 && !double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                double factor = syncToSrcRatio * trgToSync;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = (size_t) ceil(syncToSrcRatio) + 1;
                }
            }
            // CASE 56: T < t_b && T > t_a && T = m * t_a && p * T = t_b && p * m in {2,3,...}
            else if (syncToSrcRatio > 1.0 && !double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync)) {
                double factor = syncToSrcRatio * trgToSyncRatio;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = (size_t) ceil(syncToSrcRatio) + 1;
                }
            }
        }
        else if (useInputsAtEndTime) {
            // CASE 1: T = t_a && t_a = t_b
            if (double_eq(syncStep, sourceStep) && double_eq(sourceStep, targetStep)) {
                size = 2;
            }
            // CASE 4: T > t_a && T = n * t_a && t_a = t_b
            else if (syncToSrcRatio > 1.0 && double_eq(syncToSrcRatio, syncToSrc) && double_eq(sourceStep, targetStep)) {
                size = (size_t) syncToSrc + 1;
            }
            // CASE 7: T > t_a && T = m * t_a && t_a = t_b
            else if (syncToSrcRatio > 1.0 && !double_eq(syncToSrcRatio, syncToSrc) && double_eq(sourceStep, targetStep)) {
                size = (size_t) ceil(syncToSrcRatio) + 1;
            }
            // CASE 36: T = t_b && t_b > t_a && t_b = n * t_a
            else if (double_eq(syncStep, targetStep) && trgToSrcRatio > 1.0 && double_eq(trgToSrcRatio, trgToSrc)) {
                size = (size_t) trgToSrc + 1;
            }
            // CASE 38: T > t_b && T = n * t_b && t_b > t_a && t_b = k * t_a
            else if (syncToTrgRatio > 1.0 && double_eq(syncToTrgRatio, syncToTrg) && trgToSrcRatio > 1.0 && double_eq(trgToSrcRatio, trgToSrc)) {
                size = (size_t) syncToTrg * (size_t) trgToSrc + 1;
            }
            // CASE 42: T < t_b && T = t_a && n * T = t_b
            else if (double_eq(syncStep, sourceStep) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                size = 2;
            }
        }
        else if (!useInputsAtEndTime) {
            // CASE 1: T = t_a && t_a = t_b
            if (double_eq(syncStep, sourceStep) && double_eq(sourceStep, targetStep)) {
                size = 2;
            }
            // CASE 4: T > t_a && T = n * t_a && t_a = t_b
            else if (syncToSrcRatio > 1.0 && double_eq(syncToSrcRatio, syncToSrc) && double_eq(sourceStep, targetStep)) {
                size = (size_t) syncToSrc + 1;
            }
            // CASE 7: T > t_a && T = m * t_a && t_a = t_b
            else if (syncToSrcRatio > 1.0 && !double_eq(syncToSrcRatio, syncToSrc) && double_eq(sourceStep, targetStep)) {
                size = (size_t) ceil(syncToSrcRatio) + 1;
            }
            // CASE 36: T = t_b && t_b > t_a && t_b = n * t_a
            else if (double_eq(syncStep, targetStep) && trgToSrcRatio > 1.0 && double_eq(trgToSrcRatio, trgToSrc)) {
                size = (size_t) trgToSrc + 1;
            }
            // CASE 38: T > t_b && T = n * t_b && t_b > t_a && t_b = k * t_a
            else if (syncToTrgRatio > 1.0 && double_eq(syncToTrgRatio, syncToTrg) && trgToSrcRatio > 1.0 && double_eq(trgToSrcRatio, trgToSrc)) {
                size = (size_t) syncToTrg * (size_t) trgToSrc + 1;
            }
            // CASE 40: T > t_b && t = m * t_b && t_b > t_a && t_b = k * t_a
            else if (syncToTrgRatio > 1.0 && !double_eq(syncToTrgRatio, syncToTrg) && trgToSrcRatio > 1.0 && double_eq(trgToSrcRatio, trgToSrc)) {
                size = (size_t) ceil(syncToTrgRatio) * (size_t) trgToSrc + 1;
            }
            // CASE 41: T > t_b && t = m * t_b && t_b > t_a && t_b = p * t_a
            else if (syncToTrgRatio > 1.0 && !double_eq(syncToTrgRatio, syncToTrg) && trgToSrcRatio > 1.0 && !double_eq(trgToSrcRatio, trgToSrc)) {
                size = (size_t) ceil(syncToTrgRatio) * (size_t) ceil(trgToSrcRatio) + 1;
            }
            // CASE 42: T < t_b && T = t_a && n * T = t_b
            else if (double_eq(syncStep, sourceStep) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                size = 2;
            }
            // CASE 44: T < t_b && T < t_a &&& n * T = t_a && k * T = t_b && k / n in {2,3,...}
            else if (srcToSyncRatio > 1.0 && double_eq(srcToSyncRatio, srcToSync) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                double factor = trgToSync / srcToSync;
                if (factor > 1 && double_eq(factor, round(factor))) {
                    size = 2;
                }
            }
            // CASE 48: T < t_b && T < t_a &&& m * T = t_a && k * T = t_b && k / m in {2,3,...}
            else if (srcToSyncRatio > 1.0 && !double_eq(srcToSyncRatio, srcToSync) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                double factor = trgToSync / srcToSyncRatio;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = 2;
                }
            }
            // CASE 49: T < t_b && T < t_a &&& m * T = t_a && p * T = t_b && p / m in {2,3,...}
            else if (srcToSyncRatio > 1.0 && !double_eq(srcToSyncRatio, srcToSync) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync)) {
                double factor = trgToSyncRatio / srcToSyncRatio;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = 2;
                }
            }
            // CASE 51: T < t_b && T > t_a && T = n * t_a && k * T = t_b
            else if (syncToSrcRatio > 1.0 && double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                size = (size_t)syncToSrc + 1;
            }
            // CASE 52: T < t_b && T > t_a && T = n * t_a && p * T = t_b && n * p in {2,3,...}
            else if (syncToSrcRatio > 1.0 && double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync)) {
                double factor = syncToSrc * trgToSyncRatio;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = (size_t) syncToSrc + 1;
                }
            }
            // CASE 55: T < t_b && T > t_a && T = m * t_a && k * T = t_b && k * m in {2,3,...}
            else if (syncToSrcRatio > 1.0 && !double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && double_eq(trgToSyncRatio, trgToSync)) {
                double factor = syncToSrcRatio * trgToSync;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = (size_t) ceil(syncToSrcRatio) + 1;
                }
            }
            // CASE 56: T < t_b && T > t_a && T = m * t_a && p * T = t_b && p * m in {2,3,...}
            else if (syncToSrcRatio > 1.0 && !double_eq(syncToSrcRatio, syncToSrc) && trgToSyncRatio > 1.0 && !double_eq(trgToSyncRatio, trgToSync)) {
                double factor = syncToSrcRatio * trgToSyncRatio;
                if (factor > 1.0 && double_eq(factor, round(factor))) {
                    size = (size_t) ceil(syncToSrcRatio) + 1;
                }
            }
        }
    }

    if (size == 0) {
        return 0;
    }

    if (size + model->config->memFilterHistoryExtra > limit) {
        mcx_log(LOG_WARNING, "%s: history size limit exceeded (%zu > &zu). Limit can be changed via MC_MEM_FILTER_HISTORY_LIMIT. "
                             "Disabling memory filter",
                connString, size + model->config->memFilterHistoryExtra, limit);

        return 0;
    }

    return size + model->config->memFilterHistoryExtra;
}

static MemoryFilter * SetMemoryFilter(int reverseSearch, ChannelType * sourceType, size_t historySize) {
    McxStatus retVal = RETURN_OK;

    MemoryFilter * filter = (MemoryFilter *)object_create(MemoryFilter);
    if (!filter) {
        mcx_log(LOG_ERROR, "Memory filter creation failed");
        return NULL;
    }

    mcx_log(LOG_DEBUG, "    Setting up memory filter. (%p)", filter);
    mcx_log(LOG_DEBUG, "    History size: %zu", historySize);

    retVal = filter->Setup(filter, sourceType, historySize, reverseSearch);
    if (RETURN_ERROR == retVal) {
        mcx_log(LOG_ERROR, "Memory filter setup failed");
        object_destroy(filter);
        return NULL;
    }

    return filter;
}

ChannelFilter * FilterFactory(ConnectionState * state,
                              InterExtrapolationType extrapolation_type,
                              InterExtrapolationParams * extrapolation_params,
                              ChannelType * channel_type,
                              InterExtrapolatingType inter_extrapolating_type,
                              int is_decoupled,
                              Component * sourceComp,
                              Component * targetComp,
                              const char * connString) {
    ChannelFilter * filter = NULL;
    McxStatus retVal;

    Model * model = sourceComp->GetModel(sourceComp);
    Task * task = model->GetTask(model);
    int useInputsAtEndTime = task->useInputsAtEndTime;

    if (ChannelTypeEq(channel_type, &ChannelTypeDouble)) {
        if (!(INTERVAL_COUPLING == extrapolation_params->interpolationInterval &&
              INTERVAL_SYNCHRONIZATION == extrapolation_params->extrapolationInterval))
        {
            mcx_log(LOG_WARNING, "The use of inter/extrapolation interval settings for double is not supported");
        }
        if (extrapolation_type == INTEREXTRAPOLATION_POLYNOMIAL) {

            if (INTERPOLATING == inter_extrapolating_type && is_decoupled) {
                inter_extrapolating_type = INTEREXTRAPOLATING;
            }

            int degree = (INTERPOLATING == inter_extrapolating_type) ? extrapolation_params->interpolationOrder :
                                                                       extrapolation_params->extrapolationOrder;

            if (EXTRAPOLATING == inter_extrapolating_type || INTEREXTRAPOLATING == inter_extrapolating_type) {
                    size_t memFilterHist = MemoryFilterHistorySize(sourceComp, targetComp, extrapolation_params->extrapolationOrder, connString);
                    if (0 != memFilterHist) {
                        filter = (ChannelFilter *) SetMemoryFilter(useInputsAtEndTime, channel_type, memFilterHist);
                        if (!filter) {
                            return NULL;
                        }
                    } else if (INTEREXTRAPOLATING == inter_extrapolating_type) {
                        IntExtFilter * intExtFilter = (IntExtFilter *)object_create(IntExtFilter);
                        filter = (ChannelFilter *)intExtFilter;
                        mcx_log(LOG_DEBUG, "    Setting up dynamic filter. (%p)", filter);
                        mcx_log(LOG_DEBUG, "    Interpolation order: %d, extrapolation order: %d", extrapolation_params->interpolationOrder, extrapolation_params->extrapolationOrder);
                        size_t buffSize = DetermineFilterBufferSize(sourceComp, targetComp, connString);
                        retVal = intExtFilter->Setup(intExtFilter, extrapolation_params->extrapolationOrder, extrapolation_params->interpolationOrder, buffSize);
                        if (RETURN_OK != retVal) {
                            return NULL;
                        }
                    } else {
                        ExtFilter * extFilter = (ExtFilter *)object_create(ExtFilter);
                        filter = (ChannelFilter *)extFilter;
                        mcx_log(LOG_DEBUG, "    Setting up synchronization step extrapolation filter. (%p)", filter);
                        mcx_log(LOG_DEBUG, "    Extrapolation order: %d", degree);
                        retVal = extFilter->Setup(extFilter, degree);
                        if (RETURN_OK != retVal) {
                            return NULL;
                        }
                    }
            } else {
                size_t memFilterHist = MemoryFilterHistorySize(sourceComp, targetComp, degree, connString);
                if (0 != memFilterHist) {
                    filter = (ChannelFilter *) SetMemoryFilter(useInputsAtEndTime, channel_type, memFilterHist);
                    if (!filter) {
                        return NULL;
                    }
                } else {
                    IntFilter* intFilter = (IntFilter*)object_create(IntFilter);
                    filter = (ChannelFilter*)intFilter;
                    mcx_log(LOG_DEBUG, "    Setting up coupling step interpolation filter. (%p)", filter);
                    mcx_log(LOG_DEBUG, "    Interpolation order: %d", degree);
                    size_t buffSize = DetermineFilterBufferSize(sourceComp, targetComp, connString);
                    retVal = intFilter->Setup(intFilter, degree, buffSize);
                    if (RETURN_OK != retVal) {
                        mcx_log(LOG_ERROR, "Connection: Filter: Could not setup");
                        return NULL;
                    }
                }
            }

            if (!filter) {
                mcx_log(LOG_ERROR, "Connection: Filter: Filter creation failed");
                return NULL;
            }
        }
    } else {
        DiscreteFilter * discreteFilter = NULL;

        if (!(0 == extrapolation_params->extrapolationOrder &&
              0 == extrapolation_params->interpolationOrder &&
              INTERVAL_COUPLING == extrapolation_params->interpolationInterval &&
              INTERVAL_SYNCHRONIZATION == extrapolation_params->extrapolationInterval
        )) {
            mcx_log(LOG_WARNING, "Invalid inter/extrapolation settings for non-double connection detected");
        }
        mcx_log(LOG_DEBUG, "Using constant synchronization step extrapolation for non-double connection");

        discreteFilter = (DiscreteFilter *) object_create(DiscreteFilter);
        discreteFilter->Setup(discreteFilter, channel_type);


        filter = (ChannelFilter *) discreteFilter;
    }

    if (NULL == filter && ChannelTypeEq(channel_type, &ChannelTypeDouble)) {
        // TODO: add a check to avoid filters for non-multirate cases

        size_t memFilterHist = MemoryFilterHistorySize(sourceComp, targetComp, 0, connString);
        if (0 != memFilterHist) {
            filter = (ChannelFilter *) SetMemoryFilter(useInputsAtEndTime, channel_type, memFilterHist);
            if (!filter) {
                return NULL;
            }
        } else {
            ExtFilter * extFilter = (ExtFilter *) object_create(ExtFilter);
            extFilter->Setup(extFilter, 0);
            filter = (ChannelFilter *) extFilter;
        }
    }

    filter->AssignState(filter, state);

    return filter;
}

static void * ConnectionGetValueReference(Connection * connection) {
    return (void *)connection->value_;
}

static void ConnectionSetValueReference(Connection * connection, void * reference) {
    connection->value_ = reference;
}

static ChannelDimension * ConnectionGetValueDimension(Connection * connection) {
    return NULL;
}

static ChannelType * ConnectionGetValueType(Connection * connection) {
    ChannelOut * out = connection->out_;
    Channel * channel = (Channel *) out;
    ChannelInfo * channelInfo = &channel->info;

    return channelInfo->type;
}


static void ConnectionDestructor(Connection * connection) {
    ChannelValueDestructor(&connection->store_);
}

static ChannelOut * ConnectionGetSource(Connection * connection) {
    return connection->out_;
}

static ChannelIn  * ConnectionGetTarget(Connection * connection) {
    return connection->in_;
}

static ConnectionInfo * ConnectionGetInfo(Connection * connection) {
    return &connection->info;
}

static int ConnectionIsDecoupled(Connection * connection) {
    return ConnectionInfoIsDecoupled(&connection->info);
}

static int ConnectionIsDefinedDuringInit(Connection * connection) {
    Channel * channel = (Channel *) connection->GetTarget(connection);
    return channel->IsDefinedDuringInit(channel);
}

static void ConnectionSetDefinedDuringInit(Connection * connection) {
    Channel * channel = (Channel *) connection->GetTarget(connection);
    channel->SetDefinedDuringInit(channel);
}


static int ConnectionIsActiveDependency(Connection * conn) {
    return conn->isActiveDependency_;
}

static void ConnectionSetActiveDependency(Connection * conn, int active) {
    conn->isActiveDependency_ = active;
}

static void ConnectionUpdateFromInput(Connection * connection, TimeInterval * time) {
}

static McxStatus ConnectionUpdateInitialValue(Connection * connection) {
    ConnectionInfo * info = connection->GetInfo(connection);

    Channel * in = (Channel *) connection->in_;
    Channel * out = (Channel *) connection->out_;

    ChannelInfo * inInfo = &in->info;
    ChannelInfo * outInfo = &out->info;

    if (connection->state_ != InInitializationMode) {
        char * buffer = ConnectionInfoConnectionString(info);
        mcx_log(LOG_ERROR, "Connection %s: Update initial value: Cannot update initial value outside of initialization mode", buffer);
        mcx_free(buffer);
        return RETURN_ERROR;
    }

    if (!out || !in) {
        char * buffer = ConnectionInfoConnectionString(info);
        mcx_log(LOG_ERROR, "Connection %s: Update initial value: Cannot update initial value for unconnected connection", buffer);
        mcx_free(buffer);
        return RETURN_ERROR;
    }

    if (inInfo->initialValue) {
        McxStatus retVal = RETURN_OK;
        ChannelValue * store = &connection->store_;
        ChannelValue * inChannelValue = inInfo->initialValue;
        ChannelValue * inValue = ChannelValueClone(inChannelValue);

        if (NULL == inValue) {
            mcx_log(LOG_ERROR, "Could not clone initial value for initial connection");
            return RETURN_ERROR;
        }

        // The type of the stored value of a connection is the type of the out channel.
        // If the value is taken from the in channel, the value must be converted.
        // TODO: It might be a better idea to use the type of the in channel as type of the connection.
        // Such a change might be more complex to implement.
        if (!ChannelTypeEq(inValue->type, store->type)) {
            TypeConversion * typeConv = (TypeConversion *) object_create(TypeConversion);
            Conversion * conv = (Conversion *) typeConv;
            retVal = typeConv->Setup(typeConv, inValue->type, store->type);
            if (RETURN_ERROR == retVal) {
                mcx_log(LOG_ERROR, "Could not set up initial type conversion");
                object_destroy(typeConv);
                mcx_free(inValue);
                return RETURN_ERROR;
            }
            retVal = conv->convert(conv, inValue);
            object_destroy(typeConv);

            if (RETURN_ERROR == retVal) {
                mcx_log(LOG_ERROR, "Could not convert type of initial value");
                mcx_free(inValue);
                return RETURN_ERROR;
            }
        }

        retVal = ChannelValueSet(store, inValue);
        if (RETURN_ERROR == retVal) {
            mcx_log(LOG_ERROR, "Could not set up initial value in connection");
            mcx_free(inValue);
            return RETURN_ERROR;
        }
        mcx_free(inValue);

        connection->useInitialValue_ = TRUE;
    } else if (outInfo->initialValue) {
        ChannelValueSet(&connection->store_, outInfo->initialValue);
        connection->useInitialValue_ = TRUE;
    } else {
        {
            char * buffer = ConnectionInfoConnectionString(info);
            mcx_log(LOG_WARNING, "Connection %s: No initial values are specified for the ports of the connection", buffer);
            mcx_free(buffer);
        }
        ChannelValueInit(&connection->store_, ChannelTypeClone(ConnectionInfoGetType(info)));
    }

    return RETURN_OK;
}

static void ConnectionInitUpdateFrom(Connection * connection, TimeInterval * time) {
#ifdef MCX_DEBUG
    if (time->startTime < MCX_DEBUG_LOG_TIME) {
        Channel * channel = (Channel *) connection->out_;
        ChannelInfo * info = &channel->info;
        MCX_DEBUG_LOG("[%f] CONN   (%s) UpdateFromInput", time->startTime, ChannelInfoGetName(info));
    }
#endif
    // Do nothing
}

static McxStatus ConnectionInitUpdateTo(Connection * connection, TimeInterval * time) {
    Channel * channel = (Channel *) connection->out_;

#ifdef MCX_DEBUG
    if (time->startTime < MCX_DEBUG_LOG_TIME) {
        ChannelInfo * info = &channel->info;
        MCX_DEBUG_LOG("[%f] CONN   (%s) UpdateToOutput", time->startTime, ChannelInfoGetName(info));
    }
#endif

    if (!connection->useInitialValue_) {
        if (RETURN_OK != ChannelValueSetFromReference(&connection->store_, channel->GetValueReference(channel))) {
            return RETURN_ERROR;
        }
        if (channel->IsDefinedDuringInit(channel)) {
            connection->SetDefinedDuringInit(connection);
        }
    } else {
        connection->SetDefinedDuringInit(connection);
    }

    return RETURN_OK;
}

static McxStatus ConnectionEnterInitializationMode(Connection * connection) {
#ifdef MCX_DEBUG
        Channel * channel = (Channel *) connection->out_;
        ChannelInfo * info = &channel->info;
        MCX_DEBUG_LOG("[%f] CONN   (%s) EnterInit", 0.0, ChannelInfoGetName(info));
#endif

    if (connection->state_ == InInitializationMode) {
        mcx_log(LOG_ERROR, "Connection: Enter initialization mode: Called multiple times");
        return RETURN_ERROR;
    }

    connection->state_ = InInitializationMode;

    // save functions for normal mode
    connection->NormalUpdateFrom_ = connection->UpdateFromInput;
    connection->NormalUpdateTo_ = connection->UpdateToOutput;
    connection->normalValue_ = connection->value_;

    // set functions for initialization mode
    connection->UpdateFromInput = ConnectionInitUpdateFrom;
    connection->UpdateToOutput = ConnectionInitUpdateTo;
    connection->value_ = ChannelValueReference(&connection->store_);
    connection->IsDefinedDuringInit = ConnectionIsDefinedDuringInit;
    connection->SetDefinedDuringInit = ConnectionSetDefinedDuringInit;

    return RETURN_OK;
}

static McxStatus ConnectionExitInitializationMode(Connection * connection, double time) {
    TimeInterval interval = {time, time};

        McxStatus retVal = RETURN_OK;

#ifdef MCX_DEBUG
    if (time < MCX_DEBUG_LOG_TIME) {
        Channel * channel = (Channel *) connection->out_;
        ChannelInfo * info = &channel->info;
        MCX_DEBUG_LOG("[%f] CONN   (%s) ExitInit", time, ChannelInfoGetName(info));
    }
#endif

    if (connection->state_ != InInitializationMode) {
        mcx_log(LOG_ERROR, "Connection: Exit initialization mode: Called multiple times");
        return RETURN_ERROR;
    }

    // restore functions for normal mode
    connection->UpdateFromInput = connection->NormalUpdateFrom_;
    connection->UpdateToOutput = connection->NormalUpdateTo_;
    connection->value_ = connection->normalValue_;
    connection->IsDefinedDuringInit = NULL;
    connection->SetDefinedDuringInit(connection); // After initialization all values are defined
    connection->SetDefinedDuringInit = NULL;

    connection->UpdateFromInput(connection, &interval);
    retVal = connection->EnterCommunicationMode(connection, time);
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "Connection: Exit initialization mode: Cannot enter communication mode");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus ConnectionEnterCommunicationMode(Connection * connection, double time) {
    connection->state_ = InCommunicationMode;

    return RETURN_OK;
}

static McxStatus ConnectionEnterCouplingStepMode(Connection * connection
    , double communicationTimeStepSize, double sourceTimeStepSize, double targetTimeStepSize)
{
    connection->state_ = InCouplingStepMode;

    return RETURN_OK;
}

McxStatus ConnectionSetup(Connection * connection, ChannelOut * out, ChannelIn * in, ConnectionInfo * info) {
    McxStatus retVal = RETURN_OK;

    Channel * chOut = (Channel *) out;
    ChannelInfo * outInfo = &chOut->info;

    connection->out_  = out;
    connection->in_   = in;

    if (in->IsDiscrete(in)) {
        info->hasDiscreteTarget = TRUE;
    }

    connection->info = *info;

    ChannelValueInit(&connection->store_, ChannelTypeClone(outInfo->type));

    // Add connection to channel out
    retVal = out->RegisterConnection(out, connection);
    if (RETURN_OK != retVal) {
        char * buffer = ConnectionInfoConnectionString(info);
        mcx_log(LOG_ERROR, "Connection %s: Setup connection: Could not register with outport", buffer);
        mcx_free(buffer);
        return RETURN_ERROR;
    }

    retVal = in->RegisterConnection(in, connection, outInfo->unitString, outInfo->type);
    if (RETURN_OK != retVal) {
        char * buffer = ConnectionInfoConnectionString(info);
        mcx_log(LOG_ERROR, "Connection %s: Setup connection: Could not register with inport", buffer);
        mcx_free(buffer);
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static Connection * ConnectionCreate(Connection * connection) {
    McxStatus retVal = RETURN_OK;

    connection->Setup = NULL;

    connection->GetSource = ConnectionGetSource;
    connection->GetTarget = ConnectionGetTarget;

    connection->GetValueReference = ConnectionGetValueReference;
    connection->SetValueReference = ConnectionSetValueReference;
    connection->GetValueDimension = ConnectionGetValueDimension;
    connection->GetValueType = ConnectionGetValueType;

    connection->GetInfo   = ConnectionGetInfo;

    connection->IsDecoupled = ConnectionIsDecoupled;

    connection->IsDefinedDuringInit = NULL;
    connection->SetDefinedDuringInit = ConnectionSetDefinedDuringInit;

    connection->IsActiveDependency = ConnectionIsActiveDependency;
    connection->SetActiveDependency = ConnectionSetActiveDependency;

    connection->UpdateFromInput = ConnectionUpdateFromInput;
    connection->UpdateToOutput = NULL;
    connection->UpdateInitialValue = ConnectionUpdateInitialValue;

    connection->EnterCommunicationMode = ConnectionEnterCommunicationMode;
    connection->EnterCouplingStepMode     = ConnectionEnterCouplingStepMode;
    connection->EnterInitializationMode = ConnectionEnterInitializationMode;
    connection->ExitInitializationMode = ConnectionExitInitializationMode;

    connection->AddFilter = NULL;

    connection->out_ = NULL;
    connection->in_ = NULL;

    retVal = ConnectionInfoInit(&connection->info);
    if (RETURN_ERROR == retVal) {
        return NULL;
    }

    connection->value_ = NULL;
    connection->useInitialValue_ = FALSE;

    connection->isActiveDependency_ = TRUE;

    ChannelValueInit(&connection->store_, &ChannelTypeUnknown);

    connection->state_ = InCommunicationMode;

    connection->NormalUpdateFrom_ = NULL;
    connection->NormalUpdateTo_ = NULL;
    connection->normalValue_ = NULL;

    return connection;
}

OBJECT_CLASS(Connection, Object);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */