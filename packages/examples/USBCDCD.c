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

/*
 *  ======== USBCDCD.c ========
 */

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/gates/GateMutex.h>
#include <ti/sysbios/hal/Hwi.h>
#include <ti/sysbios/knl/Semaphore.h>

#include <stdbool.h>
#include <stdint.h>

/* driverlib Header files */
#include <inc/hw_ints.h>
#include <inc/hw_types.h>

/* usblib Header files */
#include <usblib/usb-ids.h>
#include <usblib/usblib.h>
#include <usblib/usbcdc.h>
#include <usblib/device/usbdevice.h>
#include <usblib/device/usbdcdc.h>

/* Example/Board Header files */
#include "USBCDCD.h"

#if defined(TIVAWARE)
/* c99 types required by TivaWare */
#include <stdint.h>
typedef uint32_t            USBCDCDEventType;
#else
#define eUSBModeForceDevice USB_MODE_FORCE_DEVICE
typedef ULong               USBCDCDEventType;
#endif

/* Defines */
#define USBBUFFERSIZE   256

/* Typedefs */
typedef volatile enum {
    USBCDCD_STATE_IDLE = 0,
    USBCDCD_STATE_INIT,
    USBCDCD_STATE_UNCONFIGURED
} USBCDCD_USBState;

/* Static variables and handles */
static volatile USBCDCD_USBState state;
static UChar receiveBuffer[USBBUFFERSIZE];
static UChar receiveBufferWorkspace[USB_BUFFER_WORKSPACE_SIZE];
static UChar transmitBuffer[USBBUFFERSIZE];
static UChar transmitBufferWorkspace[USB_BUFFER_WORKSPACE_SIZE];
const tUSBBuffer txBuffer;
const tUSBBuffer rxBuffer;
static GateMutex_Handle gateTxSerial;
static GateMutex_Handle gateRxSerial;
static GateMutex_Handle gateUSBWait;
static Semaphore_Handle semTxSerial;
static Semaphore_Handle semRxSerial;
static Semaphore_Handle semUSBConnected;

/* Function prototypes */
static USBCDCDEventType cbRxHandler(Void *cbData, USBCDCDEventType event,
                                    USBCDCDEventType eventMsg,
                                    Void *eventMsgPtr);
static USBCDCDEventType cbSerialHandler(Void *cbData, USBCDCDEventType event,
                                        USBCDCDEventType eventMsg,
                                        Void *eventMsgPtr);
static USBCDCDEventType cbTxHandler(Void *cbData, USBCDCDEventType event,
                                    USBCDCDEventType eventMsg,
                                    Void *eventMsgPtr);
static Void USBCDCD_hwiHandler(UArg arg0);
static UInt rxData(UChar *pStr, UInt length, UInt timeout);
static UInt txData(const UChar *pStr, Int length, UInt timeout);
Void USBCDCD_init(Void);
UInt USBCDCD_receiveData(UChar *pStr, UInt length, UInt timeout);
UInt USBCDCD_sendData(const UChar *pStr, UInt length, UInt timeout);
Bool USBCDCD_waitForConnect(UInt timeout);

/* The languages supported by this device. */
const UChar langDescriptor[] =
{
    4,
    USB_DTYPE_STRING,
    USBShort(USB_LANG_EN_US)
};

/* The manufacturer string. */
const UChar manufacturerString[] =
{
    (17 + 1) * 2,
    USB_DTYPE_STRING,
    'T', 0, 'e', 0, 'x', 0, 'a', 0, 's', 0, ' ', 0, 'I', 0, 'n', 0, 's', 0,
    't', 0, 'r', 0, 'u', 0, 'm', 0, 'e', 0, 'n', 0, 't', 0, 's', 0,
};

/* The product string. */
const UChar productString[] =
{
    2 + (16 * 2),
    USB_DTYPE_STRING,
    'V', 0, 'i', 0, 'r', 0, 't', 0, 'u', 0, 'a', 0, 'l', 0, ' ', 0,
    'C', 0, 'O', 0, 'M', 0, ' ', 0, 'P', 0, 'o', 0, 'r', 0, 't', 0
};

/* The serial number string. */
const UChar serialNumberString[] =
{
    (8 + 1) * 2,
    USB_DTYPE_STRING,
    '1', 0, '2', 0, '3', 0, '4', 0, '5', 0, '6', 0, '7', 0, '8', 0
};

/* The interface description string. */
const UChar controlInterfaceString[] =
{
    2 + (21 * 2),
    USB_DTYPE_STRING,
    'A', 0, 'C', 0, 'M', 0, ' ', 0, 'C', 0, 'o', 0, 'n', 0, 't', 0,
    'r', 0, 'o', 0, 'l', 0, ' ', 0, 'I', 0, 'n', 0, 't', 0, 'e', 0,
    'r', 0, 'f', 0, 'a', 0, 'c', 0, 'e', 0
};

