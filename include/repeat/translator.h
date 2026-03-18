#ifndef REPEAT_TRANSLATOR_H
#define REPEAT_TRANSLATOR_H

#include <cstdint>
#include <memory>
#include <string>
#include <system_error>

#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/Support/raw_ostream.h>

namespace repeat
{

struct SegmentInfo
{
    char m_suffix;
    uint64_t m_vaddr;
    uint64_t m_memsz;
};

class TranslatorInstance
{
public:
    virtual ~TranslatorInstance() = default;
    virtual std::error_code Load() = 0;
    virtual void Translate(llvm::raw_ostream& os) = 0;
    virtual llvm::DWARFContext* GetDwarfContext() const = 0;
};

class Translator
{
public:
    Translator(const std::string& elf_path, llvm::raw_ostream& warningStream = llvm::errs());
    ~Translator();

    std::error_code Load();

    void Translate(llvm::raw_ostream& os);

    // Exposed to allow unit tests to verify internal DWARF parsing state.
    llvm::DWARFContext* GetDwarfContext() const;

private:
    std::string m_elfPath;
    llvm::raw_ostream& m_warningStream;
    std::unique_ptr<TranslatorInstance> m_impl;
};

} // namespace repeat

#endif // REPEAT_TRANSLATOR_H
