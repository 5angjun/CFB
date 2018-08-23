#include "PipeComm.h"

/*++

Reference:
https://www.codeproject.com/Articles/9504/Driver-Development-Part-1-Introduction-to-Drivers
https://www.codeproject.com/Articles/8651/A-simple-demo-for-WDM-Driver-development

--*/
NTSTATUS GetDataFromIrp(IN PIRP Irp, IN PIO_STACK_LOCATION Stack, IN PVOID *Buffer2)
{
	NTSTATUS Status = STATUS_SUCCESS;

	*Buffer2 = NULL;

	ULONG InputBufferLen = Stack->Parameters.DeviceIoControl.InputBufferLength;
	PVOID Buffer = ExAllocatePoolWithTag(NonPagedPool, InputBufferLen, CFB_DEVICE_TAG);
	if (!Buffer)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	__try
	{
		Status = STATUS_SUCCESS;

		do
		{
			ULONG IoctlCode = Stack->Parameters.DeviceIoControl.IoControlCode;
			if (METHOD_FROM_CTL_CODE(IoctlCode) == METHOD_NEITHER)
			{
				CfbDbgPrintInfo(L"GetDataFromIrp() - Using METHOD_NEITHER for IoctlCode=%#x\n", IoctlCode);
				if (Stack->Parameters.DeviceIoControl.Type3InputBuffer >= (PVOID)(1 << 16))
				{
					RtlCopyMemory(Buffer, Stack->Parameters.DeviceIoControl.Type3InputBuffer, InputBufferLen);
				}
				else
				{
					Status = STATUS_UNSUCCESSFUL;
				}

				break;
			}

			if (METHOD_FROM_CTL_CODE(IoctlCode) == METHOD_BUFFERED)
			{
				CfbDbgPrintInfo(L"GetDataFromIrp() - Using METHOD_BUFFERED for IoctlCode=%#x\n", IoctlCode);
				if (!Irp->AssociatedIrp.SystemBuffer)
				{
					Status = STATUS_INVALID_PARAMETER;
				}
				else
				{
					RtlCopyMemory(Buffer, Irp->AssociatedIrp.SystemBuffer, InputBufferLen);
				}

				break;
			}

			CfbDbgPrintInfo(L"GetDataFromIrp() - Using METHOD_IN_DIRECT for IoctlCode=%#x\n", IoctlCode);
			if (!Irp->MdlAddress)
			{
				Status = STATUS_INVALID_PARAMETER;
				break;
			}

			PVOID pDataAddr = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
			if (!pDataAddr)
			{
				Status = STATUS_INVALID_PARAMETER;
				break;
			}
			else
			{
				RtlCopyMemory(Buffer, pDataAddr, InputBufferLen);
			}

		} while (0);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		Status = GetExceptionCode();
		CfbDbgPrintErr(L"GetDataFromIrp() - Exception Code: 0x%X\n", Status);
	}

	CfbDbgPrintInfo(L"GetDataFromIrp() - Return status = %#x\n", Status);

	if (!NT_SUCCESS(Status))
	{
		CfbDbgPrintErr(L"GetDataFromIrp() - Freeing %p\n", Buffer);
		ExFreePoolWithTag(Buffer, CFB_DEVICE_TAG);
	}
	else
	{
		*Buffer2 = Buffer;
	}

	return Status;
}


