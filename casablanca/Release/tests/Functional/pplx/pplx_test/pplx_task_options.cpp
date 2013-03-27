/***
* ==++==
*
* Copyright (c) Microsoft Corporation. All rights reserved. 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* ==--==
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* Basic tests for PPLX task options.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

class TaskOptionsTestScheduler : public pplx::scheduler
{
public:
    TaskOptionsTestScheduler() : m_scheduler(pplx::get_ambient_scheduler()), m_numTasks(0)
    {
    }

    virtual void schedule(pplx::TaskProc proc, void* param)
    {
        pplx::atomic_increment(m_numTasks);
        m_scheduler->schedule(proc, param);
    }

    long get_num_tasks()
    {
        return m_numTasks;
    }

private:

    pplx::atomic_long m_numTasks;
    pplx::scheduler_ptr m_scheduler;

	TaskOptionsTestScheduler(const TaskOptionsTestScheduler &);
	TaskOptionsTestScheduler & operator=(const TaskOptionsTestScheduler &);
};

class CheckLifetimeScheduler : public pplx::scheduler
{
public:

    CheckLifetimeScheduler(pplx::notification_event& ev)
        : m_event(ev), m_numTasks(0)
    {
    }

    ~CheckLifetimeScheduler()
    {
        m_event.set();
    }

    virtual void schedule(pplx::TaskProc proc, void* param)
    {
		pplx::atomic_increment(m_numTasks);
        pplx::get_ambient_scheduler()->schedule(proc, param);
    }

    long get_num_tasks()
    {
        return m_numTasks;
    }

    pplx::notification_event& m_event;
	pplx::atomic_long m_numTasks;
};

namespace tests { namespace functional { namespace PPLX {

SUITE(pplx_task_options_tests)
{

TEST(voidtask_schedoption_test)
{
    TaskOptionsTestScheduler sched;
    long n=0;

    auto t1 = pplx::create_task([&n]() {n++;}, sched); // run on sched
    t1.wait();

    VERIFY_ARE_EQUAL(sched.get_num_tasks(), n);
}

TEST(task_schedoption_test)
{
    TaskOptionsTestScheduler sched;
    long n=0;

    auto t1 = pplx::create_task([&n]() -> int {n++; return 1;}, sched); // run on sched
    t1.wait();

    VERIFY_ARE_EQUAL(sched.get_num_tasks(), n);
}

TEST(then_nooptions_test)
{
    TaskOptionsTestScheduler sched;
    long n=0;

    auto t1 = pplx::create_task([&n]() {n++;}, sched);
    t1.then([&n](){n++;}) // inherit sched
      .then([&n](){n++;})
      .wait();

    VERIFY_ARE_EQUAL(sched.get_num_tasks(), n);
}

TEST(then_multiple_schedulers_test1)
{
    TaskOptionsTestScheduler sched1;
    TaskOptionsTestScheduler sched2;
    
    auto emptyFunc = [](){};

    auto t1 = pplx::create_task(emptyFunc, sched1); // sched1
    t1.then(emptyFunc, sched2).wait(); // sched2

    VERIFY_ARE_EQUAL(sched1.get_num_tasks(), 1);
    VERIFY_ARE_EQUAL(sched2.get_num_tasks(), 1);
}

TEST(then_multiple_schedulers_test2)
{
    TaskOptionsTestScheduler sched1;
    TaskOptionsTestScheduler sched2;
    
    auto emptyFunc = [](){};

    auto t1 = pplx::create_task(emptyFunc, sched1);
    t1.then(emptyFunc, sched2)
      .then(emptyFunc) // inherit sched2
      .wait();

    VERIFY_ARE_EQUAL(sched1.get_num_tasks(), 1);
    VERIFY_ARE_EQUAL(sched2.get_num_tasks(), 2);
}

TEST(opand_nooptions_test)
{
    TaskOptionsTestScheduler sched;

    auto t1 = pplx::create_task([]() {}, sched);
    auto t2 = pplx::create_task([]() {}, sched);

    auto t3 = t1 && t2; // Does not run on the scheduler - it should run inline
    t3.then([](){}, sched).wait(); // run on sched
    
    VERIFY_ARE_EQUAL(sched.get_num_tasks(), 3);
}

TEST(whenall_nooptions_test)
{
    TaskOptionsTestScheduler sched1;
    TaskOptionsTestScheduler sched2;

    std::vector<pplx::task<void>> taskVect;
    const int n  = 10;
    for (int i = 0; i < n; i++)
    {
        taskVect.push_back(pplx::create_task([]() {}, sched1));
    }

    auto t3 = pplx::when_all(begin(taskVect), end(taskVect)); // Does not run on the scheduler - it should run inline
    t3.then([](){}, sched2).wait(); // run on sched2
    
    VERIFY_ARE_EQUAL(sched1.get_num_tasks(), n);
    VERIFY_ARE_EQUAL(sched2.get_num_tasks(), 1);
}

TEST(whenall_options_test1)
{
    TaskOptionsTestScheduler sched1;
    TaskOptionsTestScheduler sched2;

    std::vector<pplx::task<void>> taskVect;
    const int n  = 10;
    for (int i = 0; i < n; i++)
    {
        taskVect.push_back(pplx::create_task([]() {}, sched1));
    }

    auto t3 = pplx::when_all(begin(taskVect), end(taskVect), sched2); // Does not run on the scheduler - it should run inline
    t3.then([](){}).wait(); // run on sched2 (inherits from the when_all task
    
    VERIFY_ARE_EQUAL(sched1.get_num_tasks(), n);
    VERIFY_ARE_EQUAL(sched2.get_num_tasks(), 1);
}

TEST(whenall_options_test2)
{
    // Same as the above test but use task<int> to instatinate those templates
    TaskOptionsTestScheduler sched1;
    TaskOptionsTestScheduler sched2;

    std::vector<pplx::task<int>> taskVect;
    const int n  = 10;
    for (int i = 0; i < n; i++)
    {
        taskVect.push_back(pplx::create_task([i]() ->int { return i;}, sched1));
    }

    auto t3 = pplx::when_all(begin(taskVect), end(taskVect), sched2); // Does not run on the scheduler - it should run inline
    t3.then([](std::vector<int>){}).wait(); // run on sched2 (inherits from the when_all task
    
    VERIFY_ARE_EQUAL(sched1.get_num_tasks(), n);
    VERIFY_ARE_EQUAL(sched2.get_num_tasks(), 1);
}

TEST(whenall_options_test3)
{
    // Same as the above test but use multiple when_all
    TaskOptionsTestScheduler sched1;
    TaskOptionsTestScheduler sched2;

    std::vector<pplx::task<int>> taskVect;
    const int n  = 10;
    for (int i = 0; i < n; i++)
    {
        taskVect.push_back(pplx::create_task([i]() ->int { return i;}, sched1));
    }
    
    auto t2 = pplx::create_task([]()->int { return 0;}, sched1);

    auto t3 = pplx::when_all(begin(taskVect), end(taskVect), sched2); // Does not run on the scheduler - it should run inline

    auto t4 = t2 && t3;
    t4.then([](std::vector<int>){}).wait(); // run on default scheduler as the operator && breaks the chain of inheritance
    
    VERIFY_ARE_EQUAL(sched1.get_num_tasks(), n+1);
    VERIFY_ARE_EQUAL(sched2.get_num_tasks(), 0);
}

TEST(opor_nooptions_test)
{
    TaskOptionsTestScheduler sched;

    auto t1 = pplx::create_task([]() {}, sched);
    auto t2 = pplx::create_task([]() {}, sched);

    auto t3 = t1 || t2; // Runs inline
    t3.then([](){}, sched).wait();
    
    VERIFY_ARE_EQUAL(sched.get_num_tasks(), 3);
}

TEST(whenany_nooptions_test)
{
    TaskOptionsTestScheduler sched1;
    TaskOptionsTestScheduler sched2;

    std::vector<pplx::task<void>> taskVect;
    const int n  = 10;
    for (int i = 0; i < n; i++)
    {
        taskVect.push_back(pplx::create_task([]() {}, sched1));
    }

    auto t3 = pplx::when_any(begin(taskVect), end(taskVect)); // Does not run on the scheduler - it should run inline
    t3.then([](size_t){}, sched2).wait(); // run on sched2
    
    // Do a whenall to wait for all the tasks
    pplx::when_all(begin(taskVect), end(taskVect)).wait();

    VERIFY_ARE_EQUAL(sched1.get_num_tasks(), n);
    VERIFY_ARE_EQUAL(sched2.get_num_tasks(), 1);
}

TEST(whenany_options_test1)
{
    TaskOptionsTestScheduler sched1;
    TaskOptionsTestScheduler sched2;

    std::vector<pplx::task<void>> taskVect;
    const int n  = 10;
    for (int i = 0; i < n; i++)
    {
        taskVect.push_back(pplx::create_task([]() {}, sched1));
    }

    auto t3 = pplx::when_any(begin(taskVect), end(taskVect), sched2); // Does not run on the scheduler - it should run inline
    t3.then([](size_t){}).wait(); // run on sched2 (inherits from the when_all task
    
    // Do a whenall to wait for all the tasks
    pplx::when_all(begin(taskVect), end(taskVect)).wait();

    VERIFY_ARE_EQUAL(sched1.get_num_tasks(), n);
    VERIFY_ARE_EQUAL(sched2.get_num_tasks(), 1);
}

TEST(whenany_options_test2)
{
    // Same as whenany_options_test1 except that we instantiate a different set of template functions
    TaskOptionsTestScheduler sched1;
    TaskOptionsTestScheduler sched2;

    std::vector<pplx::task<int>> taskVect;
    const int n  = 10;
    for (int i = 0; i < n; i++)
    {
        taskVect.push_back(pplx::create_task([]() -> int {return 0;}, sched1));
    }

    auto t3 = pplx::when_any(begin(taskVect), end(taskVect), sched2); // Does not run on the scheduler - it should run inline
    t3.then([](std::pair<int, size_t>){}).wait(); // run on sched2 (inherits from the when_all task
    
    // Do a whenall to wait for all the tasks
    pplx::when_all(begin(taskVect), end(taskVect)).wait();

    VERIFY_ARE_EQUAL(sched1.get_num_tasks(), n);
    VERIFY_ARE_EQUAL(sched2.get_num_tasks(), 1);
}

TEST(tce_nooptions_test)
{
    TaskOptionsTestScheduler sched;
    TaskOptionsTestScheduler sched1;
    TaskOptionsTestScheduler sched2;

    pplx::task_completion_event<void> tce;
    auto t1 = pplx::create_task(tce, sched1);
    auto t2 = pplx::create_task(tce, sched2);

    tce.set();
    t1.wait();
    t2.wait();
    
    // There is nothing to execute
    VERIFY_ARE_EQUAL(sched1.get_num_tasks(), 0);
    VERIFY_ARE_EQUAL(sched2.get_num_tasks(), 0);

    auto emptyFunc = [](){};

    auto t3 = t1.then(emptyFunc);
    auto t4 = t2.then(emptyFunc);

    t3.wait();
    t4.wait();

    VERIFY_ARE_EQUAL(sched1.get_num_tasks(), 1);
    VERIFY_ARE_EQUAL(sched2.get_num_tasks(), 1);
}

TEST(fromresult_options_test)
{
    TaskOptionsTestScheduler sched;

    int value = 10;
    auto t1 = pplx::task_from_result(value); 
    t1.wait();
    VERIFY_ARE_EQUAL(sched.get_num_tasks(), 0);

    t1.then([](int i) -> int {return i;}, sched).wait();
    VERIFY_ARE_EQUAL(sched.get_num_tasks(), 1);

}

TEST(scheduler_lifetime)
{
    pplx::notification_event ev;
    {
        auto sched = std::make_shared<CheckLifetimeScheduler>(ev);

        pplx::create_task([]() {}, sched) // runs on sched (1)
            .then([](){}) // runs on sched (2)
            .wait();

        VERIFY_ARE_EQUAL(sched->get_num_tasks(), 2);
    }

    ev.wait();
}

TEST(scheduler_lifetime_mixed)
{
    pplx::notification_event ev;
    auto t = pplx::create_task([](){}); // use default scheduler
    {
        auto sched = std::make_shared<CheckLifetimeScheduler>(ev);

        t.then([]() {}, sched) // (1)
         .then([](){})         // (2)
         .wait();

        VERIFY_ARE_EQUAL(sched->get_num_tasks(), 2);
    }

    ev.wait();
}

TEST(scheduler_lifetime_nested)
{
    pplx::notification_event ev;
    auto t = pplx::create_task([](){}); // use default scheduler
    {
        auto sched = std::make_shared<CheckLifetimeScheduler>(ev);

        t.then([]() {}, sched) // custom scheduler (1)
         .then([sched]()
            {
                // We are on the default scheduler
                pplx::create_task([](){}, sched); // run on custom scheduler (2)
            }, t.scheduler())
         .wait();

        VERIFY_ARE_EQUAL(sched->get_num_tasks(), 2);
    }

    ev.wait();
}

} // SUITE(pplx_task_options_tests)
}}}   // namespaces
