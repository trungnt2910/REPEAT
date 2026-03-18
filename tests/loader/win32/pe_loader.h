#ifndef REPEAT_TESTS_LOADER_PE_LOADER_H
#define REPEAT_TESTS_LOADER_PE_LOADER_H

#include <string>

namespace repeat
{
namespace test
{

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
    void* m_module; // Use void* to avoid including Windows.h in this header.
};

} // namespace test
} // namespace repeat

#endif // REPEAT_TESTS_LOADER_PE_LOADER_H
