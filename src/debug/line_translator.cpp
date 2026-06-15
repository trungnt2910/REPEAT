#include "repeat/debug/line_translator.h"

#include <llvm/ADT/SmallString.h>
#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/DebugInfo/DWARF/DWARFDebugLine.h>
#include <llvm/Support/Path.h>

namespace repeat
{

void LineTranslator::PrepareLineInfo(DebugTranslationContext& ctx)
{
    if (!ctx.m_dwarfCtx)
    {
        return;
    }

    unsigned next_global_file_idx = 1;

    for (const auto& CU : ctx.m_dwarfCtx->compile_units())
    {
        const llvm::DWARFDebugLine::LineTable* lt = ctx.m_dwarfCtx->getLineTableForUnit(CU.get());
        if (!lt)
        {
            continue;
        }

        const char* comp_dir = CU->getCompilationDir();
        auto& local_map = ctx.m_cuLocalToGlobalFile[CU.get()];

        std::optional<uint64_t> last_idx = lt->getLastValidFileIndex();
        if (last_idx)
        {
            uint64_t start_idx = (lt->Prologue.getVersion() >= 5) ? 0 : 1;
            for (uint64_t i = start_idx; i <= *last_idx; ++i)
            {
                std::string full_path;
                if (lt->getFileNameByIndex(
                        i,
                        comp_dir,
                        llvm::DILineInfoSpecifier::FileLineInfoKind::AbsoluteFilePath,
                        full_path))
                {
                    std::string final_path;
                    if (!full_path.empty() && full_path[0] == '/')
                    {
                        // This is a UNIX path.
                        final_path = full_path;
                    }
                    else
                    {
                        // This may be a Windows path.
                        // Prefer backslashes since WinDbg does not understand forward slashes.
                        llvm::SmallString<128> win_path_buf;
                        llvm::sys::path::native(
                            full_path, win_path_buf, llvm::sys::path::Style::windows_backslash);
                        final_path = std::string(win_path_buf.str());
                    }
                    auto it = ctx.m_globalFilePaths.find(final_path);
                    if (it == ctx.m_globalFilePaths.end())
                    {
                        ctx.m_globalFilePaths[final_path] = next_global_file_idx;
                        local_map[i] = next_global_file_idx;
                        next_global_file_idx++;
                    }
                    else
                    {
                        local_map[i] = it->second;
                    }
                }
            }
        }

        for (const auto& row : lt->Rows)
        {
            if (row.EndSequence)
            {
                continue;
            }
            uint64_t addr = row.Address.Address;
            auto file_it = local_map.find(row.File);
            if (file_it != local_map.end())
            {
                ctx.m_addrToLineMap[addr] = {file_it->second, row.Line, row.Column};
            }
        }
    }
}

void LineTranslator::EmitDebugFiles(DebugTranslationContext& ctx, llvm::raw_ostream& os)
{
    os << "# --- REPEAT: CodeView Source Files ---\n";
    for (const auto& pair : ctx.m_globalFilePaths)
    {
        os << "  .cv_file " << pair.second << " \"";
        for (char c : pair.first)
        {
            if (c == '\\')
            {
                os << "\\\\";
            }
            else
            {
                os << c;
            }
        }
        os << "\"\n";
    }
}

void LineTranslator::EmitLineLoc(uint64_t addr,
                                 uint64_t func_start,
                                 DebugTranslationContext& ctx,
                                 llvm::raw_ostream& os)
{
    auto it = ctx.m_addrToLineMap.find(addr);
    if (it != ctx.m_addrToLineMap.end())
    {
        auto func_it = ctx.m_funcToIdMap.find(func_start);
        if (func_it != ctx.m_funcToIdMap.end())
        {
            if (ctx.m_columnInfo)
            {
                os << "  .cv_loc " << func_it->second << " " << it->second.m_fileIdx << " "
                   << it->second.m_line << " " << it->second.m_column << "\n";
            }
            else
            {
                auto& last = ctx.m_lastEmittedLine[func_start];
                if (last.m_fileIdx != it->second.m_fileIdx || last.m_line != it->second.m_line)
                {
                    last = it->second;
                    os << "  .cv_loc " << func_it->second << " " << it->second.m_fileIdx << " "
                       << it->second.m_line << "\n";
                }
            }
        }
    }
}

} // namespace repeat
