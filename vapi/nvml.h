/*
 * Copyright 1993-2010 NVIDIA Corporation.  All rights reserved.
 *
 * NOTICE TO USER:   
 *
 * This source code is subject to NVIDIA ownership rights under U.S. and 
 * international Copyright laws.  Users and possessors of this source code 
 * are hereby granted a nonexclusive, royalty-free license to use this code 
 * in individual and commercial software.
 *
 * NVIDIA MAKES NO REPRESENTATION ABOUT THE SUITABILITY OF THIS SOURCE 
 * CODE FOR ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR 
 * IMPLIED WARRANTY OF ANY KIND.  NVIDIA DISCLAIMS ALL WARRANTIES WITH 
 * REGARD TO THIS SOURCE CODE, INCLUDING ALL IMPLIED WARRANTIES OF 
 * MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE.
 * IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL, 
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS 
 * OF USE, DATA OR PROFITS,  WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE 
 * OR OTHER TORTIOUS ACTION,  ARISING OUT OF OR IN CONNECTION WITH THE USE 
 * OR PERFORMANCE OF THIS SOURCE CODE.  
 *
 * U.S. Government End Users.   This source code is a "commercial item" as 
 * that term is defined at  48 C.F.R. 2.101 (OCT 1995), consisting  of 
 * "commercial computer  software"  and "commercial computer software 
 * documentation" as such terms are  used in 48 C.F.R. 12.212 (SEPT 1995) 
 * and is provided to the U.S. Government only as a commercial end item.  
 * Consistent with 48 C.F.R.12.212 and 48 C.F.R. 227.7202-1 through 
 * 227.7202-4 (JUNE 1995), all U.S. Government End Users acquire the 
 * source code with only those rights set forth herein. 
 *
 * Any use of this source code in individual and commercial software must 
 * include, in the user documentation and internal comments to the code,
 * the above Disclaimer and U.S. Government End Users Notice.
 */

/*! 
\mainpage NVML API Reference

The NVIDIA Management Library (NVML) is a C-based programatic interface for monitoring and 
managing various states within NVIDIA Tesla GPUs.<br>It is intended to be a platform for building
3rd party applications, and is also the underlying library for the NVIDIA-supported nvidia-smi
tool.<br>NVML is thread-safe.
<p>
<hr size="1">
<p>
<h2 align="center">API Documentation</h2>
<p>
Supported OS platforms:
- Windows: Windows Server 2008 R2 64bit, Windows 7 64bit
- Linux: 32bit & 64bit

Supported products:
- Full Support
    - Tesla Line:   S1070, S2050, C1060, C2050/70, M2050/70/90, X2070/90
    - Quadro Line:  4000, 5000, 6000, 7000, M2070-Q
    - Geforce Line: None
- Limited Support
    - Tesla Line:   None
    - Quadro Line:  All other current and previous generation Quadro-branded parts
    - Geforce Line: All current and previous generation Geforce-branded parts

Support Methods:
- \ref group1 

Query Methods:
- \ref group3 
- \ref group2 
- \ref group4 

Control Methods:
- \ref group10 
- \ref group5 

<div width=100% align="center">
<p>
<hr size="1">
<p>
<h2>Device Feature Matrix</h2>
<p>
<div align="center">This chart shows which features are available for each GPU product.<br> 
An updated version of the board's inforom may be required for some features.</div>
<p>
\htmlinclude device_features.html
<p>
<br>
<hr size="1">
<p>
<h2>Unit Feature Matrix</h2>
<p>
<div align="center">This chart shows which unit-level features are available for each S-class product.<br>
All GPUs within each S-class product also provide the information listed in the Device chart above.</div>
<p>
\htmlinclude unit_features.html
<p>
</div>
*/

#ifndef __nvml_nvml_h__
#define __nvml_nvml_h__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * On Windows, set up methods for DLL export
 */
#if defined _WINDOWS
    #if defined LIB_EXPORT
        #define DECLDIR __declspec(dllexport)
    #else
        #define DECLDIR __declspec(dllimport)
    #endif
#else
    #define DECLDIR
#endif

/***************************************************************************************************/
/** @defgroup group6 Device Structs
 *  @{
 */
/***************************************************************************************************/

/** 
 * Handle to a GPU device. 
 * @cond
 */
typedef struct nvmlDevice_st* nvmlDevice_t;
/** @endcond */

/**
 * PCI information about a GPU device.
 */
typedef struct nvmlPciInfo_st {
    char busId[16];                  //!< The 3-tuple domain:bus:device PCI identifier (+ null terminator)
    unsigned int domain;             //!< The PCI domain on which the device's bus resides, 0 to 255
    unsigned int bus;                //!< The bus on which the device resides, 0 to 255
    unsigned int device;             //!< The device's id on the bus, 0 to 31
    unsigned int pciDeviceId;        //!< The combined 16-bit device id and 16-bit vendor id
} nvmlPciInfo_t;

/**
 * Detailed ECC error counts for a device.
 */
typedef struct nvmlEccErrorCounts_st {
    unsigned long long l1Cache;      //!< L1 cache errors
    unsigned long long l2Cache;      //!< L2 cache errors
    unsigned long long deviceMemory; //!< Device memory errors
    unsigned long long registerFile; //!< Register file errors
} nvmlEccErrorCounts_t;

/** 
 * Utilization info for a device.
 *
 * All readings are percents.
 */
typedef struct nvmlUtilization_st {
    unsigned int gpu;                //!< Percent of time over the past second during which one or more kernels was executing on the GPU
    unsigned int memory;             //!< Percent of time over the past second during which global (device) memory was being read or written
} nvmlUtilization_t;

