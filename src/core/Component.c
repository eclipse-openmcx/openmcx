/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "core/Component.h"

#include "CentralParts.h"
#include "components/ComponentFactory.h"
#include "core/Component_impl.h"
#include "core/Databus.h"
#include "core/Model.h"
#include "core/channels/Channel.h"
#include "core/channels/Channel_impl.h"
#include "core/connections/Connection_impl.h"
#include "steptypes/StepType.h"
#include "storage/ComponentStorage.h"
#include "storage/ResultsStorage.h"
#include "util/compare.h"
#include "util/signals.h"
#include "util/string.h"
#include "util/time.h"

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


void ComponentLog(const Component * comp, LogSeverity sev, const char * format, ...) {
#define MSG_MAX_SIZE 2048
    char msg[MSG_MAX_SIZE];
    size_t len = strlen(format);

    va_list args;

    msg[0] = '\0';

    if (0 == len) {
        return;
    }

    va_start(args, format);
    vsnprintf(msg, MSG_MAX_SIZE, format, args);
    va_end(args);

    if (comp) {
        mcx_log(sev, "%s: %s", comp->GetName(comp), msg);
    } else {
        mcx_log(sev, "Unknown element: %s", msg);
    }
}

McxStatus ComponentRead(Component * comp, ComponentInput * input, const struct Config * const config) {
    InputElement * inputElement = (InputElement *)input;
    McxStatus retVal = RETURN_OK;

    // copy the input data for component cloning
    comp->data->input = (ComponentInput*)inputElement->Clone(inputElement);
    if (!comp->data->input) {
        mcx_log(LOG_ERROR, "Cloning component input representation failed");
        return RETURN_ERROR;
    }

    // name is unique within the model!!
    comp->data->name = mcx_string_copy(input->name);
    if (!strcmp(comp->data->name, "")) {
        mcx_log(LOG_ERROR, "Illegal name value: \"\"");
        return RETURN_ERROR;
    }

    mcx_log(LOG_DEBUG, "  Reading \"%s\" component: \"%s\"", comp->data->typeString, comp->data->name);

    // trigger sequence
    if (input->triggerSequence.defined) {
        comp->data->triggerSequence = input->triggerSequence.value;
    }
    if (comp->data->triggerSequence < -1) {
        ComponentLog(comp, LOG_ERROR, "Invalid trigger sequence number %d, expected non-negative", comp->data->triggerSequence);
        return RETURN_ERROR;
    }
    // input_time
    if (input->inputAtEndTime.defined) {
        mcx_log(LOG_DEBUG, "    Using individual evaluation time for input");
        comp->data->useInputsAtCouplingStepEndTime = input->inputAtEndTime.value;
        ComponentSetHasOwnInputEvaluationTime(comp, TRUE);
        ComponentSetStoreInputsAtCouplingStepEndTime(comp, ComponentGetUseInputsAtCouplingStepEndTime(comp));
    }

    // coupling time step size
    if (input->deltaTime.defined) {
        if (comp->SetTimeStep(comp, input->deltaTime.value) == RETURN_ERROR) {
            return RETURN_ERROR;
        }
    }

    // read inports
    if (input->inports) {
        DatabusInfo * info = DatabusGetInInfo(comp->data->databus);
        mcx_log(LOG_DEBUG, "  Reading inports:");

        // Read
        retVal = DatabusInfoRead(info, input->inports, comp->ChannelInRead, comp, comp->GetInChannelDefaultMode(comp));
        if (RETURN_ERROR == retVal) {
            ComponentLog(comp, LOG_ERROR, "Could not read inports");
            return RETURN_ERROR;
        }

        // Setup
        if (comp->ChannelInSetup) {
            comp->ChannelInSetup(comp);
            if (RETURN_OK != retVal) {
                ComponentLog(comp, LOG_ERROR, "Could not setup inports");
                return RETURN_ERROR;
            }
        }
    }

    // read outports
    if (input->outports) {
        DatabusInfo * info = DatabusGetOutInfo(comp->data->databus);
        mcx_log(LOG_DEBUG, "  Reading outports:");

        // Read
        retVal = DatabusInfoRead(info, input->outports, comp->ChannelOutRead, comp, CHANNEL_OPTIONAL);
        if (RETURN_ERROR == retVal) {
            ComponentLog(comp, LOG_ERROR, "Could not read outports");
            return RETURN_ERROR;
        }

        // Setup
        if (comp->ChannelOutSetup) {
            comp->ChannelOutSetup(comp);
            if (RETURN_OK != retVal) {
                ComponentLog(comp, LOG_ERROR, "Could not setup outports");
                return RETURN_ERROR;
            }
        }
    }

    // read results
    if (input->results) {
        if (input->results->rtFactor.defined) {
            comp->data->rtData.defined = TRUE;
            comp->data->rtData.enabled = input->results->rtFactor.value;
            comp->data->rtData.funcTimings.profilingTimesEnabled = config->profilingMode;
        }

        retVal = comp->data->storage->Read(comp->data->storage, input->results);
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_ERROR, "Could not read component specific result settings");
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

McxStatus ComponentSetup(Component * comp) {
    McxStatus retVal = RETURN_OK;

    // fill structures with data
    DatabusSetup(comp->data->databus,
                 DatabusGetInInfo(comp->data->databus),
                 DatabusGetOutInfo(comp->data->databus),
                 comp->data->model->config);

    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Could not setup ports");
        return RETURN_ERROR;
    }

    if (comp->data->model) {
        // get settings from task
        if (comp->data->model->task) {
            if (!comp->data->rtData.defined) {
                comp->data->rtData.enabled = comp->data->model->task->rtFactorEnabled;
                comp->data->rtData.funcTimings.profilingTimesEnabled = comp->data->model->config->profilingMode;
            }

            if (comp->data->model->task->params) {
                comp->data->sumTime = comp->data->model->task->params->sumTime;
            } else {
                ComponentLog(comp, LOG_ERROR, "Could not access simulation parameters");
                return RETURN_ERROR;
            }
        } else {
            ComponentLog(comp, LOG_ERROR, "Could not access simulation settings");
            return RETURN_ERROR;
        }
        // get settings from config
        if (comp->data->model->config) {
            comp->data->maxNumTimeSnapWarnings = comp->data->model->config->maxNumTimeSnapWarnings;
        }
    }
    retVal = comp->SetupRTFactor(comp);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Could not setup real time factor");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus DefineTimingChannel(Component * comp, const char * chName, const char * unit, const void * reference) {
    McxStatus retVal = RETURN_OK;

    char * id = CreateChannelID(comp->GetName(comp), chName);
    if (!id) {
        ComponentLog(comp, LOG_ERROR, "Could not create ID for timing port %s", chName);
        return RETURN_ERROR;
    }

    retVal = DatabusAddRTFactorChannel(comp->data->databus, chName, id, unit, reference, CHANNEL_DOUBLE);

    mcx_free(id);

    if (retVal == RETURN_ERROR) {
        ComponentLog(comp, LOG_ERROR, "Could not add timing port %s", chName);
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus ComponentSetupRTFactor(Component * comp) {
    if (comp->data->rtData.enabled) {
        McxStatus retVal = RETURN_OK;

        // RealTime channels
        retVal = DefineTimingChannel(comp, "RealTime Clock", GetTimeUnitString(), &comp->data->rtData.rtTotalSum_s);
        if (RETURN_ERROR == retVal) {
            return RETURN_ERROR;
        }

        retVal = DefineTimingChannel(comp, "RealTime Clock Calc", GetTimeUnitString(), &comp->data->rtData.rtCalcSum_s);
        if (RETURN_ERROR == retVal) {
            return RETURN_ERROR;
        }

        retVal = DefineTimingChannel(comp, "RealTime Factor Calc", "-", &comp->data->rtData.rtFactorCalc);
        if (RETURN_ERROR == retVal) {
            return RETURN_ERROR;
        }

        retVal = DefineTimingChannel(comp, "RealTime Factor Calc (Avg)", "-", &comp->data->rtData.rtFactorCalcAvg);
        if (RETURN_ERROR == retVal) {
            return RETURN_ERROR;
        }

        retVal = DefineTimingChannel(comp, "RealTime Factor", "-", &comp->data->rtData.rtFactorTotal);
        if (RETURN_ERROR == retVal) {
            return RETURN_ERROR;
        }

        retVal = DefineTimingChannel(comp, "RealTime Factor (Avg)", "-", &comp->data->rtData.rtFactorTotalAvg);
        if (RETURN_ERROR == retVal) {
            return RETURN_ERROR;
        }

        // Scheduling channels
        retVal = DefineTimingChannel(comp, "CalcStartWallClockTime", "time~mys", &comp->data->rtData.funcTimings.rtCalc.startTime);
        if (RETURN_ERROR == retVal) {
            return RETURN_ERROR;
        }

        retVal = DefineTimingChannel(comp, "CalcEndWallClockTime", "time~mys", &comp->data->rtData.funcTimings.rtCalc.endTime);
        if (RETURN_ERROR == retVal) {
            return RETURN_ERROR;
        }

        retVal = DefineTimingChannel(comp, "SyncStartWallClockTime", "time~mys", &comp->data->rtData.funcTimings.rtSync.startTime);
        if (RETURN_ERROR == retVal) {
            return RETURN_ERROR;
        }

        retVal = DefineTimingChannel(comp, "SyncEndWallClockTime", "time~mys", &comp->data->rtData.funcTimings.rtSync.endTime);
        if (RETURN_ERROR == retVal) {
            return RETURN_ERROR;
        }

        if (comp->data->rtData.funcTimings.profilingTimesEnabled) {
            retVal = DefineTimingChannel(comp, "InputStartWallClockTime", "time~mys", &comp->data->rtData.funcTimings.rtInput.startTime);
            if (RETURN_ERROR == retVal) {
                return RETURN_ERROR;
            }

            retVal = DefineTimingChannel(comp, "InputEndWallClockTime", "time~mys", &comp->data->rtData.funcTimings.rtInput.endTime);
            if (RETURN_ERROR == retVal) {
                return RETURN_ERROR;
            }

            retVal = DefineTimingChannel(comp, "OutputStartWallClockTime", "time~mys", &comp->data->rtData.funcTimings.rtOutput.startTime);
            if (RETURN_ERROR == retVal) {
                return RETURN_ERROR;
            }

            retVal = DefineTimingChannel(comp, "OutputEndWallClockTime", "time~mys", &comp->data->rtData.funcTimings.rtOutput.endTime);
            if (RETURN_ERROR == retVal) {
                return RETURN_ERROR;
            }

            retVal = DefineTimingChannel(comp, "StoreStartWallClockTime", "time~mys", &comp->data->rtData.funcTimings.rtStore.snapshot.startTime);
            if (RETURN_ERROR == retVal) {
                return RETURN_ERROR;
            }

            retVal = DefineTimingChannel(comp, "StoreEndWallClockTime", "time~mys", &comp->data->rtData.funcTimings.rtStore.snapshot.endTime);
            if (RETURN_ERROR == retVal) {
                return RETURN_ERROR;
            }

            retVal = DefineTimingChannel(comp, "StoreInStartWallClockTime", "time~mys", &comp->data->rtData.funcTimings.rtStoreIn.snapshot.startTime);
            if (RETURN_ERROR == retVal) {
                return RETURN_ERROR;
            }

            retVal = DefineTimingChannel(comp, "StoreInEndWallClockTime", "time~mys", &comp->data->rtData.funcTimings.rtStoreIn.snapshot.endTime);
            if (RETURN_ERROR == retVal) {
                return RETURN_ERROR;
            }

            retVal = DefineTimingChannel(comp, "TriggerInStartWallClockTime", "time~mys", &comp->data->rtData.funcTimings.rtTriggerIn.startTime);
            if (RETURN_ERROR == retVal) {
                return RETURN_ERROR;
            }

            retVal = DefineTimingChannel(comp, "TriggerInEndWallClockTime", "time~mys", &comp->data->rtData.funcTimings.rtTriggerIn.endTime);
            if (RETURN_ERROR == retVal) {
                return RETURN_ERROR;
            }
        }
    }

    return RETURN_OK;
}

McxStatus ComponentRegisterStorage(Component * comp, ResultsStorage * storage) {
    Databus * db = comp->GetDatabus(comp);
    size_t numInChannels = DatabusGetInChannelsNum(db);
    size_t numOutChannels = DatabusGetOutChannelsNum(db);
    size_t numLocalChannels = DatabusGetLocalChannelsNum(db);
    size_t numRTFactorChannels = DatabusGetRTFactorChannelsNum(db);

    ComponentStorage * compStore = comp->data->storage;

    Model * model = NULL;
    Task * task = NULL;

    double synchronizationStep = 1.0;
    double couplingStep = 1.0;

    size_t i = 0;

    McxStatus retVal = RETURN_OK;

    model = comp->GetModel(comp);
    if (!model) {
        ComponentLog(comp, LOG_ERROR, "Register at storage: No model");
        return RETURN_ERROR;
    }
    task = model->GetTask(model);
    if (!task) {
        ComponentLog(comp, LOG_ERROR, "Register at storage: No task");
        return RETURN_ERROR;
    }

    synchronizationStep = task->GetTimeStep(task);

    // may be 0.0 if comp has no own time
    couplingStep = comp->GetTimeStep(comp);

    retVal = compStore->Setup(compStore, storage, comp, synchronizationStep, couplingStep);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Could not register at storage");
        return RETURN_ERROR;
    }

    for (i = 0; i < numInChannels; i++) {
        Channel * channel = (Channel *) DatabusGetInChannel(db, i);

        if (channel->IsValid(channel)) {
            retVal = compStore->RegisterChannel(compStore, CHANNEL_STORE_IN, channel);
            if (RETURN_OK != retVal) {
                ComponentLog(comp, LOG_ERROR, "Could not register inport %d at storage", i);
                return RETURN_ERROR;
            }
        }
    }

    for (i = 0; i < numOutChannels; i++) {
        Channel * channel = (Channel *) DatabusGetOutChannel(db, i);

        retVal = compStore->RegisterChannel(compStore, CHANNEL_STORE_OUT, channel);
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_ERROR, "Could not register outport %d at storage", i);
            return RETURN_ERROR;
        }
    }

    for (i = 0; i < numLocalChannels; i++) {
        Channel * channel = (Channel *) DatabusGetLocalChannel(db, i);

        retVal = compStore->RegisterChannel(compStore, CHANNEL_STORE_LOCAL, channel);
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_ERROR, "Could not register local variable %d at storage", i);
            return RETURN_ERROR;
        }
    }

    for (i = 0; i < numRTFactorChannels; i++) {
        Channel * channel = (Channel *) DatabusGetRTFactorChannel(db, i);

        retVal = compStore->RegisterChannel(compStore, CHANNEL_STORE_RTFACTOR, channel);
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_ERROR, "Could not register rtfactor variable %d at storage", i);
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

