/********************************************************************************
 * Copyright (c) 2020 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "components/comp_fmu.h"

#include "FMI/fmi_import_context.h"
#include "components/comp_fmu_impl.h"
#include "core/Databus.h"
#include "core/Component_impl.h"
#include "core/channels/ChannelInfo.h"
#include "core/connections/ConnectionInfoFactory.h"
#include "fmilib.h"
#include "fmu/Fmu1Value.h"
#include "fmu/Fmu2Value.h"
#include "objects/Map.h"
#include "reader/model/components/specific_data/FmuInput.h"
#include "util/string.h"
#include "util/signals.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

static struct Dependencies* Fmu2GetInOutGroupsInitialDependency(const Component * comp);
static McxStatus Fmu1Setup(Component * comp);
static McxStatus Fmu2Setup(Component * comp);

static McxStatus Fmu1SetupDatabus(Component * comp) {
    CompFMU * compFmu = (CompFMU *) comp;
    Fmu1CommonStruct * fmu1 = &compFmu->fmu1;

    Databus * db = comp->GetDatabus(comp);
    DatabusInfo * dbInfo = NULL;
    size_t numChannels = 0;

    ObjectContainer * vals = NULL;

    McxStatus retVal = RETURN_OK;

    size_t i = 0;

    dbInfo = DatabusGetInInfo(db);
    numChannels = DatabusInfoGetChannelNum(dbInfo);
    vals = fmu1->in;
    for (i = 0; i < numChannels; i++) {
        ChannelInfo * info = DatabusInfoGetChannel(dbInfo, i);

        if (DatabusChannelInIsValid(db, i)) {
            Fmu1Value * val = NULL;
            fmi1_import_variable_t * var = NULL;

            jm_status_enu_t status = jm_status_success;

            const char * channelName = info->nameInTool;
            if (NULL == channelName) {
                channelName = ChannelInfoGetName(info);
            }

            var = fmi1_import_get_variable_by_name(fmu1->fmiImport, channelName);
            if (!var) {
                ComponentLog(comp, LOG_ERROR, "Could not get variable %s", channelName);
                return RETURN_ERROR;
            }

            if (!ChannelTypeEq(info->type, Fmi1TypeToChannelType(fmi1_import_get_variable_base_type(var)))) {
                ComponentLog(comp, LOG_ERROR, "Variable types of %s do not match", channelName);
                ComponentLog(comp, LOG_ERROR, "Expected: %s, Imported from FMU: %s",
                    ChannelTypeToString(info->type), ChannelTypeToString(Fmi1TypeToChannelType(fmi1_import_get_variable_base_type(var))));
                return RETURN_ERROR;
            }

            val = Fmu1ValueMake(channelName, var, info->channel);
            if (!val) {
                ComponentLog(comp, LOG_ERROR, "Could not set value for channel %s", channelName);
                return RETURN_ERROR;
            }

            retVal = vals->PushBackNamed(vals, (Object *)val, channelName);
            if (RETURN_OK != retVal) {
                ComponentLog(comp, LOG_ERROR, "Could not store value for %s", channelName);
                return RETURN_ERROR;
            }

            retVal = DatabusSetInReference(comp->GetDatabus(comp), i, ChannelValueReference(&val->val),
                ChannelValueType(&val->val));
            if (RETURN_OK != retVal) {
                ComponentLog(comp, LOG_ERROR, "Could not set reference for channel %s", channelName);
                return RETURN_ERROR;
            }
        }
    }

    dbInfo = DatabusGetOutInfo(db);
    numChannels = DatabusInfoGetChannelNum(dbInfo);
    vals = fmu1->out;
    for (i = 0; i < numChannels; i++) {
        ChannelInfo * info = DatabusInfoGetChannel(dbInfo, i);

        Fmu1Value * val = NULL;
        fmi1_import_variable_t * var = NULL;
        jm_status_enu_t status = jm_status_success;

        const char * channelName = info->nameInTool;
        if (NULL == channelName) {
            channelName = ChannelInfoGetName(info);
        }

        var = fmi1_import_get_variable_by_name(fmu1->fmiImport, channelName);
        if (!var) {
            ComponentLog(comp, LOG_ERROR, "Could not get variable %s", channelName);
            return RETURN_ERROR;
        }

        if (!ChannelTypeEq(info->type, Fmi1TypeToChannelType(fmi1_import_get_variable_base_type(var)))) {
            ComponentLog(comp, LOG_ERROR, "Variable types of %s do not match", channelName);
            ComponentLog(comp, LOG_ERROR, "Expected: %s, Imported from FMU: %s",
                ChannelTypeToString(info->type), ChannelTypeToString(Fmi1TypeToChannelType(fmi1_import_get_variable_base_type(var))));
            return RETURN_ERROR;
        }

        val = Fmu1ValueMake(channelName, var, NULL);
        if (!val) {
            ComponentLog(comp, LOG_ERROR, "Could not set value for channel %s", channelName);
            return RETURN_ERROR;
        }

        retVal = vals->PushBack(vals, (Object *)val);
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_ERROR, "Could not store value for %s", channelName);
            return RETURN_ERROR;
        }

        retVal = DatabusSetOutReference(comp->GetDatabus(comp), i, ChannelValueReference(&val->val),
            ChannelValueType(&val->val));
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_ERROR, "Could not set reference for channel %s", channelName);
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

McxStatus CompFmuSetup(Component * comp) {
    CompFMU * compFmu = (CompFMU *) comp;
    FmuCommon * common = &compFmu->common;

    if (common->version == fmi_version_1_enu) {
        return Fmu1Setup(comp);
    } else if (common->version == fmi_version_2_0_enu) {
        return Fmu2Setup(comp);
    } else {
        ComponentLog(comp, LOG_ERROR, "Unknown FMU Version: %s", fmi_version_to_string(common->version));
        return RETURN_ERROR;
    }
}

static McxStatus Fmu1Setup(Component * comp) {
    CompFMU * compFmu = (CompFMU *) comp;
    Fmu1CommonStruct * fmu1 = &compFmu->fmu1;
    FmuCommon * common = &compFmu->common;
    McxStatus retVal = RETURN_OK;
    Databus * db = comp->GetDatabus(comp);

    retVal = Fmu1CommonStructSetup(common, fmu1, Fmu1CoSimulation);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Setting up FMU Information failed");
        return RETURN_ERROR;
    }

    if (compFmu->localValues) {
        fmi1_import_variable_list_t * allVars = NULL;
        fmi1_import_variable_list_t * localVars = NULL;
        allVars = fmi1_import_get_variable_list(compFmu->fmu1.fmiImport);
        localVars = fmi1_import_filter_variables(allVars, fmi1FilterLocalVariables, NULL);
        fmi1_import_free_variable_list(allVars);
        retVal = fmi1CreateValuesOutOfVariables(compFmu->fmu1.localValues, localVars);
        fmi1_import_free_variable_list(localVars);
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_ERROR, "Could not get local variables");
            return RETURN_ERROR;
        }

        retVal = fmi1AddLocalChannelsFromLocalValues(compFmu->fmu1.localValues, comp->GetName(comp), db);
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_ERROR, "Could not add local channels");
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

static McxStatus Fmu1Initialize(Component * comp, size_t group, double startTime) {
    CompFMU * compFmu = (CompFMU *) comp;
    int a = FALSE;

    Fmu1CommonStruct * fmu1 = &compFmu->fmu1;

    fmi1_status_t status = fmi1_status_ok;

    McxStatus retVal = RETURN_OK;

    // Set variables
    retVal = Fmu1SetVariableArray(fmu1, fmu1->params);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Setting parameters failed");
        return RETURN_ERROR;
    }

    retVal = Fmu1SetVariableArray(fmu1, fmu1->initialValues);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Setting initial values failed");
        return RETURN_ERROR;
    }

    retVal = Fmu1SetVariableArray(fmu1, fmu1->in);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Setting inChannels failed");
        return RETURN_ERROR;
    }

    compFmu->lastCommunicationTimePoint = startTime;

    // Initialization Mode
    ComponentLog(comp, LOG_DEBUG, "fmiInitializeSlave");
    mcx_signal_handler_set_function("fmi1_import_initialize_slave");
    status = fmi1_import_initialize_slave(fmu1->fmiImport,
                                          startTime,
                                          fmi1_false,
                                          0.0);
    mcx_signal_handler_unset_function();
    if (fmi1_status_ok != status) {
        ComponentLog(comp, LOG_ERROR, "fmiInitializeSlave failed");
        return RETURN_ERROR;
    }
    ComponentLog(comp, LOG_DEBUG, "fmiInitializeSlave done");

    // Set variables
    retVal = Fmu1SetVariableArray(fmu1, fmu1->initialValues);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Setting initialValues failed");
        return RETURN_ERROR;
    }

    retVal = Fmu1SetVariableArray(fmu1, fmu1->in);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Setting inChannels failed");
        return RETURN_ERROR;
    }

    // local variables
    if (compFmu->localValues) {
        retVal = Fmu1GetVariableArray(fmu1, fmu1->localValues);
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_ERROR, "Retrieving local variables failed");
            return RETURN_ERROR;
        }
    }

    // Get outputs (this triggers the computation)
    retVal = Fmu1GetVariableArray(fmu1, fmu1->out);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Initialization computation failed");
        return RETURN_ERROR;
    }

    fmu1->runOk = fmi1_true;

    return RETURN_OK;
}

static McxStatus Fmu1DoStep(Component * comp, size_t group, double time, double deltaTime, double endTime, int isNewStep) {
    CompFMU * compFmu = (CompFMU *) comp;
    Fmu1CommonStruct * fmu1 = &compFmu->fmu1;

    McxStatus retVal;
    fmi1_status_t status = fmi1_status_ok;

    // Set variables
    retVal = Fmu1SetVariableArray(fmu1, fmu1->in);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Setting inChannels failed");
        return RETURN_ERROR;
    }

    // Do calculations
    mcx_signal_handler_set_function("fmi1_import_do_step");
    status = fmi1_import_do_step(fmu1->fmiImport, compFmu->lastCommunicationTimePoint, deltaTime, fmi1_true);
    mcx_signal_handler_unset_function();
    if (fmi1_status_ok == status) {
        // fine
    } else if (fmi1_status_discard == status) {
        ComponentLog(comp, LOG_WARNING, "Computation discarded");
        comp->SetIsFinished(comp);
    } else if (fmi1_status_error == status) {
        ComponentLog(comp, LOG_ERROR, "Computation failed");
        return RETURN_ERROR;
    } else if (fmi1_status_fatal == status) {
        ComponentLog(comp, LOG_ERROR, "Computation failed (fatal)");
        return RETURN_ERROR;
    } else if (fmi1_status_warning == status) {
        ComponentLog(comp, LOG_WARNING, "Computation returned with warning");
    }

    compFmu->lastCommunicationTimePoint += deltaTime;

    // Get outputs
    retVal = Fmu1GetVariableArray(fmu1, fmu1->out);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Retrieving outChannels failed");
        return RETURN_ERROR;
    }

    // local variables
    if (compFmu->localValues) {
        retVal = Fmu1GetVariableArray(fmu1, fmu1->localValues);
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_ERROR, "Retrieving local variables failed");
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

static McxStatus Fmu1Read(Component * comp, ComponentInput * input, const struct Config * const config) {
    UNUSED(config);

    CompFMU * compFmu = (CompFMU *) comp;

    FmuInput * fmuInput = (FmuInput *) input;
    InputElement * element = (InputElement *) fmuInput;

    Fmu1CommonStruct * fmu1 = &compFmu->fmu1;
    FmuCommon * common = &compFmu->common;
    ObjectContainer * vals = NULL;
    McxStatus retVal = RETURN_OK;

    retVal = Fmu1CommonStructRead(common, fmu1, Fmu1CoSimulation, fmuInput);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Reading FMU Information failed");
        return RETURN_ERROR;
    }

    if (fmuInput->modelInternalVariables.defined) {
        compFmu->localValues = fmuInput->modelInternalVariables.value;
    }

    /* read the parameters */
    {
        ParametersInput * parametersInput = input->parameters;

        if (parametersInput) {
            vals = Fmu1ReadParams(parametersInput, fmu1->fmiImport, NULL);
            if (!vals) {
                ComponentLog(comp, LOG_ERROR, "Could not read parameters");
                return RETURN_ERROR;
            }

            retVal = fmu1->params->Append(fmu1->params, vals);
            if (RETURN_OK != retVal) {
                ComponentLog(comp, LOG_ERROR, "Could not add parameters");
                return RETURN_ERROR;
            }
            object_destroy(vals);
        }
    }

    return RETURN_OK;
}

