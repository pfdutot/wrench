/**
 * Copyright (c) 2017. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <wrench-dev.h>
#include <wrench/simgrid_S4U_util/S4U_Mailbox.h>
#include <wrench/simulation/SimulationMessage.h>
#include "services/compute/standard_job_executor/StandardJobExecutorMessage.h"
#include <gtest/gtest.h>
#include <wrench/services/compute/batch/BatchService.h>
#include <wrench/services/compute/batch/BatchServiceMessage.h>
#include <wrench/util/TraceFileLoader.h>
#include "wrench/workflow/job/PilotJob.h"

#include "../include/TestWithFork.h"

XBT_LOG_NEW_DEFAULT_CATEGORY(batch_service_test, "Log category for BatchServiceTest");


class BatchServiceTest : public ::testing::Test {

public:
    wrench::StorageService *storage_service1 = nullptr;
    wrench::StorageService *storage_service2 = nullptr;
    wrench::ComputeService *compute_service = nullptr;
    wrench::Simulation *simulation;


    void do_StandardJobTaskTest_test();

    void do_MultipleStandardTaskTest_test();

    void do_PilotJobTaskTest_test();

    void do_StandardPlusPilotJobTaskTest_test();

    void do_InsufficientCoresTaskTest_test();

    void do_BestFitTaskTest_test();

    void do_noArgumentsJobSubmissionTest_test();

    void do_StandardJobTimeOutTaskTest_test();

    void do_PilotJobTimeOutTaskTest_test();

    void do_StandardJobInsidePilotJobTimeOutTaskTest_test();

    void do_StandardJobInsidePilotJobSucessTaskTest_test();

    void do_InsufficientCoresInsidePilotJobTaskTest_test();

    void do_DifferentBatchAlgorithmsSubmissionTest_test();

    void do_BatchTraceFileJobSubmissionTest_test();

    void do_BatchJobBasicEstimateWaitingTimeTest_test();

    void do_BatchJobEstimateWaitingTimeTest_test();

    void do_BatchJobLittleComplexEstimateWaitingTimeTest_test();

protected:
    BatchServiceTest() {

      // Create the simplest workflow
      workflow = std::unique_ptr<wrench::Workflow>(new wrench::Workflow());

      // Create a four-host 10-core platform file
      std::string xml = "<?xml version='1.0'?>"
              "<!DOCTYPE platform SYSTEM \"http://simgrid.gforge.inria.fr/simgrid/simgrid.dtd\">"
              "<platform version=\"4.1\"> "
              "   <zone id=\"AS0\" routing=\"Full\"> "
              "       <host id=\"Host1\" speed=\"1f\" core=\"10\"/> "
              "       <host id=\"Host2\" speed=\"1f\" core=\"10\"/> "
              "       <host id=\"Host3\" speed=\"1f\" core=\"10\"/> "
              "       <host id=\"Host4\" speed=\"1f\" core=\"10\"/> "
              "       <link id=\"1\" bandwidth=\"5000GBps\" latency=\"0us\"/>"
              "       <link id=\"2\" bandwidth=\"0.0001MBps\" latency=\"1000000us\"/>"
              "       <link id=\"3\" bandwidth=\"0.0001MBps\" latency=\"1000000us\"/>"
              "       <route src=\"Host3\" dst=\"Host1\"> <link_ctn id=\"2\"/> </route>"
              "       <route src=\"Host4\" dst=\"Host1\"> <link_ctn id=\"2\"/> </route>"
              "       <route src=\"Host1\" dst=\"Host2\"> <link_ctn id=\"1\""
              "/> </route>"
              "   </zone> "
              "</platform>";
      FILE *platform_file = fopen(platform_file_path.c_str(), "w");
      fprintf(platform_file, "%s", xml.c_str());
      fclose(platform_file);

    }

    std::string platform_file_path = "/tmp/platform.xml";
    std::unique_ptr<wrench::Workflow> workflow;

};

/**********************************************************************/
/**  ONE STANDARD JOB SUBMISSION TASK SIMULATION TEST ON ONE HOST    **/
/**********************************************************************/

class OneStandardJobSubmissionTestWMS : public wrench::WMS {

public:
    OneStandardJobSubmissionTestWMS(BatchServiceTest *test,
                                    const std::set<wrench::ComputeService *> &compute_services,
                                    const std::set<wrench::StorageService *> &storage_services,
                                    std::string hostname) :
            wrench::WMS(nullptr, nullptr,  compute_services, storage_services, {}, nullptr, hostname,
                        "test") {
      this->test = test;
    }

private:

    BatchServiceTest *test;

    int main() {
      // Create a job manager
      std::shared_ptr<wrench::JobManager> job_manager = this->createJobManager();

      {
        // Create a sequential task that lasts one min and requires 2 cores
        wrench::WorkflowTask *task = this->workflow->addTask("task", 60, 2, 2, 1.0);
        task->addInputFile(this->workflow->getFileById("input_file"));
        task->addOutputFile(this->workflow->getFileById("output_file"));


        // Create a StandardJob with some pre-copies and post-deletions (not useful, but this is testing after all)

        wrench::StandardJob *job = job_manager->createStandardJob(
                {task},
                {
                        {*(task->getInputFiles().begin()),  this->test->storage_service1},
                        {*(task->getOutputFiles().begin()), this->test->storage_service1}
                },
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *, wrench::StorageService *>(
                        this->workflow->getFileById("input_file"), this->test->storage_service1,
                        this->test->storage_service2)},
                {},
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *>(this->workflow->getFileById("input_file"),
                                                                              this->test->storage_service2)});

        std::map<std::string, std::string> batch_job_args;
        batch_job_args["-N"] = "2";
        batch_job_args["-t"] = "5"; //time in minutes
        batch_job_args["-c"] = "4"; //number of cores per node
        try {
          job_manager->submitJob(job, this->test->compute_service, batch_job_args);
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error(
                  "Got some exception"
          );
        }


        // Wait for a workflow execution event
        std::unique_ptr<wrench::WorkflowExecutionEvent> event;
        try {
          event = this->workflow->waitForNextExecutionEvent();
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error("Error while getting and execution event: " + e.getCause()->toString());
        }
        switch (event->type) {
          case wrench::WorkflowExecutionEvent::STANDARD_JOB_COMPLETION: {
            // success, do nothing for now
            break;
          }
          default: {
            throw std::runtime_error("Unexpected workflow execution event: " + std::to_string((int) (event->type)));
          }
        }
        this->workflow->removeTask(task);
      }

      return 0;
    }
};

TEST_F(BatchServiceTest, OneStandardJobSubmissionTest) {
  DO_TEST_WITH_FORK(do_StandardJobTaskTest_test);
}


void BatchServiceTest::do_StandardJobTaskTest_test() {


  // Create and initialize a simulation
  auto simulation = new wrench::Simulation();
  int argc = 1;
  auto argv = (char **) calloc(1, sizeof(char *));
  argv[0] = strdup("batch_service_test");

  EXPECT_NO_THROW(simulation->init(&argc, argv));

  // Setting up the platform
  EXPECT_NO_THROW(simulation->instantiatePlatform(platform_file_path));

  // Get a hostname
  std::string hostname = simulation->getHostnameList()[0];

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service1 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service2 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Batch Service
  ASSERT_NO_THROW(compute_service = simulation->add(
                  new wrench::BatchService(hostname, true, true, simulation->getHostnameList(),
                                           storage_service1, {})));

  simulation->setFileRegistryService(new wrench::FileRegistryService(hostname));

  // Create a WMS
  wrench::WMS *wms = nullptr;
  EXPECT_NO_THROW(wms = simulation->add(
          new OneStandardJobSubmissionTestWMS(
                  this,  {compute_service}, {storage_service1, storage_service2}, hostname)));

  EXPECT_NO_THROW(wms->addWorkflow(std::move(workflow.get())));

  // Create two workflow files
  wrench::WorkflowFile *input_file = this->workflow->addFile("input_file", 10000.0);
  wrench::WorkflowFile *output_file = this->workflow->addFile("output_file", 20000.0);

  // Staging the input_file on the storage service
  EXPECT_NO_THROW(simulation->stageFile(input_file, storage_service1));

  // Running a "run a single task" simulation
  // Note that in these tests the WMS creates workflow tasks, which a user would
  // of course not be likely to do
  EXPECT_NO_THROW(simulation->launch());

  delete simulation;

  free(argv[0]);
  free(argv);
}


/**********************************************************************/
/**  ONE PILOT JOB SUBMISSION TASK SIMULATION TEST ON ONE HOST                **/
/**********************************************************************/

class OnePilotJobSubmissionTestWMS : public wrench::WMS {

public:
    OnePilotJobSubmissionTestWMS(BatchServiceTest *test,
                                 const std::set<wrench::ComputeService *> &compute_services,
                                 const std::set<wrench::StorageService *> &storage_services,
                                 std::string hostname) :
            wrench::WMS(nullptr, nullptr,  compute_services, storage_services, {}, nullptr, hostname, "test") {
      this->test = test;
    }

private:

    BatchServiceTest *test;

    int main() {
      // Create a job manager
      std::shared_ptr<wrench::JobManager> job_manager = this->createJobManager();

      {
        // Create a pilot job that needs 1 host, 1 code, 0 bytes of RAM and 30 seconds
        wrench::PilotJob *pilot_job = job_manager->createPilotJob(this->workflow, 1, 1, 0, 30);

        std::map<std::string, std::string> batch_job_args;
        batch_job_args["-N"] = "1";
        batch_job_args["-t"] = "1"; //time in minutes
        batch_job_args["-c"] = "4"; //number of cores per node

        // Submit a pilot job
        try {
          job_manager->submitJob((wrench::WorkflowJob *) pilot_job, this->test->compute_service, batch_job_args);
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error(
                  "Got some exception " + std::string(e.what())
          );
        }

        // Wait for a workflow execution event
        std::unique_ptr<wrench::WorkflowExecutionEvent> event;
        try {
          event = workflow->waitForNextExecutionEvent();
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error("Error while getting and execution event: " + e.getCause()->toString());
        }
        switch (event->type) {
          case wrench::WorkflowExecutionEvent::PILOT_JOB_START: {
            //std::cout<<"Got the pilot job started message\n";
            // success, do nothing for now
            break;
          }
          default: {
            throw std::runtime_error("Unexpected workflow execution event: " + std::to_string((int) (event->type)));
          }
        }

        try {
          event = workflow->waitForNextExecutionEvent();
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error("Error while getting and execution event: " + e.getCause()->toString());
        }
        switch (event->type) {
          case wrench::WorkflowExecutionEvent::PILOT_JOB_EXPIRATION: {
            // success, do nothing for now
            break;
          }
          default: {
            throw std::runtime_error("Unexpected workflow execution event: " + std::to_string((int) (event->type)));
          }
        }
      }

      return 0;
    }
};

TEST_F(BatchServiceTest, OnePilotJobSubmissionTest) {
  DO_TEST_WITH_FORK(do_PilotJobTaskTest_test);
}

void BatchServiceTest::do_PilotJobTaskTest_test() {

  // Create and initialize a simulation
  wrench::Simulation *simulation = new wrench::Simulation();
  int argc = 1;
  char **argv = (char **) calloc(1, sizeof(char *));
  argv[0] = strdup("batch_service_test");

  EXPECT_NO_THROW(simulation->init(&argc, argv));

  // Setting up the platform
  EXPECT_NO_THROW(simulation->instantiatePlatform(platform_file_path));

  // Get a hostname
  std::string hostname = simulation->getHostnameList()[0];

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service1 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service2 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Batch Service
  EXPECT_NO_THROW(compute_service = simulation->add(
                  new wrench::BatchService(hostname, true, true, simulation->getHostnameList(),
                                           storage_service1, {})));

  // Create a WMS
  wrench::WMS *wms = nullptr;
  EXPECT_NO_THROW(wms = simulation->add(
          new OnePilotJobSubmissionTestWMS(
                  this,
                          {compute_service}, {storage_service1, storage_service2}, hostname)));

  EXPECT_NO_THROW(wms->addWorkflow(workflow.get()));

  simulation->setFileRegistryService(new wrench::FileRegistryService(hostname));

  // Create two workflow files
  wrench::WorkflowFile *input_file = this->workflow->addFile("input_file", 10000.0);
  wrench::WorkflowFile *output_file = this->workflow->addFile("output_file", 20000.0);

  // Staging the input_file on the storage service
  EXPECT_NO_THROW(simulation->stageFile(input_file, storage_service1));


  // Running a "run a single task" simulation
  // Note that in these tests the WMS creates workflow tasks, which a user would
  // of course not be likely to do
  EXPECT_NO_THROW(simulation->launch());

  delete simulation;

  free(argv[0]);
  free(argv);
}


/**********************************************************************/
/**  STANDARD + PILOT JOB SUBMISSION TASK SIMULATION TEST ON ONE-ONE HOST                **/
/**********************************************************************/

