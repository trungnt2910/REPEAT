#ifndef REPEAT_DEBUG_DEBUG_CONTEXT_H
#define REPEAT_DEBUG_DEBUG_CONTEXT_H

#include <map>
#include <string>
#include <vector>

#include <llvm/DebugInfo/CodeView/MergingTypeTableBuilder.h>
#include <llvm/DebugInfo/CodeView/TypeIndex.h>
#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/Allocator.h>

#include "repeat/arch/arch_backend.h"

namespace repeat
{

struct LineInfo
{
    unsigned m_fileIdx;
    unsigned m_line;
    unsigned m_column;
};

struct DebugTranslationContext
{
    llvm::BumpPtrAllocator m_typeBuilderAllocator;
    llvm::codeview::MergingTypeTableBuilder m_typeBuilder;

    llvm::DWARFContext* m_dwarfCtx;
    const llvm::object::ObjectFile* m_elfObj;

    // Caches translated types to ensure type deduplication and resolve cyclic type references
    // during DWARF to CodeView conversion.
    std::map<uint64_t, llvm::codeview::TypeIndex> m_typeMap;

    // CodeView requires a single global file table, whereas DWARF defines them per compilation
    // unit. This maps source file paths to global IDs to unify them.
    std::map<std::string, unsigned> m_globalFilePaths;

    // Debuggers require mapping instruction addresses back to source locations,
    // which is used to generate CodeView line number tables (.cv_loc).
    std::map<uint64_t, LineInfo> m_addrToLineMap;

    // DWARF CU local file indexes must be mapped to the global CodeView file table index
    // during translation to maintain correct file references.
    std::map<const llvm::DWARFUnit*, std::map<uint64_t, unsigned>> m_cuLocalToGlobalFile;

    // Pre-computed exception handling directives, cached to allow inline emission of .seh_*
    // directives during code translation.
    std::map<uint64_t, std::vector<SehDirective>> m_unwindInfo;

    // Maps function start address to its stack frame size.
    std::map<uint64_t, uint64_t> m_frameSizes;

    // Maps function start address to whether it has a frame pointer.
    std::map<uint64_t, bool> m_hasFramePointer;

    // Tracks where function prologues end, used to determine correct offsets for SEH frame setup.
    std::map<uint64_t, uint64_t> m_prologueEnds;

    // Maps symbol virtual addresses to their unique names (including renamed local symbols)
    // to allow CodeView relocations (.secrel32/.secidx) to reference them correctly.
    std::map<uint64_t, std::string> m_symbolNames;

    // Maps function start address to its CodeView FunctionId.
    std::map<uint64_t, unsigned> m_funcToIdMap;

    // Stream for emitting translation warnings.
    llvm::raw_ostream& m_warningStream;

    DebugTranslationContext(llvm::DWARFContext* dwarf,
                            const llvm::object::ObjectFile* elf,
                            llvm::raw_ostream& warningStream)
        : m_typeBuilder(m_typeBuilderAllocator),
          m_dwarfCtx(dwarf),
          m_elfObj(elf),
          m_warningStream(warningStream)
    {
    }
};

} // namespace repeat

#endif // REPEAT_DEBUG_DEBUG_CONTEXT_H
