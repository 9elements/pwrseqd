#include "Config.hpp"
#include "SignalProvider.hpp"
#include "VoltageRegulator.hpp"

#include <unistd.h>

#include <boost/asio/io_service.hpp>

#include <filesystem>
#include <iostream>

#include <gtest/gtest.h>

using namespace std::filesystem;

static void WriteFile(path p, std::string txt)
{
    std::ofstream outfile(p);
    outfile << txt;
    outfile.close();
    for (int i = 0; i < 10; i++)
    {
        std::this_thread::yield();
        usleep(100);
    }
}

static std::string ReadFile(path p)
{
    std::string line;

    try
    {
        std::ifstream infile(p);
        std::getline(infile, line);
        infile.close();
    }
    catch (exception e)
    {}
    for (int i = 0; i < 10; i++)
    {
        std::this_thread::yield();
        usleep(100);
    }
    return line;
}

TEST(Regulator, FindSignals)
{
    boost::asio::io_context io;
    struct Config cfg;
    cfg.Regulators.push_back((struct ConfigRegulator){
        .Name = "abcde",
    });
    SignalProvider sp(cfg);

    path root = path(std::tmpnam(nullptr));
    create_directories(root);
    create_directories(root / path("consumer"));

    WriteFile(root / path("name"), "abcde");
    WriteFile(root / path("state"), "");
    WriteFile(root / path("status"), "");

    WriteFile(root / path("consumer") / path("modalias"),
              "reg-userspace-consumer");

    VoltageRegulator vr(io, &cfg.Regulators[0], sp, root.string());

    EXPECT_NE(sp.Find("abcde_On"), nullptr);
    EXPECT_NE(sp.Find("abcde_PowerGood"), nullptr);
    EXPECT_NE(sp.Find("abcde_Fault"), nullptr);
    EXPECT_NE(sp.Find("abcde_Enabled"), nullptr);

    remove_all(root);
}

TEST(Regulator, EventParsing)
{
    boost::asio::io_context io;
    int result;

    struct Config cfg;
    cfg.Regulators.push_back((struct ConfigRegulator){
        .Name = "abcde",
    });
    SignalProvider sp(cfg);
    path root = path(std::tmpnam(nullptr));
    create_directories(root);
    create_directories(root / path("consumer"));

    WriteFile(root / path("name"), "abcde");
    WriteFile(root / path("state"), "disabled");
    WriteFile(root / path("status"), "off");
    WriteFile(root / path("consumer") / path("modalias"),
              "reg-userspace-consumer");
    WriteFile(root / path("consumer") / path("state"), "disabled");

    VoltageRegulator vr(io, &cfg.Regulators[0], sp, root.string());
    EXPECT_EQ(vr.DecodeRegulatorEvent("0"), 0);
    EXPECT_EQ(vr.DecodeRegulatorEvent("1"), 1);
    EXPECT_EQ(vr.DecodeRegulatorEvent("0x2"), 2);
    EXPECT_EQ(vr.DecodeRegulatorEvent("0x100"), 256);
    EXPECT_EQ(vr.DecodeRegulatorEvent("-1000"), -1000);
    EXPECT_EQ(vr.DecodeRegulatorEvent("0xffff\n"), 0xffff);

    remove_all(root);
}

TEST(Regulator, StatusParsing)
{
    boost::asio::io_context io;
    int result;

    struct Config cfg;
    cfg.Regulators.push_back((struct ConfigRegulator){
        .Name = "abcde",
    });
    SignalProvider sp(cfg);
    path root = path(std::tmpnam(nullptr));
    create_directories(root);
    create_directories(root / path("consumer"));

    WriteFile(root / path("name"), "abcde");
    WriteFile(root / path("state"), "disabled");
    WriteFile(root / path("status"), "off");
    WriteFile(root / path("consumer") / path("modalias"),
              "reg-userspace-consumer");
    WriteFile(root / path("consumer") / path("state"), "disabled");

    VoltageRegulator vr(io, &cfg.Regulators[0], sp, root.string());

    EXPECT_EQ(vr.DecodeStatus(""), ERROR);
    EXPECT_EQ(vr.DecodeStatus("on"), ON);
    EXPECT_EQ(vr.DecodeStatus("on\n"), ON);
    EXPECT_EQ(vr.DecodeStatus("off"), OFF);
    EXPECT_EQ(vr.DecodeStatus("off\n"), OFF);
    EXPECT_EQ(vr.DecodeStatus("error"), ERROR);
    EXPECT_EQ(vr.DecodeStatus("error\n"), ERROR);
    EXPECT_EQ(vr.DecodeStatus("fast"), FAST);
    EXPECT_EQ(vr.DecodeStatus("normal"), NORMAL);
    EXPECT_EQ(vr.DecodeStatus("idle"), IDLE);
    EXPECT_EQ(vr.DecodeStatus("standby"), STANDBY);

    remove_all(root);
}

TEST(Regulator, Inotify)
{
    boost::asio::io_context io;
    struct Config cfg;
    cfg.Regulators.push_back((struct ConfigRegulator){
        .Name = "abcde",
    });
    SignalProvider sp(cfg);

    path root = path(std::tmpnam(nullptr));
    create_directories(root);
    create_directories(root / path("consumer"));

    WriteFile(root / path("name"), "abcde");
    WriteFile(root / path("state"), "disabled");
    WriteFile(root / path("status"), "off");
    WriteFile(root / path("consumer") / path("modalias"),
              "reg-userspace-consumer");
    WriteFile(root / path("consumer") / path("state"), "disabled");

    VoltageRegulator vr(io, &cfg.Regulators[0], sp, root.string());

    Signal* on = sp.Find("abcde_On");
    Signal* pg = sp.Find("abcde_PowerGood");
    Signal* fa = sp.Find("abcde_Fault");
    Signal* en = sp.Find("abcde_Enabled");

    on->SetLevel(true);
    vr.Update();
    vr.Apply();
    EXPECT_EQ(ReadFile(root / path("consumer") / path("state")), "enabled");
    // Emulate sysfs change event
    WriteFile(root / path("state"), "enabled");
    WriteFile(root / path("status"), "on");

    on->SetLevel(false);
    vr.Update();
    vr.Apply();
    EXPECT_EQ(ReadFile(root / path("consumer") / path("state")), "disabled");
    // Emulate sysfs change event
    WriteFile(root / path("state"), "disabled");
    WriteFile(root / path("status"), "off");

    on->SetLevel(true);
    vr.Update();
    vr.Apply();
    // Emulate sysfs change event
    WriteFile(root / path("state"), "enabled");
    WriteFile(root / path("status"), "on");

    EXPECT_EQ(en->GetLevel(), true);
    EXPECT_EQ(pg->GetLevel(), true);
    EXPECT_EQ(fa->GetLevel(), false);

    WriteFile(root / path("status"), "off");

    EXPECT_EQ(en->GetLevel(), false);
    EXPECT_EQ(pg->GetLevel(), false);
    EXPECT_EQ(fa->GetLevel(), false);

    WriteFile(root / path("status"), "error");

    EXPECT_EQ(en->GetLevel(), true);
    EXPECT_EQ(pg->GetLevel(), false);
    EXPECT_EQ(fa->GetLevel(), true);

    remove_all(root);
}