static McxStatus ComponentStore(Component * comp, ChannelStoreType chType, double time, StoreLevel level) {
    ComponentStorage * compStore = comp->data->storage;

    if (compStore) {
        return compStore->StoreChannels(compStore, chType, time, level);
    }

    return RETURN_OK;
}

McxStatus ComponentInitialize(Component * comp, size_t group, double startTime) {
    comp->data->time = startTime;

    comp->data->rtData.simStartTime = startTime;

    return RETURN_OK;
}

McxStatus ComponentBeforeDoSteps(Component * comp, void * param) {
    FunctionTimingsSetGlobalSimStart(&comp->data->rtData.funcTimings, (McxTime *)param);

    mcx_time_get(&comp->data->rtData.rtCompStart);
    comp->data->rtData.rtLastEndCalc = comp->data->rtData.rtCompStart;
    comp->data->rtData.rtLastCompEnd = comp->data->rtData.rtCompStart;

    return RETURN_OK;
}

McxStatus ComponentExitInitializationMode(Component * comp) {
    return RETURN_OK;
}

static McxStatus ComponentUpdateInitialOutChannels(Component * comp) {
    return RETURN_OK;
}

McxStatus ComponentUpdateOutChannels(Component * comp, TimeInterval * time) {
    McxStatus retVal = RETURN_OK;

    if (NULL != comp->UpdateOutChannels) {
        comp->UpdateOutChannels(comp);
    }

    if (comp->HasOwnTime(comp)) {
        TimeInterval ownTime = {comp->data->time, comp->data->time};

        retVal = DatabusTriggerOutChannels(comp->data->databus, &ownTime);
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_ERROR, "Updating outports to time %g s failed", comp->data->time);
            return RETURN_ERROR;
        }
    } else {
        retVal = DatabusTriggerOutChannels(comp->data->databus, time);
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_ERROR, "Updating outports to time interval [%g s, %g s] failed", time->startTime, time->endTime);
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

