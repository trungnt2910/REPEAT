#ifndef REPEAT_TESTS_REPEAT_TEST_H
#define REPEAT_TESTS_REPEAT_TEST_H

#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

class TemporaryFile
{
    std::filesystem::path m_path;

public:
    explicit TemporaryFile(std::filesystem::path p) : m_path(std::move(p))
    {
        std::filesystem::remove(m_path);
    }

    ~TemporaryFile()
    {
        std::filesystem::remove(m_path);
    }

    TemporaryFile(const TemporaryFile&) = delete;
    TemporaryFile& operator=(const TemporaryFile&) = delete;

    const std::filesystem::path& GetPath() const
    {
        return m_path;
    }
};

class RepeatTest : public ::testing::Test
{
protected:
    std::string GetTestDataPath(const std::string& rel_path)
    {
        return rel_path;
    }

    std::string GetTestTextContent(const std::string& rel_path)
    {
        std::string path = GetTestDataPath(rel_path);
        std::ifstream f(path);
        if (!f.is_open())
        {
            return "";
        }
        std::istreambuf_iterator<char> it(f);
        std::string content(it, std::istreambuf_iterator<char>());
        return content;
    }

    void SaveBadOutput(const std::string& content, const std::string& rel_path)
    {
        std::filesystem::path target_path = std::filesystem::path("artifacts") / rel_path;
        std::error_code ec;
        std::filesystem::create_directories(target_path.parent_path(), ec);
        if (ec)
        {
            ADD_FAILURE() << "Failed to create directories for " << target_path << ": "
                          << ec.message();
            return;
        }
        std::ofstream f(target_path);
        if (!f.is_open())
        {
            ADD_FAILURE() << "Failed to open " << target_path << " for writing";
            return;
        }
        f << content;
    }

    void ExpectOutputMatchesGolden(const std::string& output, const std::string& golden_rel_path)
    {
        std::string expected = GetTestTextContent(golden_rel_path);
        ASSERT_FALSE(expected.empty()) << "Failed to load golden file: " << golden_rel_path;

        if (output != expected)
        {
            SaveBadOutput(output, golden_rel_path);
        }
        EXPECT_EQ(output, expected);
    }
};

#endif // REPEAT_TESTS_REPEAT_TEST_H
