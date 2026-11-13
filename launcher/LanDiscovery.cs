using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Net.NetworkInformation;
using System.Text;
using System.Text.RegularExpressions;

internal sealed class LanServer {
    public string Name, Map, Camera, Address; public string ZombieRule="None";
    public int TeamSize, PingMs=-1, Capacity=100, Bots, Players=-1, MinimumPlayers=1, LobbyPort=LobbyWire.Port; public bool Ready, Protected, LateJoin, QuickMatch; public string Code="";
    public override string ToString() { return Name + "  |  " + Map + "  |  " + (TeamSize == 1 ? "Solo" : TeamSize == 2 ? "Duo" : "Squad") + " " + Camera + (Ready ? "  |  RUNNING" : "  |  ROOM") + (Protected ? "  |  LOCKED" : "") + "  |  " + (PingMs>=0 ? PingMs+" ms" : "-- ms"); }
}
internal static class LanDiscovery {
    internal const int Port = 18888;
    internal static byte[] Encode(string name, string map, int team, string camera, int port, bool ready, bool locked) {
        if (String.IsNullOrWhiteSpace(name) || name.Length > 32 || Regex.IsMatch(name, @"[\p{C}]") || !ValidMap(map) || (team != 1 && team != 2 && team != 4) || (camera != "TPP" && camera != "FPP") || port < 1 || port > 65535) throw new ArgumentException("Invalid LAN server details.");
        return Encoding.UTF8.GetBytes("PUBG2019LAN1|" + Convert.ToBase64String(Encoding.UTF8.GetBytes(name)) + "|" + map + "|" + team + "|" + camera + "|" + port + "|" + (ready ? "1" : "0") + "|" + (locked ? "1" : "0"));
    }
    internal static byte[] Encode(LanServer room) {
        int gamePort=8888;string[] address=(room.Address??"").Split(':');if(address.Length==2&&!Int32.TryParse(address[1],out gamePort))throw new ArgumentException("Invalid game port.");if(room.LobbyPort<1||room.LobbyPort>65535)throw new ArgumentException("Invalid lobby port.");
        var basic=Encoding.UTF8.GetString(Encode(room.Name,room.Map,room.TeamSize,room.Camera,gamePort,room.Ready,room.Protected));
        if(room.Capacity<1||room.Capacity>100||room.Bots<0||room.Bots>95||room.Bots>room.Capacity||room.Players<0||room.Players>room.Capacity-room.Bots||room.MinimumPlayers<1||room.MinimumPlayers>room.Capacity||!Regex.IsMatch(room.Code??"",@"^PUBG-[A-F0-9]{8}$"))throw new ArgumentException("Invalid room capacity or code.");
        string body=basic.Replace("PUBG2019LAN1|",room.LobbyPort==LobbyWire.Port&&!room.QuickMatch?"PUBG2019LAN2|":"PUBG2019LAN3|")+"|"+room.Capacity+"|"+room.Bots+"|"+room.Players+"|"+room.MinimumPlayers+"|"+(room.LateJoin?"1":"0")+"|"+room.Code;
        if(room.LobbyPort!=LobbyWire.Port||room.QuickMatch)body+="|"+room.LobbyPort+"|"+(room.QuickMatch?"1":"0");return Encoding.UTF8.GetBytes(body);
    }
    static bool ValidMap(string map) { return map=="Miramar" || map=="Erangel" || map=="ErangelClassic" || map=="Sanhok" || map=="Vikendi" || map=="Training"; }
    internal static LanServer Decode(byte[] data, IPEndPoint source) {
        if (data == null || data.Length > 512 || source == null || source.AddressFamily != AddressFamily.InterNetwork || !LobbyWire.Private(source.Address)) return null;
        try {
            string[] p = Encoding.UTF8.GetString(data).Split('|');
            if (!((p.Length==8&&p[0]=="PUBG2019LAN1")||(p.Length==14&&p[0]=="PUBG2019LAN2")||((p.Length==15||p.Length==16)&&p[0]=="PUBG2019LAN3"))) return null;
            string name = new UTF8Encoding(false,true).GetString(Convert.FromBase64String(p[1])); int team, port;
            if (!Int32.TryParse(p[3],out team) || !Int32.TryParse(p[5],out port)) return null;
            if((p[6]!="0" && p[6]!="1") || (p[7]!="0" && p[7]!="1"))return null; Encode(name,p[2],team,p[4],port,p[6]=="1",p[7]=="1");
            var room=new LanServer { Name=name, Map=p[2], TeamSize=team, Camera=p[4], Ready=p[6]=="1", Protected=p[7]=="1", Address=source.Address.ToString()+":"+port };
            room.ZombieRule=name.StartsWith("[Z-BR] ",StringComparison.Ordinal)?"BattleRoyale":name.StartsWith("[Z-SURV] ",StringComparison.Ordinal)?"Survival":name.StartsWith("[Z-ELIM] ",StringComparison.Ordinal)?"Elimination":"None";
            if(p.Length>=14){if(p.Length>=15&&(!Int32.TryParse(p[14],out room.LobbyPort)||room.LobbyPort<1||room.LobbyPort>65535))return null;if(p.Length==16){if(p[15]!="0"&&p[15]!="1")return null;room.QuickMatch=p[15]=="1";}else room.QuickMatch=name==LauncherForm.QuickMatchRoomName;if(!Int32.TryParse(p[8],out room.Capacity)||!Int32.TryParse(p[9],out room.Bots)||!Int32.TryParse(p[10],out room.Players)||!Int32.TryParse(p[11],out room.MinimumPlayers)||(p[12]!="0"&&p[12]!="1"))return null;room.LateJoin=p[12]=="1";room.Code=p[13];Encode(room);}
            return room;
        } catch (Exception) { return null; }
    }
    internal static void Announce(string name,string map,int team,string camera,int port,bool ready,bool locked) {
        byte[] data=Encode(name,map,team,camera,port,ready,locked);
        LanBroadcast.Send(ChatGroupGuard.Wrap(data,"DISCOVERY"),Port);
    }
    internal static int Ping(IPAddress host) {return Ping(host,LobbyWire.Port);}
    internal static int Ping(IPAddress host,int port) {
        if(!LobbyWire.Private(host))return -1;
        try { using(var tcp=new TcpClient()) {
            var clock=System.Diagnostics.Stopwatch.StartNew(); var pending=tcp.BeginConnect(host,port,null,null);
            using(var wait=pending.AsyncWaitHandle) { if(!wait.WaitOne(800))return -1; }
            tcp.EndConnect(pending);tcp.ReceiveTimeout=800;tcp.SendTimeout=800;
            var stream=tcp.GetStream(); LobbyWire.Write(stream,new object(),"PING");
            if(LobbyWire.Read(stream)!="PONG")return -1;
            clock.Stop();return (int)Math.Min(Int32.MaxValue,clock.ElapsedMilliseconds);
        }} catch(Exception) {return -1;}
    }
    // A room is broadcast once per network adapter. Group those copies by
    // its unique room code and lobby port, never by name alone: independent
    // Quick Match rooms often deliberately have the same display name.
    internal static string DiscoveryKey(LanServer server) {
        return !String.IsNullOrEmpty(server.Code) ? server.Code+"|"+server.LobbyPort : server.Address;
    }
    internal static bool IsPhysicalLocalAddress(string endpoint) {
        IPAddress address;int colon=(endpoint??"").LastIndexOf(':');
        if(colon<1||!IPAddress.TryParse(endpoint.Substring(0,colon),out address))return false;
        try {foreach(var nic in NetworkInterface.GetAllNetworkInterfaces()) {
            if(nic.OperationalStatus!=OperationalStatus.Up || (nic.NetworkInterfaceType!=NetworkInterfaceType.Ethernet && nic.NetworkInterfaceType!=NetworkInterfaceType.Wireless80211))continue;
            string label=(nic.Name+" "+nic.Description).ToLowerInvariant();
            if(label.Contains("virtual")||label.Contains("hyper-v")||label.Contains("vmware")||label.Contains("vethernet")||label.Contains("docker")||label.Contains("loopback"))continue;
            foreach(var unicast in nic.GetIPProperties().UnicastAddresses)if(address.Equals(unicast.Address))return true;
        }}catch(NetworkInformationException){}catch(SocketException){}
        return false;
    }
    internal static bool PreferDiscovery(LanServer candidate,LanServer previous) {
        if(previous.PingMs<0 && candidate.PingMs>=0)return true;
        if(previous.PingMs>=0 && candidate.PingMs<0)return false;
        return IsPhysicalLocalAddress(candidate.Address)&&!IsPhysicalLocalAddress(previous.Address);
    }
    internal static List<LanServer> Scan(int milliseconds) {return ScanOnPort(milliseconds,Port);}
    internal static List<LanServer> ScanOnPort(int milliseconds,int port) {
        var results = new Dictionary<string,LanServer>(StringComparer.OrdinalIgnoreCase);
        var seenAddresses = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        using(var udp=new UdpClient()) {
            udp.Client.SetSocketOption(SocketOptionLevel.Socket,SocketOptionName.ReuseAddress,true);
            udp.Client.Bind(new IPEndPoint(IPAddress.Any,port)); udp.Client.ReceiveTimeout=300;
            DateTime end=DateTime.UtcNow.AddMilliseconds(milliseconds);
            while(DateTime.UtcNow<end) {
                try { IPEndPoint remote=null; byte[] data=udp.Receive(ref remote); byte[] payload;if(!ChatGroupGuard.TryUnwrap(data,"DISCOVERY",out payload))continue;var server=Decode(payload,remote); if(server!=null && seenAddresses.Add(server.Address+"|"+server.LobbyPort)) {server.PingMs=Ping(remote.Address,server.LobbyPort);string key=DiscoveryKey(server);LanServer prior;if(!results.TryGetValue(key,out prior)||PreferDiscovery(server,prior))results[key]=server;} }
                catch(SocketException e) { if(e.SocketErrorCode!=SocketError.TimedOut && e.SocketErrorCode!=SocketError.WouldBlock) throw; }
            }
        }
        return new List<LanServer>(results.Values);
    }
}

