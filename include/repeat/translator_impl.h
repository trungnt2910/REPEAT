#ifndef REPEAT_TRANSLATOR_IMPL_H
#define REPEAT_TRANSLATOR_IMPL_H

#include <map>
#include <vector>

#include <llvm/BinaryFormat/ELF.h>
#include <llvm/DebugInfo/CodeView/SymbolRecord.h>
#include <llvm/DebugInfo/CodeView/SymbolSerializer.h>
#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/DebugInfo/DWARF/DWARFDebugFrame.h>
#include <llvm/Object/ELF.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Support/Format.h>
#include <llvm/Support/MemoryBuffer.h>

#include "repeat/arch/arch_backend_factory.h"
#include "repeat/debug/debug_context.h"
#include "repeat/debug/debug_helper.h"
#include "repeat/debug/line_translator.h"
#include "repeat/debug/type_translator.h"
#include "repeat/debug/unwind_translator.h"
#include "repeat/debug/var_translator.h"
#include "repeat/translator.h"

namespace repeat
{

namespace ob = llvm::object;

template <typename ELFT>
class TranslatorImpl : public TranslatorInstance
{
public:
    TranslatorImpl(const std::string& elf_path, bool columnInfo, llvm::raw_ostream& warningStream)
        : m_elfPath(elf_path),
          m_columnInfo(columnInfo),
          m_warningStream(warningStream),
          m_elfBuffer(nullptr),
          m_elfObj(nullptr),
          m_dwarfCtx(nullptr),
          m_backend(nullptr)
    {
    }

    ~TranslatorImpl() override = default;

    std::error_code Load() override
    {
        auto buffer_or_err = llvm::MemoryBuffer::getFile(m_elfPath);
        if (!buffer_or_err)
        {
            return buffer_or_err.getError();
        }
        m_elfBuffer = std::move(buffer_or_err.get());

        auto elf_or_err = ob::ObjectFile::createELFObjectFile(m_elfBuffer->getMemBufferRef());
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
        m_elfObj = std::move(elf_or_err.get());
        m_dwarfCtx = llvm::DWARFContext::create(*m_elfObj);

        llvm::Triple::ArchType arch = m_elfObj->getArch();
        m_backend = ArchBackendFactory::Create<ELFT>(arch);
        if (!m_backend)
        {
            return std::make_error_code(std::errc::not_supported);
        }

        using ELFObjectType = ob::ELFObjectFile<ELFT>;
        auto* elf_obj_derived = llvm::dyn_cast<ELFObjectType>(m_elfObj.get());
        if (!elf_obj_derived)
        {
            return std::make_error_code(std::errc::not_supported);
        }

        const ob::ELFFile<ELFT>& elf = elf_obj_derived->getELFFile();
        auto phdrs_or_err = elf.program_headers();
        if (!phdrs_or_err)
        {
            std::error_code ec;
            llvm::handleAllErrors(phdrs_or_err.takeError(),
                                  [&](const llvm::ErrorInfoBase& eib)
                                  {
                                      ec = std::make_error_code(std::errc::invalid_argument);
                                  });
            return ec;
        }

        char suffix = 'A';
        for (const auto& phdr : *phdrs_or_err)
        {
            if (phdr.p_type == llvm::ELF::PT_LOAD)
            {
                m_segments.push_back({suffix,
                                      static_cast<uint64_t>(phdr.p_vaddr),
                                      static_cast<uint64_t>(phdr.p_memsz)});
                suffix++;
            }
        }

        for (const ob::SymbolRef& sym : m_elfObj->symbols())
        {
            auto type_or_err = sym.getType();
            if (type_or_err && *type_or_err == ob::SymbolRef::Type::ST_Function)
            {
                auto name_or_err = sym.getName();
                auto addr_or_err = sym.getAddress();
                if (name_or_err && addr_or_err)
                {
                    ob::ELFSymbolRef elf_sym(sym);
                    auto flags_or_err = sym.getFlags();
                    bool is_global = flags_or_err && (*flags_or_err & ob::SymbolRef::SF_Global);
                    std::string sym_name(*name_or_err);
                    std::string display_name = sym_name;
                    if (!is_global)
                    {
                        sym_name = sym_name + "." + std::to_string(*addr_or_err);
                    }
                    m_elfFunctions[*addr_or_err] = {
                        sym_name, display_name, elf_sym.getSize(), is_global};
                }
            }
        }

        if (m_dwarfCtx)
        {
            // Force parsing of the entire DWARF context by dumping to a null stream.
            // This is required because LLVM's DWARF lazy parsing does not always resolve
            // cross-references correctly during subsequent on-demand translation steps.
            llvm::DIDumpOptions DumpOpts;
            DumpOpts.DumpType = llvm::DIDT_DebugInfo;
            m_dwarfCtx->dump(llvm::nulls(), DumpOpts);

            m_debugCtx = std::make_unique<DebugTranslationContext>(
                m_dwarfCtx.get(), m_elfObj.get(), m_columnInfo, m_warningStream);

            unsigned func_id = 0;
            for (const auto& pair : m_elfFunctions)
            {
                m_debugCtx->m_funcToIdMap[pair.first] = func_id++;
            }

            for (const ob::SymbolRef& sym : m_elfObj->symbols())
            {
                auto flags_or_err = sym.getFlags();
                if (!flags_or_err)
                {
                    continue;
                }
                uint32_t flags = flags_or_err.get();
                if (flags & ob::SymbolRef::SF_Undefined)
                {
                    continue;
                }

                auto addr_or_err = sym.getAddress();
                if (!addr_or_err)
                {
                    continue;
                }
                uint64_t addr = addr_or_err.get();

                auto name_or_err = sym.getName();
                if (!name_or_err)
                {
                    continue;
                }
                llvm::StringRef name = name_or_err.get();
                if (name.empty())
                {
                    continue;
                }

                auto type_or_err = sym.getType();
                if (!type_or_err)
                {
                    continue;
                }
                ob::SymbolRef::Type type = type_or_err.get();

                if (flags & ob::SymbolRef::SF_Global)
                {
                    m_debugCtx->m_symbolNames[addr] = name.str();
                }
                else
                {
                    if (type == ob::SymbolRef::Type::ST_Function)
                    {
                        auto func_it = m_elfFunctions.find(addr);
                        if (func_it != m_elfFunctions.end())
                        {
                            m_debugCtx->m_symbolNames[addr] = func_it->second.m_name;
                        }
                    }
                    else
                    {
                        m_debugCtx->m_symbolNames[addr] = name.str();
                    }
                }
            }

            LineTranslator::PrepareLineInfo(*m_debugCtx);
            UnwindTranslator::PrepareUnwindInfo(*m_debugCtx, m_backend.get());
        }

        return std::error_code();
    }

