/********************************************************************************
 * Copyright (c) 2024 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "CentralParts.h"
#include "components/comp_dcp.h"

#if defined (ENABLE_DCP)
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <asio.hpp>

#include "dcp/master.hpp"

#include "core/Databus.h"
#include "core/Model.h"
#include "core/channels/ChannelInfo.h"
#include "reader/model/components/specific_data/DcpInput.h"

#include "util/compare.h"

#include <dcp/xml/DcpSlaveDescriptionReader.hpp>
#include <dcp/xml/DcpSlaveDescriptionElements.hpp>

#include "scope_guard.hpp"

#include "channel_value/ChannelValue.h"

struct DCPParameter;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/*************************************************************************************************/
/*                                   Common DCP master                                           */
/*************************************************************************************************/
static dcp::Master *master = NULL;

using namespace std;

/*************************************************************************************************/
/*                                      DCP component                                            */
/*************************************************************************************************/
typedef struct CompDcp {
    Component _;  /* super class first */

    shared_ptr<dcp::SlaveHandle> * slave;
    std::shared_ptr<SlaveDescription_t> * slave_desc;

    ChannelValue * inValues;
    ChannelValue * outValues;

    std::vector<std::shared_ptr<DCPParameter>> * params_fixed;
    std::vector<std::shared_ptr<DCPParameter>> * params_tunable;

} CompDcp;

#ifdef __cplusplus
}
#endif // __cplusplus


enum class VariableType {
    input,
    output,
    parameter,
    structural_parameter,
    all
};

static const Variable_t * find_variable_by_name(const SlaveDescription_t& slave_desc,
                                                const std::string & name,
                                                VariableType type = VariableType::all)
{
    for (const Variable_t & variable : slave_desc.Variables) {
        if (type == VariableType::input && !variable.Input) { continue; }
        if (type == VariableType::output && !variable.Output) { continue; }
        if (type == VariableType::parameter && !variable.Parameter) { continue; }
        if (type == VariableType::structural_parameter && !variable.StructuralParameter) { continue; }

        if (variable.name == name) {
            return &variable;
        }
    }

    return nullptr;
}

static const char * dcp_type_c_str(DcpDataType dcp_type) {
    std::ostringstream oss;
    oss << dcp_type;
    return oss.str().c_str();
}

static bool supported_type(DcpDataType dcp_type) {
    // the current implementation handles only these data types
    return (dcp_type == DcpDataType::float64 ||
            dcp_type == DcpDataType::int32);
}

static bool compatible_types(DcpDataType dcp_type, ChannelType * channel_type) {
    switch (channel_type->con) {
    case CHANNEL_DOUBLE:
        return (dcp_type == DcpDataType::float32 ||
                dcp_type == DcpDataType::float64);
    case CHANNEL_INTEGER:
    case CHANNEL_BOOL:  // our CHANNEL_BOOL is basically an int
        return (dcp_type == DcpDataType::int8 ||
                dcp_type == DcpDataType::int16 ||
                dcp_type == DcpDataType::int32 ||
                dcp_type == DcpDataType::int64 ||
                dcp_type == DcpDataType::uint8 ||
                dcp_type == DcpDataType::uint16 ||
                dcp_type == DcpDataType::uint32 ||
                dcp_type == DcpDataType::uint64);
    case CHANNEL_STRING:
        return dcp_type == DcpDataType::string;
    case CHANNEL_BINARY:
    case CHANNEL_BINARY_REFERENCE:
        return dcp_type == DcpDataType::binary;
    default:
        return false;
    }
}


struct DCPParameter {
public:
    DCPParameter(std::shared_ptr<dcp::SlaveHandle> slave,
                 const std::string& name,
                 const std::string& unit,
                 uint64_t vr,
                 DcpDataType type,
                 bool is_tunable,
                 ChannelValue& val) {
        this->val_ = val;
        this->unit_ = unit;
        this->slave_ = slave;

        // create and register the parameter with the slave
        if (is_tunable) {
            this->param_ = slave->add_tunable_param(name, vr, type, ChannelValueDataPointer(&this->val_));
        } else {
            this->param_ = slave->add_fixed_param(name, vr, type, ChannelValueDataPointer(&this->val_));
        }
    }

