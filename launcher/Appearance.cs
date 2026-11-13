using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Text;
using System.Windows.Forms;

internal sealed class SkinEntry
{
    internal string Category,Id;
    internal string Label { get { return Id.Replace("Item_Skin_","").Replace("Item_","").Replace('_',' '); } }
    public override string ToString(){return Category+": "+Label;}
}
internal static class SkinProfile
{
    internal static readonly Dictionary<string,SkinEntry> Catalog=ReadCatalog();
    static Dictionary<string,SkinEntry> ReadCatalog(){var entries=new Dictionary<string,SkinEntry>(StringComparer.Ordinal);
        using(var stream=Assembly.GetExecutingAssembly().GetManifestResourceStream("Launcher.SkinCatalog")){
            if(stream==null)return entries;using(var reader=new StreamReader(stream,Encoding.UTF8)){string line;while((line=reader.ReadLine())!=null){var p=line.Split('\t');if(p.Length==3&&!p[1].StartsWith("Item_Ghillie_",StringComparison.OrdinalIgnoreCase))entries[p[1]]=new SkinEntry {Category=p[0],Id=p[1]};}}
        }return entries;
    }
    internal static bool Valid(string selection){if(selection==null||selection.Length>3500)return false;if(selection.Length==0)return true;var ids=selection.Split(',');if(ids.Length>64)return false;var seen=new HashSet<string>();foreach(string id in ids)if(!Catalog.ContainsKey(id)||!seen.Add(id))return false;return true;}
    internal static string Read(string dir){string path=Path.Combine(dir,"PUBG2019Appearance.ini");if(!File.Exists(path))return "";foreach(string line in File.ReadAllLines(path))if(line.StartsWith("Items=",StringComparison.Ordinal)){var kept=new List<string>();var seen=new HashSet<string>();foreach(string id in line.Substring(6).Split(',')){if(Catalog.ContainsKey(id)&&seen.Add(id)&&kept.Count<64)kept.Add(id);}string selection=String.Join(",",kept.ToArray());return Valid(selection)?selection:"";}return "";}
    internal static bool ValidFeatures(string value){
        if(value==null||value.Length>1000)return false;if(value.Length==0)return true;
        var seen=new HashSet<string>(StringComparer.Ordinal);
        foreach(string entry in value.Split(',')){var pair=entry.Split(':');if(pair.Length!=2||!FeatureGroup(pair[0])||!seen.Add(pair[0])||!System.Text.RegularExpressions.Regex.IsMatch(pair[1],@"^Item_[A-Za-z0-9_]{1,95}$")||pair[1].IndexOf("zombie",StringComparison.OrdinalIgnoreCase)>=0||pair[1].IndexOf("ghillie",StringComparison.OrdinalIgnoreCase)>=0)return false;}return true;
    }
    internal static bool FeatureGroup(string group){return group=="Hair"||group=="Face"||group=="NudeBody"||group=="Makeup"||group=="SkinColor"||group=="BodyType"||group=="BodyTattoos";}
    internal static string ReadFeatures(string dir){
        string path=Path.Combine(dir,"PUBG2019Appearance.ini");if(!File.Exists(path))return "";
        bool section=false;var entries=new List<string>();var seen=new HashSet<string>(StringComparer.Ordinal);
        foreach(string line in File.ReadAllLines(path)){if(line.StartsWith("[",StringComparison.Ordinal)){section=line=="[StudioFeatures]";continue;}if(!section)continue;int eq=line.IndexOf('=');if(eq<1)continue;string group=line.Substring(0,eq),entry=group+":"+line.Substring(eq+1);if(ValidFeatures(entry)&&seen.Add(group))entries.Add(entry);}
        string value=String.Join(",",entries.ToArray());return ValidFeatures(value)?value:"";
    }
    internal static string ReadPreviewGender(string dir){string path=Path.Combine(dir,"PUBG2019Appearance.ini");if(File.Exists(path))foreach(string line in File.ReadAllLines(path))if(line=="PreviewGender=male")return "male";return "female";}
    internal static void Save(string dir,string selection,string gender="female"){if(!Valid(selection))throw new ArgumentException("Invalid skin selection.");gender=gender=="male"?"male":"female";string path=Path.Combine(dir,"PUBG2019Appearance.ini"),temp=path+".new";File.WriteAllText(temp,"[Appearance]\r\nItems="+selection+"\r\nPreviewGender="+gender+"\r\n",new UTF8Encoding(false));if(File.Exists(path))File.Replace(temp,path,null);else File.Move(temp,path);}
    internal static string WearGroup(SkinEntry entry){
        if(entry.Category=="Ghillie")return "Jackets";
        if(Cosmetic(entry)&&entry.Category!="Parachute skins")return entry.Category+":"+entry.Id.Replace("Item_Skin_","").Split('_')[0].ToLowerInvariant();
        return entry.Category;
    }
    internal static bool Cosmetic(SkinEntry entry){return entry.Category.EndsWith(" skins",StringComparison.Ordinal);}
}

