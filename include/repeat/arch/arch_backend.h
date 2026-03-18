#ifndef REPEAT_ARCH_ARCH_BACKEND_H
#define REPEAT_ARCH_ARCH_BACKEND_H

#include <optional>
#include <string>
#include <vector>

#include <llvm/DebugInfo/DWARF/LowLevel/DWARFCFIProgram.h>
#include <llvm/Object/ELF.h>
#include <llvm/Object/ObjectFile.h>

namespace repeat
{

struct RelocEntry
{
    uint64_t m_offset;
    uint64_t m_addend;
    std::string m_symName;
};

struct SehDirective
{
    uint64_t m_offset;
    std::string m_text;
};

struct UnwindInfoResult
{
    std::vector<SehDirective> m_directives;
    uint64_t m_frameSize = 0;
    bool m_hasFramePointer = false;
};

template <typename ELFT>
class ArchBackend
{
public:
    virtual ~ArchBackend() = default;

    virtual bool SupportsSeh() const
    {
        return false;
    }

    virtual UnwindInfoResult TranslateUnwindInfo(const llvm::dwarf::CFIProgram& cfis,
                                                 uint64_t& last_prologue_offset) const
    {
        return {};
    }

    // Supporting multiple architectures requires unifying different RELA relocation formats
    // so the main translation loop can process them generically.
    virtual std::optional<RelocEntry> ParseRelocation(const llvm::object::ELFFile<ELFT>& elf,
                                                      const typename ELFT::Shdr* symtab_sec,
                                                      llvm::StringRef strtab,
                                                      const typename ELFT::Rela& rela) = 0;

    // Supporting multiple architectures requires unifying different REL relocation formats
    // and handling implicit addends so the main translation loop can process them generically.
    virtual std::optional<RelocEntry>
    ParseRelocationRel(const llvm::object::ELFFile<ELFT>& elf,
                       const typename ELFT::Shdr* symtab_sec,
                       llvm::StringRef strtab,
                       const typename ELFT::Rel& rel,
                       uint64_t relocated_sec_addr,
                       llvm::ArrayRef<uint8_t> relocated_contents) = 0;
};

} // namespace repeat

#endif // REPEAT_ARCH_ARCH_BACKEND_H
