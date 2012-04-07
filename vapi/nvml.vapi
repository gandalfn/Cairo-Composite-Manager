[CCode (cheader_filename = "nvml.h")]
namespace Nvml
{
    [CCode (cname = "nvmlInit")]
    public static ReturnType init ();
    [CCode (cname = "nvmlShutdown")]
    public static ReturnType shutdown ();

    public struct System
    {
        [CCode (cname = "nvmlSystemGetDriverVersion")]
        public static ReturnType get_driver_version (char[] version);
    }

    [SimpleType]
    [CCode (cname = "nvmlUnit_t")]
    public struct Unit
    {
        [CCode (cname = "nvmlUnitGetCount")]
        public static ReturnType get_count (out uint unitCount);
        [CCode (cname = "nvmlUnitGetHandleByIndex")]
        public static ReturnType get_handle_by_index (uint index, out Unit unit);

        [CCode (cname = "nvmlUnitGetUnitInfo")]
        public ReturnType get_unit_info (out UnitInfo info);
        [CCode (cname = "nvmlUnitGetLedState")]
        public ReturnType get_led_state (out LedState states);
        [CCode (cname = "nvmlUnitSetLedState")]
        public ReturnType set_led_state (LedColor color);
        [CCode (cname = "nvmlUnitGetPsuInfo")]
        public ReturnType get_psu_info (out PSUInfo psu);
        [CCode (cname = "nvmlUnitGetTemperature")]
        public ReturnType get_temperature (uint type, out uint temp);
        [CCode (cname = "nvmlUnitGetFanSpeedInfo")]
        public ReturnType get_fan_speed_info (out FanSpeeds fanSpeeds);
        [CCode (cname = "nvmlUnitGetDevices")]
        public ReturnType get_devices (out uint deviceCount, out Device devices);
    }

    [SimpleType]
    [CCode (cname = "nvmlDevice_t")]
    public struct Device
    {
        [CCode (cname = "nvmlDeviceGetCount")]
        public static ReturnType get_count (out uint deviceCount);
        [CCode (cname = "nvmlDeviceGetHandleByIndex")]
        public static ReturnType get_handle_by_index (uint index, out Device device);
        [CCode (cname = "nvmlDeviceGetHandleBySerial")]
        public static ReturnType get_handle_by_serial (string serial, out Device device);
        [CCode (cname = "nvmlDeviceGetHandleByPciBusId")]
        public static ReturnType get_handle_by_pci_bus_id (string pciBusId, out Device device);

        [CCode (cname = "nvmlDeviceGetName")]
        public ReturnType get_name (char[] name);
        [CCode (cname = "nvmlDeviceGetSerial")]
        public ReturnType get_serial (char[] serial);
        [CCode (cname = "nvmlDeviceGetUUID")]
        public ReturnType get_uuid (char[] uuid);
        [CCode (cname = "nvmlDeviceGetInforomVersion")]
        public ReturnType get_inforom_version (InforomObject object, char[] version);
        [CCode (cname = "nvmlDeviceGetDisplayMode")]
        public ReturnType get_display_mode (out EnableState state);
        [CCode (cname = "nvmlDeviceGetPersistenceMode")]
        public ReturnType get_persistence_mode (out EnableState state);
        [CCode (cname = "nvmlDeviceSetPersistenceMode")]
        public ReturnType set_persistence_mode (EnableState state);
        [CCode (cname = "nvmlDeviceGetPciInfo")]
        public ReturnType get_pci_info (out PciInfo pci);
        [CCode (cname = "nvmlDeviceGetClockInfo")]
        public ReturnType get_clock_info (ClockType type, out uint clock);
        [CCode (cname = "nvmlDeviceGetFanSpeed")]
        public ReturnType get_fan_speed (out uint speed);
        [CCode (cname = "nvmlDeviceGetTemperature")]
        public ReturnType get_temperature (TemperatureSensors sensorType, out uint temp);
        [CCode (cname = "nvmlDeviceGetPowerState")]
        public ReturnType get_power_state (out PowerStates powerState);
        [CCode (cname = "nvmlDeviceGetPowerManagementMode")]
        public ReturnType get_power_management_mode (out EnableState mode);
        [CCode (cname = "nvmlDeviceGetPowerManagementLimit")]
        public ReturnType get_power_management_limit (out uint limit);
        [CCode (cname = "nvmlDeviceGetPowerUsage")]
        public ReturnType get_power_usage (out uint power);
        [CCode (cname = "nvmlDeviceGetMemoryInfo")]
        public ReturnType get_memory_info (out Memory memory);
        [CCode (cname = "nvmlDeviceGetComputeMode")]
        public ReturnType get_compute_mode (out ComputeMode mode);
        [CCode (cname = "nvmlDeviceSetComputeMode")]
        public ReturnType set_compute_mode (ComputeMode mode);
        [CCode (cname = "nvmlDeviceGetEccMode")]
        public ReturnType get_ecc_mode (out EnableState current, out EnableState pending);
        [CCode (cname = "nvmlDeviceSetEccMode")]
        public ReturnType set_ecc_mode (EnableState ecc);
        [CCode (cname = "nvmlDeviceGetTotalEccErrors")]
        public ReturnType get_total_ecc_errors (EccBitType bitType, EccCounterType counterType, out uint64 eccCounts);
        [CCode (cname = "nvmlDeviceGetDetailedEccErrors")]
        public ReturnType get_detailed_ecc_errors (EccBitType bitType, EccCounterType counterType, out EccErrorCounts eccCounts);
        [CCode (cname = "nvmlDeviceClearEccErrorCounts")]
        public ReturnType clear_ecc_error_counts (EccCounterType counterType);
        [CCode (cname = "nvmlDeviceGetUtilizationRates")]
        public ReturnType get_utilization_rates (out Utilization utilization);
    }