McxStatus ComponentDoStep(Component * comp, size_t group, double time, double deltaTime, double endTime, int isNewStep) {
    McxStatus retVal = RETURN_OK;
    double startTime;
    McxTime rtTotal, rtTotalSum;

    if (comp->data->rtData.enabled) {
        /* data for local rt factor */
        startTime = comp->GetTime(comp);
        TimeSnapshotStart(&comp->data->rtData.funcTimings.rtCalc);
    }

    MCX_DEBUG_LOG("DoStep: %.16f -> %.16f", time, endTime);

    if (comp->DoStep) {
        mcx_signal_handler_set_name(comp->GetName(comp));
        retVal = comp->DoStep(comp, group, time, deltaTime, endTime, isNewStep);
        mcx_signal_handler_unset_name();
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_DEBUG, "Component specific DoStep failed");
            return RETURN_ERROR;
        }
    }

    comp->data->numSteps += 1;

    if (comp->data->hasOwnTime) {
        comp->UpdateTime(comp);

        // avoid inter/extrapolation within the epsilon range
        // if the component time is within the epsilon range of the overall time, take the overall time
        {
            double compTime = comp->GetTime(comp);
            if (compTime != endTime) {
                if (CMP_EQ == double_cmp(compTime, endTime)) {
                    const size_t MAX_NUM_LOG_MESSAGES = comp->data->maxNumTimeSnapWarnings;
                    if (MAX_NUM_LOG_MESSAGES > 0) { // check if there is an upper limit to show the message
                        if (comp->data->countSnapTimeWarning < MAX_NUM_LOG_MESSAGES) {
                            ComponentLog(comp, LOG_DEBUG, "Updating the time from %.17e s to %.17e s", compTime, endTime);
                            comp->data->countSnapTimeWarning++;
                            if (comp->data->countSnapTimeWarning == MAX_NUM_LOG_MESSAGES) {
                                ComponentLog(comp, LOG_DEBUG, "This warning will not be shown anymore");
                            }
                        }
                    } else { // show the warning always
                        ComponentLog(comp, LOG_DEBUG, "Updating the time from %.17e s to %.17e s", compTime, endTime);
                    }
                    comp->SetTime(comp, endTime);
                }
            }
        }
    } else {
        comp->SetTime(comp, endTime);
    }

    if (comp->data->rtData.enabled) {
        McxTime rtDeltaCalc;
        ComponentRTFactorData * rtData = &comp->data->rtData;
        double simCalcSum = endTime - rtData->simStartTime;

        TimeSnapshotEnd(&rtData->funcTimings.rtCalc);
        mcx_time_diff(&rtData->funcTimings.rtCalc.start, &rtData->funcTimings.rtCalc.end, &rtDeltaCalc); // data for local rt factor

        mcx_time_diff(&rtData->rtLastCompEnd, &rtData->funcTimings.rtCalc.end, &rtTotal);  // data for total rt factor and avg rt factor
        mcx_time_diff(&rtData->rtCompStart, &rtData->funcTimings.rtCalc.end, &rtTotalSum);

        rtData->rtLastEndCalc = rtData->funcTimings.rtCalc.end; // udpate wall clock for next dostep

        mcx_time_add(&rtData->rtCalcSum, &rtDeltaCalc, &rtData->rtCalcSum); // ticks of all DoSteps of this component since start
        mcx_time_add(&rtData->rtCommStepTime, &rtDeltaCalc, &rtData->rtCommStepTime); // ticks of all DoSteps of this component for the current communication step

        /* time of all DoSteps of this component since start/for the current communication step */
        rtData->rtCalcSum_s = mcx_time_to_seconds(&rtData->rtCalcSum);
        rtData->rtTotalSum_s = mcx_time_to_seconds(&rtTotalSum);

        rtData->rtCommStepTime_s = mcx_time_to_seconds(&rtData->rtCommStepTime);

        rtData->simCommStepTime += comp->GetTime(comp) - startTime; // simulation time of the current communication time step

        rtData->rtFactorCalc = rtData->rtCommStepTime_s / rtData->simCommStepTime;
        rtData->rtFactorCalcAvg = rtData->rtCalcSum_s / simCalcSum;

        // Total = (all components + framework) rt factor
        // Sum  = from simulation begin

        rtData->rtFactorTotal = mcx_time_to_seconds(&rtTotal) / rtData->simCommStepTime;
        rtData->rtFactorTotalAvg = rtData->rtTotalSum_s / simCalcSum;

        FunctionTimingsCalculateTimeDiffs(&rtData->funcTimings);
    }

    return RETURN_OK;
}

