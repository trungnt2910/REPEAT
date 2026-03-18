#ifndef REPEAT_DEBUG_UNWIND_TRANSLATOR_H
#define REPEAT_DEBUG_UNWIND_TRANSLATOR_H

#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/DebugInfo/DWARF/DWARFDebugFrame.h>
#include <llvm/Support/raw_ostream.h>

#include "repeat/arch/arch_backend.h"
#include "repeat/debug/debug_context.h"

namespace repeat
{

class UnwindTranslator
{
public:
    // Pre-translates DWARF unwind tables (.eh_frame/.debug_frame) into architecture-specific
    // SEH directives, mapping them to function start addresses for quick lookup.
    template <typename ELFT>
    static void PrepareUnwindInfo(DebugTranslationContext& ctx, ArchBackend<ELFT>* backend);

    // Marks the start of a structured exception handling (SEH) frame for the function,
    // which is required for stack unwinding on Windows/PE targets.
    static void EmitSehProcBegin(uint64_t func_start,
                                 llvm::StringRef func_name,
                                 DebugTranslationContext& ctx,
                                 llvm::raw_ostream& os);

    // Emits the recorded SEH directives that correspond to the current offset in the
    // function's prologue, ensuring correct stack layout registration.
    static void EmitSehPrologue(uint64_t func_start,
                                uint64_t current_offset,
                                DebugTranslationContext& ctx,
                                llvm::raw_ostream& os);

    // Required to close the exception handling scope defined by .seh_proc, signaling the end
    // of the function's unwinding metadata block to the OS.
    static void
    EmitSehProcEnd(uint64_t func_start, DebugTranslationContext& ctx, llvm::raw_ostream& os);
};

template <typename ELFT>
void UnwindTranslator::PrepareUnwindInfo(DebugTranslationContext& ctx, ArchBackend<ELFT>* backend)
{
    if (!ctx.m_dwarfCtx || !backend)
    {
        return;
    }

    const llvm::DWARFDebugFrame* eh_frame = nullptr;
    auto eh_frame_or_err = ctx.m_dwarfCtx->getEHFrame();
    if (eh_frame_or_err && !(*eh_frame_or_err)->empty())
    {
        eh_frame = *eh_frame_or_err;
    }
    else
    {
        if (!eh_frame_or_err)
        {
            llvm::consumeError(eh_frame_or_err.takeError());
        }
        auto debug_frame_or_err = ctx.m_dwarfCtx->getDebugFrame();
        if (debug_frame_or_err && !(*debug_frame_or_err)->empty())
        {
            eh_frame = *debug_frame_or_err;
        }
        else
        {
            if (!debug_frame_or_err)
            {
                llvm::consumeError(debug_frame_or_err.takeError());
            }
            return;
        }
    }

    for (const auto& entry : eh_frame->entries())
    {
        if (entry.getKind() != llvm::dwarf::FrameEntry::FK_FDE)
        {
            continue;
        }
        const auto* fde = static_cast<const llvm::dwarf::FDE*>(&entry);
        uint64_t func_start = fde->getInitialLocation();

        const llvm::dwarf::CFIProgram& cfis = fde->cfis();
        uint64_t last_prologue_offset = 0;
        UnwindInfoResult result = backend->TranslateUnwindInfo(cfis, last_prologue_offset);

        ctx.m_frameSizes[func_start] = result.m_frameSize;
        ctx.m_hasFramePointer[func_start] = result.m_hasFramePointer;

        if (!result.m_directives.empty())
        {
            ctx.m_unwindInfo[func_start] = result.m_directives;
            ctx.m_prologueEnds[func_start] = last_prologue_offset;
        }
    }
}

} // namespace repeat

#endif // REPEAT_DEBUG_UNWIND_TRANSLATOR_H
