#include "repeat/arch/aarch64/aarch64_backend.h"

#include <algorithm>
#include <map>
#include <set>

#include <llvm/BinaryFormat/ELF.h>

namespace repeat
{

std::optional<RelocEntry> Aarch64Backend::ParseRelocation(
    const llvm::object::ELFFile<llvm::object::ELF64LE>& elf,
    const llvm::object::ELFFile<llvm::object::ELF64LE>::Elf_Shdr* symtab_sec,
    llvm::StringRef strtab,
    const llvm::object::ELFFile<llvm::object::ELF64LE>::Elf_Rela& rela)
{
    uint32_t type = rela.getType(false);
    if (type == llvm::ELF::R_AARCH64_RELATIVE)
    {
        return RelocEntry{rela.r_offset, static_cast<uint64_t>(rela.r_addend), ""};
    }
    else if (type == llvm::ELF::R_AARCH64_GLOB_DAT || type == llvm::ELF::R_AARCH64_JUMP_SLOT)
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

std::string map_dwarf_reg_to_arm64(uint64_t reg)
{
    if (reg >= 19 && reg <= 28)
    {
        return "x" + std::to_string(reg);
    }
    if (reg == 29)
    {
        return "x29";
    }
    if (reg == 30)
    {
        return "x30";
    }
    if (reg >= 72 && reg <= 79)
    {
        return "d" + std::to_string(reg - 64);
    }
    return "";
}

} // namespace

namespace repeat
{

struct Aarch64Save
{
    uint64_t m_reg;
    uint64_t m_rspOffset;

    bool operator<(const Aarch64Save& other) const
    {
        // Sort by stack offset to facilitate pairing adjacent registers into
        // double-register save SEH codes (e.g., save_reg_p).
        return m_rspOffset < other.m_rspOffset;
    }
};

UnwindInfoResult Aarch64Backend::TranslateUnwindInfo(const llvm::dwarf::CFIProgram& cfis,
                                                     uint64_t& last_prologue_offset) const
{
    uint64_t current_offset = 0;
    uint64_t current_cfa_offset = 0;
    uint64_t max_cfa_offset = 0;
    std::vector<SehDirective> directives;
    bool has_fp = false;

    std::map<uint64_t, std::vector<Aarch64Save>> saves_at_offset;
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
            uint64_t old_cfa_offset = current_cfa_offset;
            current_cfa_offset = offset;
            if (current_cfa_offset > old_cfa_offset)
            {
                uint64_t alloc_size = current_cfa_offset - old_cfa_offset;
                std::string text = ".seh_stackalloc " + std::to_string(alloc_size);
                other_dirs_at_offset[current_offset].push_back(text);
                last_prologue_offset = current_offset;
            }
            std::string reg_name = map_dwarf_reg_to_arm64(reg);
            if (reg_name == "x29")
            {
                uint64_t fp_offset = current_cfa_offset - offset;
                std::string text = ".seh_frame x29, " + std::to_string(fp_offset);
                other_dirs_at_offset[current_offset].push_back(text);
                last_prologue_offset = current_offset;
                has_fp = true;
            }
        }
        else if (instr.Opcode == llvm::dwarf::DW_CFA_def_cfa_sf)
        {
            uint64_t reg = instr.Ops[0];
            int64_t factored_offset = static_cast<int64_t>(instr.Ops[1]) * cfis.dataAlign();
            uint64_t new_offset = std::abs(factored_offset);
            uint64_t old_cfa_offset = current_cfa_offset;
            current_cfa_offset = new_offset;
            if (current_cfa_offset > old_cfa_offset)
            {
                uint64_t alloc_size = current_cfa_offset - old_cfa_offset;
                std::string text = ".seh_stackalloc " + std::to_string(alloc_size);
                other_dirs_at_offset[current_offset].push_back(text);
                last_prologue_offset = current_offset;
            }
            std::string reg_name = map_dwarf_reg_to_arm64(reg);
            if (reg_name == "x29")
            {
                uint64_t fp_offset = current_cfa_offset - new_offset;
                std::string text = ".seh_frame x29, " + std::to_string(fp_offset);
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
            int64_t cfi_offset = std::abs(static_cast<int64_t>(instr.Ops[1]) * cfis.dataAlign());
            uint64_t rsp_offset = current_cfa_offset - cfi_offset;
            saves_at_offset[current_offset].push_back({reg, rsp_offset});
            last_prologue_offset = current_offset;
        }
        else if (instr.Opcode == llvm::dwarf::DW_CFA_def_cfa_register)
        {
            uint64_t reg = instr.Ops[0];
            std::string reg_name = map_dwarf_reg_to_arm64(reg);
            if (reg_name == "x29")
            {
                uint64_t offset = 16;
                uint64_t fp_offset =
                    (current_cfa_offset >= offset) ? (current_cfa_offset - offset) : 0;
                std::string text = ".seh_frame x29, " + std::to_string(fp_offset);
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
            auto& saves = saves_it->second;
            std::sort(saves.begin(), saves.end());
            for (size_t i = 0; i < saves.size();)
            {
                bool paired = false;
                if (i + 1 < saves.size())
                {
                    const auto& s1 = saves[i];
                    const auto& s2 = saves[i + 1];
                    if (s2.m_rspOffset == s1.m_rspOffset + 8)
                    {
                        std::string name1 = map_dwarf_reg_to_arm64(s1.m_reg);
                        std::string name2 = map_dwarf_reg_to_arm64(s2.m_reg);
                        if (!name1.empty() && !name2.empty())
                        {
                            if (s1.m_reg == 29 && s2.m_reg == 30)
                            {
                                std::string text =
                                    ".seh_save_fplr " + std::to_string(s1.m_rspOffset);
                                directives.push_back({offset, text});
                                paired = true;
                                i += 2;
                            }
                            else if (s1.m_reg >= 72 && s1.m_reg <= 79 && s2.m_reg >= 72 &&
                                     s2.m_reg <= 79)
                            {
                                std::string text = ".seh_save_freg_p " + name1 + ", " + name2 +
                                                   ", " + std::to_string(s1.m_rspOffset);
                                directives.push_back({offset, text});
                                paired = true;
                                i += 2;
                            }
                            else if (s1.m_reg <= 30 && s2.m_reg <= 30)
                            {
                                std::string text = ".seh_save_reg_p " + name1 + ", " + name2 +
                                                   ", " + std::to_string(s1.m_rspOffset);
                                directives.push_back({offset, text});
                                paired = true;
                                i += 2;
                            }
                        }
                    }
                }

                if (!paired)
                {
                    const auto& s = saves[i];
                    std::string name = map_dwarf_reg_to_arm64(s.m_reg);
                    if (!name.empty())
                    {
                        if (s.m_reg >= 72 && s.m_reg <= 79)
                        {
                            std::string text =
                                ".seh_save_freg " + name + ", " + std::to_string(s.m_rspOffset);
                            directives.push_back({offset, text});
                        }
                        else
                        {
                            std::string text =
                                ".seh_save_reg " + name + ", " + std::to_string(s.m_rspOffset);
                            directives.push_back({offset, text});
                        }
                    }
                    i++;
                }
            }
        }
    }

    return UnwindInfoResult{directives, max_cfa_offset, has_fp};
}

} // namespace repeat
