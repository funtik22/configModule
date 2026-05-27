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

std::variant<std::string, int64_t, double, bool> jsonToValue(const nlohmann::json& j, const std::string& type) {
    if (type == "string") return j.get<std::string>();
    if (type == "int64")  return j.get<int64_t>();
    if (type == "double") return j.get<double>();
    if (type == "bool")   return j.get<bool>();
    throw ConfigModuleException("jsonToValue: unknown valueType '" + type + "'");
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
    setName("ConfigModule");
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

    currentState = ConfigModuleState::VALIDATING_CONFIGURATION;
    const auto allParams = hardwareEmulator->getAllParameters();
    for (const auto& [id, param] : allParams) {
        if (!validator->validate(id, param.value)) {
            currentState = ConfigModuleState::CRASHED;
            throw ConfigModuleException(
                "Initial config validation failed for: " + id);
        }
    }

    currentState = ConfigModuleState::READY;
    publishLogInfo("Initialized");
}

void ConfigModule::start() {
    if (currentState != ConfigModuleState::READY) {
        throw ConfigModuleException("Cannot start — module not initialized");
    }

    listenerRunning = true;
    listenerThread  = std::thread(&ConfigModule::listenerLoop, this);

    publishLogInfo("Started");
}

void ConfigModule::stop() {
    listenerRunning = false;
    if (listenerThread.joinable()) {
        listenerThread.join();
    }

    messageBus->deleteConnection();
    publishLogInfo("Stopped");
}


Configuration ConfigModule::getRunningConfig() const {
    if (currentState != ConfigModuleState::READY) {
        throw ConfigModuleException("ConfigModule is not ready");
    }

    Configuration config;

    const auto snapshot = hardwareEmulator->getAllParameters();

    for (const auto& [id, param] : snapshot) {
        ConfigValue cv;
        cv.xpath      = id;
        cv.value      = param.value;
        config.values.emplace(id, std::move(cv));
    }

    publishLogInfo("Read " + std::to_string(config.values.size()) + " values from running config");
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
    publishLogInfo("Creating backup");

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
    currentState = ConfigModuleState::RESTORING_FROM_BACKUP;
    publishLogInfo("Restoring from backup: " + backupPath);

    std::filesystem::path fullPath = std::filesystem::path(backupDirectory) / backupPath;

    if (!std::filesystem::exists(fullPath)) {
        publishLogWarning("Backup file not found: " + fullPath.string());
        currentState = ConfigModuleState::READY;
        return false;
    }

    nlohmann::json j;
    try {
        std::ifstream ifs(fullPath);
        if (!ifs) {
            publishLogError("Cannot open backup file: " + fullPath.string());
            currentState = ConfigModuleState::READY;
            return false;
        }
        ifs >> j;
    } catch (const std::exception& e) {
        publishLogError("Failed to parse backup JSON: " + std::string(e.what()));
        currentState = ConfigModuleState::READY;
        return false;
    }


    std::map<std::string, ParamValue> restored;
    try {
        for (const auto& pj : j["parameters"]) {
            const auto id        = pj.at("id").get<std::string>();
            const auto valueType = pj.at("valueType").get<std::string>();
            const auto value     = jsonToValue(pj.at("value"), valueType);
            restored.emplace(id, std::move(value));
        }
    } catch (const std::exception& e) {
        publishLogError("Malformed backup entry: " + std::string(e.what()));
        currentState = ConfigModuleState::READY;
        return false;
    }


    hardwareEmulator->clear();
    for (const auto& [id, value] : restored) {
        hardwareEmulator->setParameter(id, value);
    }

    lastConfigChange = std::chrono::system_clock::now();
    publishLogInfo("Restored " + std::to_string(restored.size()) + " parameters from backup");

    currentState = ConfigModuleState::READY;
    return true;
}


std::string ConfigModule::applyConfiguration(ConfigApplyRequest &request) {
    if (currentState != ConfigModuleState::READY) {
        throw ConfigModuleException("ConfigModule is not ready");
    }

    publishLogInfo("Applying configuration requestId=" + request.requestId);

    currentState = ConfigModuleState::VALIDATING_CONFIGURATION;
    if (!validator->validate(request)) {
        currentState = ConfigModuleState::VALIDATION_ERROR;
        publishLogError("Validation failed for requestId=" + request.requestId);
        currentState = ConfigModuleState::READY;
        return {};
    }
    currentState = ConfigModuleState::READY;
    for (auto& ch : request.changes) {
        if (auto p = hardwareEmulator->getParameter(ch.xpath)) {
            ch.oldValue = p->value;
        } else {
            ch.oldValue = std::nullopt;
        }
    }

    std::string backupPath;
    try {
        backupPath = createBackup();
    } catch (const std::exception& ex) {
        publishLogWarning("Failed to create backup: " + std::string(ex.what()));
    }

    currentState = ConfigModuleState::APPLYING_CONFIGURATION;

    try {
        for (const auto& ch : request.changes) {
            if (!hardwareEmulator->setParameter(ch.xpath, ch.newValue)) {
                throw ConfigModuleException(
                    "setParameter failed for xpath: " + ch.xpath);
            }
        }

        lastConfigChange = std::chrono::system_clock::now();
        currentState = ConfigModuleState::READY;

        publishLogInfo("Applied successfully requestId=" + request.requestId);
        return request.requestId;

    } catch (const std::exception& ex) {
        currentState = ConfigModuleState::CONFIGURATION_ERROR;
        publishLogError("Failed to apply: " + std::string(ex.what()));

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
        for (const auto& ch : request.changes) {
            if (!ch.oldValue.has_value()) {
                hardwareEmulator->removeParameter(ch.xpath);
            } else {
                hardwareEmulator->setParameter(ch.xpath, *ch.oldValue);
            }
        }
        publishLogInfo("Rollback via oldValue successful");
        return true;
    } catch (const std::exception& ex) {
        publishLogError("oldValue rollback failed: " + std::string(ex.what()));
        return false;
    }
}

