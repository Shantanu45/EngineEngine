#include "gtest/gtest.h"
#include "util/logger.h"
#include <memory>

using namespace Util;

class SpdlogTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Optional: setup logger for each test
        // auto logger = spdlog::stdout_color_mt("test_logger");
    }

    void TearDown() override
    {
        spdlog::drop_all(); // safely remove all loggers after each test
    }
};

namespace EETests
{

    TEST(Logger, StdLogger)
    {
        std::unique_ptr logger = std::make_unique<StdLogger>();
        testing::internal::CaptureStdout();
        logger->logf_info("Hello %d \n", 8);
        std::string output = testing::internal::GetCapturedStdout();
        EXPECT_EQ(output, "Hello 8 \n");
    }

    TEST_F(SpdlogTest, StdSpdLogger)
    {
        std::unique_ptr logger = std::make_unique<StdSpdLogger>();
        logger->logf_info("Hello %d \n", 8);
    }

    TEST_F(SpdlogTest, RotatedFileLogger)
    {
        for (size_t i = 0; i <= 10; i++)
        {
            std::unique_ptr logger = std::make_unique<RotatedFileLogger>("./TestLogs/log.txt", 3);
            logger->logf_info("Hello %d \n", i);
        }
    }

}