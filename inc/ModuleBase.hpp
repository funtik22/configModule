#pragma once

#include <string>

class ModuleBase {
public:
    virtual ~ModuleBase() = default;
    virtual void initialize() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual std::string getName() const = 0;
    virtual std::string getStatus() const = 0;

protected:
    std::string name;
    bool isRunning = false; 
};