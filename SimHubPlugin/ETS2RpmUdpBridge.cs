using GameReaderCommon;
using SimHub.Plugins;
using System;
using System.Diagnostics;
using System.Globalization;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Threading;

namespace ETS2RpmUdpBridge
{
    [PluginDescription("Sends SimHub engine RPM to local UDP port 29100 for an ESP32 BLE panel.")]
    [PluginAuthor("DIY ETS2 Panel")]
    [PluginName("ETS2 RPM UDP Bridge")]
    public class Plugin : IPlugin, IDataPlugin, IWPFSettingsV2
    {
        private const int UdpPort = 29100;
        private const int SendIntervalMs = 50;

        private UdpClient _udp;
        private Stopwatch _clock;
        private long _lastSendMs = -1000;
        private int _lastRpm;
        private long _packetsSent;
        private int _loggedSendError;
        private readonly object _statusLock = new object();
        private string _gameStatus = "Not initialized";
        private string _lastError = String.Empty;

        public PluginManager PluginManager { get; set; }

        public string LeftMenuTitle
        {
            get { return "RPM UDP Bridge"; }
        }

        public ImageSource PictureIcon
        {
            get { return null; }
        }

        public void Init(PluginManager pluginManager)
        {
            try
            {
                _clock = Stopwatch.StartNew();
                _udp = new UdpClient();
                _udp.Connect(IPAddress.Loopback, UdpPort);
                SetGameStatus("Bridge active; waiting for game data.");
                SimHub.Logging.Current.Info("ETS2 RPM UDP Bridge started: 127.0.0.1:" + UdpPort.ToString(CultureInfo.InvariantCulture));
            }
            catch (Exception ex)
            {
                SetLastError(ex.Message);
                SetGameStatus("Could not start UDP sender.");
                SimHub.Logging.Current.Error("ETS2 RPM UDP Bridge startup failed: " + ex);
            }
        }

        public void DataUpdate(PluginManager pluginManager, ref GameData data)
        {
            if (_udp == null || _clock == null)
            {
                return;
            }

            long now = _clock.ElapsedMilliseconds;
            if (now - Interlocked.Read(ref _lastSendMs) < SendIntervalMs)
            {
                return;
            }
            Interlocked.Exchange(ref _lastSendMs, now);

            int rpm = 0;
            bool gameRunning = false;
            try
            {
                gameRunning = data.GameRunning && data.NewData != null;
                if (gameRunning)
                {
                    double rawRpm = Convert.ToDouble(data.NewData.Rpms, CultureInfo.InvariantCulture);
                    if (!Double.IsNaN(rawRpm) && !Double.IsInfinity(rawRpm))
                    {
                        rpm = (int)Math.Round(rawRpm);
                    }
                }
            }
            catch (Exception ex)
            {
                rpm = 0;
                gameRunning = false;
                SetLastError("Could not read RPM: " + ex.Message);
            }

            if (rpm < 0) rpm = 0;
            if (rpm > 30000) rpm = 30000;

            // The Python BLE bridge accepts both "1234" and "RPM:1234".
            byte[] payload = Encoding.ASCII.GetBytes("RPM:" + rpm.ToString(CultureInfo.InvariantCulture));
            try
            {
                _udp.Send(payload, payload.Length);
                Interlocked.Exchange(ref _lastRpm, rpm);
                Interlocked.Increment(ref _packetsSent);
                Interlocked.Exchange(ref _loggedSendError, 0);
                SetLastError(String.Empty);
                SetGameStatus(gameRunning ? "Live game telemetry is being sent." : "No game data; sending 0 RPM.");
            }
            catch (Exception ex)
            {
                SetLastError(ex.Message);
                SetGameStatus("UDP send failed.");
                if (Interlocked.Exchange(ref _loggedSendError, 1) == 0)
                {
                    SimHub.Logging.Current.Error("ETS2 RPM UDP Bridge send failed: " + ex);
                }
            }
        }

        public void End(PluginManager pluginManager)
        {
            try
            {
                if (_udp != null)
                {
                    _udp.Close();
                    _udp = null;
                }
            }
            catch
            {
                // The plugin is shutting down; do not prevent SimHub from closing.
            }

            SetGameStatus("Stopped.");
            SimHub.Logging.Current.Info("ETS2 RPM UDP Bridge stopped.");
        }

        public Control GetWPFSettingsControl(PluginManager pluginManager)
        {
            return new BridgeStatusControl(this);
        }

        public string GetStatusSummary()
        {
            string gameStatus;
            string lastError;
            lock (_statusLock)
            {
                gameStatus = _gameStatus;
                lastError = _lastError;
            }

            string summary = "UDP target: 127.0.0.1:" + UdpPort.ToString(CultureInfo.InvariantCulture) + Environment.NewLine
                + "Status: " + gameStatus + Environment.NewLine
                + "Latest RPM: " + Interlocked.CompareExchange(ref _lastRpm, 0, 0).ToString(CultureInfo.InvariantCulture) + Environment.NewLine
                + "Packets sent: " + Interlocked.Read(ref _packetsSent).ToString(CultureInfo.InvariantCulture);

            if (!String.IsNullOrEmpty(lastError))
            {
                summary += Environment.NewLine + "Last error: " + lastError;
            }
            return summary;
        }

        private void SetGameStatus(string value)
        {
            lock (_statusLock)
            {
                _gameStatus = value;
            }
        }

        private void SetLastError(string value)
        {
            lock (_statusLock)
            {
                _lastError = value ?? String.Empty;
            }
        }
    }

    internal sealed class BridgeStatusControl : UserControl
    {
        private readonly Plugin _plugin;
        private readonly TextBlock _statusText;
        private readonly DispatcherTimer _timer;

        public BridgeStatusControl(Plugin plugin)
        {
            _plugin = plugin;

            StackPanel root = new StackPanel();
            root.Margin = new Thickness(16);

            TextBlock title = new TextBlock();
            title.Text = "ETS2 RPM -> ESP32 (local UDP)";
            title.FontSize = 18;
            title.Margin = new Thickness(0, 0, 0, 10);
            root.Children.Add(title);

            TextBlock note = new TextBlock();
            note.Text = "Send interval: 50 ms. Target: 127.0.0.1:29100. No internet connection is used.";
            note.TextWrapping = TextWrapping.Wrap;
            note.Margin = new Thickness(0, 0, 0, 10);
            root.Children.Add(note);

            _statusText = new TextBlock();
            _statusText.FontFamily = new FontFamily("Consolas");
            _statusText.TextWrapping = TextWrapping.Wrap;
            root.Children.Add(_statusText);

            Content = root;
            _timer = new DispatcherTimer();
            _timer.Interval = TimeSpan.FromMilliseconds(250);
            _timer.Tick += delegate(object sender, EventArgs e)
            {
                _statusText.Text = _plugin.GetStatusSummary();
            };
            _timer.Start();
            Unloaded += delegate(object sender, RoutedEventArgs e)
            {
                _timer.Stop();
            };
            _statusText.Text = _plugin.GetStatusSummary();
        }
    }
}
