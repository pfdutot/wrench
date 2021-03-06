/**
 * Copyright (c) 2017. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <gtest/gtest.h>

#include "wrench/workflow/Workflow.h"

class WorkflowTaskTest : public ::testing::Test {
protected:
    WorkflowTaskTest() {
      workflow = new wrench::Workflow();

      t1 = workflow->addTask("task-01", 100000);
      t2 = workflow->addTask("task-02", 100, 2, 4, 0.5);

      workflow->addControlDependency(t1, t2);
    }

    // data members
    wrench::Workflow *workflow;
    wrench::WorkflowTask *t1, *t2;
};

TEST_F(WorkflowTaskTest, TaskStructure) {
  // WorkflowTask structure sanity check
  ASSERT_EQ(t1->getWorkflow(), workflow);

  ASSERT_NE(t1->getId(), t2->getId());
  EXPECT_EQ(t1->getId(), "task-01");
  EXPECT_NE(t2->getId(), "task-01");

  ASSERT_GT(t1->getFlops(), t2->getFlops());

  ASSERT_EQ(t1->getMinNumCores(), 1);
  ASSERT_EQ(t1->getMaxNumCores(), 1);
  ASSERT_EQ(t1->getParallelEfficiency(), 1.0);
  ASSERT_EQ(t2->getMinNumCores(), 2);
  ASSERT_EQ(t2->getMaxNumCores(), 4);
  ASSERT_EQ(t2->getParallelEfficiency(), 0.5);

  EXPECT_EQ(t1->getState(), wrench::WorkflowTask::State::READY);
  EXPECT_EQ(t2->getState(), wrench::WorkflowTask::State::NOT_READY); // due to control dependency

  EXPECT_EQ(t1->getJob(), nullptr);
  EXPECT_EQ(t2->getJob(), nullptr);

  EXPECT_EQ(t1->getNumberOfParents(), 0);
  EXPECT_EQ(t2->getNumberOfParents(), 1);

  EXPECT_EQ(t1->getNumberOfChildren(), 1);
  EXPECT_EQ(t2->getNumberOfChildren(), 0);

  EXPECT_EQ(t1->getClusterId(), "");
}

TEST_F(WorkflowTaskTest, GetSet) {
  t1->setState(wrench::WorkflowTask::State::NOT_READY);
  EXPECT_EQ(t1->getState(), wrench::WorkflowTask::State::NOT_READY);

  t1->setClusterId("my-cluster-id");
  EXPECT_EQ(t1->getClusterId(), "my-cluster-id");

  t1->setReady();
  EXPECT_EQ(t1->getState(), wrench::WorkflowTask::State::READY);
  EXPECT_EQ(t2->getState(), wrench::WorkflowTask::State::NOT_READY);

  // Now, we can to any kind of setting
  // ASSERT_THROW(t1->setCompleted(), std::runtime_error);

  t1->setRunning();
  EXPECT_EQ(t1->getState(), wrench::WorkflowTask::State::RUNNING);

  // Now, we can to any kind of setting
  // ASSERT_THROW(t1->setReady(), std::runtime_error);

  t1->setCompleted();
  EXPECT_EQ(t1->getState(), wrench::WorkflowTask::State::COMPLETED);
  EXPECT_EQ(t2->getState(), wrench::WorkflowTask::State::READY);

  EXPECT_NO_THROW(t1->setEndDate(1.0));

  EXPECT_EQ(t1->getFailureCount(), 0);
  t1->incrementFailureCount();
  EXPECT_EQ(t1->getFailureCount(), 1);
}

TEST_F(WorkflowTaskTest, InputOutputFile) {
  wrench::WorkflowFile *f1 = workflow->addFile("file-01", 10);
  wrench::WorkflowFile *f2 = workflow->addFile("file-02", 100);

  t1->addInputFile(f1);
  t1->addOutputFile(f2);

  ASSERT_THROW(t1->addInputFile(f1), std::invalid_argument);
  ASSERT_THROW(t1->addOutputFile(f2), std::invalid_argument);

  ASSERT_THROW(t1->addInputFile(f2), std::invalid_argument);
  ASSERT_THROW(t1->addOutputFile(f1), std::invalid_argument);

  wrench::WorkflowTask* t3 = workflow->addTask("task-03", 50, 2, 4, 1.0);
  t3->addInputFile(f2);

  EXPECT_EQ(t3->getNumberOfParents(), 1);
}

TEST_F(WorkflowTaskTest, StateToString) {
  EXPECT_EQ(wrench::WorkflowTask::stateToString(wrench::WorkflowTask::State::NOT_READY), "NOT READY");
  EXPECT_EQ(wrench::WorkflowTask::stateToString(wrench::WorkflowTask::State::READY), "READY");
  EXPECT_EQ(wrench::WorkflowTask::stateToString(wrench::WorkflowTask::State::PENDING), "PENDING");
  EXPECT_EQ(wrench::WorkflowTask::stateToString(wrench::WorkflowTask::State::RUNNING), "RUNNING");
  EXPECT_EQ(wrench::WorkflowTask::stateToString(wrench::WorkflowTask::State::COMPLETED), "COMPLETED");
  EXPECT_EQ(wrench::WorkflowTask::stateToString(wrench::WorkflowTask::State::FAILED), "FAILED");
  EXPECT_EQ(wrench::WorkflowTask::stateToString((wrench::WorkflowTask::State)100), "UNKNOWN STATE");
}