static McxStatus Fmu2SetupChannelIn(ObjectContainer /* Fmu2Values */ * vals, Databus * db, const char * logPrefix) {
    DatabusInfo * dbInfo = NULL;
    size_t numChannels = 0;

    size_t i = 0;

    McxStatus retVal = RETURN_OK;

    dbInfo = DatabusGetInInfo(db);
    numChannels = DatabusInfoGetChannelNum(dbInfo);

    for (i = 0; i < numChannels; i++) {
        ChannelInfo * info = DatabusInfoGetChannel(dbInfo, i);
        Fmu2Value * val = (Fmu2Value *) vals->At(vals, i);

        if (DatabusChannelInIsValid(db, i)) {
            const char * channelName = info->nameInTool;
            if (NULL == channelName) {
                channelName = ChannelInfoGetName(info);
            }

            val->SetChannel(val, info->channel);

            if (!ChannelTypeIsValid(val->val.type)) {
                ChannelValueInit(&val->val, ChannelTypeClone(info->type));
            }
            retVal = DatabusSetInReference(db, i,
                           ChannelValueReference(&val->val),
                           ChannelValueType(&val->val));
            if (RETURN_OK != retVal) {
                mcx_log(LOG_ERROR, "%s: Could not set reference for channel %s", logPrefix, val->name);
                return RETURN_ERROR;
            }
        }
    }

    return RETURN_OK;
}