static size_t ComponentGetNumInChannels(const Component * comp) {
    return DatabusGetInChannelsNum(comp->data->databus);
}

static size_t ComponentGetNumOutChannels(const Component * comp) {
    return DatabusGetOutChannelsNum(comp->data->databus);
}

static size_t ComponentGetNumLocalChannels(const Component * comp) {
    return DatabusGetLocalChannelsNum(comp->data->databus);
}

static size_t ComponentGetNumRTFactorChannels(const Component * comp) {
    return DatabusGetRTFactorChannelsNum(comp->data->databus);
}

static size_t ComponentGetNumWriteInChannels(const Component * comp) {
    return DatabusInfoGetNumWriteChannels(DatabusGetInInfo(comp->data->databus));
}

static size_t ComponentGetNumWriteOutChannels(const Component * comp) {
    return DatabusInfoGetNumWriteChannels(DatabusGetOutInfo(comp->data->databus));
}

static size_t ComponentGetNumWriteLocalChannels(const Component * comp) {
    return DatabusInfoGetNumWriteChannels(DatabusGetLocalInfo(comp->data->databus));
}

static size_t ComponentGetNumWriteRTFactorChannels(const Component * comp) {
    return DatabusInfoGetNumWriteChannels(DatabusGetRTFactorInfo(comp->data->databus));
}

static size_t ComponentGetNumObservableChannels(const Component * comp) {
    size_t num = 0;

    num += DatabusGetOutChannelsNum(comp->data->databus);
    num += DatabusGetInChannelsNum(comp->data->databus);
    num += DatabusGetLocalChannelsNum(comp->data->databus);
    num += DatabusGetRTFactorChannelsNum(comp->data->databus);

    return num;
}

static McxStatus AddObservableChannels(const Component * comp, StringContainer * container, size_t * count) {
    size_t numOut = comp->GetNumOutChannels(comp);
    size_t numIn = comp->GetNumInChannels(comp);
    size_t numLocal = comp->GetNumLocalChannels(comp);
    size_t numRTFactor = comp->GetNumRTFactorChannels(comp);

    size_t i = 0;
    Databus * db = comp->GetDatabus(comp);

    for (i = 0; i < numOut; i++) {
        Channel * channel = (Channel *) DatabusGetOutChannel(db, i);
        char * id = channel->GetInfo(channel)->id;

        if (NULL != id) {
            StringContainerSetKeyValue(container, *count, id, channel);
            (*count)++;
        }
    }

    for (i = 0; i < numIn; i++) {
        Channel * channel = (Channel *) DatabusGetInChannel(db, i);
        char * id = channel->GetInfo(channel)->id;
        int isValid = DatabusChannelInIsValid(db, i);

        if (NULL != id && isValid) {
            StringContainerSetKeyValue(container, *count, id, channel);
            (*count)++;
        }
    }

    for (i = 0; i < numLocal; i++) {
        Channel * channel = (Channel *) DatabusGetLocalChannel(db, i);
        char * id = channel->GetInfo(channel)->id;
        int isValid = DatabusChannelLocalIsValid(db, i);

        if (NULL != id && isValid) {
            StringContainerSetKeyValue(container, *count, id, channel);
            (*count)++;
        }
    }

    for (i = 0; i < numRTFactor; i++) {
        Channel * channel = (Channel *) DatabusGetRTFactorChannel(db, i);
        char * id = channel->GetInfo(channel)->id;
        int isValid = DatabusChannelRTFactorIsValid(db, i);

        if (NULL != id && isValid) {
            StringContainerSetKeyValue(container, *count, id, channel);
            (*count)++;
        }
    }

    return RETURN_OK;
}


static size_t ComponentGetNumConnectedOutChannels(const Component * comp) {
    size_t count = 0;
    size_t i = 0;

    for (i = 0; (int) i < DatabusInfoGetChannelNum(DatabusGetOutInfo(comp->data->databus)); i++) {
        Channel * channel = (Channel *) DatabusGetOutChannel(comp->data->databus, i);
        if (channel->IsValid(channel)) {
            count++;
        }
    }

    return count;
}

size_t ComponentGetNumOutGroups(const Component * comp) {
    return 1;
}

size_t ComponentGetNumInitialOutGroups(const Component * comp) {
    return DatabusGetOutChannelsNum(comp->data->databus);
}

size_t ComponentGetOutGroup(const Component * comp, size_t idx ) {
    return 0;  /* only one out group for now */
}

size_t ComponentGetInitialOutGroup(const Component * comp, size_t idx) {
    return idx;
}

/* each out group depends on all in channels */
struct Dependencies * ComponentGetInOutGroupsInitialFullDependency(const Component * comp) {
    size_t i = 0, j = 0;
    size_t numIn = comp->GetNumInChannels(comp);
    size_t numOutGroups = comp->GetNumInitialOutGroups(comp);
    struct Dependencies * A = DependenciesCreate(numIn, numOutGroups);

