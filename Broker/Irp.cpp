#include "Irp.h"


Irp::Irp(_In_ PINTERCEPTED_IRP_HEADER Header, _In_ PINTERCEPTED_IRP_BODY InputBuffer, _In_ PINTERCEPTED_IRP_BODY OutputBuffer)
{
	//dbg(L"new irp(header=%p, input=%p len=%d, output=%p len=%d)=%p\n", Header, InputBuffer, Header->InputBufferLength, OutputBuffer, Header->OutputBufferLength, this);

	PINTERCEPTED_IRP_HEADER hdr = &m_Header;
	::memcpy(hdr, Header, sizeof(INTERCEPTED_IRP_HEADER));

	m_InputBuffer.reserve(Header->InputBufferLength);
	::memcpy(m_InputBuffer.data(), InputBuffer, Header->InputBufferLength);

	m_OutputBuffer.reserve(Header->OutputBufferLength);
	::memcpy(m_OutputBuffer.data(), OutputBuffer, Header->OutputBufferLength);
}

Irp::Irp(const Irp& IrpOriginal) 
	:
	m_InputBuffer(IrpOriginal.m_InputBuffer),
	m_OutputBuffer(IrpOriginal.m_OutputBuffer)
{
	PINTERCEPTED_IRP_HEADER hdr = &m_Header;
	::memcpy(hdr, &IrpOriginal.m_Header, sizeof(INTERCEPTED_IRP_HEADER));
	//dbg(L"copy irp(%p -> %p)\n", &IrpOriginal, this);
}


Irp::~Irp()
{
	//dbg(L"del irp(%p)\n", this);
}


json Irp::IrpHeaderToJson()
{
	json header;
	header["TimeStamp"] = m_Header.TimeStamp.QuadPart;
	header["Irql"] = m_Header.Irql;
	header["Type"] = m_Header.Type;
	header["IoctlCode"] = m_Header.IoctlCode;
	header["Pid"] = m_Header.Pid;
	header["Tid"] = m_Header.Tid;
	header["Status"] = m_Header.Status;
	header["InputBufferLength"] = m_Header.InputBufferLength;
	header["OutputBufferLength"] = m_Header.OutputBufferLength;
	header["DriverName"] = std::wstring(m_Header.DriverName);
	header["DeviceName"] = std::wstring(m_Header.DeviceName);
	header["ProcessName"] = std::wstring(m_Header.ProcessName);
	return header;
}


json Irp::InputBufferToJson()
{
	return json::parse(m_InputBuffer.begin(), m_InputBuffer.end());
}


json Irp::OutputBufferToJson()
{
	return json::parse(m_OutputBuffer.begin(), m_OutputBuffer.end());
}


json Irp::ToJson()
{
	json irp;
	irp["header"] = IrpHeaderToJson();
	irp["body"]["input"] = InputBufferToJson();
	irp["body"]["output"] = OutputBufferToJson();
	return irp;
}

