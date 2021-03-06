/**
 * Copyright (c) 2017-2018. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef WRENCH_BATCHSERVICEMESSAGE_H
#define WRENCH_BATCHSERVICEMESSAGE_H


#include "wrench/services/compute/ComputeServiceMessage.h"
#include "BatchJob.h"

namespace wrench{
    /***********************/
    /** \cond INTERNAL     */
    /***********************/

    /**
     * @brief Top-level BatchServiceMessage class
     */
    class BatchServiceMessage : public ComputeServiceMessage {
    protected:
        BatchServiceMessage(std::string name, double payload);
    };

    /**
     * @brief BatchServiceJobRequestMessage class
     */
    class BatchServiceJobRequestMessage : public BatchServiceMessage {
    public:
        BatchServiceJobRequestMessage(std::string answer_mailbox, BatchJob* job , double payload);

        /** @brief The mailbox to answer to */
        std::string answer_mailbox;
        /** @brief The batch job */
        BatchJob* job;
    };

    /**
     * @brief AlarmJobTimeOutMessage class
     */
    class AlarmJobTimeOutMessage : public ServiceMessage {
    public:
        AlarmJobTimeOutMessage(WorkflowJob* job,double payload);
        WorkflowJob* job;
    };

    /**
     * @brief AlarmNotifyBatschedMessage class
     */
    class AlarmNotifyBatschedMessage : public ServiceMessage {
    public:
        AlarmNotifyBatschedMessage(std::string job_id,double payload);
        std::string job_id;
    };


    /**
     * @brief BatchSimulationBeginsToSchedulerMessage class
     */
    class BatchSimulationBeginsToSchedulerMessage : public BatchServiceMessage {
    public:
        BatchSimulationBeginsToSchedulerMessage(std::string answer_mailbox, std::string job_args_to_scheduler, double payload);

        /** @brief The mailbox to answer to */
        std::string answer_mailbox;
        /** @brief JSON data arguments to the scheduler */
        std::string job_args_to_scheduler;
    };

    /**
     * @brief BatchSchedReadyMessage class
     */
    class BatchSchedReadyMessage : public BatchServiceMessage {
    public:
        BatchSchedReadyMessage(std::string answer_mailbox, double payload);

        /** @brief The mailbox to answer to */
        std::string answer_mailbox;
    };


    /**
     * @brief BatchExecuteJobFromBatschedMessage class
     */
    class BatchExecuteJobFromBatSchedMessage : public BatchServiceMessage {
    public:
        BatchExecuteJobFromBatSchedMessage(std::string answer_mailbox, std::string batsched_decision_reply, double payload);

        /** @brief The mailbox to answer to */
        std::string answer_mailbox;

        /** @brief The decisions reply by batsched to the batchservice */
        std::string batsched_decision_reply;
    };

    /**
     * @brief BatchQueryAnswerMessage class
     */
    class BatchQueryAnswerMessage : public BatchServiceMessage {
    public:
        BatchQueryAnswerMessage(double batsched_job_estimated_time, double payload);

        /** @brief The estimated time to run the job done by batsched */
        double estimated_waiting_time;
    };

//    /**
//     * @brief BatchFakeReplyMessage class
//     */
//    class BatchFakeJobSubmissionReplyMessage : public BatchServiceMessage {
//    public:
//        BatchFakeJobSubmissionReplyMessage(std::string json_data_string, double payload);
//
//        /** @brief The resources info in json data string */
//        std::string json_data_string;
//    };


    /**
     * @brief BatchJobSubmissionToSchedulerMessage class
     */
    class BatchJobSubmissionToSchedulerMessage : public BatchServiceMessage {
    public:
        BatchJobSubmissionToSchedulerMessage(std::string answer_mailbox, WorkflowJob* job, std::string job_args_to_scheduler, double payload);

        /** @brief The mailbox to answer to */
        std::string answer_mailbox;
        /** @brief The batch job */
        WorkflowJob *job;
        /** @brief JSON data arguments to the scheduler */
        std::string job_args_to_scheduler;
    };

    /**
     * @brief BatchJobReplyFromSchedulerMessage class
     */
    class BatchJobReplyFromSchedulerMessage : public BatchServiceMessage {
    public:
        BatchJobReplyFromSchedulerMessage(std::string reply, double payload);

        /** @brief The message replied by the scheduler */
        std::string reply;
    };



}


#endif //WRENCH_BATCHSERVICEMESSAGE_H
