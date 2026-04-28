//HardwareEmulator.hpp
#pragma once

#include <optional>
#include <string>
#include <variant>
#include <map>

using ParamValue =  std::variant<std::string, int64_t, double, bool>;

struct Parameter {
    std::string  id;
    ParamValue   value;
;
};

class HardwareEmulator {
public:
    bool setParameter(const std::string& id, const ParamValue& value);
    bool removeParameter(const std::string& id);
    std::optional<Parameter> getParameter(const std::string& id) const;
    std::map<std::string, Parameter> getAllParameters() const;
    void clear();
private:
    std::map<std::string, Parameter> state;
};

