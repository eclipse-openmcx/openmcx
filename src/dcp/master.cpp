/********************************************************************************
 * Copyright (c) 2024 AVL List GmbH and others
 * 
 * This program and the accompanying materials are made available under the
 * terms of the Apache Software License 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 * 
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "master.hpp"

#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>

#include <asio/io_service.hpp>
#include <asio/ip/address.hpp>
#include <asio/ip/udp.hpp>


namespace dcp {
/*************************************************************************************************/
/*                                       Utility functions                                       */
/*************************************************************************************************/
asio::ip::address_v4 source_address(
    const asio::ip::address& ip_address) {
    using asio::ip::udp;
    asio::io_service service;
    udp::socket socket(service);
    udp::endpoint endpoint(ip_address, 0);
    socket.connect(endpoint);
    return socket.local_endpoint().address().to_v4();
}

/*************************************************************************************************/
/*                                Constructors/Destructors                                       */
/*************************************************************************************************/
Master::Master(std::string &host, uint16_t port) :
    host_{ host },
    port_{ port },
    driver_{ host, port },
    manager_{ this->driver_.getDcpDriver() }
{
    // setup callbacks
    this->manager_.setAckReceivedListener<SYNC>(std::bind(
        &Master::ackReceived, this, std::placeholders::_1, std::placeholders::_2));
    this->manager_.setNAckReceivedListener<SYNC>(std::bind(
        &Master::nackReceived, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    this->manager_.setStateChangedNotificationReceivedListener<SYNC>(std::bind(
        &Master::stateChanged, this, std::placeholders::_1, std::placeholders::_2));
    this->manager_.setDataReceivedListener<SYNC>(std::bind(
        &Master::dataReceived, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    this->manager_.setErrorListener<SYNC>(std::bind(
        &Master::errorOccurred, this, std::placeholders::_1));
}

Master::~Master() {
    this->manager_.stop();

    try {
        this->manager_thread_.join();
    } catch (std::system_error &e) {
        mcx_log(LOG_ERROR, "DCP master cleanup failed");
        mcx_log(LOG_ERROR, "Caught system error with code %d meaning %s", e.code().value(), e.what());
    }
}

/*************************************************************************************************/
/*                                     Listener callbacks                                        */
/*************************************************************************************************/
void Master::stateChanged(uint8_t slave_id, DcpState state) {
    mcx_log(LOG_DEBUG, "Slave %d changed state to %s", slave_id, to_string(state).c_str());

    // update local slave state
    DcpState prev_state = this->slaves_[slave_id - 1]->state();
    this->slaves_[slave_id - 1]->set_state(state);

    // handle slave state changes
    switch (state) {
        case DcpState::CONFIGURATION:
        {
            std::unique_lock<std::mutex> lock(this->configuration_mutex_);
            this->slaves_in_configuration_--;
            this->configuration_cv_.notify_one();
            break;
        }
        case DcpState::PREPARED:
            this->configureSlaveSTC(slave_id);
            break;
        case DcpState::CONFIGURED:
            if (prev_state == DcpState::SENDING_I) {
                // signal that the initialization step was done
                this->initStepDone(slave_id);
            } else {
                // signal that the slave has entered the configured state
                this->configuredEnter();
            }
            break;
        case DcpState::SYNCHRONIZED:
            if (prev_state == DcpState::SENDING_D) {
                this->nonRealTimeExitSlave(slave_id);
            }
            this->runSlaveSTC(slave_id, state);
            break;
        case DcpState::STOPPED:
            this->deregisterSlaveSTC(slave_id, DcpState::STOPPED);
            break;
        case DcpState::ERROR_HANDLING:
            if (this->error_handling_[slave_id]) {
                mcx_log(LOG_ERROR, "Error occurred during the shutdown procedure");
                this->fatal_error_ = true;
            }
            this->error_state_[slave_id] = true;
            break;
        case DcpState::ERROR_RESOLVED:
            if (prev_state != DcpState::ERROR_HANDLING && this->error_handling_[slave_id]) {
                mcx_log(LOG_ERROR, "Error occurred during the shutdown procedure");
                this->fatal_error_ = true;
            }
            this->error_state_[slave_id] = true;
            break;
        case DcpState::SYNCHRONIZING:
            if (prev_state == DcpState::SENDING_D) {
                this->nonRealTimeExitSlave(slave_id);
            } else if (prev_state == DcpState::CONFIGURED) {
                this->runEnterSlave();
            }
            break;
        case DcpState::RUNNING:
            if (prev_state == DcpState::SENDING_D) {
                this->nonRealTimeExitSlave(slave_id);
            }
            break;
        case DcpState::COMPUTED:
            this->sendOutputsSlaveSTC(slave_id, state);
            break;
        case DcpState::INITIALIZED:
            this->sendOutputsSlaveSTC(slave_id, state);
            break;
        case DcpState::ALIVE:
        case DcpState::PREPARING:
        case DcpState::CONFIGURING:
        case DcpState::SENDING_D:
        case DcpState::STOPPING:
        case DcpState::COMPUTING:
        case DcpState::INITIALIZING:
        case DcpState::SENDING_I:
            break;
        default:
            mcx_log(LOG_ERROR, "Invalid DCP slave state detected");
            break;
    }
}

void Master::ackReceived(uint8_t slave_id, uint16_t pdu_seq_id) {
    std::shared_ptr<SlaveHandle> slave;

    // check whether the slave_id is correct
    try {
        slave = this->slaves_.at(slave_id - 1);
    } catch (std::out_of_range& e) {
        UNUSED(e);
        this->invalidSlaveIdReceived(slave_id);
        return;
    }

    // notify the waiting thread - if any
    mcx_log(LOG_DEBUG, "Slave %d: received RSP_ACK for PDU %d", slave_id, pdu_seq_id);

    {
        std::lock_guard<std::mutex> lock(*this->response_mutices_[slave->id()]);
        this->response_received_[slave->id()] = true;
        this->response_cvs_[slave->id()]->notify_all();
    }
}

void Master::nackReceived(uint8_t slave_id, uint16_t pdu_seq_id, DcpError error_code) {
    std::shared_ptr<SlaveHandle> slave;

    // check whether the slave_id is correct
    try {
        slave = this->slaves_.at(slave_id - 1);
    } catch (std::out_of_range& e) {
        UNUSED(e);
        this->invalidSlaveIdReceived(slave_id);
        return;
    }

    mcx_log(LOG_ERROR, "Slave %d: error with code %s occurred while processing PDU %d",
           slave_id, to_string(error_code).c_str(), pdu_seq_id);

    // note that an error happened (RSP_NACK was received)
    if (this->error_handling_[slave_id]) {
        mcx_log(LOG_ERROR, "Error occurred during the shutdown procedure");
        this->fatal_error_ = true;
    } else {
        this->error_state_[slave_id] = true;
    }

    // set error code to the local slave instance for later use (if needed)
    slave->set_error_code(error_code);

    {
        // notify the waiting thread
        std::lock_guard<std::mutex> lock(*this->response_mutices_[slave->id()]);
        this->response_received_[slave->id()] = true;
        this->response_cvs_[slave->id()]->notify_all();
    }
}

void Master::pause() {
    this->paused_ = true;
}

void Master::unpause() {
    this->paused_ = false;
}

void Master::dataReceived(uint16_t data_id, size_t length, uint8_t payload[]) {
    // store the received data into an intermediate buffer

    if (!this->configure_done_) {
        // model is not set up correctly yet
        // ignoring data
        return;
    }

    auto buffer_it = this->receive_buffer_.find(data_id);
    if (buffer_it == this->receive_buffer_.end()) {
        mcx_log(LOG_WARNING, "DCP: data PDU with unknown data_id %d received. dropping ...", data_id);
        return;
    }

    if (this->paused_) {
        // drop data while paused
        return;
    }

    // after all slaves are configured, we expect that all mutices are created
    // we explictly don't check this like
    // auto mtx = this->receive_mutices_.find(data_id);
    // if (mtx == this->receive_mutices_.end()) ...

    std::unique_lock<std::mutex> lock(*this->receive_mutices_[data_id]);
    for (size_t i = 0; i < length; ++i) {
        buffer_it->second.push_back(payload[i]);
    }
}

void Master::errorOccurred(DcpError error) {
    mcx_log(LOG_ERROR, "DCP: Network error occurred");
    mcx_log(LOG_ERROR, "Error: %s", to_string(error).c_str());
    this->network_error_ = true;
}

/*************************************************************************************************/
/*                                  PDU sending methods                                          */
/*************************************************************************************************/
void Master::deregisterSlaveSTC(uint8_t slave_id, DcpState state) {
    mcx_log(LOG_DEBUG, "Sending STC_DEREGISTER to slave %d", slave_id);
    this->manager_.STC_deregister(slave_id, state);
    mcx_log(LOG_DEBUG, "Sending STC_DEREGISTER to slave %d done", slave_id);
}

void Master::stopSlaveSTC(uint8_t slave_id, DcpState state) {
    mcx_log(LOG_DEBUG, "Sending STC_STOP to slave %d", slave_id);
    this->manager_.STC_stop(slave_id, state);
    mcx_log(LOG_DEBUG, "Sending STC_STOP to slave %d done", slave_id);
}

void Master::prepareSlaveSTC(uint8_t slave_id) {
    mcx_log(LOG_DEBUG, "Sending STC_PREPARE to slave %d", slave_id);
    this->manager_.STC_prepare(slave_id, DcpState::CONFIGURATION);
    mcx_log(LOG_DEBUG, "Sending STC_PREPARE to slave %d done", slave_id);
}

void Master::configureSlaveSTC(uint8_t slave_id) {
    mcx_log(LOG_DEBUG, "Sending STC_CONFIGURE to slave %d", slave_id);
    this->manager_.STC_configure(slave_id, DcpState::PREPARED);
    mcx_log(LOG_DEBUG, "Sending STC_CONFIGURE to slave %d done", slave_id);
}

void Master::runSlaveSTC(uint8_t slave_id, DcpState state) {
    mcx_log(LOG_DEBUG, "Sending STC_RUN to slave %d", slave_id);
    this->manager_.STC_run(slave_id, state, 0);     // 0 -> tell the slave to start immediately
    mcx_log(LOG_DEBUG, "Sending STC_RUN to slave %d done", slave_id);
}

void Master::resetSlaveSTC(uint8_t slave_id) {
    mcx_log(LOG_DEBUG, "Sending STC_RESET to slave %d", slave_id);
    this->manager_.STC_reset(slave_id, DcpState::ERROR_RESOLVED);
    mcx_log(LOG_DEBUG, "Sending STC_RESET to slave %d done", slave_id);
}

void Master::doStepSlaveSTC(uint8_t slave_id, DcpState state) {
    mcx_log(LOG_DEBUG, "Sending STC_DO_STEP to slave %d", slave_id);
    this->manager_.STC_do_step(slave_id, state, 1);     // 1 -> always do only one step
    mcx_log(LOG_DEBUG, "Sending STC_DO_STEP to slave %d done", slave_id);
}

void Master::sendOutputsSlaveSTC(uint8_t slave_id, DcpState state) {
    mcx_log(LOG_DEBUG, "Sending STC_SEND_OUTPUTS to slave %d", slave_id);
    this->manager_.STC_send_outputs(slave_id, state);
    mcx_log(LOG_DEBUG, "Sending STC_SEND_OUTPUTS to slave %d done", slave_id);
}

void Master::initializeSlaveSTC(uint8_t slave_id) {
    mcx_log(LOG_DEBUG, "Sending STC_INITIALIZE to slave %d", slave_id);
    this->manager_.STC_initialize(slave_id, DcpState::CONFIGURED);
    mcx_log(LOG_DEBUG, "Sending STC_INITIALIZE to slave %d done", slave_id);
}

/*************************************************************************************************/
/*                            Slave configuration blocking methods                               */
/*************************************************************************************************/
void Master::CFG_time_res(std::shared_ptr<SlaveHandle>& slave) {
    this->prepare_to_receive_response(slave);

    mcx_log(LOG_DEBUG, "Sending CFG_time_res to slave %d", slave->id());
    this->manager_.CFG_time_res(slave->id(), slave->numerator(), slave->denominator());
    mcx_log(LOG_DEBUG, "Sending CFG_time_res to slave %d done", slave->id());

    return this->process_response(slave);
}

void Master::CFG_scope(std::shared_ptr<SlaveHandle>& slave, uint16_t data_id) {
    this->prepare_to_receive_response(slave);

    mcx_log(LOG_DEBUG, "Sending CFG_scope to slave %d", slave->id());
    this->manager_.CFG_scope(slave->id(), data_id, DcpScope::Initialization_Run_NonRealTime);
    mcx_log(LOG_DEBUG, "Sending CFG_scope to slave %d done", slave->id());

    return this->process_response(slave);
}

void Master::CFG_output(std::shared_ptr<SlaveHandle>& slave, uint16_t data_id, uint64_t vr) {
    this->prepare_to_receive_response(slave);

    mcx_log(LOG_DEBUG, "Sending CFG_output to slave %d", slave->id());
    this->manager_.CFG_output(slave->id(), data_id, 0, vr);
    mcx_log(LOG_DEBUG, "Sending CFG_output to slave %d done", slave->id());

    return this->process_response(slave);
}

void Master::CFG_steps(std::shared_ptr<SlaveHandle>& slave, uint16_t data_id, uint32_t steps) {
    this->prepare_to_receive_response(slave);

    mcx_log(LOG_DEBUG, "Sending CFG_steps to slave %d", slave->id());
    this->manager_.CFG_steps(slave->id(), data_id, steps);
    mcx_log(LOG_DEBUG, "Sending CFG_steps to slave %d done", slave->id());

    return this->process_response(slave);
}

void Master::CFG_target_network_information(std::shared_ptr<SlaveHandle>& slave, uint16_t data_id) {
    this->prepare_to_receive_response(slave);

    asio::ip::address ip = asio::ip::address_v4::from_string(this->host_);
    if (ip == asio::ip::address_v4::from_string("0.0.0.0")) {
        // If ip is 0.0.0.0, we need to get the real ip that is used
        // to connect to slave as the endpoint in the target_network_information.
        ip = source_address(asio::ip::make_address(slave->ip()));
    }

    mcx_log(LOG_DEBUG, "Sending CFG_target_network_information to slave %d", slave->id());
    this->manager_.CFG_target_network_information_UDP(
        slave->id(),
        data_id,
        ip.to_v4().to_ulong(),
        this->port_
    );
    mcx_log(LOG_DEBUG, "Sending CFG_target_network_information to slave %d done", slave->id());

    return this->process_response(slave);
}

void Master::CFG_input(std::shared_ptr<SlaveHandle>& slave, uint16_t data_id, uint64_t vr, DcpDataType type) {
    this->prepare_to_receive_response(slave);

    mcx_log(LOG_DEBUG, "Sending CFG_input to slave %d", slave->id());
    this->manager_.CFG_input(slave->id(), data_id, 0, vr, type);
    mcx_log(LOG_DEBUG, "Sending CFG_input to slave %d done", slave->id());

    return this->process_response(slave);
}

void Master::CFG_source_network_information(std::shared_ptr<SlaveHandle>& slave, uint16_t data_id) {
    this->prepare_to_receive_response(slave);

    mcx_log(LOG_DEBUG, "Sending CFG_source_network_information to slave %d", slave->id());
    this->manager_.CFG_source_network_information_UDP(
        slave->id(),
        data_id_,
        asio::ip::address_v4::from_string(slave->ip()).to_ulong(),
        slave->port());
    mcx_log(LOG_DEBUG, "Sending CFG_source_network_information to slave %d done", slave->id());

    return this->process_response(slave);
}

void Master::CFG_parameter(std::shared_ptr<SlaveHandle>& slave, std::shared_ptr<Parameter>& param) {
    this->prepare_to_receive_response(slave);

    mcx_log(LOG_DEBUG, "Sending CFG_parameter to slave %d", slave->id());
    this->manager_.CFG_parameter(
        slave->id(),
        param->vr(),
        param->type(),
        (uint8_t*)param->value(),
        getDcpDataTypeSize(param->type()));
    mcx_log(LOG_DEBUG, "Sending CFG_parameter to slave %d done", slave->id());

    return this->process_response(slave);
}

void Master::CFG_param_network_information(std::shared_ptr<SlaveHandle>& slave, uint16_t param_id) {
    this->prepare_to_receive_response(slave);

    mcx_log(LOG_DEBUG, "Sending CFG_param_network_information to slave %d", slave->id());
    this->manager_.CFG_param_network_information_UDP(
        slave->id(),
        param_id,
        asio::ip::address_v4::from_string(slave->ip()).to_ulong(),
        slave->port());
    mcx_log(LOG_DEBUG, "Sending CFG_param_network_information to slave %d done", slave->id());

    return this->process_response(slave);
}

void Master::CFG_tunable_parameter(std::shared_ptr<SlaveHandle>& slave,
                                   std::shared_ptr<Parameter>& param,
                                   uint16_t param_id) {
    this->prepare_to_receive_response(slave);

    mcx_log(LOG_DEBUG, "Sending CFG_tunable_parameter to slave %d", slave->id());
    this->manager_.CFG_tunable_parameter(slave->id(), param_id, 0, param->vr(), param->type());
    mcx_log(LOG_DEBUG, "Sending CFG_tunable_parameter to slave %d done", slave->id());

    return this->process_response(slave);
}

/*************************************************************************************************/
/*                                  State handler methods                                        */
/*************************************************************************************************/
void Master::nonRealTimeExitSlave(uint8_t slave_id) {
    // notify the thread waiting for the signal
    std::unique_lock<std::mutex> lock(*this->mutices_[slave_id]);

    this->nrt_do_step_done_[slave_id] = true;

    this->condition_vars_[slave_id]->notify_one();
}

void Master::runEnterSlave() {
    std::unique_lock<std::mutex> lock(this->run_mutex_);

    this->slaves_to_be_run_--;

    this->run_cv_.notify_one();
}

void Master::initStepDone(uint8_t slave_id) {
    // notify the thread waiting for the signal
    std::unique_lock<std::mutex> lock(*this->init_mutices_[slave_id]);

    this->init_step_done_[slave_id] = true;

    this->init_condition_vars_[slave_id]->notify_one();
}

void Master::configuredEnter() {
    // notify the thread waiting for the signal
    std::unique_lock<std::mutex> lock(this->configured_mutex_);

    this->not_configured_slaves_--;

    this->configured_cv_.notify_all();
}

/*************************************************************************************************/
/*                                       Utility methods                                         */
/*************************************************************************************************/
void Master::waitForSlave(std::shared_ptr<SlaveHandle>& slave, DcpState state) {
    mcx_log(LOG_DEBUG, "Slave %d: waiting for transition to %s started", slave->id(), to_string(state).c_str());
    while (slave->state() != state && !this->network_error_ && !this->fatal_error_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(Master::sleep_time_));
    }
    mcx_log(LOG_DEBUG, "Slave %d: waiting for transition to %s done", slave->id(), to_string(state).c_str());
}

void Master::invalidSlaveIdReceived(uint8_t slave_id) {
    mcx_log(LOG_ERROR, "Invalid slave id: %d", slave_id);

    // mark all slaves to be in error state because we don't know which slave sent the message
    for (auto& slave : this->slaves_) {

        if (this->error_handling_[slave->id()]) {
            mcx_log(LOG_ERROR, "Error occurred during the shutdown procedure");
            this->fatal_error_ = true;
        } else {
            this->error_state_[slave->id()] = true;
        }

        // signal waiting threads - if any
        std::lock_guard<std::mutex> lock(*this->response_mutices_[slave->id()]);
        this->response_received_[slave->id()] = true;
        this->response_cvs_[slave->id()]->notify_all();
    }
}

std::unique_ptr<uint8_t[]> Master::getNetInfo(std::shared_ptr<SlaveHandle>& slave) {
    std::unique_ptr<uint8_t[]> net_info{new uint8_t[6]};

    *((uint16_t*) net_info.get()) = slave->port();
    try {
        *((uint32_t*) (net_info.get() + 2)) = asio::ip::address_v4::from_string(slave->ip()).to_ulong();
    } catch (asio::system_error &e) {
        mcx_log(LOG_ERROR, "Slave %d: invalid address used for network setup", slave->id());
        mcx_log(LOG_ERROR, "Slave %d: error with code %d occurred meaning %s", slave->id(), e.code().value(), e.what());
        throw EthernetException();
    }

    return net_info;
}

void Master::prepare_wait_for_configuration_state() {
    // prepare the counter to know when all slaves get into the CONFIGURATION state
    try {
        std::lock_guard<std::mutex> lock(this->configuration_mutex_);
        this->slaves_in_configuration_ = this->slaves_.size();
    } catch (std::system_error& e) {
        mcx_log(LOG_ERROR, "Mutex locking failed: %s", e.what());
        throw StateChangeException();
    }
}

void Master::wait_for_configuration_state() {
    // wait for all slaves to get into the CONFIGURATION state
    try {
        std::unique_lock<std::mutex> lock(this->configuration_mutex_);
        bool res = this->configuration_cv_.wait_for(lock, Master::timeout_, [this] {
            return this->slaves_in_configuration_ == 0;
        });

        // check whether the timeout was reached
        if (!res) {
            mcx_log(LOG_ERROR, "Waiting for state change: timeout reached");
            throw StateChangeException();
        }
    } catch (std::system_error& e) {
        mcx_log(LOG_ERROR, "Mutex locking failed: %s", e.what());
        throw StateChangeException();
    }
}

void Master::prepare_to_receive_response(std::shared_ptr<SlaveHandle>& slave) {
    try {
        std::lock_guard<std::mutex> lock(*this->response_mutices_[slave->id()]);
        this->response_received_[slave->id()] = false;
    } catch (std::system_error& e) {
        mcx_log(LOG_ERROR, "Mutex locking failed: %s", e.what());
        throw ResponseException();
    }
}

void Master::process_response(std::shared_ptr<SlaveHandle>& slave) {
    try {
        std::unique_lock<std::mutex> lock(*this->response_mutices_[slave->id()]);
        bool res = this->response_cvs_[slave->id()]->wait_for(lock, Master::timeout_, [slave, this] {
            return this->response_received_[slave->id()];
        });

        // check whether the timeout was reached
        if (!res) {
            mcx_log(LOG_ERROR, "Waiting for response: timeout reached");
            throw ResponseException();
        }

        // check whether the response is okay, i.e. ACK properly received and NACK wasn't received
        if (this->checkForErrors(slave->id()) != RETURN_OK ) {
            throw ResponseException();
        }
    } catch (std::system_error& e) {
        mcx_log(LOG_ERROR, "Mutex locking failed: %s", e.what());
        throw ResponseException();
    }
}

/*************************************************************************************************/
/*                                     Static members                                            */
/*************************************************************************************************/
const std::chrono::milliseconds Master::sleep_time_ = std::chrono::milliseconds(100);
const std::chrono::milliseconds Master::sleep_time_start_ = std::chrono::milliseconds(500);
const std::chrono::seconds Master::sleep_time_error_ = std::chrono::seconds(10);
const std::chrono::milliseconds Master::sleep_time_error_start_ = std::chrono::milliseconds(1000);
const std::chrono::milliseconds Master::timeout_ = std::chrono::milliseconds(1000);
const uint8_t Master::version_major = 1;
const uint8_t Master::version_minor = 0;

/*************************************************************************************************/
/*                                    Private utility methods                                    */
/*************************************************************************************************/
void Master::startManager() {
    try {
        // start() blocks until the socket is closed
        this->manager_.start();
    } catch (std::exception &e) {
        // this catches the case where the slaves have already shut down and do not respond anymore
        mcx_log(LOG_ERROR, "Start failed: %s", e.what());
    }

    // in any case, no more communication with the slaves takes place
    this->network_error_ = true;
}

McxStatus Master::setupSlaveNetwork() {
    mcx_log(LOG_INFO, "Slave network setup started");
    for (auto slave : this->slaves_) {
        try {
            auto net_info = this->getNetInfo(slave);
            this->driver_.getDcpDriver().setSlaveNetworkInformation(slave->id(), net_info.get());
        } catch (EthernetException&) {
            return RETURN_ERROR;
        }
    }
    mcx_log(LOG_INFO, "Slave network setup done");
    return RETURN_OK;
}

void Master::registerSlaves() {
    mcx_log(LOG_INFO, "Registering DCP slaves started");
    for (auto& slave : this->slaves_) {
        mcx_log(LOG_DEBUG, "Registering DCP slave %d", slave->id());

        this->prepare_to_receive_response(slave);

        this->manager_.STC_register(slave->id(),
                                    DcpState::ALIVE,
                                    convertToUUID(slave->uuid()),
                                    slave->mode(),
                                    Master::version_major,
                                    Master::version_minor);

        this->process_response(slave);
    }
    mcx_log(LOG_INFO, "Registering DCP slaves done");
}

void Master::configureSlaves() {
    mcx_log(LOG_INFO, "Configuring DCP slaves started");
    // start slave configuration
    for (auto& slave : this->slaves_) {
        this->configureSlave(slave);
    }
    mcx_log(LOG_INFO, "Configuring DCP slaves done");
}

void Master::configureSlave(std::shared_ptr<SlaveHandle>& slave) {
    mcx_log(LOG_DEBUG, "Slave %d: configuration started", slave->id());

    // TODO add checks if the slave doesn't support the time_res
    // TODO the communication step size should be set properly (model coupling settings), right???
    // TODO ports (UDP/TCP) from the description file
    uint32_t steps = 1;

    // send CFG PDUs
    this->CFG_time_res(slave);

    // outports
    for (Port &outport : slave->outports()) {
        this->CFG_scope(slave, this->data_id_);
        this->CFG_output(slave, this->data_id_, outport.vr());
        this->CFG_steps(slave, this->data_id_, steps);
        this->CFG_target_network_information(slave, this->data_id_);

        outport.set_data_id(this->data_id_);
        this->receive_buffer_.emplace(this->data_id_, std::vector<uint8_t>());
        this->receive_mutices_.emplace(this->data_id_, std::unique_ptr<std::mutex>(new std::mutex()));
        this->data_id_++;
    }

    // inports
    for (Port &inport : slave->inports()) {
        this->CFG_scope(slave, this->data_id_);
        this->CFG_input(slave, this->data_id_, inport.vr(), inport.type());
        this->CFG_source_network_information(slave, this->data_id_);

        auto net_info = this->getNetInfo(slave);
        this->driver_.getDcpDriver().setTargetNetworkInformation(this->data_id_, net_info.get());

        inport.set_data_id(this->data_id_);
        this->data_id_++;
    }

    // fixed parameters
    for (auto& param : slave->fixed_params()) {
        // send initial values to the slave
        this->CFG_parameter(slave, param);
    }

    // tunable parameters
    for (auto& param : slave->tunable_params()) {
        // send initial values to the slave
        this->CFG_parameter(slave, param);

        // prepare for actuation of tunable parameters
        this->CFG_tunable_parameter(slave, param, this->param_id_);
        this->CFG_param_network_information(slave, this->param_id_);

        auto net_info = this->getNetInfo(slave);
        this->driver_.getDcpDriver().setTargetParamNetworkInformation(this->param_id_, net_info.get());

        param->set_param_id(this->param_id_);
        this->param_id_++;
    }

    mcx_log(LOG_DEBUG, "Slave %d: configuration done", slave->id());
    this->prepareSlaveSTC(slave->id());
}

/*************************************************************************************************/
/*                                      Error handling                                           */
/*************************************************************************************************/
void Master::errorResolvedShutDown(std::shared_ptr<SlaveHandle>& slave) {
    this->resetSlaveSTC(slave->id());
    this->waitForSlave(slave, DcpState::CONFIGURATION);
    this->configurationShutDown(slave);
}

void Master::configurationShutDown(std::shared_ptr<SlaveHandle>& slave) {
    this->deregisterSlaveSTC(slave->id(), slave->state());
    this->waitForSlave(slave, DcpState::ALIVE);
}

void Master::errorShutDown() {
    // The wait is added so that slaves can finish processing remaining PDUs (if any).
    // Example where this can be observed is when the slave uses an invalid id for
    // RSP_ACK PDUs. During the processing of an STC_REGISTER PDU, it will respond
    // with an invalid id, but will still change its state to CONFIGURATION. The
    // error handling procedure might be called before a state change notification
    // PDU is received by the master and because of that it won't trigger the slave
    // transition to the ALIVE state properly.
    std::this_thread::sleep_for(Master::sleep_time_error_start_);  // give slaves time to process remaining PDUs

    // try to properly shutdown each slave -> state transitions STOPPED -> ALIVE will be done
    // in the `stop` method. This implies that the `stop` method will be called after this.
    for (auto slave : this->slaves_) {
        this->error_handling_[slave->id()] = true;
        mcx_log(LOG_INFO, "Slave %d: Shutdown started", slave->id());
        if (slave->state() != DcpState::ALIVE && slave->state() != DcpState::ERROR_HANDLING &&
            slave->state() != DcpState::ERROR_RESOLVED && slave->state() != DcpState::CONFIGURATION &&
            slave->state() != DcpState::STOPPING && slave->state() != DcpState::STOPPED) {
            mcx_log(LOG_DEBUG, "Slave %d: Super-state STOPPING detected", slave->id());
        } else if (slave->state() == DcpState::ERROR_RESOLVED) {
            mcx_log(LOG_DEBUG, "Slave %d: State ERROR_RESOLVED detected", slave->id());
            this->errorResolvedShutDown(slave);
        } else if (slave->state() == DcpState::ERROR_HANDLING) {
            mcx_log(LOG_DEBUG, "Slave %d: State ERROR_HANDLING detected", slave->id());
            // try waiting for the slave to fix the error
            std::this_thread::sleep_for(Master::sleep_time_error_);
            if (slave->state() == DcpState::ERROR_RESOLVED) {
                this->errorResolvedShutDown(slave);
            }
        } else if (slave->state() == DcpState::CONFIGURATION) {
            mcx_log(LOG_DEBUG, "Slave %d: State CONFIGURATION detected", slave->id());
            this->configurationShutDown(slave);
        } else if (slave->state() == DcpState::STOPPING) {
            mcx_log(LOG_DEBUG, "Slave %d: State STOPPING detected", slave->id());
            this->waitForSlave(slave, DcpState::STOPPED);
        } else if (slave->state() == DcpState::STOPPED) {
            mcx_log(LOG_DEBUG, "Slave %d: State STOPPED detected", slave->id());
        }
        mcx_log(LOG_INFO, "Slave %d: Shutdown done", slave->id());
        this->error_handling_[slave->id()] = false;
    }
}

McxStatus Master::checkForErrors() {
    if (this->network_error_ || this->fatal_error_) {
        return RETURN_ERROR;
    }

    for (auto& slave : this->slaves_) {
        if (this->error_state_[slave->id()]) {
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

McxStatus Master::checkForErrors(uint8_t slave_id) {
    if (this->network_error_ || this->fatal_error_) {
        return RETURN_ERROR;
    }

    if (this->error_state_[slave_id]) {
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

void Master::processErrors() {
    if (this->network_error_ || this->fatal_error_) {
        return;
    }

    bool need_to_shutdown = false;
    for (auto slave : this->slaves_) {
        if (this->error_state_[slave->id()]) {
            need_to_shutdown = true;
            break;
        }
    }

    if (need_to_shutdown) {
        this->errorShutDown();
    }
}

/*************************************************************************************************/
/*                                       Public API                                              */
/*************************************************************************************************/
McxStatus Master::addSlave(std::shared_ptr<SlaveHandle> slave) {
    this->slaves_.push_back(slave);

    size_t num_slaves = this->slaves_.size();
    if (num_slaves > std::numeric_limits<uint8_t>::max()) {
        mcx_log(LOG_ERROR, "Slave id %z is out of the allowed range [%u, %u]", num_slaves, 0, std::numeric_limits<uint8_t>::max());
        return RETURN_ERROR;
    }
    uint8_t slave_id = static_cast<uint8_t>(num_slaves);

    slave->set_id(slave_id);
    this->error_handling_[slave_id] = false;
    this->mutices_.emplace(slave_id, std::unique_ptr<std::mutex>(new std::mutex()));
    this->condition_vars_.emplace(slave_id, std::unique_ptr<std::condition_variable>(new std::condition_variable()));
    this->nrt_do_step_done_[slave_id] = false;

    this->init_mutices_.emplace(slave_id, std::unique_ptr<std::mutex>(new std::mutex()));
    this->init_condition_vars_.emplace(slave_id, std::unique_ptr<std::condition_variable>(new std::condition_variable()));
    this->init_step_done_[slave_id] = false;


    // initialize condition variables and mutices for ack/nack message processing
    this->response_mutices_.emplace(slave_id, std::unique_ptr<std::mutex>(new std::mutex()));
    this->response_cvs_.emplace(slave_id, std::unique_ptr<std::condition_variable>(new std::condition_variable()));
    this->response_received_[slave_id] = false;

    return RETURN_OK;
}

McxStatus Master::start() {
    if (this->started_) {
        return RETURN_OK;
    }
    this->started_ = true;

    mcx_log(LOG_INFO, "Starting DCP master");
    if (this->setupSlaveNetwork() == RETURN_ERROR) {
        return RETURN_ERROR;
    }

    mcx_log(LOG_DEBUG, "Starting manager thread");
    this->manager_thread_ = std::thread(&Master::startManager, this);
    // This busy wait is important, otherwise the socket might not yet be open.
    std::this_thread::sleep_for( Master::sleep_time_start_ );

    if (this->checkForErrors() != RETURN_OK) {
        this->processErrors();
        return RETURN_ERROR;
    }

    {
        // set the block condition
        std::lock_guard<std::mutex> lock(this->configured_mutex_);
        this->not_configured_slaves_ = this->slaves_.size();
    }

    {
        std::lock_guard<std::mutex> lock(this->run_mutex_);
        this->instructed_to_run_ = 0;
        this->slaves_to_be_run_ = this->slaves_.size();
    }

    try {
        this->prepare_wait_for_configuration_state();
        this->registerSlaves();
        this->wait_for_configuration_state();
        this->configureSlaves();
    } catch (Exception&) {
        this->processErrors();
        return RETURN_ERROR;
    }

    // wait until all the slaves enter the CONFIGURED state
    {
        std::unique_lock<std::mutex> lock(this->configured_mutex_);
        while (this->not_configured_slaves_ != 0) {
            this->configured_cv_.wait_for(lock, Master::sleep_time_);
            if (this->checkForErrors() != RETURN_OK) {
                this->processErrors();
                return RETURN_ERROR;
            }
        }
    }
    this->configure_done_ = true;

    return RETURN_OK;
}

McxStatus Master::stop(uint8_t slave_id) {
    std::shared_ptr<SlaveHandle> slave;
    try {
        slave = this->slaves_.at(slave_id - 1);
    } catch (std::out_of_range&) {
        mcx_log(LOG_ERROR, "Invalid slave id %d", slave_id);
        return RETURN_ERROR;
    }

    if (!this->fatal_error_ && !this->network_error_ && slave->state() != DcpState::ALIVE) {
        // because if an error has happened the slave might have taken anouther route to state ALIVE
        mcx_log(LOG_DEBUG, "Stopping slave %d", slave_id);
        this->stopSlaveSTC(slave_id, slave->state());
        this->waitForSlave(slave, DcpState::ALIVE);
        mcx_log(LOG_DEBUG, "Stopping slave %d done", slave_id);
    }

    return RETURN_OK;
}

McxStatus Master::sendData(uint8_t slave_id) {
    // in case an error already happened initialize the shutdown procedure and signal the error
    if (this->checkForErrors(slave_id) != RETURN_OK) {
        this->processErrors();
        return RETURN_ERROR;
    }

    // otherwise send data to slave inputs
    std::shared_ptr<SlaveHandle> slave = this->slaves_[slave_id - 1];

    for (auto &port : slave->inports()) {
        if (port.type() == DcpDataType::int32) {
            int64_t * value = (int64_t *) port.value();
#ifdef ENABLE_BOUND_CHECKS
            if (*value > static_cast<int64_t>(std::numeric_limits<int32_t>::max())) {
                mcx_log(LOG_ERROR, "DCP (Slave %d): Integer overflow. Cannot store internal 64 bit %lld in 32 bit DCP Port", slave_id, *value);
                return RETURN_ERROR;
            }
#endif // ENABLE_BOUND_CHECKS
        }
        this->manager_.DAT_input_output(port.data_id(),
                                        (uint8_t*)port.value(),
                                        getDcpDataTypeSize(port.type()));
    }

    return RETURN_OK;
}

McxStatus Master::receiveData(uint8_t slave_id, bool blocking) {
    std::shared_ptr<SlaveHandle> slave = this->slaves_[slave_id - 1];

    // in case an error already happened initialize the shutdown procedure and signal the error
    if (this->checkForErrors(slave_id) != RETURN_OK) {
        this->processErrors();
        return RETURN_ERROR;
    }

    // read data from the buffer
    for (auto &port : slave->outports()) {
        auto data_id = port.data_id();
        std::unique_lock<std::mutex> lock(*this->receive_mutices_[data_id]);

        if (blocking) {
            while (this->receive_buffer_[port.data_id()].size() < getDcpDataTypeSize(port.type())) {
                mcx_log(LOG_DEBUG, "Slave %d: waiting to receive data", slave_id);
                std::this_thread::sleep_for(std::chrono::milliseconds(Master::sleep_time_));
            }
        }

        if (this->receive_buffer_[data_id].size() >= getDcpDataTypeSize(port.type())) {
            switch (port.type()) {
                case DcpDataType::float32: {
                    float v{ *(float*)&this->receive_buffer_[data_id][0] };
                    port.set_value(v);
                    break;
                }
                case DcpDataType::float64: {
                    double v{ *(double*)&this->receive_buffer_[data_id][0] };
                    port.set_value(v);
                    break;
                }
                case DcpDataType::int32: {
                    int32_t v{ *(int32_t*)&this->receive_buffer_[data_id][0] };
                    port.set_value(v);
                    break;
                }
                default:
                    break;
            }

            this->receive_buffer_[port.data_id()].erase(this->receive_buffer_[data_id].begin(),
                this->receive_buffer_[data_id].begin() + getDcpDataTypeSize(port.type()));
        }
    }

    return RETURN_OK;
}

McxStatus Master::sendParameter(uint8_t slave_id, std::shared_ptr<Parameter> param) {
    std::shared_ptr<dcp::SlaveHandle> slave;

    try {
        slave = this->slaves_.at(slave_id - 1);
    } catch (std::out_of_range&) {
        mcx_log(LOG_ERROR, "Invalid slave id %d", slave_id);
        return RETURN_ERROR;
    }

    // check state is correct
    if (slave->state() == DcpState::ALIVE ||
        slave->state() == DcpState::CONFIGURATION ||
        slave->state() == DcpState::PREPARING ||
        slave->state() == DcpState::PREPARED ||
        slave->state() == DcpState::CONFIGURING)
    {
        mcx_log(LOG_ERROR, "Slave %d: Sending parameters is not possible from state %s",
               slave_id, to_string(slave->state()).c_str());
        return RETURN_ERROR;
    }

    // send parameter data
    this->manager_.DAT_parameter(param->param_id(),
                                 (uint8_t*)param->value(),
                                 getDcpDataTypeSize(param->type()));

    return RETURN_OK;
}

McxStatus Master::doInitStep(uint8_t slave_id) {
    std::shared_ptr<dcp::SlaveHandle> slave;

    try {
        slave = this->slaves_.at(slave_id - 1);
    } catch (std::out_of_range&) {
        mcx_log(LOG_ERROR, "Unknown slave %d", slave_id);
        return RETURN_ERROR;
    }

    // in case an error already happened initialize the shutdown procedure and signal the error
    if (this->checkForErrors(slave_id) != RETURN_OK) {
        return RETURN_ERROR;
    }

    // check that the slave is in the CONFIGURED state
    if (slave->state() != DcpState::CONFIGURED) {
        mcx_log(LOG_ERROR, "Slave %d: Initialization step not possible from state %s",
               slave_id, to_string(slave->state()).c_str());
        return RETURN_ERROR;
    }

    {
        // prepare return condition
        std::lock_guard<std::mutex> lock(*this->init_mutices_[slave_id]);
        this->init_step_done_[slave->id()] = false;
    }

    // trigger initialization step
    this->initializeSlaveSTC(slave->id());

    {
        // wait for the slave to get back to the CONFIGURED state
        std::unique_lock<std::mutex> lock(*this->init_mutices_[slave_id]);
        this->init_condition_vars_[slave_id]->wait(lock, [this, slave_id] {
            return this->init_step_done_[slave_id];
        });
    }

    return RETURN_OK;
}

McxStatus Master::initDone(uint8_t slave_id) {
    std::shared_ptr<dcp::SlaveHandle> slave;

    try {
        slave = this->slaves_.at(slave_id - 1);
    } catch (std::out_of_range&) {
        mcx_log(LOG_ERROR, "Unknown slave %d", slave_id);
        return RETURN_ERROR;
    }

    // in case an error already happened initialize the shutdown procedure and signal the error
    if (this->checkForErrors(slave_id) != RETURN_OK) {
        return RETURN_ERROR;
    }

    // check that the slave is in the CONFIGURED state
    if (slave->state() != DcpState::CONFIGURED) {
        mcx_log(LOG_ERROR, "Slave %d: Exiting initialization mode not possible from state %s",
               slave_id, to_string(slave->state()).c_str());
        return RETURN_ERROR;
    }

    {
        std::unique_lock<std::mutex> lock(this->run_mutex_);
        this->instructed_to_run_++;
    }

    // run the slave
    this->runSlaveSTC(slave->id(), DcpState::CONFIGURED);

    {
        // wait for all slaves to enter on of the RUN states
        std::unique_lock<std::mutex> lock(this->run_mutex_);
        if (this->instructed_to_run_ == this->slaves_.size()) {
            this->run_cv_.wait(lock, [this] {
                return this->slaves_to_be_run_ == 0;
            });
        }
    }

    return RETURN_OK;
}

McxStatus Master::doStep(uint8_t slave_id) {
    std::shared_ptr<dcp::SlaveHandle> slave;
    DcpState slave_state = DcpState::ALIVE;

    try {
        slave = this->slaves_.at(slave_id - 1);
        slave_state = slave->state();
    } catch (std::out_of_range& e) {
        UNUSED(e);
        mcx_log(LOG_ERROR, "Invalid slave id %d", slave_id);
        return RETURN_ERROR;
    }

    // in case an error already happened initialize the shutdown procedure and signal the error
    if (this->checkForErrors(slave_id) != RETURN_OK) {
        this->processErrors();
        return RETURN_ERROR;
    }

    // check that the state transition is allowed (the slave is in one of the Run states)
    if (slave_state != DcpState::SYNCHRONIZING &&
            slave_state != DcpState::SYNCHRONIZED &&
            slave_state != DcpState::RUNNING) {
        mcx_log(LOG_ERROR, "Slave %d: DoStep transition not possible from state %s",
               slave_id, to_string(slave_state).c_str());
        return RETURN_ERROR;
    }

    // check that the slave operates in NRT mode
    if (slave->mode() != DcpOpMode::NRT) {
        mcx_log(LOG_ERROR, "Slave %d: Explicit DoStep only allowed for NRT mode", slave_id);
        return RETURN_ERROR;
    }

    {
        // set the return condition
        std::lock_guard<std::mutex> lock(*this->mutices_[slave_id]);
        this->nrt_do_step_done_[slave->id()] = false;
    }

    // initiate the DoStep
    this->doStepSlaveSTC(slave->id(), slave_state);

    {
        // wait for the slave to get back to the start state
        std::unique_lock<std::mutex> lock(*this->mutices_[slave_id]);
        this->condition_vars_[slave_id]->wait(lock, [this, slave_id] {
            return this->nrt_do_step_done_[slave_id] == true;
        });
    }

    return RETURN_OK;
}
}