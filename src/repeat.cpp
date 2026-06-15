#include "repeat/repeat.h"

#include <filesystem>
#include <system_error>

#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Support/raw_ostream.h>

#include "repeat/args.h"
#include "repeat/error.h"
#include "repeat/translator.h"

namespace repeat
{

Repeat::Repeat(std::ostream& out, std::ostream& err) : m_out(out), m_err(err)
{
}

int Repeat::Run(int argc, const char* const argv[])
{
    std::string program_name = "repeat";
    if (argc >= 1)
    {
        program_name = std::filesystem::path(argv[0]).filename().string();
    }

    try
    {
        ParsedArgs args = ArgsParser::Parse(argc, argv, m_out, m_err);
        if (args.m_showHelp)
        {
            ArgsParser::PrintHelp(program_name, m_out);
            return 0;
        }

        llvm::raw_os_ostream raw_err(m_err);
        Translator translator(args.m_inputElf, args.m_columnInfo, raw_err);
        std::error_code ec = translator.Load();
        if (ec)
        {
            throw RepeatException("loading ELF file '" + args.m_inputElf + "': " + ec.message());
        }

        std::error_code write_ec;
        llvm::raw_fd_ostream os(args.m_outputAssembly, write_ec);
        if (write_ec)
        {
            throw RepeatException("opening output file '" + args.m_outputAssembly +
                                  "': " + write_ec.message());
        }

        translator.Translate(os);
        os.flush();

        return 0;
    }
    catch (const CommandLineException& e)
    {
        m_err << "error: " << e.what() << "\n";
        if (e.ShouldShowHelp())
        {
            ArgsParser::PrintHelp(program_name, m_out);
        }
        return 1;
    }
    catch (const RepeatException& e)
    {
        m_err << "error: " << e.what() << "\n";
        return 1;
    }
    catch (const std::exception& e)
    {
        m_err << "error: " << e.what() << "\n";
        return 1;
    }
    catch (...)
    {
        m_err << "error: unknown error occurred\n";
        return 1;
    }
}

} // namespace repeat