static McxStatus Fmu2SetupChannelOut(ObjectContainer /* Fmu2Values */ * vals, Databus * db, const char * logPrefix) {
    DatabusInfo * dbInfo = NULL;
    size_t numChannels = 0;

    McxStatus retVal = RETURN_OK;

    size_t i = 0;

    dbInfo = DatabusGetOutInfo(db);
    numChannels = DatabusInfoGetChannelNum(dbInfo);

    for (i = 0; i < numChannels; i++) {
        ChannelInfo * info = DatabusInfoGetChannel(dbInfo, i);
        Fmu2Value * val = (Fmu2Value *) vals->At(vals, i);

        const char * channelName = info->nameInTool;
        if (NULL == channelName) {
            channelName = ChannelInfoGetName(info);
        }

        val->SetChannel(val, info->channel);

        if (!ChannelTypeEq(val->val.type, info->type)) {
            ChannelValueInit(&val->val, ChannelTypeClone(info->type));
        }
        retVal = DatabusSetOutReference(db, i,
                                        ChannelValueReference(&val->val),
                                        ChannelValueType(&val->val));
        if (RETURN_OK != retVal) {
            mcx_log(LOG_ERROR, "%s: Could not set reference for channel %s", logPrefix, channelName);
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

static McxStatus Fmu2SetupDatabus(Component * comp) {
    CompFMU * compFmu = (CompFMU *) comp;
    Fmu2CommonStruct * fmu2 = &compFmu->fmu2;
    Databus * db = comp->GetDatabus(comp);

    McxStatus retVal = RETURN_OK;

    {
        retVal = Fmu2SetupChannelIn(fmu2->in, db, comp->GetName(comp));
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_ERROR, "Could not setup inports");
            return RETURN_ERROR;
        }
    }

    {
        retVal = Fmu2SetupChannelOut(fmu2->out, db, comp->GetName(comp));
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_ERROR, "Could not setup outports");
            return RETURN_ERROR;
        }
    }

    {
#if defined (MCX_DEBUG)
        if (Fmu2CheckTunableParamsInputConsistency(fmu2->in, fmu2->params, fmu2->tunableParams) != RETURN_OK) {
            ComponentLog(comp, LOG_ERROR, "Parameters consistency check failed");
            return RETURN_ERROR;
        }
#endif // MCX_DEBUG
        Fmu2MarkTunableParamsAsInputAsDiscrete(fmu2->in);
    }

    return RETURN_OK;
}

