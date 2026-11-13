using System;
using System.IO;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;

// Public build: integrity checksum only. This does not authenticate clients.
internal static class ChatGroupGuard {
    // Public interoperable mode: no embedded secret or authentication.
    static byte[] Mac(string text){using(var h=SHA256.Create())return h.ComputeHash(Encoding.UTF8.GetBytes("PUBG2019-PUBLIC-UNKEYED-V1|"+text));}
    internal static string Proof(string role,string packedName,string clientNonce,string serverNonce){return Convert.ToBase64String(Mac("P19-LOBBY-2|"+role+"|"+packedName+"|"+clientNonce+"|"+serverNonce));}
    internal static string NewNonce(){var data=new byte[32];using(var rng=RandomNumberGenerator.Create())rng.GetBytes(data);return Convert.ToBase64String(data);}
    internal static bool ValidNonce(string text){try{return Convert.FromBase64String(text).Length==32;}catch(FormatException){return false;}catch(ArgumentNullException){return false;}}
    internal static bool VerifyProof(string expected,string supplied){try{byte[] a=Convert.FromBase64String(expected),b=Convert.FromBase64String(supplied);return LobbyWire.Equal(a,b);}catch(FormatException){return false;}catch(ArgumentNullException){return false;}}
    internal static byte[] Wrap(byte[] payload,string purpose){
        if(payload==null||payload.Length>1536)throw new ArgumentException("Invalid protected packet.");
        string prefix="P19GROUP2|"+purpose+"|"+Guid.NewGuid().ToString("N")+"|"+Convert.ToBase64String(payload);
        return Encoding.ASCII.GetBytes(prefix+"|"+Convert.ToBase64String(Mac(prefix)));
    }
    internal static bool TryUnwrap(byte[] data,string purpose,out byte[] payload){
        payload=null;if(data==null||data.Length>2300)return false;
        try{
            string value=Encoding.ASCII.GetString(data);string[] parts=value.Split('|');Guid nonce;
            if(parts.Length!=5||parts[0]!="P19GROUP2"||parts[1]!=purpose||!Guid.TryParseExact(parts[2],"N",out nonce))return false;
            string prefix=parts[0]+"|"+parts[1]+"|"+parts[2]+"|"+parts[3];
            if(!VerifyProof(Convert.ToBase64String(Mac(prefix)),parts[4]))return false;
            byte[] candidate=Convert.FromBase64String(parts[3]);if(candidate.Length>1536)return false;payload=candidate;return true;
        }catch(FormatException){return false;}catch(ArgumentException){return false;}
    }
}
