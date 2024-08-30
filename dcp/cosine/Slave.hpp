// Copyright: 2024 AVL List GmbH

#ifndef SLAVE_H_
#define SLAVE_H_

#include <dcp/helper/Helper.hpp>
#include <dcp/log/OstreamLog.hpp>
#include <dcp/logic/DcpManagerSlave.hpp>
#include <dcp/model/constant/DcpLogLevel.hpp>
#include <dcp/model/constant/DcpDataType.hpp>
#include <dcp/driver/ethernet/udp/UdpDriver.hpp>
#include <dcp/xml/DcpSlaveDescriptionWriter.hpp>

#include <cstdint>
#include <cstdio>
#include <stdarg.h>
#include <thread>
#include <cmath>
#include <memory>
#include <string>


class Slave {
public:
    Slave(const std::string & host, uint16_t port, const std::string & output_file) :
        host_(host),
        port_(port),
        output_file_(output_file == "" ? "slave.dcpx" : output_file),
        std_log_(std::cout)
    {
        this->udp_driver_ = std::make_shared<UdpDriver>(this->host_, this->port_);

        this->generate_slave_description();

        this->manager_ = std::make_shared<DcpManagerSlave>(*this->slave_description_, this->udp_driver_->getDcpDriver());

        // set callbacks
        this->manager_->setInitializeCallback<SYNC>(std::bind(&Slave::initialize, this));
        this->manager_->setConfigureCallback<SYNC>(std::bind(&Slave::configure, this));

        this->manager_->setSynchronizingStepCallback<SYNC>(std::bind(&Slave::doStep, this, std::placeholders::_1));
        this->manager_->setSynchronizedStepCallback<SYNC>(std::bind(&Slave::doStep, this, std::placeholders::_1));
        this->manager_->setRunningStepCallback<SYNC>(std::bind(&Slave::doStep, this, std::placeholders::_1));

        this->manager_->setSynchronizingNRTStepCallback<SYNC>(std::bind(&Slave::doStep, this, std::placeholders::_1));
        this->manager_->setSynchronizedNRTStepCallback<SYNC>(std::bind(&Slave::doStep, this, std::placeholders::_1));
        this->manager_->setRunningNRTStepCallback<SYNC>(std::bind(&Slave::doStep, this, std::placeholders::_1));

        this->manager_->setTimeResListener<SYNC>(std::bind(&Slave::setTimeRes, this, std::placeholders::_1, std::placeholders::_2));

        // display log messages in the console
        this->manager_->addLogListener(std::bind(&OstreamLog::logOstream, this->std_log_, std::placeholders::_1));
        this->manager_->setGenerateLogString(true);
    }

    void generate_slave_description() {
        slave_description_ = make_SlaveDescription_ptr(1, 0, "DCPCosine", "b5279485-720d-4542-9f29-bee4d9a75ef9");

        // <OpMode>
        slave_description_->OpMode.SoftRealTime = make_SoftRealTime_ptr();
        slave_description_->OpMode.NonRealTime = make_NonRealTime_ptr();

        // <TimeRes>
        slave_description_->TimeRes.resolutionRanges.push_back(make_ResolutionRange(1, 100000, 10000));

        // <TransportProtocols>
        slave_description_->TransportProtocols.UDP_IPv4 = make_UDP_ptr();
        slave_description_->TransportProtocols.UDP_IPv4->Control = make_Control_ptr(host_, port_);
        slave_description_->TransportProtocols.UDP_IPv4->DAT_input_output = make_DAT_ptr(host_);
        slave_description_->TransportProtocols.UDP_IPv4->DAT_input_output->availablePortRanges.push_back(make_AvailablePortRange(2048, 65535));
        slave_description_->TransportProtocols.UDP_IPv4->DAT_parameter = make_DAT_ptr(host_);
        slave_description_->TransportProtocols.UDP_IPv4->DAT_parameter->availablePortRanges.push_back(make_AvailablePortRange(2048, 65535));

        // <CapabilityFlags>
        slave_description_->CapabilityFlags.canAcceptConfigPdus = true;
        slave_description_->CapabilityFlags.canHandleReset = true;
        slave_description_->CapabilityFlags.canHandleVariableSteps = false;
        slave_description_->CapabilityFlags.canMonitorHeartbeat = false;
        slave_description_->CapabilityFlags.canProvideLogOnRequest = true;
        slave_description_->CapabilityFlags.canProvideLogOnNotification = true;

        // <Variables>
        std::shared_ptr<CommonCausality_t> amplitude = make_CommonCausality_ptr<float64_t>();
        amplitude->Float64->start = std::make_shared<std::vector<float64_t>>();
        amplitude->Float64->start->push_back(4.0);
        slave_description_->Variables.push_back(make_Variable_input("amplitude", 1, amplitude));

        std::shared_ptr<Output_t> cosine = make_Output_ptr<float64_t>();
        cosine->Float64->start = std::make_shared<std::vector<float64_t>>();
        cosine->Float64->start->push_back(4.0);
        slave_description_->Variables.push_back(make_Variable_output("cosine", 0, cosine));

        // <Log>
        slave_description_->Log = make_Log_ptr();
        slave_description_->Log->categories.push_back(make_Category(1, "DCP_SLAVE"));
        slave_description_->Log->templates.push_back(make_Template(1, 1, (uint8_t) DcpLogLevel::LVL_INFORMATION, "[Time = %float64]: cosine = %float64 * cos(%float64) = %float64"));
        slave_description_->Log->templates.push_back(make_Template(2, 1, (uint8_t) DcpLogLevel::LVL_INFORMATION, "INIT: cosine = %float64 * cos(%float64) = %float64"));
        slave_description_->Log->templates.push_back(make_Template(3, 1, (uint8_t) DcpLogLevel::LVL_INFORMATION, "CONFIG: cosine = %float64"));
        slave_description_->Log->templates.push_back(make_Template(4, 1, (uint8_t) DcpLogLevel::LVL_INFORMATION, "CONFIG: amplitude = %float64"));

        writeDcpSlaveDescription(*slave_description_, output_file_.c_str());
    }

