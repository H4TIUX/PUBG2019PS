using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Text.RegularExpressions;
using System.Security.Cryptography;
using System.Runtime.InteropServices;
using System.Threading;

internal static class LobbyWire {
    internal const int Port=18887, MaxLine=8192;
    internal static bool Private(IPAddress ip) {
        return ip != null && ip.AddressFamily == AddressFamily.InterNetwork;
    }
    internal static string Pack(string s) { return Convert.ToBase64String(Encoding.UTF8.GetBytes(s)); }
    internal static string Unpack(string s) { return new UTF8Encoding(false,true).GetString(Convert.FromBase64String(s)); }
    internal static string Read(NetworkStream stream) {
        var bytes=new List<byte>();
        while(true) { int b=stream.ReadByte(); if(b<0)return null; if(b==10)break; if(b==13 || b>127 || bytes.Count>=MaxLine) throw new InvalidDataException("Invalid lobby packet."); bytes.Add((byte)b); }
        return Encoding.ASCII.GetString(bytes.ToArray());
    }
    internal static void Write(NetworkStream stream, object sync, string line) {
        byte[] data=Encoding.ASCII.GetBytes(line+"\n"); if(data.Length>MaxLine)throw new InvalidDataException("Lobby packet too long.");
        lock(sync) { stream.Write(data,0,data.Length); }
    }
    internal static byte[] Derive(string password,byte[] salt) { using(var k=new Rfc2898DeriveBytes(password,salt,120000))return k.GetBytes(32); }
    internal static byte[] Response(byte[] key,byte[] nonce,string name) {byte[] n=Encoding.UTF8.GetBytes(name);byte[] data=new byte[nonce.Length+n.Length];Buffer.BlockCopy(nonce,0,data,0,nonce.Length);Buffer.BlockCopy(n,0,data,nonce.Length,n.Length);using(var h=new HMACSHA256(key))return h.ComputeHash(data);}
    internal static bool Equal(byte[] a,byte[] b) {if(a==null || b==null || a.Length!=b.Length)return false;int d=0;for(int i=0;i<a.Length;i++)d|=a[i]^b[i];return d==0;}
    internal static bool ValidName(string s) {return s!=null && Regex.IsMatch(s,@"^[A-Za-z0-9_-]{1,24}$");}
    internal static bool ValidMessage(string s) {return !String.IsNullOrWhiteSpace(s) && s.Length<=256 && !Regex.IsMatch(s,@"[\p{Cc}\p{Cf}]");}
}

