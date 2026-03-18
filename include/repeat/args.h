#ifndef REPEAT_ARGS_H
#define REPEAT_ARGS_H

#include <iostream>
#include <string>
#include <vector>

namespace repeat
{

struct Option
{
    std::string m_shortFlag;
    std::string m_longFlag;
    std::string m_valueName;
    std::string m_description;
};

struct ParsedArgs
{
    std::string m_inputElf;
    std::string m_outputAssembly;
    bool m_showHelp = false;
};

class ArgsParser
{
public:
    static ParsedArgs
    Parse(int argc, const char* const argv[], std::ostream& out, std::ostream& err);
    static void PrintHelp(const std::string& programName, std::ostream& out);

private:
    static const std::vector<Option>& GetOptionDefs();
};

} // namespace repeat

#endif // REPEAT_ARGS_H
