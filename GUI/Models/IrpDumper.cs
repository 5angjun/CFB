﻿using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.ApplicationModel.Background;
using Windows.ApplicationModel.Core;
using Windows.Storage;
using Windows.System.Threading;
using Windows.UI.Core;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Navigation;


namespace GUI.Models
{
    /// <summary>
    /// This class operates in background and manages the IRP fetching operations.
    /// Upon receiving new IRPs it'll push them to the IrpRepository.
    /// </summary>
    public class IrpDumper
    {
        private double _probe_new_data_delay = 2; //0.5; // seconds
        private ApplicationTrigger _trigger = null;
        private BackgroundTaskRegistration _task;
        IBackgroundTaskInstance _taskInstance = null;
        BackgroundTaskDeferral _deferral = null;
        ThreadPoolTimer _periodicTimer = null;

        BackgroundTaskCancellationReason _cancelReason = BackgroundTaskCancellationReason.Abort;
        volatile bool _cancelRequested = false;
        string _cancelReasonExtra = "";

        private const string IrpDumperTaskName = "IrpDumperBackgroundTask";
        private static string IrpDumperTaskClass = typeof(GUI.Tasks.IrpDumperBackgroundTask).FullName;

        private const string IrpDumperPollDelayKey = "BackgroundTaskPollDelayMs";

        public IrpDumper()
        {
            UnregisterBackgroundTask();
            _trigger = new ApplicationTrigger();
            var requestTask = BackgroundExecutionManager.RequestAccessAsync();
            var builder = new BackgroundTaskBuilder();
            builder.Name = IrpDumperTaskName;
            builder.SetTrigger(_trigger);
            _task = builder.Register();
            ApplicationData.Current.LocalSettings.Values.Remove(IrpDumperTaskName);
            ApplicationData.Current.LocalSettings.Values.Remove(IrpDumperPollDelayKey);
        }


        public async void Trigger()
            => await _trigger.RequestAsync();


        public void SetInstance(IBackgroundTaskInstance taskInstance)
        {
            _taskInstance = taskInstance;
            _deferral = taskInstance.GetDeferral();
            _taskInstance.Canceled += OnCanceled;
        }


        private bool _enabled = false;
        public bool Enabled
        {
            get => _enabled;
            set
            {
                bool success = false;
                if (value)
                    success = StartFetcher();
                else
                    success = StopFetcher();

                if (success)
                    _enabled = value;
            }
        }


        private bool StartFetcher()
        {
            Debug.WriteLine($"Starting in-process background instance '{_task.Name}'...");

            var delay = (double) (ApplicationData.Current.LocalSettings.Values[IrpDumperPollDelayKey] ?? _probe_new_data_delay);
            
            _periodicTimer = ThreadPoolTimer.CreatePeriodicTimer(
                new TimerElapsedHandler(PeriodicTimerCallback),
                TimeSpan.FromSeconds(delay)
            );

            return true;
        }


        private bool StopFetcher()
        {
            Debug.WriteLine($"Stopping in-process background instance '{_task.Name}'...");
            _periodicTimer.Cancel();
            _cancelRequested = true;
            _cancelReasonExtra = "UserRequest";
            return UnregisterBackgroundTask();
        }


        private void OnCanceled(IBackgroundTaskInstance sender, BackgroundTaskCancellationReason reason)
        {
            _cancelRequested = true;
            _cancelReason = reason;
            Debug.WriteLine($"Canceled background instance '{sender.Task.Name}'");
            _deferral.Complete();
        }


        private void OnProgress(IBackgroundTaskRegistration task, BackgroundTaskProgressEventArgs args)
        {
            Debug.WriteLine($"OnProgress('{task.Name}')");
        }


        private void OnCompleted(IBackgroundTaskRegistration task, BackgroundTaskCompletedEventArgs args)
        {
            Debug.WriteLine($"OnCompleted('{task.Name}')");
        }


        //
        // Periodic callback for the task: this is where we actually do the job of fetching new IRPs
        //
        private void PeriodicTimerCallback(ThreadPoolTimer timer)
        {
            // is there a pending cancellation request
            if (_cancelRequested)
            {
                _periodicTimer.Cancel();
                var msg = $"Cancelling background task {_task.Name }, reason: {_cancelReason.ToString()}";
                if (_cancelReasonExtra.Length > 0)
                    msg += _cancelReasonExtra;
                Debug.WriteLine(msg);
                _deferral.Complete();
                return;
            }

            try
            {
                //
                // collect the irps from the broker
                //
                List<Irp> NewIrps = FetchIrps();
                _taskInstance.Progress += (uint)NewIrps.Count;

                Debug.WriteLine($"Received {NewIrps.Count:d} new irps");

                //
                // push them to the db
                //
            }
            catch (Exception e)
            {
                _cancelRequested = true;
                _cancelReason = BackgroundTaskCancellationReason.ConditionLoss;
                _cancelReasonExtra = e.Message;
            }
        }


        //
        // Fetch new IRPs
        //
        private List<Irp> FetchIrps()
        {
            var msg = Task.Run(() => App.BrokerSession.GetInterceptedIrps()).Result;

            if (msg.header.is_success)
                return msg.body.irps;

            throw new Exception($"GetInterceptedIrps() request returned FALSE, GLE=0x{msg.header.gle}");
        }


        private static bool UnregisterBackgroundTask()
        {
            foreach (var cur in BackgroundTaskRegistration.AllTasks)
            {
                if (cur.Value.Name == IrpDumperTaskName)
                {
                    cur.Value.Unregister(true);
                    return true;
                }
            }
            return false;
        }
    }
}