class StandardPlusPilotJobSubmissionTestWMS : public wrench::WMS {

public:
    StandardPlusPilotJobSubmissionTestWMS(BatchServiceTest *test,
                                          const std::set<wrench::ComputeService *> &compute_services,
                                          const std::set<wrench::StorageService *> &storage_services,
                                          std::string hostname) :
            wrench::WMS(nullptr, nullptr,  compute_services, storage_services, {}, nullptr, hostname, "test") {
      this->test = test;
    }


private:

    BatchServiceTest *test;

    int main() {
      // Create a job manager
      std::shared_ptr<wrench::JobManager> job_manager = this->createJobManager();

      {
        // Create a sequential task that lasts one min and requires 2 cores
        wrench::WorkflowTask *task = this->workflow->addTask("task", 50, 2, 2, 1.0);
        task->addInputFile(workflow->getFileById("input_file"));
        task->addOutputFile(workflow->getFileById("output_file"));

        // Create a StandardJob with some pre-copies and post-deletions (not useful, but this is testing after all)

        wrench::StandardJob *job = job_manager->createStandardJob(
                {task},
                {
                        {*(task->getInputFiles().begin()),  this->test->storage_service1},
                        {*(task->getOutputFiles().begin()), this->test->storage_service1}
                },
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *, wrench::StorageService *>(
                        workflow->getFileById("input_file"), this->test->storage_service1,
                        this->test->storage_service2)},
                {},
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *>(workflow->getFileById("input_file"),
                                                                              this->test->storage_service2)});

        std::map<std::string, std::string> batch_job_args;
        batch_job_args["-N"] = "1";
        batch_job_args["-t"] = "2"; //time in minutes
        batch_job_args["-c"] = "4"; //number of cores per node
        try {
          job_manager->submitJob(job, this->test->compute_service, batch_job_args);
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error(
                  "Got some exception"
          );
        }
        // Wait for a workflow execution event
        std::unique_ptr<wrench::WorkflowExecutionEvent> event;
        try {
          event = workflow->waitForNextExecutionEvent();
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error("Error while getting and execution event: " + e.getCause()->toString());
        }
        switch (event->type) {
          case wrench::WorkflowExecutionEvent::STANDARD_JOB_COMPLETION: {
            // success, do nothing for now
            break;
          }
          default: {
            throw std::runtime_error("Unexpected workflow execution event: " + std::to_string((int) (event->type)));
          }
        }
        workflow->removeTask(task);
      }

      {
        // Create a pilot job that needs 1 host, 1 code, 0 bytes of RAM, and 30 seconds
        wrench::PilotJob *pilot_job = job_manager->createPilotJob(this->workflow, 1, 1, 0.0, 30);

        std::map<std::string, std::string> batch_job_args;
        batch_job_args["-N"] = "1";
        batch_job_args["-t"] = "1"; //time in minutes
        batch_job_args["-c"] = "4"; //number of cores per node

        // Submit a pilot job
        try {
          job_manager->submitJob((wrench::WorkflowJob *) pilot_job, this->test->compute_service, batch_job_args);
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error(
                  "Got some exception " + std::string(e.what())
          );
        }

        // Wait for a workflow execution event
        std::unique_ptr<wrench::WorkflowExecutionEvent> event;
        try {
          event = workflow->waitForNextExecutionEvent();
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error("Error while getting and execution event: " + e.getCause()->toString());
        }
        switch (event->type) {
          case wrench::WorkflowExecutionEvent::PILOT_JOB_START: {
            // success, do nothing for now
            break;
          }
          default: {
            throw std::runtime_error("Unexpected workflow execution event: " + std::to_string((int) (event->type)));
          }
        }

        try {
          event = workflow->waitForNextExecutionEvent();
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error("Error while getting and execution event: " + e.getCause()->toString());
        }
        switch (event->type) {
          case wrench::WorkflowExecutionEvent::PILOT_JOB_EXPIRATION: {
            // success, do nothing for now
            break;
          }
          default: {
            throw std::runtime_error("Unexpected workflow execution event: " + std::to_string((int) (event->type)));
          }
        }
      }

      return 0;
    }
};

TEST_F(BatchServiceTest, StandardPlusPilotJobSubmissionTest) {
  DO_TEST_WITH_FORK(do_StandardPlusPilotJobTaskTest_test);
}

void BatchServiceTest::do_StandardPlusPilotJobTaskTest_test() {

  // Create and initialize a simulation
  wrench::Simulation *simulation = new wrench::Simulation();
  int argc = 1;
  char **argv = (char **) calloc(1, sizeof(char *));
  argv[0] = strdup("batch_service_test");

  EXPECT_NO_THROW(simulation->init(&argc, argv));

  // Setting up the platform
  EXPECT_NO_THROW(simulation->instantiatePlatform(platform_file_path));

  // Get a hostname
  std::string hostname = simulation->getHostnameList()[0];

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service1 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service2 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Batch Service
  EXPECT_NO_THROW(compute_service = simulation->add(
                  new wrench::BatchService(hostname, true, true,
                                           simulation->getHostnameList(),
                                           storage_service1, {})));

  // Create a WMS
  wrench::WMS *wms = nullptr;
  EXPECT_NO_THROW(wms = simulation->add(
          new StandardPlusPilotJobSubmissionTestWMS(
                  this,
                          {compute_service}, {storage_service1, storage_service2}, hostname)));

  EXPECT_NO_THROW(wms->addWorkflow(workflow.get()));

  simulation->setFileRegistryService(new wrench::FileRegistryService(hostname));

  // Create two workflow files
  wrench::WorkflowFile *input_file = this->workflow->addFile("input_file", 10000.0);
  wrench::WorkflowFile *output_file = this->workflow->addFile("output_file", 20000.0);

  // Staging the input_file on the storage service
  EXPECT_NO_THROW(simulation->stageFile(input_file, storage_service1));


  // Running a "run a single task" simulation
  // Note that in these tests the WMS creates workflow tasks, which a user would
  // of course not be likely to do
  EXPECT_NO_THROW(simulation->launch());

  delete simulation;

  free(argv[0]);
  free(argv);
}


/**********************************************************************/
/**  INSUFFICIENT CORES JOB SUBMISSION TASK SIMULATION TEST ON ONE-ONE HOST                **/
/**********************************************************************/

class InsufficientCoresJobSubmissionTestWMS : public wrench::WMS {

public:
    InsufficientCoresJobSubmissionTestWMS(BatchServiceTest *test,
                                          const std::set<wrench::ComputeService *> &compute_services,
                                          const std::set<wrench::StorageService *> &storage_services,
                                          std::string hostname) :
            wrench::WMS(nullptr, nullptr,  compute_services, storage_services, {}, nullptr, hostname, "test") {
      this->test = test;
    }

private:

    BatchServiceTest *test;

    int main() {
      // Create a job manager
      std::shared_ptr<wrench::JobManager> job_manager = this->createJobManager();

      {
        // Create a sequential task that lasts one min and requires 2 cores
        wrench::WorkflowTask *task = this->workflow->addTask("task", 50, 2, 12, 1.0);
        task->addInputFile(workflow->getFileById("input_file"));
        task->addOutputFile(workflow->getFileById("output_file"));

        // Create a StandardJob with some pre-copies and post-deletions (not useful, but this is testing after all)

        wrench::StandardJob *job = job_manager->createStandardJob(
                {task},
                {
                        {*(task->getInputFiles().begin()),  this->test->storage_service1},
                        {*(task->getOutputFiles().begin()), this->test->storage_service1}
                },
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *, wrench::StorageService *>(
                        workflow->getFileById("input_file"), this->test->storage_service1,
                        this->test->storage_service2)},
                {},
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *>(workflow->getFileById("input_file"),
                                                                              this->test->storage_service2)});

        std::map<std::string, std::string> batch_job_args;
        batch_job_args["-N"] = "1";
        batch_job_args["-t"] = "2"; //time in minutes
        batch_job_args["-c"] = "12"; //number of cores per node, which is too many!

        bool success = true;
        try {
          job_manager->submitJob(job, this->test->compute_service, batch_job_args);
        } catch (wrench::WorkflowExecutionException &e) {
          success = false;
          if (e.getCause()->getCauseType() != wrench::FailureCause::NOT_ENOUGH_COMPUTE_RESOURCES) {
            throw std::runtime_error("Got an exception, as expected, but the failure cause seems wrong");
          }
        }

        if (success) {
          throw std::runtime_error("Job Submission should have generated an exception");
        }


        workflow->removeTask(task);
      }

      return 0;
    }
};

TEST_F(BatchServiceTest, InsufficientCoresJobSubmissionTest) {
  DO_TEST_WITH_FORK(do_InsufficientCoresTaskTest_test);
}

void BatchServiceTest::do_InsufficientCoresTaskTest_test() {

  // Create and initialize a simulation
  wrench::Simulation *simulation = new wrench::Simulation();
  int argc = 1;
  char **argv = (char **) calloc(1, sizeof(char *));
  argv[0] = strdup("batch_service_test");

  EXPECT_NO_THROW(simulation->init(&argc, argv));

  // Setting up the platform
  EXPECT_NO_THROW(simulation->instantiatePlatform(platform_file_path));

  // Get a hostname
  std::string hostname = simulation->getHostnameList()[0];

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service1 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service2 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Batch Service
  EXPECT_NO_THROW(compute_service = simulation->add(
                  new wrench::BatchService(hostname, true, true,
                                           simulation->getHostnameList(),
                                           storage_service1,  {})));

  // Create a WMS
  wrench::WMS *wms = nullptr;
  EXPECT_NO_THROW(wms = simulation->add(
          new InsufficientCoresJobSubmissionTestWMS(
                  this,  {compute_service}, {storage_service1, storage_service2}, hostname)));

  EXPECT_NO_THROW(wms->addWorkflow(workflow.get()));

  simulation->setFileRegistryService(new wrench::FileRegistryService(hostname));

  // Create two workflow files
  wrench::WorkflowFile *input_file = this->workflow->addFile("input_file", 10000.0);
  wrench::WorkflowFile *output_file = this->workflow->addFile("output_file", 20000.0);

  // Staging the input_file on the storage service
  EXPECT_NO_THROW(simulation->stageFile(input_file, storage_service1));


  // Running a "run a single task" simulation
  // Note that in these tests the WMS creates workflow tasks, which a user would
  // of course not be likely to do
  EXPECT_NO_THROW(simulation->launch());

  delete simulation;

  free(argv[0]);
  free(argv);
}



/**********************************************************************/
/**  NO ARGUMENTS JOB SUBMISSION TASK SIMULATION TEST ON ONE-ONE HOST                **/
/**********************************************************************/

class NoArgumentsJobSubmissionTestWMS : public wrench::WMS {

public:
    NoArgumentsJobSubmissionTestWMS(BatchServiceTest *test,
                                    const std::set<wrench::ComputeService *> &compute_services,
                                    const std::set<wrench::StorageService *> &storage_services,
                                    std::string hostname) :
            wrench::WMS(nullptr, nullptr,  compute_services, storage_services, {}, nullptr, hostname, "test") {
      this->test = test;
    }

private:

    BatchServiceTest *test;

    int main() {
      // Create a job manager
      std::shared_ptr<wrench::JobManager> job_manager = this->createJobManager();

      {
        // Create a sequential task that lasts one min and requires 2 cores
        wrench::WorkflowTask *task = this->workflow->addTask("task", 50, 2, 2, 1.0);
        task->addInputFile(workflow->getFileById("input_file"));
        task->addOutputFile(workflow->getFileById("output_file"));

        // Create a StandardJob with some pre-copies and post-deletions (not useful, but this is testing after all)

        wrench::StandardJob *job = job_manager->createStandardJob(
                {task},
                {
                        {*(task->getInputFiles().begin()),  this->test->storage_service1},
                        {*(task->getOutputFiles().begin()), this->test->storage_service1}
                },
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *, wrench::StorageService *>(
                        workflow->getFileById("input_file"), this->test->storage_service1,
                        this->test->storage_service2)},
                {},
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *>(workflow->getFileById("input_file"),
                                                                              this->test->storage_service2)});

        std::map<std::string, std::string> batch_job_args;
        bool success = false;
        try {
          job_manager->submitJob(job, this->test->compute_service, batch_job_args);
        } catch (std::invalid_argument e) {
          success = true;
        }
        if (not success) {
          throw std::runtime_error(
                  "Expecting a runtime error of not arguments but did not get any such exceptions"
          );
        }
        workflow->removeTask(task);
      }

      return 0;
    }
};

TEST_F(BatchServiceTest, NoArgumentsJobSubmissionTest) {
  DO_TEST_WITH_FORK(do_noArgumentsJobSubmissionTest_test);
}

