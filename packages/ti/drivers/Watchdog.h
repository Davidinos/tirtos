/*
 * Copyright (c) 2013, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 /** ============================================================================
 *  @file       Watchdog.h
 *
 *  @brief      Watchdog driver interface
 *
 *  The Watchdog header file should be included in an application as follows:
 *  @code
 *  #include <ti/drivers/Watchdog.h>
 *  @endcode
 *
 *  # Operation #
 *
 *  The Watchdog driver simplifies configuring and starting the Watchdog
 *  peripherals. The Watchdog can be set up to produce a reset signal after a
 *  timeout or simply cause a hardware interrupt at a programmable interval.
 *  The driver provides the ability to specify a callback function that is
 *  called when the Watchdog causes an interrupt.
 *
 *  When resets are turned on, it is the user application's responsibility to
 *  call Watchdog_clear() in order to clear the Watchdog and prevent a reset.
 *  Watchdog_clear() can be called at any time.
 *
 *  The Watchdog driver does not configure the board peripherals. This must be
 *  done before any calls to the Watchdog driver. The examples include a
 *  board-specific *Board_initWatchdog()* functions in the board .c and .h
 *  files.
 *
 *  ## Opening the driver #
 *
 *  @code
 *  Watchdog_Handle     handle;
 *  Watchdog_Params     params;
 *
 *  Watchdog_Params_init(&params);
 *  params.resetMode = Watchdog_RESET_OFF;
 *  handle = Watchdog_open(Watchdog_configIndex, &params);
 *  if (!handle) {
 *      System_printf("Watchdog did not open");
 *  }
 *
 *  // handle may now be used to interact with the Watchdog just created
 *  @endcode
 *
 *  # Implementation #
 *
 *  This module serves as the main interface for TI-RTOS
 *  applications. Its purpose is to redirect the module's APIs to specific
 *  peripheral implementations which are specified using a pointer to a
 *  Watchdog_FxnTable.
 *
 *  The Watchdog driver interface module is joined (at link time)
 *  to a NULL-terminated array of Watchdog_Config data structures named
 *  *Watchdog_config*. *Watchdog_config* is implemented in the application with
 *  each entry being an instance of a Watchdog peripheral. Each entry in
 *  *Watchdog_config* contains a:
 *  - (Watchdog_FxnTable *) to a set of functions that implement a Watchdog peripheral
 *  - (Void *) data object that is associated with the Watchdog_FxnTable
 *  - (Void *) hardware attributes that are associated to the Watchdog_FxnTable
 *
 *  Currently the following Watchdog implementations are supported:
 *  - @ref WatchdogTiva.h
 *  - @ref WatchdogMSP430.h
 *
 *  # Instrumentation #
 *
 *  The Watchdog driver interface produces log statements if
 *  instrumentation is enabled.
 *
 *  Diagnostics Mask | Log details |
 *  ---------------- | ----------- |
 *  Diags_USER1      | basic Watchdog operations to be performed |
 *
 *  ============================================================================
 */

#ifndef ti_drivers_Watchdog__include
#define ti_drivers_Watchdog__include

