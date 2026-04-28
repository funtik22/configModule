// ConfigModule.cpp
#include "ConfigModule.hpp"
#include "ConfigValidator.hpp"

#include <nlohmann/json.hpp>

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


std::string getValueType(const ParamValue& v) {
    return std::visit([](auto&& x) -> std::string {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, std::string>) return "string";
        if constexpr (std::is_same_v<T, int64_t>)     return "int64";
        if constexpr (std::is_same_v<T, double>)      return "double";
        if constexpr (std::is_same_v<T, bool>)        return "bool";
        return "unknown";
    }, v);
}

nlohmann::json valueToJson(const ParamValue& v) {
    return std::visit([](auto&& x) -> nlohmann::json {
        return x;
    }, v);
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

    hardwareEmulator = std::make_unique<HardwareEmulator>();

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

    messageBus = std::make_unique<BaseMemory>(MESSAGE_BUS_NAME);
    Result res = messageBus->createConnection();
    if (!res.result) {
        throw ConfigModuleException(
            "Failed to create MessageBus connection: " + res.message);
    }
    messageBus->subscribeTag(SUBSCRIBE_TO_TAG);
      if (!res.result) {
        throw ConfigModuleException(
            "Failed to subscribe to tag: " + res.message);
    }

    loadConfigFromSysrepo();

    currentState = ConfigModuleState::READY;
}

void ConfigModule::start(){
    if (currentState != ConfigModuleState::READY) {
        throw ConfigModuleException("Cannot start — module not initialized");
    }



    currentState   = ConfigModuleState::READY;
}

void ConfigModule::stop() {
    // listenerRunning = false;
    // if (listenerThread.joinable()) {
    //     listenerThread.join();
    // }

    // isRunning = false;
    messageBus->deleteConnection();
}


Configuration ConfigModule::getRunningConfig() const {
    if (currentState != ConfigModuleState::READY) {
        throw ConfigModuleException("ConfigModule is not ready");
    }

    std::cout << "[ConfigModule] Reading running config from sysrepo...\n";

    Configuration config;

    const auto snapshot = hardwareEmulator->getAllParameters();

    for (const auto& [id, param] : snapshot) {
        ConfigValue cv;
        cv.xpath      = id;
        cv.value      = param.value;
        config.values.emplace(id, std::move(cv));
    }

    std::cout << "[ConfigModule] Read " << config.values.size() << " values\n";
    return config;
}

std::string ConfigModule::createBackup() {

    nlohmann::json j;
    j["version"]    = 1;
    j["createdAt"]  = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch()).count();
    j["parameters"] = nlohmann::json::array();
    
    const Configuration snapshot = getRunningConfig();

    try {
        for (const auto& [xpath, cv] : snapshot.values) {
            nlohmann::json pj;
            pj["id"]        = xpath;
            pj["value"]     = valueToJson(cv.value);
            pj["valueType"] = getValueType(cv.value);
            j["parameters"].push_back(std::move(pj));
        }
    } catch (const std::exception& e) {
    
    
    }

    currentState = ConfigModuleState::CREATING_BACKUP;
    std::cout << "[ConfigModule] Creating backup...\n";

    auto now = std::chrono::system_clock::now();
    std::string timestamp = std::format("{:%Y-%m-%d_%H-%M-%S}", now);

    const std::string filename =
        backupDirectory + "/backup_" + timestamp + ".json";

    std::ofstream ofs(filename);
    ofs << j.dump(2);

    rotateBackups();

    currentState = ConfigModuleState::READY;
    return filename;
}

void ConfigModule::loadConfigFromSysrepo() {
    auto data = runningSession->getData("/*");

    if (!data.has_value()) {
        return;
    }

    for (const auto& sibling : data->siblings()) {
        for (const auto& node : sibling.childrenDfs()) {
            const auto nodeType = node.schema().nodeType();
            if (nodeType != libyang::NodeType::Leaf &&
                nodeType != libyang::NodeType::Leaflist) {
                continue;
            }

            const auto xpath = node.path();
            const auto value = parseLeafValue(node.asTerm());

            if(!validator->validate(xpath, value)){
                return;
            }

            hardwareEmulator->setParameter(xpath, value);
        }
    }
}