/** 
 * Memory allocation info for a device.
 *
 * All readings are in in bytes.
*/
typedef struct nvmlMemory_st {
    unsigned long long total;        //!< Total installed FB memory
    unsigned long long free;         //!< Unallocated FB memory
    unsigned long long used;         //!< Allocated FB memory. Note that the driver/gpu always sets aside a small amount of memory  for bookkeeping
} nvmlMemory_t;

/** @} */

/***************************************************************************************************/
/** @defgroup group7 Device Enums
 *  @{
 */
/***************************************************************************************************/

/** 
 * Generic enable/disable enum. 
 */
typedef enum nvmlEnableState_enum {
    NVML_FEATURE_DISABLED    = 0,     //!< Feature disabled 
    NVML_FEATURE_ENABLED     = 1      //!< Feature enabled
} nvmlEnableState_t;


/**
 * Generic flags used for changing default behavior of some functions.
 * See description particular of function for details.
 *
 * Flags can be combined with bitwise or operator |
 */
#define nvmlFlagDefault     0x00      //!< Default behavior
#define nvmlFlagForce       0x01      //!< Force some behavior

/** 
 * Temperature sensors. 
 */
typedef enum nvmlTemperatureSensors_enum {
    NVML_TEMPERATURE_GPU      = 0     //!< Temperature sensor for the GPU die
} nvmlTemperatureSensors_t;

/** 
 * Compute mode. 
 *
 * NVML_COMPUTEMODE_EXCLUSIVE_PROCESS was added in CUDA 4.0.
 * Earlier CUDA versions supported a single exclusive mode, 
 * which is equivalent to NVML_COMPUTEMODE_EXCLUSIVE_THREAD in CUDA 4.0 and beyond.
 */
typedef enum nvmlComputeMode_enum {
    NVML_COMPUTEMODE_DEFAULT           = 0,  //!< Default compute mode -- multiple contexts per device
    NVML_COMPUTEMODE_EXCLUSIVE_THREAD  = 1,  //!< Compute-exclusive-thread mode -- only one context per device, usable from one thread at a time
    NVML_COMPUTEMODE_PROHIBITED        = 2,  //!< Compute-prohibited mode -- no contexts per device
    NVML_COMPUTEMODE_EXCLUSIVE_PROCESS = 3   //!< Compute-exclusive-process mode -- only one context per device, usable from multiple threads at a time
} nvmlComputeMode_t;

/** 
 * ECC bit types. 
 */
typedef enum nvmlEccBitType_enum {
    NVML_SINGLE_BIT_ECC     = 0,      //!< Single bit ECC errors 
    NVML_DOUBLE_BIT_ECC     = 1       //!< Double bit ECC errors
} nvmlEccBitType_t;

/** 
 * ECC counter types. 
 */
typedef enum nvmlEccCounterType_enum {
    NVML_VOLATILE_ECC      = 0,      //!< Volatile counts are reset after each boot
    NVML_AGGREGATE_ECC     = 1       //!< Aggregate counts persist across reboots (i.e. for the lifetime of the device)
} nvmlEccCounterType_t;

/** 
 * Clock types. 
 * 
 * All speeds are in Mhz.
 */
typedef enum nvmlClockType_enum {
    NVML_CLOCK_GRAPHICS  = 0,        //!< Graphics clock domain
    NVML_CLOCK_SM        = 1,        //!< SM clock domain
    NVML_CLOCK_MEM       = 2         //!< Memory clock domain
} nvmlClockType_t;

/** 
 * Driver models. 
 *
 * windows only.
 */
typedef enum nvmlDriverModel_enum {
    NVML_DRIVER_WDDM      = 0,       //!< WDDM driver model -- GPU treated as a display device
    NVML_DRIVER_WDM       = 1        //!< WDM (TCC) model -- GPU treated as a generic device
} nvmlDriverModel_t;

/**
 * Allowed PStates.
 */
typedef enum nvmlPStates_enum {
    NVML_PSTATE_0               = 0,       //!< Power state 0 -- Maximum Performance
    NVML_PSTATE_1               = 1,       //!< Power state 1 
    NVML_PSTATE_2               = 2,       //!< Power state 2
    NVML_PSTATE_3               = 3,       //!< Power state 3
    NVML_PSTATE_4               = 4,       //!< Power state 4
    NVML_PSTATE_5               = 5,       //!< Power state 5
    NVML_PSTATE_6               = 6,       //!< Power state 6
    NVML_PSTATE_7               = 7,       //!< Power state 7
    NVML_PSTATE_8               = 8,       //!< Power state 8
    NVML_PSTATE_9               = 9,       //!< Power state 9
    NVML_PSTATE_10              = 10,      //!< Power state 10
    NVML_PSTATE_11              = 11,      //!< Power state 11
    NVML_PSTATE_12              = 12,      //!< Power state 12
    NVML_PSTATE_13              = 13,      //!< Power state 13
    NVML_PSTATE_14              = 14,      //!< Power state 14
    NVML_PSTATE_15              = 15,      //!< Power state 15 -- Minimum Power
    NVML_PSTATE_UNKNOWN         = 32,      //!< Unknown power state
} nvmlPstates_t;

/** 
 * Available inforom objects.
 */
typedef enum nvmlInforomObject_enum {
    NVML_INFOROM_OEM            = 0,       //!< The OEM object
    NVML_INFOROM_ECC            = 1,       //!< The ECC object
    NVML_INFOROM_POWER          = 2        //!< The power management object
} nvmlInforomObject_t;

/** 
 * Return values for nvml api calls. 
 */
