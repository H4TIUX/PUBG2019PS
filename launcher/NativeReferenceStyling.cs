using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Linq;
using System.Windows.Forms;

// Native drawing measurements transcribed from html/style.css. No HTML engine is used.
internal sealed partial class NativeLauncherView {
    float Measure(Graphics g,string text,Font font){return g.MeasureString(text,font,10000,StringFormat.GenericTypographic).Width;}
    void Symbol(Graphics g,string text,Rectangle r,float size,Color color){using(var font=new Font("Segoe UI Symbol",size,FontStyle.Regular,GraphicsUnit.Pixel))NativeArt.TextAt(g,text,r,font,color,StringAlignment.Center);}
    void DrawCanvasBackground(Graphics g,int w,int h){
        using(var path=new GraphicsPath()){
            path.AddEllipse(-w*.5f,72-h*.3f,w*1.4f,h*1.55f);
            using(var brush=new PathGradientBrush(path)){brush.CenterPoint=new PointF(w*.19f,72+(h-124)*.45f);brush.CenterColor=Color.FromArgb(43,43,48);brush.SurroundColors=new[]{NativeArt.Bg};brush.InterpolationColors=new ColorBlend{Colors=new[]{NativeArt.Bg,Color.FromArgb(23,24,29),Color.FromArgb(43,43,48)},Positions=new[]{0f,.51f,1f}};g.FillRectangle(brush,0,72,w,h-124);}
        }
    }
    void DrawHeader(Graphics g,int w){
        using(var b=new LinearGradientBrush(new Rectangle(0,0,w,72),Color.FromArgb(24,25,30),Color.FromArgb(17,18,22),90))g.FillRectangle(b,0,0,w,72);
        Line(g,0,71,w,71,Color.FromArgb(41,42,46));
        using(var font=new Font("Arial",11,FontStyle.Regular,GraphicsUnit.Pixel))NativeArt.TextAt(g,state==null?"V0.71":state.Version,new Rectangle(5,2,75,13),font,Color.FromArgb(153,154,161));
        bool compact=w<=1450;int pad=compact?14:(int)Math.Round(w*.024),gap=compact?8:w<=1450?14:25;float logoFont=29;
        int logoW=(int)Math.Ceiling(Measure(g,"PUBG",NativeArt.Heavy(logoFont)))+12;
        var logo=new Rectangle(pad,14,logoW,43);
        using(var pen=new Pen(NativeArt.Yellow,5))g.DrawRectangle(pen,logo.X+2.5f,logo.Y+2.5f,logo.Width-5,logo.Height-5);
        NativeArt.TextAt(g,"PUBG",new Rectangle(logo.X+6,logo.Y+3,logo.Width-12,37),NativeArt.Heavy(logoFont),NativeArt.Yellow,StringAlignment.Center);
        int brandX=logo.Right+(compact?10:20);float small=compact?12:w<=1450?16:20,large=compact?18:w<=1450?23:28;
        int brandW=(int)Math.Ceiling(Measure(g,"BATTLEGROUNDS",NativeArt.Font(large,true,true)));
        Text(g,"PLAYERUNKNOWN'S",new Rectangle(brandX,18,brandW,18),small,true,true);
        Text(g,"BATTLEGROUNDS",new Rectangle(brandX,32,brandW,25),large,true,true);
        int x=brandX+brandW+gap;
        string[] names={"JOIN","HOST SERVER","DEDICATED","SETTINGS","EXIT"};string[] commands={"connect","host","dedicatedConsole","profile","exit"};
        for(int i=0;i<names.Length;i++){
            float navFont=compact?14:17;int index=i;string navLabel=Ui(names[i]);
            int remaining=Math.Max(35,(w-269-x-4*(names.Length-i-1))/(names.Length-i));
            int width=Math.Min(remaining,(int)Math.Ceiling(Measure(g,navLabel,NativeArt.Font(navFont,true,true)))+(compact?42:64));
            var r=new Rectangle(x,11,width,49);bool selected=i<3&&((i==0&&view=="connect")||(i==1&&view=="host")||(i==2&&view=="dedicatedConsole"));
            if(selected){using(var b=new LinearGradientBrush(r,Color.Transparent,Color.Transparent,0f)){b.InterpolationColors=new ColorBlend{Colors=new[]{Color.Transparent,Color.FromArgb(118,93,0),Color.FromArgb(71,61,11),Color.Transparent},Positions=new[]{0f,.2f,.8f,1f}};g.FillRectangle(b,r);}using(var p=new Pen(Color.FromArgb(241,183,0),3))g.DrawLine(p,r.X+2,r.Bottom-1,r.Right-2,r.Bottom-1);}
            Color color=selected?Color.FromArgb(255,196,0):Color.FromArgb(194,195,204);
            DrawNavIcon(g,i==2?4:i>2?i-1:i,new Rectangle(x+(compact?4:14),23,23,24),color);
            Text(g,names[i],new Rectangle(x+(compact?30:50),11,width-(compact?36:64),49),navFont,true,true,color);
            HitBox(r,delegate{if(index<3)Go(commands[index]);else Send(commands[index]);});x+=width+4;
        }
        // The title-bar controls are anchored to the physical right edge.
        int bx=w-102;
        for(int i=0;i<3;i++){
            int index=i;var r=new Rectangle(bx+34*i,0,34,54);
            if(r.Contains(mouse))using(var hover=new SolidBrush(i==2?Color.FromArgb(196,43,28):Color.FromArgb(45,46,50)))g.FillRectangle(hover,r);
            using(var pen=new Pen(Color.White,1.5f)){
                int cx=r.X+r.Width/2,cy=27;
                if(i==0)g.DrawLine(pen,cx-4,cy+3,cx+4,cy+3);
                else if(i==1)g.DrawRectangle(pen,cx-4,cy-4,8,8);
                else{g.DrawLine(pen,cx-4,cy-4,cx+4,cy+4);g.DrawLine(pen,cx+4,cy-4,cx-4,cy+4);}
            }
            HitBox(r,delegate{Send(index==0?"minimize":index==1?"maximize":"windowClose");});
        }
        var help=new Rectangle(bx-55,15,30,25);NativeArt.Box(g,help,Color.FromArgb(29,30,34),Color.FromArgb(113,113,119),4);Text(g,"?",help,16,false,false,null,StringAlignment.Center);HitBox(help,ShowHelp);
        int switchX=help.X-112;var lan=new Rectangle(switchX,15,50,25);var net=new Rectangle(switchX+50,15,50,25);
        NativeArt.Box(g,lan,networkNet?Color.FromArgb(30,31,35):NativeArt.Yellow,networkNet?Color.FromArgb(107,108,114):NativeArt.Yellow,4);
        NativeArt.Box(g,net,networkNet?NativeArt.Yellow:Color.FromArgb(30,31,35),networkNet?NativeArt.Yellow:Color.FromArgb(107,108,114),4);
        Text(g,"LAN",lan,13,true,false,networkNet?NativeArt.Text:Color.Black,StringAlignment.Center);
        Text(g,"NET",net,13,true,false,networkNet?Color.Black:NativeArt.Text,StringAlignment.Center);
        HitBox(lan,delegate{SelectNetwork(false);});HitBox(net,delegate{SelectNetwork(true);});
        if(switchX>x)HitBox(new Rectangle(x,0,switchX-x,14),delegate{Send("drag");});
    }
    void DrawNavIcon(Graphics g,int kind,Rectangle r,Color color){
        if(kind==0){var saved=g.Save();g.TranslateTransform(r.X+12,r.Y+12);g.RotateTransform(42);using(var pen=new Pen(color,2.2f)){using(var a=NativeArt.Path(new RectangleF(-7,-14,12,18),5))g.DrawPath(pen,a);using(var b=NativeArt.Path(new RectangleF(-7,-2,12,18),5))g.DrawPath(pen,b);g.DrawLine(pen,-1,-5,-1,7);}g.Restore(saved);}
        else if(kind==1){using(var pen=new Pen(color,2)){g.DrawLine(pen,r.X+4,r.Y+12,r.Right-4,r.Y+12);g.DrawLine(pen,r.X+12,r.Y+4,r.X+12,r.Bottom-4);}}
        else if(kind==2)Symbol(g,"⚙",r,25,color);
        else if(kind==4){using(var pen=new Pen(color,2)){g.DrawRectangle(pen,r.X+3,r.Y+3,18,7);g.DrawRectangle(pen,r.X+3,r.Y+13,18,7);g.DrawLine(pen,r.X+7,r.Y+6,r.X+9,r.Y+6);g.DrawLine(pen,r.X+7,r.Y+16,r.X+9,r.Y+16);}}
        else {var saved=g.Save();g.TranslateTransform(r.X,r.Y);g.ScaleTransform(r.Width/24f,r.Height/24f);using(var pen=new Pen(color,2)){pen.StartCap=pen.EndCap=LineCap.Round;g.DrawLines(pen,new[]{new Point(10,20),new Point(4,20),new Point(4,4),new Point(10,4)});g.DrawLines(pen,new[]{new Point(13,8),new Point(17,12),new Point(13,16)});g.DrawLine(pen,7,12,17,12);}g.Restore(saved);}
    }
    void DrawHost(Graphics g,Rectangle r){
        bool compact=Width<=1450,narrow=r.Width<540;int pad=compact?14:20,gap=compact?10:14;
        int x=r.X+pad+1,y=r.Y+60,inner=r.Width-2*pad-2;
        int first=(int)((inner-gap*2)*1.1/2.82),second=(int)((inner-gap*2)/2.82),third=inner-gap*2-first-second;
        if(narrow){first=(inner-gap)/2;second=inner-first-gap;third=second;}
        int labelH=18,fieldH=32,settingW=first;
        // The CSS content determines panel height; it is not stretched to fill the center column.
        int contentEnd=y+386,actionY=contentEnd+11;
        int panelH=actionY-r.Y+48+10;var box=new Rectangle(r.X,r.Y,r.Width,panelH);
        Panel(g,box,"",43);DrawListIcon(g,new Rectangle(r.X+16,r.Y+13,20,18));Text(g,"HOST SERVER",new Rectangle(r.X+46,r.Y,r.Width-61,43),23,true,true);
        Text(g,"Server name",new Rectangle(x,y,settingW,labelH),15,true);Field(g,"serverName",new Rectangle(x,y+22,settingW,fieldH));y+=64;
        Text(g,"Password (optional)",new Rectangle(x,y,settingW,labelH),15,true);Field(g,"password",new Rectangle(x,y+22,settingW,fieldH),"(empty = no password)",25);
        var eye=new Rectangle(x+settingW-30,y+25,23,25);Symbol(g,"◉",eye,19,Color.FromArgb(221,221,221));HitBox(eye,delegate{passwordVisible=!passwordVisible;fields["password"].UseSystemPasswordChar=!passwordVisible;});y+=64;
        foreach(string id in new[]{"capacity","bots","minimum"}){
            if(id=="bots" && map=="Training") continue;
            Text(g,id=="capacity"?"Max. players":id=="bots"?(zombieRule!="None"?"Zombies":"Bots"):"Players needed for 60s countdown",new Rectangle(x,y,settingW-88,32),15,true,false,null,StringAlignment.Near,true);
            Field(g,id,new Rectangle(x+settingW-78,y,78,32));
            y+=40;
        }
        y-=1;
        Check(g,new Rectangle(x,y,settingW,25),networkNet?"List publicly":"Visible on LAN",visibleLan,delegate{visibleLan=!visibleLan;Invalidate();});y+=25;
        Check(g,new Rectangle(x,y,settingW,25),"Join localhost",autoJoin,delegate{autoJoin=!autoJoin;if(autoJoin)dedicated=false;Invalidate();});y+=25;
        if(map=="Training")Text(g,"LATE JOIN ENABLED",new Rectangle(x,y,settingW,25),14,true,false,NativeArt.Yellow);
        else Check(g,new Rectangle(x,y,settingW,25),"Allow late join",lateJoin,delegate{lateJoin=!lateJoin;Invalidate();});y+=25;
        Text(g,"Room code",new Rectangle(x,y,settingW,labelH),15,true);Field(g,"code",new Rectangle(x,y+22,settingW,32));
        int mx=x+first+gap,my=r.Y+60,mapW=second-gap-1;
        Line(g,mx,my,mx,contentEnd,Color.FromArgb(85,85,85));mx+=gap+1;Text(g,"Map",new Rectangle(mx,my,mapW,18),15,true);
        string[] ids={"Erangel","ErangelClassic","Miramar","Sanhok","Vikendi","Training"};int mw=(mapW-8)/2,mh=narrow?44:77;
        for(int i=0;i<ids.Length;i++){string id=ids[i];var card=new Rectangle(mx+(i%2)*(mw+8),my+22+(i/2)*(mh+8),mw,mh);using(var clip=NativeArt.Path(card,5)){var saved=g.Save();g.SetClip(clip);NativeArt.Cover(g,images[id],card);using(var b=new LinearGradientBrush(card,Color.FromArgb(0,0,0,0),Color.FromArgb(230,0,0,0),90)){b.InterpolationColors=new ColorBlend{Colors=new[]{Color.FromArgb(0,0,0,0),Color.FromArgb(0,0,0,0),Color.FromArgb(230,0,0,0)},Positions=new[]{0f,Math.Max(0f,1f-35f/card.Height),1f}};g.FillRectangle(b,card);}g.Restore(saved);using(var pen=new Pen(id==map?Color.FromArgb(255,196,0):Color.FromArgb(67,67,72),id==map?3:2))g.DrawPath(pen,clip);}bool wrap=Measure(g,MapName(id),NativeArt.Font(16,true))>card.Width-4;Text(g,MapName(id),new Rectangle(card.X+2,card.Bottom-(wrap?40:25),card.Width-4,wrap?38:22),16,true,false,Color.White,StringAlignment.Center,wrap);if(id==map){using(var b=new SolidBrush(Color.FromArgb(255,196,0)))g.FillRectangle(b,card.Right-24,card.Y+2,22,23);Symbol(g,"✔",new Rectangle(card.Right-24,card.Y,24,23),17,Color.FromArgb(17,17,17));}HitBox(card,delegate{map=id;if(map=="Training"){mode="Solo";camera="TPP";zombieRule="None";fields["bots"].Text="0";}Invalidate();});}
        int modeX=x+first+gap+second+gap,modeY=r.Y+60,modeW=third-gap-1;
        if(narrow){modeX=mx-gap-1;modeY=my+22+3*(mh+8)+5;modeW=mapW;}
        Line(g,modeX,modeY,modeX,contentEnd,Color.FromArgb(85,85,85));modeX+=gap+1;Text(g,map=="Training"?"Training":"Game mode",new Rectangle(modeX,modeY,modeW,18),15,true);
        if(map!="Training"){
        modeY+=22;Text(g,"Perspective",new Rectangle(modeX,modeY,69,22),14);int radioY=modeW<190?modeY+22:modeY,radioX=modeW<190?modeX:modeX+78;
        Radio(g,new Rectangle(radioX,radioY,53,22),"FPP",camera=="FPP",delegate{camera="FPP";Invalidate();});Radio(g,new Rectangle(radioX+62,radioY,53,22),"TPP",camera=="TPP",delegate{camera="TPP";Invalidate();});modeY=radioY+32;
        foreach(string item in new[]{"Solo","Duo","Squad"}){string choice=item;var row=new Rectangle(modeX,modeY,modeW,33);Radio(g,row,zombieRule=="None"?"Battle Royale ("+item+")":item,mode==item,delegate{if(map!="Training"||choice=="Solo")mode=choice;Invalidate();},true);Line(g,modeX,row.Bottom,modeX+modeW,row.Bottom,Color.FromArgb(57,57,61));modeY+=33;}
        Text(g,"Match rules",new Rectangle(modeX,modeY+5,modeW,18),15,true);modeY+=25;
        var ruleBox=new Rectangle(modeX,modeY,modeW,32);NativeArt.Box(g,ruleBox,NativeArt.Field,NativeArt.Muted,4);
        string ruleTitle=zombieRule=="None"?"Standard":zombieRule=="BattleRoyale"?"Zombie Battle Royale":"Zombie "+zombieRule;
        Text(g,ruleTitle,new Rectangle(ruleBox.X+8,ruleBox.Y,Math.Max(0,ruleBox.Width-34),ruleBox.Height),14,true);
        Symbol(g,"▼",new Rectangle(ruleBox.Right-24,ruleBox.Y,18,ruleBox.Height),12,NativeArt.Text);
        HitBox(ruleBox,delegate{ShowZombieRuleMenu(ruleBox);},true,"Match rules");
        }else Text(g,"20 MIN · NO BOTS",new Rectangle(modeX,modeY+25,modeW,40),14,true);
        int buttonX=narrow?x+first+gap:modeX,buttonW=narrow?second:modeW;
        var action=new Rectangle(buttonX,actionY,buttonW,48);Button(g,action,"",CreateRoom,true);float size=buttonW<150?17:19;int labelWidth=(int)Math.Ceiling(Measure(g,Ui("CREATE ROOM"),NativeArt.Heavy(size))),groupWidth=labelWidth+36,start=action.X+(action.Width-groupWidth)/2;Symbol(g,"▶",new Rectangle(start,action.Y+8,22,32),20,Color.FromArgb(8,8,8));NativeArt.TextAt(g,Ui("CREATE ROOM"),new Rectangle(start+36,action.Y,labelWidth+2,action.Height),NativeArt.Heavy(size),Color.FromArgb(8,8,8),StringAlignment.Center);

    }
    void ShowZombieRuleMenu(Rectangle anchor){
        var menu=new ContextMenuStrip{BackColor=NativeArt.Panel,ForeColor=NativeArt.Text,ShowImageMargin=false};zombieMenu=menu;
        foreach(string rule in new[]{"None","BattleRoyale","Survival","Elimination"}){
            string choice=rule;string title=rule=="None"?"Standard":rule=="BattleRoyale"?"Zombie Battle Royale":"Zombie "+rule;
            var item=new ToolStripMenuItem(Ui(title)){Checked=zombieRule==rule,Enabled=map!="Training"||rule=="None"};
            item.Click+=delegate{zombieRule=choice;Invalidate();};menu.Items.Add(item);
        }
        menu.Closed+=delegate{if(zombieMenu==menu)zombieMenu=null;if(IsHandleCreated&&!IsDisposed)BeginInvoke((Action)delegate{menu.Dispose();});else menu.Dispose();};menu.Show(this,new Point(anchor.X,anchor.Bottom));
    }
    void ChatTab(Graphics g,Rectangle r,string title,Action action,bool active,float fontSize=13){NativeArt.Box(g,r,active?Color.FromArgb(255,192,0):Color.FromArgb(40,41,45),active?Color.FromArgb(255,212,77):Color.FromArgb(105,105,111));Text(g,title,r,fontSize,true,true,active?Color.FromArgb(8,8,8):Color.FromArgb(221,221,221),StringAlignment.Center);HitBox(r,action);}
    void DrawPing(Graphics g,Rectangle r,int ping){int bars=ping>150?2:ping>80?3:4;using(var b=new SolidBrush(PingColor(ping)))for(int i=0;i<bars;i++)g.FillRectangle(b,r.X+i*5,r.Bottom-(4+i*3),3,4+i*3);}
    void DrawListIcon(Graphics g,Rectangle r){using(var b=new SolidBrush(NativeArt.Text))for(int i=0;i<3;i++){g.FillRectangle(b,r.X,r.Y+i*7,6,4);g.FillRectangle(b,r.X+9,r.Y+i*7,11,4);}}
    void DrawFooter(Graphics g,int w,int h){using(var b=new SolidBrush(Color.FromArgb(21,22,26)))g.FillRectangle(b,0,h-52,w,52);Line(g,0,h-52,w,h-52,Color.FromArgb(56,57,61));Symbol(g,"☕",new Rectangle(20,h-38,16,22),13,NativeArt.Text);Text(g,"Support on Ko-fi",new Rectangle(44,h-38,140,22),13,true,false,Color.FromArgb(255,196,0));HitBox(new Rectangle(20,h-38,160,22),delegate{Send("kofi");});int width=(int)Math.Ceiling(Measure(g,"Link to Github repository ↗",NativeArt.Font(13)));var top=new Rectangle(w-20-width,h-44,width,18);var lower=new Rectangle(top.X,h-26,width,18);Text(g,"Special THX to H4TIUX",top,13,false,false,Color.FromArgb(228,228,230),StringAlignment.Center);Text(g,"Link to Github repository ↗",lower,13,false,false,Color.FromArgb(228,228,230),StringAlignment.Center);HitBox(top,delegate{Send("credit");});HitBox(lower,delegate{Send("repository");});}
}

internal sealed class NativeDarkComboBox:ComboBox {
    protected override void WndProc(ref Message message){base.WndProc(ref message);if(message.Msg==0x000f){using(var g=Graphics.FromHwnd(Handle)){using(var b=new SolidBrush(NativeArt.Field))g.FillRectangle(b,Width-20,1,19,Height-2);using(var p=new Pen(Color.FromArgb(122,122,126)))g.DrawRectangle(p,0,0,Width-1,Height-1);using(var p=new Pen(NativeArt.Text,1.5f))g.DrawLines(p,new[]{new Point(Width-15,Height/2-2),new Point(Width-10,Height/2+3),new Point(Width-5,Height/2-2)});}}}
}
