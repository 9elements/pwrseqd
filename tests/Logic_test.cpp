#include "Logic.hpp"
#include "StateMachine.hpp"

#include <unistd.h>

#include <boost/thread/thread.hpp>

#include <iostream>

#include <gtest/gtest.h>

using namespace std;
using namespace std::chrono;

struct signalstate
{
    string name;
    bool value;
};

struct testcase
{
    struct ConfigLogic cfg;
    bool expectedResult;
    struct signalstate inputStates[4];
} testcases[] = {{{.Name = "all false",
                   .AndSignalInputs = {{"a1", false, 0}, {"a2", false, 0}},
                   .OrSignalInputs =
                       {
                           {"o1", false, 0},
                       },
                   .AndThenOr = false,
                   .InvertFirstGate = false,
                   .DelayOutputUsec = 0,
                   .Out = {"out", false}},
                  false,
                  {
                      {"a1", false},
                      {"a2", false},
                      {"o1", false},
                      {},
                  }},
                 {{.Name = "all false, invert first gate",
                   .AndSignalInputs = {{"a1", false, 0}, {"a2", false, 0}},
                   .OrSignalInputs =
                       {
                           {"o1", false, 0},
                       },
                   .AndThenOr = false,
                   .InvertFirstGate = true,
                   .DelayOutputUsec = 0,
                   .Out = {"out", false}},
                  false,
                  {
                      {"a1", false},
                      {"a2", false},
                      {"o1", false},
                      {},
                  }},
                 {{.Name = "all false, invert and inputs",
                   .AndSignalInputs = {{"a1", true, 0}, {"a2", true, 0}},
                   .OrSignalInputs =
                       {
                           {"o1", false, 0},
                       },
                   .AndThenOr = false,
                   .InvertFirstGate = true,
                   .DelayOutputUsec = 0,
                   .Out = {"out", false}},
                  true,
                  {
                      {"a1", false},
                      {"a2", false},
                      {"o1", false},
                      {},
                  }},
                 {{.Name = "or input true",
                   .AndSignalInputs = {{"a1", false, 0}, {"a2", false, 0}},
                   .OrSignalInputs =
                       {
                           {"o1", false, 0},
                       },
                   .AndThenOr = false,
                   .InvertFirstGate = false,
                   .DelayOutputUsec = 0,
                   .Out = {"out", false}},
                  false,
                  {
                      {"a1", false},
                      {"a2", false},
                      {"o1", true},
                      {},
                  }},
                 {{.Name = "or input true, output active low",
                   .AndSignalInputs = {{"a1", false, 0}, {"a2", false, 0}},
                   .OrSignalInputs =
                       {
                           {"o1", false, 0},
                       },
                   .AndThenOr = false,
                   .InvertFirstGate = false,
                   .DelayOutputUsec = 0,
                   .Out = {"out", true}},
                  true,
                  {
                      {"a1", false},
                      {"a2", false},
                      {"o1", true},
                      {},
                  }

                 },
                 {{.Name = "all false, output active low",
                   .AndSignalInputs = {{"a1", false, 0}, {"a2", false, 0}},
                   .OrSignalInputs =
                       {
                           {"o1", false, 0},
                       },
                   .AndThenOr = false,
                   .InvertFirstGate = false,
                   .DelayOutputUsec = 0,
                   .Out = {"out", true}},
                  true,
                  {
                      {"a1", false},
                      {"a2", false},
                      {"o1", false},
                      {},
                  }},
                 {{.Name = "and true and input of or",
                   .AndSignalInputs = {{"a1", false, 0}, {"a2", false, 0}},
                   .OrSignalInputs =
                       {
                           {"o1", false, 0},
                       },
                   .AndThenOr = true,
                   .InvertFirstGate = false,
                   .DelayOutputUsec = 0,
                   .Out = {"out", false}},
                  true,
                  {
                      {"a1", true},
                      {"a2", true},
                      {"o1", false},
                      {},
                  }},
                 {{.Name = "and input of or, or true",
                   .AndSignalInputs = {{"a1", false, 0}, {"a2", false, 0}},
                   .OrSignalInputs =
                       {
                           {"o1", false, 0},
                       },
                   .AndThenOr = true,
                   .InvertFirstGate = false,
                   .DelayOutputUsec = 0,
                   .Out = {"out", false}},
                  true,
                  {
                      {"a1", false},
                      {"a2", false},
                      {"o1", true},
                      {},
                  }},
                 {{.Name = "no or true",
                   .AndSignalInputs = {{"a1", false, 0}, {"a2", false, 0}},
                   .AndThenOr = false,
                   .InvertFirstGate = false,
                   .DelayOutputUsec = 0,
                   .Out = {"out", false}},
                  true,
                  {
                      {"a1", true},
                      {"a2", true},
                      {},
                  }},
                 {{.Name = "no or false",
                   .AndSignalInputs = {{"a1", false, 0}, {"a2", false, 0}},
                   .AndThenOr = false,
                   .InvertFirstGate = false,
                   .DelayOutputUsec = 0,
                   .Out = {"out", false}},
                  false,
                  {
                      {"a1", false},
                      {"a2", false},
                      {},
                  }},
                 {{.Name = "no and true",
                   .OrSignalInputs =
                       {
                           {"o1", false, 0},
                       },
                   .InvertFirstGate = false,
                   .DelayOutputUsec = 0,
                   .Out = {"out", false}},
                  true,
                  {
                      {"o1", true},
                      {},
                  }},
                 {{.Name = "no and false",
                   .OrSignalInputs =
                       {
                           {"o1", false, 0},
                       },
                   .InvertFirstGate = false,
                   .DelayOutputUsec = 0,
                   .Out = {"out", false}},
                  false,
                  {
                      {"o1", false},
                      {},
                  }},
                 {}};