typedef enum nvmlReturn_enum {
    NVML_SUCCESS = 0,                   //!< The operation was successful
    NVML_ERROR_UNINITIALIZED = 1,       //!< NVML was not first initialized with nvmlInit()
    NVML_ERROR_INVALID_ARGUMENT = 2,    //!< A supplied argument is invalid
    NVML_ERROR_NOT_SUPPORTED = 3,       //!< The requested operation is not available on target device
    NVML_ERROR_NO_PERMISSION = 4,       //!< The currrent user does not have permission for operation
    NVML_ERROR_ALREADY_INITIALIZED = 5, //!< NVML has already been initialized
    NVML_ERROR_NOT_FOUND = 6,           //!< A query to find an object was unccessful
    NVML_ERROR_INSUFFICIENT_SIZE = 7,   //!< An input argument is not large enough
    NVML_ERROR_INSUFFICIENT_POWER = 8,  //!< A device's external power cables are not properly attached
    NVML_ERROR_UNKNOWN = 999            //!< An internal driver error occurred
} nvmlReturn_t;

/** @} */

/***************************************************************************************************/
/** @defgroup group8 Unit Structs
 *  @{
 */
/***************************************************************************************************/

/** 
 * Handle to a GPU unit. 
 * @cond
 */
typedef struct nvmlUnit_st* nvmlUnit_t;
/** @endcond */

/** 
 * Fan state enum. 
 */
typedef enum nvmlFanState_enum {
    NVML_FAN_NORMAL       = 0,     //!< Fan is working properly
    NVML_FAN_FAILED       = 1      //!< Fan has failed
} nvmlFanState_t;

/** 
 * Led color enum. 
 */
typedef enum nvmlLedColor_enum {
    NVML_LED_COLOR_GREEN       = 0,     //!< GREEN, indicates good health
    NVML_LED_COLOR_AMBER       = 1      //!< AMBER, indicates problem
} nvmlLedColor_t;


/** 
 * LED states for an S-class unit.
 */
typedef struct nvmlLedState_st {
    char cause[256];               //!< If amber, a text description of the cause
    unsigned int color;            //!< GREEN or AMBER
} nvmlLedState_t;

/** 
 * Static S-class unit info.
 */
typedef struct nvmlUnitInfo_st {
    char name[96];                      //!< Product name
    char id[96];                        //!< Product identifier
    char serial[96];                    //!< Product serial number
    char firmwareVersion[96];           //!< Firmware version
} nvmlUnitInfo_t;

/** 
 * Power usage info for an S-class unit.
 * 
 * The power supply state is a human readable string that equals "Normal" or contains
 * a combination of "Abnormal" plus 1 or more of the following:
 *    
 *    High voltage
 *    Fan failure
 *    Heatsink temperature
 *    Current limit
 *    Voltage below UV alarm threshold
 *    Low-voltage
 *    SI2C remote off command
 *    MOD_DISABLE input
 *    Short pin transition 
*/
typedef struct nvmlPSUInfo_st {
    char state[256];                 //!< The power supply state
    unsigned int current;            //!< PSU current (A)
    unsigned int voltage;            //!< PSU voltage (V)
    unsigned int power;              //!< PSU power draw (W)
} nvmlPSUInfo_t;

/** 
 * Fan speed reading for a single fan in an S-class unit.
 */
typedef struct nvmlUnitFanInfo_st {
    unsigned int speed;              //!< Fan speed (RPM)
    nvmlFanState_t state;            //!< Flag that indicates whether fan is working properly
} nvmlUnitFanInfo_t;

/** 
 * Fan speed readings for an entire S-class unit.
 */
typedef struct nvmlUnitFanSpeeds_st {
    nvmlUnitFanInfo_t fans[24];      //!< Fan speed data for each fan
    unsigned int count;              //!< Number of fans in unit
} nvmlUnitFanSpeeds_t;

/** @} */

/***************************************************************************************************/
/** @defgroup group1 Initilization and Cleanup
 * This page describes the methods that handle NVML initialization and cleanup.
 * It is the user's resonsibility to call \ref nvmlInit() before calling any other methods, and 
 * nvmlShutdown() once NVML is no longer being used.
 *  @{
 */
/***************************************************************************************************/

/**
 * Initialize NVML by discovering and attaching to all GPU devices in the system.
 * 
 * For all products.
 *
 * This method should be called once before invoking any other methods in the library.
 * 
 * @return 
 *         - \ref NVML_SUCCESS                   if NVML has been properly initialized
 *         - \ref NVML_ERROR_ALREADY_INITIALIZED if NVML has already been initialized
 *         - \ref NVML_ERROR_NO_PERMISSION       if the user doesn't have permission to talk to any device
 *         - \ref NVML_ERROR_INSUFFICIENT_POWER  if any devices have improperly attached external power cables
 *         - \ref NVML_ERROR_UNKNOWN             on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlInit();

/**
 * Shut down NVML by releasing all GPU resources previously allocated with \ref nvmlInit().
 * 
 * For all products.
 *
 * This method should be called after NVML work is done.
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if NVML has been properly shut down
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlShutdown();

/** @} */

/***************************************************************************************************/
/** @defgroup group3 System Queries
 * This page describes that queries that NVML can perform against the local system.
 *
 * These queries are not device-specific.
 *  @{
 */
/***************************************************************************************************/

/**
 * Retrieve the version of the system's graphics driver.
 * 
 * For all products.
 *
 * The version identifier is an alphanumberic string which includes the null terminator.
 *
 * @param version                              Reference in which to return the version identifier
 * @param length                               The maximum allowed length of the string returned in \a version
 *
 * @return 
 *         - \ref NVML_SUCCESS                 if \a version has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a length is too small or \a version is null
 */
nvmlReturn_t DECLDIR nvmlSystemGetDriverVersion(char* version, unsigned int length);

/** @} */

