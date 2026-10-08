#include "Application/Application.hpp"

int main(int argc, char *argv[])
{
    using namespace ForradiaLang;

    if (argc >= 2)
    {
        Application application;

        application.Run(argv[1]);
    }
    else
    {
        std::cout << "Usage: ForradiaLang <filename>" << std::endl;
    }

    return 0;
}
