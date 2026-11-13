using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

internal sealed class SettingsImportResult {
    public int Settings, Mouse, Bindings, Skipped;
    public bool NeedExistingControls;
}

internal static class SettingsImport {
    const string GameSection="/Script/TslGame.TslGameUserSettings", EngineSection="/Script/Engine.GameUserSettings", QualitySection="ScalabilityGroups";
    static readonly string[] BooleanKeys={"bUseVSync","bIsMasterSoundMute","bIsEffectSoundMute","bIsUISoundMute","bIsBGMSoundMute","bIsVoiceInputMute","bIsVoiceOutputMute","bUsePushToTalk","bUseGlobalVoice","bUseTeamVoice","bMotionBlur","bUseForceFeedback","bUseCharacterCapture","bUseFreeLookInterp"};
    static readonly string[] Volumes={"MasterSoundVolume","EffectSoundVolume","UISoundVolume","BGMSoundVolume","VoiceInputVolume","VoiceOutputVolume","Gamma"};
    static readonly string[] Dimensions={"ResolutionSizeX","ResolutionSizeY","LastUserConfirmedResolutionSizeX","LastUserConfirmedResolutionSizeY","DesiredScreenWidth","DesiredScreenHeight"};
    static readonly string[] Modes={"FullscreenMode","LastConfirmedFullscreenMode","PreferredFullscreenMode"};
    static readonly string[] InputModes={"InputModeCrouch","InputModeProne","InputModeWalk","bToggleSprint","InputModeHoldRotation","InputModeHoldBreath","InputModePeek","InputModeMap","InputModeADS"};
    static readonly string[] Quality={"sg.ViewDistanceQuality","sg.AntiAliasingQuality","sg.ShadowQuality","sg.PostProcessQuality","sg.TextureQuality","sg.EffectsQuality","sg.FoliageQuality"};
    static readonly HashSet<string> MouseNames=new HashSet<string>(new[]{"Normal","Scoping","Targeting","Scope2X","Scope3X","Scope4X","Scope6X","Scope8X","Scope15X"},StringComparer.OrdinalIgnoreCase);
    public static string Destination(string launcherDirectory) {return Path.Combine(launcherDirectory,@"Portable2019\Client\Config\WindowsNoEditor\GameUserSettings.ini");}
    static string Decode(byte[] bytes, out Encoding encoding) {
        int skip=0;
        if(bytes.Length>=2 && bytes[0]==0xff && bytes[1]==0xfe) {encoding=new UnicodeEncoding(false,true,true);skip=2;}
        else if(bytes.Length>=2 && bytes[0]==0xfe && bytes[1]==0xff) {encoding=new UnicodeEncoding(true,true,true);skip=2;}
        else if(bytes.Length>=3 && bytes[0]==0xef && bytes[1]==0xbb && bytes[2]==0xbf) {encoding=new UTF8Encoding(true,true);skip=3;}
        else encoding=new UTF8Encoding(false,true);
        return encoding.GetString(bytes,skip,bytes.Length-skip);
    }
    static string Get(string text,string section,string key) {
        string current="", result=null;
        using(var reader=new StringReader(text)) {string line; while((line=reader.ReadLine())!=null) {
            string trim=line.Trim(); if(trim.StartsWith("[")&&trim.EndsWith("]")) {current=trim.Substring(1,trim.Length-2);continue;}
            int eq=line.IndexOf('='); if(eq>0&&!trim.StartsWith(";")&&!trim.StartsWith("#")&&String.Equals(current,section,StringComparison.OrdinalIgnoreCase)&&String.Equals(line.Substring(0,eq).Trim(),key,StringComparison.OrdinalIgnoreCase)) result=line.Substring(eq+1).Trim();
        }} return result;
    }
    static string Put(string text,string section,string key,string value) {
        string newline=text.Contains("\r\n")?"\r\n":"\n"; var lines=new List<string>(Regex.Split(text,@"\r?\n"));
        if(lines.Count>0&&lines[lines.Count-1]=="")lines.RemoveAt(lines.Count-1);
        int start=-1,end=lines.Count;
        for(int i=0;i<lines.Count;i++) {string z=lines[i].Trim();if(z.StartsWith("[")&&z.EndsWith("]")) {if(start>=0){end=i;break;}if(String.Equals(z.Substring(1,z.Length-2),section,StringComparison.OrdinalIgnoreCase))start=i;}}
        if(start<0) {if(lines.Count>0&&lines[lines.Count-1]!="")lines.Add("");lines.Add("["+section+"]");lines.Add(key+"="+value);}
        else {bool written=false;for(int i=start+1;i<end;i++){string z=lines[i].Trim();int eq=z.IndexOf('=');if(eq>0&&!z.StartsWith(";")&&String.Equals(z.Substring(0,eq).Trim(),key,StringComparison.OrdinalIgnoreCase)){if(!written){lines[i]=key+"="+value;written=true;}else{lines.RemoveAt(i);i--;end--;}}}if(!written)lines.Insert(end,key+"="+value);}
        return String.Join(newline,lines.ToArray())+newline;
    }
    static List<string> SplitTuple(string text) {
        text=(text??"").Trim(); if(text=="")return new List<string>();
        if(text.Length<2||text[0]!='('||text[text.Length-1]!=')')throw new FormatException("Invalid input settings tuple.");
        var result=new List<string>();int depth=0,start=1;bool quoted=false,escape=false;
        for(int i=1;i<text.Length-1;i++) {char c=text[i];if(escape){escape=false;continue;}if(c=='\\'&&quoted){escape=true;continue;}if(c=='"'){quoted=!quoted;continue;}if(!quoted){if(c=='(')depth++;else if(c==')'){depth--;if(depth<0)throw new FormatException();}else if(c==','&&depth==0){result.Add(text.Substring(start,i-start).Trim());start=i+1;}}}
        if(quoted||depth!=0)throw new FormatException();string last=text.Substring(start,text.Length-1-start).Trim();if(last.Length>0)result.Add(last);return result;
    }
    static Dictionary<string,string> Fields(string tuple) {
        var d=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);foreach(string part in SplitTuple(tuple)){int p=part.IndexOf('=');if(p<=0)throw new FormatException();d[part.Substring(0,p).Trim()]=part.Substring(p+1).Trim();}return d;
    }
    static string SetTupleField(string tuple,string key,string value) {
        var parts=SplitTuple(tuple);bool found=false;for(int i=0;i<parts.Count;i++){int eq=parts[i].IndexOf('=');if(eq>0&&String.Equals(parts[i].Substring(0,eq).Trim(),key,StringComparison.OrdinalIgnoreCase)){parts[i]=key+"="+value;found=true;break;}}if(!found)parts.Add(key+"="+value);return "("+String.Join(",",parts.ToArray())+")";
    }
    static string ReadSource(string source,string key) { return Get(source,GameSection,key)??Get(source,EngineSection,key); }
    static bool Number(string v,double min,double max,out double value){return Double.TryParse(v,NumberStyles.Float,CultureInfo.InvariantCulture,out value)&&!Double.IsNaN(value)&&!Double.IsInfinity(value)&&value>=min&&value<=max;}
    static string SafeKeys(string value) {
        if(value==null||value.Length>1000)return null;
        try {var keys=SplitTuple(value);if(keys.Count<1||keys.Count>2)return null;var output=new List<string>();foreach(string key in keys){var f=Fields(key);var parts=new List<string>();foreach(var pair in f){if(pair.Key.Equals("Key",StringComparison.OrdinalIgnoreCase)){if(!Regex.IsMatch(pair.Value,@"^[A-Za-z0-9_]+$"))return null;parts.Add("Key="+pair.Value);}else if(pair.Key.Equals("bShift",StringComparison.OrdinalIgnoreCase)||pair.Key.Equals("bCtrl",StringComparison.OrdinalIgnoreCase)||pair.Key.Equals("bAlt",StringComparison.OrdinalIgnoreCase)){if(!pair.Value.Equals("True",StringComparison.OrdinalIgnoreCase)&&!pair.Value.Equals("False",StringComparison.OrdinalIgnoreCase))return null;parts.Add(pair.Key+"="+(pair.Value.Equals("True",StringComparison.OrdinalIgnoreCase)?"True":"False"));}else return null;}output.Add("("+String.Join(",",parts.ToArray())+")");}return "("+String.Join(",",output.ToArray())+")";}
        catch(FormatException){return null;}
    }
    static string BindingId(string name,string scale,bool axis) {
        string clean=(name??"").Trim().Trim('"');if(!Regex.IsMatch(clean,@"^[A-Za-z0-9_]+$"))return null;
        if(!axis)return clean;
        double n;if(!Double.TryParse(scale,NumberStyles.Float,CultureInfo.InvariantCulture,out n)||Double.IsInfinity(n)||Double.IsNaN(n))return null;
        string id=clean+"|"+n.ToString("G9",CultureInfo.InvariantCulture);return id;
    }
    static string MergeBindings(string sourceList,string targetList,string nameKey,bool axis,ref int count,ref int skipped) {
        if(String.IsNullOrWhiteSpace(sourceList))return targetList;
        var targetIds=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach(string item in SplitTuple(targetList)){var f=Fields(item);string name,scale="";if(f.TryGetValue(nameKey,out name)&&(!axis||f.TryGetValue("Scale",out scale))){string id=BindingId(name,scale,axis);if(id!=null)targetIds.Add(id);}}
        var source=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
        foreach(string item in SplitTuple(sourceList)){var f=Fields(item);string name,keys,scale="";if(!f.TryGetValue(nameKey,out name)||!f.TryGetValue("Keys",out keys)||(axis&&!f.TryGetValue("Scale",out scale))){skipped++;continue;}string id=BindingId(name,scale,axis);keys=SafeKeys(keys);if(id==null||keys==null||!targetIds.Contains(id)){skipped++;continue;}source[id]=keys;}
        var output=new List<string>();foreach(string item in SplitTuple(targetList)){string updated=item;var f=Fields(item);string name,scale="",keys;if(f.TryGetValue(nameKey,out name)&&(!axis||f.TryGetValue("Scale",out scale))){string id=BindingId(name,scale,axis);if(id!=null&&source.TryGetValue(id,out keys)){updated=SetTupleField(item,"Keys",keys);source.Remove(id);count++;}}output.Add(updated);}
        return "("+String.Join(",",output.ToArray())+")";
    }
    static string MergeMouseMap(string sourceMap,string targetMap,ref int count) {
        var source=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
        foreach(string entry in SplitTuple(sourceMap)) {
            var pair=SplitTuple(entry);if(pair.Count!=2 || !pair[0].Equals("Mouse",StringComparison.OrdinalIgnoreCase))continue;
            string array;if(!Fields(pair[1]).TryGetValue("Array",out array))continue;
            foreach(string item in SplitTuple(array)) {
                var fields=Fields(item);string name,value,converted;double number;
                if(fields.TryGetValue("SensitiveName",out name) && MouseNames.Contains(name.Trim('"')) &&
                   fields.TryGetValue("Sensitivity",out value) && Number(value,0,100,out number) &&
                   fields.TryGetValue("LastConvertedSensitivity",out converted) && Number(converted,0,100,out number))
                    source[name.Trim('"')]=item;
            }
        }
        var entries=SplitTuple(targetMap);
        for(int i=0;i<entries.Count;i++) {
            var pair=SplitTuple(entries[i]);if(pair.Count!=2 || !pair[0].Equals("Mouse",StringComparison.OrdinalIgnoreCase))continue;
            string array;if(!Fields(pair[1]).TryGetValue("Array",out array))continue;
            var items=SplitTuple(array);
            for(int j=0;j<items.Count;j++) {
                string name,replacement;var dst=Fields(items[j]);
                if(!dst.TryGetValue("SensitiveName",out name) || !source.TryGetValue(name.Trim('"'),out replacement))continue;
                var src=Fields(replacement);items[j]=SetTupleField(items[j],"Sensitivity",src["Sensitivity"]);
                items[j]=SetTupleField(items[j],"LastConvertedSensitivity",src["LastConvertedSensitivity"]);count++;
            }
            entries[i]="("+pair[0]+","+SetTupleField(pair[1],"Array","("+String.Join(",",items.ToArray())+")")+")";
        }
        return "("+String.Join(",",entries.ToArray())+")";
    }
    static string MergeMouseList(string sourceList,string targetList,ref int count) {
        var source=new Dictionary<string,Dictionary<string,string>>(StringComparer.OrdinalIgnoreCase);
        foreach(string item in SplitTuple(sourceList)) {
            var fields=Fields(item);string name,value,converted;double number;
            if(fields.TryGetValue("MouseSensitiveName",out name) && MouseNames.Contains(name.Trim('"')) &&
               fields.TryGetValue("MouseSensitivity",out value) && Number(value,0,100,out number) &&
               fields.TryGetValue("LastConvertedMouseSensitivity",out converted) && Number(converted,0,100,out number))
                source[name.Trim('"')]=fields;
        }
        var items=SplitTuple(targetList);
        for(int i=0;i<items.Count;i++) {
            string name;Dictionary<string,string> src;var dst=Fields(items[i]);
            if(!dst.TryGetValue("MouseSensitiveName",out name) || !source.TryGetValue(name.Trim('"'),out src))continue;
            items[i]=SetTupleField(items[i],"MouseSensitivity",src["MouseSensitivity"]);
            items[i]=SetTupleField(items[i],"LastConvertedMouseSensitivity",src["LastConvertedMouseSensitivity"]);count++;
        }
        return "("+String.Join(",",items.ToArray())+")";
    }
    static string MergeControls(string source,string target,SettingsImportResult result) {
        string custom=Get(source,GameSection,"CustomInputSettins"), existing=Get(target,GameSection,"CustomInputSettins");
        if(String.IsNullOrWhiteSpace(custom))return target;
        bool hadExisting=!String.IsNullOrWhiteSpace(existing);
        if(!hadExisting){result.NeedExistingControls=true;return target;}
        string originalControls=existing;
        try {
            var src=Fields(custom);var dst=Fields(existing);string val,old;
            foreach(var pair in new[]{new[]{"ActionKeyList","ActionName"},new[]{"AxisKeyList","AxisName"}}) if(src.TryGetValue(pair[0],out val)&&dst.TryGetValue(pair[0],out old)){int n=0,skipped=0;string merged=MergeBindings(val,old,pair[1],pair[0]=="AxisKeyList",ref n,ref skipped);result.Skipped+=skipped;if(n>0){existing=SetTupleField(existing,pair[0],merged);result.Bindings+=n;}}
            if(src.TryGetValue("bInvertMouse",out val)&&(val.Equals("true",StringComparison.OrdinalIgnoreCase)||val.Equals("false",StringComparison.OrdinalIgnoreCase)))existing=SetTupleField(existing,"bInvertMouse",val.Equals("true",StringComparison.OrdinalIgnoreCase)?"True":"False");
            if(src.TryGetValue("MouseSensitiveList",out val)&&dst.TryGetValue("MouseSensitiveList",out old)&&!String.IsNullOrWhiteSpace(old)) {
                int n=0;string merged=MergeMouseList(val,old,ref n);
                if(n>0){existing=SetTupleField(existing,"MouseSensitiveList",merged);result.Mouse+=n;}
            }
            if(src.TryGetValue("SensitiveMap",out val)&&dst.TryGetValue("SensitiveMap",out old)) {
                int n=0;string merged=MergeMouseMap(val,old,ref n);
                if(n>0){existing=SetTupleField(existing,"SensitiveMap",merged);result.Mouse+=n;}
            }
            return existing==originalControls ? target : Put(target,GameSection,"CustomInputSettins",existing);
        } catch(FormatException){result.Skipped++;return target;}
    }
    public static SettingsImportResult Import(string sourceFile,string launcherDirectory) {
        string src=Path.GetFullPath(sourceFile),dest=Path.GetFullPath(Destination(launcherDirectory));if(String.Equals(src,dest,StringComparison.OrdinalIgnoreCase))throw new InvalidOperationException("Choose the other PUBG installation's GameUserSettings.ini.");
        string portableClient=Path.GetFullPath(Path.Combine(launcherDirectory,"TslGame.exe"));
        foreach(Process process in Process.GetProcessesByName("TslGame")) using(process) {
            try {if(String.Equals(process.MainModule.FileName,portableClient,StringComparison.OrdinalIgnoreCase))throw new InvalidOperationException("Close the 2019 game before importing its settings.");}
            catch(System.ComponentModel.Win32Exception){throw new InvalidOperationException("Cannot verify whether the 2019 game is running; close it and retry.");}
            catch(InvalidOperationException ex){if(ex.Message.StartsWith("Close the 2019",StringComparison.Ordinal))throw;}
        }
        var info=new FileInfo(src);if(!info.Exists||info.Length>5*1024*1024)throw new InvalidOperationException("The selected settings file is missing or too large.");
        byte[] sourceBytes=File.ReadAllBytes(src); Encoding sourceEncoding;string source=Decode(sourceBytes,out sourceEncoding);
        if(!Regex.IsMatch(source,@"(?im)^\s*\[(?:/Script/TslGame\.TslGameUserSettings|/Script/Engine\.GameUserSettings|ScalabilityGroups)\]\s*$"))throw new InvalidOperationException("This file does not contain recognizable PUBG settings.");
        if(!File.Exists(dest)) {
            string legacy=Path.GetFullPath(Path.Combine(launcherDirectory,@"..\..\Saved\Config\WindowsNoEditor\GameUserSettings.ini"));
            if(File.Exists(legacy)) {Directory.CreateDirectory(Path.GetDirectoryName(dest));try {File.Copy(legacy,dest,false);} catch(IOException){if(!File.Exists(dest))throw;}}
        }
        byte[] original=File.Exists(dest)?File.ReadAllBytes(dest):null;Encoding enc=new UTF8Encoding(false);string target=original==null?"":Decode(original,out enc), initial=target;var result=new SettingsImportResult();double num;
        Action<string,string,string> set=(section,key,newValue)=>{if(!String.Equals(Get(target,section,key),newValue,StringComparison.Ordinal)){target=Put(target,section,key,newValue);result.Settings++;}};
        foreach(string key in BooleanKeys){string v=ReadSource(source,key);if(v==null)continue;if(v.Equals("true",StringComparison.OrdinalIgnoreCase)||v.Equals("false",StringComparison.OrdinalIgnoreCase))set(GameSection,key,v.Equals("true",StringComparison.OrdinalIgnoreCase)?"True":"False");else result.Skipped++;}
        foreach(string key in Volumes){string v=ReadSource(source,key);if(v==null)continue;if(Number(v,0,100,out num))set(GameSection,key,v);else result.Skipped++;}
        foreach(string key in Dimensions){string v=ReadSource(source,key);int n;if(v==null)continue;if(Int32.TryParse(v,NumberStyles.Integer,CultureInfo.InvariantCulture,out n)&&n>=320&&n<=16384)set(GameSection,key,n.ToString(CultureInfo.InvariantCulture));else result.Skipped++;}
        foreach(string key in Modes){string v=ReadSource(source,key);int n;if(v==null)continue;if(Int32.TryParse(v,out n)&&n>=0&&n<=2)set(GameSection,key,n.ToString(CultureInfo.InvariantCulture));else result.Skipped++;}
        foreach(string key in InputModes){string v=ReadSource(source,key);if(v==null)continue;if(v.Equals("Toggle",StringComparison.OrdinalIgnoreCase)||v.Equals("Hold",StringComparison.OrdinalIgnoreCase)||v.Equals("DoubleTap",StringComparison.OrdinalIgnoreCase))set(GameSection,key,v);else result.Skipped++;}
        foreach(string key in Quality){string v=Get(source,QualitySection,key);int n;if(v==null)continue;if(Int32.TryParse(v,out n)&&n>=0&&n<=4)set(QualitySection,key,n.ToString(CultureInfo.InvariantCulture));else result.Skipped++;}
        string value=Get(source,QualitySection,"sg.ResolutionQuality");if(value!=null){if(Number(value,25,100,out num))set(QualitySection,"sg.ResolutionQuality",value);else result.Skipped++;}
        value=ReadSource(source,"ScreenScale");if(value!=null){if(Number(value,25,120,out num))set(GameSection,"ScreenScale",value);else result.Skipped++;}
        value=ReadSource(source,"FrameRateLimit");if(value!=null){if(Number(value,0,1000,out num))set(GameSection,"FrameRateLimit",value);else result.Skipped++;}
        value=ReadSource(source,"FpsCameraFov")??ReadSource(source,"FpsCamearFov");if(value!=null){if(Number(value,80,103,out num))set(GameSection,"FpsCameraFov",value);else result.Skipped++;}
        target=MergeControls(source,target,result);
        if(target==initial) return result;
        if(!File.ReadAllBytes(src).SequenceEqual(sourceBytes))throw new IOException("The source settings changed during import; try again.");
        if((original==null&&File.Exists(dest))||(original!=null&&(!File.Exists(dest)||!File.ReadAllBytes(dest).SequenceEqual(original))))throw new IOException("The portable settings changed during import; try again.");
        Directory.CreateDirectory(Path.GetDirectoryName(dest));string temp=dest+".import-"+Guid.NewGuid().ToString("N")+".tmp";
        try {File.WriteAllText(temp,target,enc);if(original==null)File.Move(temp,dest);else {string backups=Path.Combine(launcherDirectory,@"Portable2019\Backups");Directory.CreateDirectory(backups);string backup=Path.Combine(backups,"GameUserSettings-"+DateTime.UtcNow.ToString("yyyyMMdd-HHmmss-fff",CultureInfo.InvariantCulture)+".ini");File.Replace(temp,dest,backup);}} finally {if(File.Exists(temp))File.Delete(temp);}
        return result;
    }
}