/***************************************************************************************************/
/** @defgroup group2 Unit Queries
 * This page describes that queries that NVML can perform against each unit. For S-class systems only.
 * In each case the device is identified with an nvmlUnit_t handle. This handle is obtained by 
 * calling \ref nvmlUnitGetHandleByIndex().
 *  @{
 */
/***************************************************************************************************/

 /**
 * Retrieve the number of units in the system.
 *
 * For S-class products.
 *
 * @param unitCount                            Reference in which to return the number of units
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a unitCount has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a unitCount is null
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlUnitGetCount(unsigned int *unitCount);

/**
 * Acquire the handle for a particular unit, based on its index.
 *
 * For S-class products.
 *
 * Valid indices are derived from the \a unitCount returned by \ref nvmlUnitGetCount(). 
 *   For example, if \a unitCount is 2 the valid indices are 0 and 1, corresponding to UNIT 0 and UNIT 1.
 *
 * The order in which NVML enumerates units has no guarentees of consistency between reboots.
 *
 * @param index                                The index of the target unit, >= 0 and < \a unitCount
 * @param unit                                 Reference in which to return the unit handle
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a unit has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a index is invalid or \a unit is null
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlUnitGetHandleByIndex(unsigned int index, nvmlUnit_t *unit);

/**
 * Retrieve the static information associated with a unit.
 *
 * For S-class products.
 *
 * See \ref nvmlUnitInfo_t for details on available unit info.
 *
 * @param unit                                 The identifer of the target unit
 * @param info                                 Reference in which to return the unit information
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a info has been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a unit is invalid or \a info is null
 */
nvmlReturn_t DECLDIR nvmlUnitGetUnitInfo(nvmlUnit_t unit, nvmlUnitInfo_t *info);

/**
 * Retrieve the LED state associated with this unit.
 *
 * For S-class products.
 *
 * See \ref nvmlLedState_t for details on allowed states.
 *
 * @param unit                                 The identifer of the target unit
 * @param state                                Reference in which to return the current LED state
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a state has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a unit is invalid or \a state is null
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if this is not an S-class product
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 * 
 * @see nvmlUnitSetLedState()
 */
nvmlReturn_t DECLDIR nvmlUnitGetLedState(nvmlUnit_t unit, nvmlLedState_t *state);

/**
 * Retrieve the PSU stats for the unit.
 *
 * For S-class products.
 *
 * See \ref nvmlPSUInfo_t for details on available PSU info.
 *
 * @param unit                                 The identifer of the target unit
 * @param psu                                  Reference in which to return the PSU information
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a psu has been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a unit is invalid or \a psu is null
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if this is not an S-class product
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlUnitGetPsuInfo(nvmlUnit_t unit, nvmlPSUInfo_t *psu);

/**
 * Retrieve the temperature readings for the unit, in degrees C.
 *
 * For S-class products.
 *
 * Depending on the product, readings may be available for intake (type=0), 
 * exhaust (type=1) and board (type=2).
 *
 * @param unit                                 The identifer of the target unit
 * @param type                                 The type of reading to take
 * @param temp                                 Reference in which to return the intake temperature
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a temp has been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a unit or \a type is invalid or \a temp is null
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if this is not an S-class product
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlUnitGetTemperature(nvmlUnit_t unit, unsigned int type, unsigned int *temp);

/**
 * Retrieve the fan speed readings for the unit.
 *
 * For S-class products.
 *
 * See \ref nvmlUnitFanSpeeds_t for details on available fan speed info.
 *
 * @param unit                                 The identifer of the target unit
 * @param fanSpeeds                            Reference in which to return the fan speed information
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a fanSpeeds has been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a unit is invalid or \a fanSpeeds is null
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if this is not an S-class product
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlUnitGetFanSpeedInfo(nvmlUnit_t unit, nvmlUnitFanSpeeds_t *fanSpeeds);

/**
 * Retrieve the set of GPU devices that are attached to the specified unit.
 *
 * For S-class products.
 *
 * The \a deviceCount argument is expected to be set to the size of the input \a devices array.
 *
 * @param unit                                 The identifer of the target unit
 * @param deviceCount                          Reference in which to provide the \a devies array size, and
 *                                             to return the number of attached GPU devices
 * @param devices                              Reference in which to return the references to the attached GPU devices
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a deviceCount and \a devices have been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INSUFFICIENT_SIZE if \a deviceCount indicates that the \a devices array is too small
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a unit is invalid, either of \a deviceCount or \a devices is null
 */
nvmlReturn_t DECLDIR nvmlUnitGetDevices(nvmlUnit_t unit, unsigned int *deviceCount, nvmlDevice_t *devices);

/** @} */

/***************************************************************************************************/
/** @defgroup group4 Device Queries
 * This page describes that queries that NVML can perform against each device.
 * In each case the device is identified with an nvmlDevice_t handle. This handle is obtained by  
 * calling one of \ref nvmlDeviceGetHandleByIndex(), \ref nvmlDeviceGetHandleBySerial() or 
 * \ref nvmlDeviceGetHandleByPciBusId(). 
 *  @{
 */