/*++


--*/
NTSTATUS PreparePipeMessage(IN UINT32 Pid, IN UINT32 Tid, IN UINT32 IoctlCode, IN PVOID pBody, IN ULONG BodyLen, OUT PSNIFFED_DATA *pMessage)
{
	NTSTATUS Status = STATUS_INSUFFICIENT_RESOURCES;

	*pMessage = (PSNIFFED_DATA)ExAllocatePoolWithTag( NonPagedPool, sizeof( SNIFFED_DATA ), CFB_DEVICE_TAG );
	if ( !pMessage )
	{
		return Status;
	}

	PSNIFFED_DATA_HEADER pMsgHeader = (PSNIFFED_DATA_HEADER)ExAllocatePoolWithTag( NonPagedPool, sizeof( SNIFFED_DATA_HEADER ), CFB_DEVICE_TAG );
	if ( !pMsgHeader )
	{
		return Status;
	}


	//
	// create and fill the header structure
	//
	CfbDbgPrintInfo(L"Filling header %p\n", pMsgHeader);

	KeQuerySystemTime( &pMsgHeader->TimeStamp );
	pMsgHeader->Pid = Pid;
	pMsgHeader->Tid = Tid;
	pMsgHeader->Irql = KeGetCurrentIrql();
	pMsgHeader->BufferLength = BodyLen;
	pMsgHeader->IoctlCode = IoctlCode;
	// todo fill drivername

	//
	// fill up the message structure
	//
	CfbDbgPrintInfo( L"Filling message %p\n", *pMessage);

	(*pMessage)->Header = pMsgHeader;
	(*pMessage)->Body = pBody;

	Status = STATUS_SUCCESS;

	return Status;
}


/*++

--*/
VOID FreePipeMessage(PSNIFFED_DATA pMessage)
{
	ExFreePoolWithTag(pMessage->Header, CFB_DEVICE_TAG);
	ExFreePoolWithTag(pMessage->Body, CFB_DEVICE_TAG);
	ExFreePoolWithTag(pMessage, CFB_DEVICE_TAG);
	pMessage = NULL;
	return;
}


/*++

HandleInterceptedIrp is called every time we intercept an IRP. It will extract the data
from the IRP packet (depending on its method), and build a SNIFFED_DATA packet that will
written back to the userland client.

--*/
NTSTATUS HandleInterceptedIrp(IN PIRP Irp, IN PIO_STACK_LOCATION Stack)
{

	NTSTATUS Status = STATUS_UNSUCCESSFUL;
	UINT32 Pid, Tid = 0;
	PVOID IrpExtractedData;
	ULONG IrpExtractedDataLength = 0;
	UINT32 IoctlCode = 0;
	PSNIFFED_DATA pMessage = NULL;


	Status = GetDataFromIrp(Irp, Stack, &IrpExtractedData);

	if (!NT_SUCCESS(Status) || IrpExtractedData == NULL)
	{
		CfbDbgPrintErr(L"GetDataFromIrp() failed, Status=%#X\n", Status);
		return Status;
	}


	IrpExtractedDataLength = Stack->Parameters.DeviceIoControl.InputBufferLength;

	CfbDbgPrintInfo(L"Extracted IRP data (%p, %d B), dumping...\n", IrpExtractedData, IrpExtractedDataLength);

	if(IrpExtractedDataLength)
	{
		CfbHexDump( IrpExtractedData, IrpExtractedDataLength > 0x40 ? 0x40 : IrpExtractedDataLength );
	}
	

	IoctlCode = Stack->Parameters.DeviceIoControl.IoControlCode;
	Pid = (UINT32)((ULONG_PTR)PsGetProcessId( PsGetCurrentProcess() ) & 0xffffffff);
	Tid = (UINT32)((ULONG_PTR)PsGetCurrentThreadId() & 0xffffffff);

	Status = PreparePipeMessage(Pid, Tid, IoctlCode, IrpExtractedData, IrpExtractedDataLength, &pMessage);

	if (!NT_SUCCESS(Status) || pMessage == NULL)
	{
		CfbDbgPrintErr(L"PreparePipeMessage() failed, Status=%#X\n", Status);
		ExFreePoolWithTag(IrpExtractedData, CFB_DEVICE_TAG);
		return Status;
	}

	CfbDbgPrintOk(
		L"Prepared new pipe message %p (Pid=%llu, Ioctl=0x%#llx, Size=%lu)\n",
		pMessage,
		pMessage->Header->Pid,
		pMessage->Header->IoctlCode,
		pMessage->Header->BufferLength
	);

	UINT32 dwIndex;
	Status = PushToQueue( pMessage, &dwIndex );

	if ( !NT_SUCCESS( Status ) )
	{
		CfbDbgPrintErr( L"PushToQueue() failed, discarding message\n" );
		FreePipeMessage( pMessage );
	}
	else
	{
		CfbDbgPrintOk( L"Success! Message #%d pushed to queue...\n", dwIndex );
	}

	return Status;
}