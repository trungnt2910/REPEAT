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

class Given_Relocations : public RepeatTest
{
};

TEST_F(Given_Relocations, When_TranslatingX86_64Relocations_GeneratesSegmentOffsets)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/relocs_elf.so")))
        << "Test file not found: " << GetTestDataPath("data/relocs_elf.so");
    Translator translator(GetTestDataPath("data/relocs_elf.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load relocs ELF: " << ec.message();
    std::string assembly;
    llvm::raw_string_ostream os(assembly);

    translator.Translate(os);
    os.flush();

    // x86_64 is 64-bit and uses .quad for dynamic pointers pointing to local_var (offset 0)
    EXPECT_NE(assembly.find(".quad __elf_seg_D + 0"), std::string::npos);
}

TEST_F(Given_Relocations, When_TranslatingX86_64RelativeRelocation_PointsToCorrectSegmentOffset)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/relocs_elf.so")))
        << "Test file not found: " << GetTestDataPath("data/relocs_elf.so");
    Translator translator(GetTestDataPath("data/relocs_elf.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load relocs ELF: " << ec.message();
    std::string assembly;
    llvm::raw_string_ostream os(assembly);

    translator.Translate(os);
    os.flush();

    // local_ptr is defined at offset 8 in data segment (seg_D) and points to local_var (offset 0)
    std::string expected_pattern = ".org __elf_seg_D + 8\n.quad __elf_seg_D + 0";
    EXPECT_NE(assembly.find(expected_pattern), std::string::npos)
        << "Missing correct relative relocation for local_ptr";
}

TEST_F(Given_Relocations, When_TranslatingX86_64SymbolicRelocation_PointsToSegmentOffset)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/relocs_elf.so")))
        << "Test file not found: " << GetTestDataPath("data/relocs_elf.so");
    Translator translator(GetTestDataPath("data/relocs_elf.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load relocs ELF: " << ec.message();
    std::string assembly;
    llvm::raw_string_ostream os(assembly);

    translator.Translate(os);
    os.flush();

    // global_ptr points to global_var (offset 16 in seg_D)
    EXPECT_NE(assembly.find(".quad __elf_seg_D + 16"), std::string::npos)
        << "Missing symbolic relocation pointing to global_var";
}

TEST_F(Given_Relocations, When_TranslatingX86_64ExternalRelocation_PointsToImportSymbol)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/relocs_elf.so")))
        << "Test file not found: " << GetTestDataPath("data/relocs_elf.so");
    Translator translator(GetTestDataPath("data/relocs_elf.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load relocs ELF: " << ec.message();
    std::string assembly;
    llvm::raw_string_ostream os(assembly);

    translator.Translate(os);
    os.flush();

    // The external call to external_func generates a relocation pointing to the import symbol
    // external_func
    EXPECT_NE(assembly.find(".quad external_func"), std::string::npos)
        << "Missing external call import relocation";
}

TEST_F(Given_Relocations, When_TranslatingI386Relocations_GeneratesSegmentOffsets)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/relocs_elf_i386.so")))
        << "Test file not found: " << GetTestDataPath("data/relocs_elf_i386.so");
    Translator translator(GetTestDataPath("data/relocs_elf_i386.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load 32-bit relocs ELF: " << ec.message();
    std::string assembly;
    llvm::raw_string_ostream os(assembly);

    translator.Translate(os);
    os.flush();

    // i386 is 32-bit and uses .long for dynamic pointers pointing to local_var (offset 0)
    EXPECT_NE(assembly.find(".long __elf_seg_D + 0"), std::string::npos);
}

TEST_F(Given_Relocations, When_TranslatingAArch64Relocations_GeneratesSegmentOffsets)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/relocs_elf_aarch64.so")))
        << "Test file not found: " << GetTestDataPath("data/relocs_elf_aarch64.so");
    Translator translator(GetTestDataPath("data/relocs_elf_aarch64.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load AArch64 relocs ELF: " << ec.message();
    std::string assembly;
    llvm::raw_string_ostream os(assembly);

    translator.Translate(os);
    os.flush();

    // AArch64 is 64-bit and uses .quad for dynamic pointers pointing to local_var (offset 0)
    EXPECT_NE(assembly.find(".quad __elf_seg_D + 0"), std::string::npos);
}

TEST_F(Given_Relocations, When_TranslatingARMRelocations_GeneratesSegmentOffsets)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/relocs_elf_arm.so")))
        << "Test file not found: " << GetTestDataPath("data/relocs_elf_arm.so");
    Translator translator(GetTestDataPath("data/relocs_elf_arm.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load ARM relocs ELF: " << ec.message();
    std::string assembly;
    llvm::raw_string_ostream os(assembly);

    translator.Translate(os);
    os.flush();

    // ARM is 32-bit and uses .long for dynamic pointers pointing to local_var (offset 0)
    EXPECT_NE(assembly.find(".long __elf_seg_D + 0"), std::string::npos);
}

} // namespace test
} // namespace repeat
