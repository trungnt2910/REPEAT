#include <optional>
#include <sstream>

#include <gtest/gtest.h>

#include "RepeatTest.h"
#include "repeat/args.h"
#include "repeat/error.h"

namespace repeat
{
namespace test
{

class Given_Arguments : public RepeatTest
{
protected:
    std::stringstream m_out;
    std::stringstream m_err;
};

TEST_F(Given_Arguments, When_ParseValidArgs_ReturnsParsedPaths)
{
    const char* argv[] = {
        "repeat",
        "input.elf",
        "-o",
        "output.s",
    };
    int argc = sizeof(argv) / sizeof(argv[0]);

    ParsedArgs args = ArgsParser::Parse(argc, argv, m_out, m_err);

    EXPECT_EQ(args.m_inputElf, "input.elf");
    EXPECT_EQ(args.m_outputAssembly, "output.s");
    EXPECT_FALSE(args.m_showHelp);
}

TEST_F(Given_Arguments, When_ParseSwappedArgs_ReturnsParsedPaths)
{
    const char* argv[] = {
        "repeat",
        "-o",
        "output.s",
        "input.elf",
    };
    int argc = sizeof(argv) / sizeof(argv[0]);

    ParsedArgs args = ArgsParser::Parse(argc, argv, m_out, m_err);

    EXPECT_EQ(args.m_inputElf, "input.elf");
    EXPECT_EQ(args.m_outputAssembly, "output.s");
    EXPECT_FALSE(args.m_showHelp);
}

TEST_F(Given_Arguments, When_ParseHelpFlag_SetsShowHelp)
{
    const char* argv[] = {
        "repeat",
        "--help",
    };
    int argc = sizeof(argv) / sizeof(argv[0]);

    ParsedArgs args = ArgsParser::Parse(argc, argv, m_out, m_err);

    EXPECT_TRUE(args.m_showHelp);
}

TEST_F(Given_Arguments, When_ParseHelpFlagShort_SetsShowHelp)
{
    const char* argv[] = {
        "repeat",
        "-h",
    };
    int argc = sizeof(argv) / sizeof(argv[0]);

    ParsedArgs args = ArgsParser::Parse(argc, argv, m_out, m_err);

    EXPECT_TRUE(args.m_showHelp);
}

TEST_F(Given_Arguments, When_ParseArgsEmpty_ThrowsCommandLineException)
{
    int argc = 0;
    char** argv = nullptr;
    std::optional<CommandLineException> thrownException;

    try
    {
        ArgsParser::Parse(argc, argv, m_out, m_err);
    }
    catch (const CommandLineException& e)
    {
        thrownException = e;
    }

    ASSERT_TRUE(thrownException.has_value());
    EXPECT_STREQ(thrownException->what(), "empty arguments list");
    EXPECT_FALSE(thrownException->ShouldShowHelp());
}

TEST_F(Given_Arguments, When_ParseMissingOutputArg_ThrowsCommandLineException)
{
    const char* argv[] = {
        "repeat",
        "input.elf",
        "-o",
    };
    int argc = sizeof(argv) / sizeof(argv[0]);
    std::optional<CommandLineException> thrownException;

    try
    {
        ArgsParser::Parse(argc, argv, m_out, m_err);
    }
    catch (const CommandLineException& e)
    {
        thrownException = e;
    }

    ASSERT_TRUE(thrownException.has_value());
    EXPECT_STREQ(thrownException->what(), "missing argument after '-o'");
    EXPECT_FALSE(thrownException->ShouldShowHelp());
}

TEST_F(Given_Arguments, When_ParseUnknownArgument_ThrowsCommandLineException)
{
    const char* argv[] = {
        "repeat",
        "input.elf",
        "-o",
        "output.s",
        "-x",
    };
    int argc = sizeof(argv) / sizeof(argv[0]);
    std::optional<CommandLineException> thrownException;

    try
    {
        ArgsParser::Parse(argc, argv, m_out, m_err);
    }
    catch (const CommandLineException& e)
    {
        thrownException = e;
    }

    ASSERT_TRUE(thrownException.has_value());
    EXPECT_STREQ(thrownException->what(), "unknown argument: '-x'");
    EXPECT_TRUE(thrownException->ShouldShowHelp());
}

TEST_F(Given_Arguments, When_ParseMultipleInputFiles_ThrowsCommandLineException)
{
    const char* argv[] = {
        "repeat",
        "input1.elf",
        "input2.elf",
        "-o",
        "output.s",
    };
    int argc = sizeof(argv) / sizeof(argv[0]);
    std::optional<CommandLineException> thrownException;

    try
    {
        ArgsParser::Parse(argc, argv, m_out, m_err);
    }
    catch (const CommandLineException& e)
    {
        thrownException = e;
    }

    ASSERT_TRUE(thrownException.has_value());
    EXPECT_STREQ(thrownException->what(), "more than one input file specified");
    EXPECT_TRUE(thrownException->ShouldShowHelp());
}

TEST_F(Given_Arguments, When_ParseMissingInputFile_ThrowsCommandLineException)
{
    const char* argv[] = {
        "repeat",
        "-o",
        "output.s",
    };
    int argc = sizeof(argv) / sizeof(argv[0]);
    std::optional<CommandLineException> thrownException;

    try
    {
        ArgsParser::Parse(argc, argv, m_out, m_err);
    }
    catch (const CommandLineException& e)
    {
        thrownException = e;
    }

    ASSERT_TRUE(thrownException.has_value());
    EXPECT_STREQ(thrownException->what(), "no input file specified");
    EXPECT_TRUE(thrownException->ShouldShowHelp());
}

TEST_F(Given_Arguments, When_ParseMissingOutputFile_ThrowsCommandLineException)
{
    const char* argv[] = {
        "repeat",
        "input.elf",
    };
    int argc = sizeof(argv) / sizeof(argv[0]);
    std::optional<CommandLineException> thrownException;

    try
    {
        ArgsParser::Parse(argc, argv, m_out, m_err);
    }
    catch (const CommandLineException& e)
    {
        thrownException = e;
    }

    ASSERT_TRUE(thrownException.has_value());
    EXPECT_STREQ(thrownException->what(), "no output file specified");
    EXPECT_TRUE(thrownException->ShouldShowHelp());
}
TEST_F(Given_Arguments, When_ParseColumnInfo_SetsColumnInfo)
{
    const char* argv[] = {
        "repeat",
        "input.elf",
        "-o",
        "output.s",
        "-gcolumn-info",
    };
    int argc = sizeof(argv) / sizeof(argv[0]);

    ParsedArgs args = ArgsParser::Parse(argc, argv, m_out, m_err);

    EXPECT_TRUE(args.m_columnInfo);
}

TEST_F(Given_Arguments, When_ParseNoColumnInfo_ClearsColumnInfo)
{
    const char* argv[] = {
        "repeat",
        "input.elf",
        "-o",
        "output.s",
        "-gno-column-info",
    };
    int argc = sizeof(argv) / sizeof(argv[0]);

    ParsedArgs args = ArgsParser::Parse(argc, argv, m_out, m_err);

    EXPECT_FALSE(args.m_columnInfo);
}

TEST_F(Given_Arguments, When_ParseMultipleColumnInfoFlags_LastFlagNoColumnInfoWins)
{
    const char* argv[] = {
        "repeat",
        "input.elf",
        "-o",
        "output.s",
        "-gcolumn-info",
        "-gno-column-info",
    };
    int argc = sizeof(argv) / sizeof(argv[0]);

    ParsedArgs args = ArgsParser::Parse(argc, argv, m_out, m_err);

    EXPECT_FALSE(args.m_columnInfo);
}

TEST_F(Given_Arguments, When_ParseMultipleColumnInfoFlags_LastFlagColumnInfoWins)
{
    const char* argv[] = {
        "repeat",
        "input.elf",
        "-o",
        "output.s",
        "-gno-column-info",
        "-gcolumn-info",
    };
    int argc = sizeof(argv) / sizeof(argv[0]);

    ParsedArgs args = ArgsParser::Parse(argc, argv, m_out, m_err);

    EXPECT_TRUE(args.m_columnInfo);
}

TEST_F(Given_Arguments, When_PrintHelp_GeneratesFormattedHelpOutput)
{
    ArgsParser::PrintHelp("repeat", m_out);

    ExpectOutputMatchesGolden(m_out.str(), "data/output/cli/help_stdout.txt");
}

} // namespace test
} // namespace repeat
