// ConfigTypes.cpp
#include "ConfigTypes.hpp"

std::string configValueToString(
    const std::variant<std::string, int64_t, double, bool> &value) {
    return std::visit(
        [](const auto &v) -> std::string {
            if constexpr (std::is_same_v<std::decay_t<decltype(v)>,
                                         std::string>)
                return v;
            else if constexpr (std::is_same_v<std::decay_t<decltype(v)>, bool>)
                return v ? "true" : "false";
            else
                return std::to_string(v);
        },
        value);
}

std::ostream& operator<<(std::ostream& os, const Configuration& cfg) {
    os << "Configuration (" << cfg.values.size() << " values):\n";
    for (const auto& [xpath, cv] : cfg.values) {

        std::string typeName = std::visit([](const auto& v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>)  return "string";
            if constexpr (std::is_same_v<T, int64_t>)      return "int64_t";
            if constexpr (std::is_same_v<T, double>)        return "double";
            if constexpr (std::is_same_v<T, bool>)          return "bool";
        }, cv.value);
        os << "  module : " << cv.yangModule << "\n" 
           << "  xpath  : " << xpath << "\n"
           << "  type   : " << typeName << "\n"
           << "  value  : " << configValueToString(cv.value) << "\n\n";
    }
    return os;
}

std::ostream &operator<<(std::ostream &os, const ConfigChange &change) {
    os << "Change:\n"
       << "  xpath    : " << change.xpath << "\n"
       << "  oldValue : " << configValueToString(*(change).oldValue) << "\n"
       << "  newValue : " << configValueToString(change.newValue) << "\n";
    return os;
}