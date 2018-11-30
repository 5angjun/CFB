﻿using System;
using System.Collections.Generic;
using System.Threading;
using System.Runtime.InteropServices;
using System.Data;
using System.Windows.Forms;
using System.Collections.Concurrent;
using System.ComponentModel;

namespace Fuzzer
{
    public class IrpDataReader
    {
        
        private Thread FetchIrpsFromDriverThread, UpdateDisplayThread;
        private BlockingCollection<Irp> NewItems;
        private bool CollectIrp;
        private bool UpdateIrpDataView;
        private IrpMonitorForm RootForm;
        public List<Irp> Irps;
        private ManualResetEvent NewMessageEvent;
        private readonly IntPtr NewMessageEventHandler;
        public DataTable IrpDataTable;
        public BindingSource DataBinder;

        public bool AreThreadsRunning
        {
            get
            {
                return UpdateDisplayThread.IsAlive || FetchIrpsFromDriverThread.IsAlive;
            }
        }


        /// <summary>
        /// Constructor
        /// </summary>
        public IrpDataReader(IrpMonitorForm f)
        {
            RootForm = f;
            CollectIrp = false;
            UpdateIrpDataView = false;

            Irps = new List<Irp>();
            NewItems = new BlockingCollection<Irp>();
            NewMessageEvent = new ManualResetEvent(false);
            DataBinder = new BindingSource();

            InitializeIrpDataTable();
            ResetDataBinder();

            NewMessageEventHandler = NewMessageEvent.SafeWaitHandle.DangerousGetHandle();
        }


        private void InitializeIrpDataTable()
        {
            IrpDataTable = new DataTable("IrpData");
            IrpDataTable.Columns.Add("TimeStamp", typeof(DateTime));
            IrpDataTable.Columns.Add("IrqLevel", typeof(string));
            IrpDataTable.Columns.Add("Type", typeof(string));
            IrpDataTable.Columns.Add("IoctlCode", typeof(string));
            IrpDataTable.Columns.Add("ProcessId", typeof(UInt32));
            IrpDataTable.Columns.Add("ProcessName", typeof(string));
            IrpDataTable.Columns.Add("ThreadId", typeof(UInt32));
            IrpDataTable.Columns.Add("InputBufferLength", typeof(UInt32));
            IrpDataTable.Columns.Add("OutputBufferLength", typeof(UInt32));
            IrpDataTable.Columns.Add("DriverName", typeof(string));
            IrpDataTable.Columns.Add("DeviceName", typeof(string));
            IrpDataTable.Columns.Add("Buffer", typeof(string));
        }


        public void ResetDataBinder()
        {
            RootForm.IrpDataView.DataSource = null;
            DataBinder.DataSource = null;
            RootForm.IrpDataView.DataSource = DataBinder;
            DataBinder.DataSource = IrpDataTable;
            IrpDataTable.Clear();
            RootForm.IrpDataView.Refresh();
            DataBinder.ResetBindings(false);
            Irps.Clear();
        }


        /// <summary>
        /// Starts a dedicated thread to pop out messages from the named pipe.
        /// </summary>
        public void StartThreads()
        {
            //
            // Pass the handler to the C# event to our driver
            // src: http://www.yoda.arachsys.com/csharp/threads/waithandles.shtml
            // 
            
            RootForm.Log($"Sending {NewMessageEventHandler:x} to driver...");
            
            if (Core.SetEventNotificationHandle(NewMessageEventHandler) == false)
            {
                int ErrNo = Marshal.GetLastWin32Error();
                RootForm.Log($"Failed to pass the event handle to the driver, cannot pursue: GetLastError()=0x{ErrNo:x}");
                return;
            }

                       
            UpdateDisplayThread = new Thread(RefreshDataGridViewThreadRoutine)
            {
                Name = "UpdateDisplayThread",
                Priority = ThreadPriority.Normal,
                IsBackground = true
            };


            FetchIrpsFromDriverThread = new Thread(PopIrpsFromDriverThreadRoutine)
            {
                Name = "FetchIrpsFromDriverThread",
                Priority = ThreadPriority.Normal,
                IsBackground = true
            };


            CollectIrp = true;
            UpdateIrpDataView = true;

            UpdateDisplayThread.Start();
            FetchIrpsFromDriverThread.Start();

            RootForm.Log("Threads started!");
        }


