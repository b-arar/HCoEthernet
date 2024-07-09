/*
 * custom_routine.c
 *
 *  Created on: May 28, 2023
 *      Author: barar
 */
#include "xemacps_example.h"
#include "xil_io.h"

#define SYNC_ETHTYPE 42111
#define DATA_ETHTYPE 42112

struct ethernet_frame {
    char dest_addr[6];
    char src_addr[6];
    char eth_type[2];
    char payload[1500];
};

struct data_frame {
    char dest_addr[6];
    char src_addr[6];
    char eth_type[2];
    short order;
    char payload[1024];
};

struct sync_frame {
    char dest_addr[6];
    char src_addr[6];
    char eth_type[2];
    short state;
    int received_frames[32];
};

char device_address[6] = { 0x00, 0x0a, 0x35, 0x01, 0x02, 0x03 };
char host_address[6] = { 0x00, 0x0a, 0x35, 0x01, 0x02, 0x03 };

struct ethernet_frame TxBuffer[1024];
struct ethernet_frame RxBuffer[2048];


void fill_data_frame(struct data_frame * frame, int number) {
	for (int i = 0; i < 6; i++) {
		frame->src_addr[i] = device_address[i];
		frame->dest_addr[i] = host_address[i];
	}
	short * type = (short *) &(frame->eth_type);
	Xil_Out16BE((UINTPTR) type, DATA_ETHTYPE);
	frame->order = number;
	for (int i = 0; i < 1024; i++) {
		frame->payload[i] = i;
	}
}

void fill_sync_frame(struct sync_frame * frame, int number) {
	for (int i = 0; i < 6; i++) {
		frame->src_addr[i] = device_address[i];
		frame->dest_addr[i] = host_address[i];
	}
	short * type = (short *) &(frame->eth_type);
	Xil_Out16BE((UINTPTR) type, SYNC_ETHTYPE);
	frame->state = number;
	for (int i = 0; i < 32; i++) {
		frame->received_frames[i] = i;
	}
}