void BatchServiceTest::do_noArgumentsJobSubmissionTest_test() {

  // Create and initialize a simulation
  wrench::Simulation *simulation = new wrench::Simulation();
  int argc = 1;
  char **argv = (char **) calloc(1, sizeof(char *));
  argv[0] = strdup("batch_service_test");

  EXPECT_NO_THROW(simulation->init(&argc, argv));

  // Setting up the platform
  EXPECT_NO_THROW(simulation->instantiatePlatform(platform_file_path));

  // Get a hostname
  std::string hostname = simulation->getHostnameList()[0];

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service1 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service2 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Batch Service
  EXPECT_NO_THROW(compute_service = simulation->add(
                  new wrench::BatchService(hostname, true, true,
                                           simulation->getHostnameList(),
                                           storage_service1, {})));

  // Create a WMS
  wrench::WMS *wms = nullptr;
  EXPECT_NO_THROW(wms = simulation->add(
          new NoArgumentsJobSubmissionTestWMS(
                  this,  {compute_service}, {storage_service1, storage_service2}, hostname)));

  EXPECT_NO_THROW(wms->addWorkflow(workflow.get()));

  simulation->setFileRegistryService(new wrench::FileRegistryService(hostname));

  // Create two workflow files
  wrench::WorkflowFile *input_file = this->workflow->addFile("input_file", 10000.0);
  wrench::WorkflowFile *output_file = this->workflow->addFile("output_file", 20000.0);

  // Staging the input_file on the storage service
  EXPECT_NO_THROW(simulation->stageFile(input_file, storage_service1));


  // Running a "run a single task" simulation
  // Note that in these tests the WMS creates workflow tasks, which a user would
  // of course not be likely to do
  EXPECT_NO_THROW(simulation->launch());

  delete simulation;

  free(argv[0]);
  free(argv);
}


/**********************************************************************/
/**  STANDARDJOB TIMEOUT TASK SIMULATION TEST ON ONE-ONE HOST                **/
/**********************************************************************/

class StandardJobTimeoutSubmissionTestWMS : public wrench::WMS {

public:
    StandardJobTimeoutSubmissionTestWMS(BatchServiceTest *test,
                                        const std::set<wrench::ComputeService *> &compute_services,
                                        const std::set<wrench::StorageService *> &storage_services,
                                        std::string hostname) :
            wrench::WMS(nullptr, nullptr,  compute_services, storage_services, {}, nullptr, hostname, "test") {
      this->test = test;
    }


private:

    BatchServiceTest *test;

    int main() {
      // Create a job manager
      std::shared_ptr<wrench::JobManager> job_manager = this->createJobManager();

      {
        // Create a sequential task that lasts one min and requires 2 cores
        wrench::WorkflowTask *task = this->workflow->addTask("task", 65, 1, 1, 1.0);
        task->addInputFile(workflow->getFileById("input_file"));
        task->addOutputFile(workflow->getFileById("output_file"));


        // Create a StandardJob with some pre-copies and post-deletions (not useful, but this is testing after all)

        wrench::StandardJob *job = job_manager->createStandardJob(
                {task},
                {
                        {*(task->getInputFiles().begin()),  this->test->storage_service1},
                        {*(task->getOutputFiles().begin()), this->test->storage_service1}
                },
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *, wrench::StorageService *>(
                        workflow->getFileById("input_file"), this->test->storage_service1,
                        this->test->storage_service2)},
                {},
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *>(workflow->getFileById("input_file"),
                                                                              this->test->storage_service2)});

        std::map<std::string, std::string> batch_job_args;
        batch_job_args["-N"] = "1";
        batch_job_args["-t"] = "1"; //time in minutes
        batch_job_args["-c"] = "4"; //number of cores per node
        try {
          job_manager->submitJob(job, this->test->compute_service, batch_job_args);
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error(
                  "Got some exception"
          );
        }

        // Wait for a workflow execution event
        std::unique_ptr<wrench::WorkflowExecutionEvent> event;
        try {
          event = workflow->waitForNextExecutionEvent();
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error("Error while getting and execution event: " + e.getCause()->toString());
        }
        switch (event->type) {
          case wrench::WorkflowExecutionEvent::STANDARD_JOB_FAILURE: {
            // success, do nothing for now
            break;
          }
          default: {
            throw std::runtime_error("Unexpected workflow execution event: " + std::to_string((int) (event->type)));
          }
        }
        workflow->removeTask(task);
      }

      return 0;
    }
};

TEST_F(BatchServiceTest, StandardJobTimeOutTaskTest) {
  DO_TEST_WITH_FORK(do_StandardJobTimeOutTaskTest_test);
}

void BatchServiceTest::do_StandardJobTimeOutTaskTest_test() {

  // Create and initialize a simulation
  wrench::Simulation *simulation = new wrench::Simulation();
  int argc = 1;
  char **argv = (char **) calloc(1, sizeof(char *));
  argv[0] = strdup("batch_service_test");

  EXPECT_NO_THROW(simulation->init(&argc, argv));

  // Setting up the platform
  EXPECT_NO_THROW(simulation->instantiatePlatform(platform_file_path));

  // Get a hostname
  std::string hostname = simulation->getHostnameList()[0];

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service1 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service2 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Batch Service
  EXPECT_NO_THROW(compute_service = simulation->add(
                  new wrench::BatchService(hostname, true, true,
                                           simulation->getHostnameList(),
                                           storage_service1, {})));

  // Create a WMS
  wrench::WMS *wms = nullptr;
  EXPECT_NO_THROW(wms = simulation->add(
          new StandardJobTimeoutSubmissionTestWMS(
                  this, {compute_service}, {storage_service1, storage_service2}, hostname)));

  EXPECT_NO_THROW(wms->addWorkflow( workflow.get()));

  simulation->setFileRegistryService(new wrench::FileRegistryService(hostname));

  // Create two workflow files
  wrench::WorkflowFile *input_file = this->workflow->addFile("input_file", 10000.0);
  wrench::WorkflowFile *output_file = this->workflow->addFile("output_file", 20000.0);

  // Staging the input_file on the storage service
  EXPECT_NO_THROW(simulation->stageFile(input_file, storage_service1));


  // Running a "run a single task" simulation
  // Note that in these tests the WMS creates workflow tasks, which a user would
  // of course not be likely to do
  EXPECT_NO_THROW(simulation->launch());

  delete simulation;

  free(argv[0]);
  free(argv);
}



/**********************************************************************/
/**  PILOTJOB TIMEOUT TASK SIMULATION TEST ON ONE-ONE HOST                **/
/**********************************************************************/

class PilotJobTimeoutSubmissionTestWMS : public wrench::WMS {

public:
    PilotJobTimeoutSubmissionTestWMS(BatchServiceTest *test,
                                     const std::set<wrench::ComputeService *> &compute_services,
                                     const std::set<wrench::StorageService *> &storage_services,
                                     std::string hostname) :
            wrench::WMS(nullptr, nullptr,  compute_services, storage_services, {}, nullptr, hostname, "test") {
      this->test = test;
    }

private:

    BatchServiceTest *test;

    int main() {
      // Create a job manager
      std::shared_ptr<wrench::JobManager> job_manager = this->createJobManager();

      {
        // Create a pilot job that needs 1 host, 1 core, 0 bytes of RAM, and 90 seconds
        wrench::PilotJob *pilot_job = job_manager->createPilotJob(this->workflow, 1, 1, 0.0, 90);

        std::map<std::string, std::string> batch_job_args;
        batch_job_args["-N"] = "1";
        batch_job_args["-t"] = "1"; //time in minutes
        batch_job_args["-c"] = "4"; //number of cores per node

        // Submit a pilot job
        try {
          job_manager->submitJob((wrench::WorkflowJob *) pilot_job, this->test->compute_service, batch_job_args);
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error(
                  "Got some exception " + std::string(e.what())
          );
        }

        // Wait for a workflow execution event
        std::unique_ptr<wrench::WorkflowExecutionEvent> event;
        try {
          event = workflow->waitForNextExecutionEvent();
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error("Error while getting and execution event: " + e.getCause()->toString());
        }
        switch (event->type) {
          case wrench::WorkflowExecutionEvent::PILOT_JOB_START: {
            // success, do nothing for now
            break;
          }
          default: {
            throw std::runtime_error("Unexpected workflow execution event: " + std::to_string((int) (event->type)));
          }
        }

        try {
          event = workflow->waitForNextExecutionEvent();
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error("Error while getting and execution event: " + e.getCause()->toString());
        }
        switch (event->type) {
          case wrench::WorkflowExecutionEvent::PILOT_JOB_EXPIRATION: {
            // success, do nothing for now
            break;
          }
          default: {
            throw std::runtime_error("Unexpected workflow execution event: " + std::to_string((int) (event->type)));
          }
        }
      }

      return 0;
    }
};

TEST_F(BatchServiceTest, PilotJobTimeOutTaskTest) {
  DO_TEST_WITH_FORK(do_PilotJobTimeOutTaskTest_test);
}

void BatchServiceTest::do_PilotJobTimeOutTaskTest_test() {

  // Create and initialize a simulation
  wrench::Simulation *simulation = new wrench::Simulation();
  int argc = 1;
  char **argv = (char **) calloc(1, sizeof(char *));
  argv[0] = strdup("batch_service_test");

  EXPECT_NO_THROW(simulation->init(&argc, argv));

  // Setting up the platform
  EXPECT_NO_THROW(simulation->instantiatePlatform(platform_file_path));

  // Get a hostname
  std::string hostname = simulation->getHostnameList()[0];

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service1 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service2 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Batch Service
  EXPECT_NO_THROW(compute_service = simulation->add(
                  new wrench::BatchService(hostname, true, true,
                                           simulation->getHostnameList(),
                                           storage_service1, {})));

  // Create a WMS
  wrench::WMS *wms = nullptr;
  EXPECT_NO_THROW(wms = simulation->add(
          new PilotJobTimeoutSubmissionTestWMS(
                  this, {compute_service}, {storage_service1, storage_service2}, hostname)));

  EXPECT_NO_THROW(wms->addWorkflow(workflow.get()));

  simulation->setFileRegistryService(new wrench::FileRegistryService(hostname));

  // Create two workflow files
  wrench::WorkflowFile *input_file = this->workflow->addFile("input_file", 10000.0);
  wrench::WorkflowFile *output_file = this->workflow->addFile("output_file", 20000.0);

  // Staging the input_file on the storage service
  EXPECT_NO_THROW(simulation->stageFile(input_file, storage_service1));


  // Running a "run a single task" simulation
  // Note that in these tests the WMS creates workflow tasks, which a user would
  // of course not be likely to do
  EXPECT_NO_THROW(simulation->launch());

  delete simulation;

  free(argv[0]);
  free(argv);
}


/**********************************************************************/
/**  BEST FIT STANDARD JOB SUBMISSION TASK SIMULATION TEST ON ONE-ONE HOST                **/
/**********************************************************************/

class BestFitStandardJobSubmissionTestWMS : public wrench::WMS {

public:
    BestFitStandardJobSubmissionTestWMS(BatchServiceTest *test,
                                        const std::set<wrench::ComputeService *> &compute_services,
                                        const std::set<wrench::StorageService *> &storage_services,
                                        std::string hostname) :
            wrench::WMS(nullptr, nullptr, compute_services, storage_services, {}, nullptr, hostname, "test") {
      this->test = test;
    }

private:

    BatchServiceTest *test;

    int main() {
      // Create a job manager
      std::shared_ptr<wrench::JobManager> job_manager = this->createJobManager();

      {
        // Create a sequential task that lasts one min and requires 8 cores
        wrench::WorkflowTask *task = this->workflow->addTask("task", 50, 8, 8, 1.0);
        task->addInputFile(workflow->getFileById("input_file"));
        task->addOutputFile(workflow->getFileById("output_file"));

        //Create another sequential task that lasts one min and requires 9 cores
        wrench::WorkflowTask *task1 = this->workflow->addTask("task1", 50, 9, 9, 1.0);
        task1->addInputFile(workflow->getFileById("input_file_1"));
        task1->addOutputFile(workflow->getFileById("output_file_1"));

        //Create another sequential task that lasts one min and requires 1 cores
        wrench::WorkflowTask *task2 = this->workflow->addTask("task2", 50, 1, 1, 1.0);
        task2->addInputFile(workflow->getFileById("input_file_2"));
        task2->addOutputFile(workflow->getFileById("output_file_2"));

        // Create a StandardJob with some pre-copies and post-deletions (not useful, but this is testing after all)
        wrench::StandardJob *job = job_manager->createStandardJob(
                {task},
                {
                        {*(task->getInputFiles().begin()),  this->test->storage_service1},
                        {*(task->getOutputFiles().begin()), this->test->storage_service1}
                },
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *, wrench::StorageService *>(
                        workflow->getFileById("input_file"), this->test->storage_service1,
                        this->test->storage_service2)},
                {},
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *>(workflow->getFileById("input_file"),
                                                                              this->test->storage_service2)});

        std::map<std::string, std::string> batch_job_args;
        batch_job_args["-N"] = "1";
        batch_job_args["-t"] = "2"; //time in minutes
        batch_job_args["-c"] = "8"; //number of cores per node
        try {
          job_manager->submitJob(job, this->test->compute_service, batch_job_args);
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error(
                  "Got some exception"
          );
        }