    if (NULL == A) {
        return NULL;
    }

    for (i = 0; i < numIn; i++) {
        for (j = 0; j < numOutGroups; j++) {
            SetDependency(A, i, j, DEP_DEPENDENT);
        }
    }

    return A;
}

/* each out group depends on all in channels */
struct Dependencies * ComponentGetInOutGroupsFullDependency(const Component * comp) {
    size_t i = 0, j = 0;
    size_t numIn = comp->GetNumInChannels(comp);
    size_t numOutGroups = comp->GetNumOutGroups(comp);
    struct Dependencies * A = DependenciesCreate(numIn, numOutGroups);

    if (NULL == A) {
        return NULL;
    }

    for (i = 0; i < numIn; i++) {
        for (j = 0; j < numOutGroups; j++) {
            SetDependency(A, i, j, DEP_DEPENDENT);
        }
    }

    return A;
}

/* each out group depends on no in channels */
struct Dependencies * ComponentGetInOutGroupsInitialNoDependency(const Component * comp) {
    size_t i = 0, j = 0;
    size_t numIn = comp->GetNumInChannels(comp);
    size_t numOutGroups = comp->GetNumInitialOutGroups(comp);
    struct Dependencies * A = DependenciesCreate(numIn, numOutGroups);

    if (NULL == A) {
        return NULL;
    }

    for (i = 0; i < numIn; i++) {
        for (j = 0; j < numOutGroups; j++) {
            SetDependency(A, i, j, DEP_INDEPENDENT);
        }
    }

    return A;
}

/* each out group depends on no in channels */
struct Dependencies * ComponentGetInOutGroupsNoDependency(const Component * comp) {
    size_t i = 0, j = 0;
    size_t numIn = comp->GetNumInChannels(comp);
    size_t numOutGroups = comp->GetNumOutGroups(comp);
    struct Dependencies * A = DependenciesCreate(numIn, numOutGroups);

    if (NULL == A) {
        return NULL;
    }

    for (i = 0; i < numIn; i++) {
        for (j = 0; j < numOutGroups; j++) {
            SetDependency(A, i, j, DEP_INDEPENDENT);
        }
    }

    return A;
}

McxStatus ComponentEnterCommunicationPoint(Component * comp, TimeInterval * time) {
    McxStatus retVal = RETURN_OK;
    ComponentRTFactorData * rtData = &comp->data->rtData;

    mcx_time_init(&rtData->rtCommStepTime);
    rtData->simCommStepTime = 0;
    rtData->rtLastCompEnd = rtData->rtLastEndCalc;

    TimeSnapshotStart(&rtData->funcTimings.rtSync);
    retVal = DatabusEnterCommunicationMode(comp->data->databus, time->startTime);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Cannot enter communication mode at time %.17g s", time->startTime);
        return RETURN_ERROR;
    }
    TimeSnapshotEnd(&rtData->funcTimings.rtSync);

    return RETURN_OK;
}

McxStatus ComponentEnterCommunicationPointForConnections(Component * comp, ObjectList * connections, TimeInterval * time) {
    McxStatus retVal = RETURN_OK;
    ComponentRTFactorData * rtData = &comp->data->rtData;

    mcx_time_init(&rtData->rtCommStepTime);
    rtData->simCommStepTime = 0;
    rtData->rtLastCompEnd = rtData->rtLastEndCalc;

    TimeSnapshotStart(&rtData->funcTimings.rtSync);
    retVal = DatabusEnterCommunicationModeForConnections(comp->data->databus, connections, time->startTime);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Cannot enter communication mode for connections at time %.17g s", time->startTime);
        return RETURN_ERROR;
    }
    TimeSnapshotEnd(&rtData->funcTimings.rtSync);

    return RETURN_OK;
}

static ComponentStorage * ComponentGetStorage(const Component * comp) {
    return comp->data->storage;
}

static void ComponentSetModel(Component * comp, Model * model) {
    comp->data->model = model;
}

ConnectionInfo * GetInConnectionInfo(const Component * comp, size_t channelID) {
    size_t channelNum = DatabusInfoGetChannelNum(DatabusGetInInfo(comp->data->databus));

    if (channelID < channelNum) {
        ChannelIn * in = DatabusGetInChannel(comp->data->databus, channelID);
        return in->GetConnectionInfo(in);
    }

    return NULL;
}

Connection * GetInConnection(const Component * comp, size_t channelID) {
    size_t channelNum = DatabusInfoGetChannelNum(DatabusGetInInfo(comp->data->databus));
    if (channelID < channelNum) {
        ChannelIn * in = DatabusGetInChannel(comp->data->databus, channelID);
        return in->GetConnection(in);
    }

    return NULL;
}

static void ComponentDestructor(Component * comp) {
    object_destroy(comp->data);

}

static double ComponentGetTime(const Component * comp) {
    return comp->data->time;
}

static void ComponentSetTime(Component * comp, double time) {
    comp->data->time = time;
}

static int ComponentHasOwnTime(const Component * comp) {
    return comp->data->hasOwnTime;
}

static void ComponentSetHasOwnTime(Component * comp) {
    comp->data->hasOwnTime = 1;
}

static double ComponentGetTimeStep(const Component * comp) {
    return comp->data->timeStepSize;
}

static McxStatus ComponentSetTimeStep(Component * comp, double timeStep) {
    if (double_leq(timeStep, 0.)) {
        ComponentLog(comp, LOG_ERROR, "Set timestep: Time step %.17e <= 0.0", timeStep);
        return RETURN_ERROR;
    }

    //mcx_log(LOG_DEBUG, "Component (%s) uses time-step: %f", comp->GetName(comp), timeStep);
    comp->data->timeStepSize = timeStep;
    comp->data->hasOwnTime = 1;

    return RETURN_OK;
}

static McxStatus ComponentSetDefaultTimeStep(Component * comp, double timeStep) {
    if (double_leq(timeStep, 0.)) {
        ComponentLog(comp, LOG_ERROR, "Set timestep: Time step %.17e <= 0.0", timeStep);
        return RETURN_ERROR;
    }
    comp->data->timeStepSize = timeStep;
    return RETURN_OK;
}

static ComponentFinishState ComponentGetFinishState(const Component * comp) {
    return comp->data->finishState;
}

static void ComponentSetIsFinished(Component * comp) {
    if (comp->data->finishState != COMP_NEVER_FINISHES) {
        comp->data->finishState = COMP_IS_FINISHED;
    }
}

static void ComponentUpdateTime(Component * comp) {
    if (comp->data->sumTime || comp->data->hasOwnTime) {
        comp->data->time += comp->data->timeStepSize;
    } else {
        comp->data->time = comp->data->numSteps * comp->data->timeStepSize;
    }
}