TEST(Logic, LUT)
{
    boost::asio::io_context io;
    struct testcase* tc;
    struct Config cfg;
    SignalProvider sp(cfg);

    for (int i = 0; testcases[i].cfg.Name != ""; i++)
    {
        tc = &testcases[i];

        Logic* l = new Logic(io, sp, &tc->cfg);
        for (int states = 0; tc->inputStates[states].name != ""; states++)
        {
            Signal* s = sp.Find(tc->inputStates[states].name);
            EXPECT_NE(s, nullptr);
            std::cout << "set input " << tc->inputStates[states].name << " to "
                      << tc->inputStates[states].value << std::endl;
            s->SetLevel(tc->inputStates[states].value);
        }
        l->Update();
        Signal* out = sp.Find(tc->cfg.Out.SignalName);
        EXPECT_NE(out, nullptr);
        if (out->GetLevel() != tc->expectedResult)
        {
            FAIL() << "out->GetLevel() [" << out->GetLevel()
                   << "] != tc->expectedResult [" << tc->expectedResult
                   << "], in test " << testcases[i].cfg.Name;
        }
        delete l;
    }
}

TEST(Logic, TestInputStableNoTimer)
{
    boost::asio::io_context io;
    struct ConfigLogic cfg = {
        .Name = "all false",
        .AndSignalInputs = {{"a1", false, 1000}, {"a2", false, 0}},
        .OrSignalInputs =
            {
                {"o1", false, 0},
            },
        .AndThenOr = false,
        .InvertFirstGate = false,
        .DelayOutputUsec = 0,
        .Out = {"out", false}};
    struct Config empty;
    SignalProvider sp(empty);
    Logic l(io, sp, &cfg);

    Signal* a1 = sp.Find("a1");
    EXPECT_NE(a1, nullptr);
    Signal* a2 = sp.Find("a2");
    EXPECT_NE(a2, nullptr);
    a2->SetLevel(true);
    Signal* o1 = sp.Find("o1");
    EXPECT_NE(o1, nullptr);
    o1->SetLevel(true);
    Signal* out = sp.Find("out");
    EXPECT_NE(out, nullptr);

    steady_clock::time_point start = steady_clock::now();

    for (int i = 0; i < 1000; i++)
    {
        nanoseconds ns;

        a1->SetLevel(a1->GetLevel() ^ 1);
        while (ns.count() < 1000000)
        {
            ns = steady_clock::now() - start;
        }
        start = steady_clock::now();

        l.Update();
        EXPECT_EQ(out->GetLevel(), false);
    }
    a1->SetLevel(true);
    usleep(2000);
    l.Update();
    EXPECT_EQ(out->GetLevel(), true);
    a1->SetLevel(false);
    usleep(2000);
    l.Update();
    EXPECT_EQ(out->GetLevel(), false);
}