static McxStatus Fmu2Setup(Component * comp) {
    CompFMU * compFmu = (CompFMU *) comp;
    McxStatus retVal = RETURN_OK;
    Databus * db = comp->GetDatabus(comp);

    if (compFmu->localValues) {
        retVal = Fmi2RegisterLocalChannelsAtDatabus(compFmu->fmu2.localValues, comp->GetName(comp), db);
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_ERROR, "Could not add local channels");
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

static McxStatus Fmu2ReadChannelIn(ObjectContainer /* Fmu2Value */ * vals, Databus * db, fmi2_import_t * fmiImport, const char * logPrefix) {
    McxStatus retVal = RETURN_OK;

    size_t i = 0;

    DatabusInfo * dbInfo = DatabusGetInInfo(db);
    size_t numChannels = DatabusInfoGetChannelNum(dbInfo);

    for (i = 0; i < numChannels; i++) {
        ChannelInfo * info = DatabusInfoGetChannel(dbInfo, i);

        Fmu2Value * val = NULL;
        fmi2_import_variable_t * var = NULL;

        const char * channelName = info->nameInTool;
        if (NULL == channelName) {
            channelName = ChannelInfoGetName(info);
        }

        // TODO: move content of if-else blocks to separate functions
        if (ChannelInfoIsBinary(info)) {
            // see https://github.com/OpenSimulationInterface/osi-sensor-model-packaging for more info
            char * channelNameLo = mcx_string_merge(2, channelName, ".base.lo");
            char * channelNameHi = mcx_string_merge(2, channelName, ".base.hi");
            char * channelNameSize = mcx_string_merge(2, channelName, ".size");

            fmi2_import_variable_t * varLo = NULL;
            fmi2_import_variable_t * varHi = NULL;
            fmi2_import_variable_t * varSize = NULL;

            varLo = fmi2_import_get_variable_by_name(fmiImport, channelNameLo);
            if (!varLo) {
                mcx_log(LOG_ERROR, "%s: Could not get variable %s", logPrefix, channelNameLo);
                return RETURN_ERROR;
            }

            varHi = fmi2_import_get_variable_by_name(fmiImport, channelNameHi);
            if (!varHi) {
                mcx_log(LOG_ERROR, "%s: Could not get variable %s", logPrefix, channelNameHi);
                return RETURN_ERROR;
            }

            varSize = fmi2_import_get_variable_by_name(fmiImport, channelNameSize);
            if (!varSize) {
                mcx_log(LOG_ERROR, "%s: Could not get variable %s", logPrefix, channelNameSize);
                return RETURN_ERROR;
            }

            if (!ChannelTypeEq(&ChannelTypeInteger, Fmi2TypeToChannelType(fmi2_import_get_variable_base_type(varLo)))) {
                mcx_log(LOG_ERROR, "%s: Variable types of %s do not match", logPrefix, channelNameLo);
                mcx_log(LOG_ERROR, "%s: Expected: %s, Imported from FMU: %s", logPrefix,
                        ChannelTypeToString(&ChannelTypeInteger), ChannelTypeToString(Fmi2TypeToChannelType(fmi2_import_get_variable_base_type(varLo))));
                return RETURN_ERROR;
            }

            if (!ChannelTypeEq(&ChannelTypeInteger, Fmi2TypeToChannelType(fmi2_import_get_variable_base_type(varHi)))) {
                mcx_log(LOG_ERROR, "%s: Variable types of %s do not match", logPrefix, channelNameHi);
                mcx_log(LOG_ERROR, "%s: Expected: %s, Imported from FMU: %s", logPrefix,
                        ChannelTypeToString(&ChannelTypeInteger), ChannelTypeToString(Fmi2TypeToChannelType(fmi2_import_get_variable_base_type(varHi))));
                return RETURN_ERROR;
            }

            if (!ChannelTypeEq(&ChannelTypeInteger, Fmi2TypeToChannelType(fmi2_import_get_variable_base_type(varSize)))) {
                mcx_log(LOG_ERROR, "%s: Variable types of %s do not match", logPrefix, channelNameSize);
                mcx_log(LOG_ERROR, "%s: Expected: %s, Imported from FMU: %s", logPrefix,
                        ChannelTypeToString(&ChannelTypeInteger), ChannelTypeToString(Fmi2TypeToChannelType(fmi2_import_get_variable_base_type(varSize))));
                return RETURN_ERROR;
            }

            val = Fmu2ValueBinaryMake(channelName, varHi, varLo, varSize, info->channel);
            if (!val) {
                mcx_log(LOG_ERROR, "%s: Could not set value for channel %s", logPrefix, channelName);
                return RETURN_ERROR;
            }

            mcx_free(channelNameLo);
            mcx_free(channelNameHi);
            mcx_free(channelNameSize);

            retVal = vals->PushBack(vals, (Object *)val);
            if (RETURN_OK != retVal) {
                mcx_log(LOG_ERROR, "%s: Could not store value for %s", logPrefix, channelName);
                return RETURN_ERROR;
            }
        } else if (info->dimension) {
            val = Fmu2ReadFmu2ArrayValue(logPrefix, info->type, channelName, info->dimension, info->unitString, fmiImport);
            if (!val) {
                mcx_log(LOG_ERROR, "%s: Could not create value for %s", logPrefix, channelName);
                return RETURN_ERROR;
            }

            val->SetChannel(val, info->channel);

            retVal = vals->PushBack(vals, (Object *)val);
            if (RETURN_OK != retVal) {
                mcx_log(LOG_ERROR, "%s: Could not store value for %s", logPrefix, channelName);
                return RETURN_ERROR;
            }
        } else { // scalar
            val = Fmu2ReadFmu2ScalarValue(logPrefix, info->type, channelName, info->unitString, fmiImport);
            if (!val) {
                mcx_log(LOG_ERROR, "%s: Could not create value for %s", logPrefix, channelName);
                return RETURN_ERROR;
            }

            val->SetChannel(val, info->channel);

            retVal = vals->PushBack(vals, (Object *)val);
            if (RETURN_OK != retVal) {
                mcx_log(LOG_ERROR, "%s: Could not store value for %s", logPrefix, channelName);
                return RETURN_ERROR;
            }
        }
    }

    return RETURN_OK;
}


static McxStatus Fmu2ReadChannelOut(ObjectContainer /* Fmu2Value */ * vals, Databus * db, fmi2_import_t * fmiImport, const char * logPrefix) {
    McxStatus retVal = RETURN_OK;

    size_t i = 0;

    DatabusInfo * dbInfo = DatabusGetOutInfo(db);
    size_t numChannels = DatabusInfoGetChannelNum(dbInfo);

    for (i = 0; i < numChannels; i++) {
        ChannelInfo * info = DatabusInfoGetChannel(dbInfo, i);

        Fmu2Value * val = NULL;
        fmi2_import_variable_t * var = NULL;

        const char * channelName = info->nameInTool;
        if (NULL == channelName) {
            channelName = ChannelInfoGetName(info);
        }

        if (ChannelInfoIsBinary(info)) {
            char * channelNameLo = mcx_string_merge(2, channelName, ".base.lo");
            char * channelNameHi = mcx_string_merge(2, channelName, ".base.hi");
            char * channelNameSize = mcx_string_merge(2, channelName, ".size");

            fmi2_import_variable_t * varLo = NULL;
            fmi2_import_variable_t * varHi = NULL;
            fmi2_import_variable_t * varSize = NULL;

            varLo = fmi2_import_get_variable_by_name(fmiImport, channelNameLo);
            if (!varLo) {
                mcx_log(LOG_ERROR, "%s: Could not get variable %s", logPrefix , channelNameLo);
                return RETURN_ERROR;
            }

            varHi = fmi2_import_get_variable_by_name(fmiImport, channelNameHi);
            if (!varHi) {
                mcx_log(LOG_ERROR, "%s: Could not get variable %s", logPrefix , channelNameHi);
                return RETURN_ERROR;
            }

            varSize = fmi2_import_get_variable_by_name(fmiImport, channelNameSize);
            if (!varSize) {
                mcx_log(LOG_ERROR, "%s: Could not get variable %s", logPrefix , channelNameSize);
                return RETURN_ERROR;
            }

            if (!ChannelTypeEq(&ChannelTypeInteger, Fmi2TypeToChannelType(fmi2_import_get_variable_base_type(varLo)))) {
                mcx_log(LOG_ERROR, "%s: Variable types of %s do not match", logPrefix , channelNameLo);
                mcx_log(LOG_ERROR, "%s: Expected: %s, Imported from FMU: %s",
                        ChannelTypeToString(&ChannelTypeInteger), ChannelTypeToString(Fmi2TypeToChannelType(fmi2_import_get_variable_base_type(varLo))));
                return RETURN_ERROR;
            }

            if (!ChannelTypeEq(&ChannelTypeInteger, Fmi2TypeToChannelType(fmi2_import_get_variable_base_type(varHi)))) {
                mcx_log(LOG_ERROR, "%s: Variable types of %s do not match", logPrefix , channelNameHi);
                mcx_log(LOG_ERROR, "%s: Expected: %s, Imported from FMU: %s",
                        ChannelTypeToString(&ChannelTypeInteger), ChannelTypeToString(Fmi2TypeToChannelType(fmi2_import_get_variable_base_type(varHi))));
                return RETURN_ERROR;
            }

            if (!ChannelTypeEq(&ChannelTypeInteger, Fmi2TypeToChannelType(fmi2_import_get_variable_base_type(varSize)))) {
                mcx_log(LOG_ERROR, "%s: Variable types of %s do not match", logPrefix , channelNameSize);
                mcx_log(LOG_ERROR, "%s: Expected: %s, Imported from FMU: %s",
                    ChannelTypeToString(&ChannelTypeInteger), ChannelTypeToString(Fmi2TypeToChannelType(fmi2_import_get_variable_base_type(varSize))));
                return RETURN_ERROR;
            }

            val = Fmu2ValueBinaryMake(channelName, varHi, varLo, varSize, NULL);
            if (!val) {
                mcx_log(LOG_ERROR, "%s: Could not set value for channel %s", logPrefix , channelName);
                return RETURN_ERROR;
            }

            mcx_free(channelNameLo);
            mcx_free(channelNameHi);
            mcx_free(channelNameSize);

            retVal = vals->PushBack(vals, (Object *)val);
            if (RETURN_OK != retVal) {
                mcx_log(LOG_ERROR, "%s: Could not store value for %s", logPrefix , channelName);
                return RETURN_ERROR;
            }
        } else if (info->dimension) {
            val = Fmu2ReadFmu2ArrayValue(logPrefix, info->type, channelName, info->dimension, info->unitString, fmiImport);

            retVal = vals->PushBack(vals, (Object *)val);
            if (RETURN_OK != retVal) {
                mcx_log(LOG_ERROR, "%s: Could not store value for %s", logPrefix, channelName);
                return RETURN_ERROR;
            }
        } else { // scalar
            val = Fmu2ReadFmu2ScalarValue(logPrefix, info->type, channelName, info->unitString, fmiImport);

            retVal = vals->PushBack(vals, (Object *)val);
            if (RETURN_OK != retVal) {
                mcx_log(LOG_ERROR, "%s: Could not store value for %s", logPrefix, channelName);
                return RETURN_ERROR;
            }
        }
    }

    return RETURN_OK;
}


static McxStatus Fmu2Read(Component * comp, ComponentInput * input, const struct Config * const config) {
    UNUSED(config);

    CompFMU * compFmu = (CompFMU *) comp;
    FmuInput * fmuInput = (FmuInput *) input;
    Fmu2CommonStruct * fmu2 = &compFmu->fmu2;
    FmuCommon * common = &compFmu->common;
    Databus * db = comp->GetDatabus(comp);
    ObjectContainer * vals = NULL;
    McxStatus retVal = RETURN_OK;

    retVal = Fmu2CommonStructRead(common, fmu2, fmi2_cosimulation, fmuInput);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Reading FMU Information failed");
        return RETURN_ERROR;
    }

    {
        retVal = Fmu2ReadChannelIn(fmu2->in, db, fmu2->fmiImport, comp->GetName(comp));
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_ERROR, "Could not read inports");
            return RETURN_ERROR;
        }
    }

    {
        retVal = Fmu2ReadChannelOut(fmu2->out, db, fmu2->fmiImport, comp->GetName(comp));
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_ERROR, "Could not read outports");
            return RETURN_ERROR;
        }
    }


    // TODO: consider moving localValues into Fmu2CommonStruct
    if (fmuInput->modelInternalVariables.defined) {
        compFmu->localValues = fmuInput->modelInternalVariables.value;
    }

    {
        ObjectContainer * vals_ = NULL;
        vals = Fmu2ReadTunableParams(fmu2->fmiImport);
        if (!vals) {
            ComponentLog(comp, LOG_ERROR, "Could not get tunable parameters");
            return RETURN_ERROR;
        }
        // Remove all tunable parameters that are used as inputs
        vals_ = vals->FilterCtx(vals, Fmu2ValueIsNotContainedInObjectContainerPred, fmu2->in);
        if (!vals_) {
            return RETURN_ERROR;
        }
        retVal = fmu2->tunableParams->Append(fmu2->tunableParams, vals_);
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_ERROR, "Could not add tunable parameters");
            return RETURN_ERROR;
        }
        object_destroy(vals);
        object_destroy(vals_);
    }

    // TODO: rename localValues to show that this is a flag
    if (compFmu->localValues) {
        vals = Fmu2ReadLocalVariables(compFmu->fmu2.fmiImport);
        if (!vals) {
            ComponentLog(comp, LOG_ERROR, "Could not get local variables");
            return RETURN_ERROR;
        }

        retVal = compFmu->fmu2.localValues->Append(compFmu->fmu2.localValues, vals);
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_ERROR, "Could not add local variables");
            return RETURN_ERROR;
        }
        object_destroy(vals);
    }

    /* read the parameters */
    {
        ParametersInput * parametersInput = input->parameters;

        if (parametersInput) {
            retVal = Fmu2ReadParams(
                fmu2->params,
                fmu2->arrayParams,
                parametersInput,
                compFmu->fmu2.fmiImport,
                NULL
            );
            if (RETURN_OK != retVal) {
                ComponentLog(comp, LOG_ERROR, "Could not read parameters");
                return RETURN_ERROR;
            }

        }
    }

    {
        ParametersInput * parametersInput = input->initialValues;

        if (parametersInput) {
            retVal = Fmu2ReadParams(fmu2->initialValues, NULL, parametersInput, compFmu->fmu2.fmiImport, NULL);
            if (RETURN_OK != retVal) {
                ComponentLog(comp, LOG_ERROR, "Could not read initial values");
                return RETURN_ERROR;
            }
        }
    }


    {
        retVal = Fmu2UpdateTunableParamValues(fmu2->tunableParams, fmu2->params);
        if (retVal == RETURN_ERROR) {
            ComponentLog(comp, LOG_ERROR, "Updating tunable parameter values failed");
            return RETURN_ERROR;
        }
    }

    retVal = Fmu2CommonStructSetup(common, fmu2, fmi2_cosimulation);
    if (RETURN_ERROR == retVal) {
        ComponentLog(comp, LOG_ERROR, "Setting up FMU Information failed");
        return RETURN_ERROR;
    } else if (RETURN_WARNING == retVal) {
        ComponentLog(comp, LOG_WARNING, "Setting up FMU Information return with a warning");
        // warning is ok
    }

    return RETURN_OK;
}

