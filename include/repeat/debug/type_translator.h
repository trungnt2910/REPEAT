#ifndef REPEAT_DEBUG_TYPE_TRANSLATOR_H
#define REPEAT_DEBUG_TYPE_TRANSLATOR_H

#include <llvm/DebugInfo/CodeView/TypeIndex.h>
#include <llvm/DebugInfo/DWARF/DWARFDie.h>

#include "repeat/debug/debug_context.h"

namespace repeat
{

class TypeTranslator
{
public:
    static llvm::codeview::TypeIndex TranslateType(const llvm::DWARFDie& die,
                                                   DebugTranslationContext& ctx);
};

} // namespace repeat

#endif // REPEAT_DEBUG_TYPE_TRANSLATOR_H
