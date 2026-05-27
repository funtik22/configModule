#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#include "BaseMemory.hpp"

using namespace std;

int main() {
    Result res;

    BaseMemory writer("/config_writer");

    res = writer.createConnection();
    if (!res.result) {
        cout << "Failed to create connection: " << res.message << endl;
        return 1;
    }
    cout << "Connection created successfully" << endl;

    // Тестовые сообщения в формате "xpath value"
    vector<string> messages = {
        "/o-ran-uplane-conf:user-plane-configuration"
        "/tx-array-carriers[name='carrier1']/gain 10.5",

        "/o-ran-uplane-conf:user-plane-configuration"
        "/rx-array-carriers[name='carrier1']/gain-correction -2.3",

        "/o-ran-uplane-conf:user-plane-configuration"
        "/tx-array-carriers[name='carrier1']/center-of-channel-bandwidth 3700000000",

        "/o-ran-uplane-conf:user-plane-configuration"
        "/rx-array-carriers[name='carrier1']/channel-bandwidth 5000000",

        "/o-ran-uplane-conf:user-plane-configuration"
        "/tx-array-carriers[name='carrier1']/gain 999.9",  // невалидное — > 30 dB
    };

    cout << "Writer started. Sending config messages...\n" << endl;

    int count = 0;
    for (const auto& msg : messages) {
        cout << "Sending message #" << (count + 1) << ":\n  " << msg << "\n";

        res = writer.publishMessage(msg, {"config_changes"});
        if (res.result) {
            cout << "  → Sent successfully\n";
            count++;
        } else {
            cout << "  → Send failed: " << res.message << "\n";
        }

        // Проверяем не прочитанные сообщения
        res = writer.readOrNotMess();
        if (!res.result) {
            cout << "  → Read notification: " << res.message << "\n";
        }

        this_thread::sleep_for(chrono::seconds(1));
    }

    cout << "\nFinished sending " << count << " messages" << endl;

    writer.deleteConnection();
    return 0;
}