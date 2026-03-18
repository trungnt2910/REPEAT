#include <filesystem>
#include <string>

#include <gtest/gtest.h>
#include <llvm/Support/raw_ostream.h>

#include "RepeatTest.h"
#include "repeat/translator.h"

namespace repeat
{
namespace test
{

class Given_Symbols : public RepeatTest
{
};

TEST_F(Given_Symbols, When_TranslatingSymbols_ExportsOnlyGlobalSymbols)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/symbols_elf.so")))
        << "Test file not found: " << GetTestDataPath("data/symbols_elf.so");
    Translator translator(GetTestDataPath("data/symbols_elf.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load symbols ELF: " << ec.message();
    std::string assembly;
    llvm::raw_string_ostream os(assembly);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(assembly.find(".globl global_var"), std::string::npos);
    EXPECT_NE(assembly.find(".globl global_func"), std::string::npos);
    EXPECT_EQ(assembly.find(".globl local_var"), std::string::npos);
    EXPECT_EQ(assembly.find(".globl local_func"), std::string::npos);
}

} // namespace test
} // namespace repeat
