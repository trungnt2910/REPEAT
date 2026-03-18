#ifndef REPEAT_DEBUG_VAR_TRANSLATOR_H
#define REPEAT_DEBUG_VAR_TRANSLATOR_H

#include <llvm/DebugInfo/DWARF/DWARFDie.h>
#include <llvm/Support/raw_ostream.h>

#include "repeat/debug/debug_context.h"

namespace repeat
{

class VarTranslator
{
public:
    // Translates DWARF global variable definitions into CodeView-compatible
    // GDATA32/LDATA32 debug symbol records to enable global variable inspection in debuggers.
    static void TranslateGlobalVars(DebugTranslationContext& ctx, llvm::raw_ostream& os);

    // Translates DWARF local variables and parameters within a function scope into
    // CodeView scope symbols (BPRelativeSym/RegisterSym) to enable local variable inspection.
    static void TranslateLocalVars(const llvm::DWARFDie& func_die,
                                   uint64_t func_size,
                                   DebugTranslationContext& ctx,
                                   llvm::raw_ostream& os);
};

} // namespace repeat

#endif // REPEAT_DEBUG_VAR_TRANSLATOR_H