internal sealed partial class LanLobbyServer : IDisposable {
    sealed class Peer { internal TcpClient Tcp; internal NetworkStream Stream; internal string Name; internal int Team, Slot; internal bool ChatMuted; internal readonly object WriteLock=new object(); internal double ChatTokens=8; internal long LastChatTick=System.Diagnostics.Stopwatch.GetTimestamp(); }
    readonly object sync=new object(), rosterSync=new object(); readonly List<Peer> peers=new List<Peer>();
    readonly Dictionary<string,string> features=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
    readonly Dictionary<string,string> genders=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
    readonly Dictionary<string,string> appearances=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
    readonly Dictionary<string,int> assigned=new Dictionary<string,int>(StringComparer.OrdinalIgnoreCase);
    readonly Dictionary<string,int> seatAssigned=new Dictionary<string,int>(StringComparer.OrdinalIgnoreCase);
    TcpListener listener; int listenPort=LobbyWire.Port; volatile bool running; readonly SemaphoreSlim connectionSlots=new SemaphoreSlim(128,128);
    readonly string teamFile, hostName; int teamSize; int maxPlayers, botCount; readonly int totalCapacity; bool allowLate; readonly bool hostParticipates, muteHostChat;
    internal int PlayerCount { get {lock(sync)return peers.FindAll(p=>p.Slot>0).Count;} }
    internal byte[] Announcement; private bool starting,matchEnded,lateJoinClosed; readonly byte[] salt, passwordKey; volatile bool gameReady, matchStarted;
    internal LanLobbyServer() : this(null,4,"") {}
    internal LanLobbyServer(string path,int size) : this(path,size,"") {}
    internal LanLobbyServer(string path,int size,string password) : this(path,size,password,null) {}
    internal LanLobbyServer(string path,int size,string password,string reservedHostName) : this(path,size,password,reservedHostName,100) {}
    internal LanLobbyServer(string path,int size,string password,string reservedHostName,int capacity) : this(path,size,password,reservedHostName,capacity,0,false,true) {}
    internal LanLobbyServer(string path,int size,string password,string reservedHostName,int capacity,int bots,bool late,bool hostPlays) : this(path,size,password,reservedHostName,capacity,bots,late,hostPlays,false) {}
    internal LanLobbyServer(string path,int size,string password,string reservedHostName,int capacity,int bots,bool late,bool hostPlays,bool muteDedicatedHost) {
        muteHostChat=muteDedicatedHost;
        if(capacity<1||capacity>100||bots<0||bots>95||bots>capacity||(hostPlays&&bots==capacity))throw new ArgumentException("Invalid room capacity.");
        totalCapacity=capacity;maxPlayers=capacity-bots;botCount=bots;allowLate=late;hostParticipates=hostPlays;
        if(reservedHostName!=null && !LobbyWire.ValidName(reservedHostName))throw new ArgumentException("Invalid host name.");
        hostName=reservedHostName;teamFile=path;teamSize=size==2 || size==4 ? size : 1;
        if(!String.IsNullOrEmpty(password)){if(password.Length>64)throw new ArgumentException("Password too long.");salt=new byte[16];using(var rng=RandomNumberGenerator.Create())rng.GetBytes(salt);passwordKey=LobbyWire.Derive(password,salt);}
    }
    void SaveTeams() {
        if(teamFile==null)return;
        var text=new StringBuilder("[Teams]\r\n");foreach(var assignment in assigned) text.Append(assignment.Key).Append('=').Append(assignment.Value).Append("\r\n");
        string temp=teamFile+".tmp";File.WriteAllText(temp,text.ToString(),new UTF8Encoding(false));
        if(File.Exists(teamFile))File.Replace(temp,teamFile,null);else File.Move(temp,teamFile);
        string appearanceFile=Path.Combine(Path.GetDirectoryName(teamFile),"LAN_Appearance.ini");
        var appearanceText=new StringBuilder("[Appearance]\r\n");foreach(var p in appearances)appearanceText.Append(p.Key).Append('=').Append(p.Value).Append("\r\n");
        appearanceText.Append("[Character]\r\n");foreach(var p in genders)appearanceText.Append(p.Key).Append('=').Append(p.Value).Append("\r\n");
        appearanceText.Append("[Features]\r\n");foreach(var p in features)appearanceText.Append(p.Key).Append('=').Append(p.Value).Append("\r\n");
        string appearanceTemp=appearanceFile+".tmp";File.WriteAllText(appearanceTemp,appearanceText.ToString(),new UTF8Encoding(false));if(File.Exists(appearanceFile))File.Replace(appearanceTemp,appearanceFile,null);else File.Move(appearanceTemp,appearanceFile);
    }
    internal void SetListenPort(int port){if(running||port<1||port>65535)throw new ArgumentException("Invalid room port.");listenPort=port;}
    internal void Start() {
        lock(sync) SaveTeams();
        listener=new TcpListener(IPAddress.Any,listenPort);listener.Start(100);running=true;
        var thread=new Thread(AcceptLoop);thread.IsBackground=true;thread.Start();
    }
    void AcceptLoop() {
        while(running) { try { var tcp=listener.AcceptTcpClient();
            var ip=((IPEndPoint)tcp.Client.RemoteEndPoint).Address; if(!LobbyWire.Private(ip)){tcp.Close();continue;}
            if(!connectionSlots.Wait(0)){tcp.Close();continue;}
            try {
                tcp.SendTimeout=2000;tcp.ReceiveTimeout=15000;var peer=new Peer{Tcp=tcp,Stream=tcp.GetStream()};
                var thread=new Thread(()=>Handle(peer));thread.IsBackground=true;thread.Start();
            } catch { tcp.Close();connectionSlots.Release();throw; }
        } catch(SocketException){if(!running)return;} catch(ObjectDisposedException){return;} catch(Exception){if(!running)return;} }
    }
    internal void NotifyStarting() { lock(sync) {matchEnded=false;lateJoinClosed=false;starting=true;SaveTeams();} Broadcast("STARTING|1"); }
    internal void NotifyGameReady() { gameReady=true; Broadcast("GAME|1"); }
    internal void NotifyMatchStarted() { if(matchStarted)return;matchStarted=true;Broadcast("MATCH|1"); }
    internal void NotifyMatchEnded() {lock(sync){if(matchEnded)return;matchEnded=true;}Broadcast("MATCHEND|1");}
    internal void EnableTrainingLateJoin(){lock(sync){if(starting||gameReady||matchStarted)throw new InvalidOperationException("Cannot change late join after start.");allowLate=true;}}
    internal void NotifyLateJoinClosed(){bool changed;lock(sync){changed=allowLate;allowLate=false;lateJoinClosed=true;}if(changed)Broadcast("LATEJOIN|0");}
    internal void NotifyGameStopped() {lock(sync){gameReady=false;matchStarted=false;starting=false;lateJoinClosed=false;assigned.Clear();seatAssigned.Clear();foreach(var p in peers)if(p.Slot>0){assigned[p.Name]=p.Team;seatAssigned[p.Name]=p.Slot;}SaveTeams();}Broadcast("GAME|0");}
    void Broadcast(string line) { Peer[] copy; lock(sync) copy=peers.ToArray(); foreach(var p in copy) try { LobbyWire.Write(p.Stream,p.WriteLock,line); } catch(Exception) { p.Tcp.Close(); } }
    void BroadcastChat(string line) { Peer[] copy;lock(sync)copy=peers.ToArray();foreach(var p in copy)if(!p.ChatMuted)try{LobbyWire.Write(p.Stream,p.WriteLock,line);}catch(Exception){p.Tcp.Close();} }
    void Roster() { lock(rosterSync){string line,slots; lock(sync) { var names=new List<string>();var places=new List<string>();foreach(var p in peers){if(p.ChatMuted)continue;names.Add(LobbyWire.Pack(p.Name)+":"+p.Team);places.Add(LobbyWire.Pack(p.Name)+":"+p.Slot);}line="ROSTER|"+String.Join(",",names.ToArray());slots="SLOTS|"+String.Join(",",places.ToArray()); } Broadcast(line); Broadcast(slots);} }
    int FreeSlot(int team,string except) { for(int slot=(team-1)*teamSize+1;slot<=Math.Min(maxPlayers,team*teamSize);slot++){bool taken=false;foreach(var a in seatAssigned)if(a.Value==slot&&!String.Equals(a.Key,except,StringComparison.OrdinalIgnoreCase)){taken=true;break;}if(!taken)return slot;}return 0; }
    internal void SetQuickBotCount(int count){if(count<0||count>95||count>totalCapacity)throw new ArgumentException("Invalid bot count.");lock(sync){if(!votingEnabled||starting||gameReady||matchStarted)throw new InvalidOperationException("Cannot change bots now.");foreach(var peer in peers)if(peer.Slot>totalCapacity-count)throw new InvalidOperationException("A player occupies a bot slot.");botCount=count;maxPlayers=totalCapacity-count;}Broadcast("BOTS|"+count);}
    internal void SetQuickTeamSize(int size){if(size!=1&&size!=2&&size!=4)throw new ArgumentException("Invalid team size.");lock(sync){if(!votingEnabled||starting||gameReady||matchStarted)throw new InvalidOperationException("Cannot change teams now.");teamSize=size;foreach(var peer in peers)if(peer.Slot>0){peer.Team=(peer.Slot-1)/size+1;assigned[peer.Name]=peer.Team;}SaveTeams();}}
    internal void PublishQuickTeams(){Peer[] copy;lock(sync)copy=peers.ToArray();foreach(var p in copy)try{LobbyWire.Write(p.Stream,p.WriteLock,"TEAM|"+p.Team);}catch(Exception){p.Tcp.Close();}Roster();}
    void Handle(Peer p) {
        bool joined=false;
        try {
            string hello=LobbyWire.Read(p.Stream);
            if(hello=="PING") {LobbyWire.Write(p.Stream,p.WriteLock,"PONG");return;}
            if(hello==null)return;
            string[] greeting=hello.Split('|');
            if(greeting.Length!=3||greeting[0]!="HELLO2"||!ChatGroupGuard.ValidNonce(greeting[2])){
                LobbyWire.Write(p.Stream,p.WriteLock,"ERROR|"+LobbyWire.Pack("Incompatible launcher version."));return;
            }
            string packedName=greeting[1],clientNonce=greeting[2];
            string name=LobbyWire.Unpack(packedName);if(!LobbyWire.ValidName(name))return;
            string serverNonce=ChatGroupGuard.NewNonce();
            LobbyWire.Write(p.Stream,p.WriteLock,"GROUPCHALLENGE|"+serverNonce+"|"+ChatGroupGuard.Proof("server",packedName,clientNonce,serverNonce));
            string groupAuth=LobbyWire.Read(p.Stream);string[] groupParts=groupAuth==null?new string[0]:groupAuth.Split('|');
            if(groupParts.Length!=2||groupParts[0]!="GROUPAUTH"||!ChatGroupGuard.VerifyProof(ChatGroupGuard.Proof("client",packedName,clientNonce,serverNonce),groupParts[1])){
                LobbyWire.Write(p.Stream,p.WriteLock,"ERROR|"+LobbyWire.Pack("Incompatible launcher version."));return;
            }
            if(hostName!=null && String.Equals(name,hostName,StringComparison.OrdinalIgnoreCase) && !IPAddress.IsLoopback(((IPEndPoint)p.Tcp.Client.RemoteEndPoint).Address)) {
                LobbyWire.Write(p.Stream,p.WriteLock,"ERROR|"+LobbyWire.Pack("This name is reserved for the host."));return;
            }
            if(passwordKey!=null) {
                byte[] nonce=new byte[32];using(var rng=RandomNumberGenerator.Create())rng.GetBytes(nonce);
                LobbyWire.Write(p.Stream,p.WriteLock,"CHALLENGE|"+Convert.ToBase64String(salt)+"|"+Convert.ToBase64String(nonce));
                string auth=LobbyWire.Read(p.Stream);byte[] supplied=null;
                try{if(auth!=null && auth.StartsWith("AUTH|"))supplied=Convert.FromBase64String(auth.Substring(5));}catch(FormatException){}
                if(!LobbyWire.Equal(LobbyWire.Response(passwordKey,nonce,name),supplied)){LobbyWire.Write(p.Stream,p.WriteLock,"ERROR|"+LobbyWire.Pack("Incorrect room password."));return;}
            }
            lock(sync) {
                bool active=starting||gameReady||matchStarted;
                if(active&&matchEnded){LobbyWire.Write(p.Stream,p.WriteLock,"ERROR|"+LobbyWire.Pack("The match has ended."));return;}
                if(active&&!allowLate){LobbyWire.Write(p.Stream,p.WriteLock,"ERROR|"+LobbyWire.Pack(lateJoinClosed?"Late join is closed for this match.":"The server is running and late join is disabled."));return;}
                if(peers.Exists(x=>String.Equals(x.Name,name,StringComparison.OrdinalIgnoreCase))){LobbyWire.Write(p.Stream,p.WriteLock,"ERROR|"+LobbyWire.Pack("Name is already in use."));return;}
                p.Name=name;
                bool observer=!hostParticipates&&String.Equals(name,hostName,StringComparison.OrdinalIgnoreCase)&&IPAddress.IsLoopback(((IPEndPoint)p.Tcp.Client.RemoteEndPoint).Address);
                p.ChatMuted=observer&&muteHostChat;
                if(!observer){
                    int place=0;seatAssigned.TryGetValue(name,out place);
                    if(place==0)for(int team=1;team<=(maxPlayers+teamSize-1)/teamSize;team++){place=FreeSlot(team,null);if(place>0)break;}
                    if(place==0){LobbyWire.Write(p.Stream,p.WriteLock,"ERROR|"+LobbyWire.Pack("Lobby is full."));return;}
                    p.Team=(place-1)/teamSize+1;p.Slot=place;assigned[name]=p.Team;seatAssigned[name]=place;
                    try{SaveTeams();}catch{assigned.Remove(name);seatAssigned.Remove(name);throw;}
                }
                peers.Add(p);joined=true;
            }
            p.Tcp.ReceiveTimeout=15000;LobbyWire.Write(p.Stream,p.WriteLock,"WELCOME|"+teamSize);
            if(Announcement!=null)LobbyWire.Write(p.Stream,p.WriteLock,"INFO|"+Convert.ToBase64String(Announcement));
            SendPending(p);LobbyWire.Write(p.Stream,p.WriteLock,"BOTS|"+botCount);LobbyWire.Write(p.Stream,p.WriteLock,"TEAM|"+p.Team);
            if(gameReady)LobbyWire.Write(p.Stream,p.WriteLock,"GAME|1");else if(starting)LobbyWire.Write(p.Stream,p.WriteLock,"STARTING|1");
            if(matchStarted)LobbyWire.Write(p.Stream,p.WriteLock,"MATCH|1");Roster();
            int packetCount=0;long packetWindow=System.Diagnostics.Stopwatch.GetTimestamp();
            string line;while(running && (line=LobbyWire.Read(p.Stream))!=null) {
                long packetNow=System.Diagnostics.Stopwatch.GetTimestamp();if(packetNow-packetWindow>=System.Diagnostics.Stopwatch.Frequency){packetWindow=packetNow;packetCount=0;}if(++packetCount>120)break;
                if(line.StartsWith("VOTEPROPOSE|",StringComparison.Ordinal)||line.StartsWith("VOTECAST|",StringComparison.Ordinal)){VoteMessage(p,line);continue;}
                if(line.StartsWith("PING|")) { long stamp; if(!Int64.TryParse(line.Substring(5),out stamp) || stamp<0)break; LobbyWire.Write(p.Stream,p.WriteLock,"PONG|"+stamp); continue; }
                if(line.StartsWith("APPEARANCE|",StringComparison.Ordinal)||line.StartsWith("APPEARANCE2|",StringComparison.Ordinal)||line.StartsWith("APPEARANCE3|",StringComparison.Ordinal)) {
                    string gender=null,featureSelection="",payload=line.Substring(11);if(line.StartsWith("APPEARANCE3|",StringComparison.Ordinal)){var pieces=line.Split('|');if(pieces.Length!=4||(pieces[1]!="male"&&pieces[1]!="female"))break;gender=pieces[1];payload=pieces[2];featureSelection=LobbyWire.Unpack(pieces[3]);if(!SkinProfile.ValidFeatures(featureSelection))break;}if(line.StartsWith("APPEARANCE2|",StringComparison.Ordinal)){var pieces=line.Split('|');if(pieces.Length!=3||(pieces[1]!="male"&&pieces[1]!="female"))break;gender=pieces[1];payload=pieces[2];}
                    string selection=LobbyWire.Unpack(payload);if(!SkinProfile.Valid(selection))break;
                    bool locked;lock(sync){locked=(starting||gameReady||matchStarted)&&!(allowLate&&!appearances.ContainsKey(p.Name));if(!locked&&p.Slot>0){appearances[p.Name]=selection;if(gender!=null)genders[p.Name]=gender;features[p.Name]=featureSelection;SaveTeams();}}
                    LobbyWire.Write(p.Stream,p.WriteLock,locked?"TEAMERROR|"+LobbyWire.Pack("Appearance is locked after server start."):"APPEARANCE|OK");continue;
                }
                if(line.StartsWith("TEAM|")) {
                    int requested; if(!Int32.TryParse(line.Substring(5),out requested) || requested<1 || requested>(maxPlayers+teamSize-1)/teamSize || teamSize==1)break;
                    bool full=false, locked=false; lock(sync) {
                        locked=starting || gameReady || matchStarted || p.Slot==0;int count=0;foreach(var other in assigned)if(!String.Equals(other.Key,p.Name,StringComparison.OrdinalIgnoreCase) && other.Value==requested)++count;full=count>=teamSize;
                        if(!locked && !full) { int previous=p.Team,previousSlot=p.Slot;int slot=requested==p.Team?p.Slot:FreeSlot(requested,p.Name);if(slot==0)full=true;else{p.Team=requested;p.Slot=slot;assigned[p.Name]=requested;seatAssigned[p.Name]=slot;try{SaveTeams();}catch{p.Team=previous;p.Slot=previousSlot;assigned[p.Name]=previous;seatAssigned[p.Name]=previousSlot;throw;}} }
                    }
                    if(locked) LobbyWire.Write(p.Stream,p.WriteLock,"TEAMERROR|"+LobbyWire.Pack("Teams are locked after the match starts."));
                    else if(full) LobbyWire.Write(p.Stream,p.WriteLock,"TEAMERROR|"+LobbyWire.Pack("Team is full."));
                    else { LobbyWire.Write(p.Stream,p.WriteLock,"TEAM|"+requested);Roster(); }
                    continue;
                }
                if(line.StartsWith("SLOT|")) {
                    int slot;if(!Int32.TryParse(line.Substring(5),out slot)||slot<1||slot>maxPlayers) {LobbyWire.Write(p.Stream,p.WriteLock,"TEAMERROR|"+LobbyWire.Pack("That slot is outside the available player places."));continue;}
                    bool locked,taken=false;int target=(slot-1)/teamSize+1;
                    lock(sync){locked=starting||gameReady||matchStarted||p.Slot==0;foreach(var other in seatAssigned)if(other.Value==slot&&!String.Equals(other.Key,p.Name,StringComparison.OrdinalIgnoreCase)){taken=true;break;}
                        if(!locked&&!taken){int oldTeam=p.Team,oldSlot=p.Slot;p.Team=target;p.Slot=slot;assigned[p.Name]=target;seatAssigned[p.Name]=slot;try{SaveTeams();}catch{p.Team=oldTeam;p.Slot=oldSlot;assigned[p.Name]=oldTeam;seatAssigned[p.Name]=oldSlot;throw;}}}
                    if(locked)LobbyWire.Write(p.Stream,p.WriteLock,"TEAMERROR|"+LobbyWire.Pack("Teams are locked after the match starts."));
                    else if(taken)LobbyWire.Write(p.Stream,p.WriteLock,"TEAMERROR|"+LobbyWire.Pack("That slot is already occupied."));
                    else{LobbyWire.Write(p.Stream,p.WriteLock,"TEAM|"+target);Roster();}continue;
                }
                if(!line.StartsWith("CHAT|"))break;if(p.ChatMuted)continue;string msg=LobbyWire.Unpack(line.Substring(5));if(!LobbyWire.ValidMessage(msg))break;
                long now=System.Diagnostics.Stopwatch.GetTimestamp();
                p.ChatTokens=Math.Min(8.0,p.ChatTokens+(now-p.LastChatTick)*2.0/System.Diagnostics.Stopwatch.Frequency);p.LastChatTick=now;
                if(p.ChatTokens<1.0)continue;p.ChatTokens-=1.0;
                BroadcastChat("CHAT|"+LobbyWire.Pack(p.Name)+"|"+LobbyWire.Pack(msg));
            }
        }catch(Exception){} finally {if(joined){lock(sync){peers.Remove(p);if(!starting && !gameReady){assigned.Remove(p.Name);seatAssigned.Remove(p.Name);appearances.Remove(p.Name);genders.Remove(p.Name);features.Remove(p.Name);}if(running)try{SaveTeams();}catch(Exception){}}if(running)Roster();}p.Tcp.Close();connectionSlots.Release();}
    }
    public void Dispose(){running=false;StopVotes();if(listener!=null)listener.Stop();Peer[] copy;lock(sync){copy=peers.ToArray();peers.Clear();}foreach(var p in copy)p.Tcp.Close();if(teamFile!=null)try{File.Delete(teamFile);File.Delete(teamFile+".tmp");File.Delete(Path.Combine(Path.GetDirectoryName(teamFile),"LAN_Appearance.ini"));}catch(Exception){}}
}

