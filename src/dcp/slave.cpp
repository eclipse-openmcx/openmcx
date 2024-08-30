/********************************************************************************
 * Copyright (c) 2024 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "slave.hpp"

/*************************************************************************************************/
/*                                   Port definition                                             */
/*************************************************************************************************/
dcp::Port::Port(std::shared_ptr<SlaveHandle> &slave, uint64_t vr, DcpDataType type) :
    slave_{ slave },
    vr_{ vr },
    data_id_{ 0 },
    type_{ type }
{
}

std::shared_ptr<dcp::SlaveHandle> dcp::Port::slave() const {
    return this->slave_;
}

DcpDataType dcp::Port::type() const {
    return this->type_;
}

void* dcp::Port::value() const {
    return this->value_;
}

uint64_t dcp::Port::vr() const {
    return this->vr_;
}

uint16_t dcp::Port::data_id() const {
    return this->data_id_;
}

void dcp::Port::set_value_reference(void *value) {
    this->value_ = value;
}

void dcp::Port::set_data_id(uint16_t data_id) {
    this->data_id_ = data_id;
}

/*************************************************************************************************/
/*                                   Parameter definition                                        */
/*************************************************************************************************/
dcp::Parameter::Parameter(const std::string& name,
                          uint64_t vr,
                          DcpDataType type,
                          uint16_t param_id,
                          void * value) :
    name_(name),
    vr_(vr),
    type_(type),
    value_(value),
    param_id_(param_id)
{}

// getters
DcpDataType dcp::Parameter::type() const {
    return this->type_;
}

void* dcp::Parameter::value() const {
    return this->value_;
}

uint64_t dcp::Parameter::vr() const {
    return this->vr_;
}

uint16_t dcp::Parameter::param_id() const {
    return this->param_id_;
}

const std::string& dcp::Parameter::name() const {
    return this->name_;
}

// setters
void dcp::Parameter::set_param_id(uint16_t param_id) {
    this->param_id_ = param_id;
}

void dcp::Parameter::set_value_reference(void *value) {
    this->value_ = value;
}

/*************************************************************************************************/
/*                                     SlaveHandle                                               */
/*************************************************************************************************/
dcp::SlaveHandle::SlaveHandle(const std::string& ip,
                              uint16_t port,
                              const std::string& uuid,
                              DcpOpMode mode,
                              uint32_t numerator,
                              uint32_t denominator) :
    id_{ 0 },
    ip_{ ip },
    port_{ port },
    uuid_{ uuid },
    mode_{ mode },
    numerator_{ numerator },
    denominator_{ denominator },
    state_{ DcpState::ALIVE },
    error_code_{ DcpError::NONE }
{
}

void dcp::SlaveHandle::add_inports(std::vector<Port>& ports) {
    this->inports_.insert(this->inports_.end(), ports.begin(), ports.end());
}

void dcp::SlaveHandle::add_outports(std::vector<Port>& ports) {
    this->outports_.insert(this->outports_.end(), ports.begin(), ports.end());
}

std::shared_ptr<dcp::Parameter> dcp::SlaveHandle::add_fixed_param(const std::string& name,
                                                                  uint64_t vr,
                                                                  DcpDataType type,
                                                                  void * value) {
    // param_id = 0 is used because at point of construction it is not known
    auto param = std::make_shared<dcp::Parameter>(name, vr, type, 0, value);
    this->fixed_params_.push_back(param);
    return param;
}

std::shared_ptr<dcp::Parameter> dcp::SlaveHandle::add_tunable_param(const std::string& name,
                                                                    uint64_t vr,
                                                                    DcpDataType type,
                                                                    void * value) {
    // param_id = 0 is used because at point of construction it is not known
    auto param = std::make_shared<dcp::Parameter>(name, vr, type, 0, value);
    this->tunable_params_.push_back(param);
    return param;
}

uint8_t dcp::SlaveHandle::id() const {
    return this->id_;
}

uint16_t dcp::SlaveHandle::port() const {
    return this->port_;
}

std::string dcp::SlaveHandle::ip() const {
    return this->ip_;
}

std::string dcp::SlaveHandle::uuid() const {
    return this->uuid_;
}

uint32_t dcp::SlaveHandle::numerator() const {
    return this->numerator_;
}

uint32_t dcp::SlaveHandle::denominator() const {
    return this->denominator_;
}

DcpOpMode dcp::SlaveHandle::mode() const {
    return this->mode_;
}

DcpState dcp::SlaveHandle::state() const {
    return this->state_;
}

DcpError dcp::SlaveHandle::error_code() const {
    return this->error_code_;
}

std::vector<dcp::Port>& dcp::SlaveHandle::inports() {
    return this->inports_;
}

std::vector<dcp::Port>& dcp::SlaveHandle::outports() {
    return this->outports_;
}

std::vector<std::shared_ptr<dcp::Parameter>>& dcp::SlaveHandle::fixed_params() {
    return this->fixed_params_;
}

std::vector<std::shared_ptr<dcp::Parameter>>& dcp::SlaveHandle::tunable_params() {
    return this->tunable_params_;
}

void dcp::SlaveHandle::set_state(DcpState state) {
    this->state_ = state;
}

void dcp::SlaveHandle::set_error_code(DcpError error_code) {
    this->error_code_ = error_code;
}

void dcp::SlaveHandle::set_id(uint8_t id) {
    this->id_ = id;
}