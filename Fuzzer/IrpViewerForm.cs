﻿using System;
using System.Text;
using System.Windows.Forms;


namespace Fuzzer
{
    public partial class IrpViewerForm : Form
    {
        private Irp Irp;
        private int Index;

        public IrpViewerForm(int Index, Irp irp)
        {
            InitializeComponent();
            this.Text = $"HexViewer for IRP #{Index:d} (IoctlNumber={irp.Header.IoctlCode:x}) to {irp.DeviceName:s}";
            this.Irp = irp;
            this.Index = Index;

            UpdateIrpDetailFields();
            UpdateIrpBodyTextBox();
        }

        private void UpdateIrpDetailFields()
        {
            IrpIndexTextBox.Text = this.Index.ToString();
            IrpDriverNameTextBox.Text = this.Irp.DriverName;
            IrpDeviceNameTextBox.Text = this.Irp.DeviceName;
            IrpTimestampTextBox.Text = DateTime.FromFileTime((long)this.Irp.Header.TimeStamp).ToString();
            IrpProcessNameTextBox.Text = $"{this.Irp.ProcessName} ({this.Irp.Header.ProcessId})";
            IrpIrqlTextBox.Text = $"{this.Irp.IrqlAsString()} (0x{this.Irp.Header.Irql})";

            Irp.IrpMajorType CurrentIrpType = ( Irp.IrpMajorType )this.Irp.Header.Type;

            switch(CurrentIrpType)
            {
                case Irp.IrpMajorType.IRP_MJ_DEVICE_CONTROL:
                case Irp.IrpMajorType.IRP_MJ_INTERNAL_DEVICE_CONTROL:
                    IrpIoctlCodeTextBox.Text = $"0x{this.Irp.Header.IoctlCode:x8}";
                    break;

                default:
                    label6.Text = "IRP Type..........................";
                    IrpIoctlCodeTextBox.Text = this.Irp.TypeAsString();
                    break;
            }

        }

        private void UpdateIrpBodyTextBox()
        {
            IrpBodyHexdumpTextBox.Text = Utils.Hexdump(this.Irp.Body);
        }

    }
}
