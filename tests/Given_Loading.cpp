#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include "RepeatTest.h"
#include "repeat/translator.h"

namespace repeat
{
namespace test
{

class Given_Loading : public RepeatTest
{
};

TEST_F(Given_Loading, When_LoadingFileNotFound_ReturnsNoSuchFileOrDirectory)
{
    Translator translator("nonexistent_file_for_testing");

    std::error_code ec = translator.Load();

    EXPECT_EQ(ec, std::make_error_code(std::errc::no_such_file_or_directory));
}

TEST_F(Given_Loading, When_LoadingInvalidELF_ReturnsInvalidArgument)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/invalid_elf.txt")))
        << "Test file not found: " << GetTestDataPath("data/invalid_elf.txt");
    Translator translator(GetTestDataPath("data/invalid_elf.txt"));

    std::error_code ec = translator.Load();

    EXPECT_EQ(ec, std::make_error_code(std::errc::invalid_argument));
}

TEST_F(Given_Loading, When_LoadingValidX86_64ELF_Succeeds)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/valid_elf.so")))
        << "File not found: " << GetTestDataPath("data/valid_elf.so");
    Translator translator(GetTestDataPath("data/valid_elf.so"));

    std::error_code ec = translator.Load();

    EXPECT_FALSE(ec) << "Failed to load valid x86_64 ELF: " << ec.message();
}

TEST_F(Given_Loading, When_LoadingValidI386ELF_Succeeds)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/valid_elf_i386.so")))
        << "Test file not found: " << GetTestDataPath("data/valid_elf_i386.so");
    Translator translator(GetTestDataPath("data/valid_elf_i386.so"));

    std::error_code ec = translator.Load();

    EXPECT_FALSE(ec) << "Failed to load valid i386 ELF: " << ec.message();
}

TEST_F(Given_Loading, When_LoadingValidAArch64ELF_Succeeds)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/valid_elf_aarch64.so")))
        << "Test file not found: " << GetTestDataPath("data/valid_elf_aarch64.so");
    Translator translator(GetTestDataPath("data/valid_elf_aarch64.so"));

    std::error_code ec = translator.Load();

    EXPECT_FALSE(ec) << "Failed to load valid AArch64 ELF: " << ec.message();
}

TEST_F(Given_Loading, When_LoadingValidARMELF_Succeeds)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/valid_elf_arm.so")))
        << "Test file not found: " << GetTestDataPath("data/valid_elf_arm.so");
    Translator translator(GetTestDataPath("data/valid_elf_arm.so"));

    std::error_code ec = translator.Load();

    EXPECT_FALSE(ec) << "Failed to load valid ARM ELF: " << ec.message();
}

} // namespace test
} // namespace repeat
