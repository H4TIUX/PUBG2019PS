using System;using System.Diagnostics;using System.IO;using System.Threading;using System.Windows.Forms;
// Only a client launched by this launcher, identified by PID, creation time and executable path, may be closed.
internal static class OwnedMatchClient {
 internal static bool Close(string identityFile,string expectedExe){
  if(String.IsNullOrEmpty(identityFile)||!File.Exists(identityFile))return false;
  try{string[] parts=File.ReadAllText(identityFile).Trim().Split('|');int pid;long ticks;if(parts.Length!=2||!Int32.TryParse(parts[0],out pid)||pid<=0||!Int64.TryParse(parts[1],out ticks))return false;
   using(var p=Process.GetProcessById(pid)){if(p.HasExited||p.StartTime.ToUniversalTime().Ticks!=ticks||!String.Equals(Path.GetFullPath(p.MainModule.FileName),Path.GetFullPath(expectedExe),StringComparison.OrdinalIgnoreCase))return false;
    try{p.CloseMainWindow();}catch(InvalidOperationException){}if(!p.WaitForExit(2500)){if(!p.HasExited&&p.StartTime.ToUniversalTime().Ticks==ticks&&String.Equals(Path.GetFullPath(p.MainModule.FileName),Path.GetFullPath(expectedExe),StringComparison.OrdinalIgnoreCase)){p.Kill();p.WaitForExit(2000);}}return p.HasExited;}
  }catch(ArgumentException){return false;}catch(InvalidOperationException){return false;}catch(System.ComponentModel.Win32Exception){return false;}catch(IOException){return false;}catch(UnauthorizedAccessException){return false;}finally{try{File.Delete(identityFile);}catch(IOException){}catch(UnauthorizedAccessException){}}
 }
}
internal sealed partial class LauncherForm {
 string _clientIdentityPath;System.Windows.Forms.Timer _matchCloseTimer;
 string NewClientIdentityArgument(){Directory.CreateDirectory(Path.Combine(_dir,"Saved"));_clientIdentityPath=Path.Combine(_dir,"Saved","client-session-"+Guid.NewGuid().ToString("N")+".txt");return " -ClientIdentityFile "+Quote(_clientIdentityPath);}
 void CloseClientAfterMatch(){if(_matchCloseTimer!=null||String.IsNullOrEmpty(_clientIdentityPath))return;string identity=_clientIdentityPath;_clientIdentityPath=null;_webNotice="Match ended. Returning to launcher...";_webNoticeRevision++;NativePublish();_matchCloseTimer=new System.Windows.Forms.Timer();_matchCloseTimer.Interval=3000;_matchCloseTimer.Tick+=delegate{var timer=_matchCloseTimer;_matchCloseTimer=null;timer.Stop();timer.Dispose();string expected=Path.Combine(_dir,"TslGame.exe");ThreadPool.QueueUserWorkItem(delegate{bool closed=OwnedMatchClient.Close(identity,expected);LanUi(delegate{if(closed){_roomJoinedGame=false;_finished=false;_joinStarted=false;_webNotice="Match ended.";_webNoticeRevision++;NativePublish();}});});};_matchCloseTimer.Start();}
}
