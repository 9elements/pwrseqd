#include "Logic.hpp"
#include "StateMachine.hpp"

#include <stdlib.h> /* system, NULL, EXIT_FAILURE */
#include <unistd.h>

#include <boost/algorithm/string/predicate.hpp>

#include <filesystem>
#include <iostream>

#include <gtest/gtest.h>

using namespace std::filesystem;
using namespace std::chrono;

TEST(Waveform, GenerateTest1)
{
    struct Config cfg;

    cfg.Logic.push_back((struct ConfigLogic){
        .Name = "all false",
        .AndSignalInputs = {{"a1", false, 0}, {"a2", false, 0}},
        .OrSignalInputs =
            {
                {"o1", false, 0},
            },
        .AndThenOr = false,
        .InvertFirstGate = false,
        .DelayOutputUsec = 0,
        .Out = {"out", false}});
    cfg.Logic.push_back(
        (struct ConfigLogic){.Name = "Input stable",
                             .OrSignalInputs = {{"o1", false, 60}},
                             .AndThenOr = false,
                             .InvertFirstGate = false,
                             .DelayOutputUsec = 0,
                             .Out = {"o1_stable", false}});
    path root = temp_directory_path() / path(std::tmpnam(nullptr));
    create_directories(root);

    {
        boost::asio::io_context io;

        SignalProvider sp(cfg, root.string());
        StateMachine sm(cfg, sp, io);

        Signal* a1 = sp.Find("a1");
        EXPECT_NE(a1, nullptr);
        Signal* a2 = sp.Find("a2");
        EXPECT_NE(a2, nullptr);
        Signal* o1 = sp.Find("o1");
        EXPECT_NE(o1, nullptr);
        Signal* out = sp.Find("out");
        EXPECT_NE(out, nullptr);
        Signal* o1_stable = sp.Find("o1_stable");
        EXPECT_NE(o1_stable, nullptr);
        sm.EvaluateState();

        for (int i = 0; i < 256; i++)
        {
            a1->SetLevel(i & 1);
            a2->SetLevel(i & 4);
            o1->SetLevel(i & 64);
            steady_clock::time_point start = steady_clock::now();

            sm.Poll();
            sm.EvaluateState();
            sp.DumpSignals();

            nanoseconds ns;

            while (ns.count() < 10000)
            {
                ns = steady_clock::now() - start;
                sm.Poll();
            }

            sp.DumpSignals();
        }
    }

    remove_all(root);
}