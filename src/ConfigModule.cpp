// ConfigModule.cpp
#include "ConfigModule.hpp"
#include "ConfigValidator.hpp"

#include <format>
#include <fstream>
#include <iostream>

#include "ConfigModuleException.hpp"

namespace {

std::variant<std::string, int64_t, double, bool> parseLeafValue(
    const libyang::DataNodeTerm &term) {
    const auto baseType = term.valueType().base();

    switch (baseType) {
        case libyang::LeafBaseType::Int8:
        case libyang::LeafBaseType::Int16:
        case libyang::LeafBaseType::Int32:
        case libyang::LeafBaseType::Int64:
        case libyang::LeafBaseType::Uint8:
        case libyang::LeafBaseType::Uint16:
        case libyang::LeafBaseType::Uint32:
        case libyang::LeafBaseType::Uint64:
            return static_cast<int64_t>(
                std::stoll(std::string(term.valueStr())));

        case libyang::LeafBaseType::Dec64:
            return std::stod(std::string(term.valueStr()));

        case libyang::LeafBaseType::Bool:
            return std::string(term.valueStr()) == "true";

        default:
            return std::string(term.valueStr());
    }
}

}  // namespace

ConfigModule::ConfigModule(std::string backupDirectory_,
                           std::chrono::seconds transactionTimeout_,
                           std::shared_ptr<sysrepo::Connection> srConnection_)
    : backupDirectory(std::move(backupDirectory_)),
      transactionTimeout(transactionTimeout_),
      srConnection(std::move(srConnection_)),
      validator(std::make_unique<ConfigValidator>()),
      currentState(ConfigModuleState::UNINITIALIZED),
      lastConfigChange(std::chrono::system_clock::now()) {
    if (backupDirectory.empty()) {
        throw ConfigModuleException("backupDirectory cannot be empty");
    }
    if (!std::filesystem::exists(backupDirectory)) {
        throw ConfigModuleException("backupDirectory does not exist: " +
                                    backupDirectory);
    }
    if (transactionTimeout.count() <= 0) {
        throw ConfigModuleException("transactionTimeout must be positive");
    }
    if (transactionTimeout.count() >= 500) {
        throw ConfigModuleException("transactionTimeout must be less than 500");
    }
    if (!srConnection) {
        throw ConfigModuleException("srConnection cannot be null");
    }
}

void ConfigModule::initialize() {
    currentState = ConfigModuleState::INITIALIZING;

    try {
        runningSession = std::make_unique<sysrepo::Session>(
            srConnection->sessionStart(sysrepo::Datastore::Running));

        startupSession = std::make_unique<sysrepo::Session>(
            srConnection->sessionStart(sysrepo::Datastore::Startup));
    } catch (const std::exception &ex) {
        currentState = ConfigModuleState::CRASHED;
        throw ConfigModuleException(
            std::string("Failed to start sysrepo session: ") + ex.what());
    }

    currentState = ConfigModuleState::READY;
}

void ConfigModule::start(){

}

void ConfigModule::stop() {

}

std::string ConfigModule::getName() const {

}

std::string ConfigModule::getStatus() const {

}


Configuration ConfigModule::getRunningConfig() const {
    if (currentState != ConfigModuleState::READY) {
        throw ConfigModuleException("ConfigModule is not ready");
    }

    std::cout << "[ConfigModule] Reading running config from sysrepo...\n";

    Configuration config;

    try {
        auto data = runningSession->getData("/*");

        if (!data) {
            std::cout << "[ConfigModule] No data in datastore\n";
            return config;
        }

        for (const auto& sibling : data->siblings()) {
            for (const auto& node : sibling.childrenDfs()) {
                if (node.schema().nodeType() != libyang::NodeType::Leaf) {
                    continue;
                }

                ConfigValue configValue;
                configValue.yangModule   = std::string(node.schema().module().name());
                configValue.createdTime  = std::chrono::system_clock::now();
                configValue.modifiedTime = std::chrono::system_clock::now();
                configValue.value        = parseLeafValue(node.asTerm());

                config.values[node.path()] = configValue;
            }
        }

    } catch (const std::exception &ex) {
        throw ConfigModuleException(
            std::string("Failed to read running config: ") + ex.what());
    }

    std::cout << "[ConfigModule] Read " << config.values.size() << " values\n";
    return config;
}

