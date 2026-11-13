using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Text;
using System.Windows.Forms;
using System.Collections.Generic;

internal sealed class CharacterPreviewSession : IDisposable
{
    string dir; readonly string gameDir; readonly PictureBox picture; readonly Label status;
    readonly Timer timer=new Timer(); Process helper; string desired=""; DateTime imageTime=DateTime.MinValue, started; bool disposed; string gender="female", displayedKey="";
    readonly Dictionary<string,Bitmap> frames=new Dictionary<string,Bitmap>();readonly Queue<string> frameOrder=new Queue<string>();int angle;
    internal CharacterPreviewSession(string gameDir,PictureBox target,Label message,string initial,string initialGender="female")
    {
        picture=target;status=message;this.gameDir=gameDir;desired=initial;gender=initialGender=="male"?"male":"female";LoadSnapshot();StartPreview();
        timer.Interval=1000;timer.Tick+=delegate{Poll();};timer.Start();
    }
    void StartPreview()
    {
        dir=Path.Combine(Path.GetTempPath(),"PUBG2019_CharacterPreview_"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(dir);File.WriteAllText(Path.Combine(dir,"gender.txt"),gender);WriteSelection();
        string support=Path.Combine(gameDir,"CharacterPreview"),desktop=Path.Combine(support,"PreviewDesktop.exe"),script=Path.Combine(support,"LobbyCharacterPreview.ps1");
        string ps=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows),@"System32\WindowsPowerShell\v1.0\powershell.exe");
        if(!File.Exists(desktop)||!File.Exists(script))throw new FileNotFoundException("Character preview files are missing.");
        string args="-NoProfile -ExecutionPolicy Bypass -File \""+script+"\" -ControlDir \""+dir+"\" -OwnerPid "+Process.GetCurrentProcess().Id;
        string encoded=Convert.ToBase64String(Encoding.UTF8.GetBytes(args));
        helper=Process.Start(new ProcessStartInfo(desktop,"\""+ps+"\" \""+support+"\" "+encoded){UseShellExecute=false,CreateNoWindow=true,WorkingDirectory=support});
        started=DateTime.UtcNow;status.Text="Starting character preview...";
    }
    void WriteSelection()
    {
        string temp=Path.Combine(dir,"selection.new"),path=Path.Combine(dir,"selection.txt");File.WriteAllText(temp,desired,new UTF8Encoding(false));if(File.Exists(path))File.Replace(temp,path,null);else File.Move(temp,path);
        File.WriteAllText(Path.Combine(dir,"angle.txt"),angle.ToString(System.Globalization.CultureInfo.InvariantCulture));
    }
    string FrameKey(){var ids=desired.Split(',');Array.Sort(ids,StringComparer.Ordinal);return gender+"|"+String.Join(",",ids)+"|"+angle;}
    bool ShowCached(){Bitmap cached;if(!frames.TryGetValue(FrameKey(),out cached))return false;var previous=picture.Image;picture.Image=new Bitmap(cached);if(previous!=null)previous.Dispose();status.Text="CHARACTER PREVIEW — cached";displayedKey=FrameKey();return true;}
    void Remember(Bitmap frame){string key=FrameKey();Bitmap old;if(frames.TryGetValue(key,out old)){old.Dispose();frames[key]=new Bitmap(frame);return;}while(frameOrder.Count>=48){string first=frameOrder.Dequeue();if(frames.TryGetValue(first,out old)){frames.Remove(first);old.Dispose();}}frames[key]=new Bitmap(frame);frameOrder.Enqueue(key);}
    internal void Rotate(int steps){if(disposed||steps==0)return;angle=((angle+steps)%12+12)%12;imageTime=DateTime.MinValue;if(!ShowCached())status.Text="Rendering view...";WriteSelection();}
    internal void SetSelection(string selection)
    {
        if(!SkinProfile.Valid(selection))throw new ArgumentException("Invalid appearance.");
        if(desired==selection)return;desired=selection;imageTime=DateTime.MinValue;bool cached=ShowCached();
        WriteSelection();if(!cached)status.Text="Updating character preview...";
    }
    internal void SetGender(string value)
    {
        string next=value=="male"?"male":"female";if(disposed||next==gender)return;
        try{File.WriteAllText(Path.Combine(dir,"cleanup"),"");File.WriteAllText(Path.Combine(dir,"stop"),"");}catch{}
        if(helper!=null){helper.Dispose();helper=null;}gender=next;angle=0;displayedKey="";imageTime=DateTime.MinValue;
        if(picture.Image!=null){picture.Image.Dispose();picture.Image=null;}
        foreach(var frame in frames.Values)frame.Dispose();frames.Clear();frameOrder.Clear();
        StartPreview();
    }
    void LoadSnapshot()
    {
        try{
            string meta=Path.Combine(gameDir,"PUBG2019Character.ini"),path=Path.Combine(gameDir,"PUBG2019Character.png");
            if(!File.Exists(meta)||!File.Exists(path))return;
            string key=File.ReadAllText(meta),prefix=FrameKey();prefix=prefix.Substring(0,prefix.LastIndexOf('|')+1);
            int savedAngle;if(!key.StartsWith(prefix,StringComparison.Ordinal)||!Int32.TryParse(key.Substring(prefix.Length),out savedAngle)||savedAngle<0||savedAngle>11)return;angle=savedAngle;
            using(var stream=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete))using(var image=Image.FromStream(stream)){
                var bitmap=new Bitmap(image);picture.Image=bitmap;Remember(bitmap);displayedKey=FrameKey();
            }
        }catch(IOException){}catch(ArgumentException){}
    }
    internal bool SaveSnapshot()
    {
        if(picture.Image==null||displayedKey!=FrameKey())return false;
        string path=Path.Combine(gameDir,"PUBG2019Character.png"),temp=path+".new";
        picture.Image.Save(temp,System.Drawing.Imaging.ImageFormat.Png);
        if(File.Exists(path))File.Replace(temp,path,null);else File.Move(temp,path);
        string meta=Path.Combine(gameDir,"PUBG2019Character.ini"),metaTemp=meta+".new";File.WriteAllText(metaTemp,FrameKey(),new UTF8Encoding(false));if(File.Exists(meta))File.Replace(metaTemp,meta,null);else File.Move(metaTemp,meta);
        return true;
    }
    void Poll()
    {
        if(disposed)return;try{
            string error=Path.Combine(dir,"error.txt");if(File.Exists(error)){status.Tag=File.ReadAllText(error);status.Text="Preview unavailable — click for details";timer.Stop();return;}
            if(helper!=null&&helper.HasExited){status.Text="Preview stopped unexpectedly.";timer.Stop();return;}
            string rendered=Path.Combine(dir,"rendered.txt"),image=Path.Combine(dir,"preview.png");
            if(!File.Exists(rendered)||!File.Exists(image)){string note=Path.Combine(dir,"status.txt");if(File.Exists(note))status.Text=File.ReadAllText(note);if((DateTime.UtcNow-started).TotalMinutes>7){status.Text="Preview startup timed out.";try{File.WriteAllText(Path.Combine(dir,"stop"),"");}catch{}timer.Stop();}return;}
            string angleFile=Path.Combine(dir,"rendered-angle.txt");if(File.ReadAllText(rendered)!=desired||!File.Exists(angleFile)||File.ReadAllText(angleFile)!=angle.ToString(System.Globalization.CultureInfo.InvariantCulture)){string note=Path.Combine(dir,"status.txt");if(!frames.ContainsKey(FrameKey()))status.Text=File.Exists(note)?File.ReadAllText(note):"Updating character preview...";return;}
            DateTime timestamp=File.GetLastWriteTimeUtc(image);if(timestamp==imageTime)return;
            using(var stream=new FileStream(image,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete))using(var bmp=Image.FromStream(stream)){
                int x=(int)(bmp.Width*0.30),y=(int)(bmp.Height*0.10),w=(int)(bmp.Width*0.40),h=(int)(bmp.Height*0.84);
                var cropped=new Bitmap(w,h);using(var graphics=Graphics.FromImage(cropped))graphics.DrawImage(bmp,new Rectangle(0,0,w,h),new Rectangle(x,y,w,h),GraphicsUnit.Pixel);
                Remember(cropped);var previous=picture.Image;picture.Image=cropped;if(previous!=null)previous.Dispose();
            }
            imageTime=timestamp;displayedKey=FrameKey();bool cosmetic=false;foreach(string id in desired.Split(',')){SkinEntry e;if(SkinProfile.Catalog.TryGetValue(id,out e)&&SkinProfile.Cosmetic(e))cosmetic=true;}status.Text=cosmetic?"Clothing preview — item skins need matching equipment":"CHARACTER PREVIEW";
        }catch(IOException){}catch(Exception e){status.Text="Preview unavailable: "+e.Message;timer.Stop();}
    }
    public void Dispose(){if(disposed)return;disposed=true;timer.Stop();timer.Dispose();try{File.WriteAllText(Path.Combine(dir,"cleanup"),"");File.WriteAllText(Path.Combine(dir,"stop"),"");}catch{}if(picture.Image!=null){picture.Image.Dispose();picture.Image=null;}foreach(var frame in frames.Values)frame.Dispose();frames.Clear();}
}