/***************************************************************************************************/

 /**
 * Retrieve the number of compute devices in the system. A compute device is a single GPU.
 * 
 * For all products.
 *
 * On some platforms not all devices may be accessible due to permission restrictions. In these
 * cases the device count will reflect only the GPUs that NVML can access.
 *
 * @param deviceCount                          Reference in which to return the number of accessible devices
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a deviceCount has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a deviceCount is null
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetCount(unsigned int *deviceCount);

/**
 * Acquire the handle for a particular device, based on its index.
 * 
 * For all products.
 *
 * Valid indices are derived from the \a accessibleDevices count returned by 
 *   \ref nvmlDeviceGetCount(). For example, if \a accessibleDevices is 2 the valid indices  
 *   are 0 and 1, corresponding to GPU 0 and GPU 1.
 *
 * The order in which NVML enumerates devices has no guarentees of consistency between reboots. For that reason it
 *   is recommended that devices be looked up by their PCI ids or board serial numbers. See 
 *   \ref nvmlDeviceGetHandleBySerial() and \ref nvmlDeviceGetHandleByPciBusId().
 *
 * @param index                                The index of the target gpu, >= 0 and < \a accessibleDevices
 * @param device                               Reference in which to return the device handle
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a device has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a index is invalid or \a device is null
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetHandleByIndex(unsigned int index, nvmlDevice_t *device);

/**
 * Acquire the handle for a particular device, based on its board serial number.
 *
 * For Tesla and Quadro products from the Fermi family.
 *
 * This number corresponds to the value printed directly on the board, and to the value returned by
 *   \ref nvmlDeviceGetSerial().
 *
 * @param serial                               The board serial number of the target gpu
 * @param device                               Reference in which to return the device handle
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a device has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a serial is invalid or \a device is null
 *         - \ref NVML_ERROR_NOT_FOUND         if \a serial does not match a valid device on the system
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetHandleBySerial(char *serial, nvmlDevice_t *device);

/**
 * Acquire the handle for a particular device, based on its PCI bus id.
 * 
 * For all products.
 *
 * This number corresponds to the value \ref nvmlPciInfo_t -> busId returned by \ref nvmlDeviceGetPciInfo().
 *
 * @param pciBusId                             The PCI bus id of the target gpu
 * @param device                               Reference in which to return the device handle
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a device has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a pciBusId is invalid or \a device is null
 *         - \ref NVML_ERROR_NOT_FOUND         if \a pciBusId does not match a valid device on the system
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetHandleByPciBusId(char *pciBusId, nvmlDevice_t *device);

/**
 * Retrieve the name of this device. 
 * 
 * For all products.
 *
 * The name is an alphanumeric string that denotes a particular product, e.g. Tesla C2070. It will not
 * exceed 64 characters in length (including the null terminator).
 *
 * @param device                               The identifer of the target device
 * @param name                                 Reference in which to return the product name
 * @param length                               The maximum allowed length of the string returned in \a name
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a name has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid, \a length is too small or \a name is null
 */
nvmlReturn_t DECLDIR nvmlDeviceGetName(nvmlDevice_t device, char* name, unsigned int length);

/**
 * Retrieve the globally unique serial number associated with this device.
 *
 * For Tesla and Quadro products from the Fermi family.
 *
 * The serial number is an alphanumeric string that will not exceed 30 characters (including the null terminator).
 * This number matches the serial number tag that is physically attached to the board.
 *
 * @param device                               The identifer of the target device
 * @param serial                               Reference in which to return the board/module serial number
 * @param length                               The maximum allowed length of the string returned in \a serial
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a serial has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid, \a length is too small or \a serial is null
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 */
nvmlReturn_t DECLDIR nvmlDeviceGetSerial(nvmlDevice_t device, char* serial, unsigned int length);

/**
 * Retrieve the UUID associated with this device, as a 5 part hexidecimal string.
 *
 * For Tesla products, and Quadro products from the Fermi family.
 *
 * The UUID is a globally unique identifier. It is the only available identifier for pre-Fermi-architecture products.
 * It does NOT correspond to any identifier printed on the board.
 *
 * @param device                               The identifer of the target device
 * @param uuid                                 Reference in which to return the GPU UUID
 * @param length                               The maximum allowed length of the string returned in \a uuid
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a uuid has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid, \a uuid is null or \a length is too small
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 */
nvmlReturn_t DECLDIR nvmlDeviceGetUUID(nvmlDevice_t device, char *uuid, unsigned int length);

/**
 * Retrieve the version info for the device's inforom.
 *
 * For Tesla and Quadro products from the Fermi family.
 *
 * Fermi and higher parts have non-volatile on-board memory for persisting device info, such as aggregate 
 * ECC counts. The version of the data structures in this memory may change from time to time. This method 
 * retrieves this version info. 
 *
 * See \ref nvmlInforomObject_t for details on the available inforom objects.
 *
 * @param device                               The identifer of the target device
 * @param object                               The target inforom object
 * @param version                              Reference in which to return the inforom version
 * @param length                               The maximum allowed length of the string returned in \a version
 *
 * @return 
 *         - \ref NVML_SUCCESS                 if \a version has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a version is null or \a length is too small
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not have an inforom
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetInforomVersion(nvmlDevice_t device, nvmlInforomObject_t object, char *version, unsigned int length);

/**
 * Retrieve the display mode for the this device.
 *
 * For Tesla products, and Quadro products from the Fermi family.
 *
 * This method indicates whether a physical display is currently connected to the device.
 *
 * See \ref nvmlEnableState_t for details on allowed modes.
 *
 * @param device                               The identifer of the target device
 * @param display                              Reference in which to return the display mode
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a display has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a display is null
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 */
nvmlReturn_t DECLDIR nvmlDeviceGetDisplayMode(nvmlDevice_t device, nvmlEnableState_t *display);

/**
 * Retrieve the persistence mode associated with this device.
 *
 * For Tesla products, and Quadro products from the Fermi family.
 * For Linux only.
 *
 * When driver persistence mode is enabled the driver software state is not torn down when the last 
 * client disconnects. By default this feature is disabled. 
 *
 * See \ref nvmlEnableState_t for details on allowed modes.
 *
 * @param device                               The identifer of the target device
 * @param mode                                 Reference in which to return the current driver persistence mode
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a mode has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a mode is null
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlDeviceSetPersistenceMode()
 */
