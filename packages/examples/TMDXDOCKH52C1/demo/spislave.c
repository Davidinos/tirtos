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
 *  ======== spislave.c ========
 */

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/IHeap.h>
#include <xdc/runtime/System.h>

/* IPC Header files */
#include <ti/ipc/MessageQ.h>
#include <ti/ipc/MultiProc.h>
#include <ti/tirtos/ipc/SPIMessageQTransport.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/heaps/HeapBuf.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/hal/Hwi.h>

/* TI-RTOS Header files */
#include <ti/drivers/GPIO.h>

/* Example/Board Header files */
#include "TMDXDOCKH52C1.h"
#include "spiDemo.h"

/*
 *  Make sure this buffer in in ".dma". It is used for
 *  MessageQ msgs. The SPI uses DMA to send the data. This
 *  requires that the buffer be in a location that can be
 *  DMA'd.
 */
#pragma DATA_SECTION(buf, ".dma");
Char buf[NUMBLOCKS * BLOCKSIZE];
MessageQ_Handle msgq;

UInt totalCount = 0;

/*
 *  ======== recvFxn ========
 */
Void recvFxn(UArg arg0, UArg arg1)
{
    MessageQ_Msg   msg;
    Int status;

    Task_sleep(10);

    System_printf("Task receiver: Starting\n");

    /* Loop forever receiving messages */
    while (1) {
        status = MessageQ_get(msgq, &msg, MessageQ_FOREVER);
        if (status != MessageQ_S_SUCCESS) {
           System_abort("failed to get\n");
        }

        if (totalCount % 250 == 0) {
            GPIO_toggle(TMDXDOCKH52C1_LD2);
        }
        totalCount++;
        MessageQ_free(msg);
    }
}

/*
 *  ======== main ========
 */
Int main(Void)
{
    Error_Block    eb;
    HeapBuf_Handle heapHandle;
    HeapBuf_Params hbparams;
    Task_Params taskParams;
    SPIMessageQTransport_Params transportParams;
    SPIMessageQTransport_Object *handle;
    MessageQ_Params2 msgqParams;

    /* Call board init functions. */
    TMDXDOCKH52C1_initGeneral();
    TMDXDOCKH52C1_initGPIO();
    TMDXDOCKH52C1_initDMA();
    TMDXDOCKH52C1_initSPI();

    System_printf("MultiProc_self = %d\n", MultiProc_self());

    /* Turn on user LED */
    GPIO_write(TMDXDOCKH52C1_LD2, TMDXDOCKH52C1_LED_ON);

    /* Create the Heap that will be used for MessageQ */
    Error_init(&eb);
    HeapBuf_Params_init(&hbparams);
    hbparams.align          = 0;
    hbparams.numBlocks      = NUMBLOCKS;
    hbparams.blockSize      = BLOCKSIZE;
    hbparams.bufSize        = NUMBLOCKS * BLOCKSIZE;
    hbparams.buf            = buf;
    heapHandle = HeapBuf_create(&hbparams, &eb);
    if (heapHandle == NULL) {
        System_abort("HeapBuf_create failed\n" );
    }

    /* Register default system heap with MessageQ */
    MessageQ_registerHeap((IHeap_Handle)(heapHandle), HEAPID);

    /* Create the transport to the master M3 */
    SPIMessageQTransport_Params_init(&transportParams);
    transportParams.maxMsgSize = BLOCKSIZE;
    transportParams.heap = (IHeap_Handle)(heapHandle);
    transportParams.spiIndex = TMDXDOCKH52C1_SPI0;
    transportParams.clockRate = 1;
    transportParams.spiBitRate = 6000000;
    transportParams.master = FALSE;
    transportParams.priority = SPIMessageQTransport_Priority_NORMAL;
    handle = SPIMessageQTransport_create(MASTERM3PROCID, &transportParams, &eb);
    if (handle == NULL) {
        System_abort("SPIMessageQTransport_create failed\n" );
    }

    MessageQ_Params2_init(&msgqParams);
    msgqParams.queueIndex = SLAVEM3QUEUEINDEX;
    msgq = MessageQ_create2(NULL, &msgqParams);
    System_printf("Created Slave M3 queue\n");

    Task_Params_init(&taskParams);
    taskParams.priority = 2;
    taskParams.stackSize = 768;
    Task_create(recvFxn, &taskParams, &eb);
    if (Error_check(&eb)) {
        System_printf("Task was not created\n");
        System_abort("Aborting...\n");
    }

    /* Start BIOS. Will not return from this call. */
    BIOS_start();
}
