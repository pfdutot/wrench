/**
 * Copyright (c) 2017. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <climits>

#include "wrench/services/storage/simple/SimpleStorageService.h"
#include "wrench/services/ServiceMessage.h"
#include "services/storage/StorageServiceMessage.h"
#include "wrench/simgrid_S4U_util/S4U_Mailbox.h"
#include "wrench/logging/TerminalOutput.h"
#include "wrench/simgrid_S4U_util/S4U_Simulation.h"
#include "wrench/workflow/WorkflowFile.h"
#include "wrench/exceptions/WorkflowExecutionException.h"
#include "wrench/services/storage/simple/NetworkConnectionManager.h"


XBT_LOG_NEW_DEFAULT_CATEGORY(simple_storage_service, "Log category for Simple Storage Service");


namespace wrench {

    /**
    * @brief Generate a unique number
    *
    * @return a unique number
    */
    unsigned long SimpleStorageService::getNewUniqueNumber() {
      static unsigned long sequence_number = 0;
      return (sequence_number++);
    }

    /**
     * @brief Destructor
     */
    SimpleStorageService::~SimpleStorageService() {
      this->default_property_values.clear();
    }

    /**
     * @brief Public constructor
     *
     * @param hostname: the name of the host on which to start the service
     * @param capacity: the storage capacity in bytes
     * @param plist: a property list ({} means "use all defaults")
     */
    SimpleStorageService::SimpleStorageService(std::string hostname,
                                               double capacity,
                                               std::map<std::string, std::string> plist) :
            SimpleStorageService(std::move(hostname), capacity, plist, "_" + std::to_string(getNewUniqueNumber())) {
      if (this->getPropertyValueAsString("MAX_NUM_CONCURRENT_DATA_CONNECTIONS") == "infinity") {
        this->num_concurrent_connections = ULONG_MAX;
      } else {
        this->num_concurrent_connections = (unsigned long) (this->getPropertyValueAsDouble("MAX_NUM_CONCURRENT_DATA_CONNECTIONS"));
      }
      this->network_connection_manager =  std::unique_ptr<NetworkConnectionManager>(
              new NetworkConnectionManager(this->num_concurrent_connections));

    }

    /**
     * @brief Private constructor
     *
     * @param hostname: the name of the host on which to start the service
     * @param capacity: the storage capacity in bytes
     * @param plist: the property list
     * @param suffix: the suffix (for the service name)
     *
     * @throw std::invalid_argument
     */
    SimpleStorageService::SimpleStorageService(
            std::string hostname,
            double capacity,
            std::map<std::string, std::string> plist,
            std::string suffix) :
            StorageService(std::move(hostname), "simple" + suffix, "simple" + suffix, capacity) {

      this->setProperties(this->default_property_values, plist);
    }

    /**
     * @brief Main method of the daemon
     *
     * @return 0 on termination
     */
    int SimpleStorageService::main() {

      TerminalOutput::setThisProcessLoggingColor(WRENCH_LOGGING_COLOR_CYAN);

      WRENCH_INFO("Simple Storage Service %s starting on host %s (capacity: %lf, holding %ld files, listening on %s)",
                  this->getName().c_str(),
                  S4U_Simulation::getHostName().c_str(),
                  this->capacity,
                  this->stored_files.size(),
                  this->mailbox_name.c_str());

      /** Main loop **/
      bool should_add_incoming_control_connection = true;
      bool should_continue = true;

      while (should_continue) {

        // Post a recv on my standard mailbox_name in case there is none pending
        if (should_add_incoming_control_connection) {
          this->network_connection_manager->addConnection(std::unique_ptr<NetworkConnection>(
                  new NetworkConnection(NetworkConnection::INCOMING_CONTROL, nullptr, this->mailbox_name, "")
          ));
          should_add_incoming_control_connection = false;
        }

        // Wait a connection
        std::pair<std::unique_ptr<NetworkConnection>, bool> finished_connection;

        finished_connection = this->network_connection_manager->waitForNetworkConnection();

        if (std::get<0>(finished_connection)->type == NetworkConnection::INCOMING_CONTROL) {
          should_continue = processControlMessage(std::move(std::get<0>(finished_connection)));
          should_add_incoming_control_connection = true;
        } else {
          should_continue = processDataConnection(std::move(std::move(std::get<0>(finished_connection))));
        }
      }
//
//      //probably we have to remove everything leftover on the pending communications
//      std::vector<std::unique_ptr<S4U_PendingCommunication>>::iterator it;
//      for (it = this->pending_incoming_communications.begin();
//           it != this->pending_incoming_communications.end(); it++) {
//        if (it->get() != nullptr) {
//          it->reset();
//        }
//      }

      WRENCH_INFO("Simple Storage Service %s on host %s terminated!",
                  this->getName().c_str(),
                  S4U_Simulation::getHostName().c_str());

      return 0;
    }


    /**
     * @brief Process a received control message
     *
     * @param comm: the pending communication
     * @return false if the daemon should terminate
     */
    bool SimpleStorageService::processControlMessage(std::unique_ptr<NetworkConnection> connection) {

      // Get the message
      std::unique_ptr<SimulationMessage> message;
      try {
        message = connection->comm->wait();
      } catch (std::shared_ptr<NetworkError> &cause) {
        WRENCH_INFO("Network error while receiving a control message... ignoring");
        return true;
      }

      if (message == nullptr) {
        WRENCH_INFO("Got a NULL message. This likely means that we're all done...Aborting!");
        return false;
      }

      WRENCH_INFO("Got a [%s] message", message->getName().c_str());

      if (auto msg = dynamic_cast<ServiceStopDaemonMessage *>(message.get())) {
        try {
          S4U_Mailbox::putMessage(msg->ack_mailbox,
                                  new ServiceDaemonStoppedMessage(this->getPropertyValueAsDouble(
                                          SimpleStorageServiceProperty::DAEMON_STOPPED_MESSAGE_PAYLOAD)));
        } catch (std::shared_ptr<NetworkError> &cause) {
          return false;
        }
        return false;

      } else if (auto msg = dynamic_cast<StorageServiceFreeSpaceRequestMessage *>(message.get())) {
        double free_space = this->capacity - this->occupied_space;

        try {
          S4U_Mailbox::dputMessage(msg->answer_mailbox,
                                   new StorageServiceFreeSpaceAnswerMessage(free_space, this->getPropertyValueAsDouble(
                                           SimpleStorageServiceProperty::FREE_SPACE_ANSWER_MESSAGE_PAYLOAD)));
        } catch (std::shared_ptr<NetworkError> &cause) {
          return false;
        }
        return true;

      } else if (auto msg = dynamic_cast<StorageServiceFileDeleteRequestMessage *>(message.get())) {

        bool success = true;
        std::shared_ptr<FailureCause> failure_cause = nullptr;
        if (this->stored_files.find(msg->file) == this->stored_files.end()) {
          success = false;
          failure_cause = std::shared_ptr<FailureCause>(new FileNotFound(msg->file, this));
        } else {
          this->removeFileFromStorage(msg->file);
        }

        // Send an asynchronous reply
        try {
          S4U_Mailbox::dputMessage(msg->answer_mailbox,
                                   new StorageServiceFileDeleteAnswerMessage(msg->file,
                                                                             this,
                                                                             success,
                                                                             failure_cause,
                                                                             this->getPropertyValueAsDouble(
                                                                                     SimpleStorageServiceProperty::FILE_DELETE_ANSWER_MESSAGE_PAYLOAD)));
        } catch (std::shared_ptr<NetworkError> &cause) {
          return true;
        }

        return true;

      } else if (auto msg = dynamic_cast<StorageServiceFileLookupRequestMessage *>(message.get())) {

        bool file_found = (this->stored_files.find(msg->file) != this->stored_files.end());
        try {
          S4U_Mailbox::dputMessage(msg->answer_mailbox,
                                   new StorageServiceFileLookupAnswerMessage(msg->file, file_found,
                                                                             this->getPropertyValueAsDouble(
                                                                                     SimpleStorageServiceProperty::FILE_LOOKUP_ANSWER_MESSAGE_PAYLOAD)));
        } catch (std::shared_ptr<NetworkError> &cause) {
          return true;
        }

        return true;

      } else if (auto msg = dynamic_cast<StorageServiceFileWriteRequestMessage *>(message.get())) {

        return processFileWriteRequest(msg->file, msg->answer_mailbox);

      } else if (auto msg = dynamic_cast<StorageServiceFileReadRequestMessage *>(message.get())) {

        return processFileReadRequest(msg->file, msg->answer_mailbox, msg->mailbox_to_receive_the_file_content);

      } else if (auto msg = dynamic_cast<StorageServiceFileCopyRequestMessage *>(message.get())) {

        return processFileCopyRequest(msg->file, msg->src, msg->answer_mailbox);

      } else {
        throw std::runtime_error(
                "SimpleStorageService::processControlMessage(): Unexpected [" + message->getName() + "] message");
      }
    }

    /**
     * @brief Handle a file write request
     *
     * @param file: the file to write
     * @param answer_mailbox: the mailbox to which the reply should be sent
     * @return true if this process should keep running
     */
    bool SimpleStorageService::processFileWriteRequest(WorkflowFile *file, std::string answer_mailbox) {

      // If the file is already there, send back a failure
//      if (this->stored_files.find(file) != this->stored_files.end()) {
//        try {
//          S4U_Mailbox::putMessage(answer_mailbox,
//                                  new StorageServiceFileWriteAnswerMessage(file,
//                                                                           this,
//                                                                           false,
//                                                                           std::shared_ptr<FailureCause>(
//                                                                                   new StorageServiceFileAlreadyThere(
//                                                                                           file,
//                                                                                           this)),
//                                                                           "",
//                                                                           this->getPropertyValueAsDouble(
//                                                                                   SimpleStorageServiceProperty::FILE_WRITE_ANSWER_MESSAGE_PAYLOAD)));
//        } catch (std::shared_ptr<NetworkError> &cause) {
//          return true;
//        }
//        return true;
//      }

      // If the file is not already there, do a capacity check/update
      // (If the file is already there, then there will just be an overwrite. Not that
      // if the overwrite fails, then the file will disappear)
      if (this->stored_files.find(file) != this->stored_files.end()) {

        // Check the file size and capacity, and reply "no" if not enough space
        if (file->getSize() > (this->capacity - this->occupied_space)) {
          try {
            S4U_Mailbox::putMessage(answer_mailbox,
                                    new StorageServiceFileWriteAnswerMessage(file,
                                                                             this,
                                                                             false,
                                                                             std::shared_ptr<FailureCause>(
                                                                                     new StorageServiceNotEnoughSpace(
                                                                                             file,
                                                                                             this)),
                                                                             "",
                                                                             this->getPropertyValueAsDouble(
                                                                                     SimpleStorageServiceProperty::FILE_WRITE_ANSWER_MESSAGE_PAYLOAD)));
          } catch (std::shared_ptr<NetworkError> &cause) {
            return true;
          }
          return true;
        }

        // Update occupied space, in advance (will have to be decreased later in case of failure)
        this->occupied_space += file->getSize();
      }

      // Generate a mailbox_name name on which to receive the file
      std::string file_reception_mailbox = S4U_Mailbox::generateUniqueMailboxName("file_reception");

      // Reply with a "go ahead, send me the file" message
      try {
        S4U_Mailbox::putMessage(answer_mailbox,
                                new StorageServiceFileWriteAnswerMessage(file,
                                                                         this,
                                                                         true,
                                                                         nullptr,
                                                                         file_reception_mailbox,
                                                                         this->getPropertyValueAsDouble(
                                                                                 SimpleStorageServiceProperty::FILE_WRITE_ANSWER_MESSAGE_PAYLOAD)));
      } catch (std::shared_ptr<NetworkError> &cause) {
        return true;
      }

      this->network_connection_manager->addConnection(std::unique_ptr<NetworkConnection>(
              new NetworkConnection(NetworkConnection::INCOMING_DATA, file, file_reception_mailbox, "")));

      return true;
    }


    /**
     * @brief Handle a file read request
     * @param file: the file
     * @param answer_mailbox: the mailbox to which the answer should be sent
     * @param mailbox_to_receive_the_file_content: the mailbox to which the file will be sent
     * @return
     */
    bool SimpleStorageService::processFileReadRequest(WorkflowFile *file, std::string answer_mailbox,
                                                      std::string mailbox_to_receive_the_file_content) {

      // Figure out whether this succeeds or not
      bool success = true;
      std::shared_ptr<FailureCause> failure_cause = nullptr;
      if (this->stored_files.
              find(file) == this->stored_files.
              end()) {
        WRENCH_INFO("Received a a read request for a file I don't have (%s)", this->getName().c_str());
        success = false;
        failure_cause = std::shared_ptr<FailureCause>(new FileNotFound(file, this));
      }

      // Send back the corresponding ack, asynchronously and in a "fire and forget" fashion
      try {
        S4U_Mailbox::dputMessage(answer_mailbox,
                                 new StorageServiceFileReadAnswerMessage(file, this, success, failure_cause,
                                                                         this->getPropertyValueAsDouble(
                                                                                 SimpleStorageServiceProperty::FILE_READ_ANSWER_MESSAGE_PAYLOAD)));
      } catch (std::shared_ptr<NetworkError> &cause) {
        return true;
      }


      // If success, then follow up with sending the file (ASYNCHRONOUSLY!)
      if (success) {
        this->network_connection_manager->addConnection(std::unique_ptr<NetworkConnection>(
                new NetworkConnection(NetworkConnection::OUTGOING_DATA, file, mailbox_to_receive_the_file_content, "")
        ));
      }

      return true;
    }

    /**
     * @brief Handle a file copy request
     * @param file: the file
     * @param src: the storage service that holds the file
     * @param answer_mailbox: the mailbox to which the answer should be sent
     * @return
     */
    bool
    SimpleStorageService::processFileCopyRequest(WorkflowFile *file, StorageService *src, std::string answer_mailbox) {

//      // If the file is already here, send back a failure
//      if (this->stored_files.find(file) != this->stored_files.end()) {
//        WRENCH_INFO("Cannot perform file copy because file is already there");
//        try {
//          S4U_Mailbox::putMessage(answer_mailbox,
//                                  new StorageServiceFileCopyAnswerMessage(file,
//                                                                           this,
//                                                                           false,
//                                                                           std::shared_ptr<FailureCause>(
//                                                                                   new StorageServiceFileAlreadyThere(
//                                                                                           file,
//                                                                                           this)),
//                                                                           this->getPropertyValueAsDouble(
//                                                                                   SimpleStorageServiceProperty::FILE_COPY_ANSWER_MESSAGE_PAYLOAD)));
//        } catch (std::shared_ptr<NetworkError> &cause) {
//          return true;
//        }
//        return true;
//      }


      // If the file is not already there, do a capacity check/update
      // (If the file is already there, then there will just be an overwrite. Not that
      // if the overwrite fails, then the file will disappear)
      if (this->stored_files.find(file) == this->stored_files.end()) {

        // Figure out whether this succeeds or not
        if (file->getSize() > this->capacity - this->occupied_space) {
          WRENCH_INFO("Cannot perform file copy due to lack of space");
          try {
            S4U_Mailbox::putMessage(answer_mailbox,
                                    new StorageServiceFileCopyAnswerMessage(file, this, false,
                                                                            std::shared_ptr<FailureCause>(
                                                                                    new StorageServiceNotEnoughSpace(
                                                                                            file,
                                                                                            this)),
                                                                            this->getPropertyValueAsDouble(
                                                                                    SimpleStorageServiceProperty::FILE_COPY_ANSWER_MESSAGE_PAYLOAD)));
          } catch (std::shared_ptr<NetworkError> &cause) {
            return true;
          }
          return true;
        }

        // Update occupied space, in advance (will have to be decreased later in case of failure)
        this->occupied_space += file->getSize();
      }

      WRENCH_INFO("Asynchronously copying file %s from storage service %s",
                  file->getId().c_str(),
                  src->getName().c_str());

      // Create a unique mailbox_name on which to receive the file
      std::string file_reception_mailbox = S4U_Mailbox::generateUniqueMailboxName("file_reception");

      // Initiate an ASYNCHRONOUS file read from the source
      try {
        src->initiateFileRead(file_reception_mailbox, file);
      } catch (WorkflowExecutionException &e) {
        try {
          S4U_Mailbox::putMessage(answer_mailbox,
                                  new StorageServiceFileCopyAnswerMessage(file, this, false, e.getCause(),
                                                                          this->getPropertyValueAsDouble(
                                                                                  SimpleStorageServiceProperty::FILE_COPY_ANSWER_MESSAGE_PAYLOAD)));
        } catch (std::shared_ptr<NetworkError> &cause) {
          return true;
        }
      }


      this->network_connection_manager->addConnection(std::unique_ptr<NetworkConnection>(
              new NetworkConnection(NetworkConnection::INCOMING_DATA, file, file_reception_mailbox, answer_mailbox)
      ));

      return true;
    }


    /**
    * @brief Process a completed data connection
    *
    * @param connection: the completed data connection
    * @return false if the daemon should terminate
    *
    * @throw std::runtime_error
    */
    bool SimpleStorageService::processDataConnection(std::unique_ptr<NetworkConnection> connection) {

      if (connection->type == NetworkConnection::INCOMING_DATA) {
        return processIncomingDataConnection(std::move(connection));
      } else if (connection->type == NetworkConnection::OUTGOING_DATA) {
        return processOutgoingDataConnection(std::move(connection));
      } else {
        throw std::invalid_argument("SimpleStorageService::processDataConnection(): invalid connection type");
      }
    }

    bool SimpleStorageService::processIncomingDataConnection(std::unique_ptr<NetworkConnection> connection) {

      // Get the message
      std::unique_ptr<SimulationMessage> message = connection->getMessage();

      if (message == nullptr) {
        WRENCH_INFO("SimpleStorageService::processDataConnection(): Communication failure when receiving file '%s",
                    connection->file->getId().c_str());
        // Process the failure, meaning, just re-decrease the occupied space
        this->occupied_space -= connection->file->getSize();
        // And if this was an overwrite, now we lost the file!!!
        this->stored_files.erase(connection->file);

        WRENCH_INFO(
                "Sending back an ack since this was a file copy and some client is waiting for me to say something");
        try {
          S4U_Mailbox::putMessage(connection->ack_mailbox,
                                  new StorageServiceFileCopyAnswerMessage(connection->file, this, false, connection->failure_cause,
                                                                          this->getPropertyValueAsDouble(
                                                                                  SimpleStorageServiceProperty::FILE_COPY_ANSWER_MESSAGE_PAYLOAD)));
        } catch (std::shared_ptr<NetworkError> &cause) {
          return true;
        }
        return true;
      }

      WRENCH_INFO("Got a [%s] message", message->getName().c_str());

      if (auto msg = dynamic_cast<StorageServiceFileContentMessage *>(message.get())) {

        if (msg->file != connection->file) {
          throw std::runtime_error(
                  "SimpleStorageService::processDataConnection(): Mismatch between received file and expected file... a bug in SimpleStorageService");
        }

        // Add the file to my storage (this will not add a duplicate in case of an overwrite, because it's a set)
        this->stored_files.insert(connection->file);

        // Send back the corresponding ack?
        if (not connection->ack_mailbox.empty()) {
          WRENCH_INFO(
                  "Sending back an ack since this was a file copy and some client is waiting for me to say something");
          try {
            S4U_Mailbox::putMessage(connection->ack_mailbox,
                                    new StorageServiceFileCopyAnswerMessage(connection->file, this, true, nullptr,
                                                                            this->getPropertyValueAsDouble(
                                                                                    SimpleStorageServiceProperty::FILE_COPY_ANSWER_MESSAGE_PAYLOAD)));
          } catch (std::shared_ptr<NetworkError> &cause) {
            return true;
          }
        }

        return true;
      } else {
        throw std::runtime_error(
                "SimpleStorageService::processControlMessage(): Unexpected [" + message->getName() + "] message");
      }

    }

    bool SimpleStorageService::processOutgoingDataConnection(std::unique_ptr<NetworkConnection> connection) {
      // Nothing to do
      return true;
    }

};