nvmlReturn_t DECLDIR nvmlDeviceGetPersistenceMode(nvmlDevice_t device, nvmlEnableState_t *mode);

/**
 * Retrieve the PCI attributes of this device.
 * 
 * For all products.
 *
 * See \ref nvmlPciInfo_t for details on the available PCI info.
 *
 * @param device                               The identifer of the target device
 * @param pci                                  Reference in which to return the pci info
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a pci has been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a pci is null
 */
nvmlReturn_t DECLDIR nvmlDeviceGetPciInfo(nvmlDevice_t device, nvmlPciInfo_t *pci);

/**
 * Retrieve the current clock speeds for the device.
 *
 * For Tesla products, and Quadro products from the Fermi family.
 *
 * See \ref nvmlClockType_t for details on available clock information.
 *
 * @param device                               The identifer of the target device
 * @param type                                 Identify which clock domain to query
 * @param clock                                Reference in which to return the clock speed in MHz
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a clock has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a clock is null
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device cannot report the specified clock
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetClockInfo(nvmlDevice_t device, nvmlClockType_t type, unsigned int *clock);

/**
 * Retrieve the current operating speed of the device's fan.
 * 
 * For all discrete products with dedicated fans.
 *
 * The fan speed is expressed as a percent of the maximum, i.e. full speed is 100%.
 *
 * @param device                               The identifer of the target device
 * @param speed                                Reference in which to return the fan speed percentage
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a speed has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a speed is null
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not have a fan
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetFanSpeed(nvmlDevice_t device, unsigned int *speed);

/**
 * Retrieve the current temperature readings for the device, in degrees C. 
 * 
 * For all discrete and S-class products.
 *
 * See \ref nvmlTemperatureSensors_t for details on available temperature sensors.
 *
 * @param device                               The identifer of the target device
 * @param sensorType                           Flag that indicates which sensor reading to retrieve
 * @param temp                                 Reference in which to return the temperature reading
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a temp has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a temp is null
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not have the specified sensor
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetTemperature(nvmlDevice_t device, nvmlTemperatureSensors_t sensorType, unsigned int *temp);

/**
 * Retrieve the current power state for the device. 
 *
 * For Tesla products, and Quadro products from the Fermi family.
 *
 * See \ref nvmlPstates_t for details on allowed power states.
 *
 * @param device                               The identifer of the target device
 * @param pState                               Reference in which to return the power state reading
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a pState has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support power state readings
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetPowerState(nvmlDevice_t device, nvmlPstates_t *pState);

/**
 * Retrieve the power management mode associated with this device.
 *
 * For "GF11x" Tesla and Quadro products from the Fermi family.
 * Requires \a NVML_INFOROM_POWER version 3.0 or higher.
 *
 * This flag indicates whether any power management algorithm is currently active on the device. An 
 * enabled state does not necessarily mean the device is being actively throttled -- only that 
 * that the driver will do so if the appropriate conditions are met.
 *
 * See \ref nvmlEnableState_t for details on allowed modes.
 *
 * @param device                               The identifer of the target device
 * @param mode                                 Reference in which to return the current power management mode
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a mode has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a mode is null
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetPowerManagementMode(nvmlDevice_t device, nvmlEnableState_t *mode);

/**
 * Retrieve the power management limit associated with this device, in milliwatts.
 *
 * For "GF11x" Tesla and Quadro products from the Fermi family.
 * Requires \a NVML_INFOROM_POWER version 3.0 or higher.
 *
 * The power limit defines the upper boundary for the card's power draw. If
 * the card's total power draw reaches this limit the power management algorithm kicks in.
 *
 * This reading is only available if power management mode is supported. 
 * See \ref nvmlDeviceGetPowerManagementMode.
 *
 * @param device                               The identifer of the target device
 * @param limit                                Reference in which to return the power management limit
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a limit has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a limit is null
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetPowerManagementLimit(nvmlDevice_t device, unsigned int *limit);

/**
 * Retrieve the power usage reading for the device, in milliwatts. This is the power draw for the entire 
 * board, including GPU, memory, etc.
 *
 * For "GF11x" Tesla and Quadro products from the Fermi family.
 * Requires \a NVML_INFOROM_POWER version 3.0 or higher.
 *
 * The reading is accurate to within a range of +/- 5 watts. It is only available if power management mode
 * is supported. See \ref nvmlDeviceGetPowerManagementMode.
 *
 * @param device                               The identifer of the target device
 * @param power                                Reference in which to return the power usage information
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a power has been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a power is null
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support power readings
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetPowerUsage(nvmlDevice_t device, unsigned int *power);

/**
 * Retrieve the amount of used, free and total memory available on the device, in bytes.
 * 
 * For all products.
 *
 * Enabling ECC reduces the amount of total available memory, due to the extra required parity bits.
 * Under WDDM most device memory is allocated and managed on startup by Windows.
 *
 * Under Linux and Windows TCC, the reported amount of used memory is equal to the sum of memory allocated 
 * by all active channels on the device.
 *
 * See \ref nvmlMemory_t for details on available memory info.
 *
 * @param device                               The identifer of the target device
 * @param memory                               Reference in which to return the memory information
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a memory has been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a memory is null
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetMemoryInfo(nvmlDevice_t device, nvmlMemory_t *memory);

/**
 * Retrieve the current compute mode for the device.
 *
 * For Tesla products, and Quadro products from the Fermi family.
 *
 * See \ref nvmlComputeMode_t for details on allowed compute modes.
 *
 * @param device                               The identifer of the target device
 * @param mode                                 Reference in which to return the current compute mode
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a mode has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a mode is null
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlDeviceSetComputeMode()
 */
