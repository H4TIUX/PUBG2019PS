using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Management;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows.Forms;

internal static class Program
{
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern uint RegisterWindowMessage(string text);
    [DllImport("user32.dll")] static extern bool PostMessage(IntPtr window,uint message,IntPtr w,IntPtr l);
    internal static readonly uint ActivationMessage=RegisterWindowMessage("PUBG2019.Launcher.Activate");
    [STAThread] private static void Main()
    {
        bool first;
        using(var mutex=new System.Threading.Mutex(true,@"Local\PUBG2019.Launcher",out first)) {
            if(!first){PostMessage(new IntPtr(0xffff),ActivationMessage,IntPtr.Zero,IntPtr.Zero);return;}
            try {Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);Application.Run(new LauncherForm());}
            finally {mutex.ReleaseMutex();}
        }
    }
}

internal sealed class MapOption
{
    public string Label;
    public string Value;
    public Bitmap Preview;

    public MapOption(string label, string value, Bitmap preview)
    {
        Label = label;
        Value = value;
        Preview = preview;
    }

    public override string ToString() { return Label; }
}

internal sealed class LanguageOption
{
    public readonly string Label, Code;
    public LanguageOption(string label, string code) { Label = label; Code = code; }
    public override string ToString() { return Label; }
}

internal sealed partial class LauncherForm : Form
{
    private readonly string _dir = AppDomain.CurrentDomain.BaseDirectory.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
    private readonly object _logLock = new object();
    private readonly Timer _timer = new Timer();
    private readonly Timer _languageTimer = new Timer();
    private StyledTextField _name;
    private MapSelector _map;
    private BotCountSelector _bots;
    private ArtworkCheckBox _hostLocal;
    private ArtworkCheckBox _autoJoinLocal;
    private StyledTextField _address;
    private ArtworkButton _start;
    private ArtworkButton _stopServer;
    private ArtworkLabel _status;
    private ArtworkLabel _mapHint;
    private LauncherLogView _log;
    private Process _hostProcess;
    private Process _joinProcess;
    private DateTime _hostStartedAt;
    private DateTime _launchStartedAt;
    private bool _joinStarted;
    private bool _joinProcessHandled;
    private bool _finished;
    private bool _autoJoinRequested;
    private bool _serverReady;
    private bool _stopRequested;
    private string _requestedMap;
    private NotifyIcon _trayIcon;
    private ContextMenuStrip _trayMenu;
    private bool _exitRequested;
    private string _uiLanguage = "en";
    private string _selectedLanguage = "";
    private Font _latinStartFont, _latinStatusFont, _nonLatinStartFont, _nonLatinStatusFont;
    private string _statusKey = "READY";
    private string _startKey = "HOST SERVER";
    private Panel _updatePanel;
    private bool _closeToTray = true;
    private const string DisplayVersion = "V0.71";
    private string ProfilePath { get { return Path.Combine(_dir, "PUBG2019Profile.ini"); } }

    private static readonly Color Bg = Color.FromArgb(18, 20, 22);
    private static readonly Color Field = Color.FromArgb(25, 28, 31);
    private static readonly Color Yellow = Color.FromArgb(244, 190, 0);
    private static readonly Color Fg = Color.FromArgb(235, 235, 235);
    private static readonly Color Muted = Color.FromArgb(158, 162, 165);

    public LauncherForm()
    {
        Text = "PUBG 2019 Launcher";
        try { Icon = LoadIconResource("Launcher.PubgIcon"); } catch { }
        StartPosition = FormStartPosition.CenterScreen;
        FormBorderStyle = FormBorderStyle.None;
        MaximizeBox = true;
        MinimizeBox = true;
        AutoScaleMode = AutoScaleMode.None;
        ClientSize = new Size(Math.Min(1180, Screen.PrimaryScreen.WorkingArea.Width-24), Math.Min(850, Screen.PrimaryScreen.WorkingArea.Height-48));
        MinimumSize = new Size(900, 680);
        BackColor = Bg;
        ForeColor = Fg;
        Font = new Font("Segoe UI", 10F, FontStyle.Regular);
        BackgroundImage = null;
        BackgroundImageLayout = ImageLayout.Stretch;

        BuildUi();
        BuildWindowActions();
        ArrangeLanUi();
        // The original controls still hold backend state, but are never displayed.
        foreach (Control control in Controls) control.Visible = false;
        Resize += delegate { if(_nativeView!=null)_nativeView.Invalidate(); };
        LoadSavedValues();
        LoadLanValues();
        _uiLanguage = ReadWindowsLanguage();
        try {
            PlayerProfile profile = PlayerProfile.LoadOrCreate(ProfilePath, _name.Text);
            if (String.IsNullOrWhiteSpace(_name.Text) && profile.Name.Length > 0) _name.Text = profile.Name;
            PlayerProfile.SaveName(ProfilePath, _name.Text);
            _selectedLanguage = profile.Language;
            _closeToTray = profile.CloseToTray;
            _uiLanguage = EffectiveUiLanguage();
        } catch (Exception ex) { AppendLog("PROFILE: " + ex.Message); }
        _name.Leave += delegate {
            string name = _name.Text.Trim();
            if (!Regex.IsMatch(name, "^[A-Za-z0-9_-]{1,24}$")) return;
            try { PlayerProfile.SaveName(ProfilePath, name); SaveLauncherValues(name); }
            catch (Exception ex) { AppendLog("PROFILE: " + ex.Message); }
        };

        _timer.Interval = 700;
        _timer.Tick += TimerTick;
        _languageTimer.Interval = 5000;
        _languageTimer.Tick += delegate {
            string current = EffectiveUiLanguage();
            if (!current.Equals(_uiLanguage, StringComparison.OrdinalIgnoreCase)) { _uiLanguage = current; ApplyLanguage(); }
        };
        _languageTimer.Start();
        _map.SelectedIndexChanged += delegate { UpdateMapHint(); };
        _hostLocal.CheckedChanged += delegate { UpdateMode(); UpdateMapHint(); };
        UpdateMode();
        UpdateMapHint();
        ApplyLanguage();
        Shown += delegate {
            StartLanServices();
            StartNativeUi();
            if (Environment.GetEnvironmentVariable("PUBG2019_LAN_TEST") != "1" && !ShowPendingUpdateResult()) BeginUpdateCheck(true);
        };
    }

    private void BeginUpdateCheck(bool automatic)
    {
        UpdateManager.Start(this, _dir, automatic);
    }

