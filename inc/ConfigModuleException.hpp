//ConfigModuleException
#pragma once

#include <stdexcept>

class ConfigModuleException : public std::runtime_error {
public:
    explicit ConfigModuleException(const std::string& message)
        :  std::runtime_error(message){};
 
};