nvmlReturn_t DECLDIR nvmlDeviceGetComputeMode(nvmlDevice_t device, nvmlComputeMode_t *mode);

/**
 * Retrieve the current and pending ECC modes for the device.
 *
 * For Tesla and Quadro products from the Fermi family.
 * Requires \a NVML_INFOROM_ECC version 1.0 or higher.
 *
 * Changing ECC modes requires a reboot. The "pending" ECC mode refers to the target mode following
 * the next reboot.
 *
 * See \ref nvmlEnableState_t for details on allowed modes.
 *
 * @param device                               The identifer of the target device
 * @param current                              Reference in which to return the current ECC mode
 * @param pending                              Reference in which to return the pending ECC mode
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a current and \a pending have been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or either \a current or \a pending is null
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlDeviceSetEccMode()
 */
nvmlReturn_t DECLDIR nvmlDeviceGetEccMode(nvmlDevice_t device, nvmlEnableState_t *current, nvmlEnableState_t *pending);

/**
 * Retrieve the total ECC error counts for the device.
 *
 * For Tesla and Quadro products from the Fermi family.
 * Requires \a NVML_INFOROM_ECC version 1.0 or higher.
 *
 * The total error count is the sum of errors across each of the separate memory systems, i.e. the total set of 
 * errors across the entire device.
 *
 * See \ref nvmlEccBitType_t for a description of available bit types.\n
 * See \ref nvmlEccCounterType_t for a description of available counter types.
 *
 * @param device                               The identifer of the target device
 * @param bitType                              Flag that specifies the bit-type of the errors. 
 * @param counterType                          Flag that specifies the counter-type of the errors. 
 * @param eccCounts                            Reference in which to return the specified ECC errors
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a eccCounts has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device, \a bitType or \a counterType is invalid, or \a eccCounts is null
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlDeviceClearEccErrorCounts()
 */
nvmlReturn_t DECLDIR nvmlDeviceGetTotalEccErrors(nvmlDevice_t device, nvmlEccBitType_t bitType, nvmlEccCounterType_t counterType, unsigned long long *eccCounts);

/**
 * Retrieve the detailed ECC error counts for the device.
 *
 * For Tesla and Quadro products from the Fermi family.
 * Requires \a NVML_INFOROM_ECC version 2.0 or higher to report aggregate location-based ECC counts.
 * Requires \a NVML_INFOROM_ECC version 1.0 or higher to report all other ECC counts.
 *
 * Detailed errors provide separate ECC counts for specific parts of the memory system.
 *
 * See \ref nvmlEccBitType_t for a description of available bit types.\n
 * See \ref nvmlEccCounterType_t for a description of available counter types.\n
 * See \ref nvmlEccErrorCounts_t for a description of provided detailed ECC counts.
 *
 * @param device                               The identifer of the target device
 * @param bitType                              Flag that specifies the bit-type of the errors. 
 * @param counterType                          Flag that specifies the counter-type of the errors. 
 * @param eccCounts                            Reference in which to return the specified ECC errors
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a eccCounts has been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device, \a bitType or \a counterType is invalid, or \a eccCounts is null
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlDeviceClearEccErrorCounts()
 */
nvmlReturn_t DECLDIR nvmlDeviceGetDetailedEccErrors(nvmlDevice_t device, nvmlEccBitType_t bitType, nvmlEccCounterType_t counterType, nvmlEccErrorCounts_t *eccCounts);

/**
 * Retrieve the current utilization rates for the device's major subsystems.
 *
 * For Tesla products, and Quadro products from the Fermi family.
 *
 * See \ref nvmlUtilization_t for details on available utilization rates.
 *
 * @param device                               The identifer of the target device
 * @param utilization                          Reference in which to return the utilization information
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a utilization has been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a utilization is null
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetUtilizationRates(nvmlDevice_t device, nvmlUtilization_t *utilization);

/**
 * Retrieve the current and pending driver model for the device.
 *
 * For Tesla products, and Quadro products from the Fermi family.
 * For windows only.
 *
 * On Windows platforms the device driver can run in either WDDM or WDM (TCC) mode. If a display is attached
 * to the device it must run in WDDM mode. TCC mode is preferred if a display is not attached.
 *
 * See \ref nvmlDriverModel_t for details on available driver models.
 *
 * @param device                               The identifer of the target device
 * @param current                              Reference in which to return the current driver model
 * @param pending                              Reference in which to return the pending driver model
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a current and \a pending have been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or either of \a current or \a pending is null
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the platform is not windows
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 * 
 * @see nvmlDeviceSetDriverModel()
 */
nvmlReturn_t DECLDIR nvmlDeviceGetDriverModel(nvmlDevice_t device, nvmlDriverModel_t *current, nvmlDriverModel_t *pending);

/** @} */

/***************************************************************************************************/
/** @defgroup group10 Unit Commands
 *  This page describes NVML operations that change the state of the unit. For S-class products.
 *  Each of these requires root/admin access. Non-admin users will see an NVML_ERROR_NO_PERMISSION
 *  error code when invoking any of these methods.
 *  @{
 */
/***************************************************************************************************/

/**
 * Set the LED state for the unit. The LED can be either green (0) or amber (1).
 *
 * For S-class products.
 * Requires root/admin permissions.
 *
 * This operation takes effect immediately.
 *
 * <b>Current S-Class products don't provide unique LEDs for each unit. As such, both front 
 * and back LEDs will be toggled in unision regardless of which unit is specified with this command.</b>
 *
 * See \ref nvmlLedColor_t for available colors.
 *
 * @param unit                                 The identifer of the target unit
 * @param color                                The target LED color
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if the LED color has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a unit or \a color is invalid
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if this is not an S-class product
 *         - \ref NVML_ERROR_NO_PERMISSION     if the user doesn't have permision to perform this operation
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 * 
 * @see nvmlUnitGetLedState()
 */