std::string ConfigModule::createBackup() {
    currentState = ConfigModuleState::CREATING_BACKUP;
    std::cout << "[ConfigModule] Creating backup...\n";

    auto now = std::chrono::system_clock::now();
    std::string timestamp = std::format("{:%Y-%m-%d_%H-%M-%S}", now);

    const std::string filename =
        backupDirectory + "/backup_" + timestamp + ".xml";

    try {
         auto data = runningSession->getData("/*//.");

        if (!data) {
            throw ConfigModuleException("No data to backup");
        }

        auto xmlStr =
            data->printStr(libyang::DataFormat::XML,
                           libyang::PrintFlags::Siblings |
                               libyang::PrintFlags::WithDefaultsExplicit);

        if (!xmlStr) {
            throw ConfigModuleException("Failed to serialize config to XML");
        }

        std::ofstream file(filename);
        if (!file.is_open()) {
            throw ConfigModuleException("Cannot open backup file: " + filename);
        }
        file << *xmlStr;
        file.close();

        std::cout << "[ConfigModule] Backup created: " << filename << "\n";

    } catch (const ConfigModuleException &) {
        currentState = ConfigModuleState::READY;
        throw;
    } catch (const std::exception &ex) {
        currentState = ConfigModuleState::READY;
        throw ConfigModuleException(std::string("Failed to create backup: ") +
                                    ex.what());
    }

    rotateBackups(); // TO:DO Подумать как часто удалять

    currentState = ConfigModuleState::READY;
    return filename;
}

bool ConfigModule::restoreFromBackup(std::string backupPath) {
    currentState = ConfigModuleState::RESTORING_FROM_BACKUP;
    std::cout << "[ConfigModule] Restoring from backup: " << backupPath << "\n";

    const std::string backupFilename = backupDirectory + "/" + backupPath;

    if (!std::filesystem::exists(backupFilename)) {
        std::cout << "[ConfigModule] Backup file not found: " << backupFilename
                  << "\n";
        currentState = ConfigModuleState::READY;
        return false;
    }

    try {
        auto ctx = runningSession->getContext();

        auto data = ctx.parseData(std::filesystem::path(backupFilename),
                                  libyang::DataFormat::XML,
                                  libyang::ParseOptions::ParseOnly);

        if (!data) {
            throw ConfigModuleException("Failed to parse backup file: " +
                                        backupFilename);
        }

        runningSession->replaceConfig(data, std::nullopt);

        lastConfigChange = std::chrono::system_clock::now();
        currentState = ConfigModuleState::READY;

        std::cout << "[ConfigModule] Restore completed successfully\n";
        return true;

    } catch (const ConfigModuleException &) {
        currentState = ConfigModuleState::READY;
        throw;
    } catch (const std::exception &ex) {
        currentState = ConfigModuleState::READY;
        throw ConfigModuleException(
            std::string("Failed to restore from backup: ") + ex.what());
    }
}

std::string ConfigModule::applyConfiguration(ConfigApplyRequest &request) {
    if (currentState != ConfigModuleState::READY) {
        throw ConfigModuleException("ConfigModule is not ready");
    }

    std::cout << "[ConfigModule] Applying configuration requestId="
              << request.requestId << "\n";

    ConfigValidator configValidator = ConfigValidator();
    currentState = ConfigModuleState::VALIDATING_CONFIGURATION;
    if (!configValidator.validate(request)) {
        currentState = ConfigModuleState::VALIDATION_ERROR;
        std::cout << "[ConfigModule] Validation failed for requestId="
                << request.requestId << "\n";
        currentState = ConfigModuleState::READY;
        return {};
    }

    std::string backupPath;
    try {
        backupPath = createBackup();
    } catch (const std::exception &ex) {
        std::cout << "[ConfigModule] Warning: failed to create backup: "
                  << ex.what() << "\n";
    }

    currentState = ConfigModuleState::APPLYING_CONFIGURATION;
    try {
        for (const auto &change : request.changes) {
            const std::string value = configValueToString(change.newValue);

            std::cout << "[ConfigModule] Setting xpath=" << change.xpath
                      << " value=" << value << "\n";

            runningSession->setItem(change.xpath, value);
        }

        runningSession->applyChanges();

        lastConfigChange = std::chrono::system_clock::now();
        currentState = ConfigModuleState::READY;

        std::cout << "[ConfigModule] Applied successfully requestId="
                  << request.requestId << "\n";
        return request.requestId;

    } catch (const std::exception &ex) {
        currentState = ConfigModuleState::CONFIGURATION_ERROR;
        std::cout << "[ConfigModule] Failed to apply: " << ex.what() << "\n";

        if (request.rollbackOnFailure) {
            if (!tryRollbackViaOldValue(request)) {
                tryRollbackViaBackup(backupPath);
            }
        }

        currentState = ConfigModuleState::READY;
        return {};
    }
}