#ifdef __cplusplus
extern "C" {
#endif

#include <xdc/std.h>
#include <ti/drivers/ENV.h>

/*!
*  @brief      Watchdog Handle
*/
typedef struct Watchdog_Config *Watchdog_Handle;

/*!
 *  @brief      Watchdog debug stall settings
 *
 *  This enumeration defines the debug stall modes for the Watchdog. On some
 *  targets, the Watchdog timer will continue to count down while a debugging
 *  session is halted. To avoid unwanted resets, the Watchdog can be set to
 *  stall while the processor is stopped by the debugger.
 */
typedef enum Watchdog_DebugMode {
    Watchdog_DEBUG_STALL_ON, /*!< Watchdog will be stalled at breakpoints */
    Watchdog_DEBUG_STALL_OFF /*!< Watchdog will keep running at breakpoints */
} Watchdog_DebugMode;

/*!
 *  @brief      Watchdog reset mode settings
 *
 *  This enumeration defines the reset modes for the Watchdog. The Watchdog can
 *  be configured to either generate a reset upon timeout or simply produce a
 *  periodic interrupt.
 */
typedef enum Watchdog_ResetMode {
    Watchdog_RESET_OFF, /*!< Timeouts generate interrupts only */
    Watchdog_RESET_ON   /*!< Generates reset after timeout */
} Watchdog_ResetMode;

/*!
 *  @brief      Watchdog callback pointer
 *
 *  This is the typedef for the function pointer that will allow a callback
 *  function to be specified in the Watchdog_Params structure. The function
 *  will take a Watchdog_Handle of the Watchdog causing the interrupt (cast as
 *  a UArgas an argument.
 */
typedef Void (*Watchdog_Callback)(UArg);

/*!
 *  @brief      Watchdog Parameters
 *
 *  @see        Watchdog_Params_init
 */
typedef struct Watchdog_Params {
    Watchdog_Callback   callbackFxn;    /*!< Pointer to callback. Not supported
                                             on all targets. */
    Watchdog_ResetMode  resetMode;      /*!< Mode to enable resets.
                                             Not supported on all targets. */
    Watchdog_DebugMode  debugStallMode; /*!< Mode to stall WDT at breakpoints.
                                             Not supported on all targets. */
} Watchdog_Params;

/*!
 *  @brief      A function pointer to a driver specific implementation of
 *              Watchdog_clear().
 */
typedef Void (*Watchdog_ClearFxn)       (Watchdog_Handle handle);

/*!
 *  @brief      A function pointer to a driver specific implementation of
 *              Watchdog_close().
 */
typedef Void (*Watchdog_CloseFxn)       (Watchdog_Handle handle);

/*!
 *  @brief      A function pointer to a driver specific implementation of
 *              Watchdog_init().
 */
typedef Void (*Watchdog_InitFxn)        (Watchdog_Handle handle);

/*!
 *  @brief      A function pointer to a driver specific implementation of
 *              Watchdog_open().
 */
typedef Watchdog_Handle (*Watchdog_OpenFxn)  (Watchdog_Handle handle,
                                              Watchdog_Params *params);

/*!
 *  @brief      A function pointer to a driver specific implementation of
 *              Watchdog_setReload().
 */
typedef Void (*Watchdog_SetReloadFxn)   (Watchdog_Handle handle, ULong value);

/*!
 *  @brief      The definition of a Watchdog function table that contains the
 *              required set of functions to control a specific Watchdog driver
 *              implementation.
 */
typedef struct Watchdog_FxnTable {
    Watchdog_ClearFxn          watchdogClear;
    Watchdog_CloseFxn          watchdogClose;
    Watchdog_InitFxn           watchdogInit;
    Watchdog_OpenFxn           watchdogOpen;
    Watchdog_SetReloadFxn      watchdogSetReload;
} Watchdog_FxnTable;

/*!
 *  @brief      Watchdog Global configuration
 */
typedef struct Watchdog_Config {
    Watchdog_FxnTable const *fxnTablePtr; /*!< Pointer to Watchdog function table */
    Void              *object;            /*!< Pointer to Watchdog object */
    Void              const *hwAttrs;     /*!< Pointer to hardware attribute */
} Watchdog_Config;

/*!
 *  @brief      Clears the Watchdog
 *
 *  Clears the Watchdog to to prevent a reset signal from being generated if the
 *  module is in Watchdog_RESET_ON reset mode.
 *
 *  @param      handle      Watchdog Handle
 */
extern Void Watchdog_clear(Watchdog_Handle handle);

/*!
 *  @brief      Stop the Watchdog running
 *
 *  Stops (holds) the Watchdog counting. This function not applicable for all
 *  platforms.
 *
 *  @param      handle      Watchdog Handle
 */
extern Void Watchdog_close(Watchdog_Handle handle);

/*!
 *  @brief      Initializes the Watchdog module
 *
 *  The application-provided Watchdog_config must be present before the
 *  Watchdog_init function is called. The Watchdog_config must be persistent
 *  and not changed after Watchdog_init is called. This function must be called
 *  before any of the other Watchdog driver APIs.
 */
extern Void Watchdog_init(Void);

/*!
 *  @brief      Opens a Watchdog
 *
 *  Opens a Watchdog object with the index and parameters specified, and
 *  returns a Watchdog_Handle.
 *
 *  @param      index         Logical peripheral number indexed in array of
 *                            Watchdog_config.
 *  @param      params        Pointer to a parameter struct. If it is NULL,
 *                            default values will be used.
 *
 *  @return     A Watchdog_Handle if successful, NULL if it is not.
 */
extern Watchdog_Handle Watchdog_open(UInt index, Watchdog_Params *params);

/*!
 *  @brief      Initializes Watchdog parameters
 *
 *  Default parameters:
 *  - callbackFxn       = NULL
 *  - resetMode         = Watchdog_RESET_ON
 *  - debugStallMode    = Watchdog_DEBUG_STALL_ON
 *
 *  @param      params      Pointer to a parameter struct
 */
extern Void Watchdog_Params_init(Watchdog_Params *params);

/*!
 *  @brief      Sets the Watchdog reload value
 *
 *  Sets the value from which the Watchdog will countdown after it reaches
 *  zero. This is how the reload value can be changed after the Watchdog has
 *  already been opened. The new reload value will be loaded into the Watchdog
 *  timer when this function is called. Watchdog_setReload is not reentrant.
 *
 *  This API is not applicable for all platforms. See the page for your
 *  specific driver implementation for details.
 *
 *  @param      handle      Watchdog Handle
 *  @param      value       Value to be loaded into Watchdog timer
 */
extern Void Watchdog_setReload(Watchdog_Handle handle, ULong value);

#ifdef __cplusplus
}
#endif

#endif /* ti_drivers_Watchdog__include */
