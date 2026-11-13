using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Text;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;

// The HTML is a visual reference only. This control paints with GDI+ and uses native Win32 edit controls.
internal static class NativeArt {
    static readonly PrivateFontCollection fonts=new PrivateFontCollection();
    static readonly List<IntPtr> buffers=new List<IntPtr>();
    static readonly Dictionary<string,Font> cache=new Dictionary<string,Font>();
    [DllImport("gdi32.dll")] static extern IntPtr AddFontMemResourceEx(IntPtr data,uint length,IntPtr reserved,ref uint count);
    internal static readonly Color Bg=Color.FromArgb(16,17,20), Panel=Color.FromArgb(21,22,25), Field=Color.FromArgb(16,17,20), Border=Color.FromArgb(91,91,94), Text=Color.FromArgb(237,237,240), Muted=Color.FromArgb(170,170,177), Yellow=Color.FromArgb(255,192,0);
    static NativeArt(){foreach(string n in new[]{"FontHeadingRegular","FontHeadingBold","FontHeadingHeavy","FontBodyRegular","FontBodyBold"}){using(var stream=Assembly.GetExecutingAssembly().GetManifestResourceStream("Launcher."+n)){if(stream==null)continue;byte[] bytes=new byte[stream.Length];stream.Read(bytes,0,bytes.Length);IntPtr ptr=Marshal.AllocCoTaskMem(bytes.Length);Marshal.Copy(bytes,0,ptr,bytes.Length);fonts.AddMemoryFont(ptr,bytes.Length);uint count=0;AddFontMemResourceEx(ptr,(uint)bytes.Length,IntPtr.Zero,ref count);buffers.Add(ptr);}}}
    internal static string UiLanguage="en";
    static bool UnicodeUi { get { return !String.IsNullOrEmpty(UiLanguage) && !UiLanguage.Equals("en",StringComparison.OrdinalIgnoreCase); } }
    static Font UnicodeFont(float size,bool bold){string key="unicode/"+size+"/"+bold;Font f;if(!cache.TryGetValue(key,out f)){f=new Font("Segoe UI",size,bold?FontStyle.Bold:FontStyle.Regular,GraphicsUnit.Pixel);cache[key]=f;}return f;}
    internal static Font Font(float size,bool bold=false,bool heading=false){return UnicodeUi?UnicodeFont(size,bold):NamedFont(size,"Launcher "+(heading?"Heading":"Body")+" "+(bold?"Bold":"Regular"));}
    internal static Font Heavy(float size){return UnicodeUi?UnicodeFont(size,true):NamedFont(size,"Launcher Heading Heavy");}
    static Font NamedFont(float size,string name){string key=size+"/"+name;Font f;if(cache.TryGetValue(key,out f))return f;FontFamily family=null;foreach(var candidate in fonts.Families)if(candidate.Name==name){family=candidate;break;}f=family==null?new Font("Arial",size,FontStyle.Regular,GraphicsUnit.Pixel):new Font(family,size,FontStyle.Regular,GraphicsUnit.Pixel);cache[key]=f;return f;}
    internal static Bitmap Image(string name){using(var stream=Assembly.GetExecutingAssembly().GetManifestResourceStream("Launcher."+name)){if(stream==null)return null;using(var image=System.Drawing.Image.FromStream(stream))return new Bitmap(image);}}
    internal static GraphicsPath Path(RectangleF r,float radius){float d=Math.Min(radius*2,Math.Min(r.Width,r.Height));var path=new GraphicsPath();path.AddArc(r.X,r.Y,d,d,180,90);path.AddArc(r.Right-d,r.Y,d,d,270,90);path.AddArc(r.Right-d,r.Bottom-d,d,d,0,90);path.AddArc(r.X,r.Bottom-d,d,d,90,90);path.CloseFigure();return path;}
    internal static void Box(Graphics g,Rectangle r,Color fill,Color border,int radius=5){if(r.Width<=0||r.Height<=0)return;using(var path=Path(new RectangleF(r.X+.5f,r.Y+.5f,r.Width-1,r.Height-1),radius)){using(var b=new SolidBrush(fill))g.FillPath(b,path);using(var pen=new Pen(border))g.DrawPath(pen,path);}}
    internal static void TextAt(Graphics g,string text,RectangleF r,Font f,Color color,StringAlignment align=StringAlignment.Near,bool wrap=false){using(var b=new SolidBrush(color))using(var sf=new StringFormat(StringFormat.GenericTypographic){Alignment=align,LineAlignment=StringAlignment.Center,Trimming=StringTrimming.EllipsisCharacter,FormatFlags=wrap?0:StringFormatFlags.NoWrap})g.DrawString(text??"",f,b,r,sf);}
    internal static void Cover(Graphics g,Image image,Rectangle r,bool contain=false){if(image==null||r.Width<=0||r.Height<=0)return;float scale=contain?Math.Min((float)r.Width/image.Width,(float)r.Height/image.Height):Math.Max((float)r.Width/image.Width,(float)r.Height/image.Height);float w=image.Width*scale,h=image.Height*scale;var state=g.Save();g.SetClip(r);g.DrawImage(image,r.X+(r.Width-w)/2,contain?r.Bottom-h:r.Y+(r.Height-h)/2,w,h);g.Restore(state);}
}
internal sealed partial class NativeLauncherView:Control {
    internal event Action<string,Dictionary<string,string>> Command;
    ContextMenuStrip zombieMenu;
    NativeSnapshot state;string view="connect",map="Miramar",mode="Solo",camera="TPP",zombieRule="None",address="",signature="";bool initialized,visibleLan=true,autoJoin=true,lateJoin,showOnline,roomChat=true,filter,openOnly,favoriteOnly,passwordVisible,networkNet,dedicated;
    string voteChoiceMap="",voteChoiceMode="Solo",voteChoiceCamera="FPP";
    static readonly string[] VoteMapIds={"Erangel","ErangelClassic","Miramar","Sanhok","Vikendi","Training"};
    int noticeRevision=-1,focusHit=-1;DateTime noticeUntil;string notice="";
    readonly Dictionary<string,TextBox> fields=new Dictionary<string,TextBox>();readonly HashSet<string> visibleFields=new HashSet<string>();
    readonly Dictionary<string,Bitmap> images=new Dictionary<string,Bitmap>();Bitmap portrait;
    readonly List<Hit> hits=new List<Hit>();readonly HashSet<string> favorites=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
    readonly VScrollBar tableScroll=new VScrollBar(),chatScroll=new VScrollBar(),slotScroll=new VScrollBar(),consoleScroll=new VScrollBar();
    Rectangle centerRect,chatRect,slotRect,tableRect,consoleRect;Point mouse;bool updating;
    sealed class Hit {internal Rectangle Bounds;internal Action Action;internal bool Enabled;internal string Hint;}
    internal NativeLauncherView(){
        SetStyle(ControlStyles.AllPaintingInWmPaint|ControlStyles.UserPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.ResizeRedraw|ControlStyles.Selectable,true);BackColor=NativeArt.Bg;TabStop=true;
        foreach(string id in new[]{"Erangel","ErangelClassic","Miramar","Sanhok","Vikendi","Training"})images[id]=NativeArt.Image("Ui"+id);
        portrait=null;images["Silhouette"]=NativeArt.Image("CharacterSilhouette");images["VoteArtwork"]=NativeArt.Image("UiVoteArtwork");
        foreach(string id in new[]{"serverName","password","capacity","bots","minimum","code","search","direct","message","masterListen","dedicatedName","dedicatedCapacity","dedicatedBots","dedicatedInstances"}){
            var edit=new TextBox{BorderStyle=BorderStyle.None,BackColor=NativeArt.Field,ForeColor=NativeArt.Text,Font=NativeArt.Font(16),Visible=false,Name="Native_"+id,MaxLength=id=="message"?256:id=="password"?64:(id=="serverName"||id=="dedicatedName")?32:80};
            fields[id]=edit;Controls.Add(edit);if(id=="capacity"||id=="bots"||id=="minimum"||id=="dedicatedCapacity"||id=="dedicatedBots"||id=="dedicatedInstances")edit.KeyPress+=delegate(object s,KeyPressEventArgs e){if(!Char.IsDigit(e.KeyChar)&&!Char.IsControl(e.KeyChar))e.Handled=true;};
        }
        fields["password"].UseSystemPasswordChar=true;fields["code"].ReadOnly=true;fields["search"].TextChanged+=delegate{tableScroll.Value=0;Invalidate();};
        fields["message"].KeyDown+=delegate(object s,KeyEventArgs e){if(e.KeyCode==Keys.Enter){SendChat();e.SuppressKeyPress=true;}};
        fields["direct"].KeyDown+=delegate(object s,KeyEventArgs e){if(e.KeyCode==Keys.Enter){Send("join","address",fields["direct"].Text);e.SuppressKeyPress=true;}};
        foreach(var bar in new[]{tableScroll,chatScroll,slotScroll,consoleScroll}){bar.Visible=false;bar.SmallChange=35;bar.ValueChanged+=delegate{if(!updating)Invalidate();};Controls.Add(bar);}
        try{string p=FavoritePath;if(File.Exists(p))foreach(string line in File.ReadAllLines(p))if(line.Length<80)favorites.Add(line);}catch(IOException){}
    }
    string FavoritePath{get{return System.IO.Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"Saved","LauncherFavorites.txt");}}
    internal void SetPortrait(Bitmap image){var old=portrait;portrait=image;if(old!=null)old.Dispose();Invalidate();}
    internal void UpdateState(NativeSnapshot value){
        bool previous=state!=null&&state.InRoom;string previousLanguage=NativeArt.UiLanguage;state=value;networkNet=value.NetMode;NativeArt.UiLanguage=value.Language??"en";
        if(!String.Equals(previousLanguage,NativeArt.UiLanguage,StringComparison.OrdinalIgnoreCase))foreach(var edit in fields)edit.Value.Font=NativeArt.Font(edit.Key=="message"?ChatFontSize:16);
        if(!initialized){initialized=true;var h=state.HostSettings;map=h.Map;mode=Mode(h);camera=h.Camera;zombieRule=h.ZombieRule??"None";fields["serverName"].Text=h.Name;fields["capacity"].Text=h.Capacity.ToString();fields["bots"].Text=h.Bots.ToString();fields["minimum"].Text=h.MinimumPlayers.ToString();fields["code"].Text=h.Code;visibleLan=value.VisibleLan;autoJoin=value.AutoJoin;dedicated=value.Dedicated;fields["masterListen"].Text=value.MasterAdvertise??"";fields["dedicatedName"].Text=value.DedicatedName??"";fields["dedicatedCapacity"].Text=value.DedicatedCapacity.ToString();fields["dedicatedBots"].Text=value.DedicatedBots.ToString();fields["dedicatedInstances"].Text=value.DedicatedInstances.ToString();lateJoin=h.LateJoin;}
        if(!previous&&state.InRoom){view="room";voteChoiceMap="";slotScroll.Value=0;roomChat=true;chatScroll.Value=0;}
        if(previous&&!state.InRoom){view="connect";roomChat=false;slotScroll.Value=0;}
        if(state.NoticeRevision!=noticeRevision){noticeRevision=state.NoticeRevision;notice=state.Notice??"";noticeUntil=DateTime.UtcNow.AddSeconds(8);}
        if(!state.Rooms.Any(r=>r.Address==address))address=state.Rooms.Length>0?state.Rooms[0].Address:"";
        var text=new StringBuilder();text.Append(state.DedicatedSavedNotice).Append(state.MasterRunning).Append(state.DedicatedRunning).Append(state.NetChatConnected).Append(state.ChatServerRunning).Append(state.QuickRoom).Append(state.QuickPendingMap).Append(state.VoteMap).Append(state.VoteProposer).Append(state.VoteMode).Append(state.VoteCamera).Append(state.VoteSeconds).Append(state.VoteYes).Append(state.VoteNo).Append(state.VoteEligible).Append(state.Name).Append(state.NetMode).Append(state.Language).Append(state.Address).Append(state.InRoom).Append(state.Ready).Append(state.Starting).Append(state.Match).Append(state.Joined).Append(state.ManualActiveJoin).Append(state.JoinInProgress).Append(state.AppearanceSynced).Append(state.HasAppearance).Append(state.SlotsReady).Append(state.NoticeRevision);
        if(state.Room!=null)text.Append(state.Room.Ready).Append(state.Room.LateJoin);
        foreach(var r in state.Rooms)text.Append(r.Address).Append(r.Name).Append(r.Capacity).Append(r.Players).Append(r.Bots).Append(r.ZombieRule).Append(r.Ready).Append(r.LateJoin).Append(r.PingMs);
        foreach(var seat in state.Seats)text.Append(seat.Name).Append(seat.Slot);
        foreach(var lines in new[]{state.Online,state.RoomNames,state.GlobalChat,state.RoomChat,state.Logs,state.DedicatedLogs??new string[0]})foreach(string line in lines)text.Append(line);
        text.Append(DateTime.UtcNow<noticeUntil?notice:"");string next=text.ToString();if(next!=signature){signature=next;Invalidate();}
    }
    internal static string Mode(LanServer room){return room.TeamSize==4?"Squad":room.TeamSize==2?"Duo":"Solo";}
    static bool IsZombieRoom(LanServer room){return room!=null&&!String.IsNullOrEmpty(room.ZombieRule)&&!room.ZombieRule.Equals("None",StringComparison.OrdinalIgnoreCase);}
    static string MapName(string id){return id=="ErangelClassic"?"Erangel Classic":id=="Training"?"Training Map":id;}
    string Status(LanServer room){return room.Ready?(room.LateJoin?"In progress · Joinable":"In progress · Closed"):"Waiting";}
    void Send(string action){Send(action,new Dictionary<string,string>());}void Send(string action,string key,string value){Send(action,new Dictionary<string,string>{{key,value}});}void Send(string action,Dictionary<string,string> values){var cb=Command;if(cb!=null)cb(action,values);Invalidate();}
    void Go(string next){if(state!=null&&state.InRoom){view="room";}else view=next;Invalidate();}
    void SelectNetwork(bool useNet){
        if(state!=null&&(state.InRoom||state.Starting||state.Match)){notice="Leave the current room before changing network mode.";noticeUntil=DateTime.UtcNow.AddSeconds(8);Invalidate();return;}
        view="connect";Send(useNet?"netLogin":"lanMode");Invalidate();
    }
    void DrawNetworkUnavailable(Graphics g,Rectangle center,Rectangle right){
        Panel(g,center,"AVAILABLE SERVERS (0)");
        Text(g,"NET MODE",new Rectangle(center.X+20,center.Y+65,center.Width-40,35),23,true,true,NativeArt.Yellow,StringAlignment.Center);
        Text(g,"Internet matchmaking and server listing are not available yet.",new Rectangle(center.X+20,center.Y+105,center.Width-40,60),16,false,false,NativeArt.Muted,StringAlignment.Center,true);
        Panel(g,right,"CHAT",34);Text(g,"NET chat is not connected.",new Rectangle(right.X+15,right.Y+50,right.Width-30,50),15,false,false,NativeArt.Muted,StringAlignment.Center,true);
    }
    const float ChatFontSize = 14f;
    void SendChat(){string message=fields["message"].Text.Trim();if(message.Length==0)return;Send("chat",new Dictionary<string,string>{{"channel",state!=null&&state.InRoom&&roomChat?"room":"global"},{"text",message}});fields["message"].Clear();}
    void ToggleFavorite(){if(address.Length==0)return;if(!favorites.Add(address))favorites.Remove(address);try{Directory.CreateDirectory(System.IO.Path.GetDirectoryName(FavoritePath));File.WriteAllLines(FavoritePath,favorites.ToArray());}catch(IOException ex){notice=ex.Message;noticeUntil=DateTime.UtcNow.AddSeconds(7);}Invalidate();}
    void HitBox(Rectangle r,Action action,bool enabled=true,string hint=""){hits.Add(new Hit{Bounds=r,Action=action,Enabled=enabled,Hint=hint});}
    string Ui(string text){
        if(String.IsNullOrEmpty(text))return text;
        string lang=state==null?"en":state.Language;
        if(text=="EXIT")return NativeUiText.Get(lang,"Exit");
        foreach(string mark in new[]{"⟳  ","▼  ","▤  ","▰  ","⚑  "})if(text.StartsWith(mark,StringComparison.Ordinal))return mark+NativeUiText.Get(lang,text.Substring(mark.Length));
        foreach(string prefix in new[]{"AVAILABLE SERVERS (","AVAILABLE LAN SERVERS ("})if(text.StartsWith(prefix,StringComparison.Ordinal))return NativeUiText.Get(lang,prefix.Substring(0,prefix.Length-2))+" ("+text.Substring(prefix.Length);
        return NativeUiText.Get(lang,text);
    }
    void RawText(Graphics g,string text,Rectangle r,float size=16,bool bold=false,bool heading=false,Color? color=null,StringAlignment align=StringAlignment.Near,bool wrap=false){NativeArt.TextAt(g,text,r,NativeArt.Font(size,bold,heading),color??NativeArt.Text,align,wrap);}
    new void Text(Graphics g,string text,Rectangle r,float size=16,bool bold=false,bool heading=false,Color? color=null,StringAlignment align=StringAlignment.Near,bool wrap=false){
        string label=Ui(text);if(!wrap&&label!=text)while(size>9&&Measure(g,label,NativeArt.Font(size,bold,heading))>r.Width-2)size-=.5f;
        RawText(g,label,r,size,bold,heading,color,align,wrap);
    }
    string Detail(string label,string value){return Ui(label)+": "+value;}
    void OpenWardrobeFromPortrait(){
        string lang=state==null?"en":state.Language;
        using(var dialog=new Form{Text=Ui("CHARACTER / WARDROBE"),Name="WardrobeConfirmation",StartPosition=FormStartPosition.CenterParent,FormBorderStyle=FormBorderStyle.None,ShowInTaskbar=false,MaximizeBox=false,MinimizeBox=false,AutoScaleMode=AutoScaleMode.None,ClientSize=new Size(480,210),BackColor=NativeArt.Panel,ForeColor=NativeArt.Text}){
            dialog.Paint+=delegate(object sender,PaintEventArgs e){using(var border=new Pen(NativeArt.Border))e.Graphics.DrawRectangle(border,0,0,dialog.ClientSize.Width-1,dialog.ClientSize.Height-1);};
            var heading=new Label{Text=Ui("CHARACTER / WARDROBE"),Font=NativeArt.Font(19,true,true),ForeColor=NativeArt.Text,Bounds=new Rectangle(25,8,430,43),TextAlign=ContentAlignment.MiddleLeft,AutoEllipsis=true};
            var rule=new Panel{BackColor=NativeArt.Yellow,Bounds=new Rectangle(1,53,478,2)};
            var question=new Label{Name="WardrobeConfirmationQuestion",Text=NativeUiText.ConfirmQuestion(lang),Font=NativeArt.Font(16),ForeColor=NativeArt.Text,Bounds=new Rectangle(28,66,424,60),TextAlign=ContentAlignment.MiddleCenter};
            var no=new VoteStyleSettingsButton(false){Name="WardrobeConfirmationNo",Text=NativeUiText.Get(lang,"No"),Bounds=new Rectangle(28,151,201,40),DialogResult=DialogResult.No};
            var yes=new VoteStyleSettingsButton(true){Name="WardrobeConfirmationYes",Text=NativeUiText.Get(lang,"Yes"),Bounds=new Rectangle(251,151,201,40),DialogResult=DialogResult.Yes};
            dialog.Controls.AddRange(new Control[]{heading,rule,question,no,yes});dialog.AcceptButton=yes;dialog.CancelButton=no;dialog.Shown+=delegate{no.Focus();};
            if(dialog.ShowDialog(FindForm())==DialogResult.Yes)Send("wardrobe");
        }
    }
    void Line(Graphics g,int x,int y,int x2,int y2,Color? color=null){using(var pen=new Pen(color??NativeArt.Border))g.DrawLine(pen,x,y,x2,y2);}
    void Button(Graphics g,Rectangle r,string title,Action action,bool gold=false,bool enabled=true,bool flat=false){
        bool hover=r.Contains(mouse)&&enabled;Color border=gold?Color.FromArgb(255,205,52):hover?NativeArt.Yellow:Color.FromArgb(110,110,116);
        if(!flat){using(var p=NativeArt.Path(r,5))using(var b=new LinearGradientBrush(r,gold?Color.FromArgb(255,210,35):Color.FromArgb(41,42,46),gold?Color.FromArgb(239,165,0):Color.FromArgb(20,21,24),90)){g.FillPath(b,p);using(var pen=new Pen(border))g.DrawPath(pen,p);}}
        else if(hover)using(var b=new SolidBrush(Color.FromArgb(30,31,35)))g.FillRectangle(b,r);
        string label=Ui(title);float size=gold?19:13;while(size>8&&Measure(g,label,gold?NativeArt.Heavy(size):NativeArt.Font(size,true,true))>r.Width-8)size-=.5f;Font labelFont=gold?NativeArt.Heavy(size):NativeArt.Font(size,true,true);
        NativeArt.TextAt(g,label,r,labelFont,enabled?(gold?Color.FromArgb(8,8,8):NativeArt.Text):NativeArt.Muted,StringAlignment.Center);
        if(!enabled)using(var b=new SolidBrush(Color.FromArgb(100,NativeArt.Bg)))g.FillRectangle(b,r);HitBox(r,action,enabled,title);
    }
    void Panel(Graphics g,Rectangle r,string title,int header=43){using(var path=NativeArt.Path(r,8)){using(var brush=new LinearGradientBrush(r,Color.FromArgb(28,29,33),Color.FromArgb(14,15,18),120))g.FillPath(brush,path);using(var pen=new Pen(NativeArt.Border))g.DrawPath(pen,path);}Line(g,r.X+1,r.Y+header,r.Right-1,r.Y+header);Text(g,title,new Rectangle(r.X+15,r.Y,r.Width-30,header),header==34?17:23,true,true);}
    void Field(Graphics g,string id,Rectangle r,string placeholder="",int reserveRight=0){
        var edit=fields[id];bool active=edit.Focused;NativeArt.Box(g,r,NativeArt.Field,active?NativeArt.Yellow:Color.FromArgb(122,122,126));visibleFields.Add(id);
        int h=Math.Min(23,r.Height-8);var bounds=new Rectangle(r.X+10,r.Y+(r.Height-h)/2,r.Width-20-reserveRight,h);if(edit.Bounds!=bounds)edit.Bounds=bounds;
        if(id=="message")edit.Font=NativeArt.Font(ChatFontSize);
        edit.Visible=active;
        if(!active){RawText(g,edit.Text.Length==0?Ui(placeholder):id=="password"&&!passwordVisible?new string('●',edit.Text.Length):edit.Text,new Rectangle(r.X+10,r.Y,r.Width-20-reserveRight,r.Height),id=="search"?13:id=="message"?ChatFontSize:16,false,false,edit.Text.Length==0?Color.FromArgb(128,128,128):NativeArt.Text);}
        if(!edit.ReadOnly)HitBox(r,delegate{edit.Visible=true;edit.Focus();edit.SelectionStart=edit.Text.Length;Invalidate();});
        if(active&&edit.Text.Length==0&&placeholder.Length>0)SetCue(edit.Handle,1,Ui(placeholder));
    }
    [DllImport("user32.dll",CharSet=CharSet.Unicode,EntryPoint="SendMessageW")] static extern IntPtr SetCue(IntPtr h,int msg,IntPtr w,string text);
    static void SetCue(IntPtr h,int show,string text){SetCue(h,0x1501,new IntPtr(show),text);}
    void Check(Graphics g,Rectangle r,string title,bool value,Action action){var box=new Rectangle(r.X,r.Y+(r.Height-17)/2,17,17);NativeArt.Box(g,box,value?NativeArt.Yellow:NativeArt.Field,value?NativeArt.Yellow:NativeArt.Muted,1);if(value){using(var p=new Pen(Color.FromArgb(25,25,25),2))g.DrawLines(p,new[]{new Point(box.X+3,box.Y+8),new Point(box.X+7,box.Y+12),new Point(box.X+14,box.Y+3)});}Text(g,title,new Rectangle(r.X+25,r.Y,r.Width-25,r.Height),Width<=1450?12:14);HitBox(r,action);}
    void Radio(Graphics g,Rectangle r,string title,bool value,Action action,bool right=false){int d=right?17:13,x=right?r.Right-d:r.X;var circle=new Rectangle(x,r.Y+(r.Height-d)/2,d,d);using(var b=new SolidBrush(Color.FromArgb(53,54,57)))g.FillEllipse(b,circle);using(var p=new Pen(value?NativeArt.Yellow:NativeArt.Muted))g.DrawEllipse(p,circle);if(value)using(var b=new SolidBrush(NativeArt.Yellow))g.FillEllipse(b,circle.X+3,circle.Y+3,d-6,d-6);float size=right?15:14;int textWidth=r.Width-(right?24:20);while(size>11&&Measure(g,title,NativeArt.Font(size,true))>textWidth)size-=.5f;Text(g,title,new Rectangle(right?r.X:r.X+20,r.Y,textWidth,r.Height),size,true);HitBox(r,action);}
    void Scroll(VScrollBar bar,Rectangle area,int content){int max=Math.Max(0,content-area.Height);updating=true;bar.Visible=max>0;if(bar.Visible){bar.SetBounds(area.Right-12,area.Top,12,area.Height);bar.LargeChange=Math.Max(1,area.Height);bar.Maximum=Math.Max(0,content-1);if(bar.Value>max)bar.Value=max;}else bar.Value=0;updating=false;}
    protected override void OnPaint(PaintEventArgs e){
        base.OnPaint(e);var g=e.Graphics;g.SmoothingMode=SmoothingMode.AntiAlias;g.TextRenderingHint=TextRenderingHint.AntiAliasGridFit;g.InterpolationMode=InterpolationMode.HighQualityBicubic;
        hits.Clear();visibleFields.Clear();int w=Width,h=Height;g.Clear(NativeArt.Bg);if(w<100||h<100)return;
        DrawCanvasBackground(g,w,h);DrawHeader(g,w);int available=w-20;int left=(int)(available*(w<=1450?.21:.24)),right=Math.Max(w<=1450?300:320,(int)(available*(w<=1450?.25:.235)));int gap=14;int top=90,bottom=h-52;int middle=available-left-right-gap*2;
        if(middle<350){left=Math.Max(130,left-(350-middle));middle=available-left-right-gap*2;}
        centerRect=new Rectangle(left+gap+12,top,middle,Math.Max(100,bottom-top-10));var sidebar=new Rectangle(12,top,left,centerRect.Height);var rightRect=new Rectangle(centerRect.Right+gap,top,right,centerRect.Height);
        DrawCharacter(g,sidebar);bool inRoom=state!=null&&state.InRoom;
        {
            if(view=="host"&&!inRoom)DrawHost(g,centerRect);else if(view=="dedicatedConsole"&&!inRoom)DrawDedicatedConsole(g,centerRect);else if(inRoom&&view=="console")DrawConsole(g,centerRect);else if(inRoom)DrawRoom(g,centerRect);else DrawServers(g,centerRect);
            bool detail=!inRoom&&view!="host";int detailHeight=detail?282:0;
            chatRect=new Rectangle(rightRect.X,rightRect.Y,rightRect.Width,rightRect.Height-(detail?detailHeight+14:0));DrawChat(g,chatRect);
            if(detail)DrawDetail(g,new Rectangle(rightRect.X,chatRect.Bottom+14,rightRect.Width,detailHeight));
        }
        DrawFooter(g,w,h);
        if(state!=null && String.IsNullOrWhiteSpace(state.Name) && !state.Dedicated) DrawMissingNameNotice(g,w,h);
        foreach(var pair in fields)if(!visibleFields.Contains(pair.Key))pair.Value.Visible=false;
        if(view!="connect"||inRoom)tableScroll.Visible=false;if(networkNet&&!inRoom&&(state==null||!state.NetChatConnected))chatScroll.Visible=false;if(!inRoom||view=="console")slotScroll.Visible=false;if(view!="console")consoleScroll.Visible=false;
        if(DateTime.UtcNow<noticeUntil&&notice.Length>0&&w>700){Text(g,notice,new Rectangle(200,h-47,Math.Max(50,w-600),42),14,false,false,NativeArt.Muted,StringAlignment.Center);}
    }
    void DrawMissingNameNotice(Graphics g,int w,int h){
        int width=Math.Min(560,Math.Max(300,w-80)),height=86;
        var box=new Rectangle((w-width)/2,(h-height)/2,width,height);
        using(var shadow=new SolidBrush(Color.FromArgb(100,0,0,0)))g.FillRectangle(shadow,box.X+3,box.Y+4,box.Width,box.Height);
        NativeArt.Box(g,box,NativeArt.Yellow,Color.FromArgb(255,220,80),5);
        using(var font=new Font("Segoe UI",18,FontStyle.Bold,GraphicsUnit.Pixel))NativeArt.TextAt(g,PlayerNameNotice.Get(state.Language),new Rectangle(box.X+16,box.Y+8,box.Width-32,box.Height-16),font,Color.FromArgb(20,20,20),StringAlignment.Center,true);
        HitBox(box,delegate{Send("profile");},true,"Set player name in Settings");
    }
    void DrawCharacter(Graphics g,Rectangle r){
        // The saved studio screenshot fills the character area as a static
        // image. Keep its aspect ratio and clip the sides to fit the column.
        if(portrait==null)DrawSilhouette(g,r);else NativeArt.Cover(g,portrait,r);
        HitBox(r,delegate{OpenWardrobeFromPortrait();},state!=null&&state.Wardrobe,"Character / Wardrobe");
        if(state==null||!state.HasAppearance){
            var choose=new Rectangle(r.X+Math.Max(5,(r.Width-240)/2),r.Bottom-(state!=null&&state.NetMode?110:125),Math.Min(r.Width-10,240),42);NativeArt.Box(g,choose,Color.FromArgb(24,25,28),NativeArt.Yellow);
            using(var font=new Font("Segoe UI",14,FontStyle.Bold,GraphicsUnit.Pixel))NativeArt.TextAt(g,NativeWardrobeText.Get(state==null?"en":state.Language),new Rectangle(choose.X+8,choose.Y+3,choose.Width-16,choose.Height-6),font,NativeArt.Yellow,StringAlignment.Center,true);
            HitBox(choose,delegate{OpenWardrobeFromPortrait();},state!=null&&state.Wardrobe);
        }
        bool online=state!=null&&state.NetMode;
        var name=new Rectangle(r.X+10,r.Bottom-(online?62:73),r.Width-20,31);RawText(g,state==null?Ui("Set player name"):state.Name,name,16,true,false,Color.White,StringAlignment.Center);HitBox(name,delegate{Send("profile");});
        if(online){string status="ONLINE";var font=NativeArt.Font(14,true);int width=(int)Math.Ceiling(Measure(g,status,font)),center=r.X+r.Width/2,y=r.Bottom-26;using(var b=new SolidBrush(Color.FromArgb(72,219,80)))g.FillEllipse(b,center-width/2-14,y+7,7,7);Text(g,status,new Rectangle(r.X+10,y,r.Width-20,22),14,true,false,Color.White,StringAlignment.Center);}
        else if(state!=null){var warning=new Rectangle(r.X+7,r.Bottom-39,r.Width-14,34);using(var shade=new SolidBrush(Color.FromArgb(210,10,11,13)))g.FillRectangle(shade,warning);Text(g,Ui("LAN MODE — ONLINE SERVERS ARE SHOWN IN NET MODE"),warning,11,true,false,NativeArt.Yellow,StringAlignment.Center,true);}

    }
    void DrawSilhouette(Graphics g,Rectangle r){
        var art=images["Silhouette"];if(art==null)return;
        // Crop only the display's transparent padding; the embedded PNG keeps its alpha channel.
        var src=new RectangleF(225,24,555,1490);float scale=Math.Min((r.Width-16)/src.Width,(r.Height-90)/src.Height);float width=src.Width*scale,height=src.Height*scale;
        g.DrawImage(art,new RectangleF(r.X+(r.Width-width)/2,r.Y+Math.Max(10,(r.Height-90-height)/2),width,height),src,GraphicsUnit.Pixel);
    }
    void CreateRoom(){var values=new Dictionary<string,string>{{"dedicated","0"},{"name",fields["serverName"].Text},{"password",fields["password"].Text},{"capacity",fields["capacity"].Text},{"bots",fields["bots"].Text},{"minimum",fields["minimum"].Text},{"map",map},{"mode",mode},{"camera",camera},{"zombie",zombieRule},{"autoJoin",autoJoin?"1":"0"},{"visible",visibleLan?"1":"0"},{"late",lateJoin?"1":"0"}};Send("host",values);}
    LanServer Selected{get{return state==null?null:state.Rooms.FirstOrDefault(r=>r.Address==address);}}
    void DrawServers(Graphics g,Rectangle r){
        LanServer[] rooms=state==null?new LanServer[0]:state.Rooms.Where(s=>(s.Name+" "+s.Map+" "+Mode(s)).IndexOf(fields["search"].Text,StringComparison.OrdinalIgnoreCase)>=0&&(!openOnly||!s.Protected)&&(!favoriteOnly||favorites.Contains(s.Address))).ToArray();
        int head=r.Width<650?78:43;Panel(g,r,"",head);DrawListIcon(g,new Rectangle(r.X+16,r.Y+13,20,18));Text(g,(networkNet?"AVAILABLE SERVERS (":"AVAILABLE LAN SERVERS (")+rooms.Length+")",new Rectangle(r.X+46,r.Y,r.Width-61,43),Width<=1450?19:23,true,true);
        int toolsY=head>43?r.Y+43:r.Y+6;int toolsX=head>43?r.X+15:r.Right-354;Button(g,new Rectangle(toolsX,toolsY,82,30),"⟳  REFRESH",delegate{Send("refresh");});Button(g,new Rectangle(toolsX+89,toolsY,77,30),"▼  FILTERS",delegate{filter=!filter;Invalidate();});Field(g,"search",new Rectangle(toolsX+174,toolsY,Math.Min(165,r.Right-toolsX-187),30),"Search servers...");
        int y=r.Y+head+8;Text(g,"Direct join",new Rectangle(r.X+13,y,76,32),15,true);Field(g,"direct",new Rectangle(r.X+88,y,r.Width-151,32),"IP address or room code");Button(g,new Rectangle(r.Right-55,y,44,32),"JOIN",delegate{Send("join","address",fields["direct"].Text);});y+=41;Line(g,r.X+1,y,r.Right-1,y);
        if(filter){Check(g,new Rectangle(r.X+14,y+3,155,28),"Open servers only",openOnly,delegate{openOnly=!openOnly;Invalidate();});Check(g,new Rectangle(r.X+174,y+3,140,28),"Favorites only",favoriteOnly,delegate{favoriteOnly=!favoriteOnly;Invalidate();});y+=35;Line(g,r.X+1,y,r.Right-1,y);}
        int available=r.Width-24;double[] ratios=r.Width>=850?new[]{.175,.18,.115,.085,.085,.16,.135,.065}:new[]{.205,.145,.12,.09,.095,.14,.12,.085};int[] columns=new int[9];columns[0]=r.X+10;for(int i=0;i<8;i++)columns[i+1]=columns[i]+(int)(available*ratios[i]);columns[8]=r.Right-14;
        string aiHeading=rooms.Length>0&&rooms.All(IsZombieRoom)?"Zombies":rooms.Any(IsZombieRoom)?"AI":"Bots";
        string[] headings={"Server name ⌃","Map","Mode","Players",aiHeading,"Status","Response time",""};for(int i=0;i<8;i++)Text(g,headings[i],new Rectangle(columns[i]+(i==0&&r.Width>=850?28:3),y,columns[i+1]-columns[i]-(i==0&&r.Width>=850?30:5),38),r.Width<700?11:14,false,false,null,StringAlignment.Near,true);y+=39;
        tableRect=new Rectangle(r.X+5,y,r.Width-10,Math.Max(30,r.Bottom-y-55));int rowHeight=r.Width<700?54:36;Scroll(tableScroll,tableRect,rooms.Length*rowHeight);var save=g.Save();g.SetClip(tableRect);
        for(int row=0;row<rooms.Length;row++){var server=rooms[row];int rowY=y+row*rowHeight-tableScroll.Value;if(rowY+rowHeight<y||rowY>r.Bottom)continue;Rectangle rowRect=new Rectangle(r.X+6,rowY,r.Width-24,rowHeight);if(server.Address==address)using(var b=new LinearGradientBrush(rowRect,Color.FromArgb(82,70,28),Color.FromArgb(46,40,20),0f))g.FillRectangle(b,rowRect);Line(g,rowRect.X,rowY,rowRect.Right,rowY,Color.FromArgb(54,55,58));
            RawText(g,(server.Protected?"● ":favorites.Contains(server.Address)?"★ ":"")+server.Name,new Rectangle(columns[0]+(r.Width>=850?28:4),rowY,columns[1]-columns[0]-(r.Width>=850?32:8),rowHeight),14,true,false,null,StringAlignment.Near,true);
            int mapText=columns[1]+3;if(r.Width>=850&&!((server.QuickMatch||server.Name==LauncherForm.QuickMatchRoomName)&&!server.Ready)){NativeArt.Cover(g,images[server.Map],new Rectangle(mapText,rowY+7,43,22));mapText+=48;}
            Text(g,(server.QuickMatch||server.Name==LauncherForm.QuickMatchRoomName)&&!server.Ready?"Vote":MapName(server.Map),new Rectangle(mapText,rowY,columns[2]-mapText-3,rowHeight),14,false,false,null,StringAlignment.Near,true);Text(g,(server.QuickMatch||server.Name==LauncherForm.QuickMatchRoomName)&&!server.Ready?"Vote":Mode(server)+" / "+server.Camera,new Rectangle(columns[2]+3,rowY,columns[3]-columns[2]-6,rowHeight),14,false,false,null,StringAlignment.Near,true);
            Text(g,(server.Players<0?"—":(server.Players+server.Bots).ToString())+" / "+server.Capacity,new Rectangle(columns[3]+2,rowY,columns[4]-columns[3]-3,rowHeight),14);Text(g,server.Bots.ToString(),new Rectangle(columns[4]+3,rowY,columns[5]-columns[4]-4,rowHeight),14);
            Text(g,Status(server),new Rectangle(columns[5]+3,rowY,columns[6]-columns[5]-5,rowHeight),13,false,false,null,StringAlignment.Near,true);if(r.Width>=850&&server.PingMs>=0)DrawPing(g,new Rectangle(columns[6]+2,rowY+11,18,13),server.PingMs);Text(g,server.PingMs<0?"—":server.PingMs+" ms",new Rectangle(columns[6]+(r.Width>=850?26:2),rowY,columns[7]-columns[6]-(r.Width>=850?28:4),rowHeight),14);
            HitBox(Rectangle.Intersect(rowRect,tableRect),delegate{address=server.Address;Invalidate();});Button(g,new Rectangle(columns[7]+1,rowY+(rowHeight-26)/2,Math.Max(30,columns[8]-columns[7]-4),26),"JOIN",delegate{Send("join","address",server.Address);},false,!server.Ready||server.LateJoin);
        }
        if(rooms.Length==0)Text(g,"No matching servers found.",new Rectangle(r.X+10,y+15,r.Width-20,40),16,false,false,NativeArt.Muted,StringAlignment.Center);g.Restore(save);
        Line(g,r.X+1,r.Bottom-50,r.Right-1,r.Bottom-50);Button(g,new Rectangle(r.X+12,r.Bottom-43,190,35),"QUICK JOIN",delegate{Send("quickJoin");},true,networkNet);
    }
    Color PingColor(int ping){return ping<0?NativeArt.Muted:ping<=80?Color.FromArgb(72,219,36):ping<=150?NativeArt.Yellow:Color.FromArgb(255,75,75);}
    void DrawDetail(Graphics g,Rectangle r){Panel(g,r,"▤  SERVER DETAILS (SELECTED)",34);var s=Selected;if(s==null){Text(g,networkNet?"Select a server.":"Select a LAN server.",new Rectangle(r.X+15,r.Y+47,r.Width-30,60),16,false,false,NativeArt.Muted);return;}int iw=(int)(r.Width*.41);bool votePreview=(s.QuickMatch||s.Name==LauncherForm.QuickMatchRoomName)&&!s.Ready;var preview=new Rectangle(r.X+10,r.Y+43,iw,votePreview?Math.Max(6,Math.Min(193,r.Height-91)):140);var visiblePreview=votePreview?VotePortraitRect(preview):preview;int previewLeft=visiblePreview.Left,tx=visiblePreview.Right+8;if(votePreview)DrawMapVotePreview(g,preview,true);else NativeArt.Cover(g,images[s.Map],preview);RawText(g,s.Name,new Rectangle(tx,r.Y+40,Math.Max(1,r.Right-tx-9),27),17,true);string[] lines={Ui("Map")+": "+((s.QuickMatch||s.Name==LauncherForm.QuickMatchRoomName)&&!s.Ready?"Vote":MapName(s.Map)),Ui("Address")+": "+s.Address,Ui("Mode")+": "+((s.QuickMatch||s.Name==LauncherForm.QuickMatchRoomName)&&!s.Ready?"Vote":Mode(s)+" / "+s.Camera),Ui("Players")+": "+(s.Players<0?"—":(s.Players+s.Bots).ToString())+" / "+s.Capacity,Ui(IsZombieRoom(s)?"Zombies":"Bots")+": "+s.Bots,Ui("Password")+": "+Ui(s.Protected?"Yes":"No"),Ui("Status")+": "+Ui(Status(s)),Ui("Response time")+": "+(s.PingMs<0?"—":s.PingMs+" ms")};int y=r.Y+68;foreach(string line in lines){Text(g,line,new Rectangle(tx,y,r.Right-tx-7,21),13);y+=21;}
        Button(g,new Rectangle(previewLeft,r.Bottom-45,Math.Max(1,r.Right-70-previewLeft),37),"JOIN",delegate{Send("join","address",s.Address);},true,!s.Ready||s.LateJoin);Button(g,new Rectangle(r.Right-51,r.Bottom-45,39,37),favorites.Contains(address)?"★":"☆",ToggleFavorite);
    }
    internal static Rectangle VotePortraitRect(Rectangle bounds){
        int width=Math.Min(bounds.Width,(int)Math.Floor(bounds.Height*9.0/16.0));
        int height=Math.Min(bounds.Height,(int)Math.Round(width*16.0/9.0));
        return new Rectangle(bounds.X+(bounds.Width-width)/2,bounds.Y+(bounds.Height-height)/2,width,height);
    }
    void DrawMapVotePreview(Graphics g,Rectangle area,bool useArtwork=false){
        if(area.Width<4||area.Height<6)return;
        if(useArtwork)area=VotePortraitRect(area);
        if(area.Width<4||area.Height<6)return;
        var save=g.Save();using(var clip=NativeArt.Path(area,4)){g.SetClip(clip);
            if(useArtwork && images["VoteArtwork"]!=null){
                // Cover the portrait frame with uniform scaling; a small
                // amount at the image's sides is cropped instead of stretched.
                NativeArt.Cover(g,images["VoteArtwork"],area);
            }else{
                string[] previewMaps={"Erangel","ErangelClassic","Miramar","Sanhok","Vikendi","Training"};
                for(int i=0;i<previewMaps.Length;i++){
                    int top=area.Y+(area.Height*i)/previewMaps.Length;
                    int bottom=area.Y+(area.Height*(i+1))/previewMaps.Length;
                    var strip=new Rectangle(area.X,top,area.Width,bottom-top);
                    NativeArt.Cover(g,images[previewMaps[i]],strip);
                    if(i>0)using(var separator=new Pen(Color.FromArgb(120,0,0,0)))g.DrawLine(separator,area.X,top,area.Right-1,top);
                }
                using(var shade=new SolidBrush(Color.FromArgb(65,0,0,0)))g.FillRectangle(shade,area);
            }
            int h=Math.Min(23,Math.Max(19,area.Height/5)),w=Math.Min(area.Width,Math.Max(68,area.Width/2));var caption=new Rectangle(area.X+(area.Width-w)/2,area.Y+(area.Height-h)/2,w,h);
            using(var back=new SolidBrush(Color.FromArgb(195,0,0,0)))g.FillRectangle(back,caption);
            NativeArt.TextAt(g,Ui("MAP VOTE"),caption,NativeArt.Heavy(Math.Min(19,Math.Max(13,area.Width/8f))),Color.White,StringAlignment.Center);
        }g.Restore(save);using(var border=new Pen(Color.FromArgb(120,120,124)))g.DrawRectangle(border,area.X,area.Y,area.Width-1,area.Height-1);
    }
    void DrawRoom(Graphics g,Rectangle r){
        var room=state.Room;if(room==null)return;Panel(g,r,"SERVER DETAILS");
        int tx=r.X+213;if(!state.QuickRoom||state.Starting||state.Ready)NativeArt.Cover(g,images[room.Map],new Rectangle(r.X+15,r.Y+51,180,95));else DrawMapVotePreview(g,new Rectangle(r.X+15,r.Y+51,180,95));RawText(g,room.Name,new Rectangle(tx,r.Y+48,r.Width-228,29),24,true);
        int count=state.Seats.Length+room.Bots;string[] details={Ui("Map")+": "+(state.QuickRoom&&!state.Starting&&!state.Ready?Ui("Decided by vote"):MapName(room.Map)),Ui("Mode")+": "+(state.QuickRoom&&!state.Starting&&!state.Ready?Ui("Decided by vote"):Mode(room)+" / "+room.Camera),Ui(IsZombieRoom(room)?"Players and zombies":"Players and bots")+": "+count+" / "+room.Capacity,Ui("Status")+": "+Ui(state.Starting?"Starting server":state.Ready?"Server running":"Waiting")};for(int d=0;d<details.Length;d++)Text(g,details[d],new Rectangle(tx,r.Y+80+d*24,r.Width-228,20),16);
        int slotTeamSize=room.TeamSize;if(state.QuickRoom&&!state.Starting&&!state.Ready){string preview=String.IsNullOrEmpty(state.VoteMap)&&String.IsNullOrEmpty(state.QuickPendingMap)?voteChoiceMode:state.VoteMode;slotTeamSize=preview=="Duo"?2:preview=="Squad"?4:1;}Line(g,r.X+1,r.Y+183,r.Right-1,r.Y+183);Text(g,state.QuickRoom&&!state.Starting&&!state.Ready?"PLAYER SLOTS — TEAM PREVIEW":"PLAYER SLOTS",new Rectangle(r.X+15,r.Y+183,r.Width-30,43),23,true,true);Line(g,r.X+1,r.Y+226,r.Right-1,r.Y+226);Text(g,"Occupied   •   Available",new Rectangle(r.X+15,r.Y+231,r.Width-30,20),16,false,false,Color.FromArgb(187,187,187));
        slotRect=new Rectangle(r.X+14,r.Y+259,r.Width-28,Math.Max(40,r.Height-(state.QuickRoom?481:338)));int cols=r.Width>=540?10:5;int rowHeight=48;int rows=(room.Capacity+cols-1)/cols;Scroll(slotScroll,slotRect,rows*rowHeight);int cellW=(slotRect.Width-(slotScroll.Visible?17:0)-(cols-1)*5)/cols;
        var save=g.Save();g.SetClip(slotRect);for(int i=0;i<room.Capacity;i++){int slot=i+1,team=i/slotTeamSize+1;Rectangle cell=new Rectangle(slotRect.X+(i%cols)*(cellW+5),slotRect.Y+(i/cols)*rowHeight-slotScroll.Value,cellW,43);if(!cell.IntersectsWith(slotRect))continue;
            NativeSeat occupant=state.Seats.FirstOrDefault(p=>p.Slot==slot);bool bot=slot>room.Capacity-room.Bots,mine=occupant!=null&&occupant.Name.Equals(state.Name,StringComparison.OrdinalIgnoreCase);string name=bot?(IsZombieRoom(room)?"Zombie ":"Bot ")+(slot-room.Capacity+room.Bots):occupant==null?"Available":occupant.Name;
            NativeArt.Box(g,cell,slotTeamSize>1?Color.FromArgb(41,42,46):occupant==null?Color.FromArgb(17,17,17):Color.FromArgb(41,42,46),mine?NativeArt.Yellow:Color.FromArgb(85,85,91));RawText(g,slotTeamSize>1?Ui("Team")+" "+team:Ui("Slot")+" "+slot,new Rectangle(cell.X+2,cell.Y+3,cell.Width-4,17),16,true,false,mine?NativeArt.Yellow:NativeArt.Text,StringAlignment.Center);RawText(g,occupant==null&&!bot?Ui(name):name,new Rectangle(cell.X+3,cell.Y+20,cell.Width-6,20),bot&&IsZombieRoom(room)?9:11,false,false,occupant==null&&!bot?NativeArt.Muted:NativeArt.Text,StringAlignment.Center);
            bool can=state.SlotsReady&&!state.Starting&&!state.Ready&&!state.Match&&!bot&&(occupant==null||mine);HitBox(Rectangle.Intersect(cell,slotRect),delegate{Send("slot","slot",slot.ToString());},can,name);
        }
        if(slotTeamSize>1){using(var pen=new Pen(Color.White)){for(int first=0;first<room.Capacity;){int row=first/cols,teamEnd=((first/slotTeamSize)+1)*slotTeamSize,last=Math.Min(room.Capacity,Math.Min((row+1)*cols,teamEnd))-1;var border=new Rectangle(slotRect.X+(first%cols)*(cellW+5)-3,slotRect.Y+row*rowHeight-slotScroll.Value-3,(last-first+1)*(cellW+5)-5+6,49);using(var path=NativeArt.Path(border,5))g.DrawPath(pen,path);first=last+1;}}}
        g.Restore(save);
        if(state.QuickRoom&&!state.Starting&&!state.Ready){
            int vy=r.Bottom-205;Line(g,r.X+1,vy-5,r.Right-1,vy-5);
            if(!String.IsNullOrEmpty(state.QuickPendingMap)){Text(g,Ui("VOTE ACCEPTED")+": "+MapName(state.QuickPendingMap)+" / "+state.VoteMode+" / "+state.VoteCamera+" — "+Ui("WAITING FOR SERVER"),new Rectangle(r.X+15,vy,r.Width-30,60),15,true);}
            else if(String.IsNullOrEmpty(state.VoteMap)){
                Text(g,"SELECT MAP, MODE AND VIEW, THEN PRESS VOTE",new Rectangle(r.X+15,vy,r.Width-30,21),14,true);
                string[] modes={"Solo","Duo","Squad"};int bw=Math.Min(85,(r.Width-60)/5);for(int i=0;i<modes.Length;i++){string choice=modes[i];Button(g,new Rectangle(r.X+12+i*(bw+6),vy+23,bw,27),choice.ToUpperInvariant(),delegate{voteChoiceMode=choice;Invalidate();},choice==voteChoiceMode);}
                string[] cameras={"FPP","TPP"};for(int i=0;i<cameras.Length;i++){string choice=cameras[i];Button(g,new Rectangle(r.X+12+(i+3)*(bw+6),vy+23,bw,27),choice,delegate{voteChoiceCamera=choice;Invalidate();},choice==voteChoiceCamera);}
                string[] ids=VoteMapIds;string[] labels={"ERANGEL","ERANGEL CLASSIC","MIRAMAR","SANHOK","VIKENDI","TRAINING"};int w=(r.Width-54)/6;
                for(int i=0;i<ids.Length;i++){string mapChoice=ids[i];var mapButton=new Rectangle(r.X+12+i*(w+6),vy+57,w,29);Button(g,mapButton,labels[i],delegate{voteChoiceMap=mapChoice;Invalidate();});if(mapChoice==voteChoiceMap)using(var path=NativeArt.Path(mapButton,5))using(var pen=new Pen(NativeArt.Yellow,2))g.DrawPath(pen,path);}
                bool canVote=state.Connected&&!(state.Host&&state.Dedicated)&&!String.IsNullOrEmpty(voteChoiceMap);
                Button(g,new Rectangle(r.X+12,vy+92,120,30),"VOTE",delegate{Send("votePropose",new Dictionary<string,string>{{"map",voteChoiceMap},{"mode",voteChoiceMap=="Training"?"Solo":voteChoiceMode},{"camera",voteChoiceMap=="Training"?"TPP":voteChoiceCamera}});},true,canVote);
                Text(g,voteChoiceMap=="Training"?"TRAINING: 20 MIN / NO BOTS":"51% OF CAST VOTES MUST ACCEPT",new Rectangle(r.X+142,vy+94,r.Width-154,26),12,false,false,NativeArt.Muted);
            }else{
                int votesCast=state.VoteYes+state.VoteNo;
                Text(g,Ui("VOTE")+": "+MapName(state.VoteMap)+" / "+state.VoteMode+" / "+state.VoteCamera+"  ·  "+state.VoteSeconds+"s  ·  "+Ui("YES")+" "+state.VoteYes+"  /  "+Ui("NO")+" "+state.VoteNo+"  ·  "+votesCast+(" "+Ui(votesCast==1?"VOTE":"VOTES")),new Rectangle(r.X+15,vy,r.Width-30,25),15,true);
                if(!String.IsNullOrEmpty(state.VoteProposer)&&String.Equals(state.VoteProposer,state.Name,StringComparison.OrdinalIgnoreCase))
                    Text(g,"YOUR VOTE WAS SUBMITTED — WAITING FOR PLAYERS",new Rectangle(r.X+12,vy+30,r.Width-24,34),13,false,false,NativeArt.Muted);
                else {Button(g,new Rectangle(r.X+12,vy+30,120,34),"ACCEPT",delegate{Send("voteCast","accept","1");},true);Button(g,new Rectangle(r.X+140,vy+30,120,34),"DECLINE",delegate{Send("voteCast","accept","0");});}
            }
        }
        int footerY=r.Bottom-68;Line(g,r.X+1,footerY-5,r.Right-1,footerY-5);
        string footer=count+(" "+Ui(IsZombieRoom(room)?"players or zombies ready · 60s countdown starts at":"players or bots ready · 60s countdown starts at")+" ")+room.MinimumPlayers;
        if(state.ManualActiveJoin&&!state.Host){
            if(state.Joined)footer=state.JoinInProgress?"PLEASE WAIT TO JOIN — STARTING GAME":"GAME LAUNCH REQUESTED";
            else if(!room.LateJoin)footer="LATE JOIN CLOSED FOR THIS MATCH";
            else if(!state.AppearanceSynced)footer="PREPARING YOUR CHARACTER — PLEASE WAIT";
            else if(state.Ready)footer="MATCH IN PROGRESS — PRESS JOIN GAME";
            else footer="WAITING FOR THE SERVER";
        }
        Text(g,footer,new Rectangle(r.X+15,footerY,r.Width-30,23),13,false,false,state.ManualActiveJoin?NativeArt.Yellow:NativeArt.Muted);
        if(state.Host&&(state.Ready||state.Starting)){Button(g,new Rectangle(r.X+15,r.Bottom-37,113,28),"CONSOLE",delegate{view="console";Invalidate();});Button(g,new Rectangle(r.X+136,r.Bottom-37,110,28),"STOP SERVER",delegate{Send("stop");});}
        else Button(g,new Rectangle(r.X+15,r.Bottom-37,118,28),state.Host?"CLOSE ROOM":"LEAVE ROOM",delegate{Send("leave");});
        bool hostStart=state.Host&&!state.QuickRoom&&!state.Ready&&!state.Starting;bool available=state.Connected&&state.AppearanceSynced&&!state.Starting&&(hostStart||state.Ready&&!state.Joined&&(!state.ManualActiveJoin||room.LateJoin));
        string actionTitle=hostStart?"START SERVER":state.Starting?"STARTING...":state.ManualActiveJoin&&state.Joined?(state.JoinInProgress?"PLEASE WAIT...":"JOIN REQUESTED"):"JOIN GAME";
        Button(g,new Rectangle(r.Right-204,r.Bottom-40,188,34),actionTitle,delegate{Send(hostStart?"start":"joinGame");if(hostStart){view="console";Invalidate();}},true,available);
    }
    void DrawChat(Graphics g,Rectangle r){
        bool inRoom=state!=null&&state.InRoom;
        if(state!=null&&state.Dedicated&&state.Host){Panel(g,r,"SERVER",34);Text(g,"Dedicated server: chat is disabled.",new Rectangle(r.X+12,r.Y+48,r.Width-24,46),15,false,false,NativeArt.Muted,StringAlignment.Center,true);return;}
        if(networkNet&&!inRoom&&(state==null||!state.NetChatConnected)){Panel(g,r,"CHAT",34);Text(g,"NET global chat is not connected.",new Rectangle(r.X+12,r.Y+48,r.Width-24,46),15,false,false,NativeArt.Muted,StringAlignment.Center,true);return;}
        Panel(g,r,inRoom?"⚑  ROOM CHAT":"▰  CHAT",34);string[] users=state==null?new string[0]:(inRoom&&roomChat?state.RoomNames:state.Online);Button(g,new Rectangle(r.Right-68,r.Y+3,58,28),users.Length+"  ☰",delegate{showOnline=!showOnline;Invalidate();},false,true,true);
        if(inRoom&&networkNet){roomChat=true;ChatTab(g,new Rectangle(r.X+11,r.Y+42,r.Width-22,33),"ROOM CHAT",delegate{},true,16);}else if(inRoom){ChatTab(g,new Rectangle(r.X+11,r.Y+42,(r.Width-30)/2,33),"ROOM CHAT",delegate{roomChat=true;chatScroll.Value=0;Invalidate();},roomChat);ChatTab(g,new Rectangle(r.X+19+(r.Width-30)/2,r.Y+42,(r.Width-30)/2,33),"GLOBAL CHAT",delegate{roomChat=false;chatScroll.Value=0;Invalidate();},!roomChat,16);}
        else ChatTab(g,new Rectangle(r.X+11,r.Y+42,r.Width-22,33),"GLOBAL (EVERYONE)",delegate{},true,16);
        int onlineWidth=showOnline?Math.Min(120,r.Width/3):0;var area=new Rectangle(r.X+12,r.Y+84,r.Width-24-onlineWidth,r.Height-132);if(area.Height<20)return;
        string[] lines=state==null?new string[0]:inRoom&&roomChat?state.RoomChat:state.GlobalChat;var sizes=new List<int>();int content=0;
        foreach(string line in lines){int height=Math.Max(22,(int)Math.Ceiling(g.MeasureString(line,NativeArt.Font(ChatFontSize),Math.Max(30,area.Width-18)).Height)+3);sizes.Add(height);content+=height;}
        bool nearEnd=chatScroll.Value>=Math.Max(0,chatScroll.Maximum-chatScroll.LargeChange-24);Scroll(chatScroll,area,content);if(nearEnd&&chatScroll.Visible){updating=true;chatScroll.Value=Math.Max(0,content-area.Height);updating=false;}
        var saved=g.Save();g.SetClip(area);int y=area.Y-chatScroll.Value;for(int i=0;i<lines.Length;i++){string line=lines[i];if(y+sizes[i]>=area.Y&&y<area.Bottom)RawText(g,line,new Rectangle(area.X,y,area.Width-(chatScroll.Visible?18:0),sizes[i]),ChatFontSize,false,false,null,StringAlignment.Near,true);y+=sizes[i];}g.Restore(saved);
        if(showOnline){int x=r.Right-onlineWidth-11;Line(g,x-5,r.Y+80,x-5,r.Bottom-51,Color.FromArgb(59,59,63));Text(g,"ONLINE",new Rectangle(x+5,r.Y+85,onlineWidth-10,22),14,false,false,NativeArt.Muted);int uy=r.Y+114;foreach(string user in users){if(uy+22>r.Bottom-52)break;using(var b=new SolidBrush(Color.FromArgb(72,219,80)))g.FillEllipse(b,x+5,uy+9,7,7);RawText(g,user,new Rectangle(x+18,uy,onlineWidth-23,25),15);uy+=25;}}
        Field(g,"message",new Rectangle(r.X+10,r.Bottom-42,r.Width-61,33),inRoom&&roomChat?"Write a message to server chat...":"Write a message to global chat...",18);Text(g,"☺",new Rectangle(r.Right-77,r.Bottom-40,22,29),20,false,false,NativeArt.Muted,StringAlignment.Center);Button(g,new Rectangle(r.Right-44,r.Bottom-42,34,33),"➤",SendChat,true);
    }
    Dictionary<string,string> DedicatedValues(){return new Dictionary<string,string>{{"dedicatedName",fields["dedicatedName"].Text},{"dedicatedCapacity",fields["dedicatedCapacity"].Text},{"dedicatedBots",fields["dedicatedBots"].Text},{"dedicatedInstances",fields["dedicatedInstances"].Text}};}
    void DrawDedicatedConsole(Graphics g,Rectangle r){
        Panel(g,r,"DEDICATED SERVER",43);
        if(state==null||!state.DedicatedConsoleAvailable){Text(g,"Dedicated server components are not installed on this machine.",new Rectangle(r.X+15,r.Y+55,r.Width-30,45),16,false,false,NativeArt.Muted);return;}
        int x=r.X+14,w=r.Width-28;Text(g,"MASTER SERVER ADDRESS",new Rectangle(x,r.Y+49,w,20),14,true);
        Field(g,"masterListen",new Rectangle(x,r.Y+72,Math.Max(120,r.Width-302),32),"Public or Tailscale IPv4");
        Button(g,new Rectangle(r.Right-278,r.Y+72,125,32),state.MasterRunning?"STOP MASTER":"START MASTER",delegate{if(state.MasterRunning)Send("stopMaster");else Send("startMaster","ip",fields["masterListen"].Text);},false,true);
        Button(g,new Rectangle(r.Right-145,r.Y+72,131,32),state.DedicatedRunning?"STOP SERVICE":"START SERVICE",delegate{if(state.DedicatedRunning)Send("stopDedicated");else Send("startDedicated",DedicatedValues());},true,true);
        int y=r.Y+114;Button(g,new Rectangle(x,y,154,30),state.ChatServerRunning?"STOP CHAT SERVER":"START CHAT SERVER",delegate{Send(state.ChatServerRunning?"stopChat":"startChat");},false,true);y+=38;int nameW=Math.Max(150,w-345);int col=(w-nameW-24)/3;
        Text(g,"SERVER NAME",new Rectangle(x,y,nameW,20),14,true);Text(g,"SLOTS",new Rectangle(x+nameW+8,y,col,20),14,true);Text(g,"BOTS",new Rectangle(x+nameW+16+col,y,col,20),14,true);Text(g,"MAX ROOMS",new Rectangle(x+nameW+24+col*2,y,col,20),14,true);
        y+=21;Field(g,"dedicatedName",new Rectangle(x,y,nameW,32),"Quick Match server name");Field(g,"dedicatedCapacity",new Rectangle(x+nameW+8,y,col,32));Field(g,"dedicatedBots",new Rectangle(x+nameW+16+col,y,col,32));Field(g,"dedicatedInstances",new Rectangle(x+nameW+24+col*2,y,col,32));
        y+=40;Button(g,new Rectangle(x,y,154,30),"SAVE SETTINGS",delegate{Send("saveDedicatedSettings",DedicatedValues());});
        Text(g,!String.IsNullOrEmpty(state.DedicatedSavedNotice)?state.DedicatedSavedNotice:state.DedicatedRunning?"Restart service to apply changes.":"Players vote for map, team and view.",new Rectangle(x+165,y,w-165,30),14,false,false,NativeArt.Muted);
        y+=39;Text(g,"Ports 8000–10000 · Recommended host RAM: 16 GB",new Rectangle(x,y,w,22),14,false,false,NativeArt.Muted);
        y+=27;using(var brush=new SolidBrush(Color.FromArgb(9,10,11)))g.FillRectangle(brush,r.X+1,y,r.Width-2,Math.Max(1,r.Bottom-y-51));
        consoleRect=new Rectangle(r.X+15,y+8,r.Width-30,Math.Max(1,r.Bottom-y-66));string[] lines=state.DedicatedLogs??new string[0];int content=lines.Length*22;bool end=consoleScroll.Value>=Math.Max(0,consoleScroll.Maximum-consoleScroll.LargeChange-24);Scroll(consoleScroll,consoleRect,content);if(end&&consoleScroll.Visible){updating=true;consoleScroll.Value=Math.Max(0,content-consoleRect.Height);updating=false;}var saved=g.Save();g.SetClip(consoleRect);using(var font=new Font("Consolas",13,FontStyle.Regular,GraphicsUnit.Pixel)){for(int i=0;i<lines.Length;i++){int ly=consoleRect.Y+i*22-consoleScroll.Value;if(ly+22>=consoleRect.Y&&ly<consoleRect.Bottom)NativeArt.TextAt(g,lines[i],new Rectangle(consoleRect.X,ly,consoleRect.Width-18,22),font,NativeArt.Text);}}g.Restore(saved);Button(g,new Rectangle(r.Right-153,r.Bottom-39,138,30),"BACK",delegate{view="connect";Invalidate();});
    }
    void DrawConsole(Graphics g,Rectangle r){Panel(g,r,"",43);Text(g,state.Starting?"STARTING SERVER — PLEASE WAIT":state.Ready?"SERVER RUNNING":"SERVER STOPPED",new Rectangle(r.X+15,r.Y,r.Width-170,43),20,true,true);Button(g,new Rectangle(r.Right-146,r.Y+6,132,31),"STOP SERVER",delegate{Send("stop");},true,state.Host&&(state.Starting||state.Ready));using(var brush=new SolidBrush(Color.FromArgb(9,10,11)))g.FillRectangle(brush,r.X+1,r.Y+44,r.Width-2,r.Height-99);consoleRect=new Rectangle(r.X+16,r.Y+59,r.Width-32,r.Height-115);string[] lines=state.Logs;int content=lines.Length*22;bool end=consoleScroll.Value>=Math.Max(0,consoleScroll.Maximum-consoleScroll.LargeChange-24);Scroll(consoleScroll,consoleRect,content);if(end&&consoleScroll.Visible){updating=true;consoleScroll.Value=Math.Max(0,content-consoleRect.Height);updating=false;}var saved=g.Save();g.SetClip(consoleRect);using(var font=new Font("Consolas",13,FontStyle.Regular,GraphicsUnit.Pixel)){for(int i=0;i<lines.Length;i++){int y=consoleRect.Y+i*22-consoleScroll.Value;if(y+22<consoleRect.Y||y>consoleRect.Bottom)continue;NativeArt.TextAt(g,lines[i],new Rectangle(consoleRect.X,y,consoleRect.Width-18,22),font,NativeArt.Text);}}g.Restore(saved);Button(g,new Rectangle(r.Right-153,r.Bottom-42,138,30),"BACK TO ROOM",delegate{view="room";Invalidate();});}
    void ShowHelp(){using(var d=new Form{Text="PUBG 2019 — Help",ClientSize=new Size(440,220),BackColor=NativeArt.Panel,ForeColor=NativeArt.Text,StartPosition=FormStartPosition.CenterParent,FormBorderStyle=FormBorderStyle.FixedDialog,MaximizeBox=false,MinimizeBox=false}){var label=new Label{Text="Create a LAN room, choose a map and a game mode, then select START SERVER.\r\n\r\nOther players can join from the list or enter the host's LAN address or room code. Choose a free slot to select a team.\r\n\r\nClick the character to open the wardrobe.",Font=NativeArt.Font(16),Bounds=new Rectangle(20,15,400,150)};var ok=new Button{Text="OK",DialogResult=DialogResult.OK,BackColor=NativeArt.Yellow,ForeColor=Color.Black,FlatStyle=FlatStyle.Flat,Bounds=new Rectangle(160,177,120,30)};d.Controls.Add(label);d.Controls.Add(ok);d.AcceptButton=ok;d.ShowDialog(FindForm());}}
    protected override void OnMouseMove(MouseEventArgs e){base.OnMouseMove(e);mouse=e.Location;var hit=hits.LastOrDefault(h=>h.Enabled&&h.Bounds.Contains(e.Location));Cursor=hit==null?Cursors.Default:Cursors.Hand;if(e.Button==MouseButtons.None)Invalidate();}
    protected override void OnMouseLeave(EventArgs e){mouse=new Point(-1,-1);Invalidate();base.OnMouseLeave(e);}
    protected override void OnMouseDown(MouseEventArgs e){base.OnMouseDown(e);if(e.Button!=MouseButtons.Left)return;if(e.Y>=0&&e.Y<54&&e.X>=Width-102&&e.X<Width){int i=(e.X-(Width-102))/34;Send(i==0?"minimize":i==1?"maximize":"windowClose");return;}Focus();var hit=hits.LastOrDefault(h=>h.Enabled&&h.Bounds.Contains(e.Location));if(hit!=null){focusHit=hits.IndexOf(hit);hit.Action();}}
    protected override void OnMouseDoubleClick(MouseEventArgs e){base.OnMouseDoubleClick(e);if(e.Y<72&&!hits.Any(h=>h.Enabled&&h.Bounds.Contains(e.Location)))Send("maximize");else if(view=="connect"&&tableRect.Contains(e.Location)&&Selected!=null)Send("join","address",address);}
    protected override void OnMouseWheel(MouseEventArgs e){base.OnMouseWheel(e);VScrollBar bar=chatRect.Contains(e.Location)?chatScroll:slotRect.Contains(e.Location)&&view=="room"?slotScroll:consoleRect.Contains(e.Location)&&(view=="console"||view=="dedicatedConsole")?consoleScroll:tableScroll;if(bar.Visible)bar.Value=Math.Max(0,Math.Min(bar.Maximum-bar.LargeChange+1,bar.Value-e.Delta/120*bar.SmallChange*3));}
    protected override bool IsInputKey(Keys keyData){return keyData==Keys.Enter||keyData==Keys.Space||base.IsInputKey(keyData);}
    protected override void OnKeyDown(KeyEventArgs e){base.OnKeyDown(e);if((e.KeyCode==Keys.Enter||e.KeyCode==Keys.Space)&&focusHit>=0&&focusHit<hits.Count&&hits[focusHit].Enabled)hits[focusHit].Action();}
    protected override void Dispose(bool disposing){if(disposing){foreach(var picture in images.Values)if(picture!=null)picture.Dispose();if(portrait!=null)portrait.Dispose();}base.Dispose(disposing);}
}
