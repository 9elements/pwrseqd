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

    EXPECT_EQ(vr.control.DecodeStatus(""), ERROR);
    EXPECT_EQ(vr.control.DecodeStatus("on"), ON);
    EXPECT_EQ(vr.control.DecodeStatus("on\n"), ON);
    EXPECT_EQ(vr.control.DecodeStatus("off"), OFF);
    EXPECT_EQ(vr.control.DecodeStatus("off\n"), OFF);
    EXPECT_EQ(vr.control.DecodeStatus("error"), ERROR);
    EXPECT_EQ(vr.control.DecodeStatus("error\n"), ERROR);
    EXPECT_EQ(vr.control.DecodeStatus("fast"), FAST);
    EXPECT_EQ(vr.control.DecodeStatus("normal"), NORMAL);
    EXPECT_EQ(vr.control.DecodeStatus("idle"), IDLE);
    EXPECT_EQ(vr.control.DecodeStatus("standby"), STANDBY);

    remove_all(root);
}