    internal void UpdateLog(string message) { AppendLog(message); }
    internal void ExitForUpdate() { _exitRequested = true; Close(); }
    internal void ShowUpdateChecking()
    {
        ShowUpdatePanel("CHECKING FOR UPDATES", "Checking the signed update package. Please wait...", null, null, null);
    }
    internal void ShowUpdateNotice(string title, string message, Action dismissed)
    {
        ShowUpdatePanel(title, message, "OK", dismissed, null);
    }
    internal void ShowUpdateOffer(Action install, Action later)
    {
        ShowUpdatePanel("UPDATE AVAILABLE", "A verified update is ready. Your personal settings will be kept.", "INSTALL UPDATE", install, later);
    }
    private void ShowUpdatePanel(string title, string message, string primaryText, Action primary, Action cancel)
    {
        if (InvokeRequired) { BeginInvoke(new Action(delegate { ShowUpdatePanel(title, message, primaryText, primary, cancel); })); return; }
        if (_updatePanel != null) { Controls.Remove(_updatePanel); _updatePanel.Dispose(); }
        var panel = new Panel { Name="UpdateOverlay", BackColor=Color.FromArgb(19,23,25), BorderStyle=BorderStyle.FixedSingle };
        int width=Math.Min(ClientSize.Width-S(80),S(830)), height=S(310);
        panel.SetBounds((ClientSize.Width-width)/2,(ClientSize.Height-height)/2,width,height);
        panel.Paint += delegate(object sender, PaintEventArgs e) { using(var pen=new Pen(Yellow,Math.Max(2,S(3)))) e.Graphics.DrawRectangle(pen,1,1,panel.Width-3,panel.Height-3); };
        var strip = new Panel { BackColor=Yellow, Location=new Point(0,0), Size=new Size(width,S(6)) };
        var icon = new PictureBox { Image=(Icon ?? SystemIcons.Application).ToBitmap(), SizeMode=PictureBoxSizeMode.Zoom, Location=new Point(S(28),S(26)), Size=new Size(S(42),S(42)) };
        var heading = new Label { Text=title, ForeColor=Yellow, BackColor=Color.Transparent, Font=new Font("Impact",Math.Max(16,S(30)),FontStyle.Regular,GraphicsUnit.Pixel), Location=new Point(S(84),S(26)), Size=new Size(width-S(120),S(50)), AutoEllipsis=true };
        var line = new Panel { BackColor=Color.FromArgb(68,72,71), Location=new Point(S(28),S(88)), Size=new Size(width-S(56),1) };
        var body = new Label { Text=message, ForeColor=Fg, BackColor=Color.Transparent, Font=new Font("Segoe UI",Math.Max(12,S(18)),FontStyle.Regular,GraphicsUnit.Pixel), Location=new Point(S(32),S(108)), Size=new Size(width-S(64),S(94)), AutoEllipsis=true };
        panel.Controls.AddRange(new Control[]{strip,icon,heading,line,body});
        if (primaryText != null) {
            var yes = ProfileDialogButton(primaryText,new Point(width-S(260),height-S(68)),new Size(S(220),S(40)),true);
            yes.Name="UpdatePrimary";
            yes.Click += delegate { DismissUpdatePanel(); if(primary!=null)primary(); };
            panel.Controls.Add(yes);
            if(cancel!=null) {
                var no=ProfileDialogButton("LATER",new Point(S(32),height-S(68)),new Size(S(180),S(40)),false);
                no.Name="UpdateLater";
                no.Click += delegate { DismissUpdatePanel(); cancel(); };
                panel.Controls.Add(no);
            }
        }
        _updatePanel=panel; Controls.Add(panel); panel.BringToFront();
        if (primaryText != null) panel.Controls.Find("UpdatePrimary",false)[0].Focus();
    }
    private void DismissUpdatePanel()
    {
        if(_updatePanel==null)return;
        Controls.Remove(_updatePanel);_updatePanel.Dispose();_updatePanel=null;
    }
    private bool ShowPendingUpdateResult()
    {
        string path=Path.Combine(_dir,"UpdateBackups","LauncherUpdateResult.txt");
        if(!File.Exists(path))return false;
        try {
            string[] lines=File.ReadAllLines(path,Encoding.UTF8);
            if(lines.Length<3)return false;
            File.Delete(path);
            bool success=lines[0]=="OK";string stage=lines[1];
            string message=String.Join("\n",lines,2,lines.Length-2);
            if(success)CleanupUpdateStage(stage);
            ShowUpdateNotice(success?"UPDATE COMPLETE":"UPDATE FAILED",message,delegate {BeginUpdateCheck(true);});
            return true;
        } catch(Exception ex){AppendLog("UPDATE ERROR: "+ex.Message);return false;}
    }
    private static void CleanupUpdateStage(string path)
    {
        try {
            string root=Path.GetFullPath(Path.GetTempPath()).TrimEnd(Path.DirectorySeparatorChar);
            string full=Path.GetFullPath(path).TrimEnd(Path.DirectorySeparatorChar);
            if(!String.Equals(Path.GetDirectoryName(full),root,StringComparison.OrdinalIgnoreCase) || !Path.GetFileName(full).StartsWith("PUBG2019_Update_",StringComparison.Ordinal))return;
            var thread=new System.Threading.Thread(delegate() {for(int i=0;i<8;i++){try{if(Directory.Exists(full))Directory.Delete(full,true);return;}catch{System.Threading.Thread.Sleep(500);}}});
            thread.IsBackground=true;thread.Start();
        } catch {}
    }

    private string T(string text) { return NativeUiText.Get(_uiLanguage, text); }

    private string EffectiveUiLanguage()
    {
        return String.IsNullOrEmpty(_selectedLanguage) ? ReadWindowsLanguage() : _selectedLanguage;
    }

    [DllImport("kernel32.dll")]
    private static extern ushort GetUserDefaultUILanguage();

    private static string ReadWindowsLanguage()
    {
        try {
            return LauncherLocalization.WindowsLanguage(CultureInfo.GetCultureInfo(GetUserDefaultUILanguage()).Name);
        } catch { }
        return LauncherLocalization.WindowsLanguage(CultureInfo.CurrentUICulture.Name);
    }

    private void SetStatus(string text) { _statusKey = text; _status.Text = T(text); }
    private void SetStart(string text) { _startKey = text; _start.Text = T(text); }

    private void ApplyLanguage()
    {
        ApplyLanLanguage();
        Controls.Find("OpenProfile", false)[0].Text = T("PROFILE");
        _hostLocal.Text = T("Start a local server");
        _autoJoinLocal.Text = T("Join automatically");
        _stopServer.Text = T("STOP SERVER");
        _trayMenu.Items[0].Text = T("Exit");
        string newMap = _uiLanguage == "de" ? "neu" : LauncherTranslations.Get(_uiLanguage, "new", "new");
        string classicMap = _uiLanguage == "de" ? "klassisch" : LauncherTranslations.Get(_uiLanguage, "classic", "classic");
        ((MapOption)_map.Items[1]).Label = "Erangel (" + newMap + ")";
        ((MapOption)_map.Items[2]).Label = "Erangel (" + classicMap + ")";
        bool nonLatin = Array.IndexOf(new[] { "ar", "ja", "ko", "zh-CN", "zh-TW", "th", "ru", "uk" }, _uiLanguage) >= 0;
        float scale = ClientSize.Width / 1306F;
        if (_latinStartFont == null) { _latinStartFont = _start.Font; _latinStatusFont = _status.Font; }
        if (nonLatin && _nonLatinStartFont == null) {
            _nonLatinStartFont = new Font("Segoe UI", 34 * scale, FontStyle.Bold, GraphicsUnit.Pixel);
            _nonLatinStatusFont = new Font("Segoe UI", 25 * scale, FontStyle.Bold, GraphicsUnit.Pixel);
        }
        _start.Font = nonLatin ? _nonLatinStartFont : _latinStartFont;
        _status.Font = nonLatin ? _nonLatinStatusFont : _latinStatusFont;
        _map.Invalidate();
        SetStatus(_statusKey); SetStart(_startKey); UpdateMapHint(); UpdateLanState(); _log.Invalidate(); Invalidate(); NativePublish();
    }

