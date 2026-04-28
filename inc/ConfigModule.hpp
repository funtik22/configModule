// ConfigModule.hpp
#pragma once

#include "ConfigTypes.hpp"
#include "ConfigValidator.hpp"
#include "BaseModule.hpp"
#include "BaseMemory.hpp"
#include "HardwareEmulator.hpp"

#include <sysrepo-cpp/Subscription.hpp>

#include <memory>
#include <thread>
#include <atomic>

#define SUBSCRIBE_TO_TAG "config_changes"
#define MESSAGE_BUS_NAME "/config_module"

class ConfigModule : public BaseModule {
    public:
        
        explicit ConfigModule(
            std::string                          backupDirectory_,
            std::chrono::seconds                 transactionTimeout_,
            std::shared_ptr<sysrepo::Connection> srConnection_
        );

        ~ConfigModule(){};

        void initialize() override;
        void start() override;
        void stop() override;

        std::string applyConfiguration(ConfigApplyRequest& request);
        Configuration getRunningConfig() const;
        std::string createBackup();
        bool restoreFromBackup(std::string backupPath);
        void handleConfigChange(const std::string& message);

    private:

        std::unique_ptr<sysrepo::Session>    runningSession;
        std::unique_ptr<sysrepo::Session>    startupSession;
        std::shared_ptr<sysrepo::Connection> srConnection;

        std::unique_ptr<BaseMemory>          messageBus;
        std::thread                          listenerThread;
        std::atomic<bool>                    listenerRunning{false};

        std::chrono::seconds                          transactionTimeout;
        std::string                                   backupDirectory;
        std::unique_ptr<ConfigValidator>              validator;
        ConfigModuleState                             currentState;
        std::chrono::system_clock::time_point         lastConfigChange;
        static constexpr size_t kMaxBackups = 10;

        std::unique_ptr<HardwareEmulator>    hardwareEmulator;

        bool validateConfiguration(ConfigApplyRequest& request);
        void loadConfigFromSysrepo();
        bool tryRollbackViaOldValue(const ConfigApplyRequest& request); 
        void tryRollbackViaBackup(const std::string& backupPath);
        void rotateBackups();

        void listenerLoop();
        ConfigChange parseMessage(const std::string& message) const;
};