        wrench::StandardJob *job1 = job_manager->createStandardJob(
                {task1},
                {
                        {*(task1->getInputFiles().begin()),  this->test->storage_service1},
                        {*(task1->getOutputFiles().begin()), this->test->storage_service1}
                },
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *, wrench::StorageService *>(
                        workflow->getFileById("input_file_1"), this->test->storage_service1,
                        this->test->storage_service2)},
                {},
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *>(workflow->getFileById("input_file_1"),
                                                                              this->test->storage_service2)});

        std::map<std::string, std::string> task1_batch_job_args;
        task1_batch_job_args["-N"] = "1";
        task1_batch_job_args["-t"] = "2"; //time in minutes
        task1_batch_job_args["-c"] = "9"; //number of cores per node
        try {
          job_manager->submitJob(job1, this->test->compute_service, task1_batch_job_args);
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error(
                  "Got some exception"
          );
        }

        wrench::StandardJob *job2 = job_manager->createStandardJob(
                {task2},
                {
                        {*(task2->getInputFiles().begin()),  this->test->storage_service1},
                        {*(task2->getOutputFiles().begin()), this->test->storage_service1}
                },
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *, wrench::StorageService *>(
                        workflow->getFileById("input_file_2"), this->test->storage_service1,
                        this->test->storage_service2)},
                {},
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *>(workflow->getFileById("input_file_2"),
                                                                              this->test->storage_service2)});

        std::map<std::string, std::string> task2_batch_job_args;
        task2_batch_job_args["-N"] = "1";
        task2_batch_job_args["-t"] = "2"; //time in minutes
        task2_batch_job_args["-c"] = "1"; //number of cores per node
        try {
          job_manager->submitJob(job2, this->test->compute_service, task2_batch_job_args);
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error(
                  "Got some exception"
          );
        }

        //wait for three standard job completion events
        int num_events = 0;
        while (num_events < 3) {
          std::unique_ptr<wrench::WorkflowExecutionEvent> event;
          try {
            event = workflow->waitForNextExecutionEvent();
          } catch (wrench::WorkflowExecutionException &e) {
            throw std::runtime_error("Error while getting and execution event: " + e.getCause()->toString());
          }
          switch (event->type) {
            case wrench::WorkflowExecutionEvent::STANDARD_JOB_COMPLETION: {
              // success, do nothing for now
              break;
            }
            default: {
              throw std::runtime_error(
                      "Unexpected workflow execution event: " + std::to_string((int) (event->type)));
            }
          }
          num_events++;
        }
        workflow->removeTask(task);
        workflow->removeTask(task1);
        workflow->removeTask(task2);
      }

      return 0;
    }
};

TEST_F(BatchServiceTest, DISABLED_BestFitStandardJobSubmissionTest) {
  DO_TEST_WITH_FORK(do_BestFitTaskTest_test);
}

void BatchServiceTest::do_BestFitTaskTest_test() {

  // Create and initialize a simulation
  wrench::Simulation *simulation = new wrench::Simulation();
  int argc = 1;
  char **argv = (char **) calloc(1, sizeof(char *));
  argv[0] = strdup("batch_service_test");

  EXPECT_NO_THROW(simulation->init(&argc, argv));

  // Setting up the platform
  EXPECT_NO_THROW(simulation->instantiatePlatform(platform_file_path));

  // Get a hostname
  std::string hostname = simulation->getHostnameList()[0];

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service1 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service2 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Batch Service
  EXPECT_NO_THROW(compute_service = simulation->add(
                  new wrench::BatchService(hostname, true, true,
                                           simulation->getHostnameList(),
                                           storage_service1,
                                           {{wrench::StandardJobExecutorProperty::HOST_SELECTION_ALGORITHM, "BESTFIT"}})));

  // Create a WMS
  wrench::WMS *wms = nullptr;
  EXPECT_NO_THROW(wms = simulation->add(
          new BestFitStandardJobSubmissionTestWMS(
                  this,  {compute_service}, {storage_service1, storage_service2}, hostname)));

  EXPECT_NO_THROW(wms->addWorkflow( workflow.get()));

  simulation->setFileRegistryService(new wrench::FileRegistryService(hostname));

  // Create two workflow files
  wrench::WorkflowFile *input_file = this->workflow->addFile("input_file", 10000.0);
  wrench::WorkflowFile *output_file = this->workflow->addFile("output_file", 20000.0);
  wrench::WorkflowFile *input_file_1 = this->workflow->addFile("input_file_1", 10000.0);
  wrench::WorkflowFile *output_file_1 = this->workflow->addFile("output_file_1", 20000.0);
  wrench::WorkflowFile *input_file_2 = this->workflow->addFile("input_file_2", 10000.0);
  wrench::WorkflowFile *output_file_2 = this->workflow->addFile("output_file_2", 20000.0);

  // Staging the input_file on the storage service
  EXPECT_NO_THROW(simulation->stageFiles({{input_file->getId(), input_file},
                                          {input_file_1->getId(), input_file_1},
                                          {input_file_2->getId(), input_file_2}}, storage_service1));


  // Running a "run a single task" simulation
  // Note that in these tests the WMS creates workflow tasks, which a user would
  // of course not be likely to do
  EXPECT_NO_THROW(simulation->launch());

  delete simulation;

  free(argv[0]);
  free(argv);
}



/**********************************************************************/
/**  STANDARDJOB INSIDE PILOT JOB FAILURE TASK SIMULATION TEST ON ONE-ONE HOST                **/
/**********************************************************************/

class StandardJobInsidePilotJobTimeoutSubmissionTestWMS : public wrench::WMS {

public:
    StandardJobInsidePilotJobTimeoutSubmissionTestWMS(BatchServiceTest *test,
                                                      const std::set<wrench::ComputeService *> &compute_services,
                                                      const std::set<wrench::StorageService *> &storage_services,
                                                      std::string hostname) :
            wrench::WMS(nullptr, nullptr,  compute_services, storage_services, {}, nullptr, hostname, "test") {
      this->test = test;
    }

private:

    BatchServiceTest *test;

    int main() {
      // Create a job manager
      std::shared_ptr<wrench::JobManager> job_manager = this->createJobManager();

      {
        // Create a pilot job that needs 1 host, 1 core, 0 bytes of RAM, and 90 seconds
        wrench::PilotJob *pilot_job = job_manager->createPilotJob(this->workflow, 1, 1, 0.0, 90);

        // Create a sequential task that lasts one min and requires 2 cores
        wrench::WorkflowTask *task = this->workflow->addTask("task", 60, 2, 2, 1.0);
        task->addInputFile(workflow->getFileById("input_file"));
        task->addOutputFile(workflow->getFileById("output_file"));

        std::map<std::string, std::string> pilot_batch_job_args;
        pilot_batch_job_args["-N"] = "1";
        pilot_batch_job_args["-t"] = "2"; //time in minutes
        pilot_batch_job_args["-c"] = "4"; //number of cores per node

        // Submit a pilot job
        try {
          job_manager->submitJob((wrench::WorkflowJob *) pilot_job, this->test->compute_service, pilot_batch_job_args);
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error(
                  "Got some exception " + std::string(e.what())
          );
        }

        // Wait for a workflow execution event
        std::unique_ptr<wrench::WorkflowExecutionEvent> event;
        try {
          event = workflow->waitForNextExecutionEvent();
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error("Error while getting and execution event: " + e.getCause()->toString());
        }
        switch (event->type) {
          case wrench::WorkflowExecutionEvent::PILOT_JOB_START: {
            // success, do nothing for now
            break;
          }
          default: {
            throw std::runtime_error("Unexpected workflow execution event: " + std::to_string((int) (event->type)));
          }
        }


        // Create a StandardJob with some pre-copies and post-deletions
        wrench::StandardJob *job = job_manager->createStandardJob(
                {task}, {}, {}, {}, {});

        std::map<std::string, std::string> standard_batch_job_args;
        standard_batch_job_args["-N"] = "1";
        standard_batch_job_args["-t"] = "1"; //time in minutes
        standard_batch_job_args["-c"] = "2"; //number of cores per node
        try {
          job_manager->submitJob(job, pilot_job->getComputeService(), standard_batch_job_args);
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error(
                  "Got some exception"
          );
        }

        // Terminate the pilot job while it's running a standard job
        try {
          job_manager->terminateJob(pilot_job);
        } catch (std::exception &e) {
          throw std::runtime_error("Unexpected exception while terminating pilot job: " + std::string(e.what()));
        }

        // Wait for the standard job failure notification
        try {
          event = workflow->waitForNextExecutionEvent();
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error(
                  "Error while getting and execution event: " + std::to_string(e.getCause()->getCauseType()));
        }
        switch (event->type) {
          case wrench::WorkflowExecutionEvent::STANDARD_JOB_FAILURE: {
            if (event->failure_cause->getCauseType() != wrench::FailureCause::SERVICE_DOWN) {
              throw std::runtime_error("Got a job failure event, but the failure cause seems wrong");
            }
            wrench::ServiceIsDown *real_cause = (wrench::ServiceIsDown *) (event->failure_cause.get());
            std::string error_msg = real_cause->toString();
            if (real_cause->getService() != this->test->compute_service) {
              std::runtime_error(
                      "Got the correct failure even, a correct cause type, but the cause points to the wrong service");
            }
            break;
          }
          default: {
            throw std::runtime_error("Unexpected workflow execution event: " + std::to_string(event->type));
          }
        }

        workflow->removeTask(task);
      }

      return 0;
    }
};

TEST_F(BatchServiceTest, StandardJobInsidePilotJobTimeOutTaskTest) {
  DO_TEST_WITH_FORK(do_StandardJobInsidePilotJobTimeOutTaskTest_test);
}

void BatchServiceTest::do_StandardJobInsidePilotJobTimeOutTaskTest_test() {

  // Create and initialize a simulation
  wrench::Simulation *simulation = new wrench::Simulation();
  int argc = 1;
  char **argv = (char **) calloc(1, sizeof(char *));
  argv[0] = strdup("batch_service_test");

  EXPECT_NO_THROW(simulation->init(&argc, argv));

  // Setting up the platform
  EXPECT_NO_THROW(simulation->instantiatePlatform(platform_file_path));

  // Get a hostname
  std::string hostname = simulation->getHostnameList()[0];

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service1 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service2 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Batch Service
  EXPECT_NO_THROW(compute_service = simulation->add(
                  new wrench::BatchService(hostname, true, true,
                                           simulation->getHostnameList(),
                                           storage_service1, {})));

  // Create a WMS
  wrench::WMS *wms = nullptr;
  EXPECT_NO_THROW(wms = simulation->add(
          new StandardJobInsidePilotJobTimeoutSubmissionTestWMS(
                  this,  {compute_service}, {storage_service1, storage_service2}, hostname)));

  EXPECT_NO_THROW(wms->addWorkflow( workflow.get()));

  simulation->setFileRegistryService(new wrench::FileRegistryService(hostname));

  // Create two workflow files
  wrench::WorkflowFile *input_file = this->workflow->addFile("input_file", 10000.0);
  wrench::WorkflowFile *output_file = this->workflow->addFile("output_file", 20000.0);

  // Staging the input_file on the storage service
  EXPECT_NO_THROW(simulation->stageFile(input_file, storage_service1));


  // Running a "run a single task" simulation
  // Note that in these tests the WMS creates workflow tasks, which a user would
  // of course not be likely to do
  EXPECT_NO_THROW(simulation->launch());

  delete simulation;

  free(argv[0]);
  free(argv);
}


/**********************************************************************/
/**  STANDARDJOB INSIDE PILOT JOB SUCESS TASK SIMULATION TEST ON ONE-ONE HOST                **/
/**********************************************************************/

class StandardJobInsidePilotJobSucessSubmissionTestWMS : public wrench::WMS {

public:
    StandardJobInsidePilotJobSucessSubmissionTestWMS(BatchServiceTest *test,
                                                     const std::set<wrench::ComputeService *> &compute_services,
                                                     const std::set<wrench::StorageService *> &storage_services,
                                                     std::string hostname) :
            wrench::WMS(nullptr, nullptr,  compute_services, storage_services, {}, nullptr, hostname, "test") {
      this->test = test;
    }

private:

    BatchServiceTest *test;