/* The configuration description string. */
const UChar configString[] =
{
    2 + (26 * 2),
    USB_DTYPE_STRING,
    'S', 0, 'e', 0, 'l', 0, 'f', 0, ' ', 0, 'P', 0, 'o', 0, 'w', 0,
    'e', 0, 'r', 0, 'e', 0, 'd', 0, ' ', 0, 'C', 0, 'o', 0, 'n', 0,
    'f', 0, 'i', 0, 'g', 0, 'u', 0, 'r', 0, 'a', 0, 't', 0, 'i', 0,
    'o', 0, 'n', 0
};

/* The descriptor string table. */
const UChar * const stringDescriptors[] =
{
    langDescriptor,
    manufacturerString,
    productString,
    serialNumberString,
    controlInterfaceString,
    configString
};

#define STRINGDESCRIPTORSCOUNT (sizeof(stringDescriptors) / \
                                sizeof(UChar *))

#if defined(TIVAWARE)
static tUSBDCDCDevice serialDevice =
{
    USB_VID_TI_1CBE,
    USB_PID_SERIAL,
    0,
    USB_CONF_ATTR_SELF_PWR,

    cbSerialHandler,
    NULL,

    USBBufferEventCallback,
    (Void *)&rxBuffer,

    USBBufferEventCallback,
    (Void *)&txBuffer,

    stringDescriptors,
    STRINGDESCRIPTORSCOUNT
};
#else
static tCDCSerInstance serialInstance;
const tUSBDCDCDevice serialDevice =
{
    USB_VID_TI,
    USB_PID_SERIAL,
    0,
    USB_CONF_ATTR_SELF_PWR,

    cbSerialHandler,
    NULL,

    USBBufferEventCallback,
    (Void *)&rxBuffer,

    USBBufferEventCallback,
    (Void *)&txBuffer,

    stringDescriptors,
    STRINGDESCRIPTORSCOUNT,

    &serialInstance /* Old usblib stores a pointer */
};
#endif

const tUSBBuffer rxBuffer =
{
    FALSE,                      /* This is a receive buffer. */
    cbRxHandler,                /* pfnCallback */
    (Void *)&serialDevice,      /* Callback data is our device pointer. */
    USBDCDCPacketRead,          /* pfnTransfer */
    USBDCDCRxPacketAvailable,   /* pfnAvailable */
    (Void *)&serialDevice,      /* pvHandle */
    receiveBuffer,              /* pcBuffer */
    USBBUFFERSIZE,              /* ulBufferSize */
    receiveBufferWorkspace      /* pvWorkspace */
};

const tUSBBuffer txBuffer =
{
    TRUE,                       /* This is a transmit buffer. */
    cbTxHandler,                /* pfnCallback */
    (Void *)&serialDevice,      /* Callback data is our device pointer. */
    USBDCDCPacketWrite,         /* pfnTransfer */
    USBDCDCTxPacketAvailable,   /* pfnAvailable */
    (Void *)&serialDevice,      /* pvHandle */
    transmitBuffer,             /* pcBuffer */
    USBBUFFERSIZE,              /* ulBufferSize */
    transmitBufferWorkspace     /* pvWorkspace */
};

static tLineCoding g_sLineCoding=
{
    115200,                     /* 115200 baud rate. */
    1,                          /* 1 Stop Bit. */
    0,                          /* No Parity. */
    8                           /* 8 Bits of data. */
};

/*
 *  ======== cbRxHandler ========
 *  Callback handler for the USB stack.
 *
 *  Callback handler call by the USB stack to notify us on what has happened in
 *  regards to the keyboard.
 *
 *  @param(cbData)          A callback pointer provided by the client.
 *
 *  @param(event)           Identifies the event that occurred in regards to
 *                          this device.
 *
 *  @param(eventMsgData)    A data value associated with a particular event.
 *
 *  @param(eventMsgPtr)     A data pointer associated with a particular event.
 *
 */
static USBCDCDEventType cbRxHandler(Void *cbData, USBCDCDEventType event,
                                    USBCDCDEventType eventMsg,
                                    Void *eventMsgPtr)
{
    switch (event) {
        case USB_EVENT_RX_AVAILABLE:
            Semaphore_post(semRxSerial);
            break;

        case USB_EVENT_DATA_REMAINING:
            break;

        case USB_EVENT_REQUEST_BUFFER:
            break;

        default:
            break;
    }

    return (0);
}

