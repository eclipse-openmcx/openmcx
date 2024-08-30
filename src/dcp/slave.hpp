/********************************************************************************
 * Copyright (c) 2024 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_DCP_SLAVE_HPP
#define MCX_DCP_SLAVE_HPP

extern "C" {
#include "CentralParts.h"
}

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <dcp/model/constant/DcpDataType.hpp>
#include <dcp/model/constant/DcpError.hpp>
#include <dcp/model/constant/DcpOpMode.hpp>
#include <dcp/model/constant/DcpState.hpp>

namespace dcp {
class SlaveHandle;

class Port {
public:
    Port(std::shared_ptr<SlaveHandle> &slave, uint64_t vr, DcpDataType type);

    // getters
    std::shared_ptr<SlaveHandle> slave() const;
    DcpDataType type() const;
    void* value() const;

    uint64_t vr() const;
    uint16_t data_id() const;

    // setters
    void set_data_id(uint16_t data_id);
    void set_value_reference(void *value);

    template<typename T>
    void set_value(T value);

private:
    std::shared_ptr<SlaveHandle> slave_;

    uint64_t vr_;
    uint16_t data_id_;

    DcpDataType type_;

    void *value_;
};

template<typename T>
void Port::set_value(T value) {
    *(T*)this->value_ = value;
}

class Parameter {
public:
    Parameter(const std::string& name,
              uint64_t vr,
              DcpDataType type,
              uint16_t param_id = 0,
              void * value = nullptr);

    // getters
    DcpDataType type() const;
    void* value() const;

    const std::string& name() const;
    uint64_t vr() const;
    uint16_t param_id() const;

    // setters
    void set_param_id(uint16_t param_id);
    void set_value_reference(void *value);

    template<typename T>
    void set_value(T value);

private:
    std::string name_;
    uint64_t vr_;
    uint16_t param_id_;
    DcpDataType type_;

    void *value_;
};

template<typename T>
void Parameter::set_value(T value) {
    *(T*)this->value_ = value;
}

class SlaveHandle {
public:
    SlaveHandle(const std::string& ip, uint16_t port, const std::string& uuid, DcpOpMode mode,
                uint32_t numerator, uint32_t denominator);

    void add_outports(std::vector<Port>& ports);
    void add_inports(std::vector<Port>& ports);

    std::shared_ptr<Parameter> add_fixed_param(const std::string& name,
                                               uint64_t vr,
                                               DcpDataType type,
                                               void * value = nullptr);
    std::shared_ptr<Parameter> add_tunable_param(const std::string& name,
                                                 uint64_t vr,
                                                 DcpDataType type,
                                                 void * value = nullptr);

    // getters
    uint8_t id() const;
    uint16_t port() const;

    std::string ip() const;
    std::string uuid() const;

    uint32_t numerator() const;
    uint32_t denominator() const;

    DcpOpMode mode() const;
    DcpState state() const;
    DcpError error_code() const;

    std::vector<Port>& inports();
    std::vector<Port>& outports();

    std::vector<std::shared_ptr<Parameter>>& fixed_params();
    std::vector<std::shared_ptr<Parameter>>& tunable_params();

    // setters
    void set_id(uint8_t id);
    void set_state(DcpState state);
    void set_error_code(DcpError error_code);

private:
    uint8_t id_;

    std::string ip_;
    uint16_t port_;

    std::string uuid_;
    DcpOpMode mode_;

    uint32_t numerator_;
    uint32_t denominator_;

    DcpState state_;
    DcpError error_code_;

    std::vector<Port> inports_;
    std::vector<Port> outports_;

    std::vector<std::shared_ptr<Parameter>> fixed_params_;
    std::vector<std::shared_ptr<Parameter>> tunable_params_;
};
}

#endif /* MCX_DCP_SLAVE_HPP */