    int main() {
      // Create a job manager
      std::shared_ptr<wrench::JobManager> job_manager = this->createJobManager();

      {
        // Create a pilot job tbat needs 1 host, 1 core, 0 bytes of RAM, and 90 seconds
        wrench::PilotJob *pilot_job = job_manager->createPilotJob(this->workflow, 1, 1, 0.0, 90);

        // Create a sequential task that lasts one min and requires 2 cores
        wrench::WorkflowTask *task = this->workflow->addTask("task", 60, 2, 2, 1.0);
        task->addInputFile(workflow->getFileById("input_file"));
        task->addOutputFile(workflow->getFileById("output_file"));

        std::map<std::string, std::string> pilot_batch_job_args;
        pilot_batch_job_args["-N"] = "1";
        pilot_batch_job_args["-t"] = "2"; //time in minutes
        pilot_batch_job_args["-c"] = "4"; //number of cores per node

        // Submit a pilot job
        try {
          job_manager->submitJob((wrench::WorkflowJob *) pilot_job, this->test->compute_service, pilot_batch_job_args);
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error(
                  "Got some exception " + std::string(e.what())
          );
        }

        // Wait for a workflow execution event
        std::unique_ptr<wrench::WorkflowExecutionEvent> event;
        try {
          event = workflow->waitForNextExecutionEvent();
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error("Error while getting and execution event: " + e.getCause()->toString());
        }
        switch (event->type) {
          case wrench::WorkflowExecutionEvent::PILOT_JOB_START: {
            // success, do nothing for now
            break;
          }
          default: {
            throw std::runtime_error("Unexpected workflow execution event: " + std::to_string((int) (event->type)));
          }
        }


        // Create a StandardJob with some pre-copies and post-deletions
        wrench::StandardJob *job = job_manager->createStandardJob(
                {task},
                {
                        {*(task->getInputFiles().begin()),  this->test->storage_service1},
                        {*(task->getOutputFiles().begin()), this->test->storage_service1}
                },
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *, wrench::StorageService *>(
                        workflow->getFileById("input_file"), this->test->storage_service1,
                        this->test->storage_service2)},
                {},
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *>(workflow->getFileById("input_file"),
                                                                              this->test->storage_service2)});

        std::map<std::string, std::string> standard_batch_job_args;
        standard_batch_job_args["-N"] = "1";
        standard_batch_job_args["-t"] = "1"; //time in minutes
        standard_batch_job_args["-c"] = "2"; //number of cores per node
        try {
          job_manager->submitJob(job, pilot_job->getComputeService(), standard_batch_job_args);
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error(
                  "Got some exception"
          );
        }


        // Wait for the standard job success notification
        try {
          event = workflow->waitForNextExecutionEvent();
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error(
                  "Error while getting and execution event: " + std::to_string(e.getCause()->getCauseType()));
        }
        bool success = false;
        switch (event->type) {
          case wrench::WorkflowExecutionEvent::STANDARD_JOB_COMPLETION: {
            success = true;
            break;
          }
          default: {
            success = false;
          }
        }

        if (not success) {
          throw std::runtime_error("Unexpected workflow execution event: " + std::to_string(event->type));
        }

        workflow->removeTask(task);
      }

      return 0;
    }
};

TEST_F(BatchServiceTest, StandardJobInsidePilotJobSucessTaskTest) {
  DO_TEST_WITH_FORK(do_StandardJobInsidePilotJobSucessTaskTest_test);
}

void BatchServiceTest::do_StandardJobInsidePilotJobSucessTaskTest_test() {

  // Create and initialize a simulation
  wrench::Simulation *simulation = new wrench::Simulation();
  int argc = 1;
  char **argv = (char **) calloc(1, sizeof(char *));
  argv[0] = strdup("batch_service_test");

  EXPECT_NO_THROW(simulation->init(&argc, argv));

  // Setting up the platform
  EXPECT_NO_THROW(simulation->instantiatePlatform(platform_file_path));

  // Get a hostname
  std::string hostname = simulation->getHostnameList()[0];

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service1 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service2 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Batch Service
  EXPECT_NO_THROW(compute_service = simulation->add(
                  new wrench::BatchService(hostname, true, true,
                                           simulation->getHostnameList(),
                                           storage_service1, {})));

  // Create a WMS
  wrench::WMS *wms = nullptr;
  EXPECT_NO_THROW(wms = simulation->add(
          new StandardJobInsidePilotJobSucessSubmissionTestWMS(
                  this, {compute_service}, {storage_service1, storage_service2}, hostname)));

  EXPECT_NO_THROW(wms->addWorkflow( workflow.get()));

  simulation->setFileRegistryService(new wrench::FileRegistryService(hostname));

  // Create two workflow files
  wrench::WorkflowFile *input_file = this->workflow->addFile("input_file", 10000.0);
  wrench::WorkflowFile *output_file = this->workflow->addFile("output_file", 20000.0);

  // Staging the input_file on the storage service
  EXPECT_NO_THROW(simulation->stageFile(input_file, storage_service1));


  // Running a "run a single task" simulation
  // Note that in these tests the WMS creates workflow tasks, which a user would
  // of course not be likely to do
  EXPECT_NO_THROW(simulation->launch());

  delete simulation;

  free(argv[0]);
  free(argv);
}



/**********************************************************************/
/**  INSUFFICIENT CORES INSIDE PILOT JOB SIMULATION TEST ON ONE-ONE HOST                **/
/**********************************************************************/

class InsufficientCoresInsidePilotJobSubmissionTestWMS : public wrench::WMS {

public:
    InsufficientCoresInsidePilotJobSubmissionTestWMS(BatchServiceTest *test,
                                                     const std::set<wrench::ComputeService *> &compute_services,
                                                     const std::set<wrench::StorageService *> &storage_services,
                                                     std::string hostname) :
            wrench::WMS(nullptr, nullptr,  compute_services, storage_services, {}, nullptr, hostname, "test") {
      this->test = test;
    }

private:

    BatchServiceTest *test;

    int main() {
      // Create a job manager
      std::shared_ptr<wrench::JobManager> job_manager = this->createJobManager();

      {
        // Create a pilot job that needs 1 host, 1 core, 0 bytes of RAM, and 90 seconds
        wrench::PilotJob *pilot_job = job_manager->createPilotJob(this->workflow, 1, 1, 0, 90);

        // Create a sequential task that lasts one min and requires 5 cores
        wrench::WorkflowTask *task = this->workflow->addTask("task", 60, 5, 5, 1.0);
        task->addInputFile(workflow->getFileById("input_file"));
        task->addOutputFile(workflow->getFileById("output_file"));

        std::map<std::string, std::string> pilot_batch_job_args;
        pilot_batch_job_args["-N"] = "1";
        pilot_batch_job_args["-t"] = "2"; //time in minutes
        pilot_batch_job_args["-c"] = "4"; //number of cores per node

        // Submit a pilot job
        try {
          job_manager->submitJob((wrench::WorkflowJob *) pilot_job, this->test->compute_service, pilot_batch_job_args);
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error(
                  "Got some exception " + std::string(e.what())
          );
        }

        // Wait for a workflow execution event
        std::unique_ptr<wrench::WorkflowExecutionEvent> event;
        try {
          event = workflow->waitForNextExecutionEvent();
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error("Error while getting and execution event: " + e.getCause()->toString());
        }
        switch (event->type) {
          case wrench::WorkflowExecutionEvent::PILOT_JOB_START: {
            // success, do nothing for now
            break;
          }
          default: {
            throw std::runtime_error("Unexpected workflow execution event: " + std::to_string((int) (event->type)));
          }
        }


        // Create a StandardJob with some pre-copies and post-deletions
        wrench::StandardJob *job = job_manager->createStandardJob(
                {task},
                {
                        {*(task->getInputFiles().begin()),  this->test->storage_service1},
                        {*(task->getOutputFiles().begin()), this->test->storage_service1}
                },
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *, wrench::StorageService *>(
                        workflow->getFileById("input_file"), this->test->storage_service1,
                        this->test->storage_service2)},
                {},
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *>(workflow->getFileById("input_file"),
                                                                              this->test->storage_service2)});

        std::map<std::string, std::string> standard_batch_job_args;
        standard_batch_job_args["-N"] = "1";
        standard_batch_job_args["-t"] = "1"; //time in minutes
        standard_batch_job_args["-c"] = "5"; //number of cores per node
        bool success = false;
        try {
          job_manager->submitJob(job, pilot_job->getComputeService(), standard_batch_job_args);
        } catch (wrench::WorkflowExecutionException e) {
          success = true;
        }

        if (not success) {
          throw std::runtime_error(
                  "Expected a runtime error of insufficient cores in pilot job"
          );
        }

        workflow->removeTask(task);
      }

      return 0;
    }
};

TEST_F(BatchServiceTest, InsufficientCoresInsidePilotJobTaskTest) {
  DO_TEST_WITH_FORK(do_InsufficientCoresInsidePilotJobTaskTest_test);
}

void BatchServiceTest::do_InsufficientCoresInsidePilotJobTaskTest_test() {

  // Create and initialize a simulation
  wrench::Simulation *simulation = new wrench::Simulation();
  int argc = 1;
  char **argv = (char **) calloc(1, sizeof(char *));
  argv[0] = strdup("batch_service_test");

  EXPECT_NO_THROW(simulation->init(&argc, argv));

  // Setting up the platform
  EXPECT_NO_THROW(simulation->instantiatePlatform(platform_file_path));

  // Get a hostname
  std::string hostname = simulation->getHostnameList()[0];

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service1 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service2 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Batch Service
  EXPECT_NO_THROW(compute_service = simulation->add(
                  new wrench::BatchService(hostname, true, true,
                                           simulation->getHostnameList(),
                                           storage_service1,  {})));

  // Create a WMS
  wrench::WMS *wms = nullptr;
  EXPECT_NO_THROW(wms = simulation->add(
          new InsufficientCoresInsidePilotJobSubmissionTestWMS(
                  this,  {compute_service}, {storage_service1, storage_service2}, hostname)));

  EXPECT_NO_THROW(wms->addWorkflow(workflow.get()));

  simulation->setFileRegistryService(new wrench::FileRegistryService(hostname));

  // Create two workflow files
  wrench::WorkflowFile *input_file = this->workflow->addFile("input_file", 10000.0);
  wrench::WorkflowFile *output_file = this->workflow->addFile("output_file", 20000.0);

  // Staging the input_file on the storage service
  EXPECT_NO_THROW(simulation->stageFile(input_file, storage_service1));


  // Running a "run a single task" simulation
  // Note that in these tests the WMS creates workflow tasks, which a user would
  // of course not be likely to do
  EXPECT_NO_THROW(simulation->launch());

  delete simulation;

  free(argv[0]);
  free(argv);
}



/**********************************************************************/
/**  MULTIPLE STANDARD JOB SUBMISSION TASK SIMULATION TEST ON ONE HOST                **/
/**********************************************************************/

class MultipleStandardJobSubmissionTestWMS : public wrench::WMS {

public:
    MultipleStandardJobSubmissionTestWMS(BatchServiceTest *test,
                                         const std::set<wrench::ComputeService *> &compute_services,
                                         const std::set<wrench::StorageService *> &storage_services,
                                         std::string hostname) :
            wrench::WMS(nullptr, nullptr,  compute_services, storage_services, {}, nullptr, hostname, "test") {
      this->test = test;
    }

private:

    BatchServiceTest *test;

    int main() {
      // Create a job manager
      std::shared_ptr<wrench::JobManager> job_manager = this->createJobManager();

      {

        int num_standard_jobs = 10;
        int each_task_time = 60; //in seconds
        std::vector<wrench::StandardJob *> jobs;
        std::vector<wrench::WorkflowTask *> tasks;
        for (int i = 0; i < num_standard_jobs; i++) {
          // Create a sequential task that lasts for random minutes and requires 2 cores
          wrench::WorkflowTask *task = this->workflow->addTask("task" + std::to_string(i), each_task_time, 2, 2, 1.0);
          wrench::StandardJob *job = job_manager->createStandardJob(
                  {task}, {}, {}, {}, {});
          tasks.push_back(std::move(task));
          jobs.push_back(std::move(job));
        }


        std::map<std::string, std::string> batch_job_args;
        batch_job_args["-N"] = "1";
        batch_job_args["-t"] = std::to_string((each_task_time / 60) * num_standard_jobs); //time in minutes
        batch_job_args["-c"] = "2"; //number of cores per node
        for (auto standard_jobs:jobs) {
          try {
            job_manager->submitJob(standard_jobs, this->test->compute_service, batch_job_args);
          } catch (wrench::WorkflowExecutionException &e) {
            throw std::runtime_error(
                    "Got some exception"
            );
          }
        }

        for (int i = 0; i < num_standard_jobs; i++) {

          // Wait for a workflow execution event
          std::unique_ptr<wrench::WorkflowExecutionEvent> event;
          try {
            event = workflow->waitForNextExecutionEvent();
          } catch (wrench::WorkflowExecutionException &e) {
            throw std::runtime_error("Error while getting and execution event: " + e.getCause()->toString());
          }
          switch (event->type) {
            case wrench::WorkflowExecutionEvent::STANDARD_JOB_COMPLETION: {
              // success, do nothing for now
              break;
            }
            default: {
              throw std::runtime_error(
                      "Unexpected workflow execution event: " + std::to_string((int) (event->type)));
            }
          }
        }

        for (auto each_task:tasks) {
          workflow->removeTask(each_task);
        }
      }

      return 0;
    }
};

TEST_F(BatchServiceTest, MultipleStandardJobSubmissionTest) {
  DO_TEST_WITH_FORK(do_MultipleStandardTaskTest_test);
}