void ConfigModule::tryRollbackViaBackup(const std::string &backupPath) {
    if (backupPath.empty()) {
        currentState = ConfigModuleState::CRASHED;
        throw ConfigModuleException("Rollback failed and no backup available");
    }

    publishLogInfo("Trying backup restore: " + backupPath);

    if (!restoreFromBackup(backupPath)) {
        currentState = ConfigModuleState::CRASHED;
        throw ConfigModuleException("Restore from backup failed: " + backupPath);
    }

    publishLogInfo("Restore from backup successful");
}

void ConfigModule::handleConfigChange(const std::string& message) {
    publishLogInfo("handleConfigChange message: " + message);

    ConfigChange change;
    try {
        change = parseMessage(message);
    } catch (const ConfigModuleException& ex) {
        publishLogError("Failed to parse message: " + std::string(ex.what()));
        return;
    }

    ConfigApplyRequest request;
    request.requestId = "msg-" + std::format(
        "{:%Y-%m-%d_%H-%M-%S}", std::chrono::system_clock::now());
    request.reason            = "MessageBus notification";
    request.rollbackOnFailure = true;
    request.changes.push_back(std::move(change));

    const std::string result = applyConfiguration(request);

    if (!result.empty()) {
        publishLogInfo("handleConfigChange applied successfully requestId=" + result);
    } else {
        publishLogError("handleConfigChange failed");
    }
}


void ConfigModule::rotateBackups() {
    std::vector<std::filesystem::path> backups;

    for (const auto& entry :
         std::filesystem::directory_iterator(backupDirectory)) {
        if (entry.path().extension() == ".json" &&
            entry.path().filename().string().rfind("backup_", 0) == 0) {
            backups.push_back(entry.path());
        }
    }

    std::sort(backups.begin(), backups.end());

    while (backups.size() > kMaxBackups) {
        publishLogInfo("Removing old backup: " + backups.front().string());
        std::filesystem::remove(backups.front());
        backups.erase(backups.begin());
    }

    publishLogInfo("Backups: " + std::to_string(backups.size()) + "/" + std::to_string(kMaxBackups));
}

void ConfigModule::listenerLoop() {
    publishLogInfo("Listener thread started");

    while (listenerRunning.load()) {
        if (messageBus && messageBus->hasMessage()) {
            Message msg;
            const Result res = messageBus->getMessage(msg);

            if (res.result) {
                publishLogInfo("Received message from=" + msg.sender + " message=" + msg.message);
                try {
                    handleConfigChange(msg.message);
                } catch (const std::exception& ex) {
                    publishLogError("handleConfigChange exception: " + std::string(ex.what()));
                }
            } else {
                publishLogError("getMessage failed: " + res.message);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    publishLogInfo("Listener thread stopped");
}

ConfigChange ConfigModule::parseMessage(const std::string& message) const {
    const size_t spacePos = message.find(' ');
    if (spacePos == std::string::npos) {
        throw ConfigModuleException(
            "Invalid message format, expected 'xpath value': " + message);
    }

    ConfigChange change;
    change.xpath = message.substr(0, spacePos);
    const std::string valueStr = message.substr(spacePos + 1);

    if (valueStr == "true" || valueStr == "false") {
        change.newValue = (valueStr == "true");
        return change;
    }

    try {
        size_t pos = 0;
        const int64_t intVal = std::stoll(valueStr, &pos);
        if (pos == valueStr.size()) {
            change.newValue = intVal;
            return change;
        }
    } catch (...) {}

    try {
        size_t pos = 0;
        const double dblVal = std::stod(valueStr, &pos);
        if (pos == valueStr.size()) {
            change.newValue = dblVal;
            return change;
        }
    } catch (...) {}

    change.newValue = valueStr;
    return change;
}

void ConfigModule::publishLog(const std::string& tag, const std::string& text) const {
    if (!messageBus) return;
    messageBus->publishMessage("[" + getName() + "] " + text, tag);
}
