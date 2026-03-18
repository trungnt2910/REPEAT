#include "repeat/debug/var_translator.h"

#include <llvm/DebugInfo/CodeView/CodeView.h>
#include <llvm/DebugInfo/CodeView/SymbolRecord.h>
#include <llvm/DebugInfo/CodeView/SymbolSerializer.h>
#include <llvm/DebugInfo/CodeView/TypeRecord.h>
#include <llvm/DebugInfo/DWARF/DWARFDie.h>
#include <llvm/DebugInfo/DWARF/DWARFFormValue.h>
#include <llvm/DebugInfo/DWARF/DWARFLocationExpression.h>
#include <llvm/DebugInfo/DWARF/LowLevel/DWARFExpression.h>
#include <llvm/Support/DataExtractor.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/TargetParser/Triple.h>

#include "repeat/debug/debug_helper.h"
#include "repeat/debug/type_translator.h"

namespace repeat
{

static llvm::codeview::RegisterId map_dwarf_reg_to_cv_reg(uint64_t dwarf_reg,
                                                          llvm::Triple::ArchType arch)
{
    using llvm::codeview::RegisterId;
    if (arch == llvm::Triple::x86_64)
    {
        switch (dwarf_reg)
        {
        case 0:
            return RegisterId::RAX;
        case 1:
            return RegisterId::RDX;
        case 2:
            return RegisterId::RCX;
        case 3:
            return RegisterId::RBX;
        case 4:
            return RegisterId::RSI;
        case 5:
            return RegisterId::RDI;
        case 6:
            return RegisterId::RBP;
        case 7:
            return RegisterId::RSP;
        case 8:
            return RegisterId::R8;
        case 9:
            return RegisterId::R9;
        case 10:
            return RegisterId::R10;
        case 11:
            return RegisterId::R11;
        case 12:
            return RegisterId::R12;
        case 13:
            return RegisterId::R13;
        case 14:
            return RegisterId::R14;
        case 15:
            return RegisterId::R15;
        case 17:
            return RegisterId::XMM0;
        case 18:
            return RegisterId::XMM1;
        case 19:
            return RegisterId::XMM2;
        case 20:
            return RegisterId::XMM3;
        case 21:
            return RegisterId::XMM4;
        case 22:
            return RegisterId::XMM5;
        case 23:
            return RegisterId::XMM6;
        case 24:
            return RegisterId::XMM7;
        case 25:
            return RegisterId::XMM8;
        case 26:
            return RegisterId::XMM9;
        case 27:
            return RegisterId::XMM10;
        case 28:
            return RegisterId::XMM11;
        case 29:
            return RegisterId::XMM12;
        case 30:
            return RegisterId::XMM13;
        case 31:
            return RegisterId::XMM14;
        case 32:
            return RegisterId::XMM15;
        default:
            return RegisterId::NONE;
        }
    }
    else if (arch == llvm::Triple::x86)
    {
        switch (dwarf_reg)
        {
        case 0:
            return RegisterId::EAX;
        case 1:
            return RegisterId::ECX;
        case 2:
            return RegisterId::EDX;
        case 3:
            return RegisterId::EBX;
        case 4:
            return RegisterId::ESP;
        case 5:
            return RegisterId::EBP;
        case 6:
            return RegisterId::ESI;
        case 7:
            return RegisterId::EDI;
        case 8:
            return RegisterId::EIP;
        case 9:
            return RegisterId::EFLAGS;
        case 21:
            return RegisterId::XMM0;
        case 22:
            return RegisterId::XMM1;
        case 23:
            return RegisterId::XMM2;
        case 24:
            return RegisterId::XMM3;
        case 25:
            return RegisterId::XMM4;
        case 26:
            return RegisterId::XMM5;
        case 27:
            return RegisterId::XMM6;
        case 28:
            return RegisterId::XMM7;
        default:
            return RegisterId::NONE;
        }
    }
    else if (arch == llvm::Triple::arm)
    {
        if (dwarf_reg <= 12)
        {
            return static_cast<RegisterId>(static_cast<uint16_t>(RegisterId::ARM_R0) + dwarf_reg);
        }
        switch (dwarf_reg)
        {
        case 13:
            return RegisterId::ARM_SP;
        case 14:
            return RegisterId::ARM_LR;
        case 15:
            return RegisterId::ARM_PC;
        default:
            break;
        }
        if (dwarf_reg >= 64 && dwarf_reg <= 95)
        {
            return static_cast<RegisterId>(static_cast<uint16_t>(RegisterId::ARM_FS0) +
                                           (dwarf_reg - 64));
        }
        if (dwarf_reg >= 256 && dwarf_reg <= 287)
        {
            return static_cast<RegisterId>(static_cast<uint16_t>(RegisterId::ARM_ND0) +
                                           (dwarf_reg - 256));
        }
        return RegisterId::NONE;
    }
    else if (arch == llvm::Triple::aarch64)
    {
        if (dwarf_reg <= 28)
        {
            return static_cast<RegisterId>(static_cast<uint16_t>(RegisterId::ARM64_X0) + dwarf_reg);
        }
        switch (dwarf_reg)
        {
        case 29:
            return RegisterId::ARM64_FP;
        case 30:
            return RegisterId::ARM64_LR;
        case 31:
            return RegisterId::ARM64_SP;
        default:
            break;
        }
        if (dwarf_reg >= 64 && dwarf_reg <= 95)
        {
            return static_cast<RegisterId>(static_cast<uint16_t>(RegisterId::ARM64_Q0) +
                                           (dwarf_reg - 64));
        }
        return RegisterId::NONE;
    }
    return RegisterId::NONE;
}

enum class FrameBaseKind
{
    CFA,
    Register,
    Unknown
};

struct FrameBaseInfo
{
    FrameBaseKind kind = FrameBaseKind::Unknown;
    uint64_t reg = 0;
};

static llvm::DWARFDie find_subprogram_die(llvm::DWARFDie die)
{
    while (die)
    {
        if (die.getTag() == llvm::dwarf::DW_TAG_subprogram)
        {
            return die;
        }
        die = die.getParent();
    }
    return llvm::DWARFDie();
}

static FrameBaseInfo parse_frame_base(llvm::DWARFDie subprogram_die, DebugTranslationContext& ctx)
{
    auto loc_opt = subprogram_die.find(llvm::dwarf::DW_AT_frame_base);
    if (!loc_opt)
    {
        return {};
    }

    auto expr_or_err = subprogram_die.getLocations(llvm::dwarf::DW_AT_frame_base);
    if (!expr_or_err || expr_or_err->empty())
    {
        return {};
    }

    const auto& loc_expr = (*expr_or_err)[0];
    if (loc_expr.Range || expr_or_err->size() > 1)
    {
        return {};
    }

    llvm::DataExtractor data(
        loc_expr.Expr, ctx.m_elfObj->isLittleEndian(), ctx.m_elfObj->getBytesInAddress());
    llvm::DWARFExpression expr(data, ctx.m_elfObj->getBytesInAddress());

    auto it = expr.begin();
    if (it == expr.end())
    {
        return {};
    }
    const auto& op = *it;
    ++it;
    if (it != expr.end())
    {
        return {};
    }

    uint8_t code = op.getCode();
    if (code == llvm::dwarf::DW_OP_call_frame_cfa)
    {
        return {FrameBaseKind::CFA, 0};
    }
    else if (code >= llvm::dwarf::DW_OP_reg0 && code <= llvm::dwarf::DW_OP_reg31)
    {
        return {FrameBaseKind::Register, static_cast<uint64_t>(code - llvm::dwarf::DW_OP_reg0)};
    }
    else if (code == llvm::dwarf::DW_OP_regx)
    {
        return {FrameBaseKind::Register, op.getRawOperand(0)};
    }

    return {};
}

static std::optional<int32_t>
try_parse_stack_offset_expr(const llvm::DWARFLocationExpression& loc_expr,
                            const llvm::DWARFDie& die,
                            uint64_t func_start,
                            DebugTranslationContext& ctx)
{
    llvm::DataExtractor data(
        loc_expr.Expr, ctx.m_elfObj->isLittleEndian(), ctx.m_elfObj->getBytesInAddress());
    llvm::DWARFExpression expr(data, ctx.m_elfObj->getBytesInAddress());

    auto it = expr.begin();
    if (it == expr.end())
    {
        return std::nullopt;
    }
    const auto& op = *it;
    ++it;
    if (it != expr.end())
    {
        return std::nullopt; // More than one operation
    }

    if (op.getCode() == llvm::dwarf::DW_OP_fbreg)
    {
        int64_t raw_offset = op.getRawOperand(0);
        int32_t adjustment = 0;

        llvm::DWARFDie subprogram = find_subprogram_die(die);
        if (subprogram)
        {
            FrameBaseInfo fb = parse_frame_base(subprogram, ctx);
            if (fb.kind == FrameBaseKind::CFA)
            {
                auto fp_it = ctx.m_hasFramePointer.find(func_start);
                bool has_fp = (fp_it != ctx.m_hasFramePointer.end()) ? fp_it->second : false;
                llvm::Triple::ArchType arch = ctx.m_elfObj->getArch();

                if (has_fp)
                {
                    if (arch == llvm::Triple::x86_64 || arch == llvm::Triple::aarch64)
                    {
                        adjustment = 16;
                    }
                    else if (arch == llvm::Triple::x86 || arch == llvm::Triple::arm)
                    {
                        adjustment = 8;
                    }
                }
                else
                {
                    auto size_it = ctx.m_frameSizes.find(func_start);
                    uint64_t frame_size = (size_it != ctx.m_frameSizes.end()) ? size_it->second : 0;
                    if (arch == llvm::Triple::x86_64)
                    {
                        adjustment = frame_size + 8;
                    }
                    else if (arch == llvm::Triple::x86)
                    {
                        adjustment = frame_size + 4;
                    }
                    else if (arch == llvm::Triple::aarch64 || arch == llvm::Triple::arm)
                    {
                        adjustment = frame_size;
                    }
                }
            }
        }

        return static_cast<int32_t>(raw_offset + adjustment);
    }
    return std::nullopt;
}

static std::optional<int32_t>
try_parse_stack_offset(const llvm::DWARFDie& die, uint64_t func_start, DebugTranslationContext& ctx)
{
    auto loc_opt = die.find(llvm::dwarf::DW_AT_location);
    if (!loc_opt)
    {
        return std::nullopt;
    }

    auto expr_or_err = die.getLocations(llvm::dwarf::DW_AT_location);
    if (!expr_or_err || expr_or_err->empty())
    {
        return std::nullopt;
    }

    const auto& loc_expr = (*expr_or_err)[0];
    if (loc_expr.Range || expr_or_err->size() > 1)
    {
        return std::nullopt;
    }
    return try_parse_stack_offset_expr(loc_expr, die, func_start, ctx);
}

static std::optional<llvm::codeview::RegisterId>
try_parse_register_expr(const llvm::DWARFLocationExpression& loc_expr, DebugTranslationContext& ctx)
{
    llvm::DataExtractor data(
        loc_expr.Expr, ctx.m_elfObj->isLittleEndian(), ctx.m_elfObj->getBytesInAddress());
    llvm::DWARFExpression expr(data, ctx.m_elfObj->getBytesInAddress());

    auto it = expr.begin();
    if (it == expr.end())
    {
        return std::nullopt;
    }
    const auto& op = *it;
    ++it;
    if (it != expr.end())
    {
        return std::nullopt; // More than one operation
    }

    uint8_t code = op.getCode();
    if (code >= llvm::dwarf::DW_OP_reg0 && code <= llvm::dwarf::DW_OP_reg31)
    {
        return map_dwarf_reg_to_cv_reg(code - llvm::dwarf::DW_OP_reg0, ctx.m_elfObj->getArch());
    }
    else if (code == llvm::dwarf::DW_OP_regx)
    {
        return map_dwarf_reg_to_cv_reg(op.getRawOperand(0), ctx.m_elfObj->getArch());
    }
    return std::nullopt;
}

struct RegisterRelOffset
{
    llvm::codeview::RegisterId reg;
    int32_t offset;
};

static std::optional<RegisterRelOffset>
try_parse_register_rel_expr(const llvm::DWARFLocationExpression& loc_expr,
                            DebugTranslationContext& ctx)
{
    llvm::DataExtractor data(
        loc_expr.Expr, ctx.m_elfObj->isLittleEndian(), ctx.m_elfObj->getBytesInAddress());
    llvm::DWARFExpression expr(data, ctx.m_elfObj->getBytesInAddress());

    auto it = expr.begin();
    if (it == expr.end())
    {
        return std::nullopt;
    }
    const auto& op = *it;
    ++it;
    if (it != expr.end())
    {
        return std::nullopt; // More than one operation
    }

    uint8_t code = op.getCode();
    if (code >= llvm::dwarf::DW_OP_breg0 && code <= llvm::dwarf::DW_OP_breg31)
    {
        auto reg =
            map_dwarf_reg_to_cv_reg(code - llvm::dwarf::DW_OP_breg0, ctx.m_elfObj->getArch());
        if (reg == llvm::codeview::RegisterId::NONE)
        {
            return std::nullopt;
        }
        int64_t offset = op.getRawOperand(0);
        return RegisterRelOffset{reg, static_cast<int32_t>(offset)};
    }
    else if (code == llvm::dwarf::DW_OP_bregx)
    {
        auto reg = map_dwarf_reg_to_cv_reg(op.getRawOperand(0), ctx.m_elfObj->getArch());
        if (reg == llvm::codeview::RegisterId::NONE)
        {
            return std::nullopt;
        }
        int64_t offset = op.getRawOperand(1);
        return RegisterRelOffset{reg, static_cast<int32_t>(offset)};
    }
    return std::nullopt;
}

static std::optional<llvm::codeview::RegisterId> try_parse_register(const llvm::DWARFDie& die,
                                                                    DebugTranslationContext& ctx)
{
    auto loc_opt = die.find(llvm::dwarf::DW_AT_location);
    if (!loc_opt)
    {
        return std::nullopt;
    }

    auto expr_or_err = die.getLocations(llvm::dwarf::DW_AT_location);
    if (!expr_or_err || expr_or_err->empty())
    {
        return std::nullopt;
    }

    const auto& loc_expr = (*expr_or_err)[0];
    if (loc_expr.Range || expr_or_err->size() > 1)
    {
        return std::nullopt;
    }
    return try_parse_register_expr(loc_expr, ctx);
}

void VarTranslator::TranslateGlobalVars(DebugTranslationContext& ctx, llvm::raw_ostream& os)
{
    if (!ctx.m_dwarfCtx)
    {
        return;
    }

    for (const auto& CU : ctx.m_dwarfCtx->compile_units())
    {
        llvm::DWARFDie cu_die = CU->getUnitDIE();
        for (const auto& child : cu_die.children())
        {
            if (child.getTag() == llvm::dwarf::DW_TAG_variable)
            {
                auto loc_opt = child.find(llvm::dwarf::DW_AT_location);
                if (!loc_opt)
                {
                    continue;
                }

                auto block_opt = loc_opt->getAsBlock();
                if (!block_opt)
                {
                    continue;
                }

                uint64_t addr = 0;
                bool has_addr = false;
                llvm::DataExtractor data(
                    *block_opt, ctx.m_elfObj->isLittleEndian(), ctx.m_elfObj->getBytesInAddress());
                llvm::DWARFExpression expr(data, ctx.m_elfObj->getBytesInAddress());
                for (const auto& op : expr)
                {
                    if (op.getCode() == llvm::dwarf::DW_OP_addr)
                    {
                        addr = op.getRawOperand(0);
                        has_addr = true;
                        break;
                    }
                    else if (op.getCode() == llvm::dwarf::DW_OP_addrx)
                    {
                        uint64_t idx = op.getRawOperand(0);
                        if (auto addr_opt = child.getDwarfUnit()->getAddrOffsetSectionItem(idx))
                        {
                            addr = addr_opt->Address;
                            has_addr = true;
                        }
                        break;
                    }
                }

                if (!has_addr)
                {
                    continue;
                }

                const char* name_ptr = child.getName(llvm::DINameKind::LinkageName);
                if (!name_ptr)
                {
                    name_ptr = child.getName(llvm::DINameKind::ShortName);
                }
                if (!name_ptr)
                {
                    continue;
                }

                llvm::StringRef var_name = name_ptr;
                const char* short_name_ptr = child.getName(llvm::DINameKind::ShortName);
                llvm::StringRef display_name = short_name_ptr ? short_name_ptr : var_name;

                std::string unique_name = var_name.str();
                auto sym_it = ctx.m_symbolNames.find(addr);
                if (sym_it != ctx.m_symbolNames.end())
                {
                    unique_name = sym_it->second;
                }

                llvm::DWARFDie type_die = child.resolveReferencedType(llvm::dwarf::DW_AT_type);
                llvm::codeview::TypeIndex type_idx = TypeTranslator::TranslateType(type_die, ctx);

                auto ext_opt = child.find(llvm::dwarf::DW_AT_external);
                bool is_external =
                    ext_opt ? ext_opt->getAsUnsignedConstant().value_or(0) != 0 : false;

                uint16_t record_kind = is_external ? 0x110d : 0x110c; // S_GDATA32 vs S_LDATA32

                // Since S_GDATA32/S_LDATA32 contains relocations, we emit its prefix and type
                // manually, then output relocation operators for address offset and segment index.
                std::string var_kind_comment =
                    is_external ? "Global Variable Symbol" : "Local Variable Symbol";
                os << "  # " << var_kind_comment << ": " << unique_name << "\n";
                // We calculate the record size manually here because we are emitting the
                // address offset and segment fields as raw relocatable symbols rather
                // than serialized constants.
                uint32_t name_len_with_null = display_name.size() + 1;
                uint32_t total_record_len = 2 + 2 + 4 + 4 + 2 + name_len_with_null;
                uint32_t padding = (4 - (total_record_len % 4)) % 4;
                uint32_t record_len_field = total_record_len + padding - 2;

                os << "  .short " << record_len_field << "\n";
                os << "  .short " << record_kind << "\n";
                os << "  .long " << type_idx.getIndex() << "\n";
                os << "  .secrel32 " << unique_name << "\n";
                os << "  .secidx " << unique_name << "\n";
                os << "  .asciz \"" << display_name << "\"\n";
                for (uint32_t p = 0; p < padding; p++)
                {
                    os << "  .byte 0\n";
                }
            }
        }
    }
}

static void write_compressed_u32(std::vector<uint8_t>& data, uint32_t val)
{
    if (val < 0x80)
    {
        data.push_back(static_cast<uint8_t>(val));
    }
    else if (val < 0x4000)
    {
        data.push_back(static_cast<uint8_t>((val >> 8) | 0x80));
        data.push_back(static_cast<uint8_t>(val & 0xff));
    }
    else
    {
        data.push_back(static_cast<uint8_t>((val >> 24) | 0xc0));
        data.push_back(static_cast<uint8_t>((val >> 16) & 0xff));
        data.push_back(static_cast<uint8_t>((val >> 8) & 0xff));
        data.push_back(static_cast<uint8_t>(val & 0xff));
    }
}

static void write_compressed_s32(std::vector<uint8_t>& data, int32_t val)
{
    uint32_t uval;
    if (val < 0)
    {
        uval = (static_cast<uint32_t>(-val) << 1) | 1;
    }
    else
    {
        uval = static_cast<uint32_t>(val) << 1;
    }
    write_compressed_u32(data, uval);
}

static bool is_static_variable(const llvm::DWARFDie& die, DebugTranslationContext& ctx)
{
    auto loc_opt = die.find(llvm::dwarf::DW_AT_location);
    if (!loc_opt)
    {
        return false;
    }
    if (auto block_opt = loc_opt->getAsBlock())
    {
        llvm::DataExtractor data(
            *block_opt, ctx.m_elfObj->isLittleEndian(), ctx.m_elfObj->getBytesInAddress());
        llvm::DWARFExpression expr(data, ctx.m_elfObj->getBytesInAddress());
        for (const auto& op : expr)
        {
            if (op.getCode() == llvm::dwarf::DW_OP_addr || op.getCode() == llvm::dwarf::DW_OP_addrx)
            {
                return true;
            }
        }
    }
    return false;
}

static void translate_block_variables(const llvm::DWARFDie& block_die,
                                      uint64_t func_size,
                                      DebugTranslationContext& ctx,
                                      llvm::raw_ostream& os,
                                      int& inline_site_counter,
                                      uint64_t func_start,
                                      llvm::StringRef func_name)
{
    for (const auto& child : block_die.children())
    {
        auto tag = child.getTag();
        if (tag == llvm::dwarf::DW_TAG_lexical_block)
        {
            translate_block_variables(
                child, func_size, ctx, os, inline_site_counter, func_start, func_name);
        }
        else if (tag == llvm::dwarf::DW_TAG_inlined_subroutine)
        {
            int site_id = ++inline_site_counter;

            uint64_t low_pc = 0;
            uint64_t high_pc = 0;
            auto ranges_or_err = child.getAddressRanges();
            if (ranges_or_err)
            {
                for (const auto& range : *ranges_or_err)
                {
                    if (low_pc == 0 || range.LowPC < low_pc)
                    {
                        low_pc = range.LowPC;
                    }
                    if (range.HighPC > high_pc)
                    {
                        high_pc = range.HighPC;
                    }
                }
            }
            else
            {
                llvm::consumeError(ranges_or_err.takeError());
                translate_block_variables(
                    child, func_size, ctx, os, inline_site_counter, func_start, func_name);
                continue;
            }

            llvm::DWARFDie origin_die =
                child.resolveReferencedType(llvm::dwarf::DW_AT_abstract_origin);
            if (!origin_die)
            {
                translate_block_variables(
                    child, func_size, ctx, os, inline_site_counter, func_start, func_name);
                continue;
            }

            const char* inlinee_name_ptr = origin_die.getName(llvm::DINameKind::ShortName);
            llvm::StringRef inlinee_name = inlinee_name_ptr ? inlinee_name_ptr : "inlinee";

            llvm::codeview::TypeIndex ret_idx = llvm::codeview::TypeIndex::Void();
            auto type_opt = origin_die.find(llvm::dwarf::DW_AT_type);
            if (type_opt)
            {
                llvm::DWARFDie ret_type_die =
                    origin_die.resolveReferencedType(llvm::dwarf::DW_AT_type);
                ret_idx = TypeTranslator::TranslateType(ret_type_die, ctx);
            }

            std::vector<llvm::codeview::TypeIndex> args;
            for (const auto& param : origin_die.children())
            {
                if (param.getTag() == llvm::dwarf::DW_TAG_formal_parameter)
                {
                    llvm::DWARFDie param_type_die =
                        param.resolveReferencedType(llvm::dwarf::DW_AT_type);
                    args.push_back(TypeTranslator::TranslateType(param_type_die, ctx));
                }
            }

            llvm::codeview::ArgListRecord arg_list_rec(llvm::codeview::TypeRecordKind::ArgList,
                                                       args);
            llvm::codeview::TypeIndex arg_list_idx = ctx.m_typeBuilder.writeLeafType(arg_list_rec);

            llvm::codeview::ProcedureRecord proc_rec(ret_idx,
                                                     llvm::codeview::CallingConvention::NearC,
                                                     llvm::codeview::FunctionOptions::None,
                                                     args.size(),
                                                     arg_list_idx);
            llvm::codeview::TypeIndex proc_idx = ctx.m_typeBuilder.writeLeafType(proc_rec);

            llvm::codeview::FuncIdRecord func_id_rec(
                llvm::codeview::TypeIndex::None(), proc_idx, inlinee_name);
            llvm::codeview::TypeIndex inlinee_func_id =
                ctx.m_typeBuilder.writeLeafType(func_id_rec);

            uint64_t call_file_idx = 0;
            uint64_t call_line = 0;
            auto file_opt = child.find(llvm::dwarf::DW_AT_call_file);
            if (file_opt)
            {
                call_file_idx = file_opt->getAsUnsignedConstant().value_or(0);
            }
            auto line_opt = child.find(llvm::dwarf::DW_AT_call_line);
            if (line_opt)
            {
                call_line = line_opt->getAsUnsignedConstant().value_or(0);
            }

            const llvm::DWARFUnit* cu = child.getDwarfUnit();
            unsigned call_file_global_idx = 1;
            auto cu_map_it = ctx.m_cuLocalToGlobalFile.find(cu);
            if (cu_map_it != ctx.m_cuLocalToGlobalFile.end())
            {
                auto file_it = cu_map_it->second.find(call_file_idx);
                if (file_it != cu_map_it->second.end())
                {
                    call_file_global_idx = file_it->second;
                }
            }

            auto start_it = ctx.m_addrToLineMap.lower_bound(low_pc);
            auto end_it = ctx.m_addrToLineMap.lower_bound(high_pc);

            std::vector<uint8_t> annotations;
            unsigned last_file = call_file_global_idx;
            int32_t last_line = call_line;
            uint64_t last_addr = low_pc;
            bool open_range = false;

            for (auto it = start_it; it != end_it; ++it)
            {
                uint64_t addr = it->first;
                unsigned file = it->second.m_fileIdx;
                int32_t line = it->second.m_line;

                if (open_range && file == last_file && line == last_line)
                {
                    continue;
                }

                open_range = true;

                if (file != last_file)
                {
                    unsigned file_offset = (file - 1) * 8;
                    auto change_file_op =
                        static_cast<uint32_t>(llvm::codeview::BinaryAnnotationsOpCode::ChangeFile);
                    write_compressed_u32(annotations, change_file_op);
                    write_compressed_u32(annotations, file_offset);
                    last_file = file;
                }

                int32_t line_delta = line - last_line;
                uint64_t code_delta = addr - last_addr;

                if (line_delta != 0 || code_delta != 0)
                {
                    uint32_t encoded_line_delta;
                    if (line_delta < 0)
                    {
                        encoded_line_delta = (static_cast<uint32_t>(-line_delta) << 1) | 1;
                    }
                    else
                    {
                        encoded_line_delta = static_cast<uint32_t>(line_delta) << 1;
                    }

                    if (encoded_line_delta < 0x8 && code_delta <= 0xf)
                    {
                        unsigned operand = (encoded_line_delta << 4) | code_delta;
                        write_compressed_u32(
                            annotations,
                            static_cast<uint32_t>(llvm::codeview::BinaryAnnotationsOpCode::
                                                      ChangeCodeOffsetAndLineOffset));
                        write_compressed_u32(annotations, operand);
                    }
                    else
                    {
                        if (line_delta != 0)
                        {
                            write_compressed_u32(
                                annotations,
                                static_cast<uint32_t>(
                                    llvm::codeview::BinaryAnnotationsOpCode::ChangeLineOffset));
                            write_compressed_s32(annotations, line_delta);
                        }
                        if (code_delta != 0)
                        {
                            write_compressed_u32(
                                annotations,
                                static_cast<uint32_t>(
                                    llvm::codeview::BinaryAnnotationsOpCode::ChangeCodeOffset));
                            write_compressed_u32(annotations, static_cast<uint32_t>(code_delta));
                        }
                    }

                    last_line = line;
                    last_addr = addr;
                }
            }

            if (open_range)
            {
                uint64_t end_delta = high_pc - last_addr;
                write_compressed_u32(
                    annotations,
                    static_cast<uint32_t>(
                        llvm::codeview::BinaryAnnotationsOpCode::ChangeCodeLength));
                write_compressed_u32(annotations, static_cast<uint32_t>(end_delta));
            }

            os << "  # Inline Site Symbol: " << inlinee_name << "\n";
            os << ".Linline_site_begin_" << site_id << ":\n";
            uint32_t record_payload_size = 4 + 4 + 4 + annotations.size();
            os << "  .short " << (record_payload_size) << "\n";
            os << "  .short 0x114d # S_INLINESITE\n";
            os << "  .long 0 # Parent\n";
            os << "  .long .Linline_site_scope_end_" << site_id << " - .Lsym_begin # End offset\n";
            os << "  .long " << inlinee_func_id.getIndex() << " # Inlinee Type Index\n";
            for (uint8_t byte : annotations)
            {
                os << "  .byte " << (int)byte << "\n";
            }
            os << "  .p2align 2, 0\n";
            os << ".Linline_site_end_" << site_id << ":\n";

            translate_block_variables(
                child, func_size, ctx, os, inline_site_counter, func_start, func_name);

            os << "  .short 2\n";
            os << "  .short 6 # S_END\n";
            os << "  .p2align 2, 0\n";
            os << ".Linline_site_scope_end_" << site_id << ":\n";
        }
        else if (tag == llvm::dwarf::DW_TAG_variable || tag == llvm::dwarf::DW_TAG_formal_parameter)
        {
            if (is_static_variable(child, ctx))
            {
                continue;
            }
            const char* name_ptr = child.getName(llvm::DINameKind::ShortName);
            if (!name_ptr)
            {
                continue;
            }

            llvm::StringRef var_name = name_ptr;
            llvm::DWARFDie type_die = child.resolveReferencedType(llvm::dwarf::DW_AT_type);
            llvm::codeview::TypeIndex type_idx = TypeTranslator::TranslateType(type_die, ctx);

            bool force_local_sym = (tag == llvm::dwarf::DW_TAG_formal_parameter);
            bool parsed_simple = false;

            if (!force_local_sym)
            {
                if (auto stack_offset = try_parse_stack_offset(child, func_start, ctx))
                {
                    llvm::codeview::BPRelativeSym bp_sym(
                        llvm::codeview::SymbolRecordKind::BPRelativeSym);
                    bp_sym.Offset = *stack_offset;
                    bp_sym.Type = type_idx;
                    bp_sym.Name = var_name;

                    llvm::codeview::CVSymbol cvs = llvm::codeview::SymbolSerializer::writeOneSymbol(
                        bp_sym,
                        ctx.m_typeBuilderAllocator,
                        llvm::codeview::CodeViewContainer::ObjectFile);
                    DebugHelper::EmitSymbolBytesToAssembly(os, cvs);
                    parsed_simple = true;
                }
                else if (auto reg = try_parse_register(child, ctx))
                {
                    llvm::codeview::RegisterSym reg_sym(
                        llvm::codeview::SymbolRecordKind::RegisterSym);
                    reg_sym.Index = type_idx;
                    reg_sym.Register = *reg;
                    reg_sym.Name = var_name;

                    llvm::codeview::CVSymbol cvs = llvm::codeview::SymbolSerializer::writeOneSymbol(
                        reg_sym,
                        ctx.m_typeBuilderAllocator,
                        llvm::codeview::CodeViewContainer::ObjectFile);
                    DebugHelper::EmitSymbolBytesToAssembly(os, cvs);
                    parsed_simple = true;
                }
            }

            if (!parsed_simple)
            {
                auto expr_or_err = child.getLocations(llvm::dwarf::DW_AT_location);
                if (expr_or_err && !expr_or_err->empty())
                {
                    llvm::codeview::LocalSymFlags flags = llvm::codeview::LocalSymFlags::None;
                    if (tag == llvm::dwarf::DW_TAG_formal_parameter)
                    {
                        flags |= llvm::codeview::LocalSymFlags::IsParameter;
                    }
                    llvm::codeview::LocalSym local_sym(llvm::codeview::SymbolRecordKind::LocalSym);
                    local_sym.Type = type_idx;
                    local_sym.Flags = flags;
                    local_sym.Name = var_name;

                    llvm::codeview::CVSymbol cvs = llvm::codeview::SymbolSerializer::writeOneSymbol(
                        local_sym,
                        ctx.m_typeBuilderAllocator,
                        llvm::codeview::CodeViewContainer::ObjectFile);

                    os << "  # Local Variable: " << var_name << "\n";
                    DebugHelper::EmitSymbolBytesToAssembly(os, cvs);

                    for (const auto& loc_expr : *expr_or_err)
                    {
                        uint64_t low_pc = loc_expr.Range ? loc_expr.Range->LowPC : func_start;
                        uint64_t high_pc =
                            loc_expr.Range ? loc_expr.Range->HighPC : (func_start + func_size);

                        if (low_pc < func_start)
                        {
                            ctx.m_warningStream
                                << "warning: location range starts before function for variable '"
                                << var_name << "' ignored\n";
                            continue;
                        }

                        uint64_t start_offset = low_pc - func_start;
                        uint64_t range_len = high_pc - low_pc;

                        if (func_name.empty())
                        {
                            ctx.m_warningStream
                                << "warning: missing function name for relocation of variable '"
                                << var_name << "', omitting range\n";
                            continue;
                        }

                        if (auto reg = try_parse_register_expr(loc_expr, ctx))
                        {
                            uint16_t reg_id = static_cast<uint16_t>(*reg);
                            uint64_t current_offset = start_offset;
                            uint64_t remaining_len = range_len;
                            while (remaining_len > 0)
                            {
                                uint16_t current_len =
                                    std::min(remaining_len, (uint64_t)llvm::codeview::MaxDefRange);
                                os << "  .short 14\n";
                                os << "  .short 0x1141 # S_DEFRANGE_REGISTER\n";
                                os << "  .short " << reg_id << " # Register\n";
                                os << "  .short 0 # Flags\n";
                                os << "  .secrel32 " << func_name << " + " << current_offset
                                   << "\n";
                                os << "  .secidx " << func_name << "\n";
                                os << "  .short " << current_len << "\n";

                                current_offset += current_len;
                                remaining_len -= current_len;
                            }
                        }
                        else if (auto stack_offset =
                                     try_parse_stack_offset_expr(loc_expr, child, func_start, ctx))
                        {
                            int32_t offset = *stack_offset;
                            uint64_t current_offset = start_offset;
                            uint64_t remaining_len = range_len;
                            while (remaining_len > 0)
                            {
                                uint16_t current_len =
                                    std::min(remaining_len, (uint64_t)llvm::codeview::MaxDefRange);
                                os << "  .short 14\n";
                                os << "  .short 0x1142 # S_DEFRANGE_FRAMEPOINTER_REL\n";
                                os << "  .long " << offset << " # Offset\n";
                                os << "  .secrel32 " << func_name << " + " << current_offset
                                   << "\n";
                                os << "  .secidx " << func_name << "\n";
                                os << "  .short " << current_len << "\n";

                                current_offset += current_len;
                                remaining_len -= current_len;
                            }
                        }
                        else if (auto reg_rel = try_parse_register_rel_expr(loc_expr, ctx))
                        {
                            uint16_t reg_id = static_cast<uint16_t>(reg_rel->reg);
                            int32_t offset = reg_rel->offset;
                            uint64_t current_offset = start_offset;
                            uint64_t remaining_len = range_len;
                            while (remaining_len > 0)
                            {
                                uint16_t current_len =
                                    std::min(remaining_len, (uint64_t)llvm::codeview::MaxDefRange);
                                os << "  .short 18\n";
                                os << "  .short 0x1145 # S_DEFRANGE_REGISTER_REL\n";
                                os << "  .short " << reg_id << " # Register\n";
                                os << "  .short 0 # Flags\n";
                                os << "  .long " << offset << " # Offset\n";
                                os << "  .secrel32 " << func_name << " + " << current_offset
                                   << "\n";
                                os << "  .secidx " << func_name << "\n";
                                os << "  .short " << current_len << "\n";

                                current_offset += current_len;
                                remaining_len -= current_len;
                            }
                        }
                        else
                        {
                            ctx.m_warningStream
                                << "warning: unsupported DWARF location expression for variable '"
                                << var_name << "' in range [" << llvm::formatv("{0:x}", low_pc)
                                << ", " << llvm::formatv("{0:x}", high_pc) << "), omitting range\n";
                        }
                    }
                }
            }
        }
    }
}

void VarTranslator::TranslateLocalVars(const llvm::DWARFDie& func_die,
                                       uint64_t func_size,
                                       DebugTranslationContext& ctx,
                                       llvm::raw_ostream& os)
{
    auto low_pc_opt = func_die.find(llvm::dwarf::DW_AT_low_pc);
    uint64_t func_start = low_pc_opt ? low_pc_opt->getAsAddress().value_or(0) : 0;
    if (func_start == 0)
    {
        auto ranges_or_err = func_die.getAddressRanges();
        if (ranges_or_err && !ranges_or_err->empty())
        {
            func_start = (*ranges_or_err)[0].LowPC;
        }
        else if (!ranges_or_err)
        {
            llvm::consumeError(ranges_or_err.takeError());
        }
    }

    llvm::StringRef func_name = "";
    auto name_it = ctx.m_symbolNames.find(func_start);
    if (name_it != ctx.m_symbolNames.end())
    {
        func_name = name_it->second;
    }

    int inline_site_counter = 0;
    translate_block_variables(
        func_die, func_size, ctx, os, inline_site_counter, func_start, func_name);
}

} // namespace repeat
