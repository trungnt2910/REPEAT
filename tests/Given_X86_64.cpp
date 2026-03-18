#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include "RepeatTest.h"
#include "pe_loader.h"
#include "repeat/translator.h"

#if defined(__x86_64__) || defined(_M_X64)
#define REPEAT_HOST_IS_X86_64 1
#else
#define REPEAT_HOST_IS_X86_64 0
#endif

#define REPEAT_MS_ABI __attribute__((ms_abi))

namespace repeat
{
namespace test
{

class Given_X8664 : public RepeatTest
{
};

TEST_F(Given_X8664, When_CallSimpleFunction_ReturnsCorrectValue)
{
#if !REPEAT_HOST_IS_X86_64
    GTEST_SKIP() << "Skipping integration test: host is not x86_64";
#else
    std::string dll_path = GetTestDataPath("data/../test_image.dll");
    ASSERT_TRUE(std::filesystem::exists(dll_path)) << "DLL file not found: " << dll_path;
    PELoader loader;
    bool success = loader.Load(dll_path);
    ASSERT_TRUE(success) << "Failed to load PE DLL: " << dll_path;
    auto get_val_ptr = reinterpret_cast<int(REPEAT_MS_ABI*)(int)>(loader.GetSymbol("get_val"));
    ASSERT_NE(get_val_ptr, nullptr) << "Failed to find symbol get_val";

    int result = get_val_ptr(10);

    EXPECT_EQ(result, 52);
#endif
}

TEST_F(Given_X8664, When_GetGlobalPointer_ReturnsInitialValue)
{
#if !REPEAT_HOST_IS_X86_64
    GTEST_SKIP() << "Skipping integration test: host is not x86_64";
#else
    std::string dll_path = GetTestDataPath("data/../test_image.dll");
    ASSERT_TRUE(std::filesystem::exists(dll_path)) << "DLL file not found: " << dll_path;
    PELoader loader;
    bool success = loader.Load(dll_path);
    ASSERT_TRUE(success) << "Failed to load PE DLL: " << dll_path;
    auto get_global_ptr =
        reinterpret_cast<int*(REPEAT_MS_ABI*)()>(loader.GetSymbol("get_global_ptr"));
    ASSERT_NE(get_global_ptr, nullptr) << "Failed to find symbol get_global_ptr";

    int* ptr = get_global_ptr();

    ASSERT_NE(ptr, nullptr) << "get_global_ptr returned null";
    EXPECT_EQ(*ptr, 100);
#endif
}

TEST_F(Given_X8664, When_WriteToGlobalPointer_Succeeds)
{
#if !REPEAT_HOST_IS_X86_64
    GTEST_SKIP() << "Skipping integration test: host is not x86_64";
#else
    std::string dll_path = GetTestDataPath("data/../test_image.dll");
    ASSERT_TRUE(std::filesystem::exists(dll_path)) << "DLL file not found: " << dll_path;
    PELoader loader;
    bool success = loader.Load(dll_path);
    ASSERT_TRUE(success) << "Failed to load PE DLL: " << dll_path;
    auto get_global_ptr =
        reinterpret_cast<int*(REPEAT_MS_ABI*)()>(loader.GetSymbol("get_global_ptr"));
    ASSERT_NE(get_global_ptr, nullptr) << "Failed to find symbol get_global_ptr";
    int* ptr = get_global_ptr();
    ASSERT_NE(ptr, nullptr) << "get_global_ptr returned null";

    *ptr = 200;

    EXPECT_EQ(*ptr, 200);
#endif
}

TEST_F(Given_X8664, When_PECallsTranslatedELF_Succeeds)
{
#if !REPEAT_HOST_IS_X86_64
    GTEST_SKIP() << "Skipping integration test: host is not x86_64";
#else
    std::string dll_path = GetTestDataPath("data/../test_image.dll");
    ASSERT_TRUE(std::filesystem::exists(dll_path)) << "DLL file not found: " << dll_path;
    PELoader loader;
    bool success = loader.Load(dll_path);
    ASSERT_TRUE(success) << "Failed to load PE DLL: " << dll_path;
    auto pe_call_elf_ptr =
        reinterpret_cast<int(REPEAT_MS_ABI*)(int)>(loader.GetSymbol("pe_call_elf"));
    ASSERT_NE(pe_call_elf_ptr, nullptr) << "Failed to find symbol pe_call_elf";

    int result = pe_call_elf_ptr(5);

    EXPECT_EQ(result, 15);
#endif
}

TEST_F(Given_X8664, When_TranslatedELFCallsPE_Succeeds)
{
#if !REPEAT_HOST_IS_X86_64
    GTEST_SKIP() << "Skipping integration test: host is not x86_64";
#else
    std::string dll_path = GetTestDataPath("data/../test_image.dll");
    ASSERT_TRUE(std::filesystem::exists(dll_path)) << "DLL file not found: " << dll_path;
    PELoader loader;
    bool success = loader.Load(dll_path);
    ASSERT_TRUE(success) << "Failed to load PE DLL: " << dll_path;
    auto elf_call_pe_ptr =
        reinterpret_cast<int(REPEAT_MS_ABI*)(int)>(loader.GetSymbol("elf_call_pe"));
    ASSERT_NE(elf_call_pe_ptr, nullptr) << "Failed to find symbol elf_call_pe";

    int result = elf_call_pe_ptr(4);

    EXPECT_EQ(result, 12);
#endif
}

} // namespace test
} // namespace repeat
