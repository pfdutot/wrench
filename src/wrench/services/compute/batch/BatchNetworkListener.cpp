/**
 * Copyright (c) 2017. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "wrench/exceptions/WorkflowExecutionException.h"
#include "wrench/logging/TerminalOutput.h"
#include "wrench/services/compute/batch/BatchServiceMessage.h"
#include "wrench/services/compute/batch/BatchServiceProperty.h"
#include "wrench/services/compute/batch/BatchNetworkListener.h"
#include "wrench/simgrid_S4U_util/S4U_Mailbox.h"
#include "wrench/simgrid_S4U_util/S4U_Simulation.h"
#include <json.hpp>
#ifdef ENABLE_BATSCHED
#include <zmq.hpp>
#include <zmq.h>
#endif

XBT_LOG_NEW_DEFAULT_CATEGORY(batch_network_listener_service, "Log category for Batch Network Listener Service");

class context_t;
namespace wrench {

    /**
    * @brief Constructor
    * @param hostname: the hostname on which to start the service
    * @param self_port the port to listen to messages from scheduler
    * @param sched_port the port to send messages to scheduler
    */
    BatchNetworkListener::BatchNetworkListener(std::string hostname, std::string batch_service_mailbox,
                                               std::string sched_port,
                                               NETWORK_LISTENER_TYPE MY_TYPE, std::string data_to_send,
                                               std::map<std::string, std::string> plist) :
            BatchNetworkListener(hostname, batch_service_mailbox,
                                 sched_port, MY_TYPE, data_to_send, plist, "") {
    }


    /**
    * @brief Constructor
    * @param hostname: the hostname on which to start the service
    * @param sched_port the port to send messages to scheduler
    * @param suffix the suffix to append
    */
    BatchNetworkListener::BatchNetworkListener(
            std::string hostname, std::string batch_service_mailbox, std::string sched_port,
            NETWORK_LISTENER_TYPE MY_TYPE, std::string data_to_send, std::map<std::string, std::string> plist,
            std::string suffix = "") :
            Service(hostname, "batch_network_listener" + suffix, "batch_network_listener" + suffix) {

      // Start the daemon on the same host
      this->sched_port = sched_port;
      this->MY_LISTENER_TYPE = MY_TYPE;
      this->data_to_send = data_to_send;
      this->batch_service_mailbox = batch_service_mailbox;
      // Set default and specified properties
      this->setProperties(this->default_property_values, plist);

    }

    int BatchNetworkListener::main() {
      TerminalOutput::setThisProcessLoggingColor(WRENCH_LOGGING_COLOR_CYAN);

#ifdef ENABLE_BATSCHED

      WRENCH_INFO("Batch Network Listener Service starting on host %s!", S4U_Simulation::getHostName().c_str());

      /** Main loop **/
      if (MY_LISTENER_TYPE == NETWORK_LISTENER_TYPE::SENDER_RECEIVER) {
        if (this->data_to_send.empty()) {
          throw std::runtime_error(
                  "BatchNetworkListener::BatchNetworkListener():Network sending process has no data to send"
          );
        }
        this->send_receive();
      } else {
        throw std::runtime_error(
                "BatchNetworkListener::BatchNetworkListener():Invalid Network Listener type given"
        );
      }

#endif

      WRENCH_INFO("Batch Network Listener Service on host %s terminated!", S4U_Simulation::getHostName().c_str());
      return 0;
    }


    void BatchNetworkListener::sendExecuteMessageToBatchService(std::string answer_mailbox, std::string execute_job_reply_data) {
      try {
        S4U_Mailbox::putMessage(this->batch_service_mailbox,
                                new BatchExecuteJobFromBatSchedMessage(answer_mailbox, execute_job_reply_data,
                                                                       this->getPropertyValueAsDouble(
                                                                               BatchServiceProperty::BATCH_SCHED_READY_PAYLOAD)));
      } catch (std::shared_ptr<NetworkError> &cause) {
        throw WorkflowExecutionException(cause);
      }
    }

    void BatchNetworkListener::sendQueryAnswerMessageToBatchService(double estimated_waiting_time) {
      try {
        S4U_Mailbox::putMessage(this->batch_service_mailbox,
                                new BatchQueryAnswerMessage(estimated_waiting_time,
                                                                       this->getPropertyValueAsDouble(
                                                                               BatchServiceProperty::BATCH_SCHED_READY_PAYLOAD)));
      } catch (std::shared_ptr<NetworkError> &cause) {
        throw WorkflowExecutionException(cause);
      }
    }

    void BatchNetworkListener::send_receive() {
#ifdef ENABLE_BATSCHED
      zmq::context_t context(1);
      zmq::socket_t socket(context, ZMQ_REQ);
      socket.connect("tcp://localhost:" + this->sched_port);

      zmq::message_t request(strlen(this->data_to_send.c_str()));
      memcpy(request.data(), this->data_to_send.c_str(), strlen(this->data_to_send.c_str()));
      socket.send(request);

      //  Get the reply.
      zmq::message_t reply;
      socket.recv(&reply);

      std::string reply_data;
      reply_data = std::string(static_cast<char *>(reply.data()), reply.size());

      nlohmann::json reply_decisions;
      nlohmann::json decision_events;
      reply_decisions = nlohmann::json::parse(reply_data);
      decision_events = reply_decisions["events"];

      std::string answer_mailbox = S4U_Mailbox::generateUniqueMailboxName("batch_network_listener_mailbox");
      for (auto decisions:decision_events) {
        std::string decision_type = decisions["type"];
        double decision_timestamp = decisions["timestamp"];
        double time_to_sleep = S4U_Simulation::getClock() - decision_timestamp;
        nlohmann::json execute_json_data = decisions["data"];
        std::string job_reply_data = execute_json_data.dump();
        if (time_to_sleep > 0) {
          S4U_Simulation::sleep(time_to_sleep);
        }
        if (strcmp(decision_type.c_str(), "EXECUTE_JOB") == 0) {
          sendExecuteMessageToBatchService(answer_mailbox, job_reply_data);
        } else if (strcmp(decision_type.c_str(), "ANSWER") == 0) {
          double estimated_waiting_time = execute_json_data["estimate_waiting_time"]["estimated_waiting_time"];
          sendQueryAnswerMessageToBatchService(estimated_waiting_time);
        }
      }

      double decision_now = reply_decisions["now"];
      double time_to_sleep_again = S4U_Simulation::getClock() - decision_now;
      if (time_to_sleep_again > 0) {
        S4U_Simulation::sleep(time_to_sleep_again);
      }


      try {
        S4U_Mailbox::putMessage(this->batch_service_mailbox,
                                new BatchSchedReadyMessage(answer_mailbox,
                                                           this->getPropertyValueAsDouble(
                                                                   BatchServiceProperty::BATCH_SCHED_READY_PAYLOAD)));
      } catch (std::shared_ptr<NetworkError> &cause) {
        throw WorkflowExecutionException(cause);
      }
#endif
    }


}
