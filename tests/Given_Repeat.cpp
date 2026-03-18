#include <filesystem>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "RepeatTest.h"
#include "repeat/repeat.h"

namespace repeat
{
namespace test
{

class Given_Repeat : public RepeatTest
{
protected:
    std::stringstream m_in;
    std::stringstream m_out;
    std::stringstream m_err;
};

TEST_F(Given_Repeat, When_RunHelpOption_WritesHelpToOutAndReturnsSuccess)
{
    Repeat app(m_out, m_err);
    const char* argv[] = {
        "repeat",
        "--help",
    };
    int argc = sizeof(argv) / sizeof(argv[0]);

    int result = app.Run(argc, argv);

    EXPECT_EQ(result, 0);
    ExpectOutputMatchesGolden(m_out.str(), "data/output/cli/help_stdout.txt");
    EXPECT_TRUE(m_err.str().empty());
}

TEST_F(Given_Repeat, When_RunMissingArgs_WritesErrorToErrAndHelpToOutAndReturnsFailure)
{
    Repeat app(m_out, m_err);
    const char* argv[] = {
        "repeat",
    };
    int argc = sizeof(argv) / sizeof(argv[0]);

    int result = app.Run(argc, argv);

    EXPECT_EQ(result, 1);
    ExpectOutputMatchesGolden(m_err.str(), "data/output/cli/missing_input_stderr.txt");
    ExpectOutputMatchesGolden(m_out.str(), "data/output/cli/help_stdout.txt");
}

TEST_F(Given_Repeat, When_RunUnknownOption_WritesErrorToErrAndHelpToOutAndReturnsFailure)
{
    Repeat app(m_out, m_err);
    const char* argv[] = {
        "repeat",
        "input.elf",
        "-o",
        "output.s",
        "-x",
    };
    int argc = sizeof(argv) / sizeof(argv[0]);

    int result = app.Run(argc, argv);

    EXPECT_EQ(result, 1);
    ExpectOutputMatchesGolden(m_err.str(), "data/output/cli/unknown_option_stderr.txt");
    ExpectOutputMatchesGolden(m_out.str(), "data/output/cli/help_stdout.txt");
}

TEST_F(Given_Repeat, When_RunInvalidElf_WritesErrorToErrAndReturnsFailure)
{
    Repeat app(m_out, m_err);
    const char* argv[] = {
        "repeat",
        "nonexistent.elf",
        "-o",
        "output.s",
    };
    int argc = sizeof(argv) / sizeof(argv[0]);

    int result = app.Run(argc, argv);

    EXPECT_EQ(result, 1);
    ExpectOutputMatchesGolden(m_err.str(), "data/output/cli/invalid_elf_stderr.txt");
    EXPECT_TRUE(m_out.str().empty());
}

TEST_F(Given_Repeat, When_RunValidTranslation_ReturnsSuccess)
{
    Repeat app(m_out, m_err);
    std::string tempPathStr = "temp_output.s";
    TemporaryFile tempFile(tempPathStr);
    const char* argv[] = {
        "repeat",
        "data/symbols_elf.so",
        "-o",
        tempPathStr.c_str(),
    };
    int argc = sizeof(argv) / sizeof(argv[0]);

    int result = app.Run(argc, argv);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(m_out.str().empty());
    EXPECT_TRUE(m_err.str().empty());
    EXPECT_TRUE(std::filesystem::exists(tempFile.GetPath()));
    std::string fileContent = GetTestTextContent(tempPathStr);
    EXPECT_NE(fileContent.find(".globl global_func"), std::string::npos);
    EXPECT_NE(fileContent.find("global_func ="), std::string::npos);
    EXPECT_NE(fileContent.find(".globl global_var"), std::string::npos);
    EXPECT_NE(fileContent.find("global_var ="), std::string::npos);
}

TEST_F(Given_Repeat, When_RunValidTranslationSwapped_ReturnsSuccess)
{
    Repeat app(m_out, m_err);
    std::string tempPathStr = "temp_output.s";
    TemporaryFile tempFile(tempPathStr);
    const char* argv[] = {
        "repeat",
        "-o",
        tempPathStr.c_str(),
        "data/symbols_elf.so",
    };
    int argc = sizeof(argv) / sizeof(argv[0]);

    int result = app.Run(argc, argv);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(m_out.str().empty());
    EXPECT_TRUE(m_err.str().empty());
    EXPECT_TRUE(std::filesystem::exists(tempFile.GetPath()));
    std::string fileContent = GetTestTextContent(tempPathStr);
    EXPECT_NE(fileContent.find(".globl global_func"), std::string::npos);
    EXPECT_NE(fileContent.find("global_func ="), std::string::npos);
    EXPECT_NE(fileContent.find(".globl global_var"), std::string::npos);
    EXPECT_NE(fileContent.find("global_var ="), std::string::npos);
}

TEST_F(Given_Repeat, When_TranslatingCorruptEhFrame_FallbackToDebugFrameSucceeds)
{
    Repeat app(m_out, m_err);
    std::string tempPathStr = "temp_corrupt_output.s";
    TemporaryFile tempFile(tempPathStr);
    const char* argv[] = {
        "repeat",
        "data/corrupt_eh_frame.so",
        "-o",
        tempPathStr.c_str(),
    };
    int argc = sizeof(argv) / sizeof(argv[0]);

    int result = app.Run(argc, argv);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(m_err.str().empty());
    EXPECT_TRUE(std::filesystem::exists(tempFile.GetPath()));
}

} // namespace test
} // namespace repeat