    void configure() {
        this->current_step_ = 0;
        this->time_ = 0.0;

        // inputs
        this->amplitude_ = this->manager_->getInput<float64_t*>(this->amplitude_vr_);
        *this->amplitude_ = 4.0;
#if defined(DEBUG)
        this->manager_->Log(this->config_log_amplitude_, *this->amplitude_);
#endif

        // outputs
        this->cosine_ = this->manager_->getOutput<float64_t*>(this->cosine_vr_);
        *this->cosine_ = 4.0;
#if defined(DEBUG)
        this->manager_->Log(this->config_log_cosine_, *this->cosine_);
#endif
    }

    void initialize() {
        *this->cosine_ = *this->amplitude_ * std::cos(0.0);
#if defined(DEBUG)
        this->manager_->Log(this->init_log_cosine_, *this->amplitude_, this->time_, *this->cosine_);
#endif
    }

    void doStep(uint64_t steps) {
        double delta_time = static_cast<double>(this->numerator_) / this->denominator_ * steps;

        *this->cosine_ = *this->amplitude_ * std::cos(this->time_ + delta_time);

#if defined(DEBUG)
        this->manager_->Log(this->sim_log_cosine_, this->time_, *this->amplitude_,
                            this->time_ + delta_time, *this->cosine_);
#endif

        this->time_ += delta_time;
        this->current_step_ += steps;
    }

    void setTimeRes(const uint32_t numerator, const uint32_t denominator) {
        this->numerator_ = numerator;
        this->denominator_ = denominator;
    }

    void start() {
        this->manager_->start();
    }

private:
    // DCPLib handles
    OstreamLog std_log_;

    std::shared_ptr<DcpManagerSlave> manager_;
    std::shared_ptr<UdpDriver> udp_driver_;
    std::shared_ptr<SlaveDescription_t> slave_description_;

    // control variables
    uint64_t current_step_;
    float64_t time_;

    uint32_t numerator_;
    uint32_t denominator_;

    std::string host_;
    uint16_t port_;

    std::string output_file_;

    // value references
    const uint32_t cosine_vr_ = 0;
    const uint32_t amplitude_vr_ = 1;

    // variables
    float64_t *cosine_;
    float64_t *amplitude_;

    // log templates
    const LogTemplate sim_log_cosine_ = LogTemplate(1, 1, DcpLogLevel::LVL_INFORMATION,
                                                    "[Time = %float64]: cosine = %float64 * cos(%float64) = %float64",
                                                    {DcpDataType::float64, DcpDataType::float64, DcpDataType::float64, DcpDataType::float64});

    const LogTemplate init_log_cosine_ = LogTemplate(2, 1, DcpLogLevel::LVL_INFORMATION,
                                                     "INIT: cosine = %float64 * cos(%float64) = %float64",
                                                     {DcpDataType::float64, DcpDataType::float64, DcpDataType::float64});

    const LogTemplate config_log_cosine_ = LogTemplate(3, 1, DcpLogLevel::LVL_INFORMATION,
                                                       "CONFIG: cosine = %float64",
                                                       {DcpDataType::float64});

    const LogTemplate config_log_amplitude_ = LogTemplate(4, 1, DcpLogLevel::LVL_INFORMATION,
                                                          "CONFIG: amplitude = %float64",
                                                          {DcpDataType::float64});
};

#endif /* SLAVE_H_ */
