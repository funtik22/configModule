//ConfigValidator.hpp
#pragma once

#include "ConfigTypes.hpp"
#include <set>
#include <string>

class ConfigValidator {
public:
    ConfigValidator() = default;
    bool validate(const ConfigApplyRequest& request) const;

private:
    using Value = std::variant<std::string, int64_t, double, bool>;

    bool validateCenterOfChannelBandwidth(const Value& value) const;
    bool validateChannelBandwidth(const Value& value) const;
    bool validateNumerology(const Value& value) const;
    bool validateTxGain(const Value& value) const;
    bool validateRxGainCorrection(const Value& value) const;

    // center-of-channel-bandwidth — Band n78: 3300-3800 MHz, шаг 50 кГц
    static constexpr double kN78FreqMinMHz = 3300.0;
    static constexpr double kN78FreqMaxMHz = 3800.0;
    static constexpr double kFreqStepKHz   = 50.0;

    // gain TX — vendor-specific
    static constexpr double kTxGainMinDb = -30.0;
    static constexpr double kTxGainMaxDb = 30.0;

    // gain-correction RX — vendor-specific
    static constexpr double kRxGainMinDb = -20.0;
    static constexpr double kRxGainMaxDb = 20.0;

    static const std::set<int64_t> kAllowedBandwidthsMHz;
    static const std::set<int64_t> kAllowedNumerologies;
};