void BatchServiceTest::do_MultipleStandardTaskTest_test() {

  // Create and initialize a simulation
  wrench::Simulation *simulation = new wrench::Simulation();
  int argc = 1;
  char **argv = (char **) calloc(1, sizeof(char *));
  argv[0] = strdup("batch_service_test");

  EXPECT_NO_THROW(simulation->init(&argc, argv));

  // Setting up the platform
  EXPECT_NO_THROW(simulation->instantiatePlatform(platform_file_path));

  // Get a hostname
  std::string hostname = simulation->getHostnameList()[0];

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service1 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service2 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Batch Service
  EXPECT_NO_THROW(compute_service = simulation->add(
                  new wrench::BatchService(hostname, true, true,
                                           simulation->getHostnameList(),
                                           storage_service1, {})));

  // Create a WMS
  wrench::WMS *wms = nullptr;
  EXPECT_NO_THROW(wms = simulation->add(
          new MultipleStandardJobSubmissionTestWMS(
                  this,  {compute_service}, {storage_service1, storage_service2}, hostname)));

  EXPECT_NO_THROW(wms->addWorkflow( workflow.get()));

  simulation->setFileRegistryService(new wrench::FileRegistryService(hostname));

  // Create two workflow files
  wrench::WorkflowFile *input_file = this->workflow->addFile("input_file", 10000.0);
  wrench::WorkflowFile *output_file = this->workflow->addFile("output_file", 20000.0);

  // Staging the input_file on the storage service
  EXPECT_NO_THROW(simulation->stageFile(input_file, storage_service1));


  // Running a "run a single task" simulation
  // Note that in these tests the WMS creates workflow tasks, which a user would
  // of course not be likely to do
  EXPECT_NO_THROW(simulation->launch());

  delete simulation;

  free(argv[0]);
  free(argv);
}


/**********************************************************************/
/**  DIFFERENT BATCHSERVICE ALGORITHMS SUBMISSION TASK SIMULATION TEST **/
/**********************************************************************/

class DifferentBatchAlgorithmsSubmissionTestWMS : public wrench::WMS {

public:
    DifferentBatchAlgorithmsSubmissionTestWMS(BatchServiceTest *test,
                                              const std::set<wrench::ComputeService *> &compute_services,
                                              const std::set<wrench::StorageService *> &storage_services,
                                              std::string hostname) :
            wrench::WMS(nullptr, nullptr,  compute_services, storage_services, {}, nullptr, hostname, "test") {
      this->test = test;
    }

private:

    BatchServiceTest *test;

    int main() {
      // Create a job manager
      std::shared_ptr<wrench::JobManager> job_manager = this->createJobManager();

      {
        // Create a sequential task that lasts one min and requires 2 cores
        wrench::WorkflowTask *task = this->workflow->addTask("task", 60, 2, 2, 1.0);
        task->addInputFile(this->workflow->getFileById("input_file"));
        task->addOutputFile(this->workflow->getFileById("output_file"));


        // Create a StandardJob with some pre-copies and post-deletions (not useful, but this is testing after all)

        wrench::StandardJob *job = job_manager->createStandardJob(
                {task},
                {
                        {*(task->getInputFiles().begin()),  this->test->storage_service1},
                        {*(task->getOutputFiles().begin()), this->test->storage_service1}
                },
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *, wrench::StorageService *>(
                        this->workflow->getFileById("input_file"), this->test->storage_service1,
                        this->test->storage_service2)},
                {},
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *>(this->workflow->getFileById("input_file"),
                                                                              this->test->storage_service2)});

        std::map<std::string, std::string> batch_job_args;
        batch_job_args["-N"] = "1";
        batch_job_args["-t"] = "5"; //time in minutes
        batch_job_args["-c"] = "4"; //number of cores per node
        try {
          job_manager->submitJob(job, this->test->compute_service, batch_job_args);
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error(
                  "Got some exception"
          );
        }


        // Wait for a workflow execution event
        std::unique_ptr<wrench::WorkflowExecutionEvent> event;
        try {
          event = this->workflow->waitForNextExecutionEvent();
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error("Error while getting and execution event: " + e.getCause()->toString());
        }
        switch (event->type) {
          case wrench::WorkflowExecutionEvent::STANDARD_JOB_COMPLETION: {
            // success, do nothing for now
            break;
          }
          default: {
            throw std::runtime_error("Unexpected workflow execution event: " + std::to_string((int) (event->type)));
          }
        }
        this->workflow->removeTask(task);
      }

      return 0;
    }
};


#ifdef ENABLE_BATSCHED

TEST_F(BatchServiceTest, DifferentBatchAlgorithmsSubmissionTest) {
  DO_TEST_WITH_FORK(do_DifferentBatchAlgorithmsSubmissionTest_test);
}

#else

TEST_F(BatchServiceTest, DISABLED_DifferentBatchAlgorithmsSubmissionTest) {
  DO_TEST_WITH_FORK(do_DifferentBatchAlgorithmsSubmissionTest_test);
}

#endif


void BatchServiceTest::do_DifferentBatchAlgorithmsSubmissionTest_test() {

  // Create and initialize a simulation
  wrench::Simulation *simulation = new wrench::Simulation();
  int argc = 1;
  char **argv = (char **) calloc(1, sizeof(char *));
  argv[0] = strdup("batch_service_test");

  EXPECT_NO_THROW(simulation->init(&argc, argv));

  // Setting up the platform
  EXPECT_NO_THROW(simulation->instantiatePlatform(platform_file_path));

  // Get a hostname
  std::string hostname = simulation->getHostnameList()[0];

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service1 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service2 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Batch Service
  EXPECT_NO_THROW(compute_service = simulation->add(
                  new wrench::BatchService(hostname,
                                           true, true,
                                           simulation->getHostnameList(), storage_service1,  {
                                                   {wrench::BatchServiceProperty::BATCH_SCHEDULING_ALGORITHM,     "filler"},
                                                   {wrench::BatchServiceProperty::BATCH_QUEUE_ORDERING_ALGORITHM, "fcfs"}
                                           })));

  // Create a WMS
  wrench::WMS *wms = nullptr;
  EXPECT_NO_THROW(wms = simulation->add(
          new DifferentBatchAlgorithmsSubmissionTestWMS(
                  this,  {compute_service}, {storage_service1, storage_service2}, hostname)));

  EXPECT_NO_THROW(wms->addWorkflow(std::move(workflow).get()));

  simulation->setFileRegistryService(new wrench::FileRegistryService(hostname));

  // Create two workflow files
  wrench::WorkflowFile *input_file = this->workflow->addFile("input_file", 10000.0);
  wrench::WorkflowFile *output_file = this->workflow->addFile("output_file", 20000.0);

  // Staging the input_file on the storage service
  EXPECT_NO_THROW(simulation->stageFile(input_file, storage_service1));

  // Running a "run a single task" simulation
  // Note that in these tests the WMS creates workflow tasks, which a user would
  // of course not be likely to do
  EXPECT_NO_THROW(simulation->launch());

  delete simulation;

  free(argv[0]);
  free(argv);
}


/**********************************************************************/
/**  BATCH TRACE FILE JOB SUBMISSION TASK SIMULATION TEST **/
/**********************************************************************/

class BatchTraceFileJobSubmissionTestWMS : public wrench::WMS {

public:
    BatchTraceFileJobSubmissionTestWMS(BatchServiceTest *test,
                                       const std::set<wrench::ComputeService *> &compute_services,
                                       const std::set<wrench::StorageService *> &storage_services,
                                       std::string hostname) :
            wrench::WMS(nullptr, nullptr,  compute_services, storage_services,
                        {}, nullptr, hostname, "test") {
      this->test = test;
    }


private:

    BatchServiceTest *test;

    int main() {
      // Create a job manager
      std::shared_ptr<wrench::JobManager> job_manager = this->createJobManager();

      {
        //Let's load the trace file
        std::vector<std::pair<double, std::tuple<std::string, double, int, int, double, int>>>
                trace_file_tasks = wrench::TraceFileLoader::loadFromTraceFile("test/trace_files/NASA-iPSC-1993-3.swf",
                                                                              0);
        for (auto tasks_info:trace_file_tasks) {
          double sub_time = tasks_info.first;
          double curtime = wrench::S4U_Simulation::getClock();
          double sleeptime = sub_time - curtime;
          if (sleeptime > 0)
            wrench::S4U_Simulation::sleep(sleeptime);
          std::tuple<std::string, double, int, int, double, int> task_args = std::get<1>(tasks_info);
          std::string id = std::get<0>(task_args);
          double flops = std::get<1>(task_args);
          int min_num_cores = std::get<2>(task_args);
          int max_num_cores = std::get<3>(task_args);
          double parallel_efficiency = std::get<4>(task_args);
          int num_nodes = std::get<5>(task_args);
          if (min_num_cores == -1 || max_num_cores == -1) {
            min_num_cores = 10;
            max_num_cores = 10;
          }
          if (num_nodes > 4) {
            continue;
          }
          wrench::WorkflowTask *task = this->workflow->addTask(id, flops, min_num_cores, max_num_cores,
                                                               parallel_efficiency);

          wrench::StandardJob *job = job_manager->createStandardJob(
                  {task},
                  {},
                  {},
                  {},
                  {});


          std::map<std::string, std::string> batch_job_args;
          batch_job_args["-N"] = std::to_string(num_nodes);
          batch_job_args["-t"] = std::to_string(flops);
          batch_job_args["-c"] = std::to_string(min_num_cores); //use all cores
          try {
            job_manager->submitJob(job, this->test->compute_service, batch_job_args);
          } catch (wrench::WorkflowExecutionException &e) {
            throw std::runtime_error(
                    "Got some exception"
            );
          }

          // Wait for a fake job submission reply
          std::unique_ptr<wrench::SimulationMessage> message = nullptr;
          try {
            std::cout << "Listening to mailbox_name " << this->workflow->getCallbackMailbox() << "\n";
            message = wrench::S4U_Mailbox::getMessage(this->workflow->getCallbackMailbox());
          } catch (std::shared_ptr<wrench::NetworkError> &cause) {
            throw wrench::WorkflowExecutionException(cause);
          }

          if (auto *m = dynamic_cast<wrench::ComputeServiceInformationMessage *>(message.get())) {
            std::cout << "Resources information obtained " << m->information << "\n";
          } else {
            throw std::runtime_error(
                    "BatchServiceTest::BatchFakeJobSubmissionTestWMS(): Reply from Fake Job submission was not obtained");
          }

          this->workflow->removeTask(task);
        }
      }

      return 0;
    }
};

#ifdef ENABLE_BATSCHED

TEST_F(BatchServiceTest, DISABLED_BatchTraceFileJobSubmissionTest) {
  DO_TEST_WITH_FORK(do_BatchTraceFileJobSubmissionTest_test);
}

#else

TEST_F(BatchServiceTest, DISABLED_BatchTraceFileJobSubmissionTest) {
  DO_TEST_WITH_FORK(do_BatchTraceFileJobSubmissionTest_test);
}

#endif


void BatchServiceTest::do_BatchTraceFileJobSubmissionTest_test() {

  // Create and initialize a simulation
  auto simulation = new wrench::Simulation();
  int argc = 1;
  auto argv = (char **) calloc(1, sizeof(char *));
  argv[0] = strdup("batch_service_test");

  EXPECT_NO_THROW(simulation->init(&argc, argv));

  // Setting up the platform
  EXPECT_NO_THROW(simulation->instantiatePlatform(platform_file_path));

  // Get a hostname
  std::string hostname = simulation->getHostnameList()[0];

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service1 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service2 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Batch Service
  EXPECT_NO_THROW(compute_service = simulation->add(
                  new wrench::BatchService(hostname, true, true,
                                           simulation->getHostnameList(), storage_service1, {
                                           })));

  // Create a WMS
  wrench::WMS *wms = nullptr;
  EXPECT_NO_THROW(wms = simulation->add(
          new BatchTraceFileJobSubmissionTestWMS(
                  this, {compute_service}, {storage_service1, storage_service2}, hostname)));

  EXPECT_NO_THROW(wms->addWorkflow(std::move(workflow.get())));

  simulation->setFileRegistryService(new wrench::FileRegistryService(hostname));
  // Create two workflow files
  wrench::WorkflowFile *input_file = this->workflow->addFile("input_file", 10000.0);
  wrench::WorkflowFile *output_file = this->workflow->addFile("output_file", 20000.0);

  // Staging the input_file on the storage service
  EXPECT_NO_THROW(simulation->stageFile(input_file, storage_service1));


  // Running a "run a single task" simulation
  // Note that in these tests the WMS creates workflow tasks, which a user would
  // of course not be likely to do
  EXPECT_NO_THROW(simulation->launch());

  delete simulation;

  free(argv[0]);
  free(argv);
}

/**********************************************************************/
/**  BASIC QUERY/ANSWER BATCH_JOB_ESTIMATE_WAITING_TIME **/
/**********************************************************************/

