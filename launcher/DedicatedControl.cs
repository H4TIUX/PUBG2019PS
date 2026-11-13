using System;using System.Collections.Generic;using System.IO;using System.Text;using System.Net;using System.Net.Sockets;using System.Net.NetworkInformation;using System.Diagnostics;using System.Management;
internal sealed class DedicatedOptions { internal string Name; internal int Capacity,Bots,Instances; }
internal sealed partial class LauncherForm {
    DedicatedOptions _dedicatedOptions; string _dedicatedSavedNotice="";
    string DedicatedOptionsFile {get{return Path.Combine(_dir,"Saved","QuickMatchSettings.txt");}}
    DedicatedOptions GetDedicatedOptions(){
        if(_dedicatedOptions!=null)return _dedicatedOptions;
        string n="QUICK MATCH - "+Environment.MachineName;if(n.Length>32)n=n.Substring(0,32);
        var d=new DedicatedOptions{Name=n,Capacity=100,Bots=95,Instances=8};
        try {if(File.Exists(DedicatedOptionsFile)){var lines=File.ReadAllLines(DedicatedOptionsFile,Encoding.UTF8);int cap,bots,instances;
            if(lines.Length==4 && Int32.TryParse(lines[1],out cap) && Int32.TryParse(lines[2],out bots) && Int32.TryParse(lines[3],out instances)){
                string name=Encoding.UTF8.GetString(Convert.FromBase64String(lines[0]));
                if(IsValidDedicatedOptions(name,cap,bots,instances))d=new DedicatedOptions{Name=name,Capacity=cap,Bots=bots,Instances=instances};
            }
        }}catch(IOException){}catch(UnauthorizedAccessException){}catch(FormatException){}
        return _dedicatedOptions=d;
    }
    static bool IsValidDedicatedOptions(string name,int capacity,int bots,int instances){
        if(String.IsNullOrWhiteSpace(name)||name.Length>32||capacity<1||capacity>100||bots<0||bots>95||bots>=capacity||instances<1||instances>16)return false;
        foreach(char c in name)if(Char.IsControl(c))return false;return true;
    }
    void SaveDedicatedOptions(Dictionary<string,string> values){
        string name=NativeValue(values,"dedicatedName").Trim();int cap=NativeNumber(values,"dedicatedCapacity",1,100),bots=NativeNumber(values,"dedicatedBots",0,95),instances=NativeNumber(values,"dedicatedInstances",1,16);
        if(!IsValidDedicatedOptions(name,cap,bots,instances))throw new ArgumentException("Enter a server name and leave at least one player slot free of bots.");
        Directory.CreateDirectory(Path.Combine(_dir,"Saved"));var data=new[]{Convert.ToBase64String(Encoding.UTF8.GetBytes(name)),cap.ToString(),bots.ToString(),instances.ToString()};
        File.WriteAllLines(DedicatedOptionsFile,data,new UTF8Encoding(false));_dedicatedOptions=new DedicatedOptions{Name=name,Capacity=cap,Bots=bots,Instances=instances};
        _dedicatedSavedNotice=OwnedServiceRunning("supervisor")?"Saved. Restart the service to apply changes.":"Settings saved.";AppendLog("DEDICATED: "+_dedicatedSavedNotice);
    }