    [CCode (cname = "nvmlUnitInfo_t")]
    public struct UnitInfo
    {
        public char name            [81];
        public char id              [81];
        public char serial          [81];
        public char firmwareVersion [81];
    }

    [CCode (cname = "nvmlLedState_t")]
    public struct LedState
    {
        public LedColor color;
        public char     cause[255];
    }

    [CCode (cname = "nvmlPSUInfo_t")]
    public struct PSUInfo
    {
        public char state[255];
        public uint current;
        public uint voltage;
        public uint power;
    }

    [CCode (cname = "nvmlUnitFanSpeeds_t")]
    public struct FanSpeeds
    {
        public uint    count;
        public FanInfo fans[16];
    }

    [CCode (cname = "nvmlUnitFanInfo_t")]
    public struct FanInfo
    {
        public uint     speed;
        public FanState state;
    }

    [CCode (cname = "nvmlPciInfo_t")]
    public struct PciInfo
    {
        public uint domain;
        public uint bus;
        public uint device;
        public uint function;
        public uint linkGen;
        public uint linkSpeed;
        public uint pciDeviceId;
        public char busId[9];
    }

    [CCode (cname = "nvmlMemory_t")]
    public struct Memory
    {
        public uint total;
        public uint free;
        public uint used;
    }

    [CCode (cname = "nvmlEccErrorCounts_t")]
    public struct EccErrorCounts
    {
        uint64 l1Cache;
        uint64 l2Cache;
        uint64 deviceMemory;
        uint64 registerFile;
    }

    [CCode (cname = "nvmlUtilization_t")]
    public struct Utilization
    {
        uint gpu;
        uint memory;
    }

    [CCode (cname = "nvmlFanState_t", cprefix = "NVML_FAN_")]
    public enum FanState
    {
        NORMAL,
        FAILED
    }

    [CCode (cname = "nvmlLedColor_t", cprefix = "NVML_LED_COLOR_")]
    public enum LedColor
    {
        GREEN,
        AMBER
    }

    [CCode (cname = "nvmlReturn_t", cprefix = "NVML_")]
    public enum ReturnType
    {
        SUCCESS,
        ERROR_UNINITIALIZED,
        ERROR_INVALID_ARGUMENT,
        ERROR_NOT_SUPPORTED,
        ERROR_NO_PERMISSION,
        ERROR_ALREADY_INITIALIZED,
        ERROR_NOT_FOUND,
        ERROR_INSUFFICIENT_SIZE,
        ERROR_INSUFFICIENT_POWER,
        ERROR_UNKNOWN;

        public string
        to_string ()
        {
            switch (this)
            {
                case SUCCESS:
                    return "The operation was successful";
                case ERROR_UNINITIALIZED:
                    return "NVML was not first initialized with nvmlInit()";
                case ERROR_INVALID_ARGUMENT:
                    return "A supplied argument is invalid";
                case ERROR_NOT_SUPPORTED:
                    return "The requested operation is not available on target device";
                case ERROR_NO_PERMISSION:
                    return "The currrent user does not have permission for operation";
                case ERROR_ALREADY_INITIALIZED:
                    return "NVML has already been initialized";
                case ERROR_NOT_FOUND:
                    return "A query to find an object was unccessful";
                case ERROR_INSUFFICIENT_SIZE:
                    return "An input argument is not large enough";
                case ERROR_INSUFFICIENT_POWER:
                    return "A device's external power cables are not properly attached";
                case ERROR_UNKNOWN:
                    return "An internal driver error occurred";
            }

            return "";
        }
    }

    [CCode (cname = "nvmlInforomObject_t", cprefix = "NVML_INFOROM_")]
    public enum InforomObject
    {
        OEM,
        ECC,
        POWER
    }

    [CCode (cname = "nvmlEnableState_t", cprefix = "NVML_FEATURE_")]
    public enum EnableState
    {
        DISABLED,
        ENABLED
    }

    [CCode (cname = "nvmlClockType_t", cprefix = "NVML_CLOCK_")]
    public enum ClockType
    {
        GRAPHICS,
        SM,
        MEM
    }

    [CCode (cname = "nvmlTemperatureSensors_t", cprefix = "NVML_TEMPERATURE_")]
    public enum TemperatureSensors
    {
        GPU
    }

    [CCode (cname = "nvmlPstates_t", cprefix = "NVML_P")]
    public enum PowerStates
    {
        STATE_0,
        STATE_1,
        STATE_2,
        STATE_3,
        STATE_4,
        STATE_5,
        STATE_6,
        STATE_7,
        STATE_8,
        STATE_9,
        STATE_10,
        STATE_11,
        STATE_12,
        STATE_13,
        STATE_14,
        STATE_15,
        STATE_UNKNOWN
    }

    [CCode (cname = "nvmlComputeMode_t", cprefix = "NVML_COMPUTEMODE_")]
    public enum ComputeMode
    {
        DEFAULT,
        EXCLUSIVE_THREAD,
        PROHIBITED,
        EXCLUSIVE_PROCESS
    }

    [CCode (cname = "nvmlEccBitType_t", cprefix = "NVML_")]
    public enum EccBitType
    {
        SINGLE_BIT_ECC,
        DOUBLE_BIT_ECC
    }

    [CCode (cname = "nvmlEccCounterType_t", cprefix = "NVML_")]
    public enum EccCounterType
    {
        VOLATILE_ECC,
        AGGREGATE_ECC
    }
}