static Databus * ComponentGetDatabus(const Component * comp) {
    return comp->data->databus;
}

static const char * ComponentGetName(const Component * comp) {
    // this function is often called for error or debug messages
    // If an error occurs while the name is not yet available, an empty string will be returned
    if (NULL != comp->data) {
        if (NULL != comp->data->name) {
            return comp->data->name;
        }
    }

    return "";
}

static const char * ComponentGetType(const Component * comp) {
    return comp->data->typeString;
}

static struct Model * ComponentGetModel(const Component * comp) {
    return comp->data->model;
}

static size_t ComponentGetID(const Component * comp) {
    return comp->data->id;
}

static int ComponentGetSequenceNumber(const Component * comp) {
    return comp->data->triggerSequence;
}

static ObjectList * ComponentGetConnections(Component * fromComp, Component * toComp) {
    size_t i = 0;
    size_t j = 0;
    McxStatus retVal = RETURN_OK;

    struct Databus * db = fromComp->GetDatabus(fromComp);
    ObjectList * connections = (ObjectList *) object_create(ObjectList);

    if (!connections) {
        return NULL;
    }

    for (i = 0; i < DatabusGetOutChannelsNum(db); i++) {
        ChannelOut * out = DatabusGetOutChannel(db, i);
        ObjectList * conns = out->GetConnections(out);
        size_t connSize = conns->Size(conns);

        for (j = 0; j < connSize; j++) {
            Connection * conn = (Connection *) conns->At(conns, j);
            ConnectionInfo * info = conn->GetInfo(conn);

            if (info->GetTargetComponent(info) == toComp) {
                retVal = connections->PushBack(connections, (Object *) conn);
                if (RETURN_OK != retVal) {
                    ComponentLog(fromComp, LOG_ERROR, "Could not collect connections");
                    object_destroy(connections);
                    return NULL;
                }
            }
        }
    }

    return connections;
}

static McxStatus WriteDebugInfoAfterSimulation(Component * comp) {
    return RETURN_OK;
}

static ChannelMode GetInChannelDefaultMode(struct Component * comp) {
    return CHANNEL_MANDATORY;
}

static char * ComponentGetResultDir(Component * comp) {
    return comp->data->model->task->storage->resultPath;
}

McxStatus ComponentOutConnectionsEnterInitMode(Component * comp) {
    size_t i, j;
    struct Databus * db = comp->GetDatabus(comp);
    size_t numOutChannels = DatabusGetOutChannelsNum(db);

    McxStatus retVal = RETURN_OK;

    for (i = 0; i < numOutChannels; i++) {
        ChannelOut * out = DatabusGetOutChannel(db, i);
        ObjectList * conns = out->GetConnections(out);
        size_t connSize = conns->Size(conns);

        for (j = 0; j < connSize; j++) {
            Connection * connection = (Connection *) conns->At(conns, j);
            retVal = connection->EnterInitializationMode(connection);
            if (RETURN_OK != retVal) { // error message in calling function
                return RETURN_ERROR;
            }
        }
    }
    return RETURN_OK;
}

McxStatus ComponentDoOutConnectionsInitialization(Component * comp, int onlyIfDecoupled) {
    size_t i, j;
    struct Databus * db = comp->GetDatabus(comp);
    size_t numOutChannels = DatabusGetOutChannelsNum(db);

    for (i = 0; i < numOutChannels; i++) {
        ChannelOut * out = DatabusGetOutChannel(db, i);
        ObjectList * conns = out->GetConnections(out);
        size_t connSize = conns->Size(conns);

        for (j = 0; j < connSize; j++) {
            Connection * connection = (Connection *) conns->At(conns, j);

            if (!onlyIfDecoupled || connection->IsDecoupled(connection)) {
                McxStatus retVal = connection->UpdateInitialValue(connection);
                if (RETURN_ERROR == retVal) {
                    ComponentLog(comp, LOG_ERROR, "Could not update the initial values of connection");
                    return RETURN_ERROR;
                }
            }
        }
    }

    return RETURN_OK;
}

McxStatus ComponentOutConnectionsExitInitMode(Component * comp, double time) {
    size_t i = 0, j = 0;
    struct Databus * db = comp->GetDatabus(comp);
    size_t numOutChannels = DatabusGetOutChannelsNum(db);

    McxStatus retVal = RETURN_OK;

    for (i = 0; i < numOutChannels; i++) {
        ChannelOut * out = DatabusGetOutChannel(db, i);
        ObjectList * conns = out->GetConnections(out);
        size_t connSize = conns->Size(conns);

        for (j = 0; j < connSize; j++) {
            Connection * connection = (Connection *) conns->At(conns, j);
            retVal = connection->ExitInitializationMode(connection, time);
            if (RETURN_OK != retVal) { // error message in calling function
                return RETURN_ERROR;
            }
        }
    }

    return RETURN_OK;
}

int ComponentOneOutputOneGroup(Component * comp) {
    return comp->data->oneOutputOneGroup;
}

int ComponentIsPartOfInitCalculation(Component * comp) {
    return comp->data->isPartOfInitCalculation;
}

void ComponentSetIsPartOfInitCalculation(Component * comp, int isPartOfInitCalculation) {
    comp->data->isPartOfInitCalculation = isPartOfInitCalculation;
}

int ComponentGetHasOwnInputEvaluationTime(const Component * comp) {
    return comp->data->hasOwnInputEvaluationTime;
}

void ComponentSetHasOwnInputEvaluationTime(Component * comp, int flag) {
    comp->data->hasOwnInputEvaluationTime = flag;
}

int ComponentGetUseInputsAtCouplingStepEndTime(const Component * comp) {
    return comp->data->useInputsAtCouplingStepEndTime;
}

void ComponentSetUseInputsAtCouplingStepEndTime(Component * comp, int flag) {
    comp->data->useInputsAtCouplingStepEndTime = flag;
}

int ComponentGetStoreInputsAtCouplingStepEndTime(const Component * comp) {
    return comp->data->storeInputsAtCouplingStepEndTime;
}

void ComponentSetStoreInputsAtCouplingStepEndTime(Component * comp, int flag) {
    comp->data->storeInputsAtCouplingStepEndTime = flag;
}

static McxStatus ComponentSetResultTimeOffset(Component * comp, double offset) {
    ComponentStorage * compStore = comp->data->storage;

    if (NULL == compStore) {
        ComponentLog(comp, LOG_ERROR, "No ComponentStorage to set the result time-offset (%f s).", offset);
        return RETURN_ERROR;
    }

    compStore->timeOffset = offset;

    return RETURN_OK;
}