        /// <summary>
        /// Tries to end cleanly all the threads.
        /// </summary>
        public void JoinThreads()
        {
            CollectIrp = false;
            JoinThread(FetchIrpsFromDriverThread);

            UpdateIrpDataView = false;
            JoinThread(UpdateDisplayThread);
        }


        /// <summary>
        /// 
        /// </summary>
        /// <param name="t"></param>
        private void JoinThread(Thread t)
        {

            if (t == null || !t.IsAlive)
                return;

            RootForm.Log($"Ending thread '{t.Name:s}'...");

            for (int i = 0; i < 5; i++)
            {
                if (t.Join(1000) == false)
                {
                    continue;
                }

                RootForm.Log($"Thread '{t.Name:s}' ended!");
                return;
            }

            RootForm.Log("Failed to kill gracefully, forcing thread termination!");
            try
            {
                t.Abort();
            }
            catch (Exception Ex)
            {
                RootForm.Log($"Thread '{t.Name:s}' killed: {Ex.Message:s}");
            }

        }


        /// <summary>
        /// Read a message from the CFB driver. This function converts the raw bytes into a proper structure.
        /// </summary>
        /// <returns>An IRP struct object for the header and an array of byte for the body.</returns>
        private bool PopIrpFromDriver(out Irp irp)
        {
            int HeaderSize;
            uint ErrNo;
            string ErrMsg;
            bool bResult;
            int dwNumberOfByteRead;
            IntPtr lpdwNumberOfByteRead;


            //
            // Read the raw header 
            //
            HeaderSize = Core.GetCfbMessageHeaderSize();


            //
            // Get the exact size of the next message
            //

            lpdwNumberOfByteRead =  Marshal.AllocHGlobal(sizeof(int));

            //
            // Wait for and pop one new message
            //
            while (true)
            {

                bResult = Core.ReadCfbDevice(IntPtr.Zero, 0, lpdwNumberOfByteRead);

                if (!bResult)
                {
                    ErrNo = Kernel32.GetLastError();
                    ErrMsg = new Win32Exception(Marshal.GetLastWin32Error()).Message;
                    RootForm.Log($"ReadCfbDevice failed ({ErrNo:x}): {ErrMsg:s}");
                    irp = default(Irp);
                    return false;
                }

                dwNumberOfByteRead = (int)Marshal.PtrToStructure(lpdwNumberOfByteRead, typeof(int));

                if (dwNumberOfByteRead == 0)
                    continue;

                //if(cfg.LogLevel > 2)
                //{
                //   RootForm.Log($"ReadMessage() - ReadCfbDevice() new message of {dwNumberOfByteRead:d} Bytes");
                //}

                if( dwNumberOfByteRead < HeaderSize)
                {
                    throw new Exception($"ReadMessage() - announced size of {dwNumberOfByteRead:x} B is too small");
                }

                break;
            }


            //
            // Get the whole thing
            //

            var RawMessage = Marshal.AllocHGlobal(dwNumberOfByteRead);

            bResult = Core.ReadCfbDevice(RawMessage, dwNumberOfByteRead, new IntPtr(0));

            if (!bResult)
            {
                ErrNo = Kernel32.GetLastError();
                ErrMsg = new Win32Exception(Marshal.GetLastWin32Error()).Message;
                RootForm.Log($"ReadCfbDevice failed ({ErrNo:x}): {ErrMsg:s}");
                irp = default(Irp);
                return false;
            }



            //
            // And convert it to managed code
            //

            char[] charsToTrim = { '\0' };

            // header
            IrpHeader Header = (IrpHeader)Marshal.PtrToStructure(RawMessage, typeof(IrpHeader));

            // driver name
            byte[] DriverNameBytes = new byte[2*0x104];
            Marshal.Copy(RawMessage + Marshal.SizeOf(typeof(IrpHeader)), DriverNameBytes, 0, DriverNameBytes.Length);
            string DriverName = System.Text.Encoding.Unicode.GetString(DriverNameBytes).Trim(charsToTrim);

            // driver name
            byte[] DeviceNameBytes = new byte[2*0x104];
            Marshal.Copy(RawMessage + Marshal.SizeOf(typeof(IrpHeader)) + DriverNameBytes.Length, DeviceNameBytes, 0, DeviceNameBytes.Length);
            string DeviceName = System.Text.Encoding.Unicode.GetString(DeviceNameBytes).Trim(charsToTrim);

            // body          
            byte[] Body = new byte[Header.InputBufferLength];
            Marshal.Copy(RawMessage + HeaderSize, Body, 0, Convert.ToInt32(Header.InputBufferLength));


            Marshal.FreeHGlobal(lpdwNumberOfByteRead);
            Marshal.FreeHGlobal(RawMessage);

            //if(cfg.LogLevel > 2)
            //{
                RootForm.Log($"Dumped IRP #{Header.IoctlCode:x} to '{DriverName:s}' from PID={Header.ProcessId:d}, InBodyLen={Header.InputBufferLength:d}B");
            //}

            irp = new Irp
            {
                Header = Header,
                DriverName = DriverName,
                DeviceName = DeviceName,
                ProcessName = Utils.GetProcessById(Header.ProcessId),
                Body = Body
            };

            return true;
        }


