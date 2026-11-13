using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;

internal sealed partial class LauncherForm {
    private Panel _wardrobePanel,_wardrobeViewport;private Label _wardrobeLoading;private Image _wardrobeLoadingArt,_wardrobeClosingArt;
    private Process _wardrobeProcess;private IntPtr _wardrobeWindow;private string _wardrobeSession;
    private FormWindowState _wardrobePreviousWindowState;private bool _wardrobeRestoreWindowState,_wardrobeShowingClosing;
    private readonly System.Windows.Forms.Timer _wardrobeTimer=new System.Windows.Forms.Timer();private Mutex _wardrobeMutex;
    [DllImport("user32.dll")] private static extern bool MoveWindow(IntPtr window,int x,int y,int width,int height,bool repaint);
    [DllImport("user32.dll")] private static extern bool IsWindow(IntPtr window);
    [DllImport("user32.dll")] private static extern bool SetWindowPos(IntPtr window,IntPtr after,int x,int y,int width,int height,uint flags);
    [DllImport("user32.dll")] private static extern IntPtr GetParent(IntPtr window);
    [DllImport("user32.dll")] private static extern bool GetClientRect(IntPtr window,out WardrobeRect bounds);
    [StructLayout(LayoutKind.Sequential)] private struct WardrobeRect {internal int Left,Top,Right,Bottom;}
    private void StartWardrobe(){
        if(_wardrobePanel!=null){_wardrobePanel.BringToFront();return;}
        string script=Path.Combine(_dir,"CharacterWardrobe","EngineWardrobe.ps1");if(!File.Exists(script))throw new FileNotFoundException("The character editor is not installed.");
        bool created;_wardrobeMutex=new Mutex(true,@"Local\PUBG2019CharacterWardrobe",out created);if(!created){_wardrobeMutex.Dispose();_wardrobeMutex=null;throw new InvalidOperationException("The character editor is already open.");}
        try{
            // Open the embedded studio maximized regardless of launcher size.
            _wardrobePreviousWindowState=WindowState;_wardrobeRestoreWindowState=true;
            if(WindowState!=FormWindowState.Maximized)WindowState=FormWindowState.Maximized;
            _wardrobeSession=Path.Combine(_dir,"Saved","WardrobeSessions",Guid.NewGuid().ToString("N"));Directory.CreateDirectory(_wardrobeSession);
            _wardrobePanel=new Panel{Dock=DockStyle.Fill,BackColor=Bg,Name="EmbeddedWardrobe"};
            _wardrobeLoadingArt=NativeArt.Image("WardrobeLoading");_wardrobeClosingArt=NativeArt.Image("WardrobeClosing");
            _wardrobeViewport=new Panel{Dock=DockStyle.Fill,BackColor=Color.FromArgb(0,0,0),Name="WardrobeViewport"};_wardrobeLoading=new WardrobeLoadingLabel(_wardrobeLoadingArt){Text=_wardrobeLoadingArt==null?"Loading character studio…":"",Dock=DockStyle.Fill,TextAlign=ContentAlignment.MiddleCenter,Font=NativeArt.Font(22,true,true),ForeColor=Color.White};_wardrobeViewport.Controls.Add(_wardrobeLoading);
            // The editor owns the full area and provides its own EXIT button.
            _wardrobePanel.Controls.Add(_wardrobeViewport);Controls.Add(_wardrobePanel);_wardrobePanel.BringToFront();_wardrobeViewport.Resize+=delegate{ResizeEmbeddedWardrobe();};
            string args="-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "+Quote(script)+" -OwnerPid "+Process.GetCurrentProcess().Id+" -ParentWindow "+_wardrobeViewport.Handle.ToInt64()+" -SessionDirectory "+Quote(_wardrobeSession)+" -Language "+Quote(_uiLanguage);
            _wardrobeProcess=Process.Start(new ProcessStartInfo(Path.Combine(Environment.SystemDirectory,@"WindowsPowerShell\v1.0\powershell.exe"),args){WorkingDirectory=_dir,UseShellExecute=false,CreateNoWindow=true});
            _wardrobeTimer.Interval=250;_wardrobeTimer.Tick-=PollEmbeddedWardrobe;_wardrobeTimer.Tick+=PollEmbeddedWardrobe;_wardrobeTimer.Start();
        }catch{DisposeEmbeddedWardrobe();throw;}
    }
    private void PollEmbeddedWardrobe(object sender,EventArgs e){
        try{
            if(_wardrobeProcess==null)return;_wardrobeProcess.Refresh();if(_wardrobeProcess.HasExited){string error=Path.Combine(_wardrobeSession,"error.txt");if(File.Exists(error))LanError(File.ReadAllText(error));DisposeEmbeddedWardrobe();_portraitTime=DateTime.MinValue;NativePublish();if(_roomClient!=null&&_roomConnected&&!_roomStarting&&!_roomReady){_appearanceSynced=false;_roomClient.SetAppearance(SkinProfile.Read(_dir),SkinProfile.ReadPreviewGender(_dir),SkinProfile.ReadFeatures(_dir));}return;}
            bool closing=File.Exists(Path.Combine(_wardrobeSession,"cancel"))||File.Exists(Path.Combine(_wardrobeSession,"closing"));
            bool preset=File.Exists(Path.Combine(_wardrobeSession,"preset-request"));
            if(closing){if(!_wardrobeShowingClosing)ShowWardrobeClosing();KeepWardrobeCovered();string ack=Path.Combine(_wardrobeSession,"closing-covered");if(!File.Exists(ack))File.WriteAllText(ack,"1");}
            else if(preset){string value=File.ReadAllText(Path.Combine(_wardrobeSession,"preset-request")).Trim();if(value.Length>0){string ack=Path.Combine(_wardrobeSession,"preset-covered");if(!File.Exists(ack)||File.ReadAllText(ack).Trim()!=value)File.WriteAllText(ack,value);}}
            else if(!File.Exists(Path.Combine(_wardrobeSession,"ready"))){ShowWardrobeLoading();KeepWardrobeCovered();}
            string togglePath=Path.Combine(_wardrobeSession,"toggle-window.request");
            if(File.Exists(togglePath)){File.Delete(togglePath);ToggleWardrobeFullscreen();}
            if(_wardrobeWindow!=IntPtr.Zero&&!IsWindow(_wardrobeWindow))_wardrobeWindow=IntPtr.Zero;
            string windowPath=Path.Combine(_wardrobeSession,"window.txt");if(_wardrobeWindow==IntPtr.Zero&&File.Exists(windowPath)){long handle;if(Int64.TryParse(File.ReadAllText(windowPath).Trim(),out handle)){var window=new IntPtr(handle);if(IsWindow(window)&&GetParent(window)==_wardrobeViewport.Handle){_wardrobeWindow=window;ResizeEmbeddedWardrobe();if(_wardrobeLoading!=null&&_wardrobeLoading.Visible)_wardrobeLoading.BringToFront();}}}
            if(_wardrobeWindow!=IntPtr.Zero&&_wardrobeLoading!=null&&_wardrobeLoading.Visible&&!closing&&File.Exists(Path.Combine(_wardrobeSession,"ready")))_wardrobeLoading.Visible=false;
            else if(_wardrobeLoading!=null&&_wardrobeLoading.Visible)KeepWardrobeCovered();
        }catch(IOException){}catch(Exception ex){LanError(ex.Message);CloseEmbeddedWardrobe();}
    }
    private void ToggleWardrobeFullscreen(){if(_wardrobePanel==null)return;WindowState=WindowState==FormWindowState.Maximized?FormWindowState.Normal:FormWindowState.Maximized;PerformLayout();ResizeEmbeddedWardrobe();}
    protected override bool ProcessCmdKey(ref Message message,Keys keyData){
        if(_wardrobePanel!=null&&(keyData==Keys.F11||keyData==(Keys.Alt|Keys.Enter))){
            // The embedded game's own window handles these when it has focus.
            // This path covers the launcher and its loading screen only.
            if((message.LParam.ToInt64()&(1L<<30))==0)ToggleWardrobeFullscreen();
            return true;
        }
        return base.ProcessCmdKey(ref message,keyData);
    }
    private void ResizeEmbeddedWardrobe(){if(_wardrobeWindow!=IntPtr.Zero&&IsWindow(_wardrobeWindow)&&_wardrobeViewport!=null)MoveWindow(_wardrobeWindow,0,0,Math.Max(320,_wardrobeViewport.ClientSize.Width),Math.Max(240,_wardrobeViewport.ClientSize.Height),true);}
    private void KeepWardrobeCovered(){if(_wardrobeLoading==null||!_wardrobeLoading.Visible)return;const uint flags=0x0001|0x0002|0x0010; if(_wardrobeWindow!=IntPtr.Zero&&IsWindow(_wardrobeWindow))SetWindowPos(_wardrobeWindow,new IntPtr(1),0,0,0,0,flags);_wardrobeLoading.BringToFront();SetWindowPos(_wardrobeLoading.Handle,IntPtr.Zero,0,0,0,0,flags);_wardrobeLoading.Refresh();}
    private void ShowWardrobeLoading(){if(_wardrobeLoading==null||_wardrobeShowingClosing)return;if(!_wardrobeLoading.Visible){_wardrobeLoading.BackgroundImage=_wardrobeLoadingArt;_wardrobeLoading.Text=_wardrobeLoadingArt==null?"Saving character preview…":"";_wardrobeLoading.Visible=true;}}
    private void ShowWardrobeClosing(){if(_wardrobeLoading==null)return;_wardrobeShowingClosing=true;var animated=_wardrobeLoading as WardrobeLoadingLabel;if(animated!=null)animated.BeginClosingAnimation(_wardrobeClosingArt);else _wardrobeLoading.BackgroundImage=_wardrobeClosingArt;_wardrobeLoading.Text=_wardrobeClosingArt==null?"Closing character studio…":"";_wardrobeLoading.ForeColor=Color.White;_wardrobeLoading.Visible=true;KeepWardrobeCovered();}
    private void CloseEmbeddedWardrobe(){if(_wardrobeSession!=null)try{File.WriteAllText(Path.Combine(_wardrobeSession,"cancel"),"1");}catch(IOException){}ShowWardrobeClosing();}
    private void DisposeEmbeddedWardrobe(){_wardrobeTimer.Stop();_wardrobeShowingClosing=false;_wardrobeWindow=IntPtr.Zero;if(_wardrobePanel!=null){Controls.Remove(_wardrobePanel);_wardrobePanel.Dispose();_wardrobePanel=null;_wardrobeViewport=null;_wardrobeLoading=null;}
        if(_wardrobeLoadingArt!=null){_wardrobeLoadingArt.Dispose();_wardrobeLoadingArt=null;}if(_wardrobeClosingArt!=null){_wardrobeClosingArt.Dispose();_wardrobeClosingArt=null;}
        if(_wardrobeProcess!=null){_wardrobeProcess.Dispose();_wardrobeProcess=null;}if(_wardrobeMutex!=null){try{_wardrobeMutex.ReleaseMutex();}catch(ApplicationException){}_wardrobeMutex.Dispose();_wardrobeMutex=null;}
        if(_wardrobeSession!=null)try{Directory.Delete(_wardrobeSession,true);}catch(IOException){} _wardrobeSession=null;
        if(_wardrobeRestoreWindowState){_wardrobeRestoreWindowState=false;WindowState=_wardrobePreviousWindowState;}
    }
}
