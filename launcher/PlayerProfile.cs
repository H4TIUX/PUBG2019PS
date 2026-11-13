using System;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using System.Security.Cryptography;
using System.Threading;
using System.Collections.Generic;

internal sealed class PlayerProfile
{
    public readonly string Name;
    public readonly string Language;
    public readonly bool CloseToTray;
    private PlayerProfile(string name,string language,bool closeToTray) { Name=name; Language=language; CloseToTray=closeToTray; }
    public static readonly string[] GameLanguages = { "en", "ko", "zh-CN", "zh-TW", "ar", "de", "es", "es-MX", "fr", "it", "pl", "pt", "pt-BR", "tr", "ru", "ja", "th", "vi", "id", "uk", "ga-IE" };
    private static string ValidLanguage(string language) {
        if(String.IsNullOrEmpty(language)) return "";
        foreach(string value in GameLanguages) if(value.Equals(language,StringComparison.OrdinalIgnoreCase)) return value;
        throw new InvalidDataException("The saved game language is not supported by this launcher.");
    }
    private static string ValidName(string name) {
        name=(name??"").Trim(); return Regex.IsMatch(name,"^[A-Za-z0-9_-]{1,24}$") ? name : "";
    }
    private static T Locked<T>(string path,Func<T> action) {
        string key;using(var sha=SHA256.Create()) key=BitConverter.ToString(sha.ComputeHash(Encoding.UTF8.GetBytes(Path.GetFullPath(path).ToUpperInvariant()))).Replace("-","");
        using(var mutex=new Mutex(false,@"Local\PUBG2019Profile_"+key)) {try{mutex.WaitOne();}catch(AbandonedMutexException){} try{return action();}finally{mutex.ReleaseMutex();}}
    }
    private static PlayerProfile ReadUnlocked(string path) {
        string name="",language="";bool closeToTray=true;
        if(File.Exists(path)) foreach(string line in File.ReadAllLines(path,Encoding.UTF8)) {
            int p=line.IndexOf('=');if(p<0)continue;string key=line.Substring(0,p).Trim();string value=line.Substring(p+1).Trim();
            if(key.Equals("PlayerName",StringComparison.OrdinalIgnoreCase))name=value;
            else if(key.Equals("GameLanguage",StringComparison.OrdinalIgnoreCase))language=value;
            else if(key.Equals("CloseAction",StringComparison.OrdinalIgnoreCase))closeToTray=!value.Equals("Exit",StringComparison.OrdinalIgnoreCase);
        }
        return new PlayerProfile(ValidName(name),ValidLanguage(language),closeToTray);
    }
    private static void SetValue(string path,string key,string value) {
        var lines=new List<string>();bool replaced=false;
        if(File.Exists(path)) foreach(string line in File.ReadAllLines(path,Encoding.UTF8)) {
            int p=line.IndexOf('=');
            if(p>=0 && line.Substring(0,p).Trim().Equals(key,StringComparison.OrdinalIgnoreCase)) {if(!replaced)lines.Add(key+"="+value);replaced=true;}
            else lines.Add(line);
        }
        if(!replaced) lines.Add(key+"="+value);
        string temp=path+"."+Guid.NewGuid().ToString("N")+".tmp";
        try {File.WriteAllLines(temp,lines.ToArray(),new UTF8Encoding(false));if(File.Exists(path))File.Replace(temp,path,null);else File.Move(temp,path);}
        finally {if(File.Exists(temp))File.Delete(temp);}
    }
    public static PlayerProfile Read(string path) {return Locked(path,delegate{return ReadUnlocked(path);});}
    public static PlayerProfile LoadOrCreate(string path,string name) {return Locked(path,delegate{if(!File.Exists(path))SetValue(path,"GameLanguage","");return ReadUnlocked(path);});}
    public static PlayerProfile SaveName(string path,string name) {return Locked(path,delegate{string valid=ValidName(name);if(valid.Length>0)SetValue(path,"PlayerName",valid);else if(!File.Exists(path))SetValue(path,"GameLanguage","");return ReadUnlocked(path);});}
    public static PlayerProfile SaveCloseToTray(string path,bool value) {return Locked(path,delegate{SetValue(path,"CloseAction",value?"Tray":"Exit");return ReadUnlocked(path);});}
    public static PlayerProfile SaveLanguage(string path,string language) {return Locked(path,delegate{SetValue(path,"GameLanguage",ValidLanguage(language));return ReadUnlocked(path);});}
}
