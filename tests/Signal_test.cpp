#include "Config.hpp"
#include "Signal.hpp"
#include "SignalProvider.hpp"
#include "StateMachine.hpp"

#include <iostream>

#include <gtest/gtest.h>

TEST(Signal, TestInputHasSignal)
{
    boost::asio::io_context io;
    struct Config cfg;
    SignalProvider sp(cfg);

    cfg.Inputs.push_back((struct ConfigInput){
        .Name = "GPIO_A1",
        .GpioChipName = "",
        .SignalName = "in",
        .ActiveLow = false,
        .Description = "",
        .InputType = INPUT_TYPE_NULL,
    });

    StateMachine sm(cfg, sp, io);

    Signal* in = sp.Find("in");
    EXPECT_NE(in, nullptr);
}

TEST(Signal, TestDirty)
{
    boost::asio::io_context io;
    struct Config cfg;
    SignalProvider sp(cfg);

    cfg.Inputs.push_back((struct ConfigInput){
        .Name = "GPIO_A1",
        .GpioChipName = "",
        .SignalName = "in",
        .ActiveLow = false,
        .Description = "",
        .InputType = INPUT_TYPE_NULL,
    });

    StateMachine sm(cfg, sp, io);

    Signal* in = sp.Find("in");
    // All signals start dirty
    EXPECT_EQ(in->Dirty(), true);

    auto vec = sp.GetDirtySignalsAndClearList();
    EXPECT_EQ(in->Dirty(), false);
    EXPECT_EQ(vec->size(), 1);

    in->SetLevel(in->GetLevel() ^ 1);
    EXPECT_EQ(in->Dirty(), true);
    in->ClearDirty();
    EXPECT_EQ(in->Dirty(), false);
    in->SetLevel(in->GetLevel() ^ 1);
    vec = sp.GetDirtySignalsAndClearList();
    EXPECT_EQ(vec->size(), 1);
    EXPECT_EQ(vec->at(0), in);
    sp.ClearDirty();
    EXPECT_EQ(in->Dirty(), false);
    vec = sp.GetDirtySignalsAndClearList();
    EXPECT_EQ(vec->size(), 0);
}

TEST(Signal, TestValidate)
{
    boost::asio::io_context io;
    struct Config cfg;
    SignalProvider sp(cfg);

    cfg.Inputs.push_back((struct ConfigInput){
        .Name = "GPIO_IN",
        .GpioChipName = "",
        .SignalName = "inout",
        .ActiveLow = false,
        .Description = "",
        .InputType = INPUT_TYPE_NULL,
    });
    cfg.Outputs.push_back((struct ConfigOutput){
        .Name = "GPIO_OUT",
        .GpioChipName = "",
        .SignalName = "inout",
        .ActiveLow = false,
        .Description = "",
        .OutputType = OUTPUT_TYPE_NULL,
    });

    StateMachine sm(cfg, sp, io);

    Signal* inout = sp.Find("inout");
    EXPECT_NE(inout, nullptr);

    sm.Validate();
}

TEST(Signal, TestValidateThrow)
{
    boost::asio::io_context io;
    struct Config cfg;
    SignalProvider sp(cfg);

    cfg.Inputs.push_back((struct ConfigInput){
        .Name = "GPIO_IN",
        .GpioChipName = "",
        .SignalName = "inout",
        .ActiveLow = false,
        .Description = "",
        .InputType = INPUT_TYPE_NULL,
    });

    StateMachine sm(cfg, sp, io);

    Signal* inout = sp.Find("inout");
    EXPECT_NE(inout, nullptr);

    try
    {
        sm.Validate();
        FAIL() << "sm.Validate() should throw an runtime_error error\n";
    }
    catch (std::runtime_error& exception)
    {
        EXPECT_EQ(std::string(exception.what()),
                  "no one listens to signal inout");
    }
}

TEST(Signal, TestValidateThrow2)
{
    boost::asio::io_context io;
    struct Config cfg;
    SignalProvider sp(cfg);

    cfg.Outputs.push_back((struct ConfigOutput){
        .Name = "GPIO_OUT",
        .GpioChipName = "",
        .SignalName = "inout",
        .ActiveLow = false,
        .Description = "",
        .OutputType = OUTPUT_TYPE_NULL,
    });

    StateMachine sm(cfg, sp, io);

    Signal* inout = sp.Find("inout");
    EXPECT_NE(inout, nullptr);

    try
    {
        sm.Validate();
        FAIL() << "sm.Validate() should throw an runtime_error error\n";
    }
    catch (std::runtime_error& exception)
    {
        EXPECT_EQ(std::string(exception.what()), "no one drives signal inout");
    }
}

TEST(Signal, TestValidateFloatingWire)
{
    boost::asio::io_context io;
    struct Config cfg;

    cfg.Inputs.push_back((struct ConfigInput){
        .Name = "GPIO_OUT",
        .GpioChipName = "",
        .SignalName = "inout",
        .ActiveLow = false,
        .Description = "",
        .InputType = INPUT_TYPE_NULL,
    });
    cfg.FloatingSignals.push_back("inout");

    SignalProvider sp(cfg);
    StateMachine sm(cfg, sp, io);

    Signal* inout = sp.Find("inout");
    EXPECT_NE(inout, nullptr);

    sm.Validate();
}