LONG customRoutine(XEmacPs *EmacPsInstancePtr, int GemVersion, int * TxFramesPtr, int * RxFramesPtr)
{
	LONG Status;
	u32 NumRxBuf = 0;
	u32 RxFrLen;
	XEmacPs_Bd *Bd1Ptr;
	XEmacPs_Bd *BdRxPtr;
	XEmacPs_BdRing* RxRingPtr = &(EmacPsInstancePtr->RxBdRing);
	XEmacPs_BdRing* TxRingPtr = &(EmacPsInstancePtr->TxBdRing);

	struct data_frame* test_data = (struct data_frame*) &TxBuffer[0];
	struct data_frame* test_data_recv = (struct data_frame*) &RxBuffer[0];

	fill_data_frame(test_data, 22);


	/* FLUSH FRAMES */

	if (EmacPsInstancePtr->Config.IsCacheCoherent == 0) {
		Xil_DCacheFlushRange((UINTPTR) test_data, sizeof(struct data_frame));
	}

	if (EmacPsInstancePtr->Config.IsCacheCoherent == 0) {
		Xil_DCacheFlushRange((UINTPTR) test_data_recv, sizeof(struct data_frame));
	}

	/* SETUP ID FILTERING */

	//XEmacPs_SetTypeIdCheck(EmacPsInstancePtr, DATA_ETHTYPE, 0);
	//XEmacPs_SetTypeIdCheck(EmacPsInstancePtr, SYNC_ETHTYPE, 1);

	/* ALLOCATE THE BDs WE WILL USE IN RXBD RING */

	Status = XEmacPs_BdRingAlloc(&(XEmacPs_GetRxRing(EmacPsInstancePtr)), 2048, &BdRxPtr);
	if (Status != XST_SUCCESS) {
		EmacPsUtilErrorTrap("Error allocating RxBD");
		return XST_FAILURE;
	}


	/*
	 * Setup the RxBD.
	 */

	XEmacPs_BdSetAddressRx(BdRxPtr, (UINTPTR) test_data_recv);
	XEmacPs_Bd* next_rx = XEmacPs_BdRingNext(RxRingPtr, BdRxPtr);
	XEmacPs_BdSetAddressRx(next_rx, (UINTPTR) &RxBuffer[1]);

	printf("Bd content = %x %x\n", (*BdRxPtr)[0], (*BdRxPtr)[1]);
	printf("Bd content = %x %x\n", (*next_rx)[0], (*next_rx)[1]);



	/*
	 * Enqueue to HW
	 */
	Status = XEmacPs_BdRingToHw(&(XEmacPs_GetRxRing(EmacPsInstancePtr)),
				    1, BdRxPtr);
	if (Status != XST_SUCCESS) {
		EmacPsUtilErrorTrap("Error committing RxBD to HW");
		return XST_FAILURE;
	}
	/*
	 * Though the max BD size is 16 bytes for extended desc mode, performing
	 * cache flush for 64 bytes. which is equal to the cache line size.
	 */
	if (GemVersion > 2)
	{
		if (EmacPsInstancePtr->Config.IsCacheCoherent == 0) {
			Xil_DCacheFlushRange((UINTPTR)BdRxPtr, 64);
		}
	}
	/*
	 * Allocate, setup, and enqueue 1 TxBDs. The first BD will
	 * describe the first 32 bytes of TxFrame and the rest of BDs
	 * will describe the rest of the frame.
	 *
	 * The function below will allocate 1 adjacent BDs with Bd1Ptr
	 * being set as the lead BD.
	 */
	Status = XEmacPs_BdRingAlloc(&(XEmacPs_GetTxRing(EmacPsInstancePtr)),
				      1024, &Bd1Ptr);
	if (Status != XST_SUCCESS) {
		EmacPsUtilErrorTrap("Error allocating TxBD");
		return XST_FAILURE;
	}

/*
	if (1) {
		print("HELLO");
		printf("Bd content = %x%x\n%x%x\n", Bd1Ptr[0], Bd1Ptr[1], Bd1Ptr[2], Bd1Ptr[3]);
		return 0;
	}*/

	/*
	 * Setup first TxBD
	 */
	XEmacPs_BdSetAddressTx(Bd1Ptr, (UINTPTR) test_data);
	XEmacPs_BdSetLength(Bd1Ptr, sizeof(struct data_frame));
	XEmacPs_BdClearTxUsed(Bd1Ptr);
	XEmacPs_BdSetLast(Bd1Ptr);

	/*
	 * Enqueue to HW
	 */
	Status = XEmacPs_BdRingToHw(&(XEmacPs_GetTxRing(EmacPsInstancePtr)),
				     1, Bd1Ptr);
	if (Status != XST_SUCCESS) {
		EmacPsUtilErrorTrap("Error committing TxBD to HW");
		return XST_FAILURE;
	}
	if (EmacPsInstancePtr->Config.IsCacheCoherent == 0) {
		Xil_DCacheFlushRange((UINTPTR)Bd1Ptr, 64);
	}
	/*
	 * Set the Queue pointers
	 */
	XEmacPs_SetQueuePtr(EmacPsInstancePtr, EmacPsInstancePtr->RxBdRing.BaseBdAddr, 0, XEMACPS_RECV);
	if (GemVersion > 2) {
	XEmacPs_SetQueuePtr(EmacPsInstancePtr, EmacPsInstancePtr->TxBdRing.BaseBdAddr, 1, XEMACPS_SEND);
	}else {
		XEmacPs_SetQueuePtr(EmacPsInstancePtr, EmacPsInstancePtr->TxBdRing.BaseBdAddr, 0, XEMACPS_SEND);
	}

	/*
	 * Start the device
	 */
	XEmacPs_Start(EmacPsInstancePtr);

	/* Start transmit */
	XEmacPs_Transmit(EmacPsInstancePtr);

	/*
	 * Wait for transmission to complete
	 */
	while (!(*TxFramesPtr));

	/*
	 * Now that the frame has been sent, post process our TxBDs.
	 * Since we have only submitted 1 to hardware, then there should
	 * be only 1 ready for post processing.
	 */
	if (XEmacPs_BdRingFromHwTx(&(XEmacPs_GetTxRing(EmacPsInstancePtr)),
				    1, &Bd1Ptr) == 0) {
		EmacPsUtilErrorTrap
			("TxBDs were not ready for post processing");
		return XST_FAILURE;
	}

	/*
	 * Examine the TxBDs.
	 *
	 * There isn't much to do. The only thing to check would be DMA
	 * exception bits. But this would also be caught in the error
	 * handler. So we just return these BDs to the free list.
	 */


	Status = XEmacPs_BdRingFree(&(XEmacPs_GetTxRing(EmacPsInstancePtr)),
				     1, Bd1Ptr);

	if (Status != XST_SUCCESS) {
		EmacPsUtilErrorTrap("Error freeing up TxBDs");
		return XST_FAILURE;
	}

	/*
	 * Wait for Rx indication
	 */
	while (!(*RxFramesPtr));

	/*
	 * Now that the frame has been received, post process our RxBD.
	 * Since we have submitted to hardware, then there should be only 1
	 * ready for post processing.
	 */
	NumRxBuf = XEmacPs_BdRingFromHwRx(&(XEmacPs_GetRxRing
					  (EmacPsInstancePtr)), 1,
					 &BdRxPtr);
	if (0 == NumRxBuf) {
		EmacPsUtilErrorTrap("RxBD was not ready for post processing");
		return XST_FAILURE;
	}

	/*
	 * There is no device status to check. If there was a DMA error,
	 * it should have been reported to the error handler. Check the
	 * receive lengthi against the transmitted length, then verify
	 * the data.
	 */
	if (GemVersion > 2) {
		/* API to get correct RX frame size - jumbo or otherwise */
		RxFrLen = XEmacPs_GetRxFrameSize(EmacPsInstancePtr, BdRxPtr);
	} else {
		RxFrLen = XEmacPs_BdGetLength(BdRxPtr);
	}

	printf("Bd content = %x %x\n", (*BdRxPtr)[0], (*BdRxPtr)[1]);
	printf("Bd content = %x %x\n", (*next_rx)[0], (*next_rx)[1]);

	if (RxFrLen != sizeof(struct data_frame)) {
		EmacPsUtilErrorTrap("Length mismatch");
		return XST_FAILURE;
	}

	if (1) {
		EmacPsUtilErrorTrap("Data mismatch");
		return XST_FAILURE;
	}

	/*
	 * Return the RxBD back to the channel for later allocation. Free
	 * the exact number we just post processed.
	 */
	Status = XEmacPs_BdRingFree(&(XEmacPs_GetRxRing(EmacPsInstancePtr)),
				     NumRxBuf, BdRxPtr);
	if (Status != XST_SUCCESS) {
		EmacPsUtilErrorTrap("Error freeing up RxBDs");
		return XST_FAILURE;
	}

	/*
	 * Finished this example. If everything worked correctly, all TxBDs
	 * and RxBDs should be free for allocation. Stop the device.
	 */
	XEmacPs_Stop(EmacPsInstancePtr);

	return XST_SUCCESS;
}


/*
 *
 * STATE MACHINE FOR HCoE PROTOCOL:
 * 1 - IDLE : if receive frame on sync eth type type, move to state 2
 * 2 - HANDSHAKE: Send ACK frame, expect ACK frame in return to establish a connection
 * 3 - RECEIVE DATA: Receive all the data frames with data eth type from source. If you rec
 * -eive sync frame and not all data frames are received, request the ones that are missing.
 * Repeat until all data is received.
 * 4- WORKING: Use accelerator to perfrom the computation necessary, set up Tx buffer for sending.
 * 5- SEND DATA: Send the resulting data frame by frame. If data is missing, send those frames again.
 * 6- IDLE
 *
 *
 */
