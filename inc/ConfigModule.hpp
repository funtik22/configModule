// ConfigModule.hpp
#pragma once

#include "ConfigTypes.hpp"
#include "ConfigValidator.hpp"
#include "ModuleBase.hpp"

#include <sysrepo-cpp/Subscription.hpp>

#include <memory>

class ConfigModule : public ModuleBase {
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
        std::string getName() const override;
        std::string getStatus() const override;

        std::string applyConfiguration(ConfigApplyRequest& request);
        Configuration getRunningConfig() const;
        std::string createBackup();
        bool restoreFromBackup(std::string backupPath);
        void handleConfigChange(Configuration& config);

    private:

        std::unique_ptr<sysrepo::Session>    runningSession;
        std::unique_ptr<sysrepo::Session>    startupSession;
        std::shared_ptr<sysrepo::Connection> srConnection;
    
        std::chrono::seconds                          transactionTimeout;
        std::string                                   backupDirectory;
        std::unique_ptr<ConfigValidator>              validator;
        ConfigModuleState                             currentState;
        std::chrono::system_clock::time_point         lastConfigChange;
        static constexpr size_t kMaxBackups = 10;


        bool validateConfiguration(ConfigApplyRequest& request);
        bool tryRollbackViaOldValue(const ConfigApplyRequest& request); 
        void tryRollbackViaBackup(const std::string& backupPath);
        void rotateBackups();

};