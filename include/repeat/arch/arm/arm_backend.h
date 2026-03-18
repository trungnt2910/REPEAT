#ifndef REPEAT_ARCH_ARM_ARM_BACKEND_H
#define REPEAT_ARCH_ARM_ARM_BACKEND_H

#include "repeat/arch/arch_backend.h"

namespace repeat
{

class ArmBackend : public ArchBackend<llvm::object::ELF32LE>
{
public:
    bool SupportsSeh() const override
    {
        return true;
    }

    UnwindInfoResult TranslateUnwindInfo(const llvm::dwarf::CFIProgram& cfis,
                                         uint64_t& last_prologue_offset) const override;

    std::optional<RelocEntry>
    ParseRelocation(const llvm::object::ELFFile<llvm::object::ELF32LE>& elf,
                    const llvm::object::ELFFile<llvm::object::ELF32LE>::Elf_Shdr* symtab_sec,
                    llvm::StringRef strtab,
                    const llvm::object::ELFFile<llvm::object::ELF32LE>::Elf_Rela& rela) override
    {
        return std::nullopt;
    }

    std::optional<RelocEntry>
    ParseRelocationRel(const llvm::object::ELFFile<llvm::object::ELF32LE>& elf,
                       const llvm::object::ELFFile<llvm::object::ELF32LE>::Elf_Shdr* symtab_sec,
                       llvm::StringRef strtab,
                       const llvm::object::ELFFile<llvm::object::ELF32LE>::Elf_Rel& rel,
                       uint64_t relocated_sec_addr,
                       llvm::ArrayRef<uint8_t> relocated_contents) override;
};

} // namespace repeat

#endif // REPEAT_ARCH_ARM_ARM_BACKEND_H