    string DedicatedServicePath {get{return Path.Combine(_dir,"QuickMatchService");}}
    string DedicatedRoot {get{return Path.Combine(DedicatedServicePath,"Instances");}}
    string OwnedIdentityPath(string kind){return Path.Combine(_dir,"Saved","QuickMatch-"+kind+".pid");}
    bool DedicatedInstalled {get{return File.Exists(Path.Combine(DedicatedServicePath,"QuickSupervisor.ps1"))&&File.Exists(Path.Combine(DedicatedServicePath,"PUBG2019Launcher.exe"))&&File.Exists(Path.Combine(_dir,@"NetDirectory\PUBG2019NetDirectory.exe"));}}
    Process FindOwnedService(string kind){
        string file=OwnedIdentityPath(kind);if(!File.Exists(file))return null;
        try{string[] p=File.ReadAllText(file).Trim().Split('|');int id;long ticks;if(p.Length!=2||!Int32.TryParse(p[0],out id)||!Int64.TryParse(p[1],out ticks))return null;var process=Process.GetProcessById(id);
            if(process.HasExited||process.StartTime.ToUniversalTime().Ticks!=ticks){process.Dispose();return null;}
            string expected=kind=="chat"?Path.Combine(_dir,@"ChatServer\ChatServer.exe"):kind=="master"?Path.Combine(_dir,@"NetDirectory\PUBG2019NetDirectory.exe"):Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows),@"System32\WindowsPowerShell\v1.0\powershell.exe");
            if(!String.Equals(Path.GetFullPath(process.MainModule.FileName),Path.GetFullPath(expected),StringComparison.OrdinalIgnoreCase)){process.Dispose();return null;}
            if(kind=="supervisor"){bool valid=false;using(var search=new ManagementObjectSearcher("SELECT CommandLine FROM Win32_Process WHERE ProcessId="+id))using(var rows=search.Get())foreach(ManagementObject row in rows){string command=Convert.ToString(row["CommandLine"]);valid=!String.IsNullOrEmpty(command)&&command.IndexOf(Path.Combine(DedicatedServicePath,"QuickSupervisor.ps1"),StringComparison.OrdinalIgnoreCase)>=0;}if(!valid){process.Dispose();return null;}}
            return process;
        }catch(ArgumentException){return null;}catch(InvalidOperationException){return null;}catch(System.ComponentModel.Win32Exception){return null;}catch(IOException){return null;}catch(UnauthorizedAccessException){return null;}catch(ManagementException){return null;}
    }
    bool OwnedServiceRunning(string kind){using(var p=FindOwnedService(kind))return p!=null;}
    void SaveOwnedService(string kind,Process process){Directory.CreateDirectory(Path.Combine(_dir,"Saved"));File.WriteAllText(OwnedIdentityPath(kind),process.Id+"|"+process.StartTime.ToUniversalTime().Ticks,new UTF8Encoding(false));}
    string GetAdvertiseAddress(){string path=Path.Combine(_dir,"Saved","QuickMasterAdvertise.txt");if(File.Exists(path)){string saved=File.ReadAllText(path,Encoding.UTF8).Trim();IPAddress ip;if(IPAddress.TryParse(saved,out ip)&&ip.AddressFamily==AddressFamily.InterNetwork)return ip.ToString();}try{foreach(var nic in NetworkInterface.GetAllNetworkInterfaces())if(nic.OperationalStatus==OperationalStatus.Up)foreach(var u in nic.GetIPProperties().UnicastAddresses){var b=u.Address.GetAddressBytes();if(u.Address.AddressFamily==AddressFamily.InterNetwork&&b[0]==100&&b[1]>=64&&b[1]<=127)return u.Address.ToString();}}catch(NetworkInformationException){}return "";}
    void StartOwnedMaster(string address){
        if(!DedicatedInstalled)throw new InvalidOperationException("Dedicated server support is not installed.");if(OwnedServiceRunning("master"))throw new InvalidOperationException("The master server is already running.");IPAddress ip;if(!IPAddress.TryParse((address??"").Trim(),out ip)||ip.AddressFamily!=AddressFamily.InterNetwork||IPAddress.IsLoopback(ip))throw new ArgumentException("Enter the IP address that other players use to reach this host.");byte[] b=ip.GetAddressBytes();if(b[0]==10||(b[0]==172&&b[1]>=16&&b[1]<=31)||(b[0]==192&&b[1]==168))throw new ArgumentException("Enter the public or Tailscale IPv4 address, not the private LAN address.");
        string exe=Path.Combine(_dir,@"NetDirectory\PUBG2019NetDirectory.exe");var start=new ProcessStartInfo(exe,"--bind 0.0.0.0 --port 18900 --local-advertise "+ip){UseShellExecute=false,CreateNoWindow=true,WorkingDirectory=Path.GetDirectoryName(exe)};var process=Process.Start(start);try{if(process.WaitForExit(500))throw new InvalidOperationException("Master server could not start; check whether TCP port 18900 is already in use.");SaveOwnedService("master",process);Directory.CreateDirectory(Path.Combine(_dir,"Saved"));File.WriteAllText(Path.Combine(_dir,"Saved","QuickMasterAdvertise.txt"),ip.ToString(),new UTF8Encoding(false));AppendLog("MASTER: starting on TCP 18900.");}catch{try{if(!process.HasExited)process.Kill();}catch{}throw;}finally{process.Dispose();}
    }
    void StopOwnedMaster(){using(var process=FindOwnedService("master")){if(process==null)throw new InvalidOperationException("The launcher-owned master server is not running.");process.Kill();process.WaitForExit(3000);}try{File.Delete(OwnedIdentityPath("master"));}catch(IOException){}AppendLog("MASTER: stopped.");}
    void StartOwnedChat(){string exe=Path.Combine(_dir,@"ChatServer\ChatServer.exe");if(!File.Exists(exe))throw new InvalidOperationException("Chat Server is not installed.");if(OwnedServiceRunning("chat"))throw new InvalidOperationException("Chat Server is already running.");var p=Process.Start(new ProcessStartInfo(exe,"--bind 0.0.0.0 --port 18901"){UseShellExecute=false,CreateNoWindow=true,WorkingDirectory=Path.GetDirectoryName(exe)});try{if(p==null||p.WaitForExit(500))throw new InvalidOperationException("Chat Server failed to start; check TCP port 18901.");SaveOwnedService("chat",p);}finally{if(p!=null)p.Dispose();}}
    void StopOwnedChat(){using(var p=FindOwnedService("chat")){if(p==null)throw new InvalidOperationException("Chat Server is not running.");p.Kill();p.WaitForExit(3000);}try{File.Delete(OwnedIdentityPath("chat"));}catch(IOException){}}
    void StartOwnedDedicated(){
        if(!DedicatedInstalled)throw new InvalidOperationException("Dedicated server support is not installed.");if(OwnedServiceRunning("supervisor"))throw new InvalidOperationException("The dedicated service is already running.");if(!File.Exists(Path.Combine(_dir,@"Saved\PUBG2019NetCredentials.dpapi")))throw new InvalidOperationException("Sign in to NET and select Remember login before starting the dedicated service.");
        Directory.CreateDirectory(DedicatedRoot);string stop=Path.Combine(DedicatedRoot,"stop-supervisor.flag");if(File.Exists(stop))File.Delete(stop);
        string ps=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows),@"System32\WindowsPowerShell\v1.0\powershell.exe");string script=Path.Combine(DedicatedServicePath,"QuickSupervisor.ps1");
        string directory=OwnedServiceRunning("master")?"http://127.0.0.1:18900/":NetDirectoryAddress().AbsoluteUri;
        var options=GetDedicatedOptions();string encodedName=Convert.ToBase64String(Encoding.UTF8.GetBytes(options.Name));
        string arguments="-NoLogo -NoProfile -ExecutionPolicy Bypass -File "+Quote(script)+" -Root "+Quote(DedicatedRoot)+" -Payload "+Quote(DedicatedServicePath)+" -GameDir "+Quote(_dir)+" -DirectoryUrl "+Quote(directory)+" -DisplayNameBase64 "+Quote(encodedName)+" -Capacity "+options.Capacity+" -BotCount "+options.Bots+" -MaxInstances "+options.Instances;
        var start=new ProcessStartInfo(ps,arguments){UseShellExecute=false,CreateNoWindow=true,WorkingDirectory=DedicatedServicePath};var process=Process.Start(start);
        try{if(process.WaitForExit(500))throw new InvalidOperationException("Dedicated service exited during startup (code "+process.ExitCode+"). Check supervisor.log.");SaveOwnedService("supervisor",process);Directory.CreateDirectory(Path.Combine(_dir,"Saved"));File.WriteAllText(Path.Combine(_dir,@"Saved\QuickMatchSupervisor.path"),DedicatedRoot,new UTF8Encoding(false));AppendLog("DEDICATED: starting Quick Match service.");}catch{try{if(!process.HasExited)process.Kill();}catch{}throw;}finally{process.Dispose();}
    }
    void StopOwnedDedicated(){using(var process=FindOwnedService("supervisor")){if(process==null)throw new InvalidOperationException("The launcher-owned dedicated service is not running.");Directory.CreateDirectory(DedicatedRoot);File.WriteAllText(Path.Combine(DedicatedRoot,"stop-supervisor.flag"),"stop",Encoding.ASCII);}AppendLog("DEDICATED: stopping Quick Match service.");}
}
