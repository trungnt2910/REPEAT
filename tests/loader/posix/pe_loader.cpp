#include "pe_loader.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#include <sys/mman.h>
#include <unistd.h>

namespace repeat
{
namespace test
{

PELoader::PELoader() : m_mappedAddr(nullptr), m_mappedSize(0), m_entryPoint(0)
{
}

PELoader::~PELoader()
{
    Unload();
}

void PELoader::Unload()
{
    if (m_mappedAddr != nullptr && m_mappedSize > 0)
    {
        munmap(m_mappedAddr, m_mappedSize);
        m_mappedAddr = nullptr;
        m_mappedSize = 0;
        m_entryPoint = 0;
    }
}

uintptr_t PELoader::GetBaseAddress() const
{
    return reinterpret_cast<uintptr_t>(m_mappedAddr);
}

const IMAGE_NT_HEADERS64* PELoader::GetNtHeaders() const
{
    if (m_mappedAddr == nullptr)
    {
        return nullptr;
    }
    const auto* dos_header = static_cast<const IMAGE_DOS_HEADER*>(m_mappedAddr);
    return reinterpret_cast<const IMAGE_NT_HEADERS64*>(static_cast<const char*>(m_mappedAddr) +
                                                       dos_header->e_lfanew);
}

const IMAGE_SECTION_HEADER* PELoader::GetSectionHeaders() const
{
    const auto* nt_headers = GetNtHeaders();
    if (nt_headers == nullptr)
    {
        return nullptr;
    }
    return reinterpret_cast<const IMAGE_SECTION_HEADER*>(
        reinterpret_cast<const char*>(&nt_headers->OptionalHeader) +
        nt_headers->FileHeader.SizeOfOptionalHeader);
}

void* PELoader::RvaToPtr(uint32_t rva) const
{
    if (m_mappedAddr == nullptr || rva >= m_mappedSize)
    {
        return nullptr;
    }
    return static_cast<char*>(m_mappedAddr) + rva;
}

bool PELoader::Load(const std::string& pe_path)
{
    Unload();

    std::ifstream file(pe_path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        std::cerr << "Failed to open PE file: " << pe_path << "\n";
        return false;
    }

    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> file_data(file_size);
    if (!file.read(file_data.data(), file_size))
    {
        std::cerr << "Failed to read PE file contents.\n";
        return false;
    }

    if (file_size < static_cast<std::streamsize>(sizeof(IMAGE_DOS_HEADER)))
    {
        std::cerr << "File too small to be a PE (missing DOS Header).\n";
        return false;
    }
    const auto* dos_header = reinterpret_cast<const IMAGE_DOS_HEADER*>(file_data.data());
    if (dos_header->e_magic != 0x5A4D) // "MZ"
    {
        std::cerr << "Invalid DOS magic number.\n";
        return false;
    }

    const auto min_size =
        static_cast<std::streamsize>(dos_header->e_lfanew + sizeof(IMAGE_NT_HEADERS64));
    if (file_size < min_size)
    {
        std::cerr << "File too small to be a PE (missing NT Headers).\n";
        return false;
    }
    const auto* nt_headers =
        reinterpret_cast<const IMAGE_NT_HEADERS64*>(file_data.data() + dos_header->e_lfanew);
    if (nt_headers->Signature != 0x00004550) // "PE\0\0"
    {
        std::cerr << "Invalid PE signature.\n";
        return false;
    }

    m_mappedSize = nt_headers->OptionalHeader.SizeOfImage;
    m_mappedAddr = mmap(nullptr,
                        m_mappedSize,
                        PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS,
                        -1,
                        0);
    if (m_mappedAddr == MAP_FAILED)
    {
        std::cerr << "Failed to allocate memory for PE image.\n";
        m_mappedAddr = nullptr;
        m_mappedSize = 0;
        return false;
    }

    std::memcpy(m_mappedAddr, file_data.data(), nt_headers->OptionalHeader.SizeOfHeaders);

    const IMAGE_SECTION_HEADER* sections = reinterpret_cast<const IMAGE_SECTION_HEADER*>(
        file_data.data() + dos_header->e_lfanew + sizeof(uint32_t) + sizeof(IMAGE_FILE_HEADER) +
        nt_headers->FileHeader.SizeOfOptionalHeader);

    for (uint16_t i = 0; i < nt_headers->FileHeader.NumberOfSections; ++i)
    {
        const auto& sec = sections[i];
        if (sec.VirtualAddress >= m_mappedSize)
        {
            continue;
        }

        void* dest = static_cast<char*>(m_mappedAddr) + sec.VirtualAddress;

        uint32_t copy_size = std::min(sec.SizeOfRawData, sec.Misc.VirtualSize);
        if (copy_size > 0 && sec.PointerToRawData < file_size)
        {
            std::memcpy(dest, file_data.data() + sec.PointerToRawData, copy_size);
        }

        if (sec.Misc.VirtualSize > sec.SizeOfRawData)
        {
            std::memset(static_cast<char*>(dest) + sec.SizeOfRawData,
                        0,
                        sec.Misc.VirtualSize - sec.SizeOfRawData);
        }
    }

    uintptr_t actual_base = reinterpret_cast<uintptr_t>(m_mappedAddr);
    uintptr_t preferred_base = nt_headers->OptionalHeader.ImageBase;
    intptr_t delta = actual_base - preferred_base;

    if (delta != 0)
    {
        const IMAGE_DATA_DIRECTORY& reloc_dir =
            nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        if (reloc_dir.Size > 0 && reloc_dir.VirtualAddress > 0)
        {
            const char* reloc_table_start =
                static_cast<const char*>(m_mappedAddr) + reloc_dir.VirtualAddress;
            const char* reloc_table_end = reloc_table_start + reloc_dir.Size;
            const char* curr_ptr = reloc_table_start;

            while (curr_ptr < reloc_table_end)
            {
                const auto* block = reinterpret_cast<const IMAGE_BASE_RELOCATION*>(curr_ptr);
                if (block->SizeOfBlock == 0)
                {
                    break;
                }

                uint32_t num_entries =
                    (block->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(uint16_t);
                const uint16_t* entries =
                    reinterpret_cast<const uint16_t*>(curr_ptr + sizeof(IMAGE_BASE_RELOCATION));

                for (uint32_t i = 0; i < num_entries; ++i)
                {
                    uint16_t type = entries[i] >> 12;
                    uint16_t offset = entries[i] & 0x0FFF;

                    if (type == IMAGE_REL_BASED_DIR64)
                    {
                        uint32_t target_rva = block->VirtualAddress + offset;
                        auto* patch_ptr = reinterpret_cast<uint64_t*>(
                            static_cast<char*>(m_mappedAddr) + target_rva);
                        *patch_ptr += delta;
                    }
                }
                curr_ptr += block->SizeOfBlock;
            }
        }
    }

    m_entryPoint = actual_base + nt_headers->OptionalHeader.AddressOfEntryPoint;
    return true;
}

void* PELoader::GetSymbol(const std::string& name) const
{
    const auto* nt_headers = GetNtHeaders();
    if (nt_headers == nullptr)
    {
        return nullptr;
    }

    const IMAGE_DATA_DIRECTORY& export_dir =
        nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (export_dir.Size == 0 || export_dir.VirtualAddress == 0)
    {
        return nullptr;
    }

    const auto* exports =
        static_cast<const IMAGE_EXPORT_DIRECTORY*>(RvaToPtr(export_dir.VirtualAddress));
    if (exports == nullptr)
    {
        return nullptr;
    }

    const char* names_base = static_cast<const char*>(RvaToPtr(exports->AddressOfNames));
    const char* ordinals_base = static_cast<const char*>(RvaToPtr(exports->AddressOfNameOrdinals));
    const char* functions_base = static_cast<const char*>(RvaToPtr(exports->AddressOfFunctions));

    if (names_base == nullptr || ordinals_base == nullptr || functions_base == nullptr)
    {
        return nullptr;
    }

    for (uint32_t i = 0; i < exports->NumberOfNames; ++i)
    {
        uint32_t name_rva;
        std::memcpy(&name_rva, names_base + i * sizeof(uint32_t), sizeof(uint32_t));

        const char* sym_name = static_cast<const char*>(RvaToPtr(name_rva));
        if (sym_name != nullptr && std::strcmp(sym_name, name.c_str()) == 0)
        {
            uint16_t ordinal;
            std::memcpy(&ordinal, ordinals_base + i * sizeof(uint16_t), sizeof(uint16_t));

            uint32_t func_rva;
            std::memcpy(&func_rva, functions_base + ordinal * sizeof(uint32_t), sizeof(uint32_t));

            return RvaToPtr(func_rva);
        }
    }

    return nullptr;
}

} // namespace test
} // namespace repeat
