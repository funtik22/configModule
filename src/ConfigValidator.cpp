//ConfigValidator.cpp
#include "ConfigValidator.hpp"

#include <cmath>
#include <iostream>

const std::set<int64_t> ConfigValidator::kAllowedBandwidthsMHz = {
    5, 10, 15, 20, 25, 30, 40, 50, 60, 70, 80, 90, 100
};

const std::set<int64_t> ConfigValidator::kAllowedNumerologies = {
    0, 1, 2, 3, 4
};

bool ConfigValidator::validate(const std::string& xpath,
                                const Value& value) const {
  
    if (xpath.find("center-of-channel-bandwidth") != std::string::npos) {
        return validateCenterOfChannelBandwidth(value);
    }
    if (xpath.find("channel-bandwidth") != std::string::npos) {
        return validateChannelBandwidth(value);
    }
    if (xpath.find("numerology") != std::string::npos) {
        return validateNumerology(value);
    }
    if (xpath.find("tx-array-carriers") != std::string::npos
        && xpath.find("/gain") != std::string::npos
        && xpath.find("gain-correction") == std::string::npos) {
        return validateTxGain(value);
    }
    if (xpath.find("rx-array-carriers") != std::string::npos
        && xpath.find("gain-correction") != std::string::npos) {
        return validateRxGainCorrection(value);
    }

    return true;
}

bool ConfigValidator::validate(const ConfigApplyRequest& request) const {
    std::cout << "[ConfigValidator] Validating requestId="
              << request.requestId << "\n";

    for (const auto& change : request.changes) {
        if (!validate(change.xpath, change.newValue)) {
            std::cout << "[ConfigValidator] FAILED xpath=" << change.xpath
                      << "\n";
            return false;
        }
    }

    std::cout << "[ConfigValidator] All checks passed\n";
    return true;
}


bool ConfigValidator::validateCenterOfChannelBandwidth(
    const Value& value) const
{
    double freqKHz = 0.0;

    if (std::holds_alternative<int64_t>(value)) {
        freqKHz = static_cast<double>(std::get<int64_t>(value));
    } else if (std::holds_alternative<double>(value)) {
        freqKHz = std::get<double>(value);
    } else {
        std::cout << "[ConfigValidator] center-of-channel-bandwidth: "
                     "unexpected type\n";
        return false;
    }

    double freqMHz = freqKHz / 1000.0;

    if (freqMHz < kN78FreqMinMHz || freqMHz > kN78FreqMaxMHz) {
        std::cout << "[ConfigValidator] center-of-channel-bandwidth="
                  << freqMHz << " MHz out of n78 range ["
                  << kN78FreqMinMHz << ", " << kN78FreqMaxMHz << "] MHz\n";
        return false;
    }

    if (std::fmod(freqKHz, kFreqStepKHz) > 1e-6) {
        std::cout << "[ConfigValidator] center-of-channel-bandwidth="
                  << freqKHz << " kHz not aligned to "
                  << kFreqStepKHz << " kHz grid\n";
        return false;
    }

    return true;
}

bool ConfigValidator::validateChannelBandwidth(const Value& value) const {
    if (!std::holds_alternative<int64_t>(value)) {
        std::cout << "[ConfigValidator] channel-bandwidth: "
                     "expected int64_t\n";
        return false;
    }

    int64_t bwMHz = std::get<int64_t>(value);

    if (kAllowedBandwidthsMHz.find(bwMHz) == kAllowedBandwidthsMHz.end()) {
        std::cout << "[ConfigValidator] channel-bandwidth=" << bwMHz
                  << " MHz not in allowed set "
                     "{5,10,15,20,25,30,40,50,60,70,80,90,100}\n";
        return false;
    }

    return true;
}

bool ConfigValidator::validateNumerology(const Value& value) const {
    if (!std::holds_alternative<int64_t>(value)) {
        std::cout << "[ConfigValidator] numerology: expected int64_t\n";
        return false;
    }

    int64_t num = std::get<int64_t>(value);

    if (kAllowedNumerologies.find(num) == kAllowedNumerologies.end()) {
        std::cout << "[ConfigValidator] numerology=" << num
                  << " not in allowed set {0,1,2,3,4}\n";
        return false;
    }

    return true;
}

bool ConfigValidator::validateTxGain(const Value& value) const {
    double gain = 0.0;

    if (std::holds_alternative<double>(value)) {
        gain = std::get<double>(value);
    } else if (std::holds_alternative<int64_t>(value)) {
        gain = static_cast<double>(std::get<int64_t>(value));
    } else {
        std::cout << "[ConfigValidator] tx gain: unexpected type\n";
        return false;
    }

    if (gain < kTxGainMinDb || gain > kTxGainMaxDb) {
        std::cout << "[ConfigValidator] tx gain=" << gain
                  << " dB out of range ["
                  << kTxGainMinDb << ", " << kTxGainMaxDb << "] dB\n";
        return false;
    }

    return true;
}

bool ConfigValidator::validateRxGainCorrection(const Value& value) const {
    double gain = 0.0;

    if (std::holds_alternative<double>(value)) {
        gain = std::get<double>(value);
    } else if (std::holds_alternative<int64_t>(value)) {
        gain = static_cast<double>(std::get<int64_t>(value));
    } else {
        std::cout << "[ConfigValidator] rx gain-correction: unexpected type\n";
        return false;
    }

    if (gain < kRxGainMinDb || gain > kRxGainMaxDb) {
        std::cout << "[ConfigValidator] rx gain-correction=" << gain
                  << " dB out of range ["
                  << kRxGainMinDb << ", " << kRxGainMaxDb << "] dB\n";
        return false;
    }

    return true;
}