    void Translate(llvm::raw_ostream& os) override
    {
        EmitHeaders(os);
        if (m_debugCtx)
        {
            LineTranslator::EmitDebugFiles(*m_debugCtx, os);
        }
        EmitSections(os);
        EmitSymbols(os);
        if (m_debugCtx)
        {
            EmitCodeviewSymbolTable(os);
            EmitCodeviewTypeTable(os);
        }
    }

    llvm::DWARFContext* GetDwarfContext() const override
    {
        return m_dwarfCtx.get();
    }

private:
    std::string m_elfPath;
    bool m_columnInfo;
    llvm::raw_ostream& m_warningStream;
    std::unique_ptr<llvm::MemoryBuffer> m_elfBuffer;
    std::unique_ptr<llvm::object::ObjectFile> m_elfObj;
    std::unique_ptr<llvm::DWARFContext> m_dwarfCtx;
    std::vector<SegmentInfo> m_segments;
    std::unique_ptr<ArchBackend<ELFT>> m_backend;

    struct FunctionSymbol
    {
        std::string m_name;
        std::string m_displayName;
        uint64_t m_size;
        bool m_isGlobal;
    };
    std::map<uint64_t, FunctionSymbol> m_elfFunctions;
    std::unique_ptr<DebugTranslationContext> m_debugCtx;

