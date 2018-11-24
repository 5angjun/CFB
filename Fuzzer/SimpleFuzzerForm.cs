﻿using System;
using System.Collections;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace Fuzzer
{
    public class SimpleFuzzerForm : System.Windows.Forms.Form
    {
        private readonly Irp Irp;

        private System.Windows.Forms.Button startAsyncButton;
        private System.Windows.Forms.Button cancelAsyncButton;
        private System.Windows.Forms.ProgressBar progressBar1;
        private System.Windows.Forms.Label statusLabel;
        private System.Windows.Forms.Label resultLabel;
        private Label label1;
        private Label label2;
        private Label label3;
        private Label label4;
        private Label label5;
        private Label label6;
        private TextBox FuzzByteStartIndexTextbox;
        private TextBox FuzzByteEndIndexTextbox;
        private TextBox MaxTestCaseTextbox;
        private System.ComponentModel.BackgroundWorker worker;
        private ComboBox StrategyComboBox;

        private FuzzingStrategy[] Strategies;
        private FuzzingSession Session;

        
        public SimpleFuzzerForm(Irp irp)
        {
            InitializeComponent();
            InitializeFuzzingObjects();
            this.Irp = irp;
            this.Text = $"Fuzzing {Irp.ToString()}";
            InitializeWorker();
        }

        private void InitializeWorker()
        {
            worker.DoWork                += new DoWorkEventHandler(BackgroundWorkRoutine);
            worker.RunWorkerCompleted    += new RunWorkerCompletedEventHandler(OnComplete);
            worker.ProgressChanged       += new ProgressChangedEventHandler(OnProgressChange);
        }


        private void InitializeFuzzingObjects()
        {
            Session = new FuzzingSession();

            Strategies = new FuzzingStrategy[]
            {
                new RandomFuzzingStrategy(),
            };

            foreach (FuzzingStrategy s in Strategies)
            {
                StrategyComboBox.Items.Add(s);
            }
        }


        private void StartAsyncButton_Click(Object sender, EventArgs e)
        {
            resultLabel.Text = String.Empty;
            this.startAsyncButton.Enabled = false;
            this.cancelAsyncButton.Enabled = true;
            worker.RunWorkerAsync();
        }

        private void CancelAsyncButton_Click(Object sender, EventArgs e)
        {
            this.worker.CancelAsync();
            resultLabel.Text = "Cancelling...";
            cancelAsyncButton.Enabled = false;
        }


        private void BackgroundWorkRoutine(object sender, DoWorkEventArgs evt)
        {
            BackgroundWorker worker = sender as BackgroundWorker;

            resultLabel.Text = $"Defining strategy...";
            FuzzingStrategy SelectedStrategy = (FuzzingStrategy)StrategyComboBox.SelectedItem;

            if (SelectedStrategy == null)
            {
                MessageBox.Show($"Strategy '{SelectedStrategy}' does not exist", "UnimplementedStrategy");
                return;
            }


            resultLabel.Text = $"Validating session parameters...";

            bool res = System.Int32.TryParse(FuzzByteStartIndexTextbox.Text, out int StartByteIndex);
            if(!res || StartByteIndex < 0 || StartByteIndex > this.Irp.Body.Length-1)
                StartByteIndex = 0;

            res = System.Int32.TryParse(FuzzByteEndIndexTextbox.Text, out int EndByteIndex);
            if (!res || EndByteIndex < 0 || EndByteIndex > this.Irp.Body.Length-1)
                EndByteIndex = this.Irp.Body.Length-1;

            if (StartByteIndex > EndByteIndex)
            {
                StartByteIndex = 0;
                EndByteIndex = this.Irp.Body.Length - 1;
            }


            resultLabel.Text = $"Initiating fuzzing with strategy '{SelectedStrategy}'...";
            Session.Start(SelectedStrategy, Irp, worker, evt, StartByteIndex, EndByteIndex);
        }


        private void OnComplete(object sender, RunWorkerCompletedEventArgs e)
        {
            if (e.Error != null)
            {
                MessageBox.Show(e.Error.Message);
            }
            else if (e.Cancelled)
            {
                resultLabel.Text = "Cancelled";
            }
            else
            {
                resultLabel.Text = "Done";
                progressBar1.Value = 0;
            }

            startAsyncButton.Enabled = true;
            cancelAsyncButton.Enabled = false;
        }


        private void OnProgressChange(object sender, ProgressChangedEventArgs e)
        {
            this.progressBar1.Value = e.ProgressPercentage;
        }
                     

        #region Windows Form Designer generated code
        private void InitializeComponent()
        {
            this.startAsyncButton = new System.Windows.Forms.Button();
            this.cancelAsyncButton = new System.Windows.Forms.Button();
            this.resultLabel = new System.Windows.Forms.Label();
            this.statusLabel = new System.Windows.Forms.Label();
            this.progressBar1 = new System.Windows.Forms.ProgressBar();
            this.worker = new System.ComponentModel.BackgroundWorker();
            this.label1 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.label3 = new System.Windows.Forms.Label();
            this.label4 = new System.Windows.Forms.Label();
            this.FuzzByteStartIndexTextbox = new System.Windows.Forms.TextBox();
            this.FuzzByteEndIndexTextbox = new System.Windows.Forms.TextBox();
            this.label5 = new System.Windows.Forms.Label();
            this.MaxTestCaseTextbox = new System.Windows.Forms.TextBox();
            this.StrategyComboBox = new System.Windows.Forms.ComboBox();
            this.label6 = new System.Windows.Forms.Label();
            this.SuspendLayout();
            // 
            // startAsyncButton
            // 
            this.startAsyncButton.Location = new System.Drawing.Point(37, 203);
            this.startAsyncButton.Name = "startAsyncButton";
            this.startAsyncButton.Size = new System.Drawing.Size(120, 30);
            this.startAsyncButton.TabIndex = 1;
            this.startAsyncButton.Text = "Start Run";
            this.startAsyncButton.Click += new System.EventHandler(this.StartAsyncButton_Click);
            // 
            // cancelAsyncButton
            // 
            this.cancelAsyncButton.Enabled = false;
            this.cancelAsyncButton.Location = new System.Drawing.Point(445, 203);
            this.cancelAsyncButton.Name = "cancelAsyncButton";
            this.cancelAsyncButton.Size = new System.Drawing.Size(119, 30);
            this.cancelAsyncButton.TabIndex = 2;
            this.cancelAsyncButton.Text = "Cancel Run";
            this.cancelAsyncButton.Click += new System.EventHandler(this.CancelAsyncButton_Click);
            // 
            // resultLabel
            // 
            this.resultLabel.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.resultLabel.Location = new System.Drawing.Point(192, 16);
            this.resultLabel.Name = "resultLabel";
            this.resultLabel.Size = new System.Drawing.Size(372, 23);
            this.resultLabel.TabIndex = 3;
            this.resultLabel.Text = "Not started";
            this.resultLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // statusLabel
            // 
            this.statusLabel.Location = new System.Drawing.Point(44, 16);
            this.statusLabel.Name = "statusLabel";
            this.statusLabel.Size = new System.Drawing.Size(79, 23);
            this.statusLabel.TabIndex = 3;
            this.statusLabel.Text = "Current state";
            this.statusLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // progressBar1
            // 
            this.progressBar1.Location = new System.Drawing.Point(37, 180);
            this.progressBar1.Name = "progressBar1";
            this.progressBar1.Size = new System.Drawing.Size(527, 17);
            this.progressBar1.Step = 2;
            this.progressBar1.TabIndex = 4;
            // 
            // worker
            // 
            this.worker.WorkerReportsProgress = true;
            this.worker.WorkerSupportsCancellation = true;
            // 
            // label1
            // 
            this.label1.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.label1.Location = new System.Drawing.Point(192, 51);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(372, 23);
            this.label1.TabIndex = 5;
            this.label1.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // label2
            // 
            this.label2.Location = new System.Drawing.Point(44, 51);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(113, 23);
            this.label2.TabIndex = 6;
            this.label2.Text = "GetLastError()";
            this.label2.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // label3
            // 
            this.label3.Location = new System.Drawing.Point(44, 86);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(113, 23);
            this.label3.TabIndex = 7;
            this.label3.Text = "From Index (optional)";
            this.label3.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // label4
            // 
            this.label4.Location = new System.Drawing.Point(44, 115);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(113, 23);
            this.label4.TabIndex = 8;
            this.label4.Text = "To Index (optional)";
            this.label4.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // FuzzByteStartIndexTextbox
            // 
            this.FuzzByteStartIndexTextbox.Location = new System.Drawing.Point(192, 88);
            this.FuzzByteStartIndexTextbox.Name = "FuzzByteStartIndexTextbox";
            this.FuzzByteStartIndexTextbox.Size = new System.Drawing.Size(100, 20);
            this.FuzzByteStartIndexTextbox.TabIndex = 9;
            // 
            // FuzzByteEndIndexTextbox
            // 
            this.FuzzByteEndIndexTextbox.Location = new System.Drawing.Point(192, 117);
            this.FuzzByteEndIndexTextbox.Name = "FuzzByteEndIndexTextbox";
            this.FuzzByteEndIndexTextbox.Size = new System.Drawing.Size(100, 20);
            this.FuzzByteEndIndexTextbox.TabIndex = 10;
            // 
            // label5
            // 
            this.label5.Location = new System.Drawing.Point(44, 143);
            this.label5.Name = "label5";
            this.label5.Size = new System.Drawing.Size(113, 23);
            this.label5.TabIndex = 11;
            this.label5.Text = "Maximum Test Cases";
            this.label5.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // MaxTestCaseTextbox
            // 
            this.MaxTestCaseTextbox.Location = new System.Drawing.Point(192, 145);
            this.MaxTestCaseTextbox.Name = "MaxTestCaseTextbox";
            this.MaxTestCaseTextbox.Size = new System.Drawing.Size(100, 20);
            this.MaxTestCaseTextbox.TabIndex = 12;
            this.MaxTestCaseTextbox.Text = "-1";
            // 
            // StrategyComboBox
            // 
            this.StrategyComboBox.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.StrategyComboBox.FormattingEnabled = true;
            this.StrategyComboBox.Location = new System.Drawing.Point(443, 88);
            this.StrategyComboBox.Name = "StrategyComboBox";
            this.StrategyComboBox.Size = new System.Drawing.Size(121, 21);
            this.StrategyComboBox.TabIndex = 13;
            this.StrategyComboBox.TabStop = false;
            // 
            // label6
            // 
            this.label6.Location = new System.Drawing.Point(345, 88);
            this.label6.Name = "label6";
            this.label6.Size = new System.Drawing.Size(96, 23);
            this.label6.TabIndex = 14;
            this.label6.Text = "Fuzzing Strategy";
            this.label6.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // SimpleFuzzerForm
            // 
            this.ClientSize = new System.Drawing.Size(599, 243);
            this.Controls.Add(this.label6);
            this.Controls.Add(this.StrategyComboBox);
            this.Controls.Add(this.MaxTestCaseTextbox);
            this.Controls.Add(this.label5);
            this.Controls.Add(this.FuzzByteEndIndexTextbox);
            this.Controls.Add(this.FuzzByteStartIndexTextbox);
            this.Controls.Add(this.label4);
            this.Controls.Add(this.label3);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.label1);
            this.Controls.Add(this.progressBar1);
            this.Controls.Add(this.statusLabel);
            this.Controls.Add(this.resultLabel);
            this.Controls.Add(this.cancelAsyncButton);
            this.Controls.Add(this.startAsyncButton);
            this.Name = "SimpleFuzzerForm";
            this.Text = "Simple IRP Fuzzer";
            this.ResumeLayout(false);
            this.PerformLayout();

        }
        #endregion
    }
}