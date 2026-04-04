#!/ bin / bash
#остановить скрипт при любой ошибке

YANG_BASE =
    "./O-RAN.WG4.MP-YANGs-R003-v12.00"

    echo "=== Installing YANG models ==="

#Включаем feature для ietf - interfaces
    sysrepoctl -
    c ietf - interfaces - e if -
    mib

#Imported Models
        sysrepoctl -
    i "$YANG_BASE/Imported Models/ietf-dhcpv6-common.yang" sysrepoctl -
    i "$YANG_BASE/Imported Models/ietf-dhcpv6-types.yang" sysrepoctl -
    i "$YANG_BASE/Imported Models/iana-if-type.yang" sysrepoctl -
    i "$YANG_BASE/Imported Models/iana-hardware.yang" sysrepoctl -
    i "$YANG_BASE/Imported Models/ietf-hardware.yang"

#Common Models — System
    sysrepoctl -
    i "$YANG_BASE/Common Models/System/o-ran-wg4-features.yang"

#Common Models — Interfaces
    sysrepoctl -
    i "$YANG_BASE/Common Models/Interfaces/o-ran-ald-port.yang" -
    v3 sysrepoctl - i "$YANG_BASE/Common Models/Interfaces/o-ran-dhcp.yang" -
    v3 sysrepoctl -
    i "$YANG_BASE/Common Models/Interfaces/o-ran-externalio.yang" sysrepoctl -
    i "$YANG_BASE/Common Models/Interfaces/o-ran-interfaces.yang" sysrepoctl -
    c o - ran - interfaces - e UDPIP - BASED - CU - PLANE sysrepoctl -
    i "$YANG_BASE/Common "
      "Models/Interfaces/o-ran-ethernet-forwarding.yang" sysrepoctl -
    i "$YANG_BASE/Common Models/Interfaces/o-ran-mplane-int.yang" sysrepoctl -
    i "$YANG_BASE/Common Models/Interfaces/o-ran-transceiver.yang"

#Common Models — System(продолжение)
    sysrepoctl -
    i "$YANG_BASE/Common Models/System/o-ran-fan.yang" sysrepoctl -
    i "$YANG_BASE/Common Models/System/o-ran-fm.yang" sysrepoctl -
    i "$YANG_BASE/Common Models/System/o-ran-hardware.yang" sysrepoctl -
    i "$YANG_BASE/Common Models/System/o-ran-usermgmt.yang" sysrepoctl -
    i "$YANG_BASE/Common Models/System/o-ran-supervision.yang"

#Common Models — Operations
    sysrepoctl -
    i "$YANG_BASE/Common Models/Operations/o-ran-ald.yang" sysrepoctl -
    i "$YANG_BASE/Common "
      "Models/Operations/o-ran-file-management.yang" sysrepoctl -
    i "$YANG_BASE/Common Models/Operations/o-ran-operations.yang" sysrepoctl -
    i "$YANG_BASE/Common "
      "Models/Operations/o-ran-software-management.yang" sysrepoctl -
    i "$YANG_BASE/Common Models/Operations/o-ran-trace.yang" sysrepoctl -
    i "$YANG_BASE/Common Models/Operations/o-ran-troubleshooting.yang"

#Common Models — Sync
    sysrepoctl -
    i "$YANG_BASE/Common Models/Sync/o-ran-sync.yang"

#RU Specific Models — Interfaces
    sysrepoctl -
    i "$YANG_BASE/RU Specific Models/Interfaces/o-ran-processing-element.yang"

#RU Specific Models — Radio
    sysrepoctl -
    i "$YANG_BASE/RU Specific "
      "Models/Radio/o-ran-antenna-calibration.yang" sysrepoctl -
    i "$YANG_BASE/RU Specific "
      "Models/Radio/o-ran-compression-factors.yang" sysrepoctl -
    i "$YANG_BASE/RU Specific Models/Radio/o-ran-module-cap.yang"

#RU Specific Models — Operations
    sysrepoctl -
    i "$YANG_BASE/RU Specific Models/Operations/o-ran-uplane-conf.yang"

#RU Specific Models — Radio(продолжение)
    sysrepoctl -
    i "$YANG_BASE/RU Specific Models/Radio/o-ran-beamforming.yang" sysrepoctl -
    i "$YANG_BASE/RU Specific Models/Radio/o-ran-delay-management.yang"

#Cross - WG Common Models
    sysrepoctl -
    i "$YANG_BASE/Cross-WG Common Models/o-ran-common-yang-types.yang"

#RU Specific Models — Radio(продолжение)
    sysrepoctl -
    i "$YANG_BASE/RU Specific Models/Radio/o-ran-shared-cell.yang"

#RU Specific Models — Operations(продолжение)
    sysrepoctl -
    i "$YANG_BASE/RU Specific "
      "Models/Operations/o-ran-ecpri-delay.yang" sysrepoctl -
    i "$YANG_BASE/RU Specific Models/Operations/o-ran-lbm.yang" sysrepoctl -
    i "$YANG_BASE/RU Specific "
      "Models/Operations/o-ran-performance-management.yang" sysrepoctl -
    i "$YANG_BASE/RU Specific Models/Operations/o-ran-udp-echo.yang"

#Common Models — System(fm - ext)
    sysrepoctl -
    i "$YANG_BASE/Common Models/System/o-ran-fm-ext.yang"

#Включаем features
    sysrepoctl -
    c ietf - ip - e ipv4 - non - contiguous - netmasks sysrepoctl - c o - ran -
    wg4 - features - e MULTIPLE - TRANSPORT - SESSION - TYPE sysrepoctl - c o -
    ran - uplane - conf - e TX - REFERENCE - LEVEL sysrepoctl - c o - ran -
    ald - port - e OVERCURRENT - SUPPORTED sysrepoctl - c ietf - hardware -
    e hardware - state sysrepoctl - c o - ran - fm - ext - e VSWR - HW - THR -
    REPORT

    echo "=== All YANG models installed successfully ==="

#Показать список установленных модулей
    sysrepoctl -
    l