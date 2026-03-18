#include <filesystem>
#include <string>

#include <gtest/gtest.h>
#include <llvm/Object/ELF.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Support/raw_ostream.h>

#include "RepeatTest.h"
#include "repeat/translator.h"

namespace repeat
{
namespace test
{

class Given_MemoryLayout : public RepeatTest
{
};

TEST_F(Given_MemoryLayout, When_TranslatingBSS_EmitsPaddingToMemorySize)
{
    ASSERT_TRUE(std::filesystem::exists(GetTestDataPath("data/bss_elf.so")))
        << "Test file not found: " << GetTestDataPath("data/bss_elf.so");
    Translator translator(GetTestDataPath("data/bss_elf.so"));
    std::error_code ec = translator.Load();
    ASSERT_FALSE(ec) << "Failed to load BSS ELF: " << ec.message();
    auto buffer_or_err = llvm::MemoryBuffer::getFile(GetTestDataPath("data/bss_elf.so"));
    ASSERT_TRUE(!!buffer_or_err);
    auto elf_or_err =
        llvm::object::ObjectFile::createELFObjectFile(buffer_or_err.get()->getMemBufferRef());
    ASSERT_TRUE(!!elf_or_err);
    auto* elf_obj = elf_or_err.get().get();
    auto* elf_obj_64 = llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(elf_obj);
    ASSERT_NE(elf_obj_64, nullptr);
    const llvm::object::ELFFile<llvm::object::ELF64LE>& elf = elf_obj_64->getELFFile();
    auto phdrs_or_err = elf.program_headers();
    ASSERT_TRUE(!!phdrs_or_err);
    std::string expected_padding_directive;
    char suffix = 'A';
    bool has_bss_segment_to_expect = false;
    for (const auto& phdr : *phdrs_or_err)
    {
        if (phdr.p_type == llvm::ELF::PT_LOAD)
        {
            if (phdr.p_memsz > phdr.p_filesz)
            {
                expected_padding_directive = ".org __elf_seg_";
                expected_padding_directive += suffix;
                expected_padding_directive += " + " + std::to_string(phdr.p_memsz);
                has_bss_segment_to_expect = true;
                break;
            }
            suffix++;
        }
    }
    ASSERT_TRUE(has_bss_segment_to_expect);
    std::string assembly;
    llvm::raw_string_ostream os(assembly);

    translator.Translate(os);
    os.flush();

    EXPECT_NE(assembly.find(expected_padding_directive), std::string::npos);
}

} // namespace test
} // namespace repeat