double ComponentGetResultTimeOffset(struct Component * comp) {
    if (comp->data->storage) {
        return comp->data->storage->timeOffset;
    } else {
        ComponentLog(comp, LOG_WARNING, "No ComponentStorage to retrieve the result time-offset.");
    }

    return 0.0;
}

Component * CreateComponentFromComponentInput(ComponentFactory * factory,
                                              ComponentInput * componentInput,
                                              const size_t id,
                                              const struct Config * const config) {
    Component * comp = NULL;
    McxStatus retVal = RETURN_OK;

    comp = factory->CreateComponent(factory, componentInput->type, id);
    if (!comp) {
        mcx_log(LOG_ERROR, "Model: Could not create element %s", componentInput->type->ToString(componentInput->type));
        return NULL;
    }

    // General Data
    retVal = ComponentRead(comp, componentInput, config);
    if (RETURN_OK != retVal) {
        mcx_log(LOG_ERROR, "Model: Could not create element data");
        object_destroy(comp);
        return NULL;
    }

    // Specific Data
    if (comp->Read) {
        mcx_signal_handler_set_name(comp->GetName(comp));

        retVal = comp->Read(comp, componentInput, config);
        if (RETURN_OK != retVal) {
            mcx_log(LOG_ERROR, "Model: Could not read specific data of element %s", comp->GetName(comp));
            mcx_signal_handler_unset_name();
            object_destroy(comp);
            return NULL;
        }
        mcx_signal_handler_unset_name();
    }
    return comp;
}

Component * ComponentClone(ComponentFactory * factory,
                           const Component * source,
                           const char * cloneName,
                           const size_t id,
                           const struct Config * const config) {
    Component * newComp = NULL;
    if (source->data->input) {
        newComp = CreateComponentFromComponentInput(factory, source->data->input, id, config);
        if (newComp) {
            if (newComp->data->name) {
                mcx_free(newComp->data->name);
            }
            newComp->data->name = mcx_string_copy(cloneName);
            newComp->data->model = source->data->model;
        }
    }

    return newComp;
}

static Component * ComponentCreate(Component * comp) {
    comp->GetType = ComponentGetType;

    comp->Read = NULL;
    comp->Initialize = ComponentInitialize;
    comp->ExitInitializationMode = ComponentExitInitializationMode;
    comp->UpdateInitialOutChannels = ComponentUpdateInitialOutChannels;
    comp->UpdateOutChannels = NULL;
    comp->UpdateInChannels = NULL;

    comp->RegisterStorage = ComponentRegisterStorage;
    comp->GetStorage = ComponentGetStorage;
    comp->Store = ComponentStore;

    comp->DoStep = NULL;
    comp->PostDoStep = NULL;
    comp->Finish = NULL;

    comp->GetNumInChannels = ComponentGetNumInChannels;
    comp->GetNumOutChannels = ComponentGetNumOutChannels;
    comp->GetNumLocalChannels = ComponentGetNumLocalChannels;
    comp->GetNumRTFactorChannels = ComponentGetNumRTFactorChannels;

    comp->GetNumWriteInChannels = ComponentGetNumWriteInChannels;
    comp->GetNumWriteOutChannels = ComponentGetNumWriteOutChannels;
    comp->GetNumWriteLocalChannels = ComponentGetNumWriteLocalChannels;
    comp->GetNumWriteRTFactorChannels = ComponentGetNumWriteRTFactorChannels;

    comp->GetNumConnectedOutChannels = ComponentGetNumConnectedOutChannels;
    comp->GetNumOutGroups = ComponentGetNumOutGroups;
    comp->GetNumInitialOutGroups = ComponentGetNumInitialOutGroups;

    comp->GetInOutGroupsDependency = ComponentGetInOutGroupsFullDependency;
    comp->GetInOutGroupsInitialDependency = ComponentGetInOutGroupsInitialFullDependency;
    comp->GetOutGroup = ComponentGetOutGroup;
    comp->GetInitialOutGroup = ComponentGetInitialOutGroup;

    comp->GetTime = ComponentGetTime;
    comp->SetTime = ComponentSetTime;
    comp->HasOwnTime = ComponentHasOwnTime;
    comp->SetHasOwnTime = ComponentSetHasOwnTime;

    comp->GetFinishState = ComponentGetFinishState;
    comp->SetIsFinished = ComponentSetIsFinished;

    comp->UpdateTime = ComponentUpdateTime;

    comp->SetModel = ComponentSetModel;

    comp->GetDatabus = ComponentGetDatabus;
    comp->GetName    = ComponentGetName;
    comp->GetModel   = ComponentGetModel;
    comp->GetID      = ComponentGetID;

    comp->OneOutputOneGroup = ComponentOneOutputOneGroup;
    comp->GetSequenceNumber = ComponentGetSequenceNumber;
    comp->IsPartOfInitCalculation = ComponentIsPartOfInitCalculation;
    comp->SetIsPartOfInitCalculation = ComponentSetIsPartOfInitCalculation;

    comp->GetConnections = ComponentGetConnections;

    comp->GetTimeStep = ComponentGetTimeStep;
    comp->SetTimeStep = ComponentSetTimeStep;
    comp->SetDefaultTimeStep = ComponentSetDefaultTimeStep; // does not set the hasOwnTime flag

    comp->GetResultDir = ComponentGetResultDir;

    comp->ChannelInRead      = NULL;
    comp->ChannelInSetup     = NULL;

    comp->ChannelOutRead      = NULL;
    comp->ChannelOutSetup     = NULL;

    comp->GetInChannelDefaultMode = GetInChannelDefaultMode;

    comp->WriteDebugInfoAfterSimulation = WriteDebugInfoAfterSimulation;

    comp->Setup = NULL;
    comp->SetupRTFactor = ComponentSetupRTFactor;
    comp->SetupDatabus = NULL;

    comp->GetPPDLink = NULL;

    comp->PreDoUpdateState = NULL;

    comp->PostDoUpdateState = NULL;

    comp->SetResultTimeOffset = ComponentSetResultTimeOffset;

    comp->data = (ComponentData *) object_create(ComponentData);
    if (!comp->data) {
        return NULL;
    }

    comp->data->typeString = NULL;


    comp->GetNumObservableChannels = ComponentGetNumObservableChannels;
    comp->AddObservableChannels = AddObservableChannels;

    comp->OnConnectionsDone = NULL;

    return comp;
}

void TimeSnapshotStart(TimeSnapshot * snapshot) {
    if (!snapshot->enabled) {
        return;
    }

    mcx_time_get(&snapshot->start);
}