static McxStatus Fmu2CollectConnectedInputs(Component * comp) {
    CompFMU * compFmu = (CompFMU *)comp;
    size_t num = compFmu->fmu2.in->Size(compFmu->fmu2.in);

    for (size_t i = 0; i < num; i++) {
        Fmu2Value * val = (Fmu2Value *)compFmu->fmu2.in->At(compFmu->fmu2.in, i);

        if (val->channel && val->channel->IsConnected(val->channel)) {
            McxStatus retVal = compFmu->fmu2.connectedIn->PushBack(compFmu->fmu2.connectedIn, (Object *)val);
            if (RETURN_ERROR == retVal) {
                return RETURN_ERROR;
            }
        }
    }

    return RETURN_OK;
}

static McxStatus Fmu2Initialize(Component * comp, size_t group, double startTime) {
    CompFMU * compFmu = (CompFMU *) comp;
    int a = FALSE;

    Fmu2CommonStruct * fmu2 = &compFmu->fmu2;

    fmi2_status_t status = fmi2_status_ok;
    double defaultTolerance = 0.0;

    McxStatus retVal = RETURN_OK;

    // Set variables
    retVal = Fmu2SetVariableArray(fmu2, fmu2->params);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Setting params failed");
        return RETURN_ERROR;
    }

    retVal = Fmu2SetVariableArray(fmu2, fmu2->initialValues);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Setting initialValues failed");
        return RETURN_ERROR;
    }

    retVal = Fmu2SetVariableArray(fmu2, fmu2->in);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Setting inChannels failed");
        return RETURN_ERROR;
    }

    defaultTolerance = fmi2_import_get_default_experiment_tolerance(fmu2->fmiImport);

    compFmu->lastCommunicationTimePoint = startTime;
    mcx_signal_handler_set_function("fmi2_import_setup_experiment");
    status = fmi2_import_setup_experiment(fmu2->fmiImport,
                                          fmi2_false, /* toleranceDefine */
                                          defaultTolerance,
                                          startTime, /* startTime */
                                          fmi2_false, /* stopTimeDefined */
                                          0.0 /* stopTime */);
    mcx_signal_handler_unset_function();

    if (fmi2_status_ok != status) {
        ComponentLog(comp, LOG_ERROR, "SetupExperiment failed");
        return RETURN_ERROR;
    }

    // Initialization Mode
    mcx_signal_handler_set_function("fmi2_import_enter_initialization_mode");
    status = fmi2_import_enter_initialization_mode(fmu2->fmiImport);
    mcx_signal_handler_unset_function();
    if (fmi2_status_ok != status) {
        ComponentLog(comp, LOG_ERROR, "Could not enter Initialization Mode");
        return RETURN_ERROR;
    }

    // Set variables
    retVal = Fmu2SetVariableArray(fmu2, fmu2->initialValues);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Setting initialValues failed");
        return RETURN_ERROR;
    }

    retVal = Fmu2SetVariableArray(fmu2, fmu2->in);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Setting inChannels failed");
        return RETURN_ERROR;
    }

    // Get outputs (this triggers the computation)
    retVal = Fmu2GetVariableArray(fmu2, fmu2->out);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Initialization computation failed");
        return RETURN_ERROR;
    }

    // local variables
    if (compFmu->localValues) {
        retVal = Fmu2GetVariableArray(fmu2, fmu2->localValues);
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_ERROR, "Retrieving local variables failed");
            return RETURN_ERROR;
        }
    }

    fmu2->runOk = fmi2_true;

    return RETURN_OK;
}

