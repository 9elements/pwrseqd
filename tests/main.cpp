#include <iostream>

#include <gtest/gtest.h>

int _loglevel;

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    _loglevel = 2;
    return RUN_ALL_TESTS();
}