    private void BuildWindowActions()
    {
        int width = Math.Max(36, S(46));
        int height = Math.Max(28, S(36));
        int inset = Math.Max(6, S(10));
        WindowActionButton minimize = new WindowActionButton(false);
        minimize.Name = "MinimizeLauncher";
        minimize.AccessibleName = "Minimize launcher";
        minimize.SetBounds(ClientSize.Width - inset - width * 2, inset, width, height);
        minimize.Anchor = AnchorStyles.Top | AnchorStyles.Right;
        minimize.Click += delegate { WindowState = FormWindowState.Minimized; };
        Controls.Add(minimize);
        WindowActionButton hide = new WindowActionButton(true);
        hide.Name = "HideLauncher";
        hide.AccessibleName = "Hide launcher to notification area";
        hide.SetBounds(ClientSize.Width - inset - width, inset, width, height);
        hide.Anchor = AnchorStyles.Top | AnchorStyles.Right;
        hide.Click += delegate { HandleWindowClose(); };
        Controls.Add(hide);
        ArtworkButton profileButton = new ArtworkButton();
        profileButton.Name = "OpenProfile";
        profileButton.AccessibleName = "Open player profile";
        profileButton.Text = "PROFILE";
        profileButton.Font = LauncherStyle.BodyFont(Math.Max(14, S(19)));
        profileButton.SetBounds(ClientSize.Width - inset - width * 2 - Math.Max(86, S(110)), inset, Math.Max(80, S(104)), height);
        profileButton.Anchor = AnchorStyles.Top | AnchorStyles.Right;
        profileButton.Click += delegate { ShowProfile(); };
        Controls.Add(profileButton);
        ArtworkButton characterButton = new ArtworkButton();
        characterButton.Name = "OpenCharacter"; characterButton.Text = "CHARACTER";
        characterButton.AccessibleName = "Open character and clothing";
        characterButton.Font = LauncherStyle.BodyFont(Math.Max(14, S(19)));
        characterButton.Anchor = AnchorStyles.Top | AnchorStyles.Right;
        characterButton.Click += delegate { try { StartWardrobe(); } catch(Exception ex) { LanError(ex.Message); } };
        Controls.Add(characterButton);
        profileButton.BringToFront();
        minimize.BringToFront();
        hide.BringToFront();

        _trayMenu = new ContextMenuStrip();
        ToolStripMenuItem exit = new ToolStripMenuItem("Exit");
        exit.Name = "ExitLauncher";
        exit.Click += delegate { _exitRequested = true; Close(); };
        _trayMenu.Items.Add(exit);
        _trayIcon = new NotifyIcon();
        _trayIcon.Icon = Icon ?? SystemIcons.Application;
        _trayIcon.Text = "PUBG 2019 Launcher";
        _trayIcon.ContextMenuStrip = _trayMenu;
        _trayIcon.MouseClick += delegate(object sender, MouseEventArgs e) {
            if (e.Button == MouseButtons.Left) RestoreFromTray();
        };
        _trayIcon.Visible = true;
    }