static McxStatus Fmu2ExitInitializationMode(Component *comp) {
    CompFMU *compFmu = (CompFMU*)comp;

    mcx_signal_handler_set_function("fmi2_import_exit_initialization_mode");
    fmi2_status_t status = fmi2_import_exit_initialization_mode(compFmu->fmu2.fmiImport);
    mcx_signal_handler_unset_function();
    if (fmi2_status_ok != status) {
        ComponentLog(comp, LOG_ERROR, "Could not exit Initialization Mode");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus Fmu2DoStep(Component * comp, size_t group, double time, double deltaTime, double endTime, int isNewStep) {
    CompFMU * compFmu = (CompFMU *) comp;
    Fmu2CommonStruct * fmu2 = &compFmu->fmu2;

    McxStatus retVal;
    fmi2_status_t status = fmi2_status_ok;

    TimeSnapshotStart(&comp->data->rtData.funcTimings.rtInput);
    // Set variables
    retVal = Fmu2SetVariableArray(fmu2, fmu2->connectedIn);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Setting inChannels failed");
        return RETURN_ERROR;
    }
    TimeSnapshotEnd(&comp->data->rtData.funcTimings.rtInput);

    // Do calculations
    mcx_signal_handler_set_function("fmi2_import_do_step");
    status = fmi2_import_do_step(fmu2->fmiImport, compFmu->lastCommunicationTimePoint, deltaTime, fmi2_true);
    mcx_signal_handler_unset_function();
    if (fmi2_status_ok == status) {
        // fine
    } else if (fmi2_status_discard == status) {
        fmi2_status_t fmi2status;
        fmi2_boolean_t isTerminated = fmi2_false;

        mcx_signal_handler_set_function("fmi2_import_get_boolean_status");
        fmi2status = fmi2_import_get_boolean_status(fmu2->fmiImport, fmi2_terminated, &isTerminated);
        mcx_signal_handler_unset_function();
        if (fmi2_status_ok == fmi2status) {
            if (fmi2_true == isTerminated) {
                comp->SetIsFinished(comp);
            } else {
                ComponentLog(comp, LOG_ERROR, "FMU discarded DoStep but has not finished yet");
                return RETURN_ERROR;
            }
        } else if (fmi2_status_discard == fmi2status) {
            ComponentLog(comp, LOG_WARNING, "FMU discarded DoStep but it returned no status whether it terminated deliberately or not");
            comp->SetIsFinished(comp);
        } else if (fmi2_status_error == fmi2status || fmi2_status_fatal == fmi2status) {
            ComponentLog(comp, LOG_WARNING, "FMU discarded DoStep but the status-function of the FMU returned an error");
            comp->SetIsFinished(comp);
        } else if (fmi2_status_warning == fmi2status || fmi2_status_pending == fmi2status) {
            // should never happen according to fmi2.0 standard
            ComponentLog(comp, LOG_ERROR, "FMU discarded DoStep but the status-function returned unexpectedly");
            return RETURN_ERROR;
        } else {
            // should never happen according to fmi2.0 standard
            ComponentLog(comp, LOG_ERROR, "FMU discarded DoStep but the status-function returned unexpectedly");
            return RETURN_ERROR;
        }
    } else if (fmi2_status_error == status) {
        ComponentLog(comp, LOG_ERROR, "Computation failed");
        return RETURN_ERROR;
    } else if (fmi2_status_fatal == status) {
        ComponentLog(comp, LOG_ERROR, "Computation failed (fatal)");
        return RETURN_ERROR;
    } else if (fmi2_status_warning == status) {
        ComponentLog(comp, LOG_WARNING, "Computation returned with warning");
    }

    compFmu->lastCommunicationTimePoint += deltaTime;

    return RETURN_OK;
}

static McxStatus Read(Component * comp, ComponentInput * input, const struct Config * const config) {
    CompFMU * compFmu = (CompFMU *) comp;
    InputElement * element = (InputElement *) input;
    FmuCommon * common = &compFmu->common;
    McxStatus retVal = RETURN_OK;
    double deltaTime = 0.;
    FmuInput * fmuInput = (FmuInput *) input;

    common->instanceName = mcx_string_copy(comp->GetName(comp));

    retVal = FmuCommonRead(common, fmuInput);
    if (RETURN_ERROR == retVal) {
        ComponentLog(comp, LOG_ERROR, "Could not read FMU");
        return RETURN_ERROR;
    }
    retVal = FmuCommonSetup(common);
    if (RETURN_ERROR == retVal) {
        ComponentLog(comp, LOG_ERROR, "Could not setup FMU");
        return RETURN_ERROR;
    }

        // no path to an extracted fmu given -> create a path for extraction
        retVal = CreateFmuExtractPath(common, comp->GetName(comp), config);
        if (RETURN_OK != retVal) {
            ComponentLog(comp, LOG_ERROR, "Could not get extraction path");
            return RETURN_ERROR;
        }
    retVal = FmuOpen(common, config);
    if (RETURN_ERROR == retVal) {
        ComponentLog(comp, LOG_ERROR, "Could not open FMU");
        return RETURN_ERROR;
    }

    if (common->version == fmi_version_1_enu) {
        comp->Read = Fmu1Read;
        comp->SetupDatabus = Fmu1SetupDatabus;
        comp->Initialize = Fmu1Initialize;
        comp->DoStep = Fmu1DoStep;

    } else if (common->version == fmi_version_2_0_enu) {
        comp->Read = Fmu2Read;
        comp->SetupDatabus = Fmu2SetupDatabus;
        comp->Initialize = Fmu2Initialize;
        comp->DoStep = Fmu2DoStep;
        comp->ExitInitializationMode = Fmu2ExitInitializationMode;
        comp->GetInOutGroupsInitialDependency = Fmu2GetInOutGroupsInitialDependency;

        comp->SetIsPartOfInitCalculation(comp, TRUE);

    } else {
        ComponentLog(comp, LOG_ERROR, "Unknown FMU Version: %s", fmi_version_to_string(common->version));
        return RETURN_ERROR;
    }

    return comp->Read(comp, input, config);
}

static ChannelMode GetInChannelDefaultMode(struct Component * comp) {
    return CHANNEL_OPTIONAL;
}

static McxStatus SetDependenciesFMU2(CompFMU *compFmu, struct Dependencies *deps) {
    Component * comp = (Component *) compFmu;
    McxStatus ret_val = RETURN_OK;

    size_t *start_index = NULL;
    size_t *dependency = NULL;
    char   *factor_kind = NULL;

    size_t i = 0, j = 0, k = 0;
    size_t num_dependencies = 0;
    size_t dep_idx = 0;

    SizeTSizeTMap *dependencies_to_in_channels = (SizeTSizeTMap*)object_create(SizeTSizeTMap);
    // dictionary used to store input connection information
    SizeTSizeTMap *in_channel_connectivity = (SizeTSizeTMap*)object_create(SizeTSizeTMap);
    SizeTSizeTMap *unknowns_to_out_channels = (SizeTSizeTMap*)object_create(SizeTSizeTMap);
    // dictionary used to later find ommitted <Unknown> elements
    SizeTSizeTMap *processed_out_channels = (SizeTSizeTMap*)object_create(SizeTSizeTMap);

    // get dependency information via the fmi library
    fmi2_import_variable_list_t * init_unknowns = fmi2_import_get_initial_unknowns_list(compFmu->fmu2.fmiImport);
    size_t num_init_unknowns = fmi2_import_get_variable_list_size(init_unknowns);

    fmi2_import_get_initial_unknowns_dependencies(compFmu->fmu2.fmiImport, &start_index, &dependency, &factor_kind);

    // the dependency information in <InitialUnknowns> is encoded via variable indices in modelDescription.xml
    // our dependency matrix uses channel indices
    // to align those 2 index types we use helper dictionaries which store the mapping between them

    // map each dependency index to an input channel index
    ObjectContainer *in_vars = compFmu->fmu2.in;
    size_t num_in_vars = in_vars->Size(in_vars);

    Databus * db = comp->GetDatabus(comp);
    DatabusInfo * db_info = DatabusGetInInfo(db);
    size_t num_in_channels = DatabusInfoGetChannelNum(db_info);

    for (i = 0; i < num_in_vars; ++i) {
        Fmu2Value *val = (Fmu2Value *)in_vars->At(in_vars, i);
        ChannelInfo * info = DatabusInfoGetChannel(db_info, i);
        if (DatabusChannelInIsValid(db, k) && info->connected) {
            // key i in the map means channel i is connected
            in_channel_connectivity->Add(in_channel_connectivity, i, 1 /* true */);
        }

        if (val->data->type == FMU2_VALUE_SCALAR) {
            fmi2_import_variable_t *var = val->data->data.scalar;
            size_t idx = fmi2_import_get_variable_original_order(var) + 1;
            dependencies_to_in_channels->Add(dependencies_to_in_channels, idx, i);
        }
    }

    // <InitialUnknowns> element is not present in modelDescription.xml
    // The dependency matrix consists of only 1 (if input is connected)
    if (start_index == NULL) {
        for (i = 0; i < GetDependencyNumOut(deps); ++i) {
            for (j = 0; j < GetDependencyNumIn(deps); ++j) {
                SizeTSizeTElem * elem = in_channel_connectivity->Get(in_channel_connectivity, j);
                if (elem) {
                    ret_val = SetDependency(deps, j, i, DEP_DEPENDENT);
                    if (RETURN_OK != ret_val) {
                        goto cleanup;
                    }
                }
            }
        }

        goto cleanup;
    }

    // map each initial_unkown index to an output channel index
    ObjectContainer *out_vars = compFmu->fmu2.out;
    size_t num_out_vars = out_vars->Size(out_vars);

    for (i = 0; i < num_out_vars; ++i) {
        Fmu2Value *val = (Fmu2Value *)out_vars->At(out_vars, i);

        if (val->data->type == FMU2_VALUE_SCALAR) {
            fmi2_import_variable_t *var = val->data->data.scalar;
            size_t idx = fmi2_import_get_variable_original_order(var) + 1;
            unknowns_to_out_channels->Add(unknowns_to_out_channels, idx, i);
        }
    }

    // fill up the dependency matrix
    for (i = 0; i < num_init_unknowns; ++i) {
        fmi2_import_variable_t *init_unknown = fmi2_import_get_variable(init_unknowns, i);
        size_t init_unknown_idx = fmi2_import_get_variable_original_order(init_unknown) + 1;

        SizeTSizeTElem * out_pair = unknowns_to_out_channels->Get(unknowns_to_out_channels, init_unknown_idx);
        if (out_pair == NULL) {
            continue;      // in case some variables are ommitted from the input file
        }

        processed_out_channels->Add(processed_out_channels, out_pair->value, 1);

        num_dependencies = start_index[i + 1] - start_index[i];
        for (j = 0; j < num_dependencies; ++j) {
            dep_idx = dependency[start_index[i] + j];
            if (dep_idx == 0) {
                // The <Unknown> element does not explicitly define a `dependencies` attribute
                // In this case it depends on all inputs
                for (k = 0; k < num_in_channels; ++k) {
                    SizeTSizeTElem * elem = in_channel_connectivity->Get(in_channel_connectivity, k);
                    if (elem) {
                        ret_val = SetDependency(deps, k, out_pair->value, DEP_DEPENDENT);
                        if (RETURN_OK != ret_val) {
                            goto cleanup;
                        }
                    }
                }
            } else {
                // The <Unknown> element explicitly defines its dependencies
                SizeTSizeTElem * in_pair = dependencies_to_in_channels->Get(dependencies_to_in_channels, dep_idx);

                if (in_pair) {
                    SizeTSizeTElem * elem = in_channel_connectivity->Get(in_channel_connectivity, in_pair->value);
                    if (elem) {
                        ret_val = SetDependency(deps, in_pair->value, out_pair->value, DEP_DEPENDENT);
                        if (RETURN_OK != ret_val) {
                            goto cleanup;
                        }
                    }
                }
            }
        }
    }

    // Initial unknowns which are ommitted from the <InitialUnknowns> element in
    // modelDescription.xml file depend on all inputs
    for (i = 0; i < num_out_vars; ++i) {
        if (processed_out_channels->Get(processed_out_channels, i) == NULL) {
            Fmu2Value *val = (Fmu2Value *)out_vars->At(out_vars, i);

            if (fmi2_import_get_initial(val->data->data.scalar) != fmi2_initial_enu_exact) {
                for (k = 0; k < num_in_channels; ++k) {
                    SizeTSizeTElem * elem = in_channel_connectivity->Get(in_channel_connectivity, k);
                    if (elem) {
                        ret_val = SetDependency(deps, k, i, DEP_DEPENDENT);
                        if (RETURN_OK != ret_val) {
                            goto cleanup;
                        }
                    }
                }
            }
        }
    }

cleanup:    // free dynamically allocated objects
    object_destroy(dependencies_to_in_channels);
    object_destroy(in_channel_connectivity);
    object_destroy(unknowns_to_out_channels);
    object_destroy(processed_out_channels);
    fmi2_import_free_variable_list(init_unknowns);

    return ret_val;
}

static struct Dependencies* Fmu2GetInOutGroupsInitialDependency(const Component * comp) {
    CompFMU *comp_fmu = (CompFMU *)comp;
    struct Dependencies *dependencies = NULL;

    if (comp_fmu->fmu2.fmiImport != NULL) {
        Databus * db = comp->GetDatabus(comp);
        DatabusInfo * dbInfo = DatabusGetInInfo(db);
        size_t num_in = comp->GetNumInChannels(comp);
        size_t num_out = comp->GetNumOutChannels(comp);

        if ( 0 == num_out ) {  // create dummy output-dep, so that internal variables of the FMU get evaluated in the right order
            McxStatus retVal = RETURN_OK;
            size_t j;
            size_t dummy_num_out = 1;
            dependencies = DependenciesCreate(num_in, dummy_num_out);
            for (j = 0; j < num_in; ++j) {
                if (DatabusChannelInIsValid(db, j)) {
                    retVal = SetDependency(dependencies, j, 0, DEP_DEPENDENT);
                    if (RETURN_OK != retVal) {
                        mcx_log(LOG_ERROR, "Initial dependency matrix for %s could not be created", comp->GetName(comp));
                        return NULL;
                    }
                }
            }
        } else {
            dependencies = DependenciesCreate(num_in, num_out);
            if (SetDependenciesFMU2(comp_fmu, dependencies) != RETURN_OK) {
                mcx_log(LOG_ERROR, "Initial dependency matrix for %s could not be created", comp->GetName(comp));
                return NULL;
            }
        }
    }

    return dependencies;
}

static McxStatus Fmu2UpdateOutChannels(Component * comp) {
    CompFMU *comp_fmu = (CompFMU *)comp;
    Fmu2CommonStruct * fmu2 = &comp_fmu->fmu2;
    McxStatus retVal;

    TimeSnapshotStart(&comp->data->rtData.funcTimings.rtOutput);
    retVal = Fmu2GetVariableArray(fmu2, fmu2->out);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Initialization computation failed");
        return RETURN_ERROR;
    }

    retVal = Fmu2GetVariableArray(fmu2, fmu2->localValues);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Initialization computation failed");
        return RETURN_ERROR;
    }
    TimeSnapshotEnd(&comp->data->rtData.funcTimings.rtOutput);

    return RETURN_OK;
}

