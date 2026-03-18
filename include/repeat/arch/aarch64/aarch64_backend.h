#ifndef REPEAT_ARCH_AARCH64_AARCH64_BACKEND_H
#define REPEAT_ARCH_AARCH64_AARCH64_BACKEND_H

#include "repeat/arch/arch_backend.h"

namespace repeat
{

class Aarch64Backend : public ArchBackend<llvm::object::ELF64LE>
{
public:
    using ELFFile = llvm::object::ELFFile<llvm::object::ELF64LE>;

    bool SupportsSeh() const override
    {
        return true;
    }

    UnwindInfoResult TranslateUnwindInfo(const llvm::dwarf::CFIProgram& cfis,
                                         uint64_t& last_prologue_offset) const override;

    std::optional<RelocEntry> ParseRelocation(const ELFFile& elf,
                                              const ELFFile::Elf_Shdr* symtab_sec,
                                              llvm::StringRef strtab,
                                              const ELFFile::Elf_Rela& rela) override;

    std::optional<RelocEntry>
    ParseRelocationRel(const ELFFile& elf,
                       const ELFFile::Elf_Shdr* symtab_sec,
                       llvm::StringRef strtab,
                       const ELFFile::Elf_Rel& rel,
                       uint64_t relocated_sec_addr,
                       llvm::ArrayRef<uint8_t> relocated_contents) override
    {
        return std::nullopt;
    }
};

} // namespace repeat

#endif // REPEAT_ARCH_AARCH64_AARCH64_BACKEND_H
