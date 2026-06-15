#include "repeat/debug/type_translator.h"

#include <llvm/DebugInfo/CodeView/ContinuationRecordBuilder.h>
#include <llvm/DebugInfo/CodeView/TypeRecord.h>
#include <llvm/DebugInfo/DWARF/DWARFDie.h>
#include <llvm/DebugInfo/DWARF/DWARFFormValue.h>
#include <llvm/Support/FormatVariadic.h>

namespace repeat
{

static std::string get_qualified_name(const llvm::DWARFDie& die)
{
    std::vector<std::string> components;
    llvm::DWARFDie parent = die;
    while (parent)
    {
        auto tag = parent.getTag();
        if (tag == llvm::dwarf::DW_TAG_compile_unit || tag == llvm::dwarf::DW_TAG_partial_unit)
        {
            break;
        }
        if (tag == llvm::dwarf::DW_TAG_namespace || tag == llvm::dwarf::DW_TAG_structure_type ||
            tag == llvm::dwarf::DW_TAG_class_type || tag == llvm::dwarf::DW_TAG_union_type ||
            tag == llvm::dwarf::DW_TAG_enumeration_type)
        {
            const char* name_ptr = parent.getName(llvm::DINameKind::ShortName);
            if (name_ptr && name_ptr[0] != '\0')
            {
                components.push_back(name_ptr);
            }
            else if (tag == llvm::dwarf::DW_TAG_namespace)
            {
                components.push_back("(anonymous namespace)");
            }
        }
        parent = parent.getParent();
    }
    std::string result;
    for (auto it = components.rbegin(); it != components.rend(); ++it)
    {
        if (!result.empty())
        {
            result += "::";
        }
        result += *it;
    }
    return result;
}

static llvm::codeview::TypeIndex translate_procedure_type(const llvm::DWARFDie& die,
                                                          DebugTranslationContext& ctx)
{
    llvm::codeview::TypeIndex ret_idx = llvm::codeview::TypeIndex::Void();
    auto type_opt = die.find(llvm::dwarf::DW_AT_type);
    if (type_opt)
    {
        llvm::DWARFDie ret_type_die = die.resolveReferencedType(llvm::dwarf::DW_AT_type);
        ret_idx = TypeTranslator::TranslateType(ret_type_die, ctx);
    }

    std::vector<llvm::codeview::TypeIndex> args;
    for (const auto& child : die.children())
    {
        if (child.getTag() == llvm::dwarf::DW_TAG_formal_parameter)
        {
            llvm::DWARFDie param_type_die = child.resolveReferencedType(llvm::dwarf::DW_AT_type);
            llvm::codeview::TypeIndex param_type_idx =
                TypeTranslator::TranslateType(param_type_die, ctx);
            args.push_back(param_type_idx);
        }
    }

    llvm::codeview::ArgListRecord arg_list_rec(llvm::codeview::TypeRecordKind::ArgList, args);
    llvm::codeview::TypeIndex arg_list_idx = ctx.m_typeBuilder.writeLeafType(arg_list_rec);

    llvm::codeview::ProcedureRecord proc_rec(ret_idx,
                                             llvm::codeview::CallingConvention::NearC,
                                             llvm::codeview::FunctionOptions::None,
                                             args.size(),
                                             arg_list_idx);
    return ctx.m_typeBuilder.writeLeafType(proc_rec);
}

llvm::codeview::TypeIndex TypeTranslator::TranslateType(const llvm::DWARFDie& die,
                                                        DebugTranslationContext& ctx)
{
    if (!die)
    {
        return llvm::codeview::TypeIndex::Void();
    }

    uint64_t offset = die.getOffset();
    auto it = ctx.m_typeMap.find(offset);
    if (it != ctx.m_typeMap.end())
    {
        return it->second;
    }

    // Prevent recursion cycle by inserting a temporary placeholder
    ctx.m_typeMap[offset] = llvm::codeview::TypeIndex::Void();

    llvm::codeview::TypeIndex resolved_idx = llvm::codeview::TypeIndex::Void();

    switch (die.getTag())
    {
    case llvm::dwarf::DW_TAG_base_type:
    {
        auto size_opt = die.find(llvm::dwarf::DW_AT_byte_size);
        auto enc_opt = die.find(llvm::dwarf::DW_AT_encoding);

        uint64_t size = size_opt ? size_opt->getAsUnsignedConstant().value_or(0) : 0;
        uint64_t encoding = enc_opt ? enc_opt->getAsUnsignedConstant().value_or(0) : 0;

        if (encoding == llvm::dwarf::DW_ATE_signed || encoding == llvm::dwarf::DW_ATE_signed_char)
        {
            if (size == 1)
            {
                resolved_idx = llvm::codeview::TypeIndex::SignedCharacter();
            }
            else if (size == 2)
            {
                resolved_idx = llvm::codeview::TypeIndex(llvm::codeview::SimpleTypeKind::Int16);
            }
            else if (size == 4)
            {
                resolved_idx = llvm::codeview::TypeIndex::Int32();
            }
            else if (size == 8)
            {
                resolved_idx = llvm::codeview::TypeIndex::Int64();
            }
        }
        else if (encoding == llvm::dwarf::DW_ATE_unsigned ||
                 encoding == llvm::dwarf::DW_ATE_unsigned_char)
        {
            if (size == 1)
            {
                resolved_idx = llvm::codeview::TypeIndex::UnsignedCharacter();
            }
            else if (size == 2)
            {
                resolved_idx = llvm::codeview::TypeIndex(llvm::codeview::SimpleTypeKind::UInt16);
            }
            else if (size == 4)
            {
                resolved_idx = llvm::codeview::TypeIndex::UInt32();
            }
            else if (size == 8)
            {
                resolved_idx = llvm::codeview::TypeIndex::UInt64();
            }
        }
        else if (encoding == llvm::dwarf::DW_ATE_float)
        {
            if (size == 4)
            {
                resolved_idx = llvm::codeview::TypeIndex::Float32();
            }
            else if (size == 8)
            {
                resolved_idx = llvm::codeview::TypeIndex::Float64();
            }
        }
        else if (encoding == llvm::dwarf::DW_ATE_boolean)
        {
            resolved_idx = llvm::codeview::TypeIndex(llvm::codeview::SimpleTypeKind::Boolean8);
        }
        break;
    }

    case llvm::dwarf::DW_TAG_pointer_type:
    {
        llvm::DWARFDie target_die = die.resolveReferencedType(llvm::dwarf::DW_AT_type);
        llvm::codeview::TypeIndex target_idx = TranslateType(target_die, ctx);

        llvm::codeview::PointerRecord ptr_rec(target_idx,
                                              llvm::codeview::PointerKind::Near64,
                                              llvm::codeview::PointerMode::Pointer,
                                              llvm::codeview::PointerOptions::None,
                                              8);
        resolved_idx = ctx.m_typeBuilder.writeLeafType(ptr_rec);
        break;
    }

    case llvm::dwarf::DW_TAG_const_type:
    {
        llvm::DWARFDie target_die = die.resolveReferencedType(llvm::dwarf::DW_AT_type);
        llvm::codeview::TypeIndex target_idx = TranslateType(target_die, ctx);

        llvm::codeview::ModifierRecord mod_rec(target_idx, llvm::codeview::ModifierOptions::Const);
        resolved_idx = ctx.m_typeBuilder.writeLeafType(mod_rec);
        break;
    }

    case llvm::dwarf::DW_TAG_volatile_type:
    {
        llvm::DWARFDie target_die = die.resolveReferencedType(llvm::dwarf::DW_AT_type);
        llvm::codeview::TypeIndex target_idx = TranslateType(target_die, ctx);

        llvm::codeview::ModifierRecord mod_rec(target_idx,
                                               llvm::codeview::ModifierOptions::Volatile);
        resolved_idx = ctx.m_typeBuilder.writeLeafType(mod_rec);
        break;
    }

    case llvm::dwarf::DW_TAG_typedef:
    {
        llvm::DWARFDie target_die = die.resolveReferencedType(llvm::dwarf::DW_AT_type);
        resolved_idx = TranslateType(target_die, ctx);
        break;
    }

    case llvm::dwarf::DW_TAG_structure_type:
    case llvm::dwarf::DW_TAG_class_type:
    {
        std::string name = get_qualified_name(die);

        auto size_opt = die.find(llvm::dwarf::DW_AT_byte_size);
        uint64_t size = size_opt ? size_opt->getAsUnsignedConstant().value_or(0) : 0;

        uint16_t member_count = 0;
        for (const auto& child : die.children())
        {
            auto tag = child.getTag();
            if (tag == llvm::dwarf::DW_TAG_member || tag == llvm::dwarf::DW_TAG_inheritance ||
                tag == llvm::dwarf::DW_TAG_structure_type ||
                tag == llvm::dwarf::DW_TAG_class_type || tag == llvm::dwarf::DW_TAG_union_type ||
                tag == llvm::dwarf::DW_TAG_enumeration_type ||
                tag == llvm::dwarf::DW_TAG_subprogram)
            {
                member_count++;
            }
        }

        if (member_count == 0 && size == 0)
        {
            llvm::codeview::ClassRecord class_rec(llvm::codeview::TypeRecordKind::Struct,
                                                  0,
                                                  llvm::codeview::ClassOptions::ForwardReference,
                                                  llvm::codeview::TypeIndex::None(),
                                                  llvm::codeview::TypeIndex::None(),
                                                  llvm::codeview::TypeIndex::None(),
                                                  0,
                                                  name,
                                                  "");
            resolved_idx = ctx.m_typeBuilder.writeLeafType(class_rec);
            break;
        }

        // Pre-register forward reference to prevent infinite recursion
        llvm::codeview::ClassRecord forward_rec(llvm::codeview::TypeRecordKind::Struct,
                                                0,
                                                llvm::codeview::ClassOptions::ForwardReference,
                                                llvm::codeview::TypeIndex::None(),
                                                llvm::codeview::TypeIndex::None(),
                                                llvm::codeview::TypeIndex::None(),
                                                0,
                                                name,
                                                "");
        llvm::codeview::TypeIndex forward_idx = ctx.m_typeBuilder.writeLeafType(forward_rec);
        ctx.m_typeMap[offset] = forward_idx;

        llvm::codeview::ContinuationRecordBuilder field_builder;
        field_builder.begin(llvm::codeview::ContinuationRecordKind::FieldList);

        for (const auto& child : die.children())
        {
            auto tag = child.getTag();
            if (tag == llvm::dwarf::DW_TAG_member)
            {
                const char* mem_name_ptr = child.getName(llvm::DINameKind::ShortName);
                llvm::StringRef mem_name = mem_name_ptr ? mem_name_ptr : "";

                llvm::DWARFDie mem_type_die = child.resolveReferencedType(llvm::dwarf::DW_AT_type);
                llvm::codeview::TypeIndex mem_type_idx = TranslateType(mem_type_die, ctx);

                uint64_t mem_offset = 0;
                auto mem_offset_opt = child.find(llvm::dwarf::DW_AT_data_member_location);
                if (mem_offset_opt)
                {
                    mem_offset = mem_offset_opt->getAsUnsignedConstant().value_or(0);
                }

                llvm::codeview::DataMemberRecord mem_rec(
                    llvm::codeview::MemberAccess::Public, mem_type_idx, mem_offset, mem_name);
                field_builder.writeMemberType(mem_rec);
            }
            else if (tag == llvm::dwarf::DW_TAG_inheritance)
            {
                llvm::DWARFDie base_die = child.resolveReferencedType(llvm::dwarf::DW_AT_type);
                llvm::codeview::TypeIndex base_type_idx = TranslateType(base_die, ctx);

                uint64_t base_offset = 0;
                auto base_offset_opt = child.find(llvm::dwarf::DW_AT_data_member_location);
                if (base_offset_opt)
                {
                    base_offset = base_offset_opt->getAsUnsignedConstant().value_or(0);
                }

                llvm::codeview::MemberAccess access = llvm::codeview::MemberAccess::Public;
                auto access_opt = child.find(llvm::dwarf::DW_AT_accessibility);
                if (access_opt)
                {
                    uint64_t acc_val = access_opt->getAsUnsignedConstant().value_or(3);
                    if (acc_val == llvm::dwarf::DW_ACCESS_private)
                    {
                        access = llvm::codeview::MemberAccess::Private;
                    }
                    else if (acc_val == llvm::dwarf::DW_ACCESS_protected)
                    {
                        access = llvm::codeview::MemberAccess::Protected;
                    }
                }

                llvm::codeview::BaseClassRecord base_rec(access, base_type_idx, base_offset);
                field_builder.writeMemberType(base_rec);
            }
            else if (tag == llvm::dwarf::DW_TAG_subprogram)
            {
                const char* func_name_ptr = child.getName(llvm::DINameKind::ShortName);
                llvm::StringRef func_name = func_name_ptr ? func_name_ptr : "";

                llvm::codeview::TypeIndex ret_idx = llvm::codeview::TypeIndex::Void();
                auto type_opt = child.find(llvm::dwarf::DW_AT_type);
                if (type_opt)
                {
                    llvm::DWARFDie ret_type_die =
                        child.resolveReferencedType(llvm::dwarf::DW_AT_type);
                    ret_idx = TranslateType(ret_type_die, ctx);
                }

                std::vector<llvm::codeview::TypeIndex> args;
                llvm::codeview::TypeIndex this_idx = llvm::codeview::TypeIndex::None();
                bool is_static = true;

                for (const auto& param : child.children())
                {
                    if (param.getTag() == llvm::dwarf::DW_TAG_formal_parameter)
                    {
                        const char* param_name = param.getName(llvm::DINameKind::ShortName);
                        llvm::DWARFDie param_type_die =
                            param.resolveReferencedType(llvm::dwarf::DW_AT_type);
                        llvm::codeview::TypeIndex param_type_idx =
                            TranslateType(param_type_die, ctx);

                        if (param_name && std::strcmp(param_name, "this") == 0)
                        {
                            this_idx = param_type_idx;
                            is_static = false;
                        }
                        else
                        {
                            args.push_back(param_type_idx);
                        }
                    }
                }

                llvm::codeview::ArgListRecord arg_list_rec(llvm::codeview::TypeRecordKind::ArgList,
                                                           args);
                llvm::codeview::TypeIndex arg_list_idx =
                    ctx.m_typeBuilder.writeLeafType(arg_list_rec);

                llvm::codeview::TypeIndex method_sig_idx;
                llvm::codeview::MethodKind method_kind;

                if (is_static)
                {
                    llvm::codeview::ProcedureRecord proc_rec(
                        ret_idx,
                        llvm::codeview::CallingConvention::NearC,
                        llvm::codeview::FunctionOptions::None,
                        args.size(),
                        arg_list_idx);
                    method_sig_idx = ctx.m_typeBuilder.writeLeafType(proc_rec);
                    method_kind = llvm::codeview::MethodKind::Static;
                }
                else
                {
                    llvm::codeview::MemberFunctionRecord mfunc_rec(
                        ret_idx,
                        forward_idx,
                        this_idx,
                        llvm::codeview::CallingConvention::ThisCall,
                        llvm::codeview::FunctionOptions::None,
                        args.size(),
                        arg_list_idx,
                        0);
                    method_sig_idx = ctx.m_typeBuilder.writeLeafType(mfunc_rec);
                    method_kind = llvm::codeview::MethodKind::Vanilla;
                }

                llvm::codeview::MemberAccess access = llvm::codeview::MemberAccess::Public;
                auto access_opt = child.find(llvm::dwarf::DW_AT_accessibility);
                if (access_opt)
                {
                    uint64_t acc_val = access_opt->getAsUnsignedConstant().value_or(3);
                    if (acc_val == llvm::dwarf::DW_ACCESS_private)
                    {
                        access = llvm::codeview::MemberAccess::Private;
                    }
                    else if (acc_val == llvm::dwarf::DW_ACCESS_protected)
                    {
                        access = llvm::codeview::MemberAccess::Protected;
                    }
                }

                llvm::codeview::OneMethodRecord method_rec(method_sig_idx,
                                                           access,
                                                           method_kind,
                                                           llvm::codeview::MethodOptions::None,
                                                           0,
                                                           func_name);
                field_builder.writeMemberType(method_rec);
            }
            else if (tag == llvm::dwarf::DW_TAG_structure_type ||
                     tag == llvm::dwarf::DW_TAG_class_type ||
                     tag == llvm::dwarf::DW_TAG_union_type ||
                     tag == llvm::dwarf::DW_TAG_enumeration_type)
            {
                const char* nested_name_ptr = child.getName(llvm::DINameKind::ShortName);
                if (nested_name_ptr && nested_name_ptr[0] != '\0')
                {
                    llvm::codeview::TypeIndex nested_idx = TranslateType(child, ctx);
                    llvm::codeview::NestedTypeRecord nested_rec(nested_idx, nested_name_ptr);
                    field_builder.writeMemberType(nested_rec);
                }
            }
        }

        std::vector<llvm::codeview::CVType> field_records =
            field_builder.end(ctx.m_typeBuilder.nextTypeIndex());
        llvm::codeview::TypeIndex field_list_idx;
        for (const auto& rec : field_records)
        {
            llvm::ArrayRef<uint8_t> bytes = rec.data();
            field_list_idx = ctx.m_typeBuilder.insertRecordBytes(bytes);
        }

        llvm::codeview::ClassRecord class_rec(llvm::codeview::TypeRecordKind::Struct,
                                              member_count,
                                              llvm::codeview::ClassOptions::None,
                                              field_list_idx,
                                              llvm::codeview::TypeIndex::None(),
                                              llvm::codeview::TypeIndex::None(),
                                              size,
                                              name,
                                              "");
        resolved_idx = ctx.m_typeBuilder.writeLeafType(class_rec);
        break;
    }

    case llvm::dwarf::DW_TAG_enumeration_type:
    {
        std::string name = get_qualified_name(die);

        llvm::codeview::TypeIndex underlying_idx = llvm::codeview::TypeIndex::Int32();
        llvm::DWARFDie underlying_die = die.resolveReferencedType(llvm::dwarf::DW_AT_type);
        if (underlying_die)
        {
            underlying_idx = TranslateType(underlying_die, ctx);
        }

        llvm::codeview::ContinuationRecordBuilder enum_builder;
        enum_builder.begin(llvm::codeview::ContinuationRecordKind::FieldList);

        uint16_t member_count = 0;
        for (const auto& child : die.children())
        {
            if (child.getTag() == llvm::dwarf::DW_TAG_enumerator)
            {
                const char* enum_name_ptr = child.getName(llvm::DINameKind::ShortName);
                llvm::StringRef enum_name = enum_name_ptr ? enum_name_ptr : "";

                auto enum_val_opt = child.find(llvm::dwarf::DW_AT_const_value);
                int64_t enum_val =
                    enum_val_opt ? enum_val_opt->getAsSignedConstant().value_or(0) : 0;

                llvm::codeview::EnumeratorRecord enum_rec(
                    llvm::codeview::MemberAccess::Public,
                    llvm::APSInt(llvm::APInt(64, enum_val, true), false),
                    enum_name);
                enum_builder.writeMemberType(enum_rec);
                member_count++;
            }
        }

        llvm::codeview::TypeIndex field_list_idx;
        std::vector<llvm::codeview::CVType> field_records =
            enum_builder.end(ctx.m_typeBuilder.nextTypeIndex());
        for (const auto& rec : field_records)
        {
            llvm::ArrayRef<uint8_t> bytes = rec.data();
            field_list_idx = ctx.m_typeBuilder.insertRecordBytes(bytes);
        }

        llvm::codeview::EnumRecord enum_rec(member_count,
                                            llvm::codeview::ClassOptions::None,
                                            field_list_idx,
                                            name,
                                            "",
                                            underlying_idx);
        resolved_idx = ctx.m_typeBuilder.writeLeafType(enum_rec);
        break;
    }

    case llvm::dwarf::DW_TAG_array_type:
    {
        llvm::DWARFDie element_type_die = die.resolveReferencedType(llvm::dwarf::DW_AT_type);
        llvm::codeview::TypeIndex element_type_idx = TranslateType(element_type_die, ctx);

        auto size_opt = die.find(llvm::dwarf::DW_AT_byte_size);
        uint64_t size_in_bytes = size_opt ? size_opt->getAsUnsignedConstant().value_or(0) : 0;

        if (size_in_bytes == 0)
        {
            uint64_t element_count = 1;
            for (const auto& child : die.children())
            {
                if (child.getTag() == llvm::dwarf::DW_TAG_subrange_type)
                {
                    auto count_opt = child.find(llvm::dwarf::DW_AT_count);
                    if (count_opt)
                    {
                        element_count = count_opt->getAsUnsignedConstant().value_or(1);
                    }
                    else
                    {
                        auto upper_opt = child.find(llvm::dwarf::DW_AT_upper_bound);
                        if (upper_opt)
                        {
                            element_count = upper_opt->getAsUnsignedConstant().value_or(0) + 1;
                        }
                    }
                }
            }
            auto elem_size_opt = element_type_die.find(llvm::dwarf::DW_AT_byte_size);
            uint64_t elem_size =
                elem_size_opt ? elem_size_opt->getAsUnsignedConstant().value_or(0) : 0;
            if (elem_size == 0)
            {
                if (element_type_die.getTag() == llvm::dwarf::DW_TAG_pointer_type)
                {
                    elem_size = ctx.m_elfObj->getBytesInAddress();
                }
            }
            size_in_bytes = element_count * elem_size;
        }

        llvm::codeview::ArrayRecord array_rec(
            element_type_idx, llvm::codeview::TypeIndex::Int32(), size_in_bytes, "");
        resolved_idx = ctx.m_typeBuilder.writeLeafType(array_rec);
        break;
    }

    case llvm::dwarf::DW_TAG_union_type:
    {
        std::string name = get_qualified_name(die);

        auto size_opt = die.find(llvm::dwarf::DW_AT_byte_size);
        uint64_t size = size_opt ? size_opt->getAsUnsignedConstant().value_or(0) : 0;

        uint16_t member_count = 0;
        for (const auto& child : die.children())
        {
            if (child.getTag() == llvm::dwarf::DW_TAG_member)
            {
                member_count++;
            }
        }

        if (member_count == 0 && size == 0)
        {
            llvm::codeview::UnionRecord union_rec(0,
                                                  llvm::codeview::ClassOptions::ForwardReference,
                                                  llvm::codeview::TypeIndex::None(),
                                                  0,
                                                  name,
                                                  "");
            resolved_idx = ctx.m_typeBuilder.writeLeafType(union_rec);
            break;
        }

        // Pre-register forward reference to prevent infinite recursion
        llvm::codeview::UnionRecord forward_union(0,
                                                  llvm::codeview::ClassOptions::ForwardReference,
                                                  llvm::codeview::TypeIndex::None(),
                                                  0,
                                                  name,
                                                  "");
        llvm::codeview::TypeIndex forward_idx = ctx.m_typeBuilder.writeLeafType(forward_union);
        ctx.m_typeMap[offset] = forward_idx;

        llvm::codeview::ContinuationRecordBuilder field_builder;
        field_builder.begin(llvm::codeview::ContinuationRecordKind::FieldList);

        for (const auto& child : die.children())
        {
            auto tag = child.getTag();
            if (tag == llvm::dwarf::DW_TAG_member)
            {
                const char* mem_name_ptr = child.getName(llvm::DINameKind::ShortName);
                llvm::StringRef mem_name = mem_name_ptr ? mem_name_ptr : "";

                llvm::DWARFDie mem_type_die = child.resolveReferencedType(llvm::dwarf::DW_AT_type);
                llvm::codeview::TypeIndex mem_type_idx = TranslateType(mem_type_die, ctx);

                llvm::codeview::DataMemberRecord mem_rec(
                    llvm::codeview::MemberAccess::Public, mem_type_idx, 0, mem_name);
                field_builder.writeMemberType(mem_rec);
            }
            else if (tag == llvm::dwarf::DW_TAG_structure_type ||
                     tag == llvm::dwarf::DW_TAG_class_type ||
                     tag == llvm::dwarf::DW_TAG_union_type ||
                     tag == llvm::dwarf::DW_TAG_enumeration_type)
            {
                const char* nested_name_ptr = child.getName(llvm::DINameKind::ShortName);
                if (nested_name_ptr && nested_name_ptr[0] != '\0')
                {
                    llvm::codeview::TypeIndex nested_idx = TranslateType(child, ctx);
                    llvm::codeview::NestedTypeRecord nested_rec(nested_idx, nested_name_ptr);
                    field_builder.writeMemberType(nested_rec);
                }
            }
        }

        std::vector<llvm::codeview::CVType> field_records =
            field_builder.end(ctx.m_typeBuilder.nextTypeIndex());
        llvm::codeview::TypeIndex field_list_idx;
        for (const auto& rec : field_records)
        {
            llvm::ArrayRef<uint8_t> bytes = rec.data();
            field_list_idx = ctx.m_typeBuilder.insertRecordBytes(bytes);
        }

        llvm::codeview::UnionRecord union_rec(
            member_count, llvm::codeview::ClassOptions::None, field_list_idx, size, name, "");
        resolved_idx = ctx.m_typeBuilder.writeLeafType(union_rec);
        break;
    }

    case llvm::dwarf::DW_TAG_subprogram:
    {
        std::string parent_name = get_qualified_name(die);
        const char* short_name = die.getName(llvm::DINameKind::ShortName);
        std::string name = parent_name.empty() ? short_name : (parent_name + "::" + short_name);

        llvm::codeview::TypeIndex proc_idx = translate_procedure_type(die, ctx);

        llvm::codeview::FuncIdRecord func_id_rec(llvm::codeview::TypeIndex::None(), proc_idx, name);
        resolved_idx = ctx.m_typeBuilder.writeLeafType(func_id_rec);
        break;
    }

    case llvm::dwarf::DW_TAG_subroutine_type:
    {
        resolved_idx = translate_procedure_type(die, ctx);
        break;
    }

    default:
        break;
    }

    ctx.m_typeMap[offset] = resolved_idx;
    return resolved_idx;
}

} // namespace repeat
