using System;
internal sealed partial class LauncherForm {
    internal const string QuickMatchRoomName="QUICK MATCH - A17";
    bool _quickMatchHost,_quickMatchRoom;int _gamePort=8888,_lobbyPort=LobbyWire.Port;string _voteMap="",_voteProposer="",_pendingQuickMap="",_quickPendingMap="",_voteMode="Solo",_voteCamera="FPP",_pendingQuickMode="Solo",_pendingQuickCamera="FPP";int _voteSeconds,_voteYes,_voteNo,_voteEligible;
    void BeginQuickJoin(){
        if(!_netMode||_onlineIdentity==null){LanError("Sign in to NET mode to use Quick Join.");return;}
        if(_roomClient!=null||_finished){LanError("Leave the current room first.");return;}
        foreach(var room in _netRooms)if((room.QuickMatch||room.Name==QuickMatchRoomName)&&!room.Ready&&!room.Protected&&room.Players>=0&&room.Players<room.Capacity-room.Bots){JoinRoom(room);return;}
        _nextNetPoll=DateTime.MinValue;NetDirectoryTick();LanError("No Quick Match waiting room is currently available.");
    }
    void ApproveQuickMap(string selected,string mode,string camera){LanUi(delegate{
        if(!_quickMatchHost||_lobbyServer==null||_roomStarting||_roomReady)return;
        _pendingQuickMap=selected;_pendingQuickMode=mode;_pendingQuickCamera=camera;
        if(selected=="Training"){_allowLateJoin=true;_lobbyServer.EnableTrainingLateJoin();_bots.Value=0;_lobbyServer.SetQuickBotCount(0);_lobbyServer.SetQuickTeamSize(1);if(_connectedRoom!=null){_connectedRoom.Bots=0;_connectedRoom.TeamSize=1;_connectedRoom.LateJoin=true;}_lobbyServer.Announcement=LanDiscovery.Encode(HostRoomDetails(true));_lobbyServer.PublishAnnouncement();_lobbyServer.PublishQuickTeams();}
        _lobbyServer.SetMapPending(selected);TryStartQuickMap();
    });}
    void TryStartQuickMap(){
        if(!_quickMatchHost||_lobbyServer==null||_roomStarting||_roomReady||String.IsNullOrEmpty(_pendingQuickMap))return;
        string selected=_pendingQuickMap;for(int i=0;i<_map.Items.Count;i++)if(((MapOption)_map.Items[i]).Value==selected){
            _map.SelectedIndex=i;_teamMode.SelectedIndex=_pendingQuickMode=="Duo"?1:_pendingQuickMode=="Squad"?2:0;_cameraMode.SelectedIndex=_pendingQuickCamera=="FPP"?1:0;_lobbyServer.SetQuickTeamSize(SelectedTeamSize);if(_connectedRoom!=null){_connectedRoom.Map=selected;_connectedRoom.TeamSize=SelectedTeamSize;_connectedRoom.Camera=_pendingQuickCamera;}
            _lobbyServer.Announcement=LanDiscovery.Encode(HostRoomDetails(true));_lobbyServer.PublishAnnouncement();_lobbyServer.PublishQuickTeams();
            // Consume the accepted vote before trying once: a failed start needs a new vote.
            _pendingQuickMap="";_lobbyServer.ClearMapPending();
            AddChat(true,"Match vote accepted: "+selected+" / "+_pendingQuickMode+" / "+_pendingQuickCamera);LanPrimaryAction();return;
        }
    }
    void ResetQuickVote(){_voteMap="";_voteProposer="";_voteSeconds=_voteYes=_voteNo=_voteEligible=0;}
}
