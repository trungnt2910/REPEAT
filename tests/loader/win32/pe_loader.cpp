#include "pe_loader.h"

#include <iostream>

#include <windows.h>

namespace repeat
{
namespace test
{

PELoader::PELoader() : m_module(nullptr)
{
}

PELoader::~PELoader()
{
    Unload();
}

void PELoader::Unload()
{
    if (m_module != nullptr)
    {
        FreeLibrary(static_cast<HMODULE>(m_module));
        m_module = nullptr;
    }
}

bool PELoader::Load(const std::string& pe_path)
{
    Unload();

    // Use LOAD_WITH_ALTERED_SEARCH_PATH to ensure DLL dependencies in the same directory are found.
    m_module = LoadLibraryExA(pe_path.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (m_module == nullptr)
    {
        std::cerr << "Failed to load PE DLL using LoadLibraryExA: " << pe_path;
        std::cerr << " (Error code: " << GetLastError() << ")\n";
        return false;
    }

    return true;
}

void* PELoader::GetSymbol(const std::string& name) const
{
    if (m_module == nullptr)
    {
        return nullptr;
    }
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(m_module), name.c_str()));
}

uintptr_t PELoader::GetBaseAddress() const
{
    return reinterpret_cast<uintptr_t>(m_module);
}

} // namespace test
} // namespace repeat
