// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/scheduler/null_worker_scheduler.h"

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"
#include "content/child/scheduler/null_idle_task_runner.h"

namespace content {

NullWorkerScheduler::NullWorkerScheduler()
    : task_runner_(base::MessageLoopProxy::current()),
      idle_task_runner_(new NullIdleTaskRunner()) {
}

NullWorkerScheduler::~NullWorkerScheduler() {
}

scoped_refptr<base::SingleThreadTaskRunner>
NullWorkerScheduler::DefaultTaskRunner() {
  return task_runner_;
}

scoped_refptr<SingleThreadIdleTaskRunner>
NullWorkerScheduler::IdleTaskRunner() {
  return idle_task_runner_;
}

void NullWorkerScheduler::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  base::MessageLoop::current()->AddTaskObserver(task_observer);
}

void NullWorkerScheduler::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  base::MessageLoop::current()->RemoveTaskObserver(task_observer);
}

bool NullWorkerScheduler::CanExceedIdleDeadlineIfRequired() const {
  return false;
}

void NullWorkerScheduler::Init() {
}

void NullWorkerScheduler::Shutdown() {
}

}  // namespace content
