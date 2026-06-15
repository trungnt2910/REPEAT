#include "repeat/args.h"

#include <algorithm>
#include <iomanip>
#include <iostream>

#include "repeat/error.h"

namespace repeat
{

const std::vector<Option>& ArgsParser::GetOptionDefs()
{
    static const std::vector<Option> optionDefs = {
        {"-h", "--help", "", "Display available options"},
        {"-o", "", "<file>", "Write output to <file>"},
        {"-gcolumn-info", "", "", "Emit column number information in CodeView line tables"},
        {"-gno-column-info", "", "", "Do not emit column number information (default)"},
    };
    return optionDefs;
}

ParsedArgs
ArgsParser::Parse(int argc, const char* const argv[], std::ostream& out, std::ostream& err)
{
    ParsedArgs args;
    if (argc < 1)
    {
        throw CommandLineException("empty arguments list");
    }

    const auto& optionDefs = GetOptionDefs();

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        auto it = std::find_if(optionDefs.begin(),
                               optionDefs.end(),
                               [&arg](const Option& opt)
                               {
                                   return (!opt.m_shortFlag.empty() && arg == opt.m_shortFlag) ||
                                          (!opt.m_longFlag.empty() && arg == opt.m_longFlag);
                               });

        if (it != optionDefs.end())
        {
            if (it->m_valueName.empty())
            {
                if (it->m_shortFlag == "-h" || it->m_longFlag == "--help")
                {
                    args.m_showHelp = true;
                    return args;
                }
                else if (it->m_shortFlag == "-gcolumn-info")
                {
                    args.m_columnInfo = true;
                }
                else if (it->m_shortFlag == "-gno-column-info")
                {
                    args.m_columnInfo = false;
                }
            }
            else
            {
                if (i + 1 >= argc)
                {
                    throw CommandLineException("missing argument after '" + arg + "'");
                }
                std::string val = argv[++i];
                if (it->m_shortFlag == "-o")
                {
                    args.m_outputAssembly = val;
                }
            }
        }
        else if (!arg.empty() && arg[0] == '-')
        {
            throw CommandLineException("unknown argument: '" + arg + "'", true);
        }
        else
        {
            if (!args.m_inputElf.empty())
            {
                throw CommandLineException("more than one input file specified", true);
            }
            args.m_inputElf = arg;
        }
    }

    if (args.m_inputElf.empty())
    {
        throw CommandLineException("no input file specified", true);
    }

    if (args.m_outputAssembly.empty())
    {
        throw CommandLineException("no output file specified", true);
    }

    return args;
}

void ArgsParser::PrintHelp(const std::string& programName, std::ostream& out)
{
    out << "OVERVIEW: REPEAT ELF repackager\n\n";
    out << "USAGE: " << programName << " [options] <input_elf>\n\n";
    out << "OPTIONS:\n";

    const auto& optionDefs = GetOptionDefs();

    size_t maxWidth = 0;
    std::vector<std::string> optionStrings;
    optionStrings.reserve(optionDefs.size());

    for (const auto& opt : optionDefs)
    {
        std::string optStr = "  ";
        if (!opt.m_shortFlag.empty())
        {
            optStr += opt.m_shortFlag;
            if (!opt.m_longFlag.empty())
            {
                optStr += ", " + opt.m_longFlag;
            }
        }
        else if (!opt.m_longFlag.empty())
        {
            optStr += opt.m_longFlag;
        }

        if (!opt.m_valueName.empty())
        {
            optStr += " " + opt.m_valueName;
        }

        maxWidth = std::max(maxWidth, optStr.size());
        optionStrings.push_back(optStr);
    }

    size_t padding = maxWidth + 4;
    for (size_t i = 0; i < optionDefs.size(); ++i)
    {
        out << std::left << std::setw(padding) << optionStrings[i] << optionDefs[i].m_description
            << "\n";
    }
}

} // namespace repeat
