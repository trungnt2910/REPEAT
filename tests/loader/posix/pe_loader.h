#ifndef REPEAT_TESTS_LOADER_PE_LOADER_H
#define REPEAT_TESTS_LOADER_PE_LOADER_H

#include <cstdint>
#include <string>

namespace repeat
{
namespace test
{

struct IMAGE_DOS_HEADER
{
    uint16_t e_magic;    // Magic number ("MZ")
    uint16_t e_cblp;     // Bytes on last page of file
    uint16_t e_cp;       // Pages in file
    uint16_t e_crlc;     // Relocations
    uint16_t e_cparhdr;  // Size of header in paragraphs
    uint16_t e_minalloc; // Minimum extra paragraphs needed
    uint16_t e_maxalloc; // Maximum extra paragraphs needed
    uint16_t e_ss;       // Initial (relative) SS value
    uint16_t e_sp;       // Initial SP value
    uint16_t e_csum;     // Checksum
    uint16_t e_ip;       // Initial IP value
    uint16_t e_cs;       // Initial (relative) CS value
    uint16_t e_lfarlc;   // File address of relocation table
    uint16_t e_ovno;     // Overlay number
    uint16_t e_res[4];   // Reserved words
    uint16_t e_oemid;    // OEM identifier; e_oeminfo specific
    uint16_t e_oeminfo;  // OEM information; e_oemid specific
    uint16_t e_res2[10]; // Reserved words
    int32_t e_lfanew;    // File address of new exe header
};

struct IMAGE_FILE_HEADER
{
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
};

struct IMAGE_DATA_DIRECTORY
{
    uint32_t VirtualAddress;
    uint32_t Size;
};

#define IMAGE_NUMBEROF_DIRECTORY_ENTRIES 16

struct IMAGE_OPTIONAL_HEADER64
{
    uint16_t Magic;
    uint8_t MajorLinkerVersion;
    uint8_t MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve;
    uint64_t SizeOfStackCommit;
    uint64_t SizeOfHeapReserve;
    uint64_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
};

struct IMAGE_NT_HEADERS64
{
    uint32_t Signature; // "PE\0\0"
    IMAGE_FILE_HEADER FileHeader;
    IMAGE_OPTIONAL_HEADER64 OptionalHeader;
};

#define IMAGE_SIZEOF_SHORT_NAME 8

struct IMAGE_SECTION_HEADER
{
    uint8_t Name[IMAGE_SIZEOF_SHORT_NAME];
    union
    {
        uint32_t PhysicalAddress;
        uint32_t VirtualSize;
    } Misc;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
};

// Data Directory indexes
#define IMAGE_DIRECTORY_ENTRY_EXPORT 0
#define IMAGE_DIRECTORY_ENTRY_IMPORT 1
#define IMAGE_DIRECTORY_ENTRY_EXCEPTION 3
#define IMAGE_DIRECTORY_ENTRY_BASERELOC 5

struct IMAGE_BASE_RELOCATION
{
    uint32_t VirtualAddress;
    uint32_t SizeOfBlock;
};

#define IMAGE_REL_BASED_DIR64 10

struct IMAGE_EXPORT_DIRECTORY
{
    uint32_t Characteristics;
    uint32_t TimeDateStamp;
    uint16_t MajorVersion;
    uint16_t MinorVersion;
    uint32_t Name;
    uint32_t Base;
    uint32_t NumberOfFunctions;
    uint32_t NumberOfNames;
    uint32_t AddressOfFunctions;    // RVA from base of image
    uint32_t AddressOfNames;        // RVA from base of image
    uint32_t AddressOfNameOrdinals; // RVA from base of image
};

class PELoader
{
public:
    PELoader();
    ~PELoader();

    PELoader(const PELoader&) = delete;
    PELoader& operator=(const PELoader&) = delete;

    bool Load(const std::string& pe_path);

    void Unload();

    void* GetSymbol(const std::string& name) const;

    uintptr_t GetBaseAddress() const;

private:
    void* m_mappedAddr;
    size_t m_mappedSize;
    uintptr_t m_entryPoint;

    const IMAGE_NT_HEADERS64* GetNtHeaders() const;
    const IMAGE_SECTION_HEADER* GetSectionHeaders() const;
    void* RvaToPtr(uint32_t rva) const;
};

} // namespace test
} // namespace repeat

#endif // REPEAT_TESTS_LOADER_PE_LOADER_H
