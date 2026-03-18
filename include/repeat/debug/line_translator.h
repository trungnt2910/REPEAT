#ifndef REPEAT_DEBUG_LINE_TRANSLATOR_H
#define REPEAT_DEBUG_LINE_TRANSLATOR_H

#include <llvm/Support/raw_ostream.h>

#include "repeat/debug/debug_context.h"

namespace repeat
{

class LineTranslator
{
public:
    // Pre-processes DWARF line info to build a fast address-to-line lookup map,
    // avoiding expensive DWARF queries during the main translation pass.
    static void PrepareLineInfo(DebugTranslationContext& ctx);

    // Registers all referenced source files with the assembler to enable
    // source-level debugging in the output assembly.
    static void EmitDebugFiles(DebugTranslationContext& ctx, llvm::raw_ostream& os);

    // Emits location metadata for the current address to map the generated
    // instruction back to its original source line.
    static void EmitLineLoc(uint64_t addr,
                            uint64_t func_start,
                            DebugTranslationContext& ctx,
                            llvm::raw_ostream& os);
};

} // namespace repeat

#endif // REPEAT_DEBUG_LINE_TRANSLATOR_H