/*
 *  ======== cbSerialHandler ========
 *  Callback handler for the USB stack.
 *
 *  Callback handler call by the USB stack to notify us on what has happened in
 *  regards to the keyboard.
 *
 *  @param(cbData)          A callback pointer provided by the client.
 *
 *  @param(event)           Identifies the event that occurred in regards to
 *                          this device.
 *
 *  @param(eventMsgData)    A data value associated with a particular event.
 *
 *  @param(eventMsgPtr)     A data pointer associated with a particular event.
 *
 */
static USBCDCDEventType cbSerialHandler(Void *cbData, USBCDCDEventType event,
                                        USBCDCDEventType eventMsg,
                                        Void *eventMsgPtr)
{
    tLineCoding *psLineCoding;

    /* Determine what event has happened */
    switch (event) {
        case USB_EVENT_CONNECTED:
            state = USBCDCD_STATE_INIT;
            Semaphore_post(semUSBConnected);
            break;

        case USB_EVENT_DISCONNECTED:
            state = USBCDCD_STATE_UNCONFIGURED;
            break;

        case USBD_CDC_EVENT_GET_LINE_CODING:
            /* Create a pointer to the line coding information. */
            psLineCoding = (tLineCoding *)eventMsgPtr;

            /* Copy the current line coding information into the structure. */
            *(psLineCoding) = g_sLineCoding;
            break;

        case USBD_CDC_EVENT_SET_LINE_CODING:
            /* Create a pointer to the line coding information. */
            psLineCoding = (tLineCoding *)eventMsgPtr;

            /*
             * Copy the line coding information into the current line coding
             * structure.
             */
            g_sLineCoding = *(psLineCoding);
            break;

        case USBD_CDC_EVENT_SET_CONTROL_LINE_STATE:
            break;

        case USBD_CDC_EVENT_SEND_BREAK:
            break;

        case USBD_CDC_EVENT_CLEAR_BREAK:
            break;

        case USB_EVENT_SUSPEND:
            break;

        case USB_EVENT_RESUME:
            break;

        default:
            break;
    }

    return (0);
}

/*
 *  ======== cbTxHandler ========
 *  Callback handler for the USB stack.
 *
 *  Callback handler call by the USB stack to notify us on what has happened in
 *  regards to the keyboard.
 *
 *  @param(cbData)          A callback pointer provided by the client.
 *
 *  @param(event)           Identifies the event that occurred in regards to
 *                          this device.
 *
 *  @param(eventMsgData)    A data value associated with a particular event.
 *
 *  @param(eventMsgPtr)     A data pointer associated with a particular event.
 *
 */
static USBCDCDEventType cbTxHandler(Void *cbData, USBCDCDEventType event,
                                    USBCDCDEventType eventMsg,
                                    Void *eventMsgPtr)
{
    switch (event) {
        case USB_EVENT_TX_COMPLETE:
            /*
             * Data was sent, so there should be some space available on the
             * buffer
             */
            Semaphore_post(semTxSerial);
            break;

        default:
            break;
    }

    return (0);
}

/*
 *  ======== USBCDCD_hwiHandler ========
 *  This function calls the USB library's device interrupt handler.
 */
static Void USBCDCD_hwiHandler(UArg arg0)
{
    USB0DeviceIntHandler();
}

/*
 *  ======== rxData ========
 */
static UInt rxData(UChar *pStr, UInt length, UInt timeout)
{
    UInt read = 0;

    if (USBBufferDataAvailable(&rxBuffer) || Semaphore_pend(semRxSerial, timeout)) {
       read = USBBufferRead(&rxBuffer, pStr, length);
    }

    return (read);
}


/*
 *  ======== txData ========
 */
static UInt txData(const UChar *pStr, Int length, UInt timeout)
{
    ULong buffAvailSize;
    UInt bufferedCount = 0;
    UInt sendCount = 0;
    UChar *sendPtr;

    while (bufferedCount != length) {
        /* Determine the buffer size available */
        buffAvailSize = USBBufferSpaceAvailable(&txBuffer);

        /* Determine how much needs to be sent */
        if ((length - bufferedCount) > buffAvailSize) {
            sendCount = buffAvailSize;
        }
        else {
            sendCount = length - bufferedCount;
        }

        /* Adjust the pointer to the data */
        sendPtr = (UChar *)pStr + bufferedCount;

        /* Place the contents into the USB BUffer */
        bufferedCount += USBBufferWrite(&txBuffer, sendPtr, sendCount);

        /* Pend until some data was sent through the USB*/
        if (!Semaphore_pend(semTxSerial, timeout)) {
            break;
        }
    }

    return (bufferedCount);
}

