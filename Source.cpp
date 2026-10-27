
#include <iostream>
#include <fstream>
#include <string>

int main()
{
    std::ifstream config("config.txt");

    if (!config.is_open())
    {
        std::cout << "ERROR: Cannot find config.txt\n";
        std::cout << "Check the file location and working directory.\n";
        return 1;
    }

    std::cout << "SUCCESS: config.txt was found!\n\n";

    std::string line;

    while (std::getline(config, line))
    {
        std::cout << line << '\n';
    }

    config.close();

    std::cout << "\nConfiguration test completed.\n";
    return 0;
}