class BatchJobBasicEstimateWaitingTimeTestWMS : public wrench::WMS {

public:
    BatchJobBasicEstimateWaitingTimeTestWMS(BatchServiceTest *test,
                                       const std::set<wrench::ComputeService *> &compute_services,
                                       const std::set<wrench::StorageService *> &storage_services,
                                       std::string hostname) :
            wrench::WMS(nullptr, nullptr,  compute_services, storage_services, {}, nullptr,
                        hostname, "test") {
      this->test = test;
    }


private:

    BatchServiceTest *test;

    int main() {
      // Create a job manager
      std::shared_ptr<wrench::JobManager> job_manager = this->createJobManager();

      {

        // Create a sequential task that lasts one min and requires 2 cores
        wrench::WorkflowTask *task = this->workflow->addTask("task", 299, 1, 1, 1.0);
        task->addInputFile(this->workflow->getFileById("input_file"));
        task->addOutputFile(this->workflow->getFileById("output_file"));


        // Create a StandardJob with some pre-copies and post-deletions (not useful, but this is testing after all)

        wrench::StandardJob *job = job_manager->createStandardJob(
                {task},
                {
                        {*(task->getInputFiles().begin()),  this->test->storage_service1},
                        {*(task->getOutputFiles().begin()), this->test->storage_service1}
                },
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *, wrench::StorageService *>(
                        this->workflow->getFileById("input_file"), this->test->storage_service1,
                        this->test->storage_service2)},
                {},
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *>(this->workflow->getFileById("input_file"),
                                                                              this->test->storage_service2)});

        std::map<std::string, std::string> batch_job_args;
        batch_job_args["-N"] = "4";
        batch_job_args["-t"] = "5"; //time in minutes
        batch_job_args["-c"] = "1"; //number of cores per node
        try {
          job_manager->submitJob(job, this->test->compute_service, batch_job_args);
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error(
                  "Got some exception"
          );
        }
        double first_job_running = this->simulation->getCurrentSimulatedDate();

        auto *batch_service = dynamic_cast<wrench::BatchService *>((*this->compute_services.begin()));
        std::string job_id = "my_tentative_job";
        unsigned int nodes = 2;
        double walltime_seconds = 1000;
        //std::tuple<std::string,unsigned int,double> my_job = {job_id,nodes,walltime_seconds};
        std::tuple<std::string,unsigned int,double> my_job = std::make_tuple(job_id,nodes,walltime_seconds);
        std::set<std::tuple<std::string,unsigned int,double>> set_of_jobs = {my_job};
        std::map<std::string,double> jobs_estimated_waiting_time = batch_service->getQueueWaitingTimeEstimate(set_of_jobs);
        double expected_wait_time = 300 - first_job_running;
        double tolerance = 1; // in seconds
        double delta = fabs(expected_wait_time - (jobs_estimated_waiting_time[job_id] - tolerance));
         if (delta > 1) {
          throw std::runtime_error("Estimated queue wait time incorrect (expected: " + std::to_string(expected_wait_time) + ", got: " + std::to_string(jobs_estimated_waiting_time[job_id]) + ")");
        }


        // Wait for a workflow execution event
        std::unique_ptr<wrench::WorkflowExecutionEvent> event;
        try {
          event = this->workflow->waitForNextExecutionEvent();
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error("Error while getting and execution event: " + e.getCause()->toString());
        }
        switch (event->type) {
          case wrench::WorkflowExecutionEvent::STANDARD_JOB_COMPLETION: {
            // success, do nothing for now
            break;
          }
          default: {
            throw std::runtime_error("Unexpected workflow execution event: " + std::to_string((int) (event->type)));
          }
        }
        this->workflow->removeTask(task);

      }


      return 0;
    }
};

#ifdef ENABLE_BATSCHED

TEST_F(BatchServiceTest, BatchJobBasicEstimateWaitingTimeTest) {
  DO_TEST_WITH_FORK(do_BatchJobBasicEstimateWaitingTimeTest_test);
}

#else

TEST_F(BatchServiceTest, DISABLED_BatchJobBasicEstimateWaitingTimeTest) {
  DO_TEST_WITH_FORK(do_BatchJobBasicEstimateWaitingTimeTest_test);
}

#endif


void BatchServiceTest::do_BatchJobBasicEstimateWaitingTimeTest_test() {

  // Create and initialize a simulation
  auto simulation = new wrench::Simulation();
  int argc = 1;
  auto argv = (char **) calloc(1, sizeof(char *));
  argv[0] = strdup("batch_service_test");

  EXPECT_NO_THROW(simulation->init(&argc, argv));

  // Setting up the platform
  EXPECT_NO_THROW(simulation->instantiatePlatform(platform_file_path));

  // Get a hostname
  std::string hostname = simulation->getHostnameList()[0];

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service1 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service2 = simulation->add(
                  new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Batch Service
  EXPECT_NO_THROW(compute_service = simulation->add(
                  new wrench::BatchService(hostname, true, true,
                                           simulation->getHostnameList(), storage_service1, {
                                                   {wrench::BatchServiceProperty::BATCH_SCHEDULING_ALGORITHM, "conservative_bf"}
                                           })));

  // Create a WMS
  wrench::WMS *wms = nullptr;
  EXPECT_NO_THROW(wms = simulation->add(new BatchJobBasicEstimateWaitingTimeTestWMS(
                  this, {compute_service}, {storage_service1, storage_service2}, hostname)));

  EXPECT_NO_THROW(wms->addWorkflow(std::move(workflow.get())));

//  std::unique_ptr<wrench::FileRegistryService> file_registry_service(
//          new wrench::FileRegistryService(hostname));

  simulation->setFileRegistryService(new wrench::FileRegistryService(hostname));

  // Create two workflow files
  wrench::WorkflowFile *input_file = this->workflow->addFile("input_file", 10000.0);
  wrench::WorkflowFile *output_file = this->workflow->addFile("output_file", 20000.0);

  // Staging the input_file on the storage service
  EXPECT_NO_THROW(simulation->stageFile(input_file, storage_service1));


  // Running a "run a single task" simulation
  // Note that in these tests the WMS creates workflow tasks, which a user would
  // of course not be likely to do
  EXPECT_NO_THROW(simulation->launch());

  delete simulation;

  free(argv[0]);
  free(argv);
}


/**********************************************************************/
/**  QUERY/ANSWER BATCH_JOB_ESTIMATE_WAITING_TIME **/
/**********************************************************************/

class BatchJobEstimateWaitingTimeTestWMS : public wrench::WMS {

public:
    BatchJobEstimateWaitingTimeTestWMS(BatchServiceTest *test,
                                       const std::set<wrench::ComputeService *> &compute_services,
                                       const std::set<wrench::StorageService *> &storage_services,
                                       std::string hostname) :
            wrench::WMS(nullptr, nullptr,  compute_services, storage_services, {}, nullptr,
                        hostname, "test") {
      this->test = test;
    }


private:

    BatchServiceTest *test;

    int main() {
      // Create a job manager
      std::shared_ptr<wrench::JobManager> job_manager = this->createJobManager();

      {

        // Create a sequential task that lasts one min and requires 2 cores
        wrench::WorkflowTask *task = this->workflow->addTask("task", 299, 1, 1, 1.0);
        task->addInputFile(this->workflow->getFileById("input_file"));
        task->addOutputFile(this->workflow->getFileById("output_file"));


        // Create a StandardJob with some pre-copies and post-deletions (not useful, but this is testing after all)

        wrench::StandardJob *job = job_manager->createStandardJob(
                {task},
                {
                        {*(task->getInputFiles().begin()),  this->test->storage_service1},
                        {*(task->getOutputFiles().begin()), this->test->storage_service1}
                },
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *, wrench::StorageService *>(
                        this->workflow->getFileById("input_file"), this->test->storage_service1,
                        this->test->storage_service2)},
                {},
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *>(this->workflow->getFileById("input_file"),
                                                                              this->test->storage_service2)});

        std::map<std::string, std::string> batch_job_args;
        batch_job_args["-N"] = "4";
        batch_job_args["-t"] = "5"; //time in minutes
        batch_job_args["-c"] = "1"; //number of cores per node
        try {
          job_manager->submitJob(job, this->test->compute_service, batch_job_args);
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error(
                  "Got some exception"
          );
        }
        double first_job_running = this->simulation->getCurrentSimulatedDate();


        auto *batch_service = dynamic_cast<wrench::BatchService *>((*this->compute_services.begin()));
        std::string job_id = "my_job1";
        unsigned int nodes = 2;
        double walltime_seconds = 1000;
        std::tuple<std::string,unsigned int,double> my_job = std::make_tuple(job_id,nodes,walltime_seconds);
        std::set<std::tuple<std::string,unsigned int,double>> set_of_jobs = {my_job};
        std::map<std::string,double> jobs_estimated_waiting_time = batch_service->getQueueWaitingTimeEstimate(set_of_jobs);
        double expected_wait_time = 300 - first_job_running;
        double delta = fabs(expected_wait_time - (jobs_estimated_waiting_time[job_id] - 1));  // TODO: Weird 5s correction applied to what conservative_bf tells us
        if (delta > 1) { // 1 second accuracy threshold
          throw std::runtime_error("Estimated queue wait time incorrect (expected: " + std::to_string(expected_wait_time) + ", got: " + std::to_string(jobs_estimated_waiting_time[job_id]) + ")");
        }


        // Wait for a workflow execution event
        std::unique_ptr<wrench::WorkflowExecutionEvent> event;
        try {
          event = this->workflow->waitForNextExecutionEvent();
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error("Error while getting and execution event: " + e.getCause()->toString());
        }
        switch (event->type) {
          case wrench::WorkflowExecutionEvent::STANDARD_JOB_COMPLETION: {
            // success, do nothing for now
            break;
          }
          default: {
            throw std::runtime_error("Unexpected workflow execution event: " + std::to_string((int) (event->type)));
          }
        }
        this->workflow->removeTask(task);

      }

      {

        // Create a sequential task that lasts one min and requires 2 cores
        wrench::WorkflowTask *task = this->workflow->addTask("task", 60, 2, 2, 1.0);
        task->addInputFile(this->workflow->getFileById("input_file"));
        task->addOutputFile(this->workflow->getFileById("output_file"));


        // Create a StandardJob with some pre-copies and post-deletions (not useful, but this is testing after all)

        wrench::StandardJob *job = job_manager->createStandardJob(
                {task},
                {
                        {*(task->getInputFiles().begin()),  this->test->storage_service1},
                        {*(task->getOutputFiles().begin()), this->test->storage_service1}
                },
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *, wrench::StorageService *>(
                        this->workflow->getFileById("input_file"), this->test->storage_service1,
                        this->test->storage_service2)},
                {},
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *>(this->workflow->getFileById("input_file"),
                                                                              this->test->storage_service2)});

        std::map<std::string, std::string> batch_job_args;
        batch_job_args["-N"] = "4";
        batch_job_args["-t"] = "5"; //time in minutes
        batch_job_args["-c"] = "4"; //number of cores per node
        try {
          job_manager->submitJob(job, this->test->compute_service, batch_job_args);
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error(
                  "Got some exception"
          );
        }

        auto *batch_service = dynamic_cast<wrench::BatchService *>((*this->compute_services.begin()));
        std::set<std::tuple<std::string, unsigned int, double>> set_of_jobs = {};
        for (int i=0; i<10; i++) {
          std::string job_id = "new_job"+std::to_string(i);
          unsigned int nodes = rand() % 4 + 1;
          double walltime_seconds = nodes * (rand() % 10 + 1);
          std::tuple<std::string, unsigned int, double> my_job = std::make_tuple(job_id, nodes, walltime_seconds);
          set_of_jobs.insert(my_job);
        }
        std::map<std::string, double> jobs_estimated_waiting_time = batch_service->getQueueWaitingTimeEstimate(
                set_of_jobs);

        // Wait for a workflow execution event
        std::unique_ptr<wrench::WorkflowExecutionEvent> event;
        try {
          event = this->workflow->waitForNextExecutionEvent();
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error("Error while getting and execution event: " + e.getCause()->toString());
        }
        switch (event->type) {
          case wrench::WorkflowExecutionEvent::STANDARD_JOB_COMPLETION: {
            // success, do nothing for now
            break;
          }
          default: {
            throw std::runtime_error("Unexpected workflow execution event: " + std::to_string((int) (event->type)));
          }
        }
        this->workflow->removeTask(task);

      }

      return 0;
    }
};

#ifdef ENABLE_BATSCHED

TEST_F(BatchServiceTest, BatchJobEstimateWaitingTimeTest) {
  DO_TEST_WITH_FORK(do_BatchJobEstimateWaitingTimeTest_test);
}

#else

TEST_F(BatchServiceTest, DISABLED_BatchJobEstimateWaitingTimeTest) {
  DO_TEST_WITH_FORK(do_BatchJobEstimateWaitingTimeTest_test);
}

#endif


