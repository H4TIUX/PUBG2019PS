using System;using System.IO;using System.Text;using System.Collections.Generic;
internal sealed partial class LauncherForm {
    DateTime _nextDedicatedLogs;string[] _dedicatedLogs=new string[0];bool _dedicatedConsoleAvailable,_masterRunning,_dedicatedRunning;string _masterAdvertise="";
    readonly List<string> _dedicatedEvents=new List<string>();
    void DedicatedEvent(string message){_dedicatedEvents.Add("["+DateTime.Now.ToString("HH:mm:ss")+"] "+message);if(_dedicatedEvents.Count>60)_dedicatedEvents.RemoveAt(0);_nextDedicatedLogs=DateTime.MinValue;}
    void RefreshDedicatedLogs(){
        if(DateTime.UtcNow<_nextDedicatedLogs)return;_nextDedicatedLogs=DateTime.UtcNow.AddSeconds(2);
        var output=new List<string>();_dedicatedConsoleAvailable=DedicatedInstalled;_masterRunning=OwnedServiceRunning("master");_dedicatedRunning=OwnedServiceRunning("supervisor");_masterAdvertise=GetAdvertiseAddress();
        output.Add("CHAT: "+(OwnedServiceRunning("chat")?"PROCESS RUNNING (TCP 18901)":"STOPPED"));
        output.Add("MASTER: "+(_masterRunning?"PROCESS RUNNING (TCP 18900)":"STOPPED"));
        output.Add("DEDICATED SERVICE: "+(_dedicatedRunning?"SUPERVISOR RUNNING":"STOPPED"));
        output.AddRange(_dedicatedEvents);
        try{
            string path=Path.Combine(_dir,"Saved","QuickMatchSupervisor.path");if(!File.Exists(path)){if(!_dedicatedRunning)output.Add("Start the master server and the dedicated service when ready.");_dedicatedLogs=output.ToArray();return;}
            string root=File.ReadAllText(path,Encoding.UTF8).Trim();if(!Directory.Exists(root)){_dedicatedLogs=output.ToArray();return;}
            AppendDedicatedTail(output,Path.Combine(root,"supervisor.log"),"MANAGER",20);
            string[] dirs=Directory.GetDirectories(root,"instance-*");Array.Sort(dirs,StringComparer.OrdinalIgnoreCase);
            foreach(string dir in dirs){string bin=Path.Combine(dir,@"TslGame\Binaries\Win64");string state=Path.Combine(bin,"worker-state.txt");if(!File.Exists(state))continue;string value=File.ReadAllText(state).Trim();if(value=="stopped")continue;string portFile=Path.Combine(bin,"quick-port.txt");string port=File.Exists(portFile)?File.ReadAllText(portFile).Trim():"?";output.Add("["+port+"] "+value.ToUpperInvariant());AppendDedicatedTail(output,Path.Combine(bin,"dedicated-host.log"),port,8);AppendDedicatedTail(output,Path.Combine(bin,"PUBG2019Launcher.log"),port,15);}
            if(output.Count>160)output.RemoveRange(2,output.Count-160);
        }catch(IOException){}catch(UnauthorizedAccessException){}catch(ArgumentException){}
        _dedicatedLogs=output.ToArray();
    }
    static void AppendDedicatedTail(List<string> target,string path,string prefix,int maxLines){
        if(!File.Exists(path))return;using(var f=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete)){f.Seek(Math.Max(0,f.Length-32768),SeekOrigin.Begin);using(var reader=new StreamReader(f,Encoding.UTF8)){string[] lines=reader.ReadToEnd().Split(new[]{'\r','\n'},StringSplitOptions.RemoveEmptyEntries);for(int i=Math.Max(0,lines.Length-maxLines);i<lines.Length;i++)target.Add("["+prefix+"] "+lines[i]);}}
    }
}
