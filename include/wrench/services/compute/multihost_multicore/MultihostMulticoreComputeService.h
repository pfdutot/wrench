/**
 * Copyright (c) 2017. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef WRENCH_MULTIHOSTMULTICORECOMPUTESERVICE_H
#define WRENCH_MULTIHOSTMULTICORECOMPUTESERVICE_H


#include <queue>

#include "wrench/services/compute/ComputeService.h"
#include "wrench/services/compute/standard_job_executor/StandardJobExecutor.h"
#include "MultihostMulticoreComputeServiceProperty.h"

namespace wrench {

    class Simulation;

    class StorageService;

    class FailureCause;

    class Alarm;

    /**  @brief A ComputeService that runs on a set of
     *   multi-core hosts
     */
    class MultihostMulticoreComputeService : public ComputeService {

    private:

        std::map<std::string, std::string> default_property_values = {
                {MultihostMulticoreComputeServiceProperty::STOP_DAEMON_MESSAGE_PAYLOAD,                    "1024"},
                {MultihostMulticoreComputeServiceProperty::DAEMON_STOPPED_MESSAGE_PAYLOAD,                 "1024"},
                {MultihostMulticoreComputeServiceProperty::SUBMIT_STANDARD_JOB_REQUEST_MESSAGE_PAYLOAD,    "1024"},
                {MultihostMulticoreComputeServiceProperty::SUBMIT_STANDARD_JOB_ANSWER_MESSAGE_PAYLOAD,     "1024"},
                {MultihostMulticoreComputeServiceProperty::JOB_TYPE_NOT_SUPPORTED_MESSAGE_PAYLOAD,         "1024"},
                {MultihostMulticoreComputeServiceProperty::NOT_ENOUGH_CORES_MESSAGE_PAYLOAD,               "1024"},
                {MultihostMulticoreComputeServiceProperty::STANDARD_JOB_DONE_MESSAGE_PAYLOAD,              "1024"},
                {MultihostMulticoreComputeServiceProperty::STANDARD_JOB_FAILED_MESSAGE_PAYLOAD,            "1024"},
                {MultihostMulticoreComputeServiceProperty::TERMINATE_STANDARD_JOB_REQUEST_MESSAGE_PAYLOAD, "1024"},
                {MultihostMulticoreComputeServiceProperty::TERMINATE_STANDARD_JOB_ANSWER_MESSAGE_PAYLOAD,  "1024"},
                {MultihostMulticoreComputeServiceProperty::SUBMIT_PILOT_JOB_REQUEST_MESSAGE_PAYLOAD,       "1024"},
                {MultihostMulticoreComputeServiceProperty::SUBMIT_PILOT_JOB_ANSWER_MESSAGE_PAYLOAD,        "1024"},
                {MultihostMulticoreComputeServiceProperty::PILOT_JOB_STARTED_MESSAGE_PAYLOAD,              "1024"},
                {MultihostMulticoreComputeServiceProperty::PILOT_JOB_EXPIRED_MESSAGE_PAYLOAD,              "1024"},
                {MultihostMulticoreComputeServiceProperty::PILOT_JOB_FAILED_MESSAGE_PAYLOAD,               "1024"},
                {MultihostMulticoreComputeServiceProperty::TERMINATE_PILOT_JOB_REQUEST_MESSAGE_PAYLOAD,    "1024"},
                {MultihostMulticoreComputeServiceProperty::TERMINATE_PILOT_JOB_ANSWER_MESSAGE_PAYLOAD,     "1024"},
                {MultihostMulticoreComputeServiceProperty::RESOURCE_DESCRIPTION_REQUEST_MESSAGE_PAYLOAD,     "1024"},
                {MultihostMulticoreComputeServiceProperty::RESOURCE_DESCRIPTION_ANSWER_MESSAGE_PAYLOAD,      "1024"},
                {MultihostMulticoreComputeServiceProperty::THREAD_STARTUP_OVERHEAD,                        "0.0"},
                {MultihostMulticoreComputeServiceProperty::JOB_SELECTION_POLICY,                           "FCFS"},
                {MultihostMulticoreComputeServiceProperty::RESOURCE_ALLOCATION_POLICY,                     "aggressive"},
                {MultihostMulticoreComputeServiceProperty::TASK_SCHEDULING_CORE_ALLOCATION_ALGORITHM,      "maximum"},
                {MultihostMulticoreComputeServiceProperty::TASK_SCHEDULING_TASK_SELECTION_ALGORITHM,       "maximum_flops"},
                {MultihostMulticoreComputeServiceProperty::TASK_SCHEDULING_HOST_SELECTION_ALGORITHM,       "best_fit"},
        };


    public:

        // Public Constructor
        MultihostMulticoreComputeService(const std::string &hostname,
                                         const bool supports_standard_jobs,
                                         const bool supports_pilot_jobs,
                                         const std::set<std::tuple<std::string, unsigned long, double>> compute_resources,
                                         StorageService *default_storage_service,
                                         std::map<std::string, std::string> plist = {});

        // Public Constructor
        MultihostMulticoreComputeService(const std::string &hostname,
                                         bool supports_standard_jobs,
                                         bool supports_pilot_jobs,
                                         const std::set<std::string> compute_hosts,
                                         StorageService *default_storage_service,
                                         std::map<std::string, std::string> plist = {});


        /***********************/
        /** \cond DEVELOPER    */
        /***********************/

        void submitStandardJob(StandardJob *job, std::map<std::string, std::string> &service_specific_args) override;

        void submitPilotJob(PilotJob *job, std::map<std::string, std::string> &service_specific_args) override;

        void terminateStandardJob(StandardJob *job) override;

        void terminatePilotJob(PilotJob *job) override;

        /***********************/
        /** \endcond           */
        /***********************/

        /***********************/
        /** \cond INTERNAL     */
        /***********************/

        ~MultihostMulticoreComputeService();

        /***********************/
        /** \endcond           */
        /***********************/

    private:

        friend class Simulation;

        // Low-level Constructor
        MultihostMulticoreComputeService(const std::string &hostname,
                                         bool supports_standard_jobs,
                                         bool supports_pilot_jobs,
                                         std::set<std::tuple<std::string, unsigned long, double>> compute_resources,
                                         std::map<std::string, std::string> plist,
                                         double ttl,
                                         PilotJob *pj, std::string suffix,
                                         StorageService *default_storage_service);

        // Low-level constructor helper method
        void initiateInstance(const std::string &hostname,
                              bool supports_standard_jobs,
                              bool supports_pilot_jobs,
                              std::set<std::tuple<std::string, unsigned long, double>> compute_resources,
                              std::map<std::string, std::string> plist,
                              double ttl,
                              PilotJob *pj,
                              StorageService *default_storage_service);

        std::set<std::tuple<std::string, unsigned long, double>> compute_resources;

        // Core availabilities (for each hosts, how many cores and how many bytes of RAM are currently available on it)
        std::map<std::string, std::pair<unsigned long, double>> core_and_ram_availabilities;
        unsigned long total_num_cores;

        double ttl;
        bool has_ttl;
        double death_date;
        std::shared_ptr<Alarm> death_alarm = nullptr;

        PilotJob *containing_pilot_job; // In case this service is in fact a pilot job

        // Set of current standard job executors
        std::set<std::shared_ptr<StandardJobExecutor>> standard_job_executors;
        // Set of completed standard job executors
        std::set<std::shared_ptr<StandardJobExecutor>> completed_job_executors;

        // Set of running jobs
        std::set<WorkflowJob *> running_jobs;

        // Queue of pending jobs (standard or pilot) that haven't begun executing
        std::deque<WorkflowJob *> pending_jobs;

        int main() override;

        // Helper functions to make main() a bit more palatable

        void terminate(bool notify_pilot_job_submitters);

        void terminateAllPilotJobs();

        void failCurrentStandardJobs(std::shared_ptr<FailureCause> cause);

        void processStandardJobCompletion(StandardJobExecutor *executor, StandardJob *job);

        void processStandardJobFailure(StandardJobExecutor *executor,
                                       StandardJob *job,
                                       std::shared_ptr<FailureCause> cause);

        void processPilotJobCompletion(PilotJob *job);

        void processStandardJobTerminationRequest(StandardJob *job, std::string answer_mailbox);

        void processPilotJobTerminationRequest(PilotJob *job, std::string answer_mailbox);

        bool processNextMessage();

        bool dispatchNextPendingJob();

        bool dispatchStandardJob(StandardJob *job);

        bool dispatchPilotJob(PilotJob *job);

        std::set<std::tuple<std::string, unsigned long, double>> computeResourceAllocation(StandardJob *job);

        std::set<std::tuple<std::string, unsigned long, double>> computeResourceAllocationAggressive(StandardJob *job);


//        void createWorkForNewlyDispatchedJob(StandardJob *job);

        void terminateRunningStandardJob(StandardJob *job);

        void terminateRunningPilotJob(PilotJob *job);

        void failPendingStandardJob(StandardJob *job, std::shared_ptr<FailureCause> cause);

        void failRunningStandardJob(StandardJob *job, std::shared_ptr<FailureCause> cause);

        void processGetResourceInformation(const std::string &answer_mailbox);

        void processSubmitStandardJob(const std::string &answer_mailbox, StandardJob *job,
                                      std::map<std::string, std::string> &service_specific_arguments);

        void processSubmitPilotJob(const std::string &answer_mailbox, PilotJob *job);
    };
};


#endif //WRENCH_MULTIHOSTMULTICORECOMPUTESERVICE_H
