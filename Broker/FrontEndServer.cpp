#include <WinSock2.h>
#include <ws2tcpip.h>

#include "FrontEndServer.h"

#include <aclapi.h>
#include <psapi.h>
#include <iostream>
#include <locale>
#include <codecvt>

#include "taskmanager.h"
#include "Session.h"


#include "json.hpp"
using json = nlohmann::json;


/*++
Routine Description:

Creates the FrontEndServer object.


Arguments:

	None


Return Value:
	May throw an exception if the the allocation failed.

--*/
FrontEndServer::FrontEndServer()
{
	if (!m_Transport.Initialize())
		RAISE_GENERIC_EXCEPTION("m_Transport.Initialize() failed");
	
}


/*++
Routine Description:

Destroys the FrontEndServer object.


Arguments:

	None


Return Value:
	May throw an exception if the the deallocation failed.

--*/
FrontEndServer::~FrontEndServer() noexcept(false)
{
	if (!m_Transport.Terminate())
	{
		RAISE_GENERIC_EXCEPTION("m_Transport.Terminate() failed");
	}
}



/*++
Routine Description:

Create the named pipe responsible for the communication with the GUI. To do, a Security 
Descriptor is created with Explicit Access set for Everyone (including remote clients),
to Read/Write the pipe.

Therefore, we must be careful about that as any user would be able to send some commands
to the broker pipe (and therefore to the kernel driver). 


Arguments:

	None


Return Value:
	Returns TRUE upon successful creation of the pipe, FALSE if any error occured.

--*/
BOOL PipeTransportManager::CreatePipe()
{
	BOOL fSuccess = FALSE;
	SID_IDENTIFIER_AUTHORITY SidAuthWorld = SECURITY_WORLD_SID_AUTHORITY;
	EXPLICIT_ACCESS ea[1] = { 0 };
	PACL pNewAcl = NULL;
	PSID pEveryoneSid = NULL;
	SECURITY_ATTRIBUTES SecurityAttributes = { 0 };
	SECURITY_DESCRIPTOR SecurityDescriptor = { 0 };

	do
	{
		dbg(L"Defining new SD for pipe\n");

		//
		// For now, SD is set for Everyone to have RW access 
		//

		if (!::AllocateAndInitializeSid(
				&SidAuthWorld,
				1,
				SECURITY_WORLD_RID,
				0, 0, 0, 0, 0, 0, 0,
				&pEveryoneSid
			)
		)
		{
			PrintErrorWithFunctionName(L"AllocateAndInitializeSid");
			fSuccess = FALSE;
			break;
		}


		//
		// Populate the EA entry
		//
		ea[0].grfAccessPermissions = GENERIC_ALL;
		ea[0].grfAccessMode = SET_ACCESS;
		ea[0].grfInheritance = NO_INHERITANCE;
		ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
		ea[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
		ea[0].Trustee.ptstrName = (LPTSTR)pEveryoneSid;


		//
		// Apply the EA to the ACL
		//
		if (::SetEntriesInAcl(1, ea, NULL, &pNewAcl) != ERROR_SUCCESS)
		{
			PrintErrorWithFunctionName(L"SetEntriesInAcl");
			fSuccess = FALSE;
			break;
		}


		//
		// Set the SD to new ACL
		//
		if (!::InitializeSecurityDescriptor(&SecurityDescriptor, SECURITY_DESCRIPTOR_REVISION))
		{
			PrintErrorWithFunctionName(L"InitializeSecurityDescriptor");
			fSuccess = FALSE;
			break;
		}

		if (!::SetSecurityDescriptorDacl(&SecurityDescriptor, TRUE, pNewAcl, FALSE))
		{
			PrintErrorWithFunctionName(L"SetSecurityDescriptorDacl");
			fSuccess = FALSE;
			break;
		}


		//
		// Init the SA
		//
		SecurityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
		SecurityAttributes.lpSecurityDescriptor = &SecurityDescriptor;
		SecurityAttributes.bInheritHandle = FALSE;


#ifdef _DEBUG
		xlog(LOG_DEBUG, L"Creating named pipe '%s'...\n", CFB_PIPE_NAME);
#endif

		//
		// create the overlapped pipe
		//
		HANDLE hServer = ::CreateNamedPipe(
			CFB_PIPE_NAME,
			PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_ACCEPT_REMOTE_CLIENTS | PIPE_WAIT,
			CFB_PIPE_MAXCLIENTS,
			CFB_PIPE_INBUFLEN,
			CFB_PIPE_OUTBUFLEN,
			0,
			&SecurityAttributes
		);

		if (hServer == INVALID_HANDLE_VALUE)
		{
			PrintErrorWithFunctionName(L"CreateNamedPipe()");
			fSuccess = FALSE;
			break;
		}

		m_hServer = hServer;

		m_oOverlap.hEvent = ::CreateEvent(nullptr, FALSE, TRUE, nullptr);
		if (!m_oOverlap.hEvent)
		{
			xlog(LOG_CRITICAL, L"failed to create an event object for frontend thread\n");
			return false;
		}


		//
		// last, connect to the named pipe
		//
		fSuccess = ConnectPipe();
	} 
	while (false);

	if (pEveryoneSid)
		FreeSid(pEveryoneSid);

	if (pNewAcl)
		LocalFree(pNewAcl);


	return fSuccess;
}





/*++

Routine Description:

Flush all the data and close the pipe.


Arguments:

	None


Return Value:
	Returns TRUE upon successful termination of the pipe, FALSE if any error occured.

--*/
BOOL PipeTransportManager::ClosePipe()
{
	dbg(L"Closing NamedPipe '%s'...\n", CFB_PIPE_NAME);


	BOOL fSuccess = TRUE;

	do
	{
		if (m_hServer == INVALID_HANDLE_VALUE)
		{
			//
			// already closed
			//
			break;
		}

		if (m_dwServerState != ServerState::Disconnected)
		{
			//
			// Wait until all data was consumed
			//
			if (!::FlushFileBuffers(m_hServer))
			{
				PrintErrorWithFunctionName(L"FlushFileBuffers()");
				fSuccess = FALSE;
			}

			//
			// Then close down the named pipe
			//
			if (!::DisconnectNamedPipe(m_hServer))
			{
				PrintErrorWithFunctionName(L"DisconnectNamedPipe()");
				fSuccess = FALSE;
			}
		}

		fSuccess = ::CloseHandle(m_hServer);
		m_hServer = INVALID_HANDLE_VALUE;
	} 
	while (false);

	return fSuccess;
}


/*++

Routine Description:


Arguments:


Return Value:

--*/
BOOL PipeTransportManager::DisconnectAndReconnect()
{
	if (!DisconnectNamedPipe(m_hServer))
	{
		PrintErrorWithFunctionName(L"DisconnectNamedPipe");
		return false;
	}

	if (!ConnectPipe())
	{
		xlog(LOG_ERROR, L"error in ConnectPipe()\n");
		return false;
	}

	return true;
}


/*++

Routine Description:


Arguments:


Return Value:
	
	Returns TRUE on success, FALSE otherwise
--*/
BOOL PipeTransportManager::ConnectPipe()
{
	dbg(L"Connecting NamedPipe '%s'...\n", CFB_PIPE_NAME);

	BOOL fSuccess = ::ConnectNamedPipe(m_hServer, &m_oOverlap);
	if (fSuccess)
	{
		PrintErrorWithFunctionName(L"ConnectNamedPipe");
		::DisconnectNamedPipe(m_hServer);
		return FALSE;
	}

	DWORD gle = ::GetLastError();

	switch (gle)
	{
	case ERROR_IO_PENDING:
		m_fPendingIo = TRUE;
		m_dwServerState = ServerState::Connecting;
		break;

	case ERROR_PIPE_CONNECTED:
		m_dwServerState = ServerState::ReadyToReadFromClient;
		::SetEvent(m_oOverlap.hEvent);
		break;

	default:
		m_dwServerState = ServerState::Disconnected;
		xlog(LOG_ERROR, L"ConnectNamedPipe failed with %d.\n", gle);
		fSuccess = false;
		break;
	}

	return TRUE;
}


/*++

Synchronous send for named pipe

--*/
BOOL PipeTransportManager::SendSynchronous(_In_ const std::vector<byte>& data)
{
	if (data.size() >= MAX_ACCEPTABLE_MESSAGE_SIZE)
		return false;

	DWORD dwDataLength = (DWORD)data.size(), dwNbByteWritten;
	BOOL fSuccess = ::WriteFile(m_hServer, data.data(), dwDataLength, &dwNbByteWritten, NULL);
	if (!fSuccess || dwDataLength != dwNbByteWritten)
		return false;

	return true;
}


/*++

Synchronous receive for named pipe

--*/
std::vector<byte> PipeTransportManager::ReceiveSynchronous()
{
	auto buf = std::make_unique<byte[]>(MAX_MESSAGE_SIZE);
	RtlZeroMemory(buf.get(), MAX_MESSAGE_SIZE);

	DWORD dwRequestSize;
	BOOL fSuccess = ::ReadFile(m_hServer, buf.get(), MAX_ACCEPTABLE_MESSAGE_SIZE, &dwRequestSize, NULL);
	if (!fSuccess)
		RAISE_GENERIC_EXCEPTION("ReceiveSynchronous");

	std::vector<byte> res; 
	for (DWORD i = 0; i < dwRequestSize; i++) res.push_back(buf[i]);
	return res;
}




/*++

Routine Description:


Arguments:
	
	Session -


Return Value:

	Returns 0 on success, -1 on failure.

--*/
DWORD SendInterceptedIrps(_In_ Session& Session)
{
	json j = {
		{"header", {
			{"success", true},
			{"gle", ERROR_SUCCESS}
		}
	} };

	//
	// Make sure no element are being added concurrently
	//
	std::unique_lock<std::mutex> mlock(Session.m_IrpMutex);
	size_t i = 0;

	j["body"]["entries"] = json::array();

	while(!Session.m_IrpQueue.empty())
	{
		//
		// pop an IRP
		//
		Irp irp(Session.m_IrpQueue.front());
		Session.m_IrpQueue.pop();

		//
		// format a new JSON entry
		//
		j["body"]["entries"].push_back(irp.AsJson());


		//
		// The IRP is ready to be deleted
		//
		irp.Dispose();

		i++;
	}

	mlock.unlock();

	j["body"]["nb_entries"] = i;


	//
	// Write the data back
	//

	const std::string& str = j.dump();
	const std::vector<byte> raw(str.begin(), str.end());

	if (!Session.FrontEndServer.Send(raw))
	{
		PrintErrorWithFunctionName(L"SendSynchronous(Irps)");
		return ERROR_INVALID_DATA;
	}

	return ERROR_SUCCESS;
}



/*++

Routine Description:

	Sends the list of drivers

Arguments:

	Session -


Return Value:

	Returns 0 on success, -1 on failure.

--*/
DWORD SendDriverList(_In_ Session& Sess)
{
	int i=0;
	
	json j = {
		{"header", {
			{"success", true},
			{"gle", ERROR_SUCCESS}
		}
	}};


	j["body"]["drivers"] = json::array();
	
	// wstring -> string converter
	std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> wide_converter;

	for (auto driver : Utils::EnumerateObjectDirectory(L"\\Driver"))
	{
		std::string driver_name = wide_converter.to_bytes(driver.first);
		j["body"]["drivers"].push_back(driver_name);
		i++;
	}

	const std::string& str = j.dump();
	const std::vector<byte> raw(str.begin(), str.end());

	if (!Sess.FrontEndServer.Send(raw))
	{
		PrintErrorWithFunctionName(L"SendSynchronous(Drivers)");
		return ERROR_INVALID_DATA;
	}

	return ERROR_SUCCESS;
}



/*++

Routine Description:

	The MainLoop function for the NamedPipe connector.

Arguments:

	Sess - the current session


Return Value:

	Returns 0 the thread execution went successfully, the value from GetLastError() otherwise.

--*/
DWORD PipeTransportManager::RunForever(_In_ Session& Sess)
{
	DWORD dwIndexObject, cbRet;
	DWORD retcode = ERROR_SUCCESS;
	HANDLE hResEvent = Sess.ResponseTasks.m_hPushEvent;
	BOOL fSuccess;

	const HANDLE Handles[4] = {
		Sess.m_hTerminationEvent,
		m_oOverlap.hEvent,
		hResEvent,
		m_hServer
	};


	

	while (Sess.IsRunning())
	{
		//
		// Wait for the pipe to be written to, or a termination notification event
		//

		dwIndexObject = ::WaitForMultipleObjects(_countof(Handles), Handles, FALSE, INFINITE) - WAIT_OBJECT_0;

		if (dwIndexObject < 0 || dwIndexObject >= _countof(Handles))
		{
			PrintErrorWithFunctionName(L"WaitForMultipleObjects()");
			xlog(LOG_CRITICAL, L"WaitForMultipleObjects(FrontEnd) has failed, cannot proceed...\n");
			Sess.Stop();
			continue;
		}


		//
		// if we received a termination event, stop everything
		//
		if (dwIndexObject == 0)
		{
#ifdef _DEBUG
			xlog(LOG_DEBUG, L"received termination Event\n");
#endif // _DEBUG
			Sess.Stop();
			continue;
		}
	

		//
		// otherwise, start by checking for pending IOs and update the state if needed
		//

		if (m_dwServerState == ServerState::Connecting)
		{
			LPOVERLAPPED ov = &m_oOverlap;
			fSuccess = ::GetOverlappedResult(m_hServer, ov, &cbRet, FALSE);

			if (!fSuccess)
			{
				//
				// assume the connection has closed
				//
				DisconnectAndReconnect();
				continue;
			}

			dbg(L"new pipe connection\n");

			m_dwServerState = ServerState::ReadyToReadFromClient;

		}


		//
		// process the state itself
		//
		if (m_dwServerState == ServerState::ReadyToReadFromClient)
		{
			try
			{
				//
				// read the json message
				//
				auto RequestBufferRaw = Sess.FrontEndServer.Receive();

#ifdef _DEBUG
				SIZE_T dwRequestSize = RequestBufferRaw.size();
				dbg(L"new pipe message (len=%lu)\n", dwRequestSize);
				hexdump(RequestBufferRaw.data(), dwRequestSize);
#endif // _DEBUG

				const std::string request_str(RequestBufferRaw.begin(), RequestBufferRaw.end());
				auto json_request = json::parse(request_str);
				const TaskType type = static_cast<TaskType>(json_request["body"]["type"]);
				DWORD dwDataLength = json_request["body"]["data_length"];
				auto data = json_request["body"]["data"].get<std::string>();
				auto lpData = Utils::base64_decode(data);

				assert(lpData.size() == dwDataLength);

				//
				// build a Task object from the next message read from the pipe
				//
				Task task(type, lpData.data(), dwDataLength, -1);


				dbg(L"new request task (id=%d, type='%s', length=%d)\n", task.Id(), task.TypeAsString(), task.Length());

				switch (task.Type())
				{
				case TaskType::GetInterceptedIrps:
					SendInterceptedIrps(Sess);
					continue;

				case TaskType::EnumerateDrivers:
					SendDriverList(Sess);
					continue;

				case TaskType::ReplayIrp:
					//
					// Replay the IRP
					//
					// TODO
				default:

					// push the task to request task list
					Sess.RequestTasks.push(task);

					// change the state
					m_dwServerState = ServerState::ReadyToReadFromServer;
				}
			}
			catch (BrokenPipeException&)
			{
				xlog(LOG_WARNING, L"Broken pipe detected...\n");
				DisconnectAndReconnect();
				continue;
			}
			catch (BaseException &e)
			{
				xlog(LOG_ERROR, L"An exception occured while processing incoming message:\n%S\n", e.what());
				DisconnectAndReconnect();
				continue;
			}

		}
		else if (m_dwServerState == ServerState::ReadyToReadFromServer)
		{
			try
			{

				//
				// pop the response task and build the json message
				//
				auto task = Sess.ResponseTasks.pop();

				dbg(L"new response task (id=%d, type='%s', length=%d, gle=%d)\n", task.Id(), task.TypeAsString(), task.Length(), task.ErrCode());

				json json_response = {
					{"header", {
						{"success", task.ErrCode()==ERROR_SUCCESS},
						{"gle", task.ErrCode()},
					}
				} };

				json_response["body"]["data_length"] = task.Length();
				if (task.Length() > 0)
					json_response["body"]["data"] = Utils::base64_encode(task.Data(), task.Length());

				const std::string& str = json_response.dump();
				const std::vector<byte> raw(str.begin(), str.end());
				if (!Sess.FrontEndServer.Send(raw))
				{
					PrintErrorWithFunctionName(L"SendSynchronous(Ioctl)");
				}
				else
				{
					task.SetState(TaskState::Completed);
				}

				dbg(L"task tid=%d sent to frontend, terminating...\n", task.Id());

				// change the state
				m_dwServerState = ServerState::ReadyToReadFromClient;
			}
			catch (BrokenPipeException&)
			{
				xlog(LOG_WARNING, L"Broken pipe detected...\n");
				DisconnectAndReconnect();
				continue;
			}
			catch (BaseException & e)
			{
				xlog(LOG_ERROR, L"An exception occured while processing incoming message:\n%S\n", e.what());
				continue;
			}
		}
		else
		{
			xlog(LOG_WARNING, L"Unexpected state (state=%d, fd=%d), invalid?\n", m_dwServerState, dwIndexObject);
			DisconnectAndReconnect();
		}

	}

	dbg(L"terminating thread TID=%d\n", GetThreadId(GetCurrentThread()));

	return ERROR_SUCCESS;
}



/*++

Routine Description:

This routine handles the communication with the front-end of CFB (for now, the only one implemented
is the GUI).

Once a message from the frontend is received, it is parsed and pushed as an incoming Task, and notify
the BackEnd driver thread, then wait for an event from that same thread, notifying a response. Once the
response is popped from the outgoing Task list, the data is sent back to the frontend.


Arguments:

	lpParameter - the thread parameter


Return Value:

	Returns 0 the thread execution went successfully, the value from GetLastError() otherwise.

--*/
static DWORD ServerThreadRoutine(_In_ LPVOID lpParameter)
{
	Session& Sess = *(reinterpret_cast<Session*>(lpParameter));
	return Sess.FrontEndServer.RunForever(Sess);
}



/*++

Routine Description:

This function is a simple wrapper around CreateThread() to start the thread handling the conversation
with the frontend part of the application.


Arguments:

	lpParameter - a generic pointer to the global Session


Return Value:
	Returns TRUE upon successful creation of the thread, FALSE if any error occured.

--*/
_Success_(return)
BOOL FrontendThreadRoutine(_In_ LPVOID lpParameter)
{
	DWORD dwThreadId;

	HANDLE hThread = ::CreateThread(
		NULL,
		0,
		ServerThreadRoutine,
		lpParameter,
		CREATE_SUSPENDED,
		&dwThreadId
	);

	if (!hThread)
	{
		PrintErrorWithFunctionName(L"CreateThread(hThreadPipeIn)");
		return FALSE;
	}


#ifdef _DEBUG
	xlog(LOG_DEBUG, "Created frontend thread as TID=%d\n", dwThreadId);
#endif

	Session& Sess = *(reinterpret_cast<Session*>(lpParameter));
	Sess.m_hFrontendThread = hThread;

	return TRUE;
}



TcpSocketTransportManager::TcpSocketTransportManager()
	: m_Socket(INVALID_SOCKET)
{
	WSADATA WsaData = { 0, };
	
	if (WSAStartup(MAKEWORD(2, 2), &WsaData))
		RAISE_GENERIC_EXCEPTION("TcpSocketTransportManager::WSAStartup()");

	if (LOBYTE(WsaData.wVersion) != 2 || HIBYTE(WsaData.wVersion) != 2)
		RAISE_GENERIC_EXCEPTION("TcpSocketTransportManager - version check");
}


TcpSocketTransportManager::~TcpSocketTransportManager()
{
	WSACleanup();
}


/*++

Prepare the socket: init + bind + listen

--*/
BOOL TcpSocketTransportManager::Initialize()
{
	WSAPROTOCOL_INFO info = {0, };
	info.dwServiceFlags1 |= XP1_IFS_HANDLES;

	m_Socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_NO_HANDLE_INHERIT);
	if (m_Socket == INVALID_SOCKET)
	{
		xlog(LOG_CRITICAL, L"Cannot create socket (WSAGetLastError=0x%x)\n", ::WSAGetLastError());
		return false;
	}

	return true;
}


