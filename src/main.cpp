#include <iostream>

#include "repeat/repeat.h"

int main(int argc, char** argv)
{
    repeat::Repeat app(std::cout, std::cerr);
    return app.Run(argc, argv);
}