    // getters
    ChannelType * channel_type() { return ChannelValueType(&this->val_); }
    ChannelValue channel_value() { return this->val_; }
    const std::string& name() { return this->param_->name(); }
    const std::string& unit() { return this->unit_; }

    // setters
    McxStatus set_value_from_reference(void * value) { return ChannelValueSetFromReference(&this->val_, value); }

    // sends parameter data to the slave
    McxStatus send_to_slave() {
        return master->sendParameter(this->slave_->id(), this->param_);
    }

private:
    ChannelValue val_;
    std::string unit_;
    std::shared_ptr<dcp::SlaveHandle> slave_;

    std::shared_ptr<dcp::Parameter> param_;
};


static McxStatus ReadSlaveParameters(Component * comp,
                                     std::shared_ptr<dcp::SlaveHandle> slave,
                                     ParametersInput * input) {
    CompDcp * self = reinterpret_cast<CompDcp *>(comp);
    const SlaveDescription_t & slave_desc = *self->slave_desc->get();
    McxStatus ret_val = RETURN_OK;

    if (!input) {
        return RETURN_OK;
    }

    for (size_t i = 0; i < input->parameters->Size(input->parameters); i++) {
        ParameterInput * paramInput = (ParameterInput *) input->parameters->At(input->parameters, i);
        ScalarParameterInput * scalarParamInput = NULL;

        if (paramInput->type != PARAMETER_SCALAR) {
            mcx_log(LOG_WARNING, "Slave %ud: Ignoring array parameter '%s'", slave->id(), paramInput->parameter.arrayParameter->name);
            continue;
        }
        scalarParamInput = paramInput->parameter.scalarParameter;

        if (!scalarParamInput->value.defined) {
            mcx_log(LOG_ERROR, "Parameter %s: No value defined", scalarParamInput->name);
            return RETURN_ERROR;
        }

        ChannelValue channel_value;
        ChannelValueInit(&channel_value, ChannelTypeClone(scalarParamInput->type));
        if (RETURN_OK != ChannelValueSetFromReference(&channel_value, &scalarParamInput->value.value)) {
            ComponentLog(comp, LOG_ERROR, "Parameter '%s': Could not set channel value", scalarParamInput->name);
            return RETURN_ERROR;
        }

        // get value reference of the parameter based on its name
        const Variable_t * variable = find_variable_by_name(slave_desc, scalarParamInput->name, VariableType::parameter);
        if (variable == nullptr) {
            ComponentLog(comp, LOG_ERROR, "Parameter '%s': Not defined in the slave description file", scalarParamInput->name);
            return RETURN_ERROR;
        }

        // read the DCP type from the slave description
        DcpDataType type = slavedescription::getDataType(slave_desc, variable->valueReference);

        // check that the DCP type is supported
        if (!supported_type(type)) {
            ComponentLog(comp, LOG_ERROR, "Parameter '%s' (value reference: %llu): Not supported DCP type '%s' "
                         "from the slave description file", scalarParamInput->name, variable->valueReference, dcp_type_c_str(type));
            return RETURN_ERROR;
        }

        // check that the DCP type matches our type
        if (!compatible_types(type, scalarParamInput->type)) {
            ComponentLog(comp, LOG_ERROR, "Parameter '%s' (value reference: %llu): Type is not compatible with the DCP type '%s' "
                         "from the slave description file", scalarParamInput->name, variable->valueReference, dcp_type_c_str(type));
            return RETURN_ERROR;
        }

        MCX_DEBUG_LOG("%s: Adding parameter {name=%s, vr=%llu, type=%s}",
                      comp->GetName(comp), scalarParamInput->name, variable->valueReference, dcp_type_c_str(type));
        try {
            auto param = std::make_shared<DCPParameter>(slave,
                                                        scalarParamInput->name,
                                                        scalarParamInput->unit != nullptr ? scalarParamInput->unit : "",
                                                        variable->valueReference,
                                                        type,
                                                        variable->variability == Variability::TUNABLE,
                                                        channel_value);
            if (variable->variability == Variability::TUNABLE) {
                self->params_tunable->push_back(param);
            } else {
                self->params_fixed->push_back(param);
            }
        } catch (std::bad_alloc) {
            ComponentLog(comp, LOG_ERROR, "Not enough memory for parameter allocation");
            return RETURN_ERROR;
        }
    }

    return ret_val;
}

