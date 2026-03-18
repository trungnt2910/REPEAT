#include "repeat/translator.h"

#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/MemoryBuffer.h>

#include "repeat/translator_impl.h"

namespace repeat
{

namespace ob = llvm::object;

Translator::Translator(const std::string& elf_path, llvm::raw_ostream& warningStream)
    : m_elfPath(elf_path), m_warningStream(warningStream), m_impl(nullptr)
{
}

Translator::~Translator() = default;

std::error_code Translator::Load()
{
    auto buffer_or_err = llvm::MemoryBuffer::getFile(m_elfPath);
    if (!buffer_or_err)
    {
        return buffer_or_err.getError();
    }

    auto elf_or_err = ob::ObjectFile::createELFObjectFile(buffer_or_err.get()->getMemBufferRef());
    if (!elf_or_err)
    {
        std::error_code ec;
        llvm::handleAllErrors(elf_or_err.takeError(),
                              [&](const llvm::ErrorInfoBase& eib)
                              {
                                  ec = std::make_error_code(std::errc::invalid_argument);
                              });
        return ec;
    }

    auto* elf_obj = elf_or_err.get().get();

    if (elf_obj->getBytesInAddress() == 8)
    {
        m_impl =
            std::make_unique<TranslatorImpl<llvm::object::ELF64LE>>(m_elfPath, m_warningStream);
    }
    else if (elf_obj->getBytesInAddress() == 4)
    {
        m_impl =
            std::make_unique<TranslatorImpl<llvm::object::ELF32LE>>(m_elfPath, m_warningStream);
    }
    else
    {
        return std::make_error_code(std::errc::not_supported);
    }

    return m_impl->Load();
}

void Translator::Translate(llvm::raw_ostream& os)
{
    if (m_impl)
    {
        m_impl->Translate(os);
    }
}

llvm::DWARFContext* Translator::GetDwarfContext() const
{
    return m_impl ? m_impl->GetDwarfContext() : nullptr;
}

} // namespace repeat
