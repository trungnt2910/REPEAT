#include "repeat/arch/i386/i386_backend.h"

#include <algorithm>

#include <llvm/BinaryFormat/ELF.h>
#include <llvm/DebugInfo/DWARF/DWARFDebugFrame.h>

namespace repeat
{

std::optional<RelocEntry> I386Backend::ParseRelocationRel(
    const llvm::object::ELFFile<llvm::object::ELF32LE>& elf,
    const llvm::object::ELFFile<llvm::object::ELF32LE>::Elf_Shdr* symtab_sec,
    llvm::StringRef strtab,
    const llvm::object::ELFFile<llvm::object::ELF32LE>::Elf_Rel& rel,
    uint64_t relocated_sec_addr,
    llvm::ArrayRef<uint8_t> relocated_contents)
{
    uint32_t type = rel.getType(false);
    uint64_t offset_in_sec = rel.r_offset - relocated_sec_addr;
    if (offset_in_sec + 4 > relocated_contents.size())
    {
        return std::nullopt;
    }

    // For i386 ELF REL relocations, the addend is stored inline in the memory location
    // being relocated, rather than in the relocation entry structure.
    uint32_t raw_addend =
        *reinterpret_cast<const uint32_t*>(relocated_contents.data() + offset_in_sec);
    uint64_t addend = static_cast<uint64_t>(raw_addend);

    if (type == llvm::ELF::R_386_RELATIVE)
    {
        return RelocEntry{rel.r_offset, addend, ""};
    }
    else if (type == llvm::ELF::R_386_GLOB_DAT || type == llvm::ELF::R_386_JUMP_SLOT)
    {
        uint32_t sym_idx = rel.getSymbol(false);
        auto sym_or_err = elf.getSymbol(symtab_sec, sym_idx);
        if (sym_or_err)
        {
            const auto* sym = *sym_or_err;
            if (sym->isUndefined())
            {
                if (sym->st_name < strtab.size())
                {
                    std::string sym_name = strtab.data() + sym->st_name;
                    return RelocEntry{rel.r_offset, addend, sym_name};
                }
            }
            else
            {
                uint64_t sym_value = sym->st_value;
                uint64_t target_value = sym_value + addend;
                return RelocEntry{rel.r_offset, target_value, ""};
            }
        }
    }
    return std::nullopt;
}

UnwindInfoResult I386Backend::TranslateUnwindInfo(const llvm::dwarf::CFIProgram& cfis,
                                                  uint64_t& last_prologue_offset) const
{
    uint64_t current_cfa_offset = 4;
    uint64_t max_cfa_offset = 4;
    bool has_fp = false;

    for (const auto& instr : cfis)
    {
        if (instr.Opcode == llvm::dwarf::DW_CFA_def_cfa_offset)
        {
            current_cfa_offset = instr.Ops[0];
        }
        else if (instr.Opcode == llvm::dwarf::DW_CFA_def_cfa_offset_sf)
        {
            int64_t factored_offset = static_cast<int64_t>(instr.Ops[0]) * cfis.dataAlign();
            current_cfa_offset = std::abs(factored_offset);
        }
        else if (instr.Opcode == llvm::dwarf::DW_CFA_def_cfa)
        {
            uint64_t reg = instr.Ops[0];
            current_cfa_offset = instr.Ops[1];
            if (reg == 5)
            {
                has_fp = true;
            }
        }
        else if (instr.Opcode == llvm::dwarf::DW_CFA_def_cfa_sf)
        {
            uint64_t reg = instr.Ops[0];
            int64_t factored_offset = static_cast<int64_t>(instr.Ops[1]) * cfis.dataAlign();
            current_cfa_offset = std::abs(factored_offset);
            if (reg == 5)
            {
                has_fp = true;
            }
        }
        else if (instr.Opcode == llvm::dwarf::DW_CFA_def_cfa_register)
        {
            uint64_t reg = instr.Ops[0];
            if (reg == 5)
            {
                has_fp = true;
            }
        }
        max_cfa_offset = std::max(max_cfa_offset, current_cfa_offset);
    }

    // On 32-bit x86, the initial CFA offset is 4 because the call instruction pushes the 4-byte
    // return address onto the stack. We subtract 4 from the maximum CFA offset to obtain
    // the actual stack size allocated by the function itself.
    return UnwindInfoResult{{}, max_cfa_offset - 4, has_fp};
}

} // namespace repeat
