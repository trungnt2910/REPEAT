#ifndef REPEAT_REPEAT_H
#define REPEAT_REPEAT_H

#include <iostream>

namespace repeat
{

class Repeat
{
public:
    explicit Repeat(std::ostream& out = std::cout, std::ostream& err = std::cerr);
    ~Repeat() = default;

    int Run(int argc, const char* const argv[]);

private:
    std::ostream& m_out;
    std::ostream& m_err;
};

} // namespace repeat

#endif // REPEAT_REPEAT_H