TEST(Logic, TestInputStableWithTimer)
{
    boost::asio::io_context io;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
        work_guard(io.get_executor());

    struct ConfigLogic cfg = {
        .Name = "all false",
        .AndSignalInputs = {{"a1", false, 10000}, {"a2", false, 0}},
        .OrSignalInputs =
            {
                {"o1", false, 0},
            },
        .AndThenOr = false,
        .InvertFirstGate = false,
        .DelayOutputUsec = 0,
        .Out = {"out", false}};
    struct Config empty;
    SignalProvider sp(empty);
    Logic l(io, sp, &cfg);

    Signal* a1 = sp.Find("a1");
    EXPECT_NE(a1, nullptr);
    Signal* a2 = sp.Find("a2");
    EXPECT_NE(a2, nullptr);
    a2->SetLevel(true);
    Signal* o1 = sp.Find("o1");
    EXPECT_NE(o1, nullptr);
    o1->SetLevel(true);
    Signal* out = sp.Find("out");
    EXPECT_NE(out, nullptr);

    steady_clock::time_point start = steady_clock::now();

    for (int i = 0; i < 1000; i++)
    {
        nanoseconds ns;

        a1->SetLevel(a1->GetLevel() ^ 1);
        a1->UpdateReceivers();
        while (ns.count() < 1000000)
        {
            ns = steady_clock::now() - start;
        }
        start = steady_clock::now();
        io.poll();

        EXPECT_EQ(out->GetLevel(), false);
    }
    a1->SetLevel(true);
    for (int i = 0; i < 2000; i++)
    {
        usleep(1);
        io.poll();
    }
    EXPECT_EQ(out->GetLevel(), true);
    work_guard.reset();
}

TEST(Logic, TestOutputDelay)
{
    boost::asio::io_context io;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
        work_guard(io.get_executor());

    struct ConfigLogic cfg = {
        .Name = "all false",
        .AndSignalInputs = {{"a1", false, 0}, {"a2", false, 0}},
        .OrSignalInputs =
            {
                {"o1", false, 0},
            },
        .AndThenOr = false,
        .InvertFirstGate = false,
        .DelayOutputUsec = 1000,
        .Out = {"out", false}};
    struct Config empty;
    SignalProvider sp(empty);
    Logic l(io, sp, &cfg);

    Signal* a1 = sp.Find("a1");
    EXPECT_NE(a1, nullptr);
    Signal* a2 = sp.Find("a2");
    EXPECT_NE(a2, nullptr);
    a2->SetLevel(true);
    Signal* o1 = sp.Find("o1");
    EXPECT_NE(o1, nullptr);
    o1->SetLevel(true);
    Signal* out = sp.Find("out");
    EXPECT_NE(out, nullptr);

    l.Update();

    a1->SetLevel(true);
    EXPECT_EQ(out->GetLevel(), false);
    steady_clock::time_point teststart = steady_clock::now();

    for (int i = 0; i < 10000; i++)
    {
        nanoseconds ns;
        if (out->GetLevel())
        {
            ns = steady_clock::now() - teststart;
            if (ns.count() < 900000)
            {
                FAIL() << "Delay is too small:" << i;
            }
            else if (ns.count() > 3000000)
            {
                FAIL() << "Delay is too big:" << i;
            }
            break;
        }
        else if (i == 9999)
        {
            FAIL() << "Level did not change";
            break;
        }
        usleep(10);
        l.Update();
        io.poll();
    }

    a1->SetLevel(false);
    EXPECT_EQ(out->GetLevel(), true);
    l.Update();

    // Expect the level to not change when io_service isn't used
    for (int i = 0; i < 1000; i++)
    {
        if (!out->GetLevel())
        {
            FAIL() << "Level changed without timer being used";
            break;
        }
        usleep(10);
        l.Update();
    }

    io.poll();

    // Now test a pulse that is smaller than output delay
    a1->SetLevel(true);
    EXPECT_EQ(out->GetLevel(), false);
    teststart = steady_clock::now();

    bool expect_low = false;
    bool expect_high = true;
    for (int i = 0; i < 10000; i++)
    {
        nanoseconds ns = steady_clock::now() - teststart;
        if (ns.count() > 500000)
        {
            a1->SetLevel(false);
        }

        if (expect_high && out->GetLevel())
        {
            if (ns.count() < 700000)
            {
                FAIL() << "Delay is too small: " << ns.count();
                break;
            }
            else if (ns.count() > 1300000)
            {
                FAIL() << "Delay is too big: " << ns.count();
                break;
            }
            expect_low = true;
            expect_high = false;
        }
        else if (expect_low && !out->GetLevel())
        {
            if (ns.count() < 1300000)
            {
                FAIL() << "Delay is too small: " << ns.count();
                break;
            }
            else if (ns.count() > 1800000)
            {
                FAIL() << "Delay is too big: " << ns.count();
                break;
            }
            break;
        }
        else if (i == 9999)
        {
            FAIL() << "Level did not change";
            break;
        }
        usleep(10);
        l.Update();
        io.poll();
    }
    work_guard.reset();
}