nvmlReturn_t DECLDIR nvmlUnitSetLedState(nvmlUnit_t unit, nvmlLedColor_t color);

/** @} */

/***************************************************************************************************/
/** @defgroup group5 Device Commands
 *  This page describes NVML operations that change the state of the device.
 *  Each of these requires root/admin access. Non-admin users will see an NVML_ERROR_NO_PERMISSION
 *  error code when invoking any of these methods.
 *  @{
 */
/***************************************************************************************************/

/**
 * Set the persistence mode for the device.
 *
 * For Tesla products, and Quadro products from the Fermi family.
 * For Linux only.
 * Requires root/admin permissions.
 *
 * The persistence mode determines whether the GPU driver software is torn down after the last client
 * exits.
 *
 * This operation takes effect immediately. It is not persistent across reboots. After each reboot the
 * persistence mode is reset to "Disabled".
 *
 * See \ref nvmlEnableState_t for available modes.
 *
 * @param device                               The identifer of the target device
 * @param mode                                 The target persistence mode
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if the persistence mode was set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_NO_PERMISSION     if the user doesn't have permision to perform this operation
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlDeviceGetPersistenceMode()
 */
nvmlReturn_t DECLDIR nvmlDeviceSetPersistenceMode(nvmlDevice_t device, nvmlEnableState_t mode);

/**
 * Set the compute mode for the device.
 *
 * For Tesla products, and Quadro products from the Fermi family.
 * Requires root/admin permissions.
 *
 * The compute mode determines whether a GPU can be used for compute operations and whether it can
 * be shared across contexts.
 *
 * This operation takes effect immediately. Under Linux it is not persistent across reboots and
 * always resets to "Default". Under windows it is persistent.
 *
 * Under windows compute mode may only be set to DEFAULT when running in WDDM
 *
 * See \ref nvmlComputeMode_t for details on available compute modes.
 *
 * @param device                               The identifer of the target device
 * @param mode                                 The target compute mode
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if the compute mode was set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_NO_PERMISSION     if the user doesn't have permision to perform this operation
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlDeviceGetComputeMode()
 */
nvmlReturn_t DECLDIR nvmlDeviceSetComputeMode(nvmlDevice_t device, nvmlComputeMode_t mode);

/**
 * Set the ECC mode for the device.
 *
 * For Tesla and Quadro products from the Fermi family.
 * Requires \a NVML_INFOROM_ECC version 1.0 or higher.
 * Requires root/admin permissions.
 *
 * The ECC mode determines whether the GPU enables its ECC support.
 *
 * This operation takes effect after the next reboot.
 *
 * See \ref nvmlEnableState_t for details on available modes.
 *
 * @param device                               The identifer of the target device
 * @param ecc                                  The target ECC mode
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if the ECC mode was set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_NO_PERMISSION     if the user doesn't have permision to perform this operation
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlDeviceGetEccMode()
 */
nvmlReturn_t DECLDIR nvmlDeviceSetEccMode(nvmlDevice_t device, nvmlEnableState_t ecc);  

/**
 * Clear the ECC error counts for the device.
 *
 * For Tesla and Quadro products from the Fermi family.
 * Requires \a NVML_INFOROM_ECC version 2.0 or higher to clear aggregate location-based ECC counts.
 * Requires \a NVML_INFOROM_ECC version 1.0 or higher to clear all other ECC counts.
 * Requires root/admin permissions.
 *
 * Sets all of the specified ECC counters to 0, including both detailed and total counts.
 *
 * This operation takes effect immediately.
 *
 * See \ref nvmlEccCounterType_t for details on available counter types.
 *
 * @param device                               The identifer of the target device
 * @param counterType                          Flag that indicates which type of errors should be cleared.
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if the error counts were cleared
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_NO_PERMISSION     if the user doesn't have permision to perform this operation
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see 
 *      - nvmlDeviceGetDetailedEccErrors()
 *      - nvmlDeviceGetTotalEccErrors()
 */
nvmlReturn_t DECLDIR nvmlDeviceClearEccErrorCounts(nvmlDevice_t device, nvmlEccCounterType_t counterType);

/**
 * Set the driver model for the device.
 *
 * For Tesla products, and Quadro products from the Fermi family.
 * For windows only.
 * Requires root/admin permissions.
 *
 * On Windows platforms the device driver can run in either WDDM or WDM (TCC) mode. If a display is attached
 * to the device it must run in WDDM mode.  
 *
 * It is possible to force the change to WDM (TCC) while the display is still attached with a force flag (nvmlFlagForce).
 * This should only be done if the host is subsequently powered down and the display is detached from the device
 * before the next reboot. 
 *
 * This operation takes effect after the next reboot.
 * 
 * Under windows driver model may only be set to WDDM when running in DEFAULT compute mode.
 *
 * See \ref nvmlDriverModel_t for details on available driver models.
 * See \ref nvmlFlagDefault and \ref nvmlFlagForce
 *
 * @param device                               The identifier of the target device
 * @param driverModel                          The target driver model
 * @param flags                                Flags that change the default behavior
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if the driver model has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the platform is not windows or the device does not support this feature
 *         - \ref NVML_ERROR_NO_PERMISSION     if the user doesn't have permission to perform this operation
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 * 
 * @see nvmlDeviceGetDriverModel()
 */
nvmlReturn_t DECLDIR nvmlDeviceSetDriverModel(nvmlDevice_t device, nvmlDriverModel_t driverModel, unsigned int flags);

/** @} */

#ifdef __cplusplus
}
#endif

#endif