static inline void fill_vr_mapping(std::map<uint64_t, size_t>& mapping,
                                   const std::vector<dcp::Port>& ports) {
    for (size_t i = 0; i < ports.size(); i++) {
        mapping[ports[i].vr()] = i;
    }
}

static McxStatus CollectPorts(CompDcp * comp_dcp,
                              DatabusInfo * db_info,
                              std::vector<dcp::Port>& ports,
                              ChannelValue * values,
                              VariableType variable_type) {
    Component * comp = reinterpret_cast<Component *>(comp_dcp);
    const SlaveDescription_t & slave_desc = *comp_dcp->slave_desc->get();

    size_t num_channels = DatabusInfoGetChannelNum(db_info);
    size_t num_ports = ports.size();
    for (size_t i = 0; i < num_channels; ++i) {
        ChannelInfo * info = DatabusInfoGetChannel(db_info, i);

        // use `name_in_model` to find the variable in the slave description file
        // in case `name_in_model` is not present, fall back to `name`
        const char * name_in_model = info->nameInTool ? info->nameInTool : ChannelInfoGetName(info);

        // get value reference of the port
        const Variable_t * variable = find_variable_by_name(slave_desc, name_in_model, variable_type);
        if (variable == nullptr) {
            ComponentLog(comp, LOG_ERROR, "Port '%s' does not have a corresponding variable (named '%s') "
                         "in the slave description file", ChannelInfoGetName(info), name_in_model);
            return RETURN_ERROR;
        }

        // read the DCP type from the slave description
        DcpDataType type = slavedescription::getDataType(slave_desc, variable->valueReference);

        // check that the DCP type is supported
        if (!supported_type(type)) {
            ComponentLog(comp, LOG_ERROR, "Port '%s' (value reference: %llu): Not supported DCP type '%s' "
                         "from the slave description file", ChannelInfoGetName(info), variable->valueReference, dcp_type_c_str(type));
            return RETURN_ERROR;
        }

        // check that the DCP type matches our type
        if (!compatible_types(type, info->type)) {
            ComponentLog(comp, LOG_ERROR, "Port '%s' (value reference: %llu): Type is not compatible with the DCP type '%s' "
                         "from the slave description file", ChannelInfoGetName(info), variable->valueReference, dcp_type_c_str(type));
            return RETURN_ERROR;
        }

        // create port handle
        ports.emplace_back(*comp_dcp->slave, variable->valueReference, type);
        ports[num_ports].set_value_reference(ChannelValueDataPointer(&values[i]));
        ++num_ports;
    }

    return RETURN_OK;
}