static McxStatus Fmu2UpdateInChannels(Component * comp) {
    CompFMU *comp_fmu = (CompFMU *)comp;
    Fmu2CommonStruct * fmu2 = &comp_fmu->fmu2;
    McxStatus retVal;

    retVal = Fmu2SetVariableArray(fmu2, fmu2->in);
    if (RETURN_OK != retVal) {
        ComponentLog(comp, LOG_ERROR, "Initialization computation failed");
        return RETURN_ERROR;
    }
    return RETURN_OK;
}

static void CompFMUDestructor(CompFMU * compFmu) {
    Fmu1CommonStruct * fmu1 = & compFmu->fmu1;
    Fmu2CommonStruct * fmu2 = & compFmu->fmu2;
    FmuCommon * common = & compFmu->common;

    // TOOD: Move this to the common struct destructors
    if (fmu1->fmiImport) {
        if (fmi1_true == fmu1->runOk) {
            mcx_signal_handler_set_function("fmi1_import_terminate_slave");
            fmi1_import_terminate_slave(fmu1->fmiImport);
            mcx_signal_handler_unset_function();
        }

        if (fmi1_true == fmu1->instantiateOk) {
            mcx_signal_handler_set_function("fmi1_import_free_slave_instance");
            fmi1_import_free_slave_instance(fmu1->fmiImport);
            mcx_signal_handler_unset_function();
        }
    }

    if (fmu2->fmiImport) {
        if (fmi2_true == fmu2->runOk) {
            mcx_signal_handler_set_function("fmi2_import_terminate");
            fmi2_import_terminate(fmu2->fmiImport);
            mcx_signal_handler_unset_function();
        }

        if (fmi2_true == fmu2->instantiateOk) {
            mcx_signal_handler_set_function("fmi2_import_free_instance");
            fmi2_import_free_instance(fmu2->fmiImport);
            mcx_signal_handler_unset_function();
        }
    }

    Fmu1CommonStructDestructor(fmu1);
    Fmu2CommonStructDestructor(fmu2);

    FmuCommonDestructor(common);
}