internal sealed class LanLobbyClient : IDisposable {
    TcpClient tcp; NetworkStream stream; readonly object sync=new object(); volatile bool connected, disposed; string loginName, password;
    internal event Action<string> Packet; internal event Action Disconnected;
    internal void Connect(IPAddress address,string name) { Connect(address,name,""); }
    internal void Connect(IPAddress address,string name,string roomPassword) {Connect(address,name,roomPassword,LobbyWire.Port);}
    internal void Connect(IPAddress address,string name,string roomPassword,int port) {
        if(port<1||port>65535)throw new ArgumentException("Invalid room port.");
        if(!LobbyWire.Private(address) || !LobbyWire.ValidName(name))throw new ArgumentException("Invalid LAN address or player name.");
        if(roomPassword!=null && roomPassword.Length>64)throw new ArgumentException("Password too long.");loginName=name;password=roomPassword ?? "";
        if(disposed)throw new ObjectDisposedException("LanLobbyClient");
        tcp=new TcpClient();tcp.SendTimeout=2000;tcp.ReceiveTimeout=15000;
        var pending=tcp.BeginConnect(address,port,null,null);
        using(var wait=pending.AsyncWaitHandle){if(!wait.WaitOne(4000)){tcp.Close();throw new TimeoutException("Room connection timed out.");}}
        tcp.EndConnect(pending);if(disposed){tcp.Close();throw new ObjectDisposedException("LanLobbyClient");}stream=tcp.GetStream();
        try{
            string packedName=LobbyWire.Pack(name),clientNonce=ChatGroupGuard.NewNonce();
            LobbyWire.Write(stream,sync,"HELLO2|"+packedName+"|"+clientNonce);
            string challenge=LobbyWire.Read(stream);string[] parts=challenge==null?new string[0]:challenge.Split('|');
            if(parts.Length!=3||parts[0]!="GROUPCHALLENGE"||!ChatGroupGuard.ValidNonce(parts[1])||!ChatGroupGuard.VerifyProof(ChatGroupGuard.Proof("server",packedName,clientNonce,parts[1]),parts[2]))throw new InvalidDataException("Incompatible launcher version or communication key.");
            LobbyWire.Write(stream,sync,"GROUPAUTH|"+ChatGroupGuard.Proof("client",packedName,clientNonce,parts[1]));
            if(disposed)throw new ObjectDisposedException("LanLobbyClient");connected=true;
            var thread=new Thread(ReadLoop);thread.IsBackground=true;thread.Start();
        }catch{password=null;connected=false;tcp.Close();throw;}
    }
    void ReadLoop(){try{string line;long window=System.Diagnostics.Stopwatch.GetTimestamp();int packets=0;while(connected && (line=LobbyWire.Read(stream))!=null){
        long now=System.Diagnostics.Stopwatch.GetTimestamp();if(now-window>=System.Diagnostics.Stopwatch.Frequency){window=now;packets=0;}if(++packets>120)break;
        if(line.StartsWith("CHALLENGE|")) {var parts=line.Split('|');if(parts.Length!=3)break;var salt=Convert.FromBase64String(parts[1]);var nonce=Convert.FromBase64String(parts[2]);if(salt.Length!=16 || nonce.Length!=32)break;byte[] key=LobbyWire.Derive(password,salt);password=null;var response=LobbyWire.Response(key,nonce,loginName);Array.Clear(key,0,key.Length);LobbyWire.Write(stream,sync,"AUTH|"+Convert.ToBase64String(response));continue;}
        var cb=Packet;if(cb!=null)cb(line);
    }}catch(Exception){}finally{password=null;connected=false;if(tcp!=null)tcp.Close();var cb=Disconnected;if(cb!=null)cb();}}
    internal void Ping(){if(connected)LobbyWire.Write(stream,sync,"PING|"+System.Diagnostics.Stopwatch.GetTimestamp());}
    internal void SetAppearance(string selection,string gender,string featureSelection){if(!connected)throw new InvalidOperationException("Not connected.");if(!SkinProfile.Valid(selection)||!SkinProfile.ValidFeatures(featureSelection)||(gender!="male"&&gender!="female"))throw new ArgumentException("Invalid appearance.");LobbyWire.Write(stream,sync,"APPEARANCE3|"+gender+"|"+LobbyWire.Pack(selection)+"|"+LobbyWire.Pack(featureSelection));}
    internal void SetAppearance(string selection,string gender){if(!connected)throw new InvalidOperationException("Not connected.");if(!SkinProfile.Valid(selection)||(gender!="male"&&gender!="female"))throw new ArgumentException("Invalid appearance.");LobbyWire.Write(stream,sync,"APPEARANCE2|"+gender+"|"+LobbyWire.Pack(selection));}
    internal void SetAppearance(string selection){if(!connected)throw new InvalidOperationException("Not connected.");if(!SkinProfile.Valid(selection))throw new ArgumentException("Invalid appearance.");LobbyWire.Write(stream,sync,"APPEARANCE|"+LobbyWire.Pack(selection));}
    internal void ProposeMap(string map){if(!connected)throw new InvalidOperationException("Not connected.");LobbyWire.Write(stream,sync,"VOTEPROPOSE|"+map);}
    internal void ProposeMatch(string map,string mode,string camera){if(!connected)throw new InvalidOperationException("Not connected.");LobbyWire.Write(stream,sync,"VOTEPROPOSE|"+map+"|"+mode+"|"+camera);}
    internal void CastMapVote(bool accept){if(!connected)throw new InvalidOperationException("Not connected.");LobbyWire.Write(stream,sync,"VOTECAST|"+(accept?"1":"0"));}
    internal void ChooseSlot(int slot){if(!connected)throw new InvalidOperationException("Not connected.");if(slot<1 || slot>100)throw new ArgumentOutOfRangeException("slot");LobbyWire.Write(stream,sync,"SLOT|"+slot);}
    internal void ChooseTeam(int team){if(!connected)throw new InvalidOperationException("Not connected.");if(team<1 || team>100)throw new ArgumentOutOfRangeException("team");LobbyWire.Write(stream,sync,"TEAM|"+team);}
    internal void Send(string text){if(!connected)throw new InvalidOperationException("Not connected.");if(!LobbyWire.ValidMessage(text))throw new ArgumentException("Enter a message of at most 256 characters.");LobbyWire.Write(stream,sync,"CHAT|"+LobbyWire.Pack(text));}
    public void Dispose(){disposed=true;connected=false;if(tcp!=null)tcp.Close();}
}