#ifdef __cplusplus
extern "C" {
#endif // __cplusplus


static McxStatus Read(Component * comp, ComponentInput * input, const struct Config * const config) {
    CompDcp * comp_dcp = reinterpret_cast<CompDcp *>(comp);
    DcpInput * dcp_input = reinterpret_cast<DcpInput *>(input);

    // parse slave description file
    try {
        *comp_dcp->slave_desc = readSlaveDescription(dcp_input->slaveDescPath);
    } catch (std::invalid_argument& e) {
        ComponentLog(comp, LOG_ERROR, "Parsing slave description file '%s' failed: %s",
                     dcp_input->slaveDescPath, e.what());
        return RETURN_ERROR;
    }

    // read other specific data parameters
    const SlaveDescription_t & slave_desc = *comp_dcp->slave_desc->get();

    size_t numerator = dcp_input->numerator.defined ? dcp_input->numerator.value : 10;
    size_t denominator = dcp_input->denominator.defined ? dcp_input->denominator.value : 1000;

    // check that the step size defined via numerator/denominator matches the coupling step size
    double num_denom_time_step_size = 1.0 * numerator / denominator;
    if (comp->HasOwnTime(comp) && !double_eq(comp->GetTimeStep(comp), num_denom_time_step_size)) {
        ComponentLog(comp, LOG_ERROR, "Coupling time step size %f does not match the step size %f "
                     "defined via numerator/denominator (%u/%u)",
                     comp->GetTimeStep(comp), num_denom_time_step_size, numerator, denominator);
        return RETURN_ERROR;
    }

    if (comp->SetTimeStep(comp, num_denom_time_step_size) == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    uint16_t port = 0;
    if (dcp_input->port.defined) {
        if (port < 0 || port > std::numeric_limits<uint16_t>::max()) {
            ComponentLog(comp, LOG_ERROR, "Control port outside of allowed range [%hu, %hu]",
                         std::numeric_limits<uint16_t>::min(), std::numeric_limits<uint16_t>::max());
            return RETURN_ERROR;
        }
        port = static_cast<uint16_t>(dcp_input->port.value);
    } else if (slave_desc.TransportProtocols.UDP_IPv4 &&
               slave_desc.TransportProtocols.UDP_IPv4->Control &&
               slave_desc.TransportProtocols.UDP_IPv4->Control->port) {
        port = *slave_desc.TransportProtocols.UDP_IPv4->Control->port;
    } else {
        ComponentLog(comp, LOG_ERROR, "Control port neither defined in the slave description file nor in the input file");
        return RETURN_ERROR;
    }
    ComponentLog(comp, LOG_DEBUG, "Control port: %hu", port);

    DcpOpMode dcp_mode = DcpOpMode::SRT;
    if (dcp_input->mode == SLAVE_OP_MODE_NRT) {
        dcp_mode = DcpOpMode::NRT;
    } else if (dcp_input->mode == SLAVE_OP_MODE_HRT) {
        ComponentLog(comp, LOG_ERROR, "Slave operation mode 'HRT' is not supported");
        return RETURN_ERROR;
    } else if (dcp_input->mode == SLAVE_OP_MODE_SRT) {
        dcp_mode = DcpOpMode::SRT;
    } else {
        ComponentLog(comp, LOG_ERROR, "Invalid slave operation mode (allowed modes: SRT, NRT, HRT)");
        return RETURN_ERROR;
    }

    try {
        ComponentLog(comp, LOG_DEBUG, "Creating DCP slave handle");
        *comp_dcp->slave = std::make_shared<dcp::SlaveHandle>(
            dcp_input->ip,
            port,
            slave_desc.uuid,
            dcp_mode,
            static_cast<uint32_t>(numerator),
            static_cast<uint32_t>(denominator)
        );
    } catch (std::bad_alloc&) {
        ComponentLog(comp, LOG_ERROR, "Slave handle creation failed");
        return RETURN_ERROR;
    }

    // read slave parameters
    if (ReadSlaveParameters(comp, *comp_dcp->slave, input->parameters) == RETURN_ERROR) {
        ComponentLog(comp, LOG_ERROR, "Reading slave parameters failed");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

uint16_t FindFreePort(const std::string& ip, uint16_t port_from, uint16_t port_to) {
    mcx_log(LOG_DEBUG, "Searching for a free port in range [%u, %u]", port_from, port_to);
    for (uint16_t port = port_from; port <= port_to; port++) {
        asio::io_service service;

        mcx_log(LOG_DEBUG, "Trying port %u", port);
        asio::ip::udp::endpoint ep;
        try {
            ep = asio::ip::udp::endpoint(asio::ip::address::from_string(ip), port);
        } catch (asio::system_error&) {
            mcx_log(LOG_ERROR, "Invalid DCP master IP address %s", ip.c_str());
            break;
        }

        try {
            asio::ip::udp::socket(service, ep);
        } catch (asio::system_error& e) {
            mcx_log(LOG_DEBUG, "Port %u is not available", port);
            mcx_log(LOG_DEBUG, "[%d] %s", e.code().value(), e.what());
            continue;
        }

        mcx_log(LOG_DEBUG, "Port %u is free", port);
        return port;
    }

    mcx_log(LOG_DEBUG, "No free port found. Using port 0");
    return 0;
}

static McxStatus Setup(Component * comp) {
    CompDcp * comp_dcp = (CompDcp*)comp;
    Databus * db = comp->GetDatabus(comp);

    // setup out channels
    DatabusInfo * db_out_info = DatabusGetOutInfo(db);
    size_t num_out = DatabusInfoGetChannelNum(db_out_info);

    comp_dcp->outValues = (ChannelValue *) mcx_calloc(num_out, sizeof(ChannelValue));

    for (size_t i = 0; i < num_out; i++) {
        ChannelInfo * info = DatabusInfoGetChannel(db_out_info, i);
        ChannelValueInit(&comp_dcp->outValues[i], ChannelTypeClone(info->type));
        if (DatabusSetOutReference(db, i, ChannelValueDataPointer(&comp_dcp->outValues[i]),
                                   info->type) == RETURN_ERROR) {
            return RETURN_ERROR;
        }
    }

    ComponentLog(comp, LOG_INFO, "Setting up output ports");

    std::vector<dcp::Port> out_ports;
    if (CollectPorts(comp_dcp, db_out_info, out_ports, comp_dcp->outValues, VariableType::output) == RETURN_ERROR) {
        return RETURN_ERROR;
    }
    (*comp_dcp->slave)->add_outports(out_ports);

    // setup in channels
    DatabusInfo * db_in_info = DatabusGetInInfo(db);
    size_t num_in = DatabusInfoGetChannelNum(db_in_info);

    comp_dcp->inValues = (ChannelValue *) mcx_calloc(num_in, sizeof(ChannelValue));

    for (size_t i = 0; i < num_in; i++) {
        ChannelInfo * info = DatabusInfoGetChannel(db_in_info, i);
        ChannelValueInit(&comp_dcp->inValues[i], ChannelTypeClone(info->type));
        if (DatabusSetInReference(db, i, ChannelValueDataPointer(&comp_dcp->inValues[i]),
                                  info->type) == RETURN_ERROR) {
            return RETURN_ERROR;
        }
    }

    ComponentLog(comp, LOG_INFO, "Setting up input ports");

    std::vector<dcp::Port> in_ports;
    if (CollectPorts(comp_dcp, db_in_info, in_ports, comp_dcp->inValues, VariableType::input) == RETURN_ERROR) {
        return RETURN_ERROR;
    }
    (*comp_dcp->slave)->add_inports(in_ports);

    // setup DCP master
    // TODO: this expects Setup() to be called non-concurrently
    if (!master) {
        std::string ip(comp->GetModel(comp)->config->dcpMasterIp);
        uint16_t free_port = FindFreePort(ip,
                                          static_cast<uint16_t>(comp->GetModel(comp)->config->dcpMasterPortFrom),
                                          static_cast<uint16_t>(comp->GetModel(comp)->config->dcpMasterPortTo));
        if (free_port == 0) {
            mcx_log(LOG_ERROR, "DCP master: using port 0 is not allowed");
            return RETURN_ERROR;
        }

        try {
            master = new dcp::Master(ip, free_port);
        } catch (std::exception& e) {
            mcx_log(LOG_ERROR, "DCP master creation failed");
            mcx_log(LOG_DEBUG, "Exception message: %s", e.what());
            return RETURN_ERROR;
        }
    }

    return master->addSlave(*comp_dcp->slave);
}

static McxStatus Initialize(Component * comp, size_t idx, double startTime) {
    CompDcp * dcpComp = (CompDcp *) comp;

    // the first call starts, maybe change this to last
    McxStatus ret = RETURN_OK;

    try {
        ret = master->start();
    } catch (std::exception& e) {
        ComponentLog(comp, LOG_ERROR, "Initialization failed: %s", e.what());
        return RETURN_ERROR;
    }

    return ret;
}

static McxStatus DoStepNRT(shared_ptr<dcp::SlaveHandle> slave) {
    // set inputs of the slave
    if (master->sendData(slave->id()) != RETURN_OK) { return RETURN_ERROR; }

    // let the slave make a do-step
    if (master->doStep(slave->id()) != RETURN_OK) { return RETURN_ERROR; }

    // collect the outputs from the slave
    if (master->receiveData(slave->id(), true) != RETURN_OK) { return RETURN_ERROR; }

    return RETURN_OK;
}

static McxStatus DoStepSRT(shared_ptr<dcp::SlaveHandle> slave) {
    // set inputs of the slave
    if (master->sendData(slave->id()) != RETURN_OK) { return RETURN_ERROR; }

    // get outputs from the slave
    if (master->receiveData(slave->id(), false) != RETURN_OK) { return RETURN_ERROR; }

    return RETURN_OK;
}

static McxStatus DoStep(Component * comp, size_t group, double time, double deltaTime, double endTime, int isNewStep) {
    CompDcp * dcpComp = (CompDcp *) comp;

    // first update parameters
    // update inputs/outputs
    std::shared_ptr<dcp::SlaveHandle> slave = *dcpComp->slave;
    switch (slave->mode()) {
        case DcpOpMode::SRT:

            return DoStepSRT(slave);
        case DcpOpMode::NRT:
            return DoStepNRT(slave);
        case DcpOpMode::HRT:
            ComponentLog(comp, LOG_ERROR, "HRT mode is not supported");
            return RETURN_ERROR;
    }

    return RETURN_OK;
}

static McxStatus Finish(Component * comp, FinishState * finishState) {
    CompDcp * dcpComp = (CompDcp *) comp;

    return master->stop((*dcpComp->slave)->id());
}

static ChannelMode GetInChannelDefaultMode(struct Component * comp) {
    return CHANNEL_OPTIONAL;
}

static inline McxStatus make_output_depend_on_all(size_t out_idx, struct Dependencies *deps) {
    // no dependencies defined - output depends on every input (potentially)
    for (size_t in_idx = 0; in_idx < GetDependencyNumIn(deps); in_idx++) {
        if (SetDependency(deps, in_idx, out_idx, DEP_DEPENDENT) == RETURN_ERROR) {
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

static McxStatus DcpSetDependencies(const Component * comp, struct Dependencies *deps) {
    CompDcp * comp_dcp = (CompDcp*)comp;
    McxStatus ret_val = RETURN_OK;

    // mappings between variable value references and channel indices
    std::map<uint64_t, size_t> out_vr_to_idx;
    std::map<uint64_t, size_t> in_vr_to_idx;
    fill_vr_mapping(out_vr_to_idx, (*comp_dcp->slave)->outports());
    fill_vr_mapping(in_vr_to_idx, (*comp_dcp->slave)->inports());

    // initially the dependency matrix is filled with zeroes
    // change its content based on the dependency information from the slave description
    shared_ptr<SlaveDescription_t>& slave_desc = *comp_dcp->slave_desc;
    for (Variable_t& variable : slave_desc->Variables) {
        // only Output elements can have dependencies
        if (variable.Output == nullptr) {
            continue;
        }

        size_t out_idx = 0;
        try {
            out_idx = out_vr_to_idx.at(variable.valueReference);
        } catch (std::out_of_range&) {
            // Output defined in slave description, but not in the input file - ignore it
            continue;
        }

        std::shared_ptr<Dependencies_t> dependencies = variable.Output->Dependencies;
        if (dependencies != nullptr) {
            if (dependencies->Initialization != nullptr) {
                // if the vector is empty, the output does not depend on any inputs
                for (Dependency_t& dep : dependencies->Initialization->dependecies) {
                    try {
                        // dependency type is not important for us - we use DEP_DEPENDENT always
                        ret_val = SetDependency(deps, in_vr_to_idx.at(dep.vr), out_idx, DEP_DEPENDENT);
                        if (ret_val == RETURN_ERROR) {
                            return ret_val;
                        }
                    } catch (std::out_of_range&) {
                        // not an input
                    }
                }
            } else {
                if (make_output_depend_on_all(out_idx, deps) == RETURN_ERROR) {
                    return RETURN_ERROR;
                }
            }
        } else {
            if (make_output_depend_on_all(out_idx, deps) == RETURN_ERROR) {
                return RETURN_ERROR;
            }
        }
    }

    return RETURN_OK;
}

static struct Dependencies* DcpGetInOutGroupsInitialDependency(const Component * comp) {
    CompDcp * comp_dcp = (CompDcp *)comp;
    size_t num_in = comp->GetNumInChannels(comp);
    size_t num_out = comp->GetNumOutChannels(comp);

    struct Dependencies * dependencies = DependenciesCreate(num_in, num_out);
    if (DcpSetDependencies(comp, dependencies) != RETURN_OK) {
        ComponentLog(comp, LOG_ERROR, "Initial dependency matrix could not be created");
        return nullptr;
    }

    return dependencies;
}

static McxStatus DcpUpdateInChannels(Component * comp) {
    CompDcp * comp_dcp = (CompDcp*)comp;
    std::shared_ptr<dcp::SlaveHandle> slave = *comp_dcp->slave;

    // send inputs to the slave
    return master->sendData(slave->id());
}

static McxStatus DcpUpdateInitialOutChannels(Component * comp) {
    CompDcp * comp_dcp = (CompDcp*)comp;
    std::shared_ptr<dcp::SlaveHandle> slave = *comp_dcp->slave;

    // trigger the calculation in the slave
    if (master->doInitStep(slave->id()) != RETURN_OK) { return RETURN_ERROR; }

    // receive the data from the slave
    if (master->receiveData(slave->id(), true) != RETURN_OK) { return RETURN_ERROR; }

    return RETURN_OK;
}

static McxStatus DcpExitInitializationMode(Component *comp) {
    CompDcp * comp_dcp = (CompDcp*)comp;
    std::shared_ptr<dcp::SlaveHandle> slave = *comp_dcp->slave;

    return master->initDone(slave->id());
}

static void CompDcpDestructor(CompDcp * comp) {
    if (master) {
        delete master;
        master = nullptr;
    }

    if (comp->params_fixed) { delete comp->params_fixed; }
    if (comp->params_tunable) { delete comp->params_tunable; }
    if (comp->slave_desc) { delete comp->slave_desc; }
    if (comp->slave) { delete comp->slave; }
}

static Component * CompDcpCreate(Component * comp) {
    CompDcp * self = (CompDcp *) comp;

    // map to local functions
    comp->Read = Read;
    comp->Setup = Setup;
    comp->DoStep = DoStep;
    comp->Finish = Finish;

    comp->Initialize = Initialize;
    comp->GetInOutGroupsInitialDependency = DcpGetInOutGroupsInitialDependency;
    comp->UpdateInChannels = DcpUpdateInChannels;
    comp->UpdateInitialOutChannels = DcpUpdateInitialOutChannels;
    comp->ExitInitializationMode = DcpExitInitializationMode;

    comp->GetInChannelDefaultMode = GetInChannelDefaultMode;

    try {
        self->slave = new std::shared_ptr<dcp::SlaveHandle>;
        self->slave_desc = new std::shared_ptr<SlaveDescription_t>;
        self->params_fixed = new std::vector<std::shared_ptr<DCPParameter>>;
        self->params_tunable = new std::vector<std::shared_ptr<DCPParameter>>;
    } catch (std::bad_alloc&) {
        ComponentLog(comp, LOG_ERROR, "No memory for slave handle");
        return NULL;
    }

    return comp;
}

OBJECT_CLASS(CompDcp, Component);

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#else /* not ENABLE_DCP */

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef struct CompDcp {
    Component _;  /* super class first */
} CompDcp;

static void CompDcpDestructor(CompDcp * comp) {
}

static Component * CompDcpCreate(Component * comp) {
    mcx_log(LOG_ERROR, "This executable does not support DCP");
    return NULL;
}

OBJECT_CLASS(CompDcp, Component);

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /* ENABLE_DCP */