bool ConfigModule::restoreFromBackup(std::string backupPath) {
    // currentState = ConfigModuleState::RESTORING_FROM_BACKUP;
    // std::cout << "[ConfigModule] Restoring from backup: " << backupPath << "\n";

    // const std::string backupFilename = backupDirectory + "/" + backupPath;

    // if (!std::filesystem::exists(backupFilename)) {
    //     std::cout << "[ConfigModule] Backup file not found: " << backupFilename
    //               << "\n";
    //     currentState = ConfigModuleState::READY;
    //     return false;
    // }

    // try {
    //     auto ctx = runningSession->getContext();

    //     auto data = ctx.parseData(std::filesystem::path(backupFilename),
    //                               libyang::DataFormat::XML,
    //                               libyang::ParseOptions::ParseOnly);

    //     if (!data) {
    //         throw ConfigModuleException("Failed to parse backup file: " +
    //                                     backupFilename);
    //     }

    //     runningSession->replaceConfig(data, std::nullopt);

    //     lastConfigChange = std::chrono::system_clock::now();
    //     currentState = ConfigModuleState::READY;

    //     std::cout << "[ConfigModule] Restore completed successfully\n";
    //     return true;

    // } catch (const ConfigModuleException &) {
    //     currentState = ConfigModuleState::READY;
    //     throw;
    // } catch (const std::exception &ex) {
    //     currentState = ConfigModuleState::READY;
    //     throw ConfigModuleException(
    //         std::string("Failed to restore from backup: ") + ex.what());
    // }
}

std::string ConfigModule::applyConfiguration(ConfigApplyRequest &request) {

    
    if (currentState != ConfigModuleState::READY) {
        throw ConfigModuleException("ConfigModule is not ready");
    }

    std::cout << "[ConfigModule] Applying configuration requestId="
              << request.requestId << "\n";

    // ConfigValidator configValidator = ConfigValidator();
    // currentState = ConfigModuleState::VALIDATING_CONFIGURATION;
    // if (!configValidator.validate(request)) {
    //     currentState = ConfigModuleState::VALIDATION_ERROR;
    //     std::cout << "[ConfigModule] Validation failed for requestId="
    //             << request.requestId << "\n";
    //     currentState = ConfigModuleState::READY;
    //     return {};
    // }

    for (auto& ch : request.changes) {
        if (auto p = hardwareEmulator->getParameter(ch.xpath)) {
            ch.oldValue = p->value;
        } else {
            ch.oldValue = std::nullopt;
        }
    }

    currentState = ConfigModuleState::APPLYING_CONFIGURATION;

    for (const auto& ch : request.changes) {
        const bool ok = hardwareEmulator->setParameter(ch.xpath, ch.newValue);
    }





    // std::string backupPath;
    // try {
    //     backupPath = createBackup();
    // } catch (const std::exception &ex) {
    //     std::cout << "[ConfigModule] Warning: failed to create backup: "
    //               << ex.what() << "\n";
    // }

    // currentState = ConfigModuleState::APPLYING_CONFIGURATION;
    // try {
    //     for (const auto &change : request.changes) {
    //         const std::string value = configValueToString(change.newValue);

    //         std::cout << "[ConfigModule] Setting xpath=" << change.xpath
    //                   << " value=" << value << "\n";

    //         runningSession->setItem(change.xpath, value);
    //     }

    //     runningSession->applyChanges();

    //     lastConfigChange = std::chrono::system_clock::now();
    //     currentState = ConfigModuleState::READY;

    //     std::cout << "[ConfigModule] Applied successfully requestId="
    //               << request.requestId << "\n";
    //     return request.requestId;

    // } catch (const std::exception &ex) {
    //     currentState = ConfigModuleState::CONFIGURATION_ERROR;
    //     std::cout << "[ConfigModule] Failed to apply: " << ex.what() << "\n";

    //     if (request.rollbackOnFailure) {
    //         if (!tryRollbackViaOldValue(request)) {
    //             tryRollbackViaBackup(backupPath);
    //         }
    //     }

    //     currentState = ConfigModuleState::READY;
    //     return {};
    // }
}

bool ConfigModule::tryRollbackViaOldValue(const ConfigApplyRequest &request) {
    // try {
    //     for (const auto &change : request.changes) {
    //         if (!change.oldValue.has_value()) {
    //             std::cout << "[ConfigModule] Deleting xpath=" << change.xpath
    //                       << "\n";
    //             runningSession->deleteItem(change.xpath);
    //         } else {
    //             std::cout << "[ConfigModule] Restoring xpath=" << change.xpath
    //                       << "\n";
    //             runningSession->setItem(
    //                 change.xpath, configValueToString(*change.oldValue));
    //         }
    //     }
    //     runningSession->applyChanges();
    //     std::cout << "[ConfigModule] Rollback via oldValue successful\n";
    //     return true;

    // } catch (const std::exception &ex) {
    //     std::cout << "[ConfigModule] oldValue rollback failed: " << ex.what()
    //               << "\n";
    //     return false;
    // }
}

