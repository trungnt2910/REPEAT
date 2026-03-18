#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <llvm/DebugInfo/DWARF/LowLevel/DWARFCFIProgram.h>

#include "repeat/arch/aarch64/aarch64_backend.h"
#include "repeat/arch/arm/arm_backend.h"
#include "repeat/arch/i386/i386_backend.h"
#include "repeat/arch/x86_64/x86_64_backend.h"

namespace repeat
{
namespace test
{

namespace dwarf_reg
{
struct X86_64
{
    enum : uint32_t
    {
        RBP = 6,
        RBX = 3,
        R12 = 12,
        XMM6 = 23
    };
};

struct AArch64
{
    enum : uint32_t
    {
        X19 = 19,
        X20 = 20,
        X21 = 21,
        FP = 29,
        LR = 30,
        D8 = 72,
        D9 = 73,
        D10 = 74
    };
};

struct Arm
{
    enum : uint32_t
    {
        R4 = 4,
        FP = 11,
        D0 = 256,
        D1 = 257
    };
};
} // namespace dwarf_reg

bool HasDirective(const std::vector<SehDirective>& directives, const std::string& pattern)
{
    for (const auto& dir : directives)
    {
        if (dir.m_text == pattern)
        {
            return true;
        }
    }
    return false;
}

// ============================================================================
// X86_64 Unwind Tests
// ============================================================================

class Given_X8664Unwind : public ::testing::Test
{
protected:
    X8664Backend m_backend;
};

TEST_F(Given_X8664Unwind, When_DefCfaOffset_EmitsStackAlloc)
{
    llvm::dwarf::CFIProgram cfis(1, -8, llvm::Triple::x86_64);
    cfis.addInstruction(llvm::dwarf::DW_CFA_def_cfa_offset, 32);
    uint64_t last_prologue_offset = 0;

    auto result = m_backend.TranslateUnwindInfo(cfis, last_prologue_offset);

    EXPECT_TRUE(HasDirective(result.m_directives, ".seh_stackalloc 24"));
    EXPECT_EQ(result.m_frameSize, 24);
}

TEST_F(Given_X8664Unwind, When_DefCfa_EmitsSetFrame)
{
    llvm::dwarf::CFIProgram cfis(1, -8, llvm::Triple::x86_64);
    cfis.addInstruction(llvm::dwarf::DW_CFA_def_cfa, dwarf_reg::X86_64::RBP, 16);
    uint64_t last_prologue_offset = 0;

    auto result = m_backend.TranslateUnwindInfo(cfis, last_prologue_offset);

    EXPECT_TRUE(HasDirective(result.m_directives, ".seh_setframe rbp, 0"));
    EXPECT_EQ(result.m_frameSize, 8);
}

TEST_F(Given_X8664Unwind, When_OffsetMatchingPC_EmitsPushReg)
{
    llvm::dwarf::CFIProgram cfis(1, -8, llvm::Triple::x86_64);
    cfis.addInstruction(llvm::dwarf::DW_CFA_def_cfa_offset, 48);
    cfis.addInstruction(llvm::dwarf::DW_CFA_offset, dwarf_reg::X86_64::RBX, 2);
    uint64_t last_prologue_offset = 0;

    auto result = m_backend.TranslateUnwindInfo(cfis, last_prologue_offset);

    EXPECT_TRUE(HasDirective(result.m_directives, ".seh_pushreg rbx"));
    EXPECT_EQ(result.m_frameSize, 40);
}

TEST_F(Given_X8664Unwind, When_OffsetExceedingPC_EmitsSaveReg)
{
    llvm::dwarf::CFIProgram cfis(1, -8, llvm::Triple::x86_64);
    cfis.addInstruction(llvm::dwarf::DW_CFA_def_cfa_offset, 48);
    cfis.addInstruction(llvm::dwarf::DW_CFA_advance_loc, 4);
    cfis.addInstruction(llvm::dwarf::DW_CFA_offset_extended, dwarf_reg::X86_64::R12, 3);
    uint64_t last_prologue_offset = 0;

    auto result = m_backend.TranslateUnwindInfo(cfis, last_prologue_offset);

    EXPECT_TRUE(HasDirective(result.m_directives, ".seh_savereg r12, 24"));
    EXPECT_EQ(result.m_frameSize, 40);
}

TEST_F(Given_X8664Unwind, When_XmmOffset_EmitsSaveXmm)
{
    llvm::dwarf::CFIProgram cfis(1, -8, llvm::Triple::x86_64);
    cfis.addInstruction(llvm::dwarf::DW_CFA_def_cfa_offset, 48);
    cfis.addInstruction(llvm::dwarf::DW_CFA_offset, dwarf_reg::X86_64::XMM6, 4);
    uint64_t last_prologue_offset = 0;

    auto result = m_backend.TranslateUnwindInfo(cfis, last_prologue_offset);

    EXPECT_TRUE(HasDirective(result.m_directives, ".seh_savexmm xmm6, 16"));
    EXPECT_EQ(result.m_frameSize, 40);
}

// ============================================================================
// AArch64 Unwind Tests
// ============================================================================

class Given_Aarch64Unwind : public ::testing::Test
{
protected:
    Aarch64Backend m_backend;
};

TEST_F(Given_Aarch64Unwind, When_DefCfaOffset_EmitsStackAlloc)
{
    llvm::dwarf::CFIProgram cfis(4, -8, llvm::Triple::aarch64);
    cfis.addInstruction(llvm::dwarf::DW_CFA_def_cfa_offset, 128);
    uint64_t last_prologue_offset = 0;

    auto result = m_backend.TranslateUnwindInfo(cfis, last_prologue_offset);

    EXPECT_TRUE(HasDirective(result.m_directives, ".seh_stackalloc 128"));
    EXPECT_EQ(result.m_frameSize, 128);
}

TEST_F(Given_Aarch64Unwind, When_DefCfa_EmitsFrame)
{
    llvm::dwarf::CFIProgram cfis(4, -8, llvm::Triple::aarch64);
    cfis.addInstruction(llvm::dwarf::DW_CFA_def_cfa, dwarf_reg::AArch64::FP, 128);
    uint64_t last_prologue_offset = 0;

    auto result = m_backend.TranslateUnwindInfo(cfis, last_prologue_offset);

    EXPECT_TRUE(HasDirective(result.m_directives, ".seh_frame x29, 0"));
    EXPECT_EQ(result.m_frameSize, 128);
}

TEST_F(Given_Aarch64Unwind, When_RegPairOffset_EmitsSaveRegP)
{
    llvm::dwarf::CFIProgram cfis(4, -8, llvm::Triple::aarch64);
    cfis.addInstruction(llvm::dwarf::DW_CFA_def_cfa_offset, 128);
    cfis.addInstruction(llvm::dwarf::DW_CFA_offset, dwarf_reg::AArch64::X19, 3);
    cfis.addInstruction(llvm::dwarf::DW_CFA_offset_extended, dwarf_reg::AArch64::X20, 2);
    uint64_t last_prologue_offset = 0;

    auto result = m_backend.TranslateUnwindInfo(cfis, last_prologue_offset);

    EXPECT_TRUE(HasDirective(result.m_directives, ".seh_save_reg_p x19, x20, 104"));
    EXPECT_EQ(result.m_frameSize, 128);
}

TEST_F(Given_Aarch64Unwind, When_FplrOffset_EmitsSaveFplr)
{
    llvm::dwarf::CFIProgram cfis(4, -8, llvm::Triple::aarch64);
    cfis.addInstruction(llvm::dwarf::DW_CFA_def_cfa_offset, 128);
    cfis.addInstruction(llvm::dwarf::DW_CFA_offset, dwarf_reg::AArch64::FP, 5);
    cfis.addInstruction(llvm::dwarf::DW_CFA_offset, dwarf_reg::AArch64::LR, 4);
    uint64_t last_prologue_offset = 0;

    auto result = m_backend.TranslateUnwindInfo(cfis, last_prologue_offset);

    EXPECT_TRUE(HasDirective(result.m_directives, ".seh_save_fplr 88"));
    EXPECT_EQ(result.m_frameSize, 128);
}

TEST_F(Given_Aarch64Unwind, When_FregPairOffset_EmitsSaveFregP)
{
    llvm::dwarf::CFIProgram cfis(4, -8, llvm::Triple::aarch64);
    cfis.addInstruction(llvm::dwarf::DW_CFA_def_cfa_offset, 128);
    cfis.addInstruction(llvm::dwarf::DW_CFA_offset, dwarf_reg::AArch64::D8, 7);
    cfis.addInstruction(llvm::dwarf::DW_CFA_offset, dwarf_reg::AArch64::D9, 6);
    uint64_t last_prologue_offset = 0;

    auto result = m_backend.TranslateUnwindInfo(cfis, last_prologue_offset);

    EXPECT_TRUE(HasDirective(result.m_directives, ".seh_save_freg_p d8, d9, 72"));
    EXPECT_EQ(result.m_frameSize, 128);
}

TEST_F(Given_Aarch64Unwind, When_UnpairedFregOffset_EmitsSaveFreg)
{
    llvm::dwarf::CFIProgram cfis(4, -8, llvm::Triple::aarch64);
    cfis.addInstruction(llvm::dwarf::DW_CFA_def_cfa_offset, 128);
    cfis.addInstruction(llvm::dwarf::DW_CFA_offset, dwarf_reg::AArch64::D10, 8);
    uint64_t last_prologue_offset = 0;

    auto result = m_backend.TranslateUnwindInfo(cfis, last_prologue_offset);

    EXPECT_TRUE(HasDirective(result.m_directives, ".seh_save_freg d10, 64"));
    EXPECT_EQ(result.m_frameSize, 128);
}

TEST_F(Given_Aarch64Unwind, When_UnpairedRegOffset_EmitsSaveReg)
{
    llvm::dwarf::CFIProgram cfis(4, -8, llvm::Triple::aarch64);
    cfis.addInstruction(llvm::dwarf::DW_CFA_def_cfa_offset, 128);
    cfis.addInstruction(llvm::dwarf::DW_CFA_offset, dwarf_reg::AArch64::X21, 9);
    uint64_t last_prologue_offset = 0;

    auto result = m_backend.TranslateUnwindInfo(cfis, last_prologue_offset);

    EXPECT_TRUE(HasDirective(result.m_directives, ".seh_save_reg x21, 56"));
    EXPECT_EQ(result.m_frameSize, 128);
}

// ============================================================================
// ARM Unwind Tests
// ============================================================================

class Given_ArmUnwind : public ::testing::Test
{
protected:
    ArmBackend m_backend;
};

TEST_F(Given_ArmUnwind, When_DefCfaOffset_EmitsStackAlloc)
{
    llvm::dwarf::CFIProgram cfis(2, -4, llvm::Triple::arm);
    cfis.addInstruction(llvm::dwarf::DW_CFA_def_cfa_offset, 32);
    uint64_t last_prologue_offset = 0;

    auto result = m_backend.TranslateUnwindInfo(cfis, last_prologue_offset);

    EXPECT_TRUE(HasDirective(result.m_directives, ".seh_stackalloc 32"));
    EXPECT_EQ(result.m_frameSize, 32);
}

TEST_F(Given_ArmUnwind, When_DefCfa_EmitsFrame)
{
    llvm::dwarf::CFIProgram cfis(2, -4, llvm::Triple::arm);
    cfis.addInstruction(llvm::dwarf::DW_CFA_def_cfa, dwarf_reg::Arm::FP, 8);
    uint64_t last_prologue_offset = 0;

    auto result = m_backend.TranslateUnwindInfo(cfis, last_prologue_offset);

    EXPECT_TRUE(HasDirective(result.m_directives, ".seh_frame r11, 0"));
    EXPECT_EQ(result.m_frameSize, 8);
}

TEST_F(Given_ArmUnwind, When_RegOffset_EmitsSaveRegs)
{
    llvm::dwarf::CFIProgram cfis(2, -4, llvm::Triple::arm);
    cfis.addInstruction(llvm::dwarf::DW_CFA_offset, dwarf_reg::Arm::R4, 1);
    uint64_t last_prologue_offset = 0;

    auto result = m_backend.TranslateUnwindInfo(cfis, last_prologue_offset);

    EXPECT_TRUE(HasDirective(result.m_directives, ".seh_save_regs {r4}"));
    EXPECT_EQ(result.m_frameSize, 0);
}

TEST_F(Given_ArmUnwind, When_FregOffset_EmitsSaveFregs)
{
    llvm::dwarf::CFIProgram cfis(2, -4, llvm::Triple::arm);
    cfis.addInstruction(llvm::dwarf::DW_CFA_offset, dwarf_reg::Arm::D0, 2);
    cfis.addInstruction(llvm::dwarf::DW_CFA_offset, dwarf_reg::Arm::D1, 3);
    uint64_t last_prologue_offset = 0;

    auto result = m_backend.TranslateUnwindInfo(cfis, last_prologue_offset);

    EXPECT_TRUE(HasDirective(result.m_directives, ".seh_save_fregs {d0, d1}"));
    EXPECT_EQ(result.m_frameSize, 0);
}

// ============================================================================
// i386 Unwind Tests
// ============================================================================

class Given_I386Unwind : public ::testing::Test
{
protected:
    I386Backend m_backend;
};

TEST_F(Given_I386Unwind, When_DefCfaOffset_CalculatesStackSize)
{
    llvm::dwarf::CFIProgram cfis(1, -4, llvm::Triple::x86);
    cfis.addInstruction(llvm::dwarf::DW_CFA_def_cfa_offset, 16);
    uint64_t last_prologue_offset = 0;

    auto result = m_backend.TranslateUnwindInfo(cfis, last_prologue_offset);

    EXPECT_TRUE(result.m_directives.empty());
    EXPECT_EQ(result.m_frameSize, 12);
}

TEST_F(Given_I386Unwind, When_DefCfa_CalculatesStackSize)
{
    llvm::dwarf::CFIProgram cfis(1, -4, llvm::Triple::x86);
    cfis.addInstruction(llvm::dwarf::DW_CFA_def_cfa, 5, 8);
    uint64_t last_prologue_offset = 0;

    auto result = m_backend.TranslateUnwindInfo(cfis, last_prologue_offset);

    EXPECT_TRUE(result.m_directives.empty());
    EXPECT_EQ(result.m_frameSize, 4);
}

} // namespace test
} // namespace repeat
