#include <filesystem>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>

#include <gtest/gtest.h>
#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/Object/ELFObjectFile.h>

#include "RepeatTest.h"
#include "repeat/translator.h"

namespace repeat
{
namespace test
{

static std::string ToByteSequence(const std::string& str, bool null_terminate = true)
{
    std::string result;
    for (char c : str)
    {
        result += "  .byte " + std::to_string((int)(uint8_t)c) + "\n";
    }
    if (null_terminate)
    {
        result += "  .byte 0\n";
    }
    return result;
}

static ::testing::AssertionResult VerifyLocDirectives(const std::string& asm_output,
                                                      size_t count_limit,
                                                      bool expected_has_column,
                                                      bool exact_count = true)
{
    std::regex re(R"(\.cv_loc\s+(\d+)\s+(\d+)\s+(\d+)(?:\s+(\d+))?)");
    auto begin = std::sregex_iterator(asm_output.begin(), asm_output.end(), re);
    auto end = std::sregex_iterator();

    size_t count = 0;
    for (auto it = begin; it != end; ++it)
    {
        count++;
        std::smatch match = *it;
        if (match[4].matched != expected_has_column)
        {
            return ::testing::AssertionFailure()
                   << "Directive " << match.str() << " has column=" << match[4].matched
                   << ", expected=" << expected_has_column;
        }
    }

    if (exact_count)
    {
        if (count != count_limit)
        {
            return ::testing::AssertionFailure()
                   << "Expected exactly " << count_limit << " .cv_loc directives, got " << count;
        }
    }
    else
    {
        if (count < count_limit)
        {
            return ::testing::AssertionFailure()
                   << "Expected at least " << count_limit << " .cv_loc directives, got " << count;
        }
    }

    return ::testing::AssertionSuccess();
}

class Given_DebugInfo : public RepeatTest
{
};

TEST_F(Given_DebugInfo, When_LoadingValidELFWithDebugInfo_DwarfContextHasCompileUnits)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/valid_elf.so")))
        << "File not found: " << GetTestDataPath("data/valid_elf.so");
    Translator translator(GetTestDataPath("data/valid_elf.so"));

    std::error_code ec = translator.Load();

    ASSERT_FALSE(ec) << "Failed to load valid ELF: " << ec.message();
    auto dwarf = translator.GetDwarfContext();
    ASSERT_NE(dwarf, nullptr) << "Failed to retrieve DWARF Context";
    auto& units = dwarf->getNormalUnitsVector();
    EXPECT_GT(units.size(), 0) << "No compilation units found in debug info";
}

TEST_F(Given_DebugInfo, When_TranslatingELFWithDebugInfo_EmitsFileDirectives)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/valid_elf.so")))
        << "File not found: " << GetTestDataPath("data/valid_elf.so");
    Translator translator(GetTestDataPath("data/valid_elf.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load valid ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find(".cv_file 1 \""), std::string::npos) << "Missing .cv_file index 1";
    EXPECT_NE(asm_output.find("data\\\\valid_elf.c\""), std::string::npos)
        << "Missing valid_elf.c file mapping";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/valid_elf.s");
}