    llvm::DWARFDie FindDwarfFunctionDie(uint64_t addr)
    {
        if (!m_dwarfCtx)
        {
            return llvm::DWARFDie();
        }
        for (const auto& CU : m_dwarfCtx->compile_units())
        {
            llvm::DWARFDie cu_die = CU->getUnitDIE();
            for (const auto& child : cu_die.children())
            {
                if (child.getTag() == llvm::dwarf::DW_TAG_subprogram)
                {
                    auto low_pc_opt = child.find(llvm::dwarf::DW_AT_low_pc);
                    if (low_pc_opt)
                    {
                        uint64_t low_pc = low_pc_opt->getAsAddress().value_or(0);
                        if (low_pc == addr)
                        {
                            return child;
                        }
                    }
                    else
                    {
                        auto ranges_or_err = child.getAddressRanges();
                        if (ranges_or_err)
                        {
                            for (const auto& range : *ranges_or_err)
                            {
                                if (range.LowPC == addr)
                                {
                                    return child;
                                }
                            }
                        }
                        else
                        {
                            llvm::consumeError(ranges_or_err.takeError());
                        }
                    }
                }
            }
        }
        return llvm::DWARFDie();
    }

    llvm::codeview::CPUType MapElfMachineToCvCpu(llvm::Triple::ArchType arch)
    {
        switch (arch)
        {
        case llvm::Triple::x86_64:
            return llvm::codeview::CPUType::X64;
        case llvm::Triple::x86:
            return llvm::codeview::CPUType::Intel80386;
        case llvm::Triple::aarch64:
            return llvm::codeview::CPUType::ARM64;
        case llvm::Triple::arm:
            return llvm::codeview::CPUType::ARMNT;
        default:
            return llvm::codeview::CPUType::Unknown;
        }
    }

    void EmitCodeviewTypeTable(llvm::raw_ostream& os)
    {
        if (!m_debugCtx || m_debugCtx->m_typeBuilder.empty())
        {
            return;
        }

        os << "\n# --- REPEAT: CodeView Type Table (.debug$T) ---\n";
        os << ".section .debug$T,\"r\"\n";
        os << "  .long 4 # CodeView signature\n";

        m_debugCtx->m_typeBuilder.ForEachRecord(
            [&](llvm::codeview::TypeIndex ti, const llvm::codeview::CVType& type)
            {
                llvm::ArrayRef<uint8_t> data = type.data();
                os << "  # TypeIndex=" << ti.getIndex() << ", Length=" << data.size()
                   << ", Leaf=" << (int)type.kind() << "\n";
                for (size_t i = 0; i < data.size(); i++)
                {
                    os << "  .byte " << (int)data[i] << "\n";
                }
            });
    }

