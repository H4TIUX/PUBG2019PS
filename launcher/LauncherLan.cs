using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Net;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Windows.Forms;

internal sealed partial class LauncherForm
{
    [System.Runtime.InteropServices.DllImport("user32.dll")] private static extern bool ReleaseCapture();
    [System.Runtime.InteropServices.DllImport("user32.dll")] private static extern IntPtr SendMessage(IntPtr h,int message,IntPtr w,IntPtr l);
    private StyledModeSelector _teamMode, _cameraMode, _teamChoice;
    private StyledTextField _serverName;
    private TextBox _roomPasswordBox, _chatInput, _chatHistory;
    private ListBox _roomList, _players;
    private Button _globalTab, _roomTab, _sendChat, _joinRoom, _leaveRoom, _chooseTeam;
    private Label _roomInfo, _onlineInfo, _lanNotice;
    private TableLayoutPanel _lanRoot, _hostFields, _chatPanel;
    private Panel _headerPanel;
    private readonly Dictionary<Control,string> _lanLabels = new Dictionary<Control,string>();
    private readonly List<string> _globalHistory = new List<string>(), _roomHistory = new List<string>();
    private string[] _onlineNames = new string[0], _roomNames = new string[0];
    private sealed class WebSeat { public string name; public int slot; }
    private WebSeat[] _webSeats=new WebSeat[0];private bool _webSlotsReady;
    private LanGlobal _globalNetwork;
    private LanLobbyServer _lobbyServer;
    private LanLobbyClient _roomClient;
    private LanServer _connectedRoom;
    private readonly System.Windows.Forms.Timer _lanTimer = new System.Windows.Forms.Timer();
    private bool _scanBusy, _roomConnected, _roomStarting, _roomReady, _roomMatch, _launchFromRoom, _showRoomChat, _roomJoinedGame, _manualActiveJoin, _initialRoomHandshake;
    private bool _lanDisposed, _appearanceSynced;
    private int _roomGeneration, _chosenTeam;
    private DateTime _nextScan, _lastChatSend;
    private string _roomPlayerName = "";
    private string _hostZombieRule = "None";
    private string AdvertisedHostName() {
        string name=_serverName.Text.Trim();
        if(_hostZombieRule=="None"||String.IsNullOrWhiteSpace(name))return name;
        string tag=_hostZombieRule=="BattleRoyale"?"[Z-BR] ":_hostZombieRule=="Survival"?"[Z-SURV] ":"[Z-ELIM] ";
        int limit=32-tag.Length;if(name.Length>limit){name=name.Substring(0,limit);if(name.Length>0&&Char.IsHighSurrogate(name[name.Length-1]))name=name.Substring(0,name.Length-1);}
        return tag+name;
    }
    private int _roomCapacity=100, _minimumPlayers=1;
    private bool _visibleLan=true, _allowLateJoin;
    private string _roomCode="PUBG-"+Guid.NewGuid().ToString("N").Substring(0,8).ToUpperInvariant();
    // The non-playing dedicated observer must not reserve the owner's player name.
    private readonly string _dedicatedObserverName="Server_"+Guid.NewGuid().ToString("N").Substring(0,12);
    private LanServer HostRoomDetails(bool advertise=false) {return new LanServer{Name=advertise?AdvertisedHostName():_serverName.Text.Trim(),ZombieRule=_hostZombieRule,Map=((MapOption)_map.SelectedItem).Value,TeamSize=SelectedTeamSize,Camera=_cameraMode.SelectedIndex==1?"FPP":"TPP",Address="127.0.0.1:"+_gamePort,LobbyPort=_lobbyPort,QuickMatch=_quickMatchHost,Protected=_roomPasswordBox.Text.Length>0,Capacity=_roomCapacity,Bots=(int)_bots.Value,Players=_lobbyServer==null?(_autoJoinLocal.Checked?1:0):_lobbyServer.PlayerCount,MinimumPlayers=_minimumPlayers,LateJoin=_allowLateJoin,Code=_roomCode,Ready=_roomStarting||_roomReady};}

