#include "Application.hpp"

// std
#include <cstdlib>
#include <iostream>
#include <stdexcept>


int main() 
{
    HedgehogClient::HedgehogClient app{};
    try
    {
        app.Run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

     return EXIT_SUCCESS;
}
