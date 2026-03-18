#ifndef REPEAT_ERROR_H
#define REPEAT_ERROR_H

#include <stdexcept>
#include <string>

namespace repeat
{

class RepeatException : public std::runtime_error
{
public:
    explicit RepeatException(const std::string& message) : std::runtime_error(message)
    {
    }
};

class CommandLineException : public RepeatException
{
public:
    explicit CommandLineException(const std::string& message, bool shouldShowHelp = false)
        : RepeatException(message), m_shouldShowHelp(shouldShowHelp)
    {
    }

    bool ShouldShowHelp() const
    {
        return m_shouldShowHelp;
    }

private:
    bool m_shouldShowHelp;
};

} // namespace repeat

#endif // REPEAT_ERROR_H
