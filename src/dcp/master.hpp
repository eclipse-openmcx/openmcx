/********************************************************************************
 * Copyright (c) 2024 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef MCX_DCP_MASTER_HPP
#define MCX_DCP_MASTER_HPP

extern "C" {
#include "CentralParts.h"
}

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <iostream>

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include <asio/io_service.hpp>

#include <dcp/driver/ethernet/udp/UdpDriver.hpp>
#include <dcp/logic/DcpManagerMaster.hpp>
#include <dcp/model/pdu/DcpPdu.hpp>

#include "slave.hpp"

// TODO use logging functionality from the DCPLib

namespace dcp {
class Exception : public std::exception {
    const char * what () const noexcept {
        return "Generic DCP error occurred";
    }
};

class ResponseException : public Exception {
    const char * what () const noexcept {
        return "Unexpected response message received";
    }
};

class StateChangeException : public Exception {
    const char * what () const noexcept {
        return "Unexpected error occurred while trying to change slave states";
    }
};

class EthernetException : public Exception {
    const char * what () const noexcept {
        return "Unexpected ethernet configuration error occurred";
    }
};

class Master {
public:
    // constructors/destructor
    Master(std::string &ip, uint16_t port);
    ~Master();

    // DCP version used by the master
    static const uint8_t version_major;
    static const uint8_t version_minor;

    // starts the manager thread, registers all slaves `slaves_` and waits for them to
    // switch to the CONFIGURED state
    McxStatus start();

    // stops all managed slaves
    McxStatus stop(uint8_t slave_id);

    // pause simulation and drop incoming data
    void pause();

    // resume simulation and stop dropping incoming data
    void unpause();

    // sends data from master to slave inputs
    McxStatus sendData(uint8_t slave_id);

    // receives data from slave outputs
    McxStatus receiveData(uint8_t slave_id, bool blocking = false);

    // sends parameter data from master to slave
    McxStatus sendParameter(uint8_t slave_id, std::shared_ptr<Parameter> param);

    // does a do-step (transitions once through the NonRealTime super-state of the slave)
    McxStatus doStep(uint8_t slave_id);

    // triggers a walk of the slave through its initialization states
    // i.e. CONFIGURED -> INITIALIZING -> INITIALIZED -> SENDING_I -> CONFIGURED
    McxStatus doInitStep(uint8_t slave_id);

    // triggers the transition of the slave into the RUN super-state
    // the last call (equal to the number of registered slaves) will wait for all slaves to
    // get into the RUN super-state
    McxStatus initDone(uint8_t slave_id);

    // adds a new slave to the list of slaves managed by the master
    McxStatus addSlave(std::shared_ptr<SlaveHandle> slave);

private:
    // data members
    std::string host_;
    uint16_t port_;

    UdpDriver driver_;
    DcpManagerMaster manager_;
    std::thread manager_thread_;

    // local slave instances used to change the logic state of the master
    std::vector<std::shared_ptr<SlaveHandle>> slaves_;

    // flags
    bool network_error_ = false;
    bool fatal_error_ = false;
    bool started_ = false;

    bool paused_ = false;

    // superstate switch controls
    std::mutex run_mutex_;
    std::condition_variable run_cv_;
    size_t slaves_to_be_run_;
    int instructed_to_run_;

    std::mutex configured_mutex_;
    std::condition_variable configured_cv_;
    size_t not_configured_slaves_;
    bool configure_done_ = false;

    std::mutex configuration_mutex_;
    std::condition_variable configuration_cv_;
    size_t slaves_in_configuration_ = 0;

    // message acknowledging control variables
    std::map<uint8_t, std::unique_ptr<std::mutex>> response_mutices_;
    std::map<uint8_t, std::unique_ptr<std::condition_variable>> response_cvs_;
    std::map<uint8_t, bool> response_received_;

    // NRT control variables
    std::map<uint8_t, std::unique_ptr<std::condition_variable>> condition_vars_;
    std::map<uint8_t, std::unique_ptr<std::mutex>> mutices_;
    std::map<uint8_t, bool> nrt_do_step_done_;

    // initialization control variables
    std::map<uint8_t, std::unique_ptr<std::condition_variable>> init_condition_vars_;
    std::map<uint8_t, std::unique_ptr<std::mutex>> init_mutices_;
    std::map<uint8_t, bool> init_step_done_;

    // guards used to detect errors which happend during the error handling procedure
    std::map<uint8_t, bool> error_handling_;

    // buffer which stores the received bytes for each data_id
    std::map<uint16_t, std::unique_ptr<std::mutex>> receive_mutices_;
    std::map<uint16_t, std::vector<uint8_t>> receive_buffer_;

    // error states of the slaves
    std::map<uint8_t, bool> error_state_;

    // const sleeping timers
    static const std::chrono::milliseconds sleep_time_;
    static const std::chrono::milliseconds sleep_time_start_;
    static const std::chrono::seconds sleep_time_error_;
    static const std::chrono::milliseconds sleep_time_error_start_;
    static const std::chrono::milliseconds timeout_;

    // counter used to generated globally unique ids (1 data_id per port / 1 param_id per parameter)
    uint16_t data_id_ = 0;
    uint16_t param_id_ = 0;

    // callback listeners
    void stateChanged(uint8_t slave_id, DcpState state);
    void ackReceived(uint8_t slave_id, uint16_t pdu_seq_id);
    void nackReceived(uint8_t slave_id, uint16_t pdu_seq_id, DcpError error_code);
    void dataReceived(uint16_t data_id, size_t length, uint8_t payload[]);
    void errorOccurred(DcpError error);

    // PDU sending methods
    void deregisterSlaveSTC(uint8_t slave_id, DcpState state);
    void stopSlaveSTC(uint8_t slave_id, DcpState state);
    void prepareSlaveSTC(uint8_t slave_id);
    void configureSlaveSTC(uint8_t slave_id);
    void runSlaveSTC(uint8_t slave_id, DcpState state);
    void resetSlaveSTC(uint8_t slave_id);
    void doStepSlaveSTC(uint8_t slave_id, DcpState state);
    void sendOutputsSlaveSTC(uint8_t slave_id, DcpState state);
    void initializeSlaveSTC(uint8_t slave_id);

    // state handler methods
    void nonRealTimeExitSlave(uint8_t slave_id);
    void runEnterSlave();
    void initStepDone(uint8_t slave_id);
    void configuredEnter();

    // slave configuration blocking methods
    void CFG_time_res(std::shared_ptr<SlaveHandle>& slave);
    void CFG_scope(std::shared_ptr<SlaveHandle>& slave, uint16_t data_id);
    void CFG_output(std::shared_ptr<SlaveHandle>& slave, uint16_t data_id, uint64_t vr);
    void CFG_input(std::shared_ptr<SlaveHandle>& slave, uint16_t data_id, uint64_t vr, DcpDataType type);
    void CFG_parameter(std::shared_ptr<SlaveHandle>& slave, std::shared_ptr<Parameter>& param);
    void CFG_tunable_parameter(std::shared_ptr<SlaveHandle>& slave, std::shared_ptr<Parameter>& param,
                               uint16_t param_id);
    void CFG_steps(std::shared_ptr<SlaveHandle>& slave, uint16_t data_id, uint32_t steps);
    void CFG_target_network_information(std::shared_ptr<SlaveHandle>& slave, uint16_t data_id);
    void CFG_source_network_information(std::shared_ptr<SlaveHandle>& slave, uint16_t data_id);
    void CFG_param_network_information(std::shared_ptr<SlaveHandle>& slave, uint16_t param_id);

    // utility methods
    void waitForSlave(std::shared_ptr<SlaveHandle>& slave, DcpState state);
    void invalidSlaveIdReceived(uint8_t slave_id);
    std::unique_ptr<uint8_t[]> getNetInfo(std::shared_ptr<SlaveHandle>& slave);

    void prepare_to_receive_response(std::shared_ptr<SlaveHandle>& slave);
    void process_response(std::shared_ptr<SlaveHandle>& slave);
    void prepare_wait_for_configuration_state();
    void wait_for_configuration_state();

    void registerSlaves();
    void configureSlaves();
    void configureSlave(std::shared_ptr<SlaveHandle>& slave);
    McxStatus setupSlaveNetwork();
    void startManager();

    // error handler methods
    void errorResolvedShutDown(std::shared_ptr<SlaveHandle>& slave);
    void configurationShutDown(std::shared_ptr<SlaveHandle>& slave);
    void errorShutDown();

    McxStatus checkForErrors();
    McxStatus checkForErrors(uint8_t slave_id);

    void processErrors();
};
}

#endif /* MCX_DCP_MASTER_HPP */