static Component * CompFMUCreate(Component * comp) {
    CompFMU * self = (CompFMU *) comp;

    // map to local functions
    comp->GetInChannelDefaultMode = GetInChannelDefaultMode;
    comp->Read       = Read;

    comp->SetupDatabus = Fmu2SetupDatabus;
    comp->Initialize = Fmu2Initialize;
    comp->DoStep     = Fmu2DoStep;
    comp->Setup      = CompFmuSetup;

    comp->OnConnectionsDone = Fmu2CollectConnectedInputs;

    comp->UpdateInChannels = Fmu2UpdateInChannels;
    comp->UpdateInitialOutChannels = Fmu2UpdateOutChannels;
    comp->UpdateOutChannels = Fmu2UpdateOutChannels;

    self->localValues = FALSE;
    self->lastCommunicationTimePoint = 0.;

    FmuCommonInit(&self->common);

    if (Fmu1CommonStructInit(&self->fmu1) == RETURN_ERROR) {
        ComponentLog(comp, LOG_ERROR, "Could not initialize FMU1 structure");
        return NULL;
    }
    if (Fmu2CommonStructInit(&self->fmu2) == RETURN_ERROR) {
        ComponentLog(comp, LOG_ERROR, "Could not initialize FMU2 structure");
        return NULL;
    }

    return comp;
}

OBJECT_CLASS(CompFMU, Component);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif /* __cplusplus */