void TimeSnapshotEnd(TimeSnapshot * snapshot) {
    if (!snapshot->enabled) {
        return;
    }

    mcx_time_get(&snapshot->end);
}

static void TimeSnapshotDiffToMicroSeconds(TimeSnapshot * snapshot, McxTime * startTimePoint) {
    McxTime start;
    McxTime end;

    mcx_time_diff(startTimePoint, &snapshot->start, &start);
    mcx_time_diff(startTimePoint, &snapshot->end, &end);

    snapshot->startTime = mcx_time_to_micro_s(&start);
    snapshot->endTime = mcx_time_to_micro_s(&end);

    snapshot->startTime = snapshot->startTime < 0.0 ? 0.0 : snapshot->startTime;
    snapshot->endTime = snapshot->endTime < 0.0 ? 0.0 : snapshot->endTime;
}

static void TimeSnapshotInit(TimeSnapshot * snapshot) {
    snapshot->enabled = FALSE;

    mcx_time_init(&snapshot->start);
    mcx_time_init(&snapshot->end);

    snapshot->startTime = 0.0;
    snapshot->endTime = 0.0;
}

static void DelayedTimeSnapshotInit(DelayedTimeSnapshot * snapshot) {
    TimeSnapshotInit(&snapshot->snapshot);

    snapshot->startTimePre = 0.0;
    snapshot->endTimePre = 0.0;
}

static void DelayedTimeSnapshotDiffToMicroSeconds(DelayedTimeSnapshot * snapshot, McxTime * startTimePoint) {
    McxTime start;
    McxTime end;

    snapshot->snapshot.startTime = snapshot->startTimePre;
    snapshot->snapshot.endTime = snapshot->endTimePre;;

    mcx_time_diff(startTimePoint, &snapshot->snapshot.start, &start);
    mcx_time_diff(startTimePoint, &snapshot->snapshot.end, &end);

    snapshot->startTimePre = mcx_time_to_micro_s(&start);
    snapshot->endTimePre = mcx_time_to_micro_s(&end);
}

void FunctionTimingsCalculateTimeDiffs(FunctionTimings * timings) {
    TimeSnapshotDiffToMicroSeconds(&timings->rtCalc, &timings->rtGlobalSimStart);
    TimeSnapshotDiffToMicroSeconds(&timings->rtSync, &timings->rtGlobalSimStart);

    TimeSnapshotDiffToMicroSeconds(&timings->rtInput, &timings->rtGlobalSimStart);
    TimeSnapshotDiffToMicroSeconds(&timings->rtOutput, &timings->rtGlobalSimStart);
    TimeSnapshotDiffToMicroSeconds(&timings->rtTriggerIn, &timings->rtGlobalSimStart);

    DelayedTimeSnapshotDiffToMicroSeconds(&timings->rtStore, &timings->rtGlobalSimStart);
    DelayedTimeSnapshotDiffToMicroSeconds(&timings->rtStoreIn, &timings->rtGlobalSimStart);
}

void FunctionTimingsSetGlobalSimStart(FunctionTimings * timings, McxTime * simStart) {
    timings->rtGlobalSimStart = *simStart;

    timings->rtCalc.enabled = TRUE;
    timings->rtSync.enabled = TRUE;

    timings->rtInput.enabled = TRUE;
    timings->rtOutput.enabled = TRUE;
    timings->rtTriggerIn.enabled = TRUE;

    timings->rtStore.snapshot.enabled = TRUE;
    timings->rtStoreIn.snapshot.enabled = TRUE;
}

static void FunctionTimingsInit(FunctionTimings * timings) {
    mcx_time_init(&timings->rtGlobalSimStart);

    timings->profilingTimesEnabled = FALSE;

    TimeSnapshotInit(&timings->rtCalc);
    TimeSnapshotInit(&timings->rtSync);

    TimeSnapshotInit(&timings->rtInput);
    TimeSnapshotInit(&timings->rtOutput);
    TimeSnapshotInit(&timings->rtTriggerIn);

    DelayedTimeSnapshotInit(&timings->rtStore);
    DelayedTimeSnapshotInit(&timings->rtStoreIn);
}

static void ComponentDataDestructor(ComponentData * data) {
    object_destroy(data->databus);

    object_destroy(data->storage);
    if (NULL != data->input) {
        object_destroy(data->input);
    }
    if (NULL != data->typeString) {
        mcx_free(data->typeString);
    }
    if (NULL != data->name) {
        mcx_free(data->name);
    }
}

static ComponentData * ComponentDataCreate(ComponentData * data) {
    ComponentRTFactorData * rtData = NULL;

    data->model = NULL;
    data->id = 0;
    data->name = NULL;
    data->typeString = NULL;

    data->triggerSequence = -1;
    data->oneOutputOneGroup = FALSE;
    data->isPartOfInitCalculation = FALSE;

    data->time = 0.;
    data->timeStepSize = 0.;
    data->hasOwnTime = 0;
    data->numSteps = 0;
    data->countSnapTimeWarning = 0;
    data->maxNumTimeSnapWarnings = 0;

    data->sumTime = FALSE;

    data->finishState = COMP_IS_NOT_FINISHED;

    data->input = NULL;

    rtData = &data->rtData;
    rtData->defined = FALSE;
    rtData->enabled = FALSE;

    mcx_time_init(&rtData->rtCalcSum);
    mcx_time_init(&rtData->rtCommStepTime);

    rtData->rtCalcSum_s = 0.;
    rtData->rtTotalSum_s = 0.;

    rtData->rtCommStepTime_s = 0.;

    rtData->simStartTime = 0.;
    rtData->simCommStepTime = 0.;

    rtData->rtFactorCalc = 0.;
    rtData->rtFactorCalcAvg = 0.;

    mcx_time_init(&rtData->rtCompStart);
    mcx_time_init(&rtData->rtLastEndCalc);
    mcx_time_init(&rtData->rtLastCompEnd);

    FunctionTimingsInit(&rtData->funcTimings);

    rtData->rtFactorTotal = 0.;
    rtData->rtFactorTotalAvg = 0.;

    data->hasOwnInputEvaluationTime = FALSE;
    data->useInputsAtCouplingStepEndTime = FALSE;
    data->storeInputsAtCouplingStepEndTime = FALSE;

    data->storage = (ComponentStorage *) object_create(ComponentStorage);
    if (!data->storage) {
        return NULL;
    }

    data->databus = (Databus *) object_create(Databus);
    if (!data->databus) {
        return NULL;
    }

    return data;
}

OBJECT_CLASS(Component, Object);
OBJECT_CLASS(ComponentData, Object);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */