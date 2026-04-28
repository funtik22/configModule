//HardwareEmulator.cpp

#include "HardwareEmulator.hpp"

bool HardwareEmulator::setParameter(const std::string& id, const ParamValue& value) {

    auto it = state.find(id);
    if (it == state.end()) {
        Parameter p;
        p.id           = id;
        p.value        = value;
        state.emplace(id, std::move(p));
    } else {
        it->second.value        = value;
    }
    return true;
}

bool HardwareEmulator::removeParameter(const std::string& id) {
    return state.erase(id) > 0;
}

std::optional<Parameter>
HardwareEmulator::getParameter(const std::string& id) const {
    auto it = state.find(id);
    if (it == state.end()) return std::nullopt;
    return it->second;
}

std::map<std::string, Parameter> HardwareEmulator::getAllParameters() const {
    return state;
}

void HardwareEmulator::clear() {
    state.clear();
}