void BatchServiceTest::do_BatchJobEstimateWaitingTimeTest_test() {

  // Create and initialize a simulation
  auto simulation = new wrench::Simulation();
  int argc = 1;
  auto argv = (char **) calloc(1, sizeof(char *));
  argv[0] = strdup("batch_service_test");

  EXPECT_NO_THROW(simulation->init(&argc, argv));

  // Setting up the platform
  EXPECT_NO_THROW(simulation->instantiatePlatform(platform_file_path));

  // Get a hostname
  std::string hostname = simulation->getHostnameList()[0];

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service1 = simulation->add(

          new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service2 = simulation->add(
          new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Batch Service
  EXPECT_NO_THROW(compute_service = simulation->add(
          new wrench::BatchService(hostname, true, true,
                                   simulation->getHostnameList(), storage_service1, {
                                           {wrench::BatchServiceProperty::BATCH_SCHEDULING_ALGORITHM, "conservative_bf"},
                                           {wrench::BatchServiceProperty::BATCH_RJMS_DELAY, "0"}
                                   })));

  simulation->setFileRegistryService(new wrench::FileRegistryService(hostname));

  // Create a WMS
  wrench::WMS *wms = nullptr;
  EXPECT_NO_THROW(wms = simulation->add(new BatchJobEstimateWaitingTimeTestWMS(
          this, {compute_service}, {storage_service1, storage_service2}, hostname)));

  // Create two workflow files
  wrench::WorkflowFile *input_file = this->workflow->addFile("input_file", 10000.0);
  wrench::WorkflowFile *output_file = this->workflow->addFile("output_file", 20000.0);

  EXPECT_NO_THROW(wms->addWorkflow(std::move(workflow.get())));

  // Staging the input_file on the storage service
  EXPECT_NO_THROW(simulation->stageFile(input_file, storage_service1));

  // Running a "run a single task" simulation
  // Note that in these tests the WMS creates workflow tasks, which a user would
  // of course not be likely to do
  EXPECT_NO_THROW(simulation->launch());

  delete simulation;

  free(argv[0]);
  free(argv);
}


/**********************************************************************/
/**  QUERY/ANSWER BATCH_JOB_LITTLE_COMPLEX_ESTIMATE_WAITING_TIME **/
/**********************************************************************/

class BatchJobLittleComplexEstimateWaitingTimeTestWMS : public wrench::WMS {

public:
    BatchJobLittleComplexEstimateWaitingTimeTestWMS(BatchServiceTest *test,
                                       const std::set<wrench::ComputeService *> &compute_services,
                                       const std::set<wrench::StorageService *> &storage_services,
                                       std::string hostname) :
            wrench::WMS(nullptr, nullptr,  compute_services, storage_services, {}, nullptr,
                        hostname, "test") {
      this->test = test;
    }


private:

    BatchServiceTest *test;

    int main() {
      // Create a job manager
      std::shared_ptr<wrench::JobManager> job_manager = this->createJobManager();

      {

        // Submit the first job for 300 seconds and using 4 full cores
        wrench::WorkflowTask *task = this->workflow->addTask("task", 299, 1, 1, 1.0);
        task->addInputFile(this->workflow->getFileById("input_file"));
        task->addOutputFile(this->workflow->getFileById("output_file"));


        // Create a StandardJob with some pre-copies and post-deletions (not useful, but this is testing after all)

        wrench::StandardJob *job = job_manager->createStandardJob(
                {task},
                {
                        {*(task->getInputFiles().begin()),  this->test->storage_service1},
                        {*(task->getOutputFiles().begin()), this->test->storage_service1}
                },
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *, wrench::StorageService *>(
                        this->workflow->getFileById("input_file"), this->test->storage_service1,
                        this->test->storage_service2)},
                {},
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *>(this->workflow->getFileById("input_file"),
                                                                              this->test->storage_service2)});


        std::map<std::string, std::string> batch_job_args;
        batch_job_args["-N"] = "4";
        batch_job_args["-t"] = "5"; //time in minutes
        batch_job_args["-c"] = "1"; //number of cores per node
        try {
          job_manager->submitJob(job, this->test->compute_service, batch_job_args);
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error(
                  "Got some exception"
          );
        }


        // Submit the second job for next 300 seconds and using 2 cores
        wrench::WorkflowTask *task1 = this->workflow->addTask("task1", 299, 1, 1, 1.0);
        task1->addInputFile(this->workflow->getFileById("input_file"));
        task1->addOutputFile(this->workflow->getFileById("output_file"));


        // Create a StandardJob with some pre-copies and post-deletions (not useful, but this is testing after all)

        wrench::StandardJob *job1 = job_manager->createStandardJob(
                {task1},
                {
                        {*(task1->getInputFiles().begin()),  this->test->storage_service1},
                        {*(task1->getOutputFiles().begin()), this->test->storage_service1}
                },
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *, wrench::StorageService *>(
                        this->workflow->getFileById("input_file"), this->test->storage_service1,
                        this->test->storage_service2)},
                {},
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *>(this->workflow->getFileById("input_file"),
                                                                              this->test->storage_service2)});

        std::map<std::string, std::string> batch_job_args1;
        batch_job_args1["-N"] = "2";
        batch_job_args1["-t"] = "5"; //time in minutes
        batch_job_args1["-c"] = "4"; //number of cores per node
        try {
          job_manager->submitJob(job1, this->test->compute_service, batch_job_args1);
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error(
                  "Got some exception"
          );
        }


        // Submit the third job for next 300 seconds and using 4 cores
        wrench::WorkflowTask *task2 = this->workflow->addTask("task2", 299, 1, 1, 1.0);
        task2->addInputFile(this->workflow->getFileById("input_file"));
        task2->addOutputFile(this->workflow->getFileById("output_file"));


        // Create a StandardJob with some pre-copies and post-deletions (not useful, but this is testing after all)

        wrench::StandardJob *job2 = job_manager->createStandardJob(
                {task2},
                {
                        {*(task2->getInputFiles().begin()),  this->test->storage_service1},
                        {*(task2->getOutputFiles().begin()), this->test->storage_service1}
                },
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *, wrench::StorageService *>(
                        this->workflow->getFileById("input_file"), this->test->storage_service1,
                        this->test->storage_service2)},
                {},
                {std::tuple<wrench::WorkflowFile *, wrench::StorageService *>(this->workflow->getFileById("input_file"),
                                                                              this->test->storage_service2)});

        std::map<std::string, std::string> batch_job_args2;
        batch_job_args2["-N"] = "4";
        batch_job_args2["-t"] = "5"; //time in minutes
        batch_job_args2["-c"] = "4"; //number of cores per node
        try {
          job_manager->submitJob(job2, this->test->compute_service, batch_job_args2);
        } catch (wrench::WorkflowExecutionException &e) {
          throw std::runtime_error(
                  "Got some exception"
          );
        }

        auto *batch_service = dynamic_cast<wrench::BatchService *>((*this->compute_services.begin()));

        std::string job_id = "my_job1";
        unsigned int nodes = 1;
        double walltime_seconds = 400;
        std::tuple<std::string,unsigned int,double> my_job = std::make_tuple(job_id,nodes,walltime_seconds);
        std::set<std::tuple<std::string,unsigned int,double>> set_of_jobs = {my_job};
        std::map<std::string,double> jobs_estimated_waiting_time = batch_service->getQueueWaitingTimeEstimate(set_of_jobs);

        if ((jobs_estimated_waiting_time[job_id] - 900) > 1) {
          throw std::runtime_error("Estimated queue wait time incorrect (expected: " + std::to_string(900) + ", got: " + std::to_string(jobs_estimated_waiting_time[job_id]) + ")");
        }

        job_id = "my_job2";
        nodes = 1;
        walltime_seconds = 299;
        my_job = std::make_tuple(job_id,nodes,walltime_seconds);
        set_of_jobs = {my_job};
        jobs_estimated_waiting_time = batch_service->getQueueWaitingTimeEstimate(set_of_jobs);

        if (fabs(jobs_estimated_waiting_time[job_id] - 300) > 1) {
          throw std::runtime_error("Estimated queue wait time incorrect (expected: " + std::to_string(300) + ", got: " + std::to_string(jobs_estimated_waiting_time[job_id]) + ")");
        }


        job_id = "my_job3";
        nodes = 2;
        walltime_seconds = 299;
        my_job = std::make_tuple(job_id,nodes,walltime_seconds);
        set_of_jobs = {my_job};
        jobs_estimated_waiting_time = batch_service->getQueueWaitingTimeEstimate(set_of_jobs);

        if (fabs(jobs_estimated_waiting_time[job_id] - 300) > 1) {
          throw std::runtime_error("Estimated queue wait time incorrect (expected: " + std::to_string(300) + ", got: " + std::to_string(jobs_estimated_waiting_time[job_id]) + ")");
        }


        job_id = "my_job4";
        nodes = 3;
        walltime_seconds = 299;
        my_job = std::make_tuple(job_id,nodes,walltime_seconds);
        set_of_jobs = {my_job};
        jobs_estimated_waiting_time = batch_service->getQueueWaitingTimeEstimate(set_of_jobs);

        if (fabs(jobs_estimated_waiting_time[job_id] - 900) > 1) {
          throw std::runtime_error("Estimated queue wait time incorrect (expected: " + std::to_string(900) + ", got: " + std::to_string(jobs_estimated_waiting_time[job_id]) + ")");
        }



        for (int i=0; i<3; i++) {

          // Wait for a workflow execution event
          std::unique_ptr<wrench::WorkflowExecutionEvent> event;
          try {
            event = this->workflow->waitForNextExecutionEvent();
          } catch (wrench::WorkflowExecutionException &e) {
            throw std::runtime_error("Error while getting and execution event: " + e.getCause()->toString());
          }
          switch (event->type) {
            case wrench::WorkflowExecutionEvent::STANDARD_JOB_COMPLETION: {
              // success, do nothing for now
              break;
            }
            default: {
              throw std::runtime_error("Unexpected workflow execution event: " + std::to_string((int) (event->type)));
            }
          }
        }

        this->workflow->removeTask(task);
        this->workflow->removeTask(task1);
        this->workflow->removeTask(task2);

      }

      return 0;
    }
};

#ifdef ENABLE_BATSCHED

TEST_F(BatchServiceTest, BatchJobLittleComplexEstimateWaitingTimeTest) {
  DO_TEST_WITH_FORK(do_BatchJobLittleComplexEstimateWaitingTimeTest_test);
}

#else

TEST_F(BatchServiceTest, DISABLED_BatchJobLittleComplexEstimateWaitingTimeTest) {
  DO_TEST_WITH_FORK(do_BatchJobLittleComplexEstimateWaitingTimeTest_test);
}

#endif


void BatchServiceTest::do_BatchJobLittleComplexEstimateWaitingTimeTest_test() {

  // Create and initialize a simulation
  auto simulation = new wrench::Simulation();
  int argc = 1;
  auto argv = (char **) calloc(1, sizeof(char *));
  argv[0] = strdup("batch_service_test");

  EXPECT_NO_THROW(simulation->init(&argc, argv));

  // Setting up the platform
  EXPECT_NO_THROW(simulation->instantiatePlatform(platform_file_path));

  // Get a hostname
  std::string hostname = simulation->getHostnameList()[0];

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service1 = simulation->add(
          new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Storage Service
  EXPECT_NO_THROW(storage_service2 = simulation->add(
          new wrench::SimpleStorageService(hostname, 10000000000000.0)));

  // Create a Batch Service
  EXPECT_NO_THROW(compute_service = simulation->add(
          new wrench::BatchService(hostname, true, true,
                                   simulation->getHostnameList(), storage_service1, {
                                           {wrench::BatchServiceProperty::BATCH_SCHEDULING_ALGORITHM, "conservative_bf"},
                                           {wrench::BatchServiceProperty::BATCH_RJMS_DELAY, "0"}
                                   })));

  // Create a WMS
  wrench::WMS *wms = nullptr;
  EXPECT_NO_THROW(wms = simulation->add(new BatchJobLittleComplexEstimateWaitingTimeTestWMS(
          this, {compute_service}, {storage_service1, storage_service2}, hostname)));

  EXPECT_NO_THROW(wms->addWorkflow(std::move(workflow.get())));

//  std::unique_ptr<wrench::FileRegistryService> file_registry_service(
//          new wrench::FileRegistryService(hostname));

  simulation->setFileRegistryService(new wrench::FileRegistryService(hostname));

  // Create two workflow files
  wrench::WorkflowFile *input_file = this->workflow->addFile("input_file", 10000.0);
  wrench::WorkflowFile *output_file = this->workflow->addFile("output_file", 20000.0);

  // Staging the input_file on the storage service
  EXPECT_NO_THROW(simulation->stageFile(input_file, storage_service1));


  // Running a "run a single task" simulation
  // Note that in these tests the WMS creates workflow tasks, which a user would
  // of course not be likely to do
  EXPECT_NO_THROW(simulation->launch());

  delete simulation;

  free(argv[0]);
  free(argv);
}

