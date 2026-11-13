using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Windows.Forms;
using System.Drawing;

// LAN-only presence and public chat. No central service or stored chat history.
internal sealed class LanGlobal : IDisposable {
    internal const int Port=18886;
    string id=Guid.NewGuid().ToString("N");
    readonly object gate=new object();
    readonly Dictionary<string,Entry> peers=new Dictionary<string,Entry>();
    readonly HashSet<string> seen=new HashSet<string>();
    readonly Queue<string> seenOrder=new Queue<string>();
    readonly Dictionary<string,Rate> sources=new Dictionary<string,Rate>();
    sealed class Rate {internal DateTime Window;internal int Count;}
    UdpClient receiver;volatile bool running;string myName="";readonly IPAddress bindAddress;readonly int listenPort;
    internal LanGlobal() : this(IPAddress.Any,Port) {}
    internal LanGlobal(IPAddress address) : this(address,Port) {}
    internal LanGlobal(IPAddress address,int port){if(port<1||port>65535)throw new ArgumentOutOfRangeException("port");bindAddress=address ?? IPAddress.Any;listenPort=port;}
    sealed class Entry {internal string Name;internal DateTime Seen;internal IPAddress Address;}
    internal event Action<string[]> Roster;
    internal event Action<string,string> Message;
    internal event Action<string,string,string> ChannelMessage;
    internal event Action<string,string> DirectMessage;
    internal event Action<string> Error;
    internal void Start() {
        receiver=new UdpClient();receiver.ExclusiveAddressUse=false;
        receiver.Client.SetSocketOption(SocketOptionLevel.Socket,SocketOptionName.ReuseAddress,true);
        receiver.Client.Bind(new IPEndPoint(bindAddress,listenPort));receiver.EnableBroadcast=true;running=true;
        var t=new Thread(Listen);t.IsBackground=true;t.Start();
    }
    internal void SetName(string name) {lock(gate)myName=LobbyWire.ValidName(name) ? name : "";}
    internal void SetIdentity(Guid identity) {if(identity==Guid.Empty)return;lock(gate){if(id!=identity.ToString("N"))peers.Remove(id);id=identity.ToString("N");}}
    void SendPacket(string text,IPAddress target=null) {byte[] bytes=ChatGroupGuard.Wrap(Encoding.UTF8.GetBytes(text),"GLOBAL");if(target==null){LanBroadcast.Send(bytes,listenPort);return;}using(var socket=new UdpClient()){socket.Send(bytes,bytes.Length,new IPEndPoint(target,listenPort));}}
    internal static bool ValidChannel(string name){return name!=null && Regex.IsMatch(name,@"^#[A-Za-z0-9_]{1,24}$");}
    internal void Announce() {
        string name,identity;lock(gate){name=myName;identity=id;}
        if(name.Length>0){Update(identity,name,null);try{SendPacket("P19GLOBAL1|HELLO|"+identity+"|"+LobbyWire.Pack(name));}catch(SocketException e){var cb=Error;if(cb!=null)cb(e.Message);}}
        Prune();
    }
    internal void Send(string text) {SendChannel("#global",text);}
    internal void SendChannel(string channel,string text) {
        string name,identity;lock(gate){name=myName;identity=id;}
        if(name.Length==0)throw new InvalidOperationException("Enter a player name first.");
        if(!LobbyWire.ValidMessage(text) || !ValidChannel(channel))throw new ArgumentException("Invalid message or channel.");
        string mid=Guid.NewGuid().ToString("N");
        SendPacket("P19GLOBAL1|CHAT|"+identity+"|"+LobbyWire.Pack(name)+"|"+mid+"|"+LobbyWire.Pack(text)+"|"+channel);
        Deliver(mid,name,text,channel);
    }
    internal void SendDirect(string target,string text) {
        string name,identity,key=null;IPAddress address=null;int matches=0;
        lock(gate){name=myName;identity=id;foreach(var pair in peers)if(pair.Key!=id && String.Equals(pair.Value.Name,target,StringComparison.OrdinalIgnoreCase) && pair.Value.Address!=null){key=pair.Key;address=pair.Value.Address;matches++;}}
        if(name.Length==0)throw new InvalidOperationException("Enter a player name first.");
        if(!LobbyWire.ValidMessage(text))throw new ArgumentException("Invalid message.");
        if(matches!=1)throw new InvalidOperationException(matches==0 ? "Player is not online." : "Multiple players have that name.");
        string mid=Guid.NewGuid().ToString("N");SendPacket("P19GLOBAL1|DM|"+identity+"|"+LobbyWire.Pack(name)+"|"+mid+"|"+LobbyWire.Pack(text)+"|"+key,address);
        var cb=DirectMessage;if(cb!=null)cb("@"+target,name+": "+text);
    }
    void Update(string key,string name,IPAddress address) {
        bool changed=false;
        lock(gate) {Entry entry;if(!peers.TryGetValue(key,out entry)){if(peers.Count>=128)return;entry=new Entry();peers[key]=entry;changed=true;}if(entry.Name!=name){entry.Name=name;changed=true;}if(address!=null)entry.Address=address;entry.Seen=DateTime.UtcNow;}
        if(changed)Publish();
    }
    internal string[] Names(){lock(gate){var list=new List<string>();foreach(var p in peers.Values)list.Add(p.Name);list.Sort(StringComparer.OrdinalIgnoreCase);return list.ToArray();}}
    void Publish(){string[] list;lock(gate){var names=new List<string>();foreach(var p in peers.Values)names.Add(p.Name);names.Sort(StringComparer.OrdinalIgnoreCase);list=names.ToArray();}var cb=Roster;if(cb!=null)cb(list);}
    void Prune(){bool changed=false;lock(gate){var expired=new List<string>();foreach(var pair in peers)if((DateTime.UtcNow-pair.Value.Seen).TotalSeconds>6)expired.Add(pair.Key);foreach(string k in expired){peers.Remove(k);changed=true;}}if(changed)Publish();}
    bool MarkNew(string mid){lock(gate){if(!seen.Add(mid))return false;seenOrder.Enqueue(mid);while(seenOrder.Count>512)seen.Remove(seenOrder.Dequeue());return true;}}
    void Deliver(string mid,string name,string text,string channel){if(!MarkNew(mid))return;if(channel.Equals("#global",StringComparison.OrdinalIgnoreCase)){var cb=Message;if(cb!=null)cb(name,text);}var handler=ChannelMessage;if(handler!=null)handler(channel,name,text);}
    bool Allowed(IPAddress ip) {
        string key=ip.ToString();Rate r;DateTime now=DateTime.UtcNow;
        if(!sources.TryGetValue(key,out r)) {if(sources.Count>=128)sources.Clear();r=new Rate{Window=now};sources[key]=r;}
        if((now-r.Window).TotalSeconds>=1){r.Window=now;r.Count=0;}return ++r.Count<=30;
    }
    void Listen(){while(running)try{
        IPEndPoint source=null;byte[] data=receiver.Receive(ref source);if(data.Length>2300 || source==null || !LobbyWire.Private(source.Address) || !Allowed(source.Address))continue;
        byte[] payload;if(!ChatGroupGuard.TryUnwrap(data,"GLOBAL",out payload))continue;
        string[] p=new UTF8Encoding(false,true).GetString(payload).Split('|');Guid parsed;
        if(p.Length<4 || p[0]!="P19GLOBAL1" || !Guid.TryParseExact(p[2],"N",out parsed))continue;
        string name=LobbyWire.Unpack(p[3]);if(!LobbyWire.ValidName(name))continue;
        if(p[1]=="HELLO" && p.Length==4)Update(p[2],name,source.Address);
        else if(p[1]=="BYE" && p.Length==4){bool removed=false;lock(gate){Entry entry;if(peers.TryGetValue(p[2],out entry)&&entry.Name==name&&entry.Address!=null&&entry.Address.Equals(source.Address)){peers.Remove(p[2]);removed=true;}}if(removed)Publish();}
        else if(p[1]=="CHAT" && (p.Length==6 || p.Length==7) && Guid.TryParseExact(p[4],"N",out parsed)) {string text=LobbyWire.Unpack(p[5]);string channel=p.Length==7?p[6]:"#global";if(!LobbyWire.ValidMessage(text)||!ValidChannel(channel))continue;Update(p[2],name,source.Address);Deliver(p[4],name,text,channel);}
        else if(p[1]=="DM" && p.Length==7 && Guid.TryParseExact(p[4],"N",out parsed)) {string own;lock(gate)own=id;if(p[6]!=own)continue;string text=LobbyWire.Unpack(p[5]);if(!LobbyWire.ValidMessage(text)||!MarkNew(p[4]))continue;Update(p[2],name,source.Address);var cb=DirectMessage;if(cb!=null)cb("@"+name,name+": "+text);}
    }catch(SocketException){if(!running)return;}catch(ObjectDisposedException){return;}catch(Exception){}}
    public void Dispose(){string name,identity;lock(gate){name=myName;identity=id;}if(name.Length>0)try{SendPacket("P19GLOBAL1|BYE|"+identity+"|"+LobbyWire.Pack(name));}catch(Exception){}running=false;if(receiver!=null)receiver.Close();}
}