bool ConfigModule::tryRollbackViaOldValue(const ConfigApplyRequest &request) {
    try {
        for (const auto &change : request.changes) {
            if (!change.oldValue.has_value()) {
                std::cout << "[ConfigModule] Deleting xpath=" << change.xpath
                          << "\n";
                runningSession->deleteItem(change.xpath);
            } else {
                std::cout << "[ConfigModule] Restoring xpath=" << change.xpath
                          << "\n";
                runningSession->setItem(
                    change.xpath, configValueToString(*change.oldValue));
            }
        }
        runningSession->applyChanges();
        std::cout << "[ConfigModule] Rollback via oldValue successful\n";
        return true;

    } catch (const std::exception &ex) {
        std::cout << "[ConfigModule] oldValue rollback failed: " << ex.what()
                  << "\n";
        return false;
    }
}

void ConfigModule::tryRollbackViaBackup(const std::string &backupPath) {
    if (backupPath.empty()) {
        currentState = ConfigModuleState::CRASHED;
        throw ConfigModuleException("Rollback failed and no backup available");
    }

    std::cout << "[ConfigModule] Trying backup restore: " << backupPath << "\n";
    try {
        restoreFromBackup(backupPath);
        std::cout << "[ConfigModule] Restore from backup successful\n";
    } catch (const std::exception &ex) {
        currentState = ConfigModuleState::CRASHED;
        throw ConfigModuleException(
            std::string("Both rollback and restore failed: ") + ex.what());
    }
}

void ConfigModule::handleConfigChange(Configuration &newConfig) {
    std::cout << "[ConfigModule] handleConfigChange received "
              << newConfig.values.size() << " values\n";

    Configuration currentConfig = getRunningConfig();

    ConfigApplyRequest request;
    std::string timestamp =
        std::format("{:%Y-%m-%d_%H-%M-%S}", std::chrono::system_clock::now());
    request.requestId = "auto-" + timestamp;
    request.reason = "MessageBus notification";
    request.rollbackOnFailure = true;

    for (const auto &[xpath, cv] : newConfig.values) {
        ConfigChange change;
        change.xpath = xpath;
        change.newValue = cv.value;

        auto it = currentConfig.values.find(xpath);
        if (it != currentConfig.values.end()) {
            change.oldValue = it->second.value;
        } else {
            change.oldValue = std::nullopt;
        }

        request.changes.push_back(change);
    }

    const std::string result = applyConfiguration(request);

    if (!result.empty()) {
        std::cout << "[ConfigModule] handleConfigChange applied successfully "
                  << "requestId=" << result << "\n";
    } else {
        std::cout << "[ConfigModule] handleConfigChange failed\n";
    }
}


void ConfigModule::rotateBackups() {
    std::vector<std::filesystem::path> backups;

    for (const auto& entry :
         std::filesystem::directory_iterator(backupDirectory)) {
        if (entry.path().extension() == ".xml" &&
            entry.path().filename().string().starts_with("backup_")) {
            backups.push_back(entry.path());
        }
    }

    std::sort(backups.begin(), backups.end());

    while (backups.size() > kMaxBackups) {
        std::cout << "[ConfigModule] Removing old backup: "
                  << backups.front() << "\n";
        std::filesystem::remove(backups.front());
        backups.erase(backups.begin());
    }

    std::cout << "[ConfigModule] Backups: "
              << backups.size() << "/" << kMaxBackups << "\n";
}