    void EmitCodeviewSymbolTable(llvm::raw_ostream& os)
    {
        if (!m_debugCtx)
        {
            return;
        }

        os << "\n# --- REPEAT: CodeView Symbol Table (.debug$S) ---\n";
        os << ".section .debug$S,\"r\"\n";
        os << "  .long 4 # CodeView signature\n";

        os << "  .long 0xf1 # Symbols subsection kind\n";
        os << "  .long .Lsym_end - .Lsym_begin # length of subsection\n";
        os << ".Lsym_begin:\n";

        llvm::codeview::Compile3Sym compile_sym(llvm::codeview::SymbolRecordKind::Compile3Sym);
        compile_sym.Machine = MapElfMachineToCvCpu(m_elfObj->getArch());
        compile_sym.setLanguage(llvm::codeview::SourceLanguage::Link);
        // TODO: Calculate version information from source control.
        compile_sym.VersionFrontendMajor = 0;
        compile_sym.Version = "REPEAT";

        llvm::codeview::CVSymbol cvs = llvm::codeview::SymbolSerializer::writeOneSymbol(
            compile_sym,
            m_debugCtx->m_typeBuilderAllocator,
            llvm::codeview::CodeViewContainer::ObjectFile);
        DebugHelper::EmitSymbolBytesToAssembly(os, cvs);
        os << "  .p2align 2, 0\n";

        VarTranslator::TranslateGlobalVars(*m_debugCtx, os);
        os << "  .p2align 2, 0\n";

        for (const auto& pair : m_elfFunctions)
        {
            llvm::StringRef func_name = pair.second.m_name;
            llvm::StringRef display_name = pair.second.m_displayName;
            uint64_t func_start = pair.first;
            uint64_t func_size = pair.second.m_size;

            uint64_t prologue_size = 0;
            auto prog_it = m_debugCtx->m_prologueEnds.find(func_start);
            if (prog_it != m_debugCtx->m_prologueEnds.end())
            {
                prologue_size = prog_it->second;
            }

            llvm::DWARFDie func_die = FindDwarfFunctionDie(func_start);

            llvm::codeview::TypeIndex func_type_idx = llvm::codeview::TypeIndex::None();
            if (func_die)
            {
                func_type_idx = TypeTranslator::TranslateType(func_die, *m_debugCtx);
            }

            uint16_t proc_kind = static_cast<uint16_t>(
                pair.second.m_isGlobal ? llvm::codeview::SymbolRecordKind::GlobalProcIdSym
                                       : llvm::codeview::SymbolRecordKind::ProcIdSym);
            std::string proc_comment = pair.second.m_isGlobal ? "S_GPROC32_ID" : "S_LPROC32_ID";

            os << "  # Function Symbol: " << func_name << "\n";
            os << ".Lfunc_sym_begin_" << func_name << ":\n";
            os << "  .short .Lfunc_sym_end_" << func_name << " - .Lfunc_sym_begin_" << func_name
               << " - 2\n";
            os << "  .short " << llvm::format("0x%04x", proc_kind) << " # " << proc_comment << "\n";
            os << "  .long 0 # Parent\n";
            os << "  .long .Lfunc_scope_end_record_" << func_name
               << " - .Lsym_begin # End offset\n";
            os << "  .long 0 # Next\n";
            os << "  .long " << func_size << " # CodeSize\n";
            os << "  .long " << prologue_size << " # DbgStart\n";
            os << "  .long 0 # DbgEnd\n";
            os << "  .long " << func_type_idx.getIndex() << " # FunctionType\n";
            os << "  .secrel32 " << func_name << " # CodeOffset\n";
            os << "  .secidx " << func_name << " # Segment\n";
            os << "  .byte 0 # Flags\n";
            os << "  .asciz \"" << display_name << "\"\n";
            os << "  .p2align 2, 0\n";
            os << ".Lfunc_sym_end_" << func_name << ":\n";
            uint64_t frame_size = 0;
            auto size_it = m_debugCtx->m_frameSizes.find(func_start);
            if (size_it != m_debugCtx->m_frameSizes.end())
            {
                frame_size = size_it->second;
            }

            bool has_fp = false;
            auto unwind_it = m_debugCtx->m_unwindInfo.find(func_start);
            if (unwind_it != m_debugCtx->m_unwindInfo.end())
            {
                for (const auto& dir : unwind_it->second)
                {
                    if (dir.m_text.rfind(".seh_setframe ", 0) == 0 ||
                        dir.m_text.rfind(".seh_frame ", 0) == 0)
                    {
                        has_fp = true;
                        break;
                    }
                }
            }
            uint32_t fp_flags = has_fp ? 0x28000 : 0x14000;

            os << "  # Frame Procedure Symbol\n";
            os << ".Lframe_proc_begin_" << func_name << ":\n";
            os << "  .short .Lframe_proc_end_" << func_name << " - .Lframe_proc_begin_" << func_name
               << " - 2\n";
            os << "  .short 0x1012 # S_FRAMEPROC\n";
            os << "  .long " << frame_size << " # TotalFrameBytes\n";
            os << "  .long 0 # PaddingFrameBytes\n";
            os << "  .long 0 # OffsetToPadding\n";
            os << "  .long 0 # BytesOfCalleeSavedRegisters\n";
            os << "  .long 0 # OffsetOfExceptionHandler\n";
            os << "  .short 0 # SectionIdOfExceptionHandler\n";
            os << "  .long " << llvm::format("0x%05x", fp_flags) << " # Flags\n";
            os << "  .p2align 2, 0\n";
            os << ".Lframe_proc_end_" << func_name << ":\n";

            if (func_die)
            {
                VarTranslator::TranslateLocalVars(func_die, func_size, *m_debugCtx, os);
                os << "  .p2align 2, 0\n";
            }

            os << ".Lfunc_scope_end_record_" << func_name << ":\n";
            os << "  .short 2\n";
            os << "  .short 6 # S_END\n";
            os << "  .p2align 2, 0\n";
            os << ".Lfunc_scope_end_" << func_name << ":\n";
        }

        os << ".Lsym_end:\n";

        // Emit line tables for each function in .debug$S
        for (const auto& pair : m_elfFunctions)
        {
            llvm::StringRef func_name = pair.second.m_name;
            uint64_t func_start = pair.first;
            auto id_it = m_debugCtx->m_funcToIdMap.find(func_start);
            if (id_it != m_debugCtx->m_funcToIdMap.end())
            {
                os << "  .cv_linetable " << id_it->second << ", func_code_start_" << func_name
                   << ", func_code_end_" << func_name << "\n";
            }
        }
        os << "  .cv_filechecksums\n";
        os << "  .cv_stringtable\n";
    }

