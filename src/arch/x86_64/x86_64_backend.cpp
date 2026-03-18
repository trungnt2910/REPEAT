#include "repeat/arch/x86_64/x86_64_backend.h"

#include <llvm/BinaryFormat/ELF.h>

namespace repeat
{

std::optional<RelocEntry> X8664Backend::ParseRelocation(
    const llvm::object::ELFFile<llvm::object::ELF64LE>& elf,
    const llvm::object::ELFFile<llvm::object::ELF64LE>::Elf_Shdr* symtab_sec,
    llvm::StringRef strtab,
    const llvm::object::ELFFile<llvm::object::ELF64LE>::Elf_Rela& rela)
{
    uint32_t type = rela.getType(false);
    if (type == llvm::ELF::R_X86_64_RELATIVE)
    {
        return RelocEntry{rela.r_offset, static_cast<uint64_t>(rela.r_addend), ""};
    }
    else if (type == llvm::ELF::R_X86_64_GLOB_DAT || type == llvm::ELF::R_X86_64_JUMP_SLOT)
    {
        uint32_t sym_idx = rela.getSymbol(false);
        auto sym_or_err = elf.getSymbol(symtab_sec, sym_idx);
        if (sym_or_err)
        {
            const auto* sym = *sym_or_err;
            if (sym->isUndefined())
            {
                if (sym->st_name < strtab.size())
                {
                    std::string sym_name = strtab.data() + sym->st_name;
                    return RelocEntry{
                        rela.r_offset, static_cast<uint64_t>(rela.r_addend), sym_name};
                }
            }
            else
            {
                uint64_t sym_value = sym->st_value;
                uint64_t target_value = sym_value + rela.r_addend;
                return RelocEntry{rela.r_offset, target_value, ""};
            }
        }
    }
    return std::nullopt;
}

} // namespace repeat

namespace
{

std::string map_dwarf_reg_to_x86_64(uint64_t reg)
{
    switch (reg)
    {
    case 3:
        return "rbx";
    case 6:
        return "rbp";
    case 12:
        return "r12";
    case 13:
        return "r13";
    case 14:
        return "r14";
    case 15:
        return "r15";
    default:
        return "";
    }
}

} // namespace