TEST_F(Given_DebugInfo, When_TranslatingELFWithDebugInfo_EmitsLocDirectives)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/valid_elf.so")))
        << "File not found: " << GetTestDataPath("data/valid_elf.so");
    Translator translator(GetTestDataPath("data/valid_elf.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load valid ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find(".cv_loc 0 1 9"), std::string::npos)
        << "Missing expected .cv_loc directive for get_val";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/valid_elf.s");
}

TEST_F(Given_DebugInfo, When_TranslatingELFWithDebugInfo_EmitsCvLinetableDirectives)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/valid_elf.so")))
        << "File not found: " << GetTestDataPath("data/valid_elf.so");
    Translator translator(GetTestDataPath("data/valid_elf.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load valid ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find("  .cv_linetable 0, func_code_start_get_val, func_code_end_get_val"),
              std::string::npos)
        << "Missing expected .cv_linetable directive for get_val";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/valid_elf.s");
}

TEST_F(Given_DebugInfo, When_TranslatingELFWithDebugInfo_EmitsCvFilechecksumsDirective)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/valid_elf.so")))
        << "File not found: " << GetTestDataPath("data/valid_elf.so");
    Translator translator(GetTestDataPath("data/valid_elf.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load valid ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find("  .cv_filechecksums"), std::string::npos)
        << "Missing expected .cv_filechecksums directive";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/valid_elf.s");
}

TEST_F(Given_DebugInfo, When_TranslatingELFWithDebugInfo_EmitsCvStringtableDirective)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/valid_elf.so")))
        << "File not found: " << GetTestDataPath("data/valid_elf.so");
    Translator translator(GetTestDataPath("data/valid_elf.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load valid ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find("  .cv_stringtable"), std::string::npos)
        << "Missing expected .cv_stringtable directive";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/valid_elf.s");
}

TEST_F(Given_DebugInfo, When_TranslatingELFWithUnwindInfo_EmitsSehDirectives)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/valid_elf.so")))
        << "File not found: " << GetTestDataPath("data/valid_elf.so");
    Translator translator(GetTestDataPath("data/valid_elf.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load valid ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find(".seh_proc get_val"), std::string::npos)
        << "Missing SEH proc for get_val";
    EXPECT_NE(asm_output.find(".seh_pushreg rbp"), std::string::npos)
        << "Missing seh_pushreg rbp in get_val";
    EXPECT_NE(asm_output.find(".seh_setframe rbp, 0"), std::string::npos)
        << "Missing seh_setframe in get_val";
    EXPECT_NE(asm_output.find(".seh_endprologue"), std::string::npos) << "Missing seh_endprologue";
    EXPECT_NE(asm_output.find(".seh_endproc"), std::string::npos) << "Missing seh_endproc";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/valid_elf.s");
}

TEST_F(Given_DebugInfo, When_TranslatingAArch64ELFWithUnwindInfo_EmitsAArch64SehDirectives)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/relocs_elf_aarch64.so")))
        << "File not found: " << GetTestDataPath("data/relocs_elf_aarch64.so");
    Translator translator(GetTestDataPath("data/relocs_elf_aarch64.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load relocs AArch64 ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find(".seh_proc call_ext"), std::string::npos)
        << "Missing SEH proc for call_ext";
    EXPECT_NE(asm_output.find(".seh_stackalloc 16"), std::string::npos)
        << "Missing AArch64 stackalloc 16";
    EXPECT_NE(asm_output.find(".seh_save_fplr 0"), std::string::npos)
        << "Missing FP/LR save directive in AArch64 SEH";
    EXPECT_NE(asm_output.find(".seh_endprologue"), std::string::npos) << "Missing seh_endprologue";
    EXPECT_NE(asm_output.find(".seh_endproc"), std::string::npos) << "Missing seh_endproc";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/relocs_elf_aarch64.s");
}

TEST_F(Given_DebugInfo, When_TranslatingARMELFWithUnwindInfo_EmitsARMSehDirectives)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/relocs_elf_arm.so")))
        << "File not found: " << GetTestDataPath("data/relocs_elf_arm.so");
    Translator translator(GetTestDataPath("data/relocs_elf_arm.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load relocs ARM ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find(".seh_proc call_ext"), std::string::npos)
        << "Missing SEH proc for call_ext";
    EXPECT_NE(asm_output.find(".seh_save_regs {r11, lr}"), std::string::npos)
        << "Missing expected ARM save_regs directive";
    EXPECT_NE(asm_output.find(".seh_endprologue"), std::string::npos) << "Missing seh_endprologue";
    EXPECT_NE(asm_output.find(".seh_endproc"), std::string::npos) << "Missing seh_endproc";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/relocs_elf_arm.s");
}

TEST_F(Given_DebugInfo, When_TranslatingStructTypes_EmitsSimpleStructClassRecord)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_structs.so")))
        << "File not found: " << GetTestDataPath("data/cv_structs.so");
    Translator translator(GetTestDataPath("data/cv_structs.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV structs ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find("TypeIndex=4099, Length=32, Leaf=5381"), std::string::npos)
        << "Missing Simple struct CodeView record structure";
    EXPECT_NE(asm_output.find(ToByteSequence("Simple")), std::string::npos)
        << "Missing 'Simple' struct record name";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_structs.s");
}

TEST_F(Given_DebugInfo, When_TranslatingStructTypes_EmitsNestedStructClassRecord)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_structs.so")))
        << "File not found: " << GetTestDataPath("data/cv_structs.so");
    Translator translator(GetTestDataPath("data/cv_structs.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV structs ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find("TypeIndex=4101, Length=32, Leaf=5381"), std::string::npos)
        << "Missing Nested struct CodeView record structure";
    EXPECT_NE(asm_output.find(ToByteSequence("Nested")), std::string::npos)
        << "Missing 'Nested' struct record name";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_structs.s");
}

TEST_F(Given_DebugInfo, When_TranslatingUnionTypes_EmitsCorrectUnionRecords)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_unions.so")))
        << "File not found: " << GetTestDataPath("data/cv_unions.so");
    Translator translator(GetTestDataPath("data/cv_unions.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV unions ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find("TypeIndex=4098, Length=24, Leaf=5382"), std::string::npos)
        << "Missing MyUnion CodeView record structure";
    EXPECT_NE(asm_output.find(ToByteSequence("MyUnion")), std::string::npos)
        << "Missing 'MyUnion' union record name";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_unions.s");
}

TEST_F(Given_DebugInfo, When_TranslatingEnumTypes_EmitsCorrectEnumRecords)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_enums.so")))
        << "File not found: " << GetTestDataPath("data/cv_enums.so");
    Translator translator(GetTestDataPath("data/cv_enums.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV enums ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find("TypeIndex=4097, Length=24, Leaf=5383"), std::string::npos)
        << "Missing MyEnum CodeView record structure";
    EXPECT_NE(asm_output.find(ToByteSequence("MyEnum")), std::string::npos)
        << "Missing 'MyEnum' enum record name";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_enums.s");
}

TEST_F(Given_DebugInfo, When_TranslatingArrayTypes_EmitsCorrectArrayRecords)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_arrays.so")))
        << "File not found: " << GetTestDataPath("data/cv_arrays.so");
    Translator translator(GetTestDataPath("data/cv_arrays.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV arrays ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find("TypeIndex=4096, Length=16, Leaf=5379"), std::string::npos)
        << "Missing LF_ARRAY record structure";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_arrays.s");
}

TEST_F(Given_DebugInfo, When_TranslatingLocalVariables_EmitsParameterSymbolRecord)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_vars.so")))
        << "File not found: " << GetTestDataPath("data/cv_vars.so");
    Translator translator(GetTestDataPath("data/cv_vars.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV vars ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find("Symbol Record: Kind=4414"), std::string::npos)
        << "Missing S_LOCAL symbol record";
    EXPECT_NE(asm_output.find(ToByteSequence("param_a")), std::string::npos)
        << "Missing 'param_a' symbol record";
    EXPECT_NE(asm_output.find(".short 0x1142 # S_DEFRANGE_FRAMEPOINTER_REL"), std::string::npos)
        << "Missing S_DEFRANGE_FRAMEPOINTER_REL for param_a";
    EXPECT_NE(asm_output.find("  .long -4 # Offset"), std::string::npos)
        << "Incorrect offset for param_a (expected -4)";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_vars.s");
}

TEST_F(Given_DebugInfo, When_TranslatingLocalVariables_EmitsStackLocalSymbolRecord)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_vars.so")))
        << "File not found: " << GetTestDataPath("data/cv_vars.so");
    Translator translator(GetTestDataPath("data/cv_vars.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV vars ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find("Symbol Record: Kind=4414"), std::string::npos)
        << "Missing S_LOCAL symbol record";
    EXPECT_NE(asm_output.find(ToByteSequence("local_stack")), std::string::npos)
        << "Missing 'local_stack' symbol record";
    EXPECT_NE(asm_output.find(".short 0x1142 # S_DEFRANGE_FRAMEPOINTER_REL"), std::string::npos)
        << "Missing S_DEFRANGE_FRAMEPOINTER_REL for local_stack";
    EXPECT_NE(asm_output.find("  .long -8 # Offset"), std::string::npos)
        << "Incorrect offset for local_stack (expected -8)";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_vars.s");
}

TEST_F(Given_DebugInfo, When_TranslatingLocalVariables_EmitsRegisterLocalSymbolRecord)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_vars.so")))
        << "File not found: " << GetTestDataPath("data/cv_vars.so");
    Translator translator(GetTestDataPath("data/cv_vars.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV vars ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find("Symbol Record: Kind=4414"), std::string::npos)
        << "Missing S_LOCAL symbol record";
    EXPECT_NE(asm_output.find(ToByteSequence("local_reg")), std::string::npos)
        << "Missing 'local_reg' symbol record";
    EXPECT_NE(asm_output.find(".short 0x1142 # S_DEFRANGE_FRAMEPOINTER_REL"), std::string::npos)
        << "Missing S_DEFRANGE_FRAMEPOINTER_REL for local_reg";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_vars.s");
}

TEST_F(Given_DebugInfo, When_TranslatingSubprogram_EmitsFunctionType)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_subprogram.so")))
        << "File not found: " << GetTestDataPath("data/cv_subprogram.so");
    Translator translator(GetTestDataPath("data/cv_subprogram.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV subprogram ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find(".long 4098 # FunctionType"), std::string::npos)
        << "Missing non-zero FunctionType index in S_GPROC32_ID";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_subprogram.s");
}

TEST_F(Given_DebugInfo, When_TranslatingParameters_EmitsLocalSymWithIsParameter)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_subprogram.so")))
        << "File not found: " << GetTestDataPath("data/cv_subprogram.so");
    Translator translator(GetTestDataPath("data/cv_subprogram.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV subprogram ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find("Symbol Record: Kind=4414"), std::string::npos)
        << "Missing S_LOCAL symbol record";
    EXPECT_NE(asm_output.find(ToByteSequence("param_a")), std::string::npos)
        << "Missing 'param_a' parameter symbol record";
    EXPECT_NE(asm_output.find(ToByteSequence("param_b")), std::string::npos)
        << "Missing 'param_b' parameter symbol record";
    EXPECT_NE(asm_output.find(ToByteSequence("param_c")), std::string::npos)
        << "Missing 'param_c' parameter symbol record";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_subprogram.s");
}

TEST_F(Given_DebugInfo, When_TranslatingFloatParameters_EmitsXmmRegisters)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_float.so")))
        << "File not found: " << GetTestDataPath("data/cv_float.so");
    Translator translator(GetTestDataPath("data/cv_float.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV float ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find("Symbol Record: Kind=4414"), std::string::npos)
        << "Missing S_LOCAL symbol record";
    EXPECT_NE(asm_output.find(ToByteSequence("param_xmm0")), std::string::npos)
        << "Missing 'param_xmm0' parameter symbol record";
    EXPECT_NE(asm_output.find(ToByteSequence("param_xmm1")), std::string::npos)
        << "Missing 'param_xmm1' parameter symbol record";
    EXPECT_NE(asm_output.find(".short 154 # Register"), std::string::npos)
        << "Missing XMM0 register in S_DEFRANGE_REGISTER";
    EXPECT_NE(asm_output.find(".short 155 # Register"), std::string::npos)
        << "Missing XMM1 register in S_DEFRANGE_REGISTER";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_float.s");
}

TEST_F(Given_DebugInfo, When_TranslatingHugeRange_SplitsDefRangeRecords)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_huge_range.so")))
        << "File not found: " << GetTestDataPath("data/cv_huge_range.so");
    std::string warnings;
    llvm::raw_string_ostream warn_os(warnings);
    Translator translator(GetTestDataPath("data/cv_huge_range.so"), /*columnInfo=*/false, warn_os);
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();
    warn_os.flush();

    EXPECT_NE(asm_output.find("# Local Variable: local"), std::string::npos);
    EXPECT_NE(asm_output.find(".short 61440\n"), std::string::npos)
        << "Missing split DefRange with max size 61440";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_huge_range.s");
}

TEST_F(Given_DebugInfo, When_TranslatingMovingVariables_EmitsLocalAndDefRangeRecords)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_moving_var.so")))
        << "File not found: " << GetTestDataPath("data/cv_moving_var.so");
    Translator translator(
        GetTestDataPath("data/cv_moving_var.so"), /*columnInfo=*/false, llvm::nulls());
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find("# Local Variable: local"), std::string::npos);
    EXPECT_NE(asm_output.find("# Local Variable: a"), std::string::npos);
    EXPECT_NE(asm_output.find("# Local Variable: b"), std::string::npos);
    EXPECT_NE(asm_output.find(".short 0x1141 # S_DEFRANGE_REGISTER"), std::string::npos);
    EXPECT_NE(asm_output.find(".secrel32 test_func + "), std::string::npos);
    EXPECT_NE(asm_output.find(".secidx test_func"), std::string::npos);
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_moving_var.s");
}

TEST_F(Given_DebugInfo, When_TranslatingSpilledVariables_EmitsRegisterRelDefRange)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_spill.so")))
        << "File not found: " << GetTestDataPath("data/cv_spill.so");
    std::string warnings;
    llvm::raw_string_ostream warn_os(warnings);
    Translator translator(GetTestDataPath("data/cv_spill.so"), /*columnInfo=*/false, warn_os);
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();
    warn_os.flush();

    EXPECT_NE(asm_output.find("# Local Variable: local"), std::string::npos);
    EXPECT_NE(asm_output.find(".short 0x1145 # S_DEFRANGE_REGISTER_REL"), std::string::npos)
        << "Missing S_DEFRANGE_REGISTER_REL record";
    EXPECT_NE(asm_output.find(".short 335 # Register\n  .short 0 # Flags\n  .long 12 # Offset"),
              std::string::npos)
        << "Incorrect RSP-relative DefRange encoding";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_spill.s");
}

TEST_F(Given_DebugInfo, When_TranslatingUnsupportedLocations_EmitsWarnings)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_unsupported_loc.so")))
        << "File not found: " << GetTestDataPath("data/cv_unsupported_loc.so");
    std::string warnings;
    llvm::raw_string_ostream warn_os(warnings);
    Translator translator(
        GetTestDataPath("data/cv_unsupported_loc.so"), /*columnInfo=*/false, warn_os);
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();
    warn_os.flush();

    EXPECT_FALSE(warnings.empty()) << "Expected some warnings for optimized variables";
    EXPECT_NE(warnings.find("warning: unsupported DWARF location expression"), std::string::npos)
        << "Actual warnings: " << warnings;
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_unsupported_loc.s");
    ExpectOutputMatchesGolden(warnings, "data/output/cli/cv_unsupported_loc_stderr.txt");
}

TEST_F(Given_DebugInfo, When_TranslatingMultiFileLineMapping_EmitsCorrectLocDirectives)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_lines.so")))
        << "File not found: " << GetTestDataPath("data/cv_lines.so");
    Translator translator(GetTestDataPath("data/cv_lines.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV lines ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find(".cv_file 1 "), std::string::npos) << "Missing .cv_file 1 directive";
    EXPECT_NE(asm_output.find("cv_lines.c\""), std::string::npos)
        << "Missing cv_lines.c file mapping";
    EXPECT_NE(asm_output.find(".cv_file 2 "), std::string::npos) << "Missing .cv_file 2 directive";
    EXPECT_NE(asm_output.find("cv_lines.h\""), std::string::npos)
        << "Missing cv_lines.h file mapping";
    EXPECT_NE(asm_output.find(".cv_loc 0 1 "), std::string::npos)
        << "Missing cv_loc mapping for cv_lines.c";
    EXPECT_NE(asm_output.find(".cv_loc 1 2 "), std::string::npos)
        << "Missing cv_loc mapping for cv_lines.h";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_lines.s");
}

TEST_F(Given_DebugInfo, When_TranslatingLeafFunction_HasNoSehDirectives)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/seh_leaf.so")))
        << "File not found: " << GetTestDataPath("data/seh_leaf.so");
    Translator translator(GetTestDataPath("data/seh_leaf.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load SEH leaf ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    // Since it was compiled with -O2, a leaf function should have no prologue frame stack setup,
    // and DWARF has no CFI. So we should NOT emit any SEH directives for it!
    EXPECT_EQ(asm_output.find(".seh_proc"), std::string::npos)
        << "Leaf function should not have SEH proc";
    EXPECT_EQ(asm_output.find(".seh_endproc"), std::string::npos)
        << "Leaf function should not have SEH endproc";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/seh_leaf.s");
}

TEST_F(Given_DebugInfo, When_TranslatingNormalFunction_HasCorrectSehDirectives)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/seh_normal.so")))
        << "File not found: " << GetTestDataPath("data/seh_normal.so");
    Translator translator(GetTestDataPath("data/seh_normal.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load SEH normal ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find(".seh_proc normal_func"), std::string::npos)
        << "Missing SEH proc for normal_func";
    EXPECT_NE(asm_output.find(".seh_endprologue"), std::string::npos) << "Missing SEH endprologue";
    EXPECT_NE(asm_output.find(".seh_endproc"), std::string::npos) << "Missing SEH endproc";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/seh_normal.s");
}

TEST_F(Given_DebugInfo,
       When_TranslatingRecursiveStruct_AvoidsStackOverflowAndGeneratesCorrectRecords)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_recursive.so")))
        << "File not found: " << GetTestDataPath("data/cv_recursive.so");
    Translator translator(GetTestDataPath("data/cv_recursive.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV recursive ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find(ToByteSequence("Node")), std::string::npos)
        << "Missing 'Node' struct record";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_recursive.s");
}

TEST_F(Given_DebugInfo, When_TranslatingNamespaces_EmitsQualifiedClassNames)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_namespaces.so")))
        << "File not found: " << GetTestDataPath("data/cv_namespaces.so");
    Translator translator(GetTestDataPath("data/cv_namespaces.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV namespaces ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find(ToByteSequence("ns::Outer")), std::string::npos)
        << "Missing 'ns::Outer' struct record";
    EXPECT_NE(asm_output.find(ToByteSequence("ns::Outer::Inner")), std::string::npos)
        << "Missing 'ns::Outer::Inner' struct record";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_namespaces.s");
}

TEST_F(Given_DebugInfo, When_TranslatingNamespaces_EmitsNestedTypeRecords)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_namespaces.so")))
        << "File not found: " << GetTestDataPath("data/cv_namespaces.so");
    Translator translator(GetTestDataPath("data/cv_namespaces.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV namespaces ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find(ToByteSequence("Inner")), std::string::npos)
        << "Missing 'Inner' nested type record";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_namespaces.s");
}

TEST_F(Given_DebugInfo, When_TranslatingClassInheritance_EmitsBaseClassRecords)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_inheritance.so")))
        << "File not found: " << GetTestDataPath("data/cv_inheritance.so");
    Translator translator(GetTestDataPath("data/cv_inheritance.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV inheritance ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find(ToByteSequence("Derived")), std::string::npos)
        << "Missing 'Derived' struct record";
    EXPECT_NE(asm_output.find(ToByteSequence("Base")), std::string::npos)
        << "Missing 'Base' struct record";
    std::string base_class_signature = "  .byte 0\n  .byte 20\n";
    EXPECT_NE(asm_output.find(base_class_signature), std::string::npos)
        << "Missing LF_BCLASS record signature";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_inheritance.s");
}

TEST_F(Given_DebugInfo, When_TranslatingClassMethods_EmitsOneMethodRecords)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_methods.so")))
        << "File not found: " << GetTestDataPath("data/cv_methods.so");
    Translator translator(GetTestDataPath("data/cv_methods.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV methods ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find(ToByteSequence("MethodClass")), std::string::npos)
        << "Missing 'MethodClass' struct record";
    EXPECT_NE(asm_output.find(ToByteSequence("GetVal")), std::string::npos)
        << "Missing 'GetVal' method symbol record";
    std::string one_method_signature = "  .byte 17\n  .byte 21\n";
    EXPECT_NE(asm_output.find(one_method_signature), std::string::npos)
        << "Missing LF_ONEMETHOD record signature";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_methods.s");
}

TEST_F(Given_DebugInfo, When_TranslatingInlinedFunctions_EmitsInlinedVariables)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_inlined_vars.so")))
        << "File not found: " << GetTestDataPath("data/cv_inlined_vars.so");
    Translator translator(GetTestDataPath("data/cv_inlined_vars.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV inlined vars ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find(ToByteSequence("x")), std::string::npos)
        << "Missing inlined parameter 'x'";
    EXPECT_NE(asm_output.find(ToByteSequence("y")), std::string::npos)
        << "Missing inlined parameter 'y'";
    EXPECT_NE(asm_output.find(ToByteSequence("sum")), std::string::npos)
        << "Missing inlined local variable 'sum'";
    EXPECT_NE(asm_output.find("  .short 0x114d # S_INLINESITE"), std::string::npos)
        << "Missing S_INLINESITE symbol block";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_inlined_vars.s");
}

TEST_F(Given_DebugInfo, When_TranslatingAnonymousNamespaces_EmitsQualifiedAnonClassNames)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_anon_namespaces.so")))
        << "File not found: " << GetTestDataPath("data/cv_anon_namespaces.so");
    Translator translator(GetTestDataPath("data/cv_anon_namespaces.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV anon namespaces ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find(ToByteSequence("(anonymous namespace)::AnonStruct")),
              std::string::npos)
        << "Missing '(anonymous namespace)::AnonStruct' struct record";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_anon_namespaces.s");
}

TEST_F(Given_DebugInfo, When_TranslatingTemplateClasses_EmitsCorrectTemplateNameAndRecords)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_templates.so")))
        << "File not found: " << GetTestDataPath("data/cv_templates.so");
    Translator translator(GetTestDataPath("data/cv_templates.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV templates ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find(ToByteSequence("TempClass<int>")), std::string::npos)
        << "Missing 'TempClass<int>' struct record";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_templates.s");
}

TEST_F(Given_DebugInfo, When_TranslatingStaticMethods_EmitsOneMethodStaticRecords)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_static_methods.so")))
        << "File not found: " << GetTestDataPath("data/cv_static_methods.so");
    Translator translator(GetTestDataPath("data/cv_static_methods.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV static methods ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find(ToByteSequence("StaticMethodClass")), std::string::npos)
        << "Missing 'StaticMethodClass' struct record";
    EXPECT_NE(asm_output.find(ToByteSequence("GetConstant")), std::string::npos)
        << "Missing 'GetConstant' static method symbol record";
    // 17 and 21 correspond to the LF_ONEMETHOD (0x1511) CodeView record type signature.
    std::string one_method_signature = "  .byte 17\n  .byte 21\n";
    EXPECT_NE(asm_output.find(one_method_signature), std::string::npos)
        << "Missing LF_ONEMETHOD record signature";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_static_methods.s");
}

TEST_F(Given_DebugInfo, When_TranslatingNestedInlinedFunctions_EmitsHierarchicalInlineSiteRecords)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_nested_inlines.so")))
        << "File not found: " << GetTestDataPath("data/cv_nested_inlines.so");
    Translator translator(GetTestDataPath("data/cv_nested_inlines.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV nested inlines ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find(ToByteSequence("x")), std::string::npos)
        << "Missing outer_inline parameter 'x'";
    EXPECT_NE(asm_output.find(ToByteSequence("y")), std::string::npos)
        << "Missing outer_inline parameter 'y'";
    EXPECT_NE(asm_output.find(ToByteSequence("sum")), std::string::npos)
        << "Missing outer_inline variable 'sum'";
    EXPECT_NE(asm_output.find(ToByteSequence("res")), std::string::npos)
        << "Missing outer_inline variable 'res'";
    EXPECT_NE(asm_output.find(ToByteSequence("z")), std::string::npos)
        << "Missing inner_inline parameter 'z'";
    EXPECT_NE(asm_output.find(ToByteSequence("inner_val")), std::string::npos)
        << "Missing inner_inline variable 'inner_val'";
    EXPECT_NE(asm_output.find(ToByteSequence("lexical_val")), std::string::npos)
        << "Missing inner_inline lexical block variable 'lexical_val'";
    EXPECT_NE(asm_output.find("# Inline Site Symbol: outer_inline"), std::string::npos)
        << "Missing outer_inline site block";
    EXPECT_NE(asm_output.find("# Inline Site Symbol: inner_inline"), std::string::npos)
        << "Missing inner_inline site block";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_nested_inlines.s");
}

TEST_F(Given_DebugInfo, When_TranslatingAdvancedTypes_EmitsLfModifierLeaf)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_advanced_types.so")))
        << "File not found: " << GetTestDataPath("data/cv_advanced_types.so");
    Translator translator(GetTestDataPath("data/cv_advanced_types.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV advanced types ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find(", Leaf=4097\n"), std::string::npos)
        << "Missing LF_MODIFIER record in CodeView leaves";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_advanced_types.s");
}

TEST_F(Given_DebugInfo, When_TranslatingAdvancedTypes_EmitsClassFieldAccessRecords)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_advanced_types.so")))
        << "File not found: " << GetTestDataPath("data/cv_advanced_types.so");
    Translator translator(GetTestDataPath("data/cv_advanced_types.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV advanced types ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find(ToByteSequence("m_privateField")), std::string::npos)
        << "Missing class field 'm_privateField'";
    EXPECT_NE(asm_output.find(ToByteSequence("m_protectedField")), std::string::npos)
        << "Missing class field 'm_protectedField'";
    EXPECT_NE(asm_output.find(ToByteSequence("m_publicBool")), std::string::npos)
        << "Missing class field 'm_publicBool'";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_advanced_types.s");
}

TEST_F(Given_DebugInfo, When_TranslatingAdvancedTypes_EmitsRegisterVariables)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_advanced_types.so")))
        << "File not found: " << GetTestDataPath("data/cv_advanced_types.so");
    Translator translator(GetTestDataPath("data/cv_advanced_types.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV advanced types ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find(ToByteSequence("r_rax")), std::string::npos)
        << "Missing register variable r_rax";
    EXPECT_NE(asm_output.find(ToByteSequence("r_rbx")), std::string::npos)
        << "Missing register variable r_rbx";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_advanced_types.s");
}

TEST_F(Given_DebugInfo, When_TranslatingLocalFunctions_EmitsLocalProcedureSymbolRecord)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_local_functions.so")))
        << "File not found: " << GetTestDataPath("data/cv_local_functions.so");
    Translator translator(GetTestDataPath("data/cv_local_functions.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV local functions ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find("local_func."), std::string::npos) << "Missing local function label";
    EXPECT_NE(asm_output.find("S_LPROC32_ID"), std::string::npos) << "Missing S_LPROC32_ID record";
    EXPECT_NE(asm_output.find(".asciz \"local_func\""), std::string::npos)
        << "Missing clean display name";
    EXPECT_NE(asm_output.find(".secrel32 local_func."), std::string::npos)
        << "Missing secrel relocation";
    EXPECT_NE(asm_output.find(".secidx local_func."), std::string::npos)
        << "Missing secidx relocation";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_local_functions.s");
}

TEST_F(Given_DebugInfo, When_TranslatingStaticVariables_EmitsLocalDataSymbolRecord)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_local_variables.so")))
        << "File not found: " << GetTestDataPath("data/cv_local_variables.so");
    Translator translator(GetTestDataPath("data/cv_local_variables.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV local variables ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find("file_static_var ="), std::string::npos)
        << "Missing static variable alias";
    EXPECT_NE(asm_output.find("Local Variable Symbol: file_static_var"), std::string::npos)
        << "Missing Local Variable Symbol comment";
    EXPECT_NE(asm_output.find(".asciz \"file_static_var\""), std::string::npos)
        << "Missing clean display name";
    EXPECT_NE(asm_output.find(".secrel32 file_static_var"), std::string::npos)
        << "Missing secrel relocation";
    EXPECT_NE(asm_output.find(".secidx file_static_var"), std::string::npos)
        << "Missing secidx relocation";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_local_variables.s");
}

TEST_F(Given_DebugInfo, When_TranslatingELFWithDebugInfo_AllCodeviewRecordsArePaddedTo4Bytes)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_nested_inlines.so")))
        << "File not found: " << GetTestDataPath("data/cv_nested_inlines.so");
    Translator translator(GetTestDataPath("data/cv_nested_inlines.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();
    std::regex re(R"(Length=(\d+))");
    std::vector<int> lengths;
    std::transform(std::sregex_token_iterator(asm_output.begin(), asm_output.end(), re, 1),
                   std::sregex_token_iterator(),
                   std::back_inserter(lengths),
                   [](const std::string& s)
                   {
                       return std::stoi(s);
                   });

    EXPECT_FALSE(lengths.empty()) << "No CodeView records found in output";
    for (int len : lengths)
    {
        EXPECT_EQ(len % 4, 0) << "Record is not 4-byte aligned: " << len;
    }
}

TEST_F(Given_DebugInfo, When_TranslatingFunction_EmitsCorrectEndOffsetPointer)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_scope_end.so")))
        << "File not found: " << GetTestDataPath("data/cv_scope_end.so");
    Translator translator(GetTestDataPath("data/cv_scope_end.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV scope end ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(asm_output.find(".Lfunc_scope_end_record_simple_func - .Lsym_begin # End offset"),
              std::string::npos)
        << "Missing correct End offset pointer for simple_func";
    EXPECT_NE(
        asm_output.find(".Lfunc_scope_end_record_simple_func:\n  .short 2\n  .short 6 # S_END"),
        std::string::npos)
        << "Missing correct S_END record label for simple_func";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_scope_end.s");
}

TEST_F(Given_DebugInfo, When_TranslatingFunctionPointer_EmitsProcedurePointerType)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_func_ptr.so")))
        << "File not found: " << GetTestDataPath("data/cv_func_ptr.so");
    Translator translator(GetTestDataPath("data/cv_func_ptr.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV func ptr ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    // Leaf=4104 is LF_PROCEDURE
    EXPECT_NE(asm_output.find(", Leaf=4104"), std::string::npos) << "Missing LF_PROCEDURE record";
    // Leaf=4098 is LF_POINTER
    EXPECT_NE(asm_output.find(", Leaf=4098"), std::string::npos) << "Missing LF_POINTER record";
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_func_ptr.s");
}

TEST_F(Given_DebugInfo, When_TranslatingWithDuplicateLines_EmitsDeduplicatedLocDirectives)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_loc_dedup.so")))
        << "File not found: " << GetTestDataPath("data/cv_loc_dedup.so");
    Translator translator(GetTestDataPath("data/cv_loc_dedup.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV dedup ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_TRUE(VerifyLocDirectives(asm_output, 3, false));
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_loc_dedup.s");
}

TEST_F(Given_DebugInfo,
       When_TranslatingWithDuplicateLinesAndColumnInfo_EmitsAllLocDirectivesWithColumns)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/cv_loc_dedup.so")))
        << "File not found: " << GetTestDataPath("data/cv_loc_dedup.so");
    Translator translator(
        GetTestDataPath("data/cv_loc_dedup.so"), /*columnInfo=*/true, llvm::errs());
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load CV dedup ELF: " << ec.message();
    std::string asm_output;
    llvm::raw_string_ostream os(asm_output);

    translator.Translate(os);
    os.flush();

    EXPECT_TRUE(VerifyLocDirectives(asm_output, 4, true, /*exact_count=*/false));
    ExpectOutputMatchesGolden(asm_output, "data/output/asm/cv_loc_dedup_col.s");
}

} // namespace test
} // namespace repeat