// Send on each active private IPv4 subnet. The default route may belong to a VPN or virtual adapter.
internal static class LanBroadcast {
    internal static void Send(byte[] data,int port) {
        int sent=0;var routes=new HashSet<string>(StringComparer.Ordinal);
        foreach(var nic in NetworkInterface.GetAllNetworkInterfaces()) {
            if(nic.OperationalStatus!=OperationalStatus.Up || nic.NetworkInterfaceType==NetworkInterfaceType.Loopback || nic.NetworkInterfaceType==NetworkInterfaceType.Tunnel)continue;
            try {foreach(var unicast in nic.GetIPProperties().UnicastAddresses) {
                IPAddress ip=unicast.Address;
                if(ip.AddressFamily!=AddressFamily.InterNetwork||!LobbyWire.Private(ip)||IPAddress.IsLoopback(ip))continue;
                IPAddress mask=unicast.IPv4Mask;
                if(mask==null)continue;
                byte[] a=ip.GetAddressBytes(),m=mask.GetAddressBytes(),b=new byte[4];bool hasHost=false,hasNetwork=false;
                for(int i=0;i<4;i++){b[i]=(byte)(a[i]|(~m[i]&255));if(m[i]!=255)hasHost=true;if(m[i]!=0)hasNetwork=true;}
                if(!hasHost||!hasNetwork)continue;var broadcast=new IPAddress(b);string key=ip.ToString()+"/"+broadcast;
                if(!routes.Add(key))continue;
                try {using(var udp=new UdpClient(new IPEndPoint(ip,0))){udp.EnableBroadcast=true;udp.Send(data,data.Length,new IPEndPoint(broadcast,port));sent++;}}catch(SocketException){}
            }}catch(NetworkInformationException){}catch(SocketException){}
        }
        if(sent==0){using(var udp=new UdpClient()){udp.EnableBroadcast=true;udp.Send(data,data.Length,new IPEndPoint(IPAddress.Broadcast,port));}}
    }
}