        /// <summary>
        /// Threaded function that'll open a handle to named pipe, and pop out structured messages
        /// </summary>
        private void PopIrpsFromDriverThreadRoutine()
        {
            RootForm.Log("Starting PopIrpsFromDriverThreadRoutine");

            try
            {
                while (CollectIrp)
                {
                    // 
                    // Block on signaled event from the driver
                    //
                    NewMessageEvent.WaitOne(-1);

                    do
                    {
                        if( PopIrpFromDriver(out Irp irp) )
                        {
                            Irps.Add(irp);

                            //if(cfg.LogLevel > 1)
                            //{
                            //    RootForm.Log($"Poping IRP #{Irps.Count:d}");
                            //}

                            NewItems.Add(irp);
                        }
                        else
                        {
                            break;
                        }

                    }
                    while( NewMessageEvent.WaitOne(0) );

                    //
                    // Clear the event for the driver to re-notify
                    //
                    NewMessageEvent.Reset();

                    Thread.Sleep(250);
                }
            }
            catch (Exception Ex)
            {
                RootForm.Log(Ex.Message);

                //if(cfg.LogLevel > 1)
                //{
                //    RootForm.Log("\r\n" + Ex.StackTrace);
                //}
            }

        }


        /// <summary>
        /// Pops new IRPs from the queue and display them in the DataGridView
        /// </summary>
        private void RefreshDataGridViewThreadRoutine()
        {
            RootForm.Log("Starting RefreshDataGridViewThreadRoutine");

            try
            {
                while (UpdateIrpDataView)
                {
                    if ( NewItems.TryTake(out Irp irp) )
                    {
                        AddIrpToDataTable(irp);
                    }
                    else
                    {
                        RootForm.IrpDataView.Refresh();
                        Thread.Sleep(500);
                    }
                }
            }
            catch (Exception Ex)
            {
                RootForm.Log(Ex.Message);

                //if(cfg.LogLevel > 1)
                //{
                //    RootForm.Log("\r\n" + Ex.StackTrace);
                //}
            }

        }

        private void AddIrpToDataTable(Irp irp)
        {
            string IoctlCodeStr;

            if( ( Irp.IrpMajorType )irp.Header.Type == Irp.IrpMajorType.DEVICE_CONTROL )
            {
                IoctlCodeStr = $"0x" + irp.Header.IoctlCode.ToString("x8");
            }
            else
            {
                IoctlCodeStr = "N/A";
            }
            
            IrpDataTable.Rows.Add(
                DateTime.FromFileTime(( long )irp.Header.TimeStamp),
                irp.IrqlAsString(),
                irp.TypeAsString(),
                IoctlCodeStr,
                irp.Header.ProcessId,
                irp.ProcessName,
                irp.Header.ThreadId,
                irp.Header.InputBufferLength,
                irp.Header.OutputBufferLength,
                irp.DriverName,
                irp.DeviceName,
                BitConverter.ToString(irp.Body)
            );

            return;
        }
    }
}