    void EmitHeaders(llvm::raw_ostream& os)
    {
        os << "# --- REPEAT: ELF Headers ---\n";
    }

    llvm::ArrayRef<uint8_t> GetSectionContentsForOffset(const ob::ELFFile<ELFT>& elf,
                                                        typename ELFT::uint addr,
                                                        typename ELFT::uint& sec_addr)
    {
        auto sections_or_err = elf.sections();
        if (!sections_or_err)
        {
            return {};
        }

        for (const auto& sec : *sections_or_err)
        {
            if (addr >= sec.sh_addr && addr < sec.sh_addr + sec.sh_size)
            {
                auto contents_or_err = elf.getSectionContents(sec);
                if (contents_or_err)
                {
                    sec_addr = sec.sh_addr;
                    return contents_or_err.get();
                }
            }
        }
        return {};
    }

    std::vector<RelocEntry> GetElfRelocations()
    {
        std::vector<RelocEntry> relocs;
        using ELFObjectType = ob::ELFObjectFile<ELFT>;
        auto* elf_obj_derived = llvm::dyn_cast<ELFObjectType>(m_elfObj.get());
        if (!elf_obj_derived)
        {
            return relocs;
        }

        const ob::ELFFile<ELFT>& elf = elf_obj_derived->getELFFile();
        auto sections_or_err = elf.sections();
        if (!sections_or_err)
        {
            return relocs;
        }

        for (const auto& sec : *sections_or_err)
        {
            if (sec.sh_type == llvm::ELF::SHT_RELA || sec.sh_type == llvm::ELF::SHT_REL)
            {
                auto name_or_err = elf.getSectionName(sec);
                if (!name_or_err)
                {
                    continue;
                }

                llvm::StringRef sec_name = name_or_err.get();
                if (sec_name == ".rela.dyn" || sec_name == ".rela.plt" || sec_name == ".rel.dyn" ||
                    sec_name == ".rel.plt")
                {
                    auto symtab_sec_or_err = elf.getSection(sec.sh_link);
                    if (!symtab_sec_or_err)
                    {
                        continue;
                    }
                    auto* symtab_sec = *symtab_sec_or_err;

                    auto strtab_or_err = elf.getStringTableForSymtab(*symtab_sec);
                    if (!strtab_or_err)
                    {
                        continue;
                    }
                    llvm::StringRef strtab = strtab_or_err.get();

                    if (sec.sh_type == llvm::ELF::SHT_RELA)
                    {
                        auto relas_or_err = elf.relas(sec);
                        if (relas_or_err)
                        {
                            for (const auto& rela : *relas_or_err)
                            {
                                if (auto r =
                                        m_backend->ParseRelocation(elf, symtab_sec, strtab, rela))
                                {
                                    relocs.push_back(*r);
                                }
                            }
                        }
                    }
                    else if (sec.sh_type == llvm::ELF::SHT_REL)
                    {
                        auto rels_or_err = elf.rels(sec);
                        if (rels_or_err)
                        {
                            for (const auto& rel : *rels_or_err)
                            {
                                typename ELFT::uint relocated_sec_addr = 0;
                                llvm::ArrayRef<uint8_t> relocated_contents =
                                    GetSectionContentsForOffset(
                                        elf, rel.r_offset, relocated_sec_addr);
                                if (!relocated_contents.empty())
                                {
                                    if (auto r = m_backend->ParseRelocationRel(elf,
                                                                               symtab_sec,
                                                                               strtab,
                                                                               rel,
                                                                               relocated_sec_addr,
                                                                               relocated_contents))
                                    {
                                        relocs.push_back(*r);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        return relocs;
    }

    void EmitSections(llvm::raw_ostream& os)
    {
        os << "# --- REPEAT: ELF Sections ---\n";
        os << ".section .elf, \"rwx\"\n";
        os << ".globl __elf_base\n";
        os << "__elf_base:\n";

        using ELFObjectType = ob::ELFObjectFile<ELFT>;
        auto* elf_obj_derived = llvm::dyn_cast<ELFObjectType>(m_elfObj.get());
        if (!elf_obj_derived)
        {
            return;
        }

        const ob::ELFFile<ELFT>& elf = elf_obj_derived->getELFFile();
        auto phdrs_or_err = elf.program_headers();
        if (!phdrs_or_err)
        {
            return;
        }

        std::vector<RelocEntry> relocs = GetElfRelocations();

        const size_t ptr_size = sizeof(typename ELFT::uint);
        const char* ptr_directive = (ptr_size == 8) ? ".quad" : ".long";

        char suffix = 'A';
        for (const auto& phdr : *phdrs_or_err)
        {
            if (phdr.p_type == llvm::ELF::PT_LOAD)
            {
                os << ".org __elf_base + " << phdr.p_vaddr << "\n";

                std::string seg_start_sym = "__elf_seg_";
                seg_start_sym += suffix;
                os << seg_start_sym << ":\n";

                auto contents_or_err = elf.getSegmentContents(phdr);
                if (!contents_or_err)
                {
                    llvm::consumeError(contents_or_err.takeError());
                    return;
                }

                llvm::ArrayRef<uint8_t> bytes = contents_or_err.get();
                uint64_t seg_vaddr = phdr.p_vaddr;

                std::vector<RelocEntry> seg_relocs;
                for (const auto& r : relocs)
                {
                    if (r.m_offset >= seg_vaddr && r.m_offset < seg_vaddr + phdr.p_filesz)
                    {
                        seg_relocs.push_back(r);
                    }
                }

                const FunctionSymbol* current_func = nullptr;
                uint64_t func_start_addr = 0;
                uint64_t func_size = 0;

                uint64_t pos = 0;
                while (pos < phdr.p_filesz)
                {
                    uint64_t current_vaddr = seg_vaddr + pos;

                    auto func_it = m_elfFunctions.find(current_vaddr);
                    if (func_it != m_elfFunctions.end())
                    {
                        if (current_func)
                        {
                            UnwindTranslator::EmitSehProcEnd(func_start_addr, *m_debugCtx, os);
                            os << "func_code_end_" << current_func->m_name << ":\n";
                        }
                        current_func = &func_it->second;
                        func_start_addr = current_vaddr;
                        func_size = current_func->m_size;

                        os << "func_code_start_" << current_func->m_name << ":\n";
                        if (m_debugCtx)
                        {
                            auto id_it = m_debugCtx->m_funcToIdMap.find(func_start_addr);
                            if (id_it != m_debugCtx->m_funcToIdMap.end())
                            {
                                os << "  .cv_func_id " << id_it->second << "\n";
                            }
                        }

                        UnwindTranslator::EmitSehProcBegin(
                            func_start_addr, current_func->m_name, *m_debugCtx, os);
                    }

                    if (current_func)
                    {
                        uint64_t offset_in_func = current_vaddr - func_start_addr;
                        UnwindTranslator::EmitSehPrologue(
                            func_start_addr, offset_in_func, *m_debugCtx, os);
                    }
                    LineTranslator::EmitLineLoc(current_vaddr, func_start_addr, *m_debugCtx, os);
                    const RelocEntry* matched_reloc = nullptr;
                    for (const auto& r : seg_relocs)
                    {
                        if (r.m_offset == current_vaddr)
                        {
                            matched_reloc = &r;
                            break;
                        }
                    }

                    if (matched_reloc != nullptr)
                    {
                        os << ".org " << seg_start_sym << " + " << pos << "\n";
                        if (!matched_reloc->m_symName.empty())
                        {
                            if (matched_reloc->m_addend > 0)
                            {
                                os << ptr_directive << " " << matched_reloc->m_symName << " + "
                                   << matched_reloc->m_addend << "\n";
                            }
                            else
                            {
                                os << ptr_directive << " " << matched_reloc->m_symName << "\n";
                            }
                        }
                        else
                        {
                            if (const SegmentInfo* target_seg =
                                    FindSegment(matched_reloc->m_addend))
                            {
                                uint64_t offset_in_target =
                                    matched_reloc->m_addend - target_seg->m_vaddr;
                                os << ptr_directive << " __elf_seg_" << target_seg->m_suffix
                                   << " + " << offset_in_target << "\n";
                            }
                            else
                            {
                                os << ptr_directive << " " << matched_reloc->m_addend << "\n";
                            }
                        }
                        pos += ptr_size;
                    }
                    else
                    {
                        os << ".byte " << llvm::format("0x%02x", bytes[pos]) << "\n";
                        pos++;
                    }

                    if (current_func)
                    {
                        uint64_t next_vaddr = seg_vaddr + pos;
                        bool exited_func =
                            (next_vaddr - func_start_addr >= func_size) || (pos >= phdr.p_filesz);
                        if (exited_func)
                        {
                            UnwindTranslator::EmitSehProcEnd(func_start_addr, *m_debugCtx, os);
                            os << "func_code_end_" << current_func->m_name << ":\n";
                            current_func = nullptr;
                            func_start_addr = 0;
                        }
                    }
                }

                if (phdr.p_memsz > phdr.p_filesz)
                {
                    os << ".org " << seg_start_sym << " + " << phdr.p_memsz << "\n";
                }
                suffix++;
            }
        }
    }

    void EmitSymbols(llvm::raw_ostream& os)
    {
        os << "# --- REPEAT: ELF Exported Symbols ---\n";
        for (const ob::SymbolRef& sym : m_elfObj->symbols())
        {
            auto flags_or_err = sym.getFlags();
            if (!flags_or_err)
            {
                continue;
            }

            uint32_t flags = flags_or_err.get();
            if (flags & ob::SymbolRef::SF_Undefined)
            {
                continue;
            }

            auto addr_or_err = sym.getAddress();
            if (!addr_or_err)
            {
                continue;
            }
            uint64_t addr = addr_or_err.get();

            auto type_or_err = sym.getType();
            if (!type_or_err)
            {
                continue;
            }
            ob::SymbolRef::Type type = type_or_err.get();

            if (flags & ob::SymbolRef::SF_Global)
            {
                auto name_or_err = sym.getName();
                if (!name_or_err)
                {
                    continue;
                }
                llvm::StringRef name = name_or_err.get();
                if (name.empty())
                {
                    continue;
                }

                os << ".def " << name << "; .scl 2; .type 32; .endef\n";
                os << ".globl " << name << "\n";
                EmitAlias(os, name, addr);
            }
            else
            {
                const SegmentInfo* target_seg = FindSegment(addr);
                if (target_seg)
                {
                    if (type == ob::SymbolRef::Type::ST_Function)
                    {
                        auto func_it = m_elfFunctions.find(addr);
                        if (func_it != m_elfFunctions.end())
                        {
                            llvm::StringRef unique_name = func_it->second.m_name;
                            os << ".def " << unique_name << "; .scl 3; .type 32; .endef\n";
                            EmitAlias(os, unique_name, addr);
                        }
                    }
                    else
                    {
                        auto name_or_err = sym.getName();
                        if (!name_or_err)
                        {
                            continue;
                        }
                        llvm::StringRef name = name_or_err.get();
                        if (name.empty())
                        {
                            continue;
                        }

                        EmitAlias(os, name, addr);
                    }
                }
            }
        }
    }

    void EmitAlias(llvm::raw_ostream& os, llvm::StringRef name, uint64_t addr)
    {
        if (const SegmentInfo* target_seg = FindSegment(addr))
        {
            uint64_t offset_in_target = addr - target_seg->m_vaddr;
            os << name << " = __elf_seg_" << target_seg->m_suffix << " + " << offset_in_target
               << "\n";
        }
        else
        {
            os << name << " = " << addr << "\n";
        }
    }

    const SegmentInfo* FindSegment(uint64_t addr) const
    {
        for (const auto& seg : m_segments)
        {
            if (addr >= seg.m_vaddr && addr < seg.m_vaddr + seg.m_memsz)
            {
                return &seg;
            }
        }
        return nullptr;
    }
};

} // namespace repeat

#endif // REPEAT_TRANSLATOR_IMPL_H
