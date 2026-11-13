using System;using System.Collections.Generic;using System.Threading;
// The room host is authoritative: only connected player slots may vote.
internal sealed partial class LanLobbyServer {
    readonly Dictionary<string,bool> mapVotes=new Dictionary<string,bool>(StringComparer.OrdinalIgnoreCase);
    System.Threading.Timer voteTimer; string voteMap, voteProposer, voteMode="Solo", voteCamera="FPP", pendingMode="Solo", pendingCamera="FPP"; DateTime voteDeadline; bool votingEnabled; string pendingMap; int voteTickBusy;
    internal event Action<string> MapVoteApproved;
    internal event Action<string,string,string> MatchVoteApproved;
    internal void EnableMapVoting(){lock(sync){votingEnabled=true;}}
    static bool VoteMapAllowed(string map){return Array.IndexOf(new[]{"Erangel","ErangelClassic","Miramar","Sanhok","Vikendi","Training"},map)>=0;}
    void VoteMessage(Peer peer,string line){
        string error=null;lock(sync){
            if(!votingEnabled||pendingMap!=null||peer.Slot<=0||starting||gameReady||matchStarted){error="Map voting is unavailable.";}
            else if(line.StartsWith("VOTEPROPOSE|",StringComparison.Ordinal)){
                string[] parts=line.Split('|');string map=parts.Length>1?parts[1]:"";string mode=parts.Length==4?parts[2]:"Solo",camera=parts.Length==4?parts[3]:"FPP";
                if((parts.Length!=2&&parts.Length!=4)||!VoteMapAllowed(map)||(mode!="Solo"&&mode!="Duo"&&mode!="Squad")||(map=="Training"&&mode!="Solo")||(camera!="FPP"&&camera!="TPP"))error="Invalid match settings.";
                else if(voteMap!=null)error="A map vote is already in progress.";
                else {voteMap=map;voteProposer=peer.Name;voteMode=mode;voteCamera=camera;voteDeadline=DateTime.UtcNow.AddSeconds(30);mapVotes.Clear();mapVotes[peer.Name]=true;if(voteTimer==null)voteTimer=new System.Threading.Timer(VoteTick,null,0,1000);}
            }else if(line=="VOTECAST|1"||line=="VOTECAST|0"){
                if(voteMap==null||DateTime.UtcNow>=voteDeadline)error="No active map vote.";
                else mapVotes[peer.Name]=line.EndsWith("1",StringComparison.Ordinal);
            }else error="Invalid vote.";
        }
        if(error!=null){try{LobbyWire.Write(peer.Stream,peer.WriteLock,"VOTEERROR|"+LobbyWire.Pack(error));}catch(Exception){}return;}VoteTick(null);
    }
    void VoteTick(object unused){
        if(Interlocked.Exchange(ref voteTickBusy,1)!=0)return;
        try{string update=null,result=null,accepted=null,config=null,proposer=null,approvedMode=null,approvedCamera=null;lock(sync){
            if(!running||!votingEnabled||voteMap==null)return;
            int eligible=0,responded=0,yes=0,no=0;
            foreach(var peer in peers){if(peer.Slot<=0)continue;eligible++;if(mapVotes.ContainsKey(peer.Name))responded++;}
            foreach(bool value in mapVotes.Values){if(value)yes++;else no++;}
            int seconds=Math.Max(0,(int)Math.Ceiling((voteDeadline-DateTime.UtcNow).TotalSeconds));
            config="VOTECONFIG|"+voteMode+"|"+voteCamera;
            proposer="VOTEPROPOSER|"+LobbyWire.Pack(voteProposer);
            update="VOTESTATE|"+voteMap+"|"+seconds+"|"+yes+"|"+no+"|"+eligible;
            // Abstentions do not count: decide from the votes actually cast.
            int votes=yes+no;bool passed=eligible>0&&votes>0&&yes*100>=votes*51;
            if(seconds==0||(eligible>0&&responded==eligible)){
                result="VOTERESULT|"+voteMap+"|"+(passed?"1":"0");if(passed){accepted=voteMap;pendingMap=voteMap;approvedMode=pendingMode=voteMode;approvedCamera=pendingCamera=voteCamera;}
                voteMap=null;voteProposer=null;mapVotes.Clear();var old=voteTimer;voteTimer=null;if(old!=null)old.Dispose();
            }
        }
        if(config!=null)Broadcast(config);if(proposer!=null)Broadcast(proposer);if(update!=null)Broadcast(update);if(result!=null)Broadcast(result);if(accepted!=null){var callback=MapVoteApproved;if(callback!=null)callback(accepted);var match=MatchVoteApproved;if(match!=null)match(accepted,approvedMode,approvedCamera);}
        }finally{Interlocked.Exchange(ref voteTickBusy,0);}
    }
    internal void SetMapPending(string map){lock(sync){pendingMap=map;}Broadcast("VOTEPENDING|"+map);}
    internal void ClearMapPending(){lock(sync){pendingMap=null;}}
    void SendPending(Peer peer){string map,mode,camera;lock(sync){map=pendingMap;mode=pendingMode;camera=pendingCamera;}if(map!=null){LobbyWire.Write(peer.Stream,peer.WriteLock,"VOTECONFIG|"+mode+"|"+camera);LobbyWire.Write(peer.Stream,peer.WriteLock,"VOTEPENDING|"+map);}}
    internal void SystemNotice(string message){Broadcast("NOTICE|"+LobbyWire.Pack(message));}
    internal void PublishAnnouncement(){var announcement=Announcement;if(announcement!=null)Broadcast("INFO|"+Convert.ToBase64String(announcement));}
    void StopVotes(){lock(sync){votingEnabled=false;voteMap=null;voteProposer=null;pendingMap=null;mapVotes.Clear();var old=voteTimer;voteTimer=null;if(old!=null)old.Dispose();}}
}
