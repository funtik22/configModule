#!/bin/bash

set -euo pipefail

# =============================================================================
# Скрипт импорта YANG моделей через sysrepoctl
# Источник: O-RAN.WG4.MP-YANGs-R003-v12.00
# =============================================================================

YANG_ROOT="${1:-./O-RAN.WG4.MP-YANGs-R003-v12.00}"

if [[ ! -d "$YANG_ROOT" ]]; then
    echo "[ERROR] Директория не найдена: $YANG_ROOT"
    echo "Использование: $0 [путь_к_O-RAN.WG4.MP-YANGs-R003-v12.00]"
    exit 1
fi

IMP="$YANG_ROOT/Imported Models"
COM="$YANG_ROOT/Common Models"
RU="$YANG_ROOT/RU Specific Models"
CWG="$YANG_ROOT/Cross-WG Common Models"

echo "=================================================="
echo " Импорт YANG моделей"
echo " Источник: $YANG_ROOT"
echo "=================================================="

install() {
    local path="$1"
    shift
    local extra_args=("$@")

    if [[ ! -f "$path" ]]; then
        echo "[WARN]  Файл не найден, пропускаю: $path"
        return 0
    fi

    echo "[INSTALL] $(basename "$path")"
    sysrepoctl -i "$path" "${extra_args[@]}"
}

enable_feature() {
    local module="$1"
    local feature="$2"
    echo "[FEATURE] $module -> $feature"
    sysrepoctl -c "$module" -e "$feature"
}


echo ""
echo "--- [1/7] Базовые типы (ietf/iana) ---"

install "$RU/Operations/o-ran-uplane-conf.yang"

install "$IMP/ietf-yang-types.yang"
install "$IMP/ietf-inet-types.yang"
install "$IMP/iana-if-type.yang"
install "$IMP/iana-hardware.yang"
install "$IMP/ietf-interfaces.yang"
install "$IMP/ietf-ip.yang"
install "$IMP/ietf-hardware.yang"
install "$IMP/ietf-datastores.yang"
install "$IMP/ietf-yang-library.yang"
install "$IMP/ietf-yang-schema-mount.yang"
install "$IMP/ietf-netconf-acm.yang"
install "$IMP/ietf-netconf-monitoring.yang"
install "$IMP/ietf-netconf-notifications.yang"
install "$IMP/ietf-network-instance.yang"
install "$IMP/ietf-restconf.yang"
install "$IMP/ietf-subscribed-notifications.yang"
install "$IMP/ietf-system.yang"


echo ""
echo "--- [2/7] Crypto / Certs ---"

install "$IMP/iana-crypt-hash.yang"
install "$IMP/ietf-x509-cert-to-name.yang"
install "$IMP/ietf-truststore.yang"
install "$IMP/ietf-crypto-types.yang"


echo ""
echo "--- [3/7] IEEE 802 ---"

install "$IMP/ieee802-types.yang"
install "$IMP/ieee802-dot1q-types.yang"
install "$IMP/ieee802-dot1q-cfm-types.yang"
install "$IMP/ieee802-dot1q-cfm.yang"
install "$IMP/ieee802-dot1x-types.yang"
install "$IMP/ieee802-dot1x.yang"

# DHCP
install "$IMP/ietf-dhcpv6-common.yang"
install "$IMP/ietf-dhcpv6-types.yang"


echo ""
echo "--- [4/7] Common Models ---"

# System
install "$COM/System/o-ran-wg4-features.yang"
install "$COM/System/o-ran-fan.yang"
install "$COM/System/o-ran-fm.yang"
install "$COM/System/o-ran-hardware.yang"
install "$COM/System/o-ran-usermgmt.yang"
install "$COM/System/o-ran-supervision.yang"
install "$COM/System/o-ran-fm-ext.yang"
install "$COM/System/o-ran-certificates.yang"
install "$COM/System/o-ran-ves-subscribed-notifications.yang"

# Interfaces
install "$COM/Interfaces/o-ran-ald-port.yang"          -v3
install "$COM/Interfaces/o-ran-dhcp.yang"              -v3
install "$COM/Interfaces/o-ran-externalio.yang"
install "$COM/Interfaces/o-ran-interfaces.yang"
install "$COM/Interfaces/o-ran-ethernet-forwarding.yang"
install "$COM/Interfaces/o-ran-mplane-int.yang"
install "$COM/Interfaces/o-ran-transceiver.yang"

# Operations
install "$COM/Operations/o-ran-ald.yang"
install "$COM/Operations/o-ran-file-management.yang"
install "$COM/Operations/o-ran-operations.yang"
install "$COM/Operations/o-ran-software-management.yang"
install "$COM/Operations/o-ran-trace.yang"
install "$COM/Operations/o-ran-troubleshooting.yang"

# Sync
install "$COM/Sync/o-ran-sync.yang"


echo ""
echo "--- [5/7] Cross-WG Common Models ---"

install "$CWG/o-ran-common-identity-refs.yang"
install "$CWG/o-ran-common-yang-types.yang"

# ------------------------------------------------------------------
# 6. RU Specific Models
# ------------------------------------------------------------------
echo ""
echo "--- [6/7] RU Specific Models ---"

# Interfaces
install "$RU/Interfaces/o-ran-processing-element.yang"

# Radio
install "$RU/Radio/o-ran-compression-factors.yang"
install "$RU/Radio/o-ran-module-cap.yang"
install "$RU/Radio/o-ran-antenna-calibration.yang"
install "$RU/Radio/o-ran-beamforming.yang"
install "$RU/Radio/o-ran-delay-management.yang"
install "$RU/Radio/o-ran-shared-cell.yang"
install "$RU/Radio/o-ran-laa.yang"
install "$RU/Radio/o-ran-laa-operations.yang"

# Operations
install "$RU/Operations/o-ran-uplane-conf.yang"
install "$RU/Operations/o-ran-ecpri-delay.yang"
install "$RU/Operations/o-ran-lbm.yang"
install "$RU/Operations/o-ran-ieee802-dot1q-cfm.yang"
install "$RU/Operations/o-ran-performance-management.yang"
install "$RU/Operations/o-ran-udp-echo.yang"


echo ""
echo "--- [7/7] Активация фич ---"

enable_feature ietf-interfaces        if-mib
enable_feature o-ran-interfaces       UDPIP-BASED-CU-PLANE
enable_feature ietf-ip                ipv4-non-contiguous-netmasks
enable_feature o-ran-wg4-features     MULTIPLE-TRANSPORT-SESSION-TYPE
enable_feature o-ran-uplane-conf      TX-REFERENCE-LEVEL
enable_feature o-ran-ald-port         OVERCURRENT-SUPPORTED
enable_feature ietf-hardware          hardware-state
enable_feature o-ran-fm-ext           VSWR-HW-THR-REPORT

# ------------------------------------------------------------------
# Итоговый список
# ------------------------------------------------------------------
echo ""
echo "=================================================="
echo " Список установленных модулей:"
echo "=================================================="
sysrepoctl -l

echo ""
echo "[OK] Импорт завершён успешно."