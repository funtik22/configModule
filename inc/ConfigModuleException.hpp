//ConfigModuleException
#pragma once

#include <stdexcept>

class ConfigModuleException : public std::runtime_error {
public:
    /**
     * @brief Constructs the exception with a descriptive message.
     * @param reason Human-readable description of the error.
     */
    explicit ConfigModuleException(const std::string& message)
        :  std::runtime_error(message){};
 
};