namespace repeat
{

UnwindInfoResult X8664Backend::TranslateUnwindInfo(const llvm::dwarf::CFIProgram& cfis,
                                                   uint64_t& last_prologue_offset) const
{
    uint64_t current_offset = 0;
    uint64_t current_cfa_offset = 8;
    uint64_t max_cfa_offset = 8;
    uint64_t last_cfa_offset_change_offset = 0;
    std::vector<SehDirective> directives;
    bool has_fp = false;

    for (const auto& instr : cfis)
    {
        if (instr.Opcode == llvm::dwarf::DW_CFA_advance_loc ||
            instr.Opcode == llvm::dwarf::DW_CFA_advance_loc1 ||
            instr.Opcode == llvm::dwarf::DW_CFA_advance_loc2 ||
            instr.Opcode == llvm::dwarf::DW_CFA_advance_loc4)
        {
            uint64_t delta = instr.Ops[0];
            current_offset += delta * cfis.codeAlign();
            continue;
        }

        if (instr.Opcode == llvm::dwarf::DW_CFA_def_cfa_offset)
        {
            current_cfa_offset = instr.Ops[0];
            last_cfa_offset_change_offset = current_offset;
            if (current_cfa_offset > 8)
            {
                uint64_t alloc_size = current_cfa_offset - 8;
                std::string text = ".seh_stackalloc " + std::to_string(alloc_size);
                directives.push_back({current_offset, text});
                last_prologue_offset = current_offset;
            }
        }
        else if (instr.Opcode == llvm::dwarf::DW_CFA_def_cfa_offset_sf)
        {
            int64_t factored_offset = static_cast<int64_t>(instr.Ops[0]) * cfis.dataAlign();
            current_cfa_offset = std::abs(factored_offset);
            last_cfa_offset_change_offset = current_offset;
            if (current_cfa_offset > 8)
            {
                uint64_t alloc_size = current_cfa_offset - 8;
                std::string text = ".seh_stackalloc " + std::to_string(alloc_size);
                directives.push_back({current_offset, text});
                last_prologue_offset = current_offset;
            }
        }
        else if (instr.Opcode == llvm::dwarf::DW_CFA_def_cfa)
        {
            uint64_t reg = instr.Ops[0];
            uint64_t offset = instr.Ops[1];
            current_cfa_offset = offset;
            last_cfa_offset_change_offset = current_offset;
            std::string reg_name = map_dwarf_reg_to_x86_64(reg);
            if (reg_name == "rbp")
            {
                uint64_t fp_offset = current_cfa_offset - 16;
                std::string text = ".seh_setframe rbp, " + std::to_string(fp_offset);
                directives.push_back({current_offset, text});
                last_prologue_offset = current_offset;
                has_fp = true;
            }
        }
        else if (instr.Opcode == llvm::dwarf::DW_CFA_def_cfa_sf)
        {
            uint64_t reg = instr.Ops[0];
            int64_t factored_offset = static_cast<int64_t>(instr.Ops[1]) * cfis.dataAlign();
            current_cfa_offset = std::abs(factored_offset);
            last_cfa_offset_change_offset = current_offset;
            std::string reg_name = map_dwarf_reg_to_x86_64(reg);
            if (reg_name == "rbp")
            {
                uint64_t fp_offset = current_cfa_offset - 16;
                std::string text = ".seh_setframe rbp, " + std::to_string(fp_offset);
                directives.push_back({current_offset, text});
                last_prologue_offset = current_offset;
                has_fp = true;
            }
        }
        else if (instr.Opcode == llvm::dwarf::DW_CFA_offset ||
                 instr.Opcode == llvm::dwarf::DW_CFA_offset_extended ||
                 instr.Opcode == llvm::dwarf::DW_CFA_offset_extended_sf)
        {
            uint64_t reg = instr.Ops[0];
            int64_t cfi_offset = std::abs(static_cast<int64_t>(instr.Ops[1]) * cfis.dataAlign());

            if (reg >= 23 && reg <= 32)
            {
                uint64_t rsp_offset = current_cfa_offset - cfi_offset;
                std::string text = ".seh_savexmm xmm" + std::to_string(reg - 17) + ", " +
                                   std::to_string(rsp_offset);
                directives.push_back({current_offset, text});
                last_prologue_offset = current_offset;
            }
            else
            {
                std::string reg_name = map_dwarf_reg_to_x86_64(reg);
                if (!reg_name.empty())
                {
                    if (last_cfa_offset_change_offset == current_offset)
                    {
                        for (auto it = directives.rbegin(); it != directives.rend(); ++it)
                        {
                            if (it->m_offset != current_offset)
                            {
                                break;
                            }
                            if (it->m_text.rfind(".seh_stackalloc ", 0) == 0)
                            {
                                std::string size_str = it->m_text.substr(16);
                                uint64_t size = std::stoull(size_str);
                                if (size == 8)
                                {
                                    directives.erase(std::prev(it.base()));
                                }
                                else if (size > 8)
                                {
                                    it->m_text = ".seh_stackalloc " + std::to_string(size - 8);
                                }
                                break;
                            }
                        }
                        std::string text = ".seh_pushreg " + reg_name;
                        directives.push_back({current_offset, text});
                    }
                    else
                    {
                        uint64_t rsp_offset = current_cfa_offset - cfi_offset;
                        std::string text =
                            ".seh_savereg " + reg_name + ", " + std::to_string(rsp_offset);
                        directives.push_back({current_offset, text});
                    }
                    last_prologue_offset = current_offset;
                }
            }
        }
        else if (instr.Opcode == llvm::dwarf::DW_CFA_def_cfa_register)
        {
            uint64_t reg = instr.Ops[0];
            std::string reg_name = map_dwarf_reg_to_x86_64(reg);
            if (reg_name == "rbp")
            {
                uint64_t fp_offset = current_cfa_offset - 16;
                std::string text = ".seh_setframe rbp, " + std::to_string(fp_offset);
                directives.push_back({current_offset, text});
                last_prologue_offset = current_offset;
                has_fp = true;
            }
        }
        max_cfa_offset = std::max(max_cfa_offset, current_cfa_offset);
    }

    // On x86_64, the initial CFA offset is 8 because the call instruction pushes the 8-byte
    // return address onto the stack. We subtract 8 from the maximum CFA offset to obtain
    // the actual stack size allocated by the function itself.
    return UnwindInfoResult{directives, max_cfa_offset - 8, has_fp};
}

} // namespace repeat