/*
 *  ======== USBCDCD_init ========
 */
Void USBCDCD_init(Void)
{
    Hwi_Handle hwi;
    Error_Block eb;
    Semaphore_Params semParams;

    Error_init(&eb);

    /* Install interrupt handler */
    hwi = Hwi_create(INT_USB0, USBCDCD_hwiHandler, NULL, &eb);
    if (hwi == NULL) {
        System_abort("Can't create USB Hwi");
    }

    /* RTOS primitives */
    Semaphore_Params_init(&semParams);
    semParams.mode = Semaphore_Mode_BINARY;
    semTxSerial = Semaphore_create(0, &semParams, &eb);
    if (semTxSerial == NULL) {
        System_abort("Can't create TX semaphore");
    }

    semRxSerial = Semaphore_create(0, &semParams, &eb);
    if (semRxSerial == NULL) {
        System_abort("Can't create RX semaphore");
    }

    semUSBConnected = Semaphore_create(0, &semParams, &eb);
    if (semUSBConnected == NULL) {
        System_abort("Can't create USB semaphore");
    }

    gateTxSerial = GateMutex_create(NULL, &eb);
    if (gateTxSerial == NULL) {
        System_abort("Can't create gate");
    }

    gateRxSerial = GateMutex_create(NULL, &eb);
    if (gateRxSerial == NULL) {
        System_abort("Can't create gate");
    }

    gateUSBWait = GateMutex_create(NULL, &eb);
    if (gateUSBWait == NULL) {
        System_abort("Could not create USB Wait gate");
    }

    /* State specific variables */
    state = USBCDCD_STATE_UNCONFIGURED;

    /* Set the USB stack mode to Device mode with VBUS monitoring */
    USBStackModeSet(0, eUSBModeForceDevice, 0);

    /*
     * Pass our device information to the USB HID device class driver,
     * initialize the USB controller and connect the device to the bus.
     */
    if (!USBDCDCInit(0, &serialDevice)) {
        System_abort("Error initializing the serial device");
    }
}

/*
 *  ======== USBCDCD_receiveData ========
 */
UInt USBCDCD_receiveData(UChar *pStr, UInt length, UInt timeout)
{
    UInt retValue = 0;
    UInt key;

    switch (state) {
        case USBCDCD_STATE_UNCONFIGURED:
            USBCDCD_waitForConnect(timeout);
            break;

        case USBCDCD_STATE_INIT:
            /* Acquire lock */
            key = GateMutex_enter(gateRxSerial);

            USBBufferInit(&txBuffer);
            USBBufferInit(&rxBuffer);

            state = USBCDCD_STATE_IDLE;

            retValue = rxData(pStr, length, timeout);

            /* Release lock */
            GateMutex_leave(gateRxSerial, key);
            break;

        case USBCDCD_STATE_IDLE:
            /* Acquire lock */
            key = GateMutex_enter(gateRxSerial);

            retValue = rxData(pStr, length, timeout);

            /* Release lock */
            GateMutex_leave(gateRxSerial, key);
            break;

        default:
            break;
    }

    return (retValue);
}

/*
 *  ======== USBCDCD_sendData ========
 */
UInt USBCDCD_sendData(const UChar *pStr, UInt length, UInt timeout)
{
    UInt retValue = 0;
    UInt key;

    switch (state) {
        case USBCDCD_STATE_UNCONFIGURED:
            USBCDCD_waitForConnect(timeout);
            break;

        case USBCDCD_STATE_INIT:
            /* Acquire lock */
            key = GateMutex_enter(gateTxSerial);

            USBBufferInit(&txBuffer);
            USBBufferInit(&rxBuffer);

            state = USBCDCD_STATE_IDLE;

            retValue = txData(pStr, length, timeout);

            /* Release lock */
            GateMutex_leave(gateTxSerial, key);
            break;

        case USBCDCD_STATE_IDLE:
            /* Acquire lock */
            key = GateMutex_enter(gateTxSerial);

            retValue = txData(pStr, length, timeout);

            /* Release lock */
            GateMutex_leave(gateTxSerial, key);
            break;

        default:
            break;
    }

    return (retValue);
}

/*
 *  ======== USBCDCD_waitForConnect ========
 */
Bool USBCDCD_waitForConnect(UInt timeout)
{
    Bool ret = TRUE;
    UInt key;

    /* Need exclusive access to prevent a race condition */
    key = GateMutex_enter(gateUSBWait);

    if (state == USBCDCD_STATE_UNCONFIGURED) {
        if (!Semaphore_pend(semUSBConnected, timeout)) {
            ret = FALSE;
        }
    }

    GateMutex_leave(gateUSBWait, key);

    return (ret);
}