/*++

Prepare the socket: bind + listen

--*/
BOOL TcpSocketTransportManager::Connect()
{
	SOCKADDR_IN sa = { 0, };
	InetPtonW(AF_INET, L"0.0.0.0", &sa.sin_addr.s_addr);
	sa.sin_family = AF_INET;
	sa.sin_port = htons(TCP_LISTEN_PORT);

	if (bind(m_Socket, (PSOCKADDR)&sa, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
	{
		xlog(LOG_CRITICAL, L"Cannot bind socket (WSAGetLastError=0x%x)\n", ::WSAGetLastError());
		Terminate();
		return false;
	}

	if (listen(m_Socket, SOMAXCONN_HINT(TCP_MAX_CONNECTIONS)))
	{
		xlog(LOG_CRITICAL, L"Cannot listen socket (WSAGetLastError=0x%x)\n", ::WSAGetLastError());
		Terminate();
		return false;
	}

	return true;
}


BOOL TcpSocketTransportManager::Terminate()
{
	return closesocket(m_Socket) == 0;
}


BOOL TcpSocketTransportManager::Reconnect()
{
	return Terminate() && Initialize() && Connect();
}


/*++

Accept a client socket

--*/
BOOL TcpSocketTransportManager::Accept(_Out_ SOCKET *NewClient)
{
	SOCKET ClientSocket = INVALID_SOCKET;
	SOCKADDR_IN SockInfoClient = { 0 };
	INT dwSockInfoClientSize = sizeof(SOCKADDR_IN);

	ClientSocket = WSAAccept(m_Socket, (SOCKADDR*)&SockInfoClient, &dwSockInfoClientSize, NULL, 0); // todo: add a lpfnCondition
	if (ClientSocket == INVALID_SOCKET)
	{
		xlog(LOG_ERROR, L"Cannot accept from server socket (WSAGetLastError=0x%x)\n", ::WSAGetLastError());
		return false;
	}

	dbg(L"New TCP client %s:%d\n", SockInfoClient.sin_addr.s_addr, ntohs(SockInfoClient.sin_port));
	*NewClient = ClientSocket;
	m_dwServerState = ServerState::Connecting;
	return true;
}


static DWORD HandleTcpRequestsRtn(_In_ LPVOID lpParameter)
{
	Session& Sess = *(reinterpret_cast<Session*>(lpParameter));
	dbg(L"in request handler\n");
	return 0;
}


DWORD TcpSocketTransportManager::RunForever(_In_ Session& CurrentSession)
{
	if (!Initialize())
		return ERROR_INVALID_PARAMETER;

	if (!Connect())
		return ERROR_INVALID_PARAMETER;

	dbg(L"TCP socket ready\n");

	DWORD retcode = ERROR_SUCCESS;
	HANDLE hEvent = INVALID_HANDLE_VALUE;

	std::vector<HANDLE> handles;
	handles.push_back(CurrentSession.m_hTerminationEvent);

	hEvent = ::WSACreateEvent();
	::WSAEventSelect(m_Socket, hEvent, FD_READ | FD_WRITE);
	handles.push_back(hEvent);


	while (CurrentSession.IsRunning())
	{
		DWORD dwIndexObject = ::WSAWaitForMultipleEvents((DWORD)handles.size(), handles.data(), false, INFINITE, false) - WSA_WAIT_EVENT_0;

		if (dwIndexObject < 0 || dwIndexObject >= handles.size())
		{
			PrintErrorWithFunctionName(L"WaitForMultipleObjects()");
			xlog(LOG_CRITICAL, L"WaitForMultipleObjects(FrontEnd) has failed, cannot proceed...\n");
			CurrentSession.Stop();
			continue;
		}

		SOCKET ClientSocket;
		HANDLE hClientThread;
		DWORD dwClientTid;

		switch (dwIndexObject)
		{
		case 0:
			dbg(L"received termination event\n");
			CurrentSession.Stop();
			break;

		case 1:
			// accept the connection
			if (!Accept(&ClientSocket))
				continue;

			// start a thread to handle the requests
			hClientThread = ::CreateThread(nullptr, 0, HandleTcpRequestsRtn, &CurrentSession, 0, &dwClientTid);
			if (!hClientThread)
			{
				PrintErrorWithFunctionName(L"CreateThread");
			}
			else
			{
				dbg(L"event on handle %s\n", dwIndexObject);
				handles.push_back(hClientThread);
			}
			break;

		default:
			dbg(L"event on handle %s\n", dwIndexObject);
			::CloseHandle(handles.at(dwIndexObject));
			break;
		}


	}


	return retcode;
}


/*++

Synchronous send for TCP streams

--*/
BOOL TcpSocketTransportManager::SendSynchronous(_In_ const std::vector<byte>& data)
{
	return false;
}


/*++

Synchronous receive for TCP streams

--*/
std::vector<byte> TcpSocketTransportManager::ReceiveSynchronous()
{
	std::vector<byte> res;
	return res;
}