    private void ShowProfile()
    {
        try
        {
            PlayerProfile profile = PlayerProfile.SaveName(ProfilePath, _name.Text);
            using (Form dialog = new Form())
            {
                dialog.Text = T("Player profile");
                dialog.StartPosition = FormStartPosition.CenterParent;
                dialog.FormBorderStyle = FormBorderStyle.None;
                dialog.MaximizeBox = false; dialog.MinimizeBox = false; dialog.ShowInTaskbar = false;
                dialog.AutoScaleMode = AutoScaleMode.None;
                if (_uiLanguage == "ar") dialog.RightToLeft = RightToLeft.Yes;
                dialog.ClientSize = new Size(570, 570);
                dialog.BackColor = Color.FromArgb(27,28,32); dialog.ForeColor = Fg; dialog.Font = NativeArt.Font(16);
                dialog.Paint += delegate(object sender,PaintEventArgs e){using(var pen=new Pen(Color.FromArgb(119,119,119)))e.Graphics.DrawRectangle(pen,0,0,dialog.ClientSize.Width-1,dialog.ClientSize.Height-1);};
                Button dismiss=new Button{Text="×",FlatStyle=FlatStyle.Flat,BackColor=dialog.BackColor,ForeColor=Fg,Font=new Font("Arial",24,FontStyle.Regular,GraphicsUnit.Pixel),Bounds=new Rectangle(527,5,36,36),TabStop=false};dismiss.FlatAppearance.BorderSize=0;dismiss.Click+=delegate{dialog.Close();};dialog.Controls.Add(dismiss);dialog.CancelButton=dismiss;
                bool nonLatin = Array.IndexOf(new[] { "ar", "ja", "ko", "zh-CN", "zh-TW", "th", "ru", "uk" }, _uiLanguage) >= 0;
                Label title = new Label { Text = T("PLAYER PROFILE"), ForeColor = Yellow, Font = nonLatin ? new Font("Segoe UI",22,FontStyle.Bold,GraphicsUnit.Pixel) : NativeArt.Font(22,true,true), AutoEllipsis = true, Location = new Point(27, 18), Size = new Size(510, 38) };
                Label subtitle = new Label { Text = T("Your player name and language"), ForeColor = Muted, AutoSize = true, Location = new Point(30, 60) };
                Panel rule = new Panel { BackColor = Color.FromArgb(85,85,85), Location = new Point(30, 88), Size = new Size(510, 2) };
                Label nameLabel = new Label { Text = T("PLAYER NAME"), ForeColor = Muted, Font = new Font("Segoe UI", 9F, FontStyle.Bold), AutoSize = true, Location = new Point(30, 107) };
                TextBox name = new TextBox { Name="ProfilePlayerName",Text = _netMode&&_onlineIdentity!=null?_onlineIdentity.Name:_name.Text.Trim(),MaxLength=24,Enabled=_roomClient==null&&!_netMode,ForeColor = Fg,BackColor=Field,Font=NativeArt.Font(18),Location = new Point(30,129),Size = new Size(510,30) };
                Label languageLabel = new Label { Text = T("GAME LANGUAGE"), ForeColor = Muted, Font = new Font("Segoe UI", 9F, FontStyle.Bold), AutoSize = true, Location = new Point(30, 170) };
                ComboBox language = new NativeDarkComboBox { Name = "GameLanguage", RightToLeft = RightToLeft.No, DropDownStyle = ComboBoxStyle.DropDownList, BackColor = Field, ForeColor = Fg, FlatStyle = FlatStyle.Flat, Font = new Font("Segoe UI", 10F), Location = new Point(30, 193), Size = new Size(510, 30), MaxDropDownItems = 12 };
                language.DrawMode=DrawMode.OwnerDrawFixed;language.ItemHeight=24;
                language.DrawItem += delegate(object sender,DrawItemEventArgs e){using(var brush=new SolidBrush(Field))e.Graphics.FillRectangle(brush,e.Bounds);string label=e.Index>=0?language.GetItemText(language.Items[e.Index]):language.Text;TextRenderer.DrawText(e.Graphics,label,language.Font,e.Bounds,Fg,TextFormatFlags.VerticalCenter|TextFormatFlags.Left);if((e.State&DrawItemState.Focus)!=0)e.DrawFocusRectangle();};
                name.BorderStyle=BorderStyle.None;name.Location=new Point(40,135);name.Size=new Size(490,26);
                Panel nameBorder=new Panel{Location=new Point(30,129),Size=new Size(510,33),BackColor=Field};nameBorder.Paint+=delegate(object sender,PaintEventArgs e){using(var pen=new Pen(Color.FromArgb(122,122,126)))e.Graphics.DrawRectangle(pen,0,0,nameBorder.Width-1,nameBorder.Height-1);};name.Location=new Point(10,5);nameBorder.Controls.Add(name);dialog.Controls.Add(nameBorder);
                string[] labels = _uiLanguage == "de"
                    ? new[] { "English (Englisch)", "한국어 (Koreanisch)", "简体中文 (Chinesisch, vereinfacht)", "繁體中文 (Chinesisch, traditionell)", "العربية (Arabisch)", "Deutsch", "Español (Spanisch)", "Español (Mexiko)", "Français (Französisch)", "Italiano (Italienisch)", "Polski (Polnisch)", "Português (Portugiesisch)", "Português (Brasilien)", "Türkçe (Türkisch)", "Русский (Russisch)", "日本語 (Japanisch)", "ไทย (Thailändisch)", "Tiếng Việt (Vietnamesisch)", "Bahasa Indonesia (Indonesisch)", "Українська (Ukrainisch)", "Gaeilge (Irisch)" }
                    : new[] { "English", "한국어", "简体中文", "繁體中文", "العربية", "Deutsch", "Español", "Español (México)", "Français", "Italiano", "Polski", "Português", "Português (Brasil)", "Türkçe", "Русский", "日本語", "ไทย", "Tiếng Việt", "Bahasa Indonesia", "Українська", "Gaeilge" };
                language.Items.Add(new LanguageOption(T("Use game setting"), ""));
                for (int i = 0; i < PlayerProfile.GameLanguages.Length; i++) language.Items.Add(new LanguageOption(labels[i], PlayerProfile.GameLanguages[i]));
                language.SelectedIndex = 0;
                for (int i = 1; i < language.Items.Count; i++) if (((LanguageOption)language.Items[i]).Code == profile.Language) language.SelectedIndex = i;
                Label info = new Label { Text = T("Language applies when the client next starts.\nKeep PUBG2019Profile.ini when moving to another PC."), ForeColor = Muted, Font = new Font("Segoe UI", 9F), AutoEllipsis = true, Location = new Point(28, 233), Size = new Size(515, 42) };
                Label closeModeLabel = new Label { Text = SettingsCloseText.Get(_uiLanguage,0), ForeColor = Muted, Font = new Font("Segoe UI", 9F, FontStyle.Bold), AutoSize = true, Location = new Point(30, 278) };
                ComboBox closeMode = new NativeDarkComboBox { Name="CloseBehavior", RightToLeft=RightToLeft.No, DropDownStyle=ComboBoxStyle.DropDownList, BackColor=Field, ForeColor=Fg, FlatStyle=FlatStyle.Flat, Font=new Font("Segoe UI",10F), Location=new Point(215,274), Size=new Size(325,30) };
                closeMode.Items.Add(SettingsCloseText.Get(_uiLanguage,1)); closeMode.Items.Add(SettingsCloseText.Get(_uiLanguage,2)); closeMode.SelectedIndex=profile.CloseToTray?0:1;
                Button import = VoteStyleProfileButton(SettingsImportText.Get(_uiLanguage,0), new Point(292,440), new Size(248,36), false);
                import.Name="ImportSettings";
                import.Click += delegate {
                    using (OpenFileDialog picker = new OpenFileDialog { Title=SettingsImportText.Get(_uiLanguage,1), Filter="GameUserSettings.ini|GameUserSettings.ini|INI files (*.ini)|*.ini", CheckFileExists=true, Multiselect=false }) {
                        string modern=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),@"TslGame\Saved\Config\WindowsNoEditor");
                        if(Directory.Exists(modern))picker.InitialDirectory=modern;
                        if(picker.ShowDialog(dialog)!=DialogResult.OK)return;
                        try {var result=SettingsImport.Import(picker.FileName,_dir);string message=String.Format(CultureInfo.CurrentCulture,SettingsImportText.Get(_uiLanguage,3),result.Settings,result.Mouse,result.Bindings,result.Skipped);if(result.NeedExistingControls)message+="\n\n"+SettingsImportText.Get(_uiLanguage,4);MessageBox.Show(dialog,message,SettingsImportText.Get(_uiLanguage,2),MessageBoxButtons.OK,MessageBoxIcon.Information);}
                        catch(Exception ex){MessageBox.Show(dialog,ex.Message,SettingsImportText.Get(_uiLanguage,2),MessageBoxButtons.OK,MessageBoxIcon.Error);}
                    }
                };
                Label masterLabel=new Label{Name="MasterServerLabel",Text=T("MASTER SERVERS (ONE PER LINE; FIRST FOR HOSTING)"),ForeColor=Muted,Font=new Font("Segoe UI",9F,FontStyle.Bold),AutoSize=true,Location=new Point(30,328)};
                TextBox master=new TextBox{Name="MasterServerAddress",Text=NetDirectoryAddressText(),Enabled=_roomClient==null,ForeColor=Fg,BackColor=Field,Font=NativeArt.Font(17),Location=new Point(30,349),Size=new Size(387,72),Multiline=true,AcceptsReturn=true,ScrollBars=ScrollBars.Vertical,MaxLength=2048};
                Button masterDefault=VoteStyleProfileButton(T("DEFAULT"),new Point(427,347),new Size(113,32),false);masterDefault.Name="MasterServerDefault";masterDefault.Enabled=master.Enabled;masterDefault.Click+=delegate{master.Text=DefaultNetDirectory;};
                Button appearance = VoteStyleProfileButton(T("CHARACTER / WARDROBE"),new Point(30,440),new Size(248,36),false);
                appearance.Name="OpenAppearance";appearance.Click+=delegate{dialog.Close();try{StartWardrobe();}catch(Exception ex){LanError(ex.Message);}};
                Button close = VoteStyleProfileButton(T("SAVE"), new Point(420, 505), new Size(120, 36), true);
                close.Name = "SaveProfile";
                close.Click += delegate {
                    try {
                        string chosenName=name.Text.Trim();if(!LobbyWire.ValidName(chosenName))throw new ArgumentException("Enter 1–24 letters, numbers, underscores or hyphens.");
                        List<Uri> selectedMasters=NormalizeNetDirectories(master.Text);if(master.Enabled)SaveNetDirectoryAddresses(selectedMasters);
                        if(!_netMode){_name.Text=chosenName;PlayerProfile.SaveName(ProfilePath,chosenName);SaveLauncherValues(chosenName);}
                        PlayerProfile saved = PlayerProfile.SaveLanguage(ProfilePath, ((LanguageOption)language.SelectedItem).Code);
                        saved = PlayerProfile.SaveCloseToTray(ProfilePath, closeMode.SelectedIndex==0); _closeToTray=saved.CloseToTray;
                        _selectedLanguage = saved.Language;
                        _uiLanguage = EffectiveUiLanguage();
                        dialog.DialogResult = DialogResult.OK; dialog.Close();
                    }
                    catch (Exception ex) { MessageBox.Show(dialog, ex.Message, T("Profile error"), MessageBoxButtons.OK, MessageBoxIcon.Error); }
                };
                Button checkUpdate = VoteStyleProfileButton(T("CHECK FOR UPDATES"), new Point(30, 505), new Size(300, 36), false);
                checkUpdate.Name = "CheckUpdateTest";
                checkUpdate.Click += delegate { dialog.Close(); BeginInvoke(new Action(delegate { BeginUpdateCheck(false); })); };
                dialog.Controls.AddRange(new Control[] { title, subtitle, rule, nameLabel, languageLabel, language, info, closeModeLabel, closeMode, masterLabel, master, masterDefault, import, appearance, close, checkUpdate });
                name.BringToFront();dismiss.BringToFront();
                dialog.AcceptButton = close;
                dialog.Shown += delegate { dialog.ActiveControl = close; };
                dialog.ShowDialog(this);
            }
            ApplyLanguage();
        }
        catch (Exception ex) { MessageBox.Show(this, ex.Message, T("Profile error"), MessageBoxButtons.OK, MessageBoxIcon.Error); }
    }

    private static Button VoteStyleProfileButton(string text, Point location, Size size, bool primary)
    {
        return new VoteStyleSettingsButton(primary) { Text=text, Location=location, Size=size };
    }

    private static Button ProfileDialogButton(string text, Point location, Size size, bool primary)
    {
        Button button = new Button { Text = text, Location = location, Size = size, BackColor = primary ? Yellow : Field,
            ForeColor = primary ? Bg : Fg, FlatStyle = FlatStyle.Flat, Cursor = Cursors.Hand,
            Font = new Font("Segoe UI", 9F, FontStyle.Bold), UseVisualStyleBackColor = false };
        button.FlatAppearance.BorderColor = primary ? Yellow : Color.FromArgb(92, 98, 101);
        button.FlatAppearance.MouseOverBackColor = primary ? Color.FromArgb(255, 207, 31) : Color.FromArgb(43, 47, 49);
        while (TextRenderer.MeasureText(text, button.Font).Width > size.Width - 12 && button.Font.Size > 7F)
        {
            button.Font = new Font("Segoe UI", button.Font.Size - .5F, FontStyle.Bold);
        }
        return button;
    }

    private void HandleWindowClose() { if (_closeToTray) HideToTray(); else { _exitRequested=true; Close(); } }

    private void HideToTray()
    {
        // A hidden top-level window is already removed from the taskbar.
        // Changing ShowInTaskbar recreates its Win32 handle, which can flash
        // or re-activate a maximized launcher while it is being hidden.
        Hide();
    }

    private void RestoreFromTray()
    {
        Show();
        // Preserve a maximized window; only restore an actually minimized one.
        if (WindowState == FormWindowState.Minimized) WindowState = FormWindowState.Normal;
        Activate();
    }

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        if (!_exitRequested && _closeToTray && e.CloseReason == CloseReason.UserClosing) {
            e.Cancel = true;
            HideToTray();
        }
        base.OnFormClosing(e);
    }

    protected override void OnFormClosed(FormClosedEventArgs e)
    {
        CloseEmbeddedWardrobe();_wardrobeTimer.Stop();_wardrobeTimer.Dispose();
        DisposeLan();
        _nativeTimer.Stop(); _nativeTimer.Dispose();
        _timer.Stop();
        _timer.Dispose();
        _languageTimer.Stop();
        _languageTimer.Dispose();
        if (_trayIcon != null) { _trayIcon.Visible = false; _trayIcon.Dispose(); }
        if (_trayMenu != null) _trayMenu.Dispose();
        base.OnFormClosed(e);
    }

    protected override void WndProc(ref Message m)
    {
        const int WmNcHitTest = 0x84;
        const int HtClient = 1;
        const int HtCaption = 2;
        base.WndProc(ref m);
        if (m.Msg == Program.ActivationMessage) { RestoreFromTray(); return; }
        if (m.Msg == WmNcHitTest && m.Result.ToInt32() == HtClient) {
            long position = m.LParam.ToInt64();
            Point point = PointToClient(new Point(unchecked((short)position), unchecked((short)(position >> 16))));
            if (point.Y >= 0 && point.Y < S(200) && GetChildAtPoint(point) == null)
                m.Result = new IntPtr(HtCaption);
        }
    }

    private void BuildUi() { BuildLanUi(); }

    private int S(int value) { return (int)Math.Round(value * (ClientSize.Width / 1306.0)); }

    private static Size CalculateClientSize()
    {
        const int referenceWidth = 1306;
        const int referenceHeight = 1161;
        Rectangle workArea = Screen.PrimaryScreen.WorkingArea;
        double scale = Math.Min(1.0, Math.Min((workArea.Width - 24.0) / referenceWidth, (workArea.Height - 48.0) / referenceHeight));
        if (scale <= 0.0) scale = 1.0;
        return new Size((int)Math.Round(referenceWidth * scale), (int)Math.Round(referenceHeight * scale));
    }

    private void Place(Control control, int x, int y, int width, int height)
    {
        control.SetBounds(S(x), S(y), S(width), S(height));
    }

    private static Bitmap LoadBitmapResource(string name)
    {
        using (Stream stream = Assembly.GetExecutingAssembly().GetManifestResourceStream(name))
        {
            if (stream == null) throw new InvalidOperationException("Missing embedded image resource: " + name);
            using (Bitmap source = new Bitmap(stream)) return new Bitmap(source);
        }
    }

    private static Icon LoadIconResource(string name)
    {
        using (Stream stream = Assembly.GetExecutingAssembly().GetManifestResourceStream(name))
        {
            if (stream == null) throw new InvalidOperationException("Missing embedded icon resource: " + name);
            using (Icon source = new Icon(stream)) return (Icon)source.Clone();
        }
    }

    private Bitmap CreateButtonArtwork() {var image=new Bitmap(8,8);using(var g=Graphics.FromImage(image))g.Clear(Yellow);return image;}

    private void LoadSavedValues()
    {
        string name = ReadFirstLine(Path.Combine(_dir, "PlayerName.txt"));
        string mapValue = "Miramar";
        int botsValue = ReadBotCount(Path.Combine(_dir, "settings.ini"), 95);
        string config = Path.Combine(_dir, "PUBG2019Launcher.ini");
        if (File.Exists(config))
        {
            foreach (string line in File.ReadAllLines(config))
            {
                int split = line.IndexOf('=');
                if (split < 1) continue;
                string key = line.Substring(0, split).Trim();
                string value = line.Substring(split + 1).Trim();
                if (key.Equals("PlayerName", StringComparison.OrdinalIgnoreCase)) name = value;
                else if (key.Equals("Map", StringComparison.OrdinalIgnoreCase)) mapValue = value;
                else if (key.Equals("BotCount", StringComparison.OrdinalIgnoreCase))
                {
                    int parsed;
                    if (Int32.TryParse(value, out parsed)) botsValue = Math.Max(0, Math.Min(95, parsed));
                }
                else if (key.Equals("LocalHost", StringComparison.OrdinalIgnoreCase)) _hostLocal.Checked = value != "0";
                else if (key.Equals("AutoJoin", StringComparison.OrdinalIgnoreCase)) _autoJoinLocal.Checked = value != "0";
                else if (key.Equals("Address", StringComparison.OrdinalIgnoreCase)) _address.Text = value;
            }
        }
        _name.Text = name;
        _bots.Value = botsValue;
        for (int i = 0; i < _map.Items.Count; i++)
        {
            MapOption option = (MapOption)_map.Items[i];
            if (option.Value.Equals(mapValue, StringComparison.OrdinalIgnoreCase)) { _map.SelectedIndex = i; break; }
        }
        if (_map.SelectedIndex < 0) _map.SelectedIndex = 0;
        if (String.IsNullOrWhiteSpace(_address.Text)) _address.Text = "127.0.0.1:8888";
    }

    private static string ReadFirstLine(string path)
    {
        if (!File.Exists(path)) return "";
        using (StreamReader reader = new StreamReader(path, Encoding.UTF8, true)) return (reader.ReadLine() ?? "").Trim();
    }

    private static int ReadBotCount(string path, int fallback)
    {
        if (!File.Exists(path)) return fallback;
        bool inBots = false;
        foreach (string line in File.ReadAllLines(path))
        {
            string trimmed = line.Trim();
            if (trimmed.StartsWith("[")) inBots = trimmed.Equals("[Bots]", StringComparison.OrdinalIgnoreCase);
            else if (inBots && trimmed.StartsWith("Count=", StringComparison.OrdinalIgnoreCase))
            {
                int count;
                if (Int32.TryParse(trimmed.Substring(trimmed.IndexOf('=') + 1), out count)) return Math.Max(0, Math.Min(95, count));
            }
        }
        return fallback;
    }

    private void UpdateMode() { UpdateLanState(); }

    private void UpdateMapHint()
    {
        if (_map.SelectedItem == null) return;
        _mapHint.Text = T(!_hostLocal.Checked ? "Enter the address of the server you want to join." :
            "Choose a map and bot count for your local match.");
        _mapHint.ForeColor = Muted;
    }

    private void StartClicked(object sender, EventArgs e)
    {
        if (!_launchFromRoom) { LanPrimaryAction(); return; }
        if (_serverReady && !_joinStarted && _hostProcess != null && !_hostProcess.HasExited)
        {
            try { StartLocalClientJoin(); }
            catch (Exception ex) { AppendLog("JOIN ERROR: " + ex.Message); SetStatus("CLIENT COULD NOT START — TRY JOINING AGAIN"); _start.Enabled = true; }
            return;
        }
        if (_finished) return;
        string playerName = ActivePlayerName;
        if (!Regex.IsMatch(playerName, "^[A-Za-z0-9_-]{1,24}$"))
        {
            MessageBox.Show(this, T("Enter 1–24 English letters, numbers, underscores, or hyphens for the player name."), T("Invalid player name"), MessageBoxButtons.OK, MessageBoxIcon.Warning);
            _name.Focus();
            return;
        }
        if (!_hostLocal.Checked && !ValidAddress(_address.Text.Trim()))
        {
            MessageBox.Show(this, T("Enter a hostname or IPv4 address, optionally followed by :port (for example 192.168.1.10:8888)."), T("Invalid server address"), MessageBoxButtons.OK, MessageBoxIcon.Warning);
            _address.Focus();
            return;
        }
        try
        {
            RequireFile("TslGame.exe");
            RequireFile("Injector.exe");
            if (!_hostLocal.Checked || _autoJoinLocal.Checked)
            {
                RequireFile("Join-Server.ps1");
                RequireFile("PUBG2019PlayerName.dll");
            }
            if (_hostLocal.Checked)
            {
                RequireFile("Host-Server.ps1");
                RequireFile("PUBG2019PS-LAN.dll");
                RequireFile(Path.Combine("Defaults", "settings.ini"));
                if (HasLocalServer()) throw new InvalidOperationException("A local server is already running from this game folder.");
            }
            SaveName(playerName);
            if(!_netMode){SaveLauncherValues(playerName);PlayerProfile.SaveName(ProfilePath, playerName);}
            if (_hostLocal.Checked)
            {
                SaveBotCount((int)_bots.Value);
                SaveHostModes();
                MapOption option = (MapOption)_map.SelectedItem;
                _launchStartedAt = DateTime.UtcNow;
                _hostStartedAt = DateTime.UtcNow;
                _joinStarted = false;
                _serverReady = false;
                _stopRequested = false;
                _autoJoinRequested = _autoJoinLocal.Checked;
                _requestedMap = option.Value;
                _joinProcessHandled = false;
                _hostProcess = StartPowerShell("Host-Server.ps1", "-Map " + option.Value + " -Port " + _gamePort + " -LoadWait 10 -LauncherMode -LanMode" + (_hostZombieRule=="None" ? "" : " -ZombieRule "+_hostZombieRule), "HOST");
                _finished = true;
                _stopServer.Visible = true;
                _stopServer.Enabled = true;
                UpdateMode();
                _start.Enabled = false;
                SetStatus("STARTING SERVER — WAITING FOR MAP AND BOTS");
                _status.ForeColor = Yellow;
                AppendLog("Starting " + option.Label + " with " + _bots.Value + " bots.");
                _timer.Start();
            }
            else
            {
                _launchStartedAt = DateTime.UtcNow;
                _joinStarted = true;
                _joinProcessHandled = false;
                _joinProcess = StartPowerShell("Join-Server.ps1", "-Address " + Quote(_address.Text.Trim()) + ClientLanguageArgument() + NewClientIdentityArgument(), "JOIN");
                _finished = true;
                UpdateMode();
                _start.Enabled = false;
                SetStatus("LAUNCHING CLIENT");
                _status.ForeColor = Yellow;
                AppendLog("Joining " + _address.Text.Trim() + ".");
                _timer.Start();
            }
        }
        catch (Exception ex)
        {
            string diagnostic = LauncherPermissionText.FromException(ex, "PUBG2019Launcher.exe");
            Fail(diagnostic);
        }
    }

    private string ClientLanguageArgument()
    {
        string language = PlayerProfile.Read(ProfilePath).Language;
        return language.Length == 0 ? "" : " -Language " + Quote(language);
    }

    private void StartLocalClientJoin()
    {
        RequireFile("Join-Server.ps1");
        RequireFile("Injector.exe");
        RequireFile("PUBG2019PlayerName.dll");
        _joinProcess = StartPowerShell("Join-Server.ps1", "-Address \"127.0.0.1:" + _gamePort + "\"" + ClientLanguageArgument() + NewClientIdentityArgument(), "JOIN");
        _joinStarted = true;
        _roomJoinedGame = true;
        _joinProcessHandled = false;
        _start.Enabled = false;
        SetStatus("SERVER RUNNING");
        _status.ForeColor = Yellow;
        AppendLog("Connecting to your local server.");
    }

    private void StopServerClicked(object sender, EventArgs e)
    {
        try
        {
            string path = Path.Combine(_dir, "ServerStartupState.json");
            if (!File.Exists(path) || File.GetLastWriteTimeUtc(path) < _hostStartedAt.AddSeconds(-2))
            {
                AppendLog("Server is still initializing; try STOP SERVER again in a moment.");
                return;
            }
            string state = File.ReadAllText(path, Encoding.UTF8);
            Match idMatch = Regex.Match(state, "\\\"processId\\\"\\s*:\\s*(\\d+)");
            int pid;
            if (!idMatch.Success || !Int32.TryParse(idMatch.Groups[1].Value, out pid) ||
                !JsonValue(state, "map").Equals(_requestedMap, StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException("Cannot identify the server started by this launcher.");
            string query = "SELECT ExecutablePath, CommandLine FROM Win32_Process WHERE ProcessId=" + pid;
            bool match = false;
            using (ManagementObjectSearcher searcher = new ManagementObjectSearcher(query))
            using (ManagementObjectCollection rows = searcher.Get())
                foreach (ManagementObject row in rows)
                {
                    string exe = Convert.ToString(row["ExecutablePath"]);
                    string args = Convert.ToString(row["CommandLine"]);
                    match = !String.IsNullOrEmpty(exe) && Path.GetFullPath(exe).Equals(Path.Combine(_dir, "TslGame.exe"), StringComparison.OrdinalIgnoreCase) &&
                        !String.IsNullOrEmpty(args) && Regex.IsMatch(args, "(?i)(?:^|\\s)[A-Za-z0-9_]+_Main\\?listen(?:\\?|\\s|$)") && Regex.IsMatch(args, "(?i)(?:^|\\s)-port="+_gamePort+"(?:\\s|$)");
                }
            if (!match) throw new InvalidOperationException("The matching local server process is no longer available.");
            _stopRequested = true;
            _stopServer.Enabled = false;
            SetStatus("STOPPING SERVER");
            AppendLog("Stopping the local server.");
            using (Process server = Process.GetProcessById(pid))
            {
                if (server.StartTime.ToUniversalTime() < _hostStartedAt.AddSeconds(-2))
                    throw new InvalidOperationException("The recorded server process has been replaced.");
                server.Kill();
            }
        }
        catch (Exception ex) { _stopRequested = false; _stopServer.Enabled = true; AppendLog("STOP ERROR: " + ex.Message); }
    }

    private void HandleServerReady()
    {
        _serverReady = true;
        if (_lobbyServer != null) _lobbyServer.NotifyGameReady();
        if (_autoJoinRequested) StartLocalClientJoin();
        else
        {
            SetStatus("SERVER RUNNING");
            _status.ForeColor = Yellow;
            SetStart("JOIN MY SERVER");
            _start.Enabled = true;
            AppendLog("Server ready. Automatic joining is off; use JOIN MY SERVER whenever you are ready.");
        }
    }

    private string Quote(string value) { return "\"" + value.Replace("\"", "\\\"") + "\""; }

    private void RequireFile(string relative)
    {
        string path = Path.Combine(_dir, relative);
        if (!File.Exists(path)) throw new FileNotFoundException("Missing required file: " + relative, path);
    }

    private static bool ValidAddress(string value)
    {
        if (!Regex.IsMatch(value, "^[A-Za-z0-9][A-Za-z0-9.\\-]*(?::[0-9]{1,5})?$")) return false;
        int colon = value.LastIndexOf(':');
        if (colon < 0) return true;
        int port;
        return Int32.TryParse(value.Substring(colon + 1), out port) && port >= 1 && port <= 65535;
    }

    private bool HasLocalServer()
    {
        try
        {
            string query = "SELECT ExecutablePath, CommandLine FROM Win32_Process WHERE Name='TslGame.exe'";
            using (ManagementObjectSearcher searcher = new ManagementObjectSearcher(query))
            using (ManagementObjectCollection rows = searcher.Get())
            {
                foreach (ManagementObject row in rows)
                {
                    string path = Convert.ToString(row["ExecutablePath"]);
                    string command = Convert.ToString(row["CommandLine"]);
                    if (!String.IsNullOrEmpty(path) && Path.GetFullPath(path).Equals(Path.Combine(_dir, "TslGame.exe"), StringComparison.OrdinalIgnoreCase) &&
                        !String.IsNullOrEmpty(command) && Regex.IsMatch(command, "(?i)(?:^|\\s)[A-Za-z0-9_]+_Main\\?listen(?:\\s|$)")) return true;
                }
            }
        }
        catch { }
        return false;
    }

    private void SaveName(string name)
    {
        string path = Path.Combine(_dir, "PlayerName.txt");
        BackupOnce(path, Path.Combine(_dir, "UpdateBackups", "Launcher", "PlayerName.original.txt"));
        File.WriteAllText(path, name + Environment.NewLine, new UTF8Encoding(false));
    }

    private void SaveBotCount(int count)
    {
        string path = Path.Combine(_dir, "settings.ini");
        if (!File.Exists(path)) File.Copy(Path.Combine(_dir, "Defaults", "settings.ini"), path);
        BackupOnce(path, Path.Combine(_dir, "UpdateBackups", "Launcher", "settings.original.ini"));

        string[] original = File.ReadAllLines(path, Encoding.UTF8);
        List<string> changed = new List<string>();
        bool inBots = false, foundBots = false, foundCount = false, foundMax = false;
        foreach (string line in original)
        {
            string trimmed = line.Trim();
            if (trimmed.StartsWith("["))
            {
                if (inBots) AddMissingBotKeys(changed, count, ref foundCount, ref foundMax);
                inBots = trimmed.Equals("[Bots]", StringComparison.OrdinalIgnoreCase);
                if (inBots) foundBots = true;
                changed.Add(line);
                continue;
            }
            if (inBots && Regex.IsMatch(trimmed, "^Count\\s*=", RegexOptions.IgnoreCase))
            {
                if (!foundCount) changed.Add("Count=" + count);
                foundCount = true;
                continue;
            }
            if (inBots && Regex.IsMatch(trimmed, "^MaxBots\\s*=", RegexOptions.IgnoreCase))
            {
                if (!foundMax) changed.Add("MaxBots=" + count);
                foundMax = true;
                continue;
            }
            changed.Add(line);
        }
        if (inBots) AddMissingBotKeys(changed, count, ref foundCount, ref foundMax);
        if (!foundBots) throw new InvalidDataException("settings.ini has no [Bots] section.");
        string temp = path + ".launcher.tmp";
        File.WriteAllLines(temp, changed.ToArray(), new UTF8Encoding(false));
        File.Copy(temp, path, true);
        File.Delete(temp);
    }

    private static void AddMissingBotKeys(List<string> lines, int count, ref bool foundCount, ref bool foundMax)
    {
        if (!foundCount) { lines.Add("Count=" + count); foundCount = true; }
        if (!foundMax) { lines.Add("MaxBots=" + count); foundMax = true; }
    }

    private static void BackupOnce(string source, string backup)
    {
        if (!File.Exists(source) || File.Exists(backup)) return;
        Directory.CreateDirectory(Path.GetDirectoryName(backup));
        File.Copy(source, backup, false);
    }

    private void SaveLauncherValues(string name)
    {
        string path = Path.Combine(_dir, "PUBG2019Launcher.ini");
        string map = _map.SelectedItem == null ? "Miramar" : ((MapOption)_map.SelectedItem).Value;
        string address = _hostLocal.Checked ? "127.0.0.1:8888" : _address.Text.Trim();
        string content = LanSavedValues() + "PlayerName=" + name + Environment.NewLine +
                         "Map=" + map + Environment.NewLine +
                         "BotCount=" + (int)_bots.Value + Environment.NewLine +
                         "LocalHost=" + (_hostLocal.Checked ? "1" : "0") + Environment.NewLine +
                         "AutoJoin=" + (_autoJoinLocal.Checked ? "1" : "0") + Environment.NewLine +
                         "Address=" + address + Environment.NewLine;
        File.WriteAllText(path, content, new UTF8Encoding(false));
    }

    private Process StartPowerShell(string script, string arguments, string tag)
    {
        string ps = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows), "System32\\WindowsPowerShell\\v1.0\\powershell.exe");
        if (!File.Exists(ps)) ps = "powershell.exe";
        Process process = new Process();
        process.StartInfo = new ProcessStartInfo();
        process.StartInfo.FileName = ps;
        process.StartInfo.Arguments = "-NoLogo -NoProfile -ExecutionPolicy Bypass -File " + Quote(Path.Combine(_dir, script)) + " " + arguments;
        process.StartInfo.WorkingDirectory = _dir;
        process.StartInfo.UseShellExecute = false;
        process.StartInfo.CreateNoWindow = true;
        process.StartInfo.RedirectStandardOutput = true;
        process.StartInfo.RedirectStandardError = true;
        process.EnableRaisingEvents = true;
        process.OutputDataReceived += delegate(object sender, DataReceivedEventArgs e) { if (!String.IsNullOrEmpty(e.Data)) AppendLog(tag + ": " + e.Data); };
        process.ErrorDataReceived += delegate(object sender, DataReceivedEventArgs e) { if (!String.IsNullOrEmpty(e.Data)) AppendLog(tag + " ERROR: " + e.Data); };
        process.Exited += delegate { try { AppendLog(tag + " process exited with code " + process.ExitCode + "."); } catch { } };
        process.Start();
        process.BeginOutputReadLine();
        process.BeginErrorReadLine();
        return process;
    }

    private void TimerTick(object sender, EventArgs e)
    {
        if (_stopRequested && _hostProcess != null)
        {
            if (!_hostProcess.HasExited) return;
            SetStatus("SERVER STOPPED");
            _status.ForeColor = Yellow;
            _timer.Stop(); _finished = false; _serverReady = false; _stopRequested = false;
            _stopServer.Visible = false; _hostProcess = null;
            LanGameStopped();
            UpdateMode(); _start.Enabled = true;
            return;
        }
        if (_hostProcess != null && !_serverReady && !_joinStarted)
        {
            string statePath = Path.Combine(_dir, "ServerStartupState.json");
            if (File.Exists(statePath))
            {
                try
                {
                    FileInfo info = new FileInfo(statePath);
                    if (info.LastWriteTimeUtc >= _hostStartedAt.AddSeconds(-2))
                    {
                        string json = File.ReadAllText(statePath, Encoding.UTF8);
                        string status = JsonValue(json, "status");
                        string map = JsonValue(json, "map");
                        if (status == "failed") { Fail("Server startup failed: " + JsonValue(json, "reason")); return; }
                        if (status == "ready" && map.Equals(_requestedMap, StringComparison.OrdinalIgnoreCase))
                        {
                            try { HandleServerReady(); }
                            catch (Exception ex) { Fail("Client launch failed: " + ex.Message); }
                            return;
                        }
                    }
                }
                catch (IOException) { }
                catch (UnauthorizedAccessException) { }
            }
            if (_hostProcess.HasExited) { Fail("The host launcher exited before the server became ready. Check PUBG2019Launcher.log and PUBG2019PS_Server.log."); return; }
            if (DateTime.UtcNow - _launchStartedAt > TimeSpan.FromMinutes(4)) { Fail("Server startup timed out. Check the host log."); return; }
        }
        if (_joinStarted && !_joinProcessHandled && _joinProcess != null && _joinProcess.HasExited)
        {
            _joinProcessHandled = true;
            if (_joinProcess.ExitCode != 0) { Fail("The client join script failed with exit code " + _joinProcess.ExitCode + ". Check PUBG2019Launcher.log."); return; }
            if (_hostLocal.Checked)
            {
                SetStatus("SERVER RUNNING");
                _status.ForeColor = Yellow;
            }
            else
            {
                SetStatus("CLIENT LAUNCH REQUEST FINISHED");
                _status.ForeColor = Yellow;
                _timer.Stop();
            }
        }
        if (_hostProcess != null && _hostProcess.HasExited && (_joinStarted || _serverReady))
        {
            try {
                string path=Path.Combine(_dir,"ServerStartupState.json");
                if(File.Exists(path)&&File.GetLastWriteTimeUtc(path)>=_hostStartedAt.AddSeconds(-2)){
                    string json=File.ReadAllText(path,Encoding.UTF8);
                    if(JsonValue(json,"status")=="failed"){
                        _serverReady=false;_roomReady=false;
                        Fail("Server stopped: "+JsonValue(json,"reason"));return;
                    }
                }
            }catch(IOException){}catch(UnauthorizedAccessException){}
            SetStatus("SERVER STOPPED");
            _status.ForeColor = Yellow;
            _timer.Stop();
            _finished = false;
            _serverReady = false;
            _stopServer.Visible = false;
            _hostProcess = null;
            LanGameStopped();
            UpdateMode();
            _start.Enabled = true;
        }
    }

    private static string JsonValue(string json, string key)
    {
        Match match = Regex.Match(json, "\"" + Regex.Escape(key) + "\"\\s*:\\s*\"((?:\\\\.|[^\"\\\\])*)\"");
        return match.Success ? match.Groups[1].Value.Replace("\\\"", "\"").Replace("\\\\", "\\") : "";
    }

    private void Fail(string message)
    {
        _timer.Stop();
        SetStatus("START FAILED");
        _status.ForeColor = Color.FromArgb(230, 85, 75);
        AppendLog("ERROR: " + message);
        if(_lobbyServer!=null)try{_lobbyServer.SystemNotice(message);}catch(Exception){ }
        _finished = false;
        LanLaunchFailed();
        _stopServer.Visible = false;
        UpdateMode();
        _start.Enabled = true;
        // Headless workers must return to the room instead of waiting on a hidden dialog.
        if(!_dedicatedServer)MessageBox.Show(this, T(message), "PUBG 2019 Launcher", MessageBoxButtons.OK, MessageBoxIcon.Error);
    }

    private void AppendLog(string message)
    {
        string timestamp = DateTime.Now.ToString("HH:mm:ss") + "  ";
        string line = timestamp + message;
        try
        {
            lock (_logLock) File.AppendAllText(Path.Combine(_dir, "PUBG2019Launcher.log"), line + Environment.NewLine, new UTF8Encoding(false));
        }
        catch { }
        if (IsDisposed || !IsHandleCreated) return;
        try
        {
            BeginInvoke((MethodInvoker)delegate
            {
                _log.AppendLine(timestamp + message);
                _webLog.Add(timestamp + message); if (_webLog.Count > 500) _webLog.RemoveAt(0);
            });
        }
        catch { }
    }
}

