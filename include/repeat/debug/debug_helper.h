#ifndef REPEAT_DEBUG_DEBUG_HELPER_H
#define REPEAT_DEBUG_DEBUG_HELPER_H

#include <llvm/DebugInfo/CodeView/CVRecord.h>
#include <llvm/Support/raw_ostream.h>

namespace repeat
{

class DebugHelper
{
public:
    static void EmitSymbolBytesToAssembly(llvm::raw_ostream& os,
                                          const llvm::codeview::CVSymbol& sym);
};

} // namespace repeat

#endif // REPEAT_DEBUG_DEBUG_HELPER_H
