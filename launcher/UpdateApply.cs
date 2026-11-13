using System;
using System.IO;
using System.Diagnostics;
using System.Collections.Generic;
using System.Text;

internal static class UpdateApply
{
    private static void ApplyFiles(string destination,string stage,string backup,string[] paths)
    {
        // Repeat immediately before writes: the installation may have changed
        // since the user accepted the update.
        ReleaseGuard.RequireNewer(destination,stage);
        foreach(string required in new[]{"PUBG2019Launcher.exe","PUBG2019UpdateApply.exe","release.version"})
            if(Array.FindIndex(paths,p=>String.Equals(p,required,StringComparison.OrdinalIgnoreCase))<0)
                throw new InvalidDataException("The update file list is incomplete.");
        var changed=new List<string>(); var existed=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        try {
            foreach(string path in paths) {
                if(String.IsNullOrEmpty(path) || path.StartsWith("/") || path.Contains(":") || path.Contains("\\")) throw new InvalidDataException("Invalid path.");
                string target=destination, source=stage;
                foreach(string piece in path.Split('/')) {
                    if(piece=="." || piece==".." || piece.Length==0) throw new InvalidDataException("Invalid path.");
                    target=Path.Combine(target,piece);source=Path.Combine(source,piece);
                    if((File.Exists(target)||Directory.Exists(target)) && (File.GetAttributes(target)&FileAttributes.ReparsePoint)!=0) throw new InvalidDataException("Destination contains a link.");
                }
                string previous=Path.Combine(backup,path.Replace('/',Path.DirectorySeparatorChar))+".old";
                string temp=target+".new";
                if(File.Exists(temp)||Directory.Exists(temp))throw new IOException("A .new file already exists: "+temp);
                if(File.Exists(target)) {Directory.CreateDirectory(Path.GetDirectoryName(previous));File.Copy(target,previous,false);existed.Add(path);}
                Directory.CreateDirectory(Path.GetDirectoryName(target)); changed.Add(path); bool ownsTemp=false;
                try {
                    File.Copy(source,temp,false);ownsTemp=true;
                    if(File.Exists(target)) File.Replace(temp,target,null);
                    else File.Move(temp,target);
                } finally {if(ownsTemp && File.Exists(temp))File.Delete(temp);}
            }
        } catch(Exception ex) {
            var errors=new List<string>();
            for(int i=changed.Count-1;i>=0;i--) {
                string path=changed[i], target=Path.Combine(destination,path.Replace('/',Path.DirectorySeparatorChar));
                try {if(existed.Contains(path))File.Copy(Path.Combine(backup,path.Replace('/',Path.DirectorySeparatorChar))+".old",target,true);else if(File.Exists(target))File.Delete(target);}
                catch(Exception restore) {errors.Add(path+": "+restore.Message);}
            }
            string message="Update failed: "+ex.Message;
            if(errors.Count>0)message+="\nSome files could not be restored. Recovery folder: "+backup+"\n"+String.Join("; ",errors.ToArray());
            else if(Directory.Exists(backup))Directory.Delete(backup,true);
            throw new IOException(message,ex);
        }
    }
    private static void SaveResult(string destination,string stage,bool success,string message)
    {
        string folder=Path.Combine(destination,"UpdateBackups");Directory.CreateDirectory(folder);
        string target=Path.Combine(folder,"LauncherUpdateResult.txt");
        string temp=target+".new";
        if(File.Exists(temp))throw new IOException("Update result .new file already exists.");
        File.WriteAllText(temp,(success?"OK":"ERROR")+"\n"+stage+"\n"+message,new UTF8Encoding(false));
        if(File.Exists(target))File.Replace(temp,target,null);else File.Move(temp,target);
    }
    [STAThread]
    private static void Main(string[] args)
    {
        if(args.Length!=3)return;
        string destination=Path.GetFullPath(args[0]);string stage=Path.GetFullPath(args[1]);int pid;
        if(!Int32.TryParse(args[2],out pid))return;
        string backup=Path.Combine(stage,"PUBG2019_UpdateBackup");
        try {
            // Reject stale packages before waiting for or closing any process.
            ReleaseGuard.RequireNewer(destination,stage);
            try {
                using(var process=Process.GetProcessById(pid)) {
                    string expected=Path.Combine(destination,"PUBG2019Launcher.exe");
                    if(!String.Equals(process.MainModule.FileName,expected,StringComparison.OrdinalIgnoreCase))throw new InvalidOperationException("The launcher process changed; update cancelled.");
                    if(!process.WaitForExit(8000)){process.Kill();if(!process.WaitForExit(5000))throw new InvalidOperationException("Could not close the launcher.");}
                }
            }catch(ArgumentException){}
            foreach(var process in Process.GetProcessesByName("TslGame")){process.Dispose();throw new InvalidOperationException("Close the game and server before updating.");}
            ApplyFiles(destination,stage,backup,File.ReadAllLines(Path.Combine(stage,"file-list.txt")));
            if(Directory.Exists(backup))Directory.Delete(backup,true);
            SaveResult(destination,stage,true,"The update was installed. Your personal settings were kept.");
        } catch(Exception ex) {
            try{SaveResult(destination,stage,false,ex.Message);}catch{}
        }
        try{Process.Start(new ProcessStartInfo(Path.Combine(destination,"PUBG2019Launcher.exe")){WorkingDirectory=destination,UseShellExecute=true});}catch{}
    }
}
