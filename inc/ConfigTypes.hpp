// ConfigTypes.hpp
#pragma once

#include <string>
#include <variant>
#include <chrono>
#include <map>
#include <vector>
#include <ostream>
#include <optional>

/**
 * @brief Represents a single configuration value with metadata.
 */
struct ConfigValue {
    std::string xpath;                                          ///< YANG xpath path
    std::string yangModule;                                     ///< YANG module name
    std::variant<std::string, int64_t, double, bool> value;    ///< Actual value
    std::chrono::system_clock::time_point createdTime;         ///< Creation timestamp
    std::chrono::system_clock::time_point modifiedTime;        ///< Last modification timestamp
};

/**
 * @brief Snapshot of the configuration.
 */
struct Configuration {
    std::map<std::string, ConfigValue> values; /// TO:DO Подучать что является ключом (xpath???)
};

/**
 * @brief Represents a single change.
 */
struct ConfigChange {
    std::string xpath;                                          ///< YANG xpath of changed node
    std::variant<std::string, int64_t, double, bool> newValue; ///< New value to apply
    std::optional<std::variant<std::string, int64_t, double, bool>> oldValue; ///< Previous value (for rollback)
};

/**
 * @brief Request to apply a set of configuration changes.
 */
struct ConfigApplyRequest {
    std::string requestId;              ///< Unique request identifier
    std::vector<ConfigChange> changes;  /// TO:DO Что за configChange? 
    std::string reason;                 ///< Reason for the change
    bool rollbackOnFailure;             ///< Whether to rollback all changes on any failure
};

/**
 * @brief Enumeration of all possible ConfigModule lifecycle states.
 */
enum class ConfigModuleState {
    UNINITIALIZED,
    INITIALIZING,
    INITIALIZED,
    READY,
    BUSY,
    APPLYING_CONFIGURATION,
    VALIDATING_CONFIGURATION,
    CREATING_BACKUP,
    RESTORING_FROM_BACKUP,
    CONFIGURATION_ERROR,
    VALIDATION_ERROR,
    CRASHED,
    UNKNOWN
};

std::ostream& operator<<(std::ostream& os, const Configuration& cfg);
std::ostream& operator<<(std::ostream& os, const ConfigChange& change);
std::string configValueToString(
    const std::variant<std::string, int64_t, double, bool>& value);