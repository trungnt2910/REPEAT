#include "repeat/debug/unwind_translator.h"

#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/DebugInfo/DWARF/DWARFDebugFrame.h>

namespace repeat
{

void UnwindTranslator::EmitSehProcBegin(uint64_t func_start,
                                        llvm::StringRef func_name,
                                        DebugTranslationContext& ctx,
                                        llvm::raw_ostream& os)
{
    auto it = ctx.m_unwindInfo.find(func_start);
    if (it != ctx.m_unwindInfo.end())
    {
        os << ".seh_proc " << func_name << "\n";
    }
}

void UnwindTranslator::EmitSehPrologue(uint64_t func_start,
                                       uint64_t current_offset,
                                       DebugTranslationContext& ctx,
                                       llvm::raw_ostream& os)
{
    auto it = ctx.m_unwindInfo.find(func_start);
    if (it == ctx.m_unwindInfo.end())
    {
        return;
    }

    const auto& directives = it->second;
    for (const auto& dir : directives)
    {
        if (dir.m_offset == current_offset)
        {
            os << dir.m_text << "\n";
        }
    }

    auto end_it = ctx.m_prologueEnds.find(func_start);
    if (end_it != ctx.m_prologueEnds.end() && end_it->second == current_offset)
    {
        os << ".seh_endprologue\n";
    }
}

void UnwindTranslator::EmitSehProcEnd(uint64_t func_start,
                                      DebugTranslationContext& ctx,
                                      llvm::raw_ostream& os)
{
    auto it = ctx.m_unwindInfo.find(func_start);
    if (it != ctx.m_unwindInfo.end())
    {
        os << ".seh_endproc\n";
    }
}

} // namespace repeat
