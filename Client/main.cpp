#include "Application.hpp"

// std
#include <cstdlib>
#include <iostream>
#include <stdexcept>


int main() 
{
    HedgehogClient::HedgehogClient app{};
    app.Run();
    return EXIT_SUCCESS;
}