    private string L(string text) { return LanText.Get(_uiLanguage, text); }
    private Label LanLabel(string text)
    {
        var c=new Label {Text=text,ForeColor=Muted,BackColor=Bg,AutoSize=false,Dock=DockStyle.Fill,TextAlign=ContentAlignment.MiddleLeft,Margin=new Padding(0,2,6,2),Font=new Font("Segoe UI",9F)};
        _lanLabels[c]=text; return c;
    }
    private Button LanButton(string text, string name, bool gold)
    {
        var c=new Button {Name=name,Text=text,FlatStyle=FlatStyle.Flat,BackColor=gold?Yellow:Field,ForeColor=gold?Color.Black:Fg,Dock=DockStyle.Fill,Margin=new Padding(3),Font=new Font("Segoe UI",9F,FontStyle.Bold)};
        c.FlatAppearance.BorderColor=Color.FromArgb(75,80,83);_lanLabels[c]=text;return c;
    }
    private StyledModeSelector LanCombo(string name, params string[] items)
    {
        var c=new StyledModeSelector {Name=name,BackColor=Field,ForeColor=Muted,Dock=DockStyle.Fill,Font=new Font("Segoe UI",10F),Margin=new Padding(3,6,3,3)};
        c.Items.AddRange(items);c.SelectedIndex=0;return c;
    }
    private TableLayoutPanel LanTable(int columns, params float[] widths)
    {
        var t=new TableLayoutPanel {Dock=DockStyle.Fill,ColumnCount=columns,BackColor=Bg,Margin=new Padding(0),Padding=new Padding(8)};
        foreach(float width in widths)t.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,width));return t;
    }
    private void BuildLanUi()
    {
        DoubleBuffered=true;
        _headerPanel=new Panel {Name="LauncherHeader",Dock=DockStyle.Top,Height=150,BackColor=Bg};
        _headerPanel.MouseDown+=delegate(object sender,MouseEventArgs e){if(e.Button==MouseButtons.Left){ReleaseCapture();SendMessage(Handle,0xA1,new IntPtr(2),IntPtr.Zero);}};
        _headerPanel.DoubleClick+=delegate{WindowState=WindowState==FormWindowState.Maximized?FormWindowState.Normal:FormWindowState.Maximized;};
        // Header is painted by the form so it cannot cover the caption controls.

        var version=new Label {Name="LauncherVersion",Text=DisplayVersion,BackColor=Color.Transparent,ForeColor=Muted,AutoSize=true,Location=new Point(12,7),Font=new Font("Segoe UI",9F)};Controls.Add(version);
        _lanRoot=LanTable(2,46,54);_lanRoot.Name="LanContent";_lanRoot.Dock=DockStyle.None;
        _lanRoot.RowCount=3;_lanRoot.RowStyles.Add(new RowStyle(SizeType.Absolute,318));_lanRoot.RowStyles.Add(new RowStyle(SizeType.Percent,100));_lanRoot.RowStyles.Add(new RowStyle(SizeType.Absolute,112));Controls.Add(_lanRoot);
        _hostFields=LanTable(4,23,37,18,22);_hostFields.Margin=new Padding(0,0,8,8);_hostFields.RowCount=8;
        for(int i=0;i<7;i++)_hostFields.RowStyles.Add(new RowStyle(SizeType.Absolute,33));_hostFields.RowStyles.Add(new RowStyle(SizeType.Percent,100));
        _name=new StyledTextField {Name="PlayerName",MaxLength=24,Dock=DockStyle.Fill,Font=new Font("Segoe UI",10F),Margin=new Padding(3)};
        _serverName=new StyledTextField {Name="ServerName",MaxLength=32,Text="Battlegrounds LAN",Dock=DockStyle.Fill,Font=new Font("Segoe UI",10F),Margin=new Padding(3)};
        _map=new MapSelector {Name="Map",Dock=DockStyle.Fill,Font=new Font("Segoe UI",10F),Margin=new Padding(3)};
        string[] labels={"Miramar","Erangel (new)","Erangel (classic)","Sanhok","Vikendi","Training"};
        string[] ids={"Miramar","Erangel","ErangelClassic","Sanhok","Vikendi","Training"};
        string[] resources={"MapMiramar","MapErangel","MapErangelClassic","MapSanhok","MapVikendi","MapTraining"};
        for(int i=0;i<ids.Length;i++)_map.Items.Add(new MapOption(labels[i],ids[i],LoadBitmapResource("Launcher."+resources[i])));
        _map.SelectedIndex=0;
        _bots=new BotCountSelector {Name="BotCount",Minimum=0,Maximum=95,Value=95,Dock=DockStyle.Fill,Font=new Font("Segoe UI",11F),Margin=new Padding(3)};
        _teamMode=LanCombo("TeamMode","Solo","Duo","Squad");_cameraMode=LanCombo("CameraMode","TPP","FPP");
        _roomPasswordBox=new TextBox {Name="RoomPassword",UseSystemPasswordChar=true,MaxLength=64,BackColor=Field,ForeColor=Fg,Dock=DockStyle.Fill,BorderStyle=BorderStyle.FixedSingle,Margin=new Padding(3,6,3,3)};
        _autoJoinLocal=new ArtworkCheckBox {Name="AutoJoin",Text="Join automatically",Checked=true,Dock=DockStyle.Fill,BackColor=Bg,ForeColor=Fg,Font=new Font("Segoe UI",10F)};
        _hostLocal=new ArtworkCheckBox {Name="LocalHost",Checked=true,Visible=false};Controls.Add(_hostLocal);
        _mapHint=new ArtworkLabel {Text="",ForeColor=Muted,Dock=DockStyle.Fill,BackColor=Bg,Font=new Font("Segoe UI",9F)};
        _start=new ArtworkButton {Name="Launch",Text="HOST ROOM",Dock=DockStyle.Fill,Font=new Font("Impact",23F),BackgroundImage=CreateButtonArtwork(),Margin=new Padding(3)};_start.Click+=StartClicked;
        AddHostRow(0,"PLAYER NAME",_name);AddHostRow(1,"SERVER NAME",_serverName);
        _hostFields.Controls.Add(LanLabel("MAP"),0,2);_hostFields.Controls.Add(_map,1,2);_hostFields.Controls.Add(LanLabel("BOTS"),2,2);_hostFields.Controls.Add(_bots,3,2);
        _hostFields.Controls.Add(LanLabel("MODE"),0,3);_hostFields.Controls.Add(_teamMode,1,3);_hostFields.Controls.Add(LanLabel("VIEW"),2,3);_hostFields.Controls.Add(_cameraMode,3,3);
        AddHostRow(4,"ROOM PASSWORD",_roomPasswordBox);
        _hostFields.Controls.Add(_autoJoinLocal,0,5);_hostFields.SetColumnSpan(_autoJoinLocal,4);
        _hostFields.Controls.Add(_mapHint,0,6);_hostFields.SetColumnSpan(_mapHint,4);
        _hostFields.Controls.Add(_start,0,7);_hostFields.SetColumnSpan(_start,4);_lanRoot.Controls.Add(_hostFields,0,0);

        var rooms=LanTable(3,47,27,26);rooms.Margin=new Padding(0,0,8,0);rooms.RowCount=4;
        rooms.RowStyles.Add(new RowStyle(SizeType.Absolute,26));rooms.RowStyles.Add(new RowStyle(SizeType.Percent,100));rooms.RowStyles.Add(new RowStyle(SizeType.Absolute,32));rooms.RowStyles.Add(new RowStyle(SizeType.Absolute,36));
        var roomsTitle=LanLabel("LAN ROOMS");roomsTitle.ForeColor=Yellow;rooms.Controls.Add(roomsTitle,0,0);rooms.SetColumnSpan(roomsTitle,3);
        _roomList=new ListBox {Name="LanRooms",Dock=DockStyle.Fill,BackColor=Field,ForeColor=Fg,BorderStyle=BorderStyle.None,HorizontalScrollbar=true,IntegralHeight=false,Font=new Font("Segoe UI",9F)};
        rooms.Controls.Add(_roomList,0,1);rooms.SetColumnSpan(_roomList,3);
        _address=new StyledTextField {Name="ServerAddress",Text="",Dock=DockStyle.Fill,Font=new Font("Segoe UI",9F),MaxLength=21,Margin=new Padding(3)};
        rooms.Controls.Add(_address,0,2);rooms.SetColumnSpan(_address,2);
        var direct=LanButton("JOIN BY IP","JoinByIp",false);rooms.Controls.Add(direct,2,2);direct.Click+=delegate {JoinByAddress();};
        var refresh=LanButton("REFRESH","RefreshRooms",false);rooms.Controls.Add(refresh,0,3);refresh.Click+=delegate{ScanRooms();};
        _joinRoom=LanButton("JOIN ROOM","JoinRoom",true);rooms.Controls.Add(_joinRoom,1,3);_joinRoom.Click+=delegate{JoinSelectedRoom();};
        _leaveRoom=LanButton("LEAVE ROOM","LeaveRoom",false);rooms.Controls.Add(_leaveRoom,2,3);_leaveRoom.Click+=delegate{LeaveRoom();};
        _roomList.DoubleClick+=delegate{JoinSelectedRoom();};_lanRoot.Controls.Add(rooms,0,1);

        _chatPanel=LanTable(4,38,30,16,16);_chatPanel.Name="LanChat";_chatPanel.RowCount=5;
        _chatPanel.RowStyles.Add(new RowStyle(SizeType.Absolute,36));_chatPanel.RowStyles.Add(new RowStyle(SizeType.Absolute,42));_chatPanel.RowStyles.Add(new RowStyle(SizeType.Percent,100));_chatPanel.RowStyles.Add(new RowStyle(SizeType.Absolute,38));_chatPanel.RowStyles.Add(new RowStyle(SizeType.Absolute,36));
        _globalTab=LanButton("GLOBAL CHAT","GlobalChatTab",true);_roomTab=LanButton("ROOM CHAT","RoomChatTab",false);_chatPanel.Controls.Add(_globalTab,0,0);_chatPanel.Controls.Add(_roomTab,1,0);
        _onlineInfo=LanLabel("ONLINE");_chatPanel.Controls.Add(_onlineInfo,2,0);_chatPanel.SetColumnSpan(_onlineInfo,2);
        _globalTab.Click+=delegate{ShowChat(false);};_roomTab.Click+=delegate{ShowChat(true);};
        _roomInfo=LanLabel("No room joined");_roomInfo.ForeColor=Yellow;_chatPanel.Controls.Add(_roomInfo,0,1);_chatPanel.SetColumnSpan(_roomInfo,4);
        _chatHistory=new TextBox {Name="ChatHistory",Multiline=true,ReadOnly=true,ScrollBars=ScrollBars.Vertical,BorderStyle=BorderStyle.None,BackColor=Field,ForeColor=Fg,Dock=DockStyle.Fill,Font=new Font("Segoe UI",10F),Margin=new Padding(3)};
        _players=new ListBox {Name="OnlinePlayers",BackColor=Field,ForeColor=Fg,BorderStyle=BorderStyle.None,Dock=DockStyle.Fill,HorizontalScrollbar=true,IntegralHeight=false,Font=new Font("Segoe UI",9F),Margin=new Padding(3)};
        _chatPanel.Controls.Add(_chatHistory,0,2);_chatPanel.SetColumnSpan(_chatHistory,2);_chatPanel.Controls.Add(_players,2,2);_chatPanel.SetColumnSpan(_players,2);
        _chatInput=new TextBox {Name="ChatInput",MaxLength=256,BackColor=Field,ForeColor=Fg,Dock=DockStyle.Fill,BorderStyle=BorderStyle.FixedSingle,Font=new Font("Segoe UI",10F),Margin=new Padding(3,6,3,3)};
        _sendChat=LanButton("SEND","SendChat",true);_chatPanel.Controls.Add(_chatInput,0,3);_chatPanel.SetColumnSpan(_chatInput,3);_chatPanel.Controls.Add(_sendChat,3,3);
        _sendChat.Click+=delegate{SendChat();};_chatInput.KeyDown+=delegate(object sender,KeyEventArgs e){if(e.KeyCode==Keys.Enter){e.SuppressKeyPress=true;SendChat();}};
        _teamChoice=LanCombo("RoomTeam","Team 1");_chooseTeam=LanButton("JOIN TEAM","ChooseRoomTeam",false);
        _chatPanel.Controls.Add(_teamChoice,0,4);_chatPanel.Controls.Add(_chooseTeam,1,4);_chatPanel.SetColumnSpan(_chooseTeam,3);
        _chooseTeam.Click+=delegate{try{if(_roomClient!=null)_roomClient.ChooseTeam(_teamChoice.SelectedIndex+1);}catch(Exception e){LanError(e.Message);}};
        _lanRoot.Controls.Add(_chatPanel,1,0);_lanRoot.SetRowSpan(_chatPanel,2);

        var console=LanTable(2,77,23);console.Margin=new Padding(0,8,0,0);console.RowCount=2;console.RowStyles.Add(new RowStyle(SizeType.Absolute,30));console.RowStyles.Add(new RowStyle(SizeType.Percent,100));
        _status=new ArtworkLabel {Text="READY",BackColor=Bg,ForeColor=Yellow,Dock=DockStyle.Fill,Font=new Font("Impact",16F)};
        _stopServer=new ArtworkButton {Name="StopServer",Text="STOP SERVER",Dock=DockStyle.Fill,Visible=false,Font=new Font("Impact",12F),BackgroundImage=CreateButtonArtwork()};_stopServer.Click+=StopServerClicked;
        _log=new LauncherLogView {Name="Log",BackColor=Field,ForeColor=Muted,Dock=DockStyle.Fill,Font=new Font("Segoe UI",9F)};
        _log.LineTransform=delegate(string line){return line.Length<10?LauncherLocalization.Log(_uiLanguage,line):line.Substring(0,10)+LauncherLocalization.Log(_uiLanguage,line.Substring(10));};
        console.Controls.Add(_status,0,0);console.Controls.Add(_stopServer,1,0);console.Controls.Add(_log,0,1);console.SetColumnSpan(_log,2);_lanRoot.Controls.Add(console,0,2);_lanRoot.SetColumnSpan(console,2);
        _lanNotice=new Label {Name="LanNotice",ForeColor=Muted,BackColor=Bg,Text="LAN",AutoSize=false,Font=new Font("Segoe UI",8F),TextAlign=ContentAlignment.MiddleCenter};Controls.Add(_lanNotice);
        AddLanLink("SupportLink","Support on Ko-fi","https://ko-fi.com/tressy");
        AddLanLink("H4tiuxCredit","Special THX to H4TIUX","https://github.com/H4TIUX/");
        AddLanLink("H4tiuxRepository","Link to GitHub repository","https://github.com/H4TIUX/PUBG2019PS");
        _map.SelectedIndexChanged+=delegate{if(_map.SelectedIndex==5){_teamMode.SelectedIndex=0;_bots.Value=0;}UpdateLanState();};
    }
    private void AddHostRow(int row,string text,Control control){_hostFields.Controls.Add(LanLabel(text),0,row);_hostFields.Controls.Add(control,1,row);_hostFields.SetColumnSpan(control,3);}
    private void AddLanLink(string name,string text,string url)
    {
        var c=new LinkLabel {Name=name,Text=text,ForeColor=Fg,LinkColor=Muted,ActiveLinkColor=Yellow,VisitedLinkColor=Muted,BackColor=Bg,Font=new Font("Segoe UI",8F),TextAlign=name=="SupportLink"?ContentAlignment.MiddleLeft:ContentAlignment.MiddleRight};
        c.LinkClicked+=delegate{try{System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(url){UseShellExecute=true});}catch(Exception e){LanError(e.Message);}};Controls.Add(c);
    }
    private void ArrangeLanUi()
    {
        if(_lanRoot==null)return;int h=ClientSize.Height,w=ClientSize.Width;
        _headerPanel.Height=h<760?108:150;
        _lanRoot.SetBounds(8,_headerPanel.Height,w-16,Math.Max(460,h-_headerPanel.Height-46));
        _lanRoot.RowStyles[0].Height=h<760?280:318;
        _lanRoot.RowStyles[2].Height=h<760?88:112;
        for(int i=0;i<7;i++)_hostFields.RowStyles[i].Height=h<760?30:33;
        Controls.Find("SupportLink",false)[0].SetBounds(12,h-34,150,22);
        Controls.Find("H4tiuxCredit",false)[0].SetBounds(w-250,h-39,238,18);
        Controls.Find("H4tiuxRepository",false)[0].SetBounds(w-250,h-21,238,18);
        _lanNotice.SetBounds(170,h-35,Math.Max(50,w-440),28);
        Invalidate(new Rectangle(0,0,w,_headerPanel.Height));
        Control profile=Controls.Find("OpenProfile",false)[0],hide=Controls.Find("HideLauncher",false)[0],mini=Controls.Find("MinimizeLauncher",false)[0];
        if(Controls.Find("MaximizeLauncher",false).Length==0){var max=LanButton("□","MaximizeLauncher",false);max.Dock=DockStyle.None;max.Click+=delegate{WindowState=WindowState==FormWindowState.Maximized?FormWindowState.Normal:FormWindowState.Maximized;};Controls.Add(max);}
        Control maximize=Controls.Find("MaximizeLauncher",false)[0];maximize.SetBounds(w-98,8,40,30);maximize.BringToFront();
        profile.SetBounds(w-262,8,112,30);profile.BackColor=Yellow;profile.ForeColor=Color.Black;
        Control character=Controls.Find("OpenCharacter",false)[0];character.SetBounds(w-390,8,122,30);character.BringToFront();
        mini.SetBounds(w-144,8,40,30);hide.SetBounds(w-52,8,40,30);mini.BackColor=Bg;hide.BackColor=Bg;
        foreach(var c in new[]{profile,hide,mini,Controls.Find("LauncherVersion",false)[0]})c.BringToFront();
    }
    private void ApplyLanLanguage(){foreach(var p in _lanLabels)if(!p.Key.IsDisposed)p.Key.Text=L(p.Value);ShowChat(_showRoomChat);UpdateLanState();}
    private int SelectedTeamSize { get{return _teamMode.SelectedIndex==1?2:_teamMode.SelectedIndex==2?4:1;} }
    private void UpdateLanState()
    {
        if(_start==null || _teamMode==null)return;
        bool inRoom=_roomClient!=null,host=_lobbyServer!=null,active=_finished || _roomStarting || _roomReady;
        _name.Enabled=!inRoom&&!_finished;_serverName.Enabled=_map.Enabled=_bots.Enabled=_roomPasswordBox.Enabled=!inRoom&&!_finished;
        _teamMode.Enabled=!inRoom&&!_finished&&_map.SelectedIndex!=5;_cameraMode.Enabled=!inRoom&&!_finished;
        _joinRoom.Enabled=!inRoom&&!_finished;_leaveRoom.Enabled=inRoom&&(!host||(!_finished&&!_roomStarting&&!_roomReady));
        _roomTab.Enabled=inRoom;_teamChoice.Enabled=_chooseTeam.Enabled=_roomConnected&&!active&&!_roomMatch&&_connectedRoom!=null&&_connectedRoom.TeamSize>1;
        _start.Enabled=(!inRoom&&!_finished) || (_roomConnected&&_appearanceSynced&&!active&&host&&!_finished) || (_roomReady&&!_roomJoinedGame&&!_joinStarted);
        if(!inRoom)_start.Text=L("HOST ROOM");else if(!_roomConnected)_start.Text=L("CONNECTING");else if(host&&!active)_start.Text=L("START MATCH");else _start.Text=L(_roomStarting?"STARTING SERVER":"JOIN GAME");
        _autoJoinLocal.Enabled=!_roomJoinedGame;
        if(_connectedRoom==null)_roomInfo.Text=L("No room joined");
        else _roomInfo.Text=_connectedRoom.Name+"  |  "+_connectedRoom.Map+"  |  "+(_connectedRoom.TeamSize==1?"Solo":_connectedRoom.TeamSize==2?"Duo":"Squad")+" "+_connectedRoom.Camera+"  |  "+L(_roomReady?"SERVER RUNNING":_roomStarting?"STARTING SERVER":"ROOM OPEN");
    }
    private void StartLanServices()
    {
        if(_globalNetwork!=null)return;
        _globalNetwork=new LanGlobal();
        _globalNetwork.Roster+=names=>LanUi(delegate{if(!_netMode){_onlineNames=names;RefreshPlayers();}});
        _globalNetwork.Message+=(name,message)=>LanUi(delegate{if(!_netMode)AddChat(false,name+": "+message);});
        _globalNetwork.Error+=message=>LanUi(delegate{LanError(message);});
        try{_globalNetwork.Start();_globalNetwork.SetName(_name.Text.Trim());_globalNetwork.Announce();}
        catch(Exception e){LanError(e.Message);}
        _lanTimer.Interval=1500;_lanTimer.Tick+=delegate {
            if(_globalNetwork!=null){_globalNetwork.SetName(_netMode||(_dedicatedServer&&_lobbyServer!=null)?"":_name.Text.Trim());_globalNetwork.Announce();}
            if(_lobbyServer!=null&&_connectedRoom!=null)try{
                var advertised=HostRoomDetails(true);var encoded=LanDiscovery.Encode(advertised);_lobbyServer.Announcement=encoded;
                if(_visibleLan&&(!_netMode||_dedicatedServer))LanBroadcast.Send(ChatGroupGuard.Wrap(encoded,"DISCOVERY"),LanDiscovery.Port);
                if(!_roomMatch && _roomReady && File.Exists(Path.Combine(_dir,"LAN_MatchStarted.flag"))){_roomMatch=true;_lobbyServer.NotifyMatchStarted();}
                ProcessServerMarkers();
            }catch(Exception e){LanError(e.Message);}
            if(_roomClient!=null&&_roomConnected)try{_roomClient.Ping();}catch(Exception e){LanError(e.Message);}
            if(_netMode){NetDirectoryTick();NetChatTick();}else if(DateTime.UtcNow>=_nextScan)ScanRooms();
            if(_quickMatchHost)TryStartQuickMap();
        };_lanTimer.Start();ScanRooms();
    }
    private void ProcessServerMarkers(){
        if(_lobbyServer==null)return;
        string lateClosed=Path.Combine(_dir,"LAN_LateJoinClosed.flag");if(File.Exists(lateClosed)){if(_allowLateJoin){_allowLateJoin=false;_lobbyServer.NotifyLateJoinClosed();_lobbyServer.Announcement=LanDiscovery.Encode(HostRoomDetails(true));_lobbyServer.PublishAnnouncement();}try{File.Delete(lateClosed);}catch(IOException){}}
        string ended=Path.Combine(_dir,"LAN_MatchEnded.flag");if(File.Exists(ended)){_lobbyServer.NotifyMatchEnded();try{File.Delete(ended);}catch(IOException){}}
    }
    private void ScanRooms()
    {
        if(_scanBusy||_lanDisposed)return;_scanBusy=true;_nextScan=DateTime.UtcNow.AddSeconds(5);
        ThreadPool.QueueUserWorkItem(delegate{try{var rooms=LanDiscovery.Scan(1800);LanUi(delegate{
            string selected=(_roomList.SelectedItem as LanServer)==null?null:((LanServer)_roomList.SelectedItem).Address;
            _roomList.BeginUpdate();_roomList.Items.Clear();foreach(var room in rooms){_roomList.Items.Add(room);if(room.Address==selected)_roomList.SelectedItem=room;}_roomList.EndUpdate();
            if(_roomList.SelectedIndex<0&&_roomList.Items.Count>0)_roomList.SelectedIndex=0;
        });}catch(Exception e){LanUi(delegate{LanError(e.Message);});}finally{LanUi(delegate{_scanBusy=false;});}});
    }
    private bool ValidateLanName()
    {
        if(LobbyWire.ValidName(ActivePlayerName))return true;LanError(T("Enter 1–24 English letters, numbers, or underscores for the player name."));_name.Focus();return false;
    }
    private void LanPrimaryAction()
    {
        if(!ValidateLanName())return;
        try {
            if(_roomClient==null){
                if(_finished||HasLocalServer())throw new InvalidOperationException("A local server is already running from this game folder.");
                var map=(MapOption)_map.SelectedItem;
                // Validate room metadata before opening sockets or writing teams.
                LanDiscovery.Encode(HostRoomDetails(true));
                if(!_netMode)SaveName(ActivePlayerName);SaveLauncherValues(_name.Text.Trim());
                File.Delete(Path.Combine(_dir,"LAN_MatchStarted.flag"));File.Delete(Path.Combine(_dir,"LAN_MatchEnded.flag"));File.Delete(Path.Combine(_dir,"LAN_LateJoinClosed.flag"));
                _lobbyServer=new LanLobbyServer(Path.Combine(_dir,"LAN_Teams.ini"),SelectedTeamSize,_roomPasswordBox.Text,_dedicatedServer?_dedicatedObserverName:ActivePlayerName,_roomCapacity,(int)_bots.Value,_allowLateJoin,_autoJoinLocal.Checked&&!_dedicatedServer,_dedicatedServer);
                _lobbyServer.Announcement=LanDiscovery.Encode(HostRoomDetails(true));
                if(_quickMatchHost){_lobbyServer.SetListenPort(_lobbyPort);_lobbyServer.EnableMapVoting();_lobbyServer.MatchVoteApproved+=ApproveQuickMap;}
                _lobbyServer.Start();
                var room=HostRoomDetails();
                ConnectRoom(room,_roomPasswordBox.Text);return;
            }
            if(!_roomConnected||!_appearanceSynced)return;
            if(_lobbyServer!=null&&!_roomStarting&&!_roomReady){
                File.Delete(Path.Combine(_dir,"LAN_MatchStarted.flag"));File.Delete(Path.Combine(_dir,"LAN_MatchEnded.flag"));File.Delete(Path.Combine(_dir,"LAN_LateJoinClosed.flag"));_lobbyServer.NotifyStarting();_roomStarting=true;_hostLocal.Checked=true;LaunchRoomGame();
            }else if(_roomReady&&!_roomJoinedGame)JoinRoomGame();
        }catch(Exception e){if(_roomClient==null&&_lobbyServer!=null){CloseNetLease();_lobbyServer.Dispose();_lobbyServer=null;}LanError(LauncherPermissionText.FromException(e,"LAN_Teams.ini"));UpdateLanState();}
    }
    private void JoinSelectedRoom(){var room=_roomList.SelectedItem as LanServer;if(room==null){LanError(L("Select a LAN room first"));return;}JoinRoom(room);}
    private void JoinByAddress()
    {
        IPAddress ip;string host=_address.Text.Trim();if(host.EndsWith(":8888"))host=host.Substring(0,host.Length-5);
        if(!IPAddress.TryParse(host,out ip)||!LobbyWire.Private(ip)){LanError(L("Enter a local IPv4 address"));return;}
        // Metadata is received from the room handshake; never used as a process command.
        JoinRoom(new LanServer{Name=host,Map="Miramar",TeamSize=1,Camera="TPP",Address=ip+":8888",Protected=false});
    }
    private void JoinRoom(LanServer room)
    {
        if(_roomClient!=null||_finished||!ValidateLanName())return;
        if(room.Ready&&!room.LateJoin){LanError("Late join is closed or disabled for this match.");return;}
        string password="";
        // Always allow a password for direct-IP rooms, whose discovery metadata is unknown.
        if(room.Protected||room.Name==room.Address.Split(':')[0]){password=AskRoomPassword();if(password==null)return;}
        ConnectRoom(room,password);
    }
    private string AskRoomPassword()
    {
        using(var f=new Form {Text=L("ROOM PASSWORD"),ClientSize=new Size(360,120),StartPosition=FormStartPosition.CenterParent,FormBorderStyle=FormBorderStyle.FixedDialog,MaximizeBox=false,MinimizeBox=false,BackColor=Bg,ForeColor=Fg}){
            var input=new TextBox {UseSystemPasswordChar=true,MaxLength=64,Left=20,Top=20,Width=320,BackColor=Field,ForeColor=Fg};
            var ok=LanButton("JOIN ROOM","PasswordOk",true);ok.Dock=DockStyle.None;ok.SetBounds(210,65,130,35);ok.DialogResult=DialogResult.OK;f.Controls.Add(input);f.Controls.Add(ok);f.AcceptButton=ok;
            return f.ShowDialog(this)==DialogResult.OK?input.Text:null;
        }
    }
    private void ConnectRoom(LanServer room,string password)
    {
        _connectedRoom=room;_manualActiveJoin=room.Ready&&room.LateJoin;_initialRoomHandshake=true;_quickMatchRoom=room.QuickMatch||room.Name==QuickMatchRoomName;ResetQuickVote();_roomPlayerName=_lobbyServer!=null&&_dedicatedServer?_dedicatedObserverName:ActivePlayerName;_roomHistory.Clear();_roomNames=new string[0];_webSeats=new WebSeat[0];_webSlotsReady=false;_roomJoinedGame=false;_roomConnected=_roomReady=_roomStarting=_roomMatch=false;_chosenTeam=0;_appearanceSynced=false;
        var client=new LanLobbyClient();_roomClient=client;int generation=++_roomGeneration;
        client.Packet+=packet=>LanUi(delegate{if(generation==_roomGeneration)RoomPacket(packet);});
        client.Disconnected+=()=>LanUi(delegate{if(generation==_roomGeneration){AddChat(true,L("Room disconnected"));CloseRoomConnection();}});
        ShowChat(true);UpdateLanState();
        string name=_roomPlayerName;ThreadPool.QueueUserWorkItem(delegate{try{client.Connect(IPAddress.Parse(room.Address.Split(':')[0]),name,password,room.LobbyPort);}catch(Exception e){LanUi(delegate{if(generation==_roomGeneration){LanError(e.Message);CloseRoomConnection();}});}});
    }
    private void RoomPacket(string packet)
    {
        string[] p=packet.Split('|');
        try {
            if(p[0]=="WELCOME"&&p.Length==2){_roomConnected=true;int size;if(Int32.TryParse(p[1],out size)&&(size==1||size==2||size==4))_connectedRoom.TeamSize=size;
                _teamChoice.Items.Clear();for(int i=1;i<=100/_connectedRoom.TeamSize;i++)_teamChoice.Items.Add(L("Team")+" "+i);_teamChoice.SelectedIndex=0;
                AddChat(true,L("Connected to room"));_roomClient.SetAppearance(SkinProfile.Read(_dir),SkinProfile.ReadPreviewGender(_dir),SkinProfile.ReadFeatures(_dir));}
            else if(p[0]=="APPEARANCE"&&p.Length==2&&p[1]=="OK"){_appearanceSynced=true;if(_roomReady&&_lobbyServer==null&&!_manualActiveJoin&&(_quickMatchRoom||_autoJoinLocal.Checked))JoinRoomGame();}
            else if(p[0]=="INFO"&&p.Length==2){var details=LanDiscovery.Decode(Convert.FromBase64String(p[1]),new IPEndPoint(IPAddress.Parse(_connectedRoom.Address.Split(':')[0]),8888));if(details!=null){_connectedRoom=details;if(details.Ready&&details.LateJoin)_manualActiveJoin=true;_teamChoice.Items.Clear();for(int ti=1;ti<=100/details.TeamSize;ti++)_teamChoice.Items.Add(L("Team")+" "+ti);_teamChoice.SelectedIndex=0;_quickMatchRoom=details.QuickMatch||details.Name==QuickMatchRoomName;if(_lobbyServer==null){_serverName.Text=details.Name;_teamMode.SelectedIndex=details.TeamSize==2?1:details.TeamSize==4?2:0;_cameraMode.SelectedIndex=details.Camera=="FPP"?1:0;for(int i=0;i<_map.Items.Count;i++)if(((MapOption)_map.Items[i]).Value==details.Map)_map.SelectedIndex=i;}}}
            else if(p[0]=="BOTS"&&p.Length==2){int bots;if(_lobbyServer==null&&Int32.TryParse(p[1],out bots)&&bots>=0&&bots<=95)_bots.Value=bots;}
            else if(p[0]=="TEAM"&&p.Length==2){int team;if(Int32.TryParse(p[1],out team)&&team>0&&team<=_teamChoice.Items.Count){_chosenTeam=team;_teamChoice.SelectedIndex=team-1;}}
            else if(p[0]=="ROSTER"&&p.Length==2){_initialRoomHandshake=false;var names=new List<string>();foreach(string member in p[1].Split(',')){if(member.Length==0)continue;string[] pair=member.Split(':');int team;if(pair.Length!=2||!Int32.TryParse(pair[1],out team))continue;string name=LobbyWire.Unpack(pair[0]);if(!LobbyWire.ValidName(name))continue;names.Add(name+(_connectedRoom.TeamSize>1?"  ["+L("Team")+" "+team+"]":""));}_roomNames=names.ToArray();RefreshPlayers();}
            else if(p[0]=="SLOTS"&&p.Length==2){var slots=new List<WebSeat>();var used=new HashSet<int>();foreach(string member in p[1].Split(',')){if(member.Length==0)continue;string[] pair=member.Split(':');int slot;if(pair.Length!=2||!Int32.TryParse(pair[1],out slot)||slot<1||slot>_connectedRoom.Capacity||!used.Add(slot))continue;string name=LobbyWire.Unpack(pair[0]);if(LobbyWire.ValidName(name))slots.Add(new WebSeat{name=name,slot=slot});}_webSeats=slots.ToArray();_webSlotsReady=true;}
            else if(p[0]=="CHAT"&&p.Length==3){string name=LobbyWire.Unpack(p[1]),message=LobbyWire.Unpack(p[2]);if(LobbyWire.ValidName(name)&&LobbyWire.ValidMessage(message))AddChat(true,name+": "+message);}
            else if(p[0]=="STARTING"){_quickPendingMap="";_roomStarting=true;AddChat(true,L("STARTING SERVER"));}
            else if(p[0]=="GAME"&&p.Length==2){_roomReady=p[1]=="1";_roomStarting=false;if(_roomReady){if(_initialRoomHandshake&&_connectedRoom!=null&&_connectedRoom.LateJoin)_manualActiveJoin=true;AddChat(true,L("SERVER RUNNING"));if(_lobbyServer==null&&!_manualActiveJoin&&(_quickMatchRoom||_autoJoinLocal.Checked)&&_appearanceSynced)JoinRoomGame();}else{AddChat(true,"Server stopped.");_manualActiveJoin=false;_roomJoinedGame=false;_roomMatch=false;if(_lobbyServer==null){_finished=false;_joinStarted=false;_timer.Stop();}}}
            else if(p[0]=="MATCH"){_roomMatch=true;AddChat(true,L("Match started"));}
            else if(p[0]=="LATEJOIN"&&p.Length==2&&p[1]=="0"){_allowLateJoin=false;if(_connectedRoom!=null)_connectedRoom.LateJoin=false;AddChat(true,"Late join closed after the second circle.");NativePublish();}
            else if(p[0]=="MATCHEND"&&p.Length==2&&p[1]=="1"){AddChat(true,"Match ended.");CloseClientAfterMatch();}
            else if(p[0]=="VOTECONFIG"&&p.Length==3&&(p[1]=="Solo"||p[1]=="Duo"||p[1]=="Squad")&&(p[2]=="FPP"||p[2]=="TPP")){_voteMode=p[1];_voteCamera=p[2];NativePublish();}
            else if(p[0]=="VOTEPROPOSER"&&p.Length==2){string proposer=LobbyWire.Unpack(p[1]);if(LobbyWire.ValidName(proposer)){_voteProposer=proposer;NativePublish();}}
            else if(p[0]=="VOTESTATE"&&p.Length==6){int seconds,yes,no,eligible;if(Int32.TryParse(p[2],out seconds)&&Int32.TryParse(p[3],out yes)&&Int32.TryParse(p[4],out no)&&Int32.TryParse(p[5],out eligible)){if(_voteMap!=p[1])AddChat(true,"Match vote started: "+p[1]+" / "+_voteMode+" / "+_voteCamera);_voteMap=p[1];_voteSeconds=seconds;_voteYes=yes;_voteNo=no;_voteEligible=eligible;NativePublish();}}
            else if(p[0]=="VOTERESULT"&&p.Length==3){ResetQuickVote();if(p[2]=="1")_quickPendingMap=p[1];AddChat(true,"Match vote "+(p[2]=="1"?"accepted: ":"rejected: ")+p[1]+" / "+_voteMode+" / "+_voteCamera);NativePublish();}
            else if(p[0]=="VOTEPENDING"&&p.Length==2){_quickPendingMap=p[1];ResetQuickVote();NativePublish();}
            else if(p[0]=="NOTICE"&&p.Length==2){AddChat(true,LobbyWire.Unpack(p[1]));NativePublish();}
            else if(p[0]=="VOTEERROR"&&p.Length==2){LanError(LobbyWire.Unpack(p[1]));}
            else if(p[0]=="PONG"&&p.Length==2){long stamp;if(Int64.TryParse(p[1],out stamp)){long elapsed=System.Diagnostics.Stopwatch.GetTimestamp()-stamp;if(elapsed>=0)_lanNotice.Text=L("Host ping")+": "+(elapsed*1000/System.Diagnostics.Stopwatch.Frequency)+" ms";}}
            else if((p[0]=="ERROR"||p[0]=="TEAMERROR")&&p.Length==2){LanError(L(LobbyWire.Unpack(p[1])));}
        }catch(FormatException){LanError("Invalid room response.");CloseRoomConnection();}
        UpdateLanState();
    }
    private void JoinRoomGame()
    {
        if(!_roomReady||_roomJoinedGame||!_roomConnected||!_appearanceSynced)return;
        if(_manualActiveJoin&&(_connectedRoom==null||!_connectedRoom.LateJoin)){LanError("Late join is closed for this match.");return;}
        _hostLocal.Checked=_lobbyServer!=null;_address.Text=_connectedRoom.Address;
        if(!_netMode)_name.Text=_roomPlayerName;_joinStarted=false;_joinProcessHandled=false;_finished=false;
        LaunchRoomGame();_roomJoinedGame=_finished&&_joinStarted;UpdateLanState();
    }
    private void LaunchRoomGame()
    {
        _launchFromRoom=true;try{StartClicked(null,EventArgs.Empty);}finally{_launchFromRoom=false;}
        UpdateLanState();
    }
    private void LanLaunchFailed(){if(_lobbyServer!=null&&!_serverReady){_roomStarting=false;_lobbyServer.NotifyGameStopped();}_roomJoinedGame=false;}
    private void LanGameStopped(){try{ProcessServerMarkers();}catch(Exception e){LanError(e.Message);}_roomReady=_roomStarting=_roomMatch=_roomJoinedGame=false;if(_lobbyServer!=null)_lobbyServer.NotifyGameStopped();}
    private void LeaveRoom(){if(_lobbyServer!=null&&(_finished||_roomStarting||_roomReady)){LanError(L("Stop the server before leaving the room"));return;}CloseRoomConnection();}
    private void CloseRoomConnection()
    {
        if(_lobbyServer==null){_finished=false;_joinStarted=false;_timer.Stop();}
        CloseNetLease();++_roomGeneration;if(_roomClient!=null)_roomClient.Dispose();_roomClient=null;
        if(_lobbyServer!=null)_lobbyServer.Dispose();_lobbyServer=null;_connectedRoom=null;_quickMatchRoom=_quickMatchHost=false;_pendingQuickMap=_quickPendingMap="";ResetQuickVote();
        _roomConnected=_roomStarting=_roomReady=_roomMatch=_roomJoinedGame=_manualActiveJoin=_initialRoomHandshake=false;_roomNames=new string[0];_webSeats=new WebSeat[0];_webSlotsReady=false;_hostLocal.Checked=true;ShowChat(false);UpdateLanState();
    }
    private void ShowChat(bool room)
    {
        if(_chatHistory==null)return;_showRoomChat=room;
        _globalTab.BackColor=room?Field:Yellow;_globalTab.ForeColor=room?Fg:Color.Black;
        _roomTab.BackColor=room?Yellow:Field;_roomTab.ForeColor=room?Color.Black:Fg;
        _chatHistory.Text=String.Join(Environment.NewLine,(room?_roomHistory:_globalHistory).ToArray());_chatHistory.SelectionStart=_chatHistory.TextLength;_chatHistory.ScrollToCaret();RefreshPlayers();
    }
    private void RefreshPlayers(){if(_players==null)return;_players.BeginUpdate();_players.Items.Clear();_players.Items.AddRange(_showRoomChat?_roomNames:_onlineNames);_players.EndUpdate();_onlineInfo.Text=L(_showRoomChat?"PLAYERS":"ONLINE")+" ("+_players.Items.Count+")";}
    private void AddChat(bool room,string text){var list=room?_roomHistory:_globalHistory;list.Add(DateTime.Now.ToString("HH:mm")+"  "+text);while(list.Count>200)list.RemoveAt(0);if(room==_showRoomChat)ShowChat(room);}
    private void SendChat()
    {
        if(_dedicatedServer&&_lobbyServer!=null)return;
        if((DateTime.UtcNow-_lastChatSend).TotalMilliseconds<500)return;
        string message=_chatInput.Text.Trim();if(message.Length==0)return;
        try{if(_showRoomChat){if(!_roomConnected)throw new InvalidOperationException(L("No room joined"));_roomClient.Send(message);}else if(_netMode){SendNetChat(message);}else{if(_globalNetwork==null)throw new InvalidOperationException("LAN unavailable.");_globalNetwork.SetName(_name.Text.Trim());_globalNetwork.Send(message);}_lastChatSend=DateTime.UtcNow;_chatInput.Clear();}
        catch(Exception e){LanError(e.Message);}
    }
    private string _webNotice="";private int _webNoticeRevision;
    private void LanError(string message){_webNotice=T(message);_webNoticeRevision++;if(_lanNotice!=null)_lanNotice.Text=_webNotice;AppendLog("LAN: "+message);}
    private void LanUi(Action action){if(_lanDisposed||IsDisposed||!IsHandleCreated)return;try{BeginInvoke(action);}catch(InvalidOperationException){}}
    private void DisposeLan(){CloseNetLease();if(_lanDisposed)return;_lanDisposed=true;_headerPanel.Dispose();_lanTimer.Stop();_lanTimer.Dispose();if(_roomClient!=null)_roomClient.Dispose();if(_lobbyServer!=null)_lobbyServer.Dispose();if(_globalNetwork!=null)_globalNetwork.Dispose();}
    private string LanSavedValues(){return "RoomCapacity="+_roomCapacity+Environment.NewLine+"MinimumPlayers="+_minimumPlayers+Environment.NewLine+"VisibleLan="+_visibleLan+Environment.NewLine+"DedicatedServer="+_dedicatedServer+Environment.NewLine+"AllowLateJoin="+_allowLateJoin+Environment.NewLine+"TeamMode="+_teamMode.SelectedIndex+Environment.NewLine+"CameraMode="+_cameraMode.SelectedIndex+Environment.NewLine+"ServerName="+_serverName.Text.Trim()+Environment.NewLine+"ZombieRule="+_hostZombieRule+Environment.NewLine;}
    private void LoadLanValues()
    {
        string path=Path.Combine(_dir,"PUBG2019Launcher.ini");if(!File.Exists(path))return;
        foreach(string line in File.ReadAllLines(path)){int pos=line.IndexOf('=');if(pos<1)continue;string key=line.Substring(0,pos),value=line.Substring(pos+1);int n;
            bool flag;
            if(key=="RoomCapacity"&&Int32.TryParse(value,out n)&&n>=1&&n<=100)_roomCapacity=n;
            if(key=="MinimumPlayers"&&Int32.TryParse(value,out n)&&n>=1&&n<=100)_minimumPlayers=n;
            if(key=="VisibleLan"&&Boolean.TryParse(value,out flag))_visibleLan=flag;
            if(key=="DedicatedServer"&&Boolean.TryParse(value,out flag))_dedicatedServer=flag;
            if(key=="AllowLateJoin"&&Boolean.TryParse(value,out flag))_allowLateJoin=flag;
            if(key=="TeamMode"&&Int32.TryParse(value,out n)&&n>=0&&n<=2)_teamMode.SelectedIndex=n;
            if(key=="CameraMode"&&Int32.TryParse(value,out n)&&n>=0&&n<=1)_cameraMode.SelectedIndex=n;
            if(key=="ZombieRule"&&(value=="None"||value=="BattleRoyale"||value=="Survival"||value=="Elimination"))_hostZombieRule=value;
            if(key=="ServerName"&&value.Length<=32&&!Regex.IsMatch(value,@"[\p{C}]"))_serverName.Text=value;
        }
        _hostLocal.Checked=true;if(_dedicatedServer)_autoJoinLocal.Checked=false;
    }
    private void SaveHostModes()
    {
        string path=Path.Combine(_dir,"settings.ini");var output=new List<string>();bool bots=false,found=false;
        foreach(string line in File.ReadAllLines(path)){
            string trimmed=line.Trim();if(trimmed.StartsWith("[")){bots=trimmed.Equals("[Bots]",StringComparison.OrdinalIgnoreCase);output.Add(line);if(bots&&!found){output.Add("TeamSize="+SelectedTeamSize);output.Add("CameraMode="+_cameraMode.SelectedIndex);output.Add("LobbyMinimumPlayers="+_minimumPlayers);output.Add("LanMaxPlayers="+_roomCapacity);output.Add("LanAllowLateJoin="+(_allowLateJoin?"1":"0"));found=true;}continue;}
            if(bots&&Regex.IsMatch(trimmed,@"^(TeamSize|CameraMode|LobbyMinimumPlayers|LanMaxPlayers|LanAllowLateJoin)\s*=",RegexOptions.IgnoreCase))continue;output.Add(line);
        }
        if(!found)throw new InvalidDataException("settings.ini has no [Bots] section.");
        string temp=path+".lan.new";File.WriteAllLines(temp,output.ToArray(),new UTF8Encoding(false));File.Replace(temp,path,null);
    }
}
