// main.cpp

#include <iostream>

#include "ConfigModule.hpp"

#define BACKUP_DIRECTORY "../backup"
#define BACKUP_FILE ""
#define TRANSACTION_TIMEOUT 5

#define RX_CARRIER "/o-ran-uplane-conf:user-plane-configuration/rx-array-carriers[name='carrier1']"
#define TX_CARRIER "/o-ran-uplane-conf:user-plane-configuration/tx-array-carriers[name='carrier1']"

int main() {

    
    auto connection = std::make_shared<sysrepo::Connection>();

    auto configModule = std::make_unique<ConfigModule>(
        BACKUP_DIRECTORY, std::chrono::seconds(TRANSACTION_TIMEOUT),
        connection);


    configModule->initialize();

    Configuration configuration = configModule->getRunningConfig();

    
    std::cout << "+++++++++++++++++PRINT CONGIG++++++++++++++++++++++++++++++++"
              << std::endl;
    std::cout << configuration << std::endl;

    configModule->createBackup();
/*

    std::cout << "Running... Press Ctrl+C to stop\n";

    configModule->start();
    
    std::cin.get();

    configModule->stop();
    
    Configuration configuration = configModule->getRunningConfig();

    std::cout << "+++++++++++++++++PRINT CONGIG++++++++++++++++++++++++++++++++"
              << std::endl;
    std::cout << configuration << std::endl;

    std::string backup_file_name = configModule->createBackup();

    bool isRestore = configModule->restoreFromBackup("backupFile.xml");

    configuration = configModule->getRunningConfig();
    std::cout << "+++++++++++++++++PRINT CONGIG++++++++++++++++++++++++++++++++"
              << std::endl;
    std::cout << configuration << std::endl;

    ConfigApplyRequest request;
    request.requestId = "test-request-001";
    request.reason = "Test applyConfiguration";
    request.rollbackOnFailure = true;

    ConfigChange change1;
    change1.xpath    = RX_CARRIER "/center-of-channel-bandwidth";
    change1.newValue = int64_t(3700000000);
    change1.oldValue = std::nullopt;

    request.changes.push_back(change1);

    const std::string result = configModule->applyConfiguration(request);

    if (!result.empty()) {
        std::cout << "SUCCESS requestId=" << result << "\n";
    } else {
        std::cout << "FAILED\n";
    }

    configuration = configModule->getRunningConfig();
    std::cout << "+++++++++++++++++PRINT CONGIG++++++++++++++++++++++++++++++++"
              << std::endl;
    std::cout << configuration << std::endl;

    std::cout << "\n=== Test rollback ===\n";
    ConfigApplyRequest badRequest;
    badRequest.requestId = "test-request-002";
    badRequest.rollbackOnFailure = true;

    ConfigChange badChange;
    badChange.xpath    = TX_CARRIER "/gain";
    badChange.newValue = double(50.0);
    badChange.oldValue = double(10.5);

    badRequest.changes.push_back(badChange);

    const std::string badResult = configModule->applyConfiguration(badRequest);
    if (badResult.empty()) {
        std::cout << "Rollback triggered as expected\n";
    }

    configuration = configModule->getRunningConfig();
    std::cout << "+++++++++++++++++PRINT CONGIG++++++++++++++++++++++++++++++++"
              << std::endl;
    std::cout << configuration << std::endl;


    Configuration newConfig;

    ConfigValue cv1;
    cv1.yangModule   = "o-ran-uplane-conf";
    cv1.value        = double(-2.3);
    cv1.createdTime  = std::chrono::system_clock::now();
    cv1.modifiedTime = std::chrono::system_clock::now();
    newConfig.values[RX_CARRIER "/gain-correction"] = cv1;

    ConfigValue cv2;
    cv2.yangModule   = "o-ran-uplane-conf";
    cv2.value        = double(10.5);
    cv2.createdTime  = std::chrono::system_clock::now();
    cv2.modifiedTime = std::chrono::system_clock::now();
    newConfig.values[TX_CARRIER "/gain"] = cv2;

    std::cout << "\n=== Calling handleConfigChange ===\n";
    configModule->handleConfigChange(newConfig);

    std::cout << "\n=== Config after handleConfigChange ===\n";
    Configuration updatedConfig = configModule->getRunningConfig();
    std::cout << updatedConfig;

    std::cout << "GOOD" << std::endl;
}
*/

}