void ConfigModule::tryRollbackViaBackup(const std::string &backupPath) {
    // if (backupPath.empty()) {
    //     currentState = ConfigModuleState::CRASHED;
    //     throw ConfigModuleException("Rollback failed and no backup available");
    // }

    // std::cout << "[ConfigModule] Trying backup restore: " << backupPath << "\n";
    // try {
    //     restoreFromBackup(backupPath);
    //     std::cout << "[ConfigModule] Restore from backup successful\n";
    // } catch (const std::exception &ex) {
    //     currentState = ConfigModuleState::CRASHED;
    //     throw ConfigModuleException(
    //         std::string("Both rollback and restore failed: ") + ex.what());
    // }
}

void ConfigModule::handleConfigChange(const std::string& message) {
    // std::cout << "[ConfigModule] handleConfigChange message: "
    //           << message << "\n";

    // ConfigChange change;
    // try {
    //     change = parseMessage(message);
    // } catch (const ConfigModuleException& ex) {
    //     std::cout << "[ConfigModule] Failed to parse message: "
    //               << ex.what() << "\n";
    //     return;
    // }

    // Configuration currentConfig = getRunningConfig();
    // auto it = currentConfig.values.find(change.xpath);
    // if (it != currentConfig.values.end()) {
    //     change.oldValue = it->second.value;
    // } else {
    //     change.oldValue = std::nullopt;
    // }

    // // Формируем запрос
    // ConfigApplyRequest request;
    // request.requestId = "msg-" + std::format(
    //     "{:%Y-%m-%d_%H-%M-%S}", std::chrono::system_clock::now());
    // request.reason            = "MessageBus notification";
    // request.rollbackOnFailure = true;
    // request.changes.push_back(change);

    // const std::string result = applyConfiguration(request);

    // if (!result.empty()) {
    //     std::cout << "[ConfigModule] handleConfigChange applied successfully "
    //               << "requestId=" << result << "\n";
    // } else {
    //     std::cout << "[ConfigModule] handleConfigChange failed\n";
    // }
}


void ConfigModule::rotateBackups() {
    // std::vector<std::filesystem::path> backups;

    // for (const auto& entry :
    //      std::filesystem::directory_iterator(backupDirectory)) {
    //     if (entry.path().extension() == ".xml" &&
    //         entry.path().filename().string().starts_with("backup_")) {
    //         backups.push_back(entry.path());
    //     }
    // }

    // std::sort(backups.begin(), backups.end());

    // while (backups.size() > kMaxBackups) {
    //     std::cout << "[ConfigModule] Removing old backup: "
    //               << backups.front() << "\n";
    //     std::filesystem::remove(backups.front());
    //     backups.erase(backups.begin());
    // }

    // std::cout << "[ConfigModule] Backups: "
    //           << backups.size() << "/" << kMaxBackups << "\n";
}

void ConfigModule::listenerLoop() {
    // std::cout << "[ConfigModule] Listener thread started\n";

    
    // while (listenerRunning.load()) {
    //     if (messageBus && messageBus->hasMessage()) {
    //         Message msg;
    //         Result res = messageBus->getMessage(msg);

    //         if (res.result) {
    //             std::cout << "[ConfigModule] Received message from="
    //                       << msg.sender << " message=" << msg.message << "\n";
    //             handleConfigChange(msg.message);
    //         }
    //     }

    //     std::this_thread::sleep_for(std::chrono::milliseconds(10));
    // }

    // std::cout << "[ConfigModule] Listener thread stopped\n";
}

ConfigChange ConfigModule::parseMessage(const std::string& message) const {
    // ConfigChange change;

    // const size_t spacePos = message.find(' ');
    // if (spacePos == std::string::npos) {
    //     throw ConfigModuleException(
    //         "Invalid message format, expected 'xpath value': " + message);
    // }

    // change.xpath         = message.substr(0, spacePos);
    // const std::string valueStr = message.substr(spacePos + 1);

    // if (valueStr == "true" || valueStr == "false") {
    //     change.newValue = (valueStr == "true");
    //     return change;
    // }

    // try {
    //     size_t pos = 0;
    //     int64_t intVal = std::stoll(valueStr, &pos);
    //     if (pos == valueStr.size()) {
    //         change.newValue = intVal;
    //         return change;
    //     }
    // } catch (...) {}

    // try {
    //     size_t pos = 0;
    //     double dblVal = std::stod(valueStr, &pos);
    //     if (pos == valueStr.size()) {
    //         change.newValue = dblVal;
    //         return change;
    //     }
    // } catch (...) {}

    // change.newValue = valueStr;
    // return change;
}