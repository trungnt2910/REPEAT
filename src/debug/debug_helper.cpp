#include "repeat/debug/debug_helper.h"

#include <llvm/DebugInfo/CodeView/CVRecord.h>
#include <llvm/Support/Alignment.h>

namespace repeat
{

void DebugHelper::EmitSymbolBytesToAssembly(llvm::raw_ostream& os,
                                            const llvm::codeview::CVSymbol& sym)
{
    llvm::ArrayRef<uint8_t> data = sym.data();
    uint32_t total_size = data.size();
    uint32_t padding = llvm::offsetToAlignment(total_size, llvm::Align(4));
    uint32_t padded_size = total_size + padding;

    os << "  # Symbol Record: Kind=" << (int)sym.kind() << ", Length=" << padded_size << "\n";

    uint16_t record_len_field = padded_size - 2;
    os << "  .byte " << (int)(record_len_field & 0xFF) << "\n";
    os << "  .byte " << (int)((record_len_field >> 8) & 0xFF) << "\n";

    for (size_t i = 2; i < data.size(); i++)
    {
        os << "  .byte " << (int)data[i] << "\n";
    }

    for (uint32_t p = 0; p < padding; p++)
    {
        os << "  .byte 0\n";
    }
}

} // namespace repeat
