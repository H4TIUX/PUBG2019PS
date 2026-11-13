using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Windows.Forms;

internal sealed class NativeSeat {internal string Name;internal int Slot;}
internal sealed class NativeSnapshot {
    internal string Name,Version,Language,Notice,Address,Code;internal int NoticeRevision;
    internal LanServer[] Rooms;internal NativeSeat[] Seats;internal string[] Online,RoomNames,GlobalChat,RoomChat,Logs,DedicatedLogs;
    internal LanServer Room;internal bool InRoom,Connected,Host,Starting,Ready,Match,Joined,ManualActiveJoin,JoinInProgress,AppearanceSynced,SlotsReady,Wardrobe,AutoJoin,VisibleLan,HasAppearance,NetMode,Dedicated,QuickRoom,DedicatedConsoleAvailable,MasterRunning,DedicatedRunning,NetChatConnected,ChatServerRunning; internal string VoteMap,VoteProposer,VoteMode,VoteCamera,QuickPendingMap,MasterAdvertise,DedicatedName,DedicatedSavedNotice; internal int VoteSeconds,VoteYes,VoteNo,VoteEligible,DedicatedCapacity,DedicatedBots,DedicatedInstances;
    internal LanServer HostSettings;
}
internal sealed partial class LauncherForm {
    private NativeLauncherView _nativeView;
    private readonly Timer _nativeTimer=new Timer();
    private readonly List<string> _webLog=new List<string>();
    private DateTime _portraitTime;
    private void StartNativeUi(){
        _nativeView=new NativeLauncherView();_nativeView.Dock=DockStyle.Fill;
        _nativeView.Command+=NativeCommand;Controls.Add(_nativeView);_nativeView.BringToFront();
        _nativeTimer.Interval=500;_nativeTimer.Tick+=delegate{NativePublish();};_nativeTimer.Start();NativePublish();
    }
    private void NativePublish(){
        if(_nativeView==null||_nativeView.IsDisposed)return;
        RefreshDedicatedLogs();
        var rooms=new List<LanServer>();if(_netMode)rooms.AddRange(_netRooms);else foreach(var item in _roomList.Items){var room=item as LanServer;if(room!=null)rooms.Add(room);}
        var seats=new List<NativeSeat>();foreach(var seat in _webSeats)seats.Add(new NativeSeat{Name=seat.name,Slot=seat.slot});
        if(_connectedRoom!=null&&_lobbyServer!=null)_connectedRoom.Players=_lobbyServer.PlayerCount;
        string ip="";try{foreach(var nic in NetworkInterface.GetAllNetworkInterfaces())if(nic.OperationalStatus==OperationalStatus.Up&&nic.NetworkInterfaceType!=NetworkInterfaceType.Loopback&&nic.NetworkInterfaceType!=NetworkInterfaceType.Tunnel)foreach(var unicast in nic.GetIPProperties().UnicastAddresses){var a=unicast.Address;if(a.AddressFamily==AddressFamily.InterNetwork&&LobbyWire.Private(a)&&!IPAddress.IsLoopback(a)){if(!ip.StartsWith("192.168.")||a.ToString().StartsWith("192.168."))ip=a.ToString();}}}catch(NetworkInformationException){}
        string portrait=Path.Combine(_dir,"PUBG2019Character.png");
        try{if(File.Exists(portrait)){var stamp=File.GetLastWriteTimeUtc(portrait);if(stamp!=_portraitTime){using(var file=File.OpenRead(portrait))using(var picture=Image.FromStream(file))_nativeView.SetPortrait(new Bitmap(picture));_portraitTime=stamp;}}else if(_portraitTime!=DateTime.MinValue){_nativeView.SetPortrait(null);_portraitTime=DateTime.MinValue;}}catch(Exception ex){AppendLog("Character image: "+ex.Message);}
        var dedicatedOptions=GetDedicatedOptions();
        bool hasAppearance=false;try{hasAppearance=SkinProfile.Read(_dir).Length>0;}catch(IOException){}catch(UnauthorizedAccessException){}
        _nativeView.UpdateState(new NativeSnapshot{Name=_netMode&&_onlineIdentity!=null?_onlineIdentity.Name:_name.Text,NetMode=_netMode,Dedicated=_dedicatedServer,Version=DisplayVersion,Language=_uiLanguage,Notice=_webNotice,NoticeRevision=_webNoticeRevision,Address=ip,Code=_roomCode,
            Rooms=rooms.ToArray(),Seats=seats.ToArray(),Online=_onlineNames,RoomNames=_roomNames,GlobalChat=_globalHistory.ToArray(),RoomChat=_roomHistory.ToArray(),Logs=_webLog.ToArray(),DedicatedLogs=_dedicatedLogs,DedicatedConsoleAvailable=_dedicatedConsoleAvailable,Room=_connectedRoom,
            MasterRunning=_masterRunning,DedicatedRunning=_dedicatedRunning,NetChatConnected=_netChatConnected,ChatServerRunning=OwnedServiceRunning("chat"),MasterAdvertise=_masterAdvertise,DedicatedName=dedicatedOptions.Name,DedicatedSavedNotice=_dedicatedSavedNotice,DedicatedCapacity=dedicatedOptions.Capacity,DedicatedBots=dedicatedOptions.Bots,DedicatedInstances=dedicatedOptions.Instances,QuickRoom=_quickMatchRoom,QuickPendingMap=_quickPendingMap,VoteMap=_voteMap,VoteProposer=_voteProposer,VoteMode=_voteMode,VoteCamera=_voteCamera,VoteSeconds=_voteSeconds,VoteYes=_voteYes,VoteNo=_voteNo,VoteEligible=_voteEligible,InRoom=_roomClient!=null&&_roomConnected,Connected=_roomConnected,Host=_lobbyServer!=null,Starting=_roomStarting,Ready=_roomReady,Match=_roomMatch,Joined=_roomJoinedGame,ManualActiveJoin=_manualActiveJoin,JoinInProgress=_joinStarted&&!_joinProcessHandled,AppearanceSynced=_appearanceSynced,SlotsReady=_webSlotsReady,
            HasAppearance=hasAppearance,Wardrobe=File.Exists(Path.Combine(_dir,"CharacterWardrobe","EngineWardrobe.ps1")) && File.Exists(Path.Combine(_dir,"TslGame.exe")),AutoJoin=_autoJoinLocal.Checked,VisibleLan=_visibleLan,HostSettings=HostRoomDetails()});
    }
    private static string NativeValue(Dictionary<string,string> v,string key){string s;return v!=null&&v.TryGetValue(key,out s)?s:"";}
    private static int NativeNumber(Dictionary<string,string> v,string key,int min,int max){int n;if(!Int32.TryParse(NativeValue(v,key),out n)||n<min||n>max)throw new ArgumentException("Invalid "+key+" ("+min+"–"+max+").");return n;}
    private void NativeCommand(string action,Dictionary<string,string> v){
        try{switch(action){
            case "netLogin":BeginNetLogin();break;
            case "lanMode":SwitchToLan();break;
            case "host":
                if(_roomClient!=null||_finished)throw new InvalidOperationException("Leave the current room first.");
                if(!ValidateLanName())break;
                int capacity=NativeNumber(v,"capacity",1,100),minimum=NativeNumber(v,"minimum",1,capacity);
                string zombie=NativeValue(v,"zombie");if(String.IsNullOrEmpty(zombie))zombie="None";
                if(zombie!="None"&&zombie!="BattleRoyale"&&zombie!="Survival"&&zombie!="Elimination")throw new ArgumentException("Invalid match rules.");
                int bots=NativeNumber(v,"bots",0,95);
                bool dedicated=NativeValue(v,"dedicated")=="1",auto=NativeValue(v,"autoJoin")=="1"&&!dedicated;
                if(bots>capacity-(auto?1:0))throw new ArgumentException(zombie=="None"?"Bots and the host must fit inside Max. players.":"Zombies and the host must fit inside Max. players.");
                if(zombie!="None"&&minimum>capacity-bots)throw new ArgumentException("The player countdown must fit in the slots left available after zombies.");
                string map=NativeValue(v,"map"),mode=NativeValue(v,"mode"),camera=NativeValue(v,"camera"),password=NativeValue(v,"password");int index=-1;
                for(int i=0;i<_map.Items.Count;i++)if(_map.Items[i].Value==map)index=i;
                if(index<0||(mode!="Solo"&&mode!="Duo"&&mode!="Squad")||(camera!="TPP"&&camera!="FPP")||password.Length>64)throw new ArgumentException("Invalid host settings.");
                if(map=="Training"&&(bots!=0||mode!="Solo"||zombie!="None"))throw new ArgumentException("Training supports Solo without bots or zombie rules.");
                if(zombie!="None"&&bots<1)throw new ArgumentException("Zombie modes need room for at least one player and one zombie.");
                _hostZombieRule=zombie;
                _serverName.Text=NativeValue(v,"name").Trim();_map.SelectedIndex=index;_teamMode.SelectedIndex=mode=="Solo"?0:mode=="Duo"?1:2;_cameraMode.SelectedIndex=camera=="FPP"?1:0;
                _bots.Value=bots;_roomPasswordBox.Text=password;_dedicatedServer=dedicated;_quickMatchHost=dedicated&&NativeValue(v,"quick")=="1";if(_quickMatchHost){if(String.IsNullOrWhiteSpace(_serverName.Text))_serverName.Text=QuickMatchRoomName;string gp=NativeValue(v,"gamePort"),lp=NativeValue(v,"lobbyPort");int parsed;if(!String.IsNullOrEmpty(gp)){if(!Int32.TryParse(gp,out parsed)||parsed<8000||parsed>10000)throw new ArgumentException("Invalid Quick Match game port.");_gamePort=parsed;}if(!String.IsNullOrEmpty(lp)){if(!Int32.TryParse(lp,out parsed)||parsed<8000||parsed>10000)throw new ArgumentException("Invalid Quick Match lobby port.");_lobbyPort=parsed;}}else{_gamePort=8888;_lobbyPort=LobbyWire.Port;}_autoJoinLocal.Checked=auto;_roomCapacity=capacity;_minimumPlayers=minimum;_visibleLan=NativeValue(v,"visible")=="1";_allowLateJoin=map=="Training"||NativeValue(v,"late")=="1";
                LanPrimaryAction();break;
            case "join":
                if(_roomClient!=null||_finished||!ValidateLanName())break;
                string address=NativeValue(v,"address").Trim();LanServer selected=null;
                if(_netMode){foreach(var r in _netRooms)if(r.Address==address||r.Code.Equals(address,StringComparison.OrdinalIgnoreCase)){selected=r;break;}}else foreach(var item in _roomList.Items){var r=item as LanServer;if(r!=null&&(r.Address==address||r.Code.Equals(address,StringComparison.OrdinalIgnoreCase))){selected=r;break;}}
                if(selected==null){IPAddress ip;string host=address.EndsWith(":8888")?address.Substring(0,address.Length-5):address;if(!IPAddress.TryParse(host,out ip)||!LobbyWire.Private(ip))throw new ArgumentException("No LAN room matches that code or address.");selected=new LanServer{Name=host,Address=ip+":8888",Map="Miramar",TeamSize=1,Camera="TPP"};}
                JoinRoom(selected);break;
            case "startChat":DedicatedEvent("CHAT: Starting...");StartOwnedChat();DedicatedEvent("CHAT: Process started on TCP 18901.");break;
            case "stopChat":DedicatedEvent("CHAT: Stopping...");StopOwnedChat();DedicatedEvent("CHAT: Stopped.");break;
            case "startMaster":DedicatedEvent("MASTER: Starting...");StartOwnedMaster(NativeValue(v,"ip"));DedicatedEvent("MASTER: Process started on TCP 18900.");break;
            case "stopMaster":DedicatedEvent("MASTER: Stopping...");StopOwnedMaster();DedicatedEvent("MASTER: Stopped.");break;
            case "saveDedicatedSettings":SaveDedicatedOptions(v);break;
            case "startDedicated":DedicatedEvent("DEDICATED: Starting service...");SaveDedicatedOptions(v);StartOwnedDedicated();DedicatedEvent("DEDICATED: Supervisor started; waiting for a room.");break;
            case "stopDedicated":DedicatedEvent("DEDICATED: Stopping service...");StopOwnedDedicated();DedicatedEvent("DEDICATED: Stop requested.");break;
            case "quickJoin":BeginQuickJoin();break;
            case "votePropose":if(_quickMatchRoom&&_roomClient!=null){string vm=NativeValue(v,"mode"),vc=NativeValue(v,"camera");_roomClient.ProposeMatch(NativeValue(v,"map"),String.IsNullOrEmpty(vm)?"Solo":vm,String.IsNullOrEmpty(vc)?"FPP":vc);}break;
            case "voteCast":if(_quickMatchRoom&&_roomClient!=null)_roomClient.CastMapVote(NativeValue(v,"accept")=="1");break;
            case "refresh":if(_netMode){_nextNetPoll=DateTime.MinValue;NetDirectoryTick();}else ScanRooms();break;
            case "start":if(_lobbyServer!=null)LanPrimaryAction();break;
            case "joinGame":JoinRoomGame();break;
            case "stop":StopServerClicked(null,EventArgs.Empty);break;
            case "leave":LeaveRoom();break;
            case "slot":if(_roomClient!=null&&_roomConnected&&!_roomStarting&&!_roomReady&&!_roomMatch)_roomClient.ChooseSlot(NativeNumber(v,"slot",1,_connectedRoom.Capacity));break;
            case "chat":ShowChat(NativeValue(v,"channel")=="room");_chatInput.Text=NativeValue(v,"text");SendChat();break;
            case "profile":ShowProfile();break;
            case "wardrobe":StartWardrobe();break;
            case "minimize":WindowState=FormWindowState.Minimized;break;
            case "maximize":WindowState=WindowState==FormWindowState.Maximized?FormWindowState.Normal:FormWindowState.Maximized;break;
            case "hide":HideToTray();break;
            case "windowClose":HandleWindowClose();break;
            case "exit":_exitRequested=true;Close();break;
            case "drag":ReleaseCapture();SendMessage(Handle,0xA1,new IntPtr(2),IntPtr.Zero);break;
            case "kofi":OpenNativeLink("https://ko-fi.com/tressy");break;
            case "credit":OpenNativeLink("https://github.com/H4TIUX/");break;
            case "repository":OpenNativeLink("https://github.com/H4TIUX/PUBG2019PS");break;
        }}catch(Exception ex){if(action=="startMaster"||action=="stopMaster")DedicatedEvent("MASTER: "+ex.Message);else if(action=="startDedicated"||action=="stopDedicated")DedicatedEvent("DEDICATED: "+ex.Message);LanError(ex.Message);}NativePublish();
    }
    private void OpenNativeLink(string url){System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(url){UseShellExecute=true});}
}
