﻿using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using System.Runtime.InteropServices;
using System.Data;
using System.IO;

namespace Fuzzer
{
    class CfbDataReader
    {
        /// <summary>
        /// This structure mimics the structure SNIFFED_DATA_HEADER from the driver IrpDumper (IrpDumper\PipeComm.h)
        /// </summary>
        [StructLayout(LayoutKind.Sequential, Pack = 1, CharSet = CharSet.Unicode)]
        public unsafe struct CfbMessageHeader
        {
            public ulong TimeStamp;
            public UInt32 Irql;
            public UInt32 IoctlCode;
            public UInt32 ProcessId;
            public UInt32 ThreadId;
            public UInt32 BufferLength;
            public string DriverName;
        }


        /// <summary>
        ///
        /// </summary>
        public struct IRP
        {
            public CfbMessageHeader Header;
            public byte[] Body;
        }


        public DataTable Messages;
        private Task thread;
        private bool doLoop;
        private Form1 RootForm;
        private List<IRP> Irps;


        public bool IsThreadRunning
        {
            get
            {
                return !doLoop;
            }
        }


        /// <summary>
        /// Constructor
        /// </summary>
        public CfbDataReader(Form1 f)
        {
            Messages = new DataTable("IrpData");
            Messages.Columns.Add("TimeStamp", typeof(DateTime));
            Messages.Columns.Add("Irql", typeof(string));
            Messages.Columns.Add("IoctlCode", typeof(string));
            Messages.Columns.Add("ProcessId", typeof(UInt32));
            Messages.Columns.Add("ThreadId", typeof(UInt32));
            Messages.Columns.Add("BufferLength", typeof(UInt32));
            Messages.Columns.Add("DriverName", typeof(string));
            Messages.Columns.Add("Buffer", typeof(string));

            RootForm = f;
            doLoop = false;
            Irps = new List<IRP>();
        }


        /// <summary>
        /// Starts a dedicated thread to pop out messages from the named pipe.
        /// </summary>
        public void StartClientThread()
        {
            RootForm.Log("Starting thread...");
            doLoop = true;
            thread = Task.Factory.StartNew(DataReaderThread);
            RootForm.Log("Thread started!");
        }

        /// <summary>
        /// Tries to end cleanly the thread.
        /// </summary>
        public void EndClientThread()
        {
            doLoop = false;

            if (thread == null)
                return;

            RootForm.Log("Ending thread...");

            var success_wait = false;
            Int32 waitFor = 1*1000; // 1 second

            for (int i = 0; i < 5; i++)
            {
                if (!thread.Wait(waitFor))
                {
                    continue;
                }

                success_wait = true;
                break;
            }

            if (!success_wait)
            {
                RootForm.Log("Failed to kill gracefully, forcing thread termination!");
                // TODO : call to TerminateThread
            }

             RootForm.Log("Thread ended!");
        }


        /// <summary>
        /// Read a message from the CFB driver. This function converts the raw bytes into a proper structure.
        /// </summary>
        /// <returns>An IRP struct object for the header and an array of byte for the body.</returns>
        private unsafe IRP ReadMessage()
        {
            int HeaderSize;
            int ErrNo;
            bool bResult;
            int dwNumberOfByteRead = 0;

            //
            // Read the raw header
            //
            HeaderSize = Core.GetCfbMessageHeaderSize();
            RootForm.Log($"ReadMessage() - MessageHeaderSize={HeaderSize:d}");


            //
            // Get the exact size of the next message
            //

            //var lpdwNumberOfByteRead = new IntPtr(0);

            while (true)
            {
                bResult = Core.ReadCfbDevice(null, 0, &dwNumberOfByteRead);

                ErrNo = Marshal.GetLastWin32Error();

                if (bResult)
                {
                    if (ErrNo == 0x00)
                    {
                        System.Threading.Thread.Sleep(5000);
                        continue;
                    }

                }

                break;

            }

            if (ErrNo != 0x7A) // ERROR_INSUFFICIENT_BUFFER
            {
                throw new Exception($"ReadMessage() - ReadCfbDevice(HEADER) failed unexpectedly: GetLastError()=0x{ErrNo:x}");
            }



            //dwNumberOfByteRead = (int)Marshal.PtrToStructure(lpdwNumberOfByteRead, typeof(int));
            //dwNumberOfByteRead = Marshal.ReadInt32(lpdwNumberOfByteRead);
            RootForm.Log($"ReadMessage() - ReadCfbDevice(HEADER) read {dwNumberOfByteRead:d} Bytes");


            if (dwNumberOfByteRead < HeaderSize)
            {
                throw new Exception($"ReadMessage() - announced size of {dwNumberOfByteRead:x} B is too small");
            }


            //
            // Get the whole thing
            //

            //var RawMessage = Marshal.AllocHGlobal(dwNumberOfByteRead);
            byte[] RawMessage = new byte[dwNumberOfByteRead];

            fixed (byte* DstBuf = RawMessage)
            {
                bResult = Core.ReadCfbDevice(DstBuf, dwNumberOfByteRead, null);

                if (bResult)
                {
                    ErrNo = Marshal.GetLastWin32Error();
                    throw new Exception($"ReadMessage() - ReadCfbDevice(BODY) failed unexpectedly with 0x{ErrNo:x}");
                }
            }


            //
            // And convert it to managed code
            //

            //CfbMessageHeader Header = (CfbMessageHeader)Marshal.PtrToStructure(RawMessage, typeof(CfbMessageHeader));
            Stream stream = new MemoryStream(RawMessage);
            BinaryReader br = new BinaryReader(stream);

            CfbMessageHeader Header = new CfbMessageHeader
            {
                TimeStamp = br.ReadUInt64(),
                Irql = br.ReadUInt32(),
                IoctlCode = br.ReadUInt32(),
                ProcessId = br.ReadUInt32(),
                ThreadId = br.ReadUInt32(),
                BufferLength = br.ReadUInt32(),
                DriverName = br.ReadString()
            };



            RootForm.Log($"Read Ioctl {Header.IoctlCode:x} from PID {Header.ProcessId:d} (TID={Header.ThreadId:d}), {Header.BufferLength:d} bytes of data");

            byte[] Body = new byte[Header.BufferLength];

            //Marshal.Copy(RawMessage, Body, HeaderSize, Convert.ToInt32(Header.BufferLength));


            //Marshal.FreeHGlobal(lpdwNumberOfByteRead);
            //Marshal.FreeHGlobal(RawMessage);



            //
            // Prepare and return the IRP object
            //
            IRP irp = new IRP
            {
                Header = Header,
                Body = Body
            };


            return irp;
        }


        /// <summary>
        /// Threaded function that'll open a handle to named pipe, and pop out structured messages
        /// </summary>
        private void DataReaderThread()
        {

            try
            {
                while (doLoop)
                {
                    var irp = ReadMessage();
                    Irps.Add(irp);

                    RootForm.Log("New IRP received");


                    var Header = irp.Header;
                    string BodyString = "";


                    foreach (byte b in irp.Body)
                    {
                        BodyString += $"{b:x2} ";
                    }

                    Messages.Rows.Add(
                        DateTime.FromFileTime((long)Header.TimeStamp),
                        "0x" + Header.Irql.ToString("x"),
                        "0x" + Header.IoctlCode.ToString("x"),
                        Header.ProcessId,
                        Header.ThreadId,
                        Header.BufferLength,
                        "<drivername here>",
                        BodyString
                        );

                    doLoop = false;
                }

            }
            catch (Exception Ex)
            {
                RootForm.Log(Ex.Message + "\n" + Ex.StackTrace);
            }

        }
    }
}
