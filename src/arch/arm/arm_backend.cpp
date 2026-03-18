#include "repeat/arch/arm/arm_backend.h"

#include <algorithm>
#include <map>
#include <set>

#include <llvm/BinaryFormat/ELF.h>

namespace repeat
{

std::optional<RelocEntry> ArmBackend::ParseRelocationRel(
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

    uint32_t raw_addend =
        *reinterpret_cast<const uint32_t*>(relocated_contents.data() + offset_in_sec);
    uint64_t addend = static_cast<uint64_t>(raw_addend);

    if (type == llvm::ELF::R_ARM_RELATIVE)
    {
        return RelocEntry{rel.r_offset, addend, ""};
    }
    else if (type == llvm::ELF::R_ARM_GLOB_DAT || type == llvm::ELF::R_ARM_JUMP_SLOT)
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

} // namespace repeat

namespace
{

std::string map_dwarf_reg_to_arm32(uint64_t reg)
{
    if (reg <= 12)
    {
        return "r" + std::to_string(reg);
    }
    if (reg == 14)
    {
        return "lr";
    }
    if (reg >= 256 && reg <= 287)
    {
        return "d" + std::to_string(reg - 256);
    }
    return "";
}

} // namespace

namespace repeat
{

UnwindInfoResult ArmBackend::TranslateUnwindInfo(const llvm::dwarf::CFIProgram& cfis,
                                                 uint64_t& last_prologue_offset) const
{
    uint64_t current_offset = 0;
    uint64_t current_cfa_offset = 0;
    uint64_t max_cfa_offset = 0;
    std::vector<SehDirective> directives;
    bool has_fp = false;

    std::map<uint64_t, std::vector<uint64_t>> saves_at_offset;
    std::map<uint64_t, std::vector<std::string>> other_dirs_at_offset;

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
            if (current_cfa_offset > 0)
            {
                std::string text = ".seh_stackalloc " + std::to_string(current_cfa_offset);
                other_dirs_at_offset[current_offset].push_back(text);
                last_prologue_offset = current_offset;
            }
        }
        else if (instr.Opcode == llvm::dwarf::DW_CFA_def_cfa_offset_sf)
        {
            int64_t factored_offset = static_cast<int64_t>(instr.Ops[0]) * cfis.dataAlign();
            current_cfa_offset = std::abs(factored_offset);
            if (current_cfa_offset > 0)
            {
                std::string text = ".seh_stackalloc " + std::to_string(current_cfa_offset);
                other_dirs_at_offset[current_offset].push_back(text);
                last_prologue_offset = current_offset;
            }
        }
        else if (instr.Opcode == llvm::dwarf::DW_CFA_def_cfa)
        {
            uint64_t reg = instr.Ops[0];
            uint64_t offset = instr.Ops[1];
            current_cfa_offset = offset;
            std::string reg_name = map_dwarf_reg_to_arm32(reg);
            if (reg_name == "r11")
            {
                uint64_t fp_offset = current_cfa_offset - offset;
                std::string text = ".seh_frame r11, " + std::to_string(fp_offset);
                other_dirs_at_offset[current_offset].push_back(text);
                last_prologue_offset = current_offset;
                has_fp = true;
            }
        }
        else if (instr.Opcode == llvm::dwarf::DW_CFA_def_cfa_sf)
        {
            uint64_t reg = instr.Ops[0];
            int64_t factored_offset = static_cast<int64_t>(instr.Ops[1]) * cfis.dataAlign();
            current_cfa_offset = std::abs(factored_offset);
            std::string reg_name = map_dwarf_reg_to_arm32(reg);
            if (reg_name == "r11")
            {
                uint64_t fp_offset = current_cfa_offset - std::abs(factored_offset);
                std::string text = ".seh_frame r11, " + std::to_string(fp_offset);
                other_dirs_at_offset[current_offset].push_back(text);
                last_prologue_offset = current_offset;
                has_fp = true;
            }
        }
        else if (instr.Opcode == llvm::dwarf::DW_CFA_offset ||
                 instr.Opcode == llvm::dwarf::DW_CFA_offset_extended ||
                 instr.Opcode == llvm::dwarf::DW_CFA_offset_extended_sf)
        {
            uint64_t reg = instr.Ops[0];
            saves_at_offset[current_offset].push_back(reg);
            last_prologue_offset = current_offset;
        }
        else if (instr.Opcode == llvm::dwarf::DW_CFA_def_cfa_register)
        {
            uint64_t reg = instr.Ops[0];
            std::string reg_name = map_dwarf_reg_to_arm32(reg);
            if (reg_name == "r11")
            {
                uint64_t offset = 8;
                uint64_t fp_offset =
                    (current_cfa_offset >= offset) ? (current_cfa_offset - offset) : 0;
                std::string text = ".seh_frame r11, " + std::to_string(fp_offset);
                other_dirs_at_offset[current_offset].push_back(text);
                last_prologue_offset = current_offset;
                has_fp = true;
            }
        }
        max_cfa_offset = std::max(max_cfa_offset, current_cfa_offset);
    }

    std::set<uint64_t> all_offsets;
    for (const auto& pair : saves_at_offset)
    {
        all_offsets.insert(pair.first);
    }
    for (const auto& pair : other_dirs_at_offset)
    {
        all_offsets.insert(pair.first);
    }

    for (uint64_t offset : all_offsets)
    {
        auto other_it = other_dirs_at_offset.find(offset);
        if (other_it != other_dirs_at_offset.end())
        {
            for (const auto& text : other_it->second)
            {
                directives.push_back({offset, text});
            }
        }

        auto saves_it = saves_at_offset.find(offset);
        if (saves_it != saves_at_offset.end() && !saves_it->second.empty())
        {
            std::vector<uint64_t> int_regs;
            std::vector<uint64_t> fp_regs;
            for (uint64_t reg : saves_it->second)
            {
                if (reg >= 256 && reg <= 287)
                {
                    fp_regs.push_back(reg);
                }
                else
                {
                    int_regs.push_back(reg);
                }
            }

            if (!int_regs.empty())
            {
                std::sort(int_regs.begin(), int_regs.end());
                std::string reg_list = "{";
                bool first = true;
                for (uint64_t reg : int_regs)
                {
                    std::string name = map_dwarf_reg_to_arm32(reg);
                    if (!name.empty())
                    {
                        if (!first)
                        {
                            reg_list += ", ";
                        }
                        reg_list += name;
                        first = false;
                    }
                }
                reg_list += "}";
                if (reg_list != "{}")
                {
                    uint64_t save_size = int_regs.size() * 4;
                    if (!directives.empty() && directives.back().m_offset == offset &&
                        directives.back().m_text.rfind(".seh_stackalloc ", 0) == 0)
                    {
                        std::string size_str = directives.back().m_text.substr(16);
                        uint64_t size = std::stoull(size_str);
                        if (size == save_size)
                        {
                            directives.pop_back();
                        }
                        else if (size > save_size)
                        {
                            directives.back().m_text =
                                ".seh_stackalloc " + std::to_string(size - save_size);
                        }
                    }
                    std::string text = ".seh_save_regs " + reg_list;
                    directives.push_back({offset, text});
                }
            }

            if (!fp_regs.empty())
            {
                std::sort(fp_regs.begin(), fp_regs.end());
                std::string reg_list = "{";
                bool first = true;
                for (uint64_t reg : fp_regs)
                {
                    std::string name = map_dwarf_reg_to_arm32(reg);
                    if (!name.empty())
                    {
                        if (!first)
                        {
                            reg_list += ", ";
                        }
                        reg_list += name;
                        first = false;
                    }
                }
                reg_list += "}";
                if (reg_list != "{}")
                {
                    uint64_t save_size = fp_regs.size() * 8;
                    if (!directives.empty() && directives.back().m_offset == offset &&
                        directives.back().m_text.rfind(".seh_stackalloc ", 0) == 0)
                    {
                        std::string size_str = directives.back().m_text.substr(16);
                        uint64_t size = std::stoull(size_str);
                        if (size == save_size)
                        {
                            directives.pop_back();
                        }
                        else if (size > save_size)
                        {
                            directives.back().m_text =
                                ".seh_stackalloc " + std::to_string(size - save_size);
                        }
                    }
                    std::string text = ".seh_save_fregs " + reg_list;
                    directives.push_back({offset, text});
                }
            }
        }
    }

    return UnwindInfoResult{directives, max_cfa_offset, has_fp};
}

} // namespace repeat
