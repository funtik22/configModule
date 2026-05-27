// main.cpp

#include <iostream>

#include "ConfigModule.hpp"

#define BACKUP_DIRECTORY "../backup"
#define TRANSACTION_TIMEOUT 5


int main() {

    
    auto connection = std::make_shared<sysrepo::Connection>();

    auto configModule = std::make_unique<ConfigModule>(
        BACKUP_DIRECTORY, std::chrono::seconds(TRANSACTION_TIMEOUT),
        connection);


    configModule->initialize();

    std::cout << "Running... Press Ctrl+C to stop\n";

    configModule->start();
    
    std::cin.get();

    configModule->stop();
}