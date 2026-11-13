using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Net;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;

// The login server's authenticated account is separate from the LAN profile.
internal sealed class OgbgIdentity {
    internal readonly string Name, AccountId, SessionId;internal string CookieHeader, BearerToken;
    internal OgbgIdentity(string name,string account,string session){Name=name;AccountId=account;SessionId=session;}
    static string Field(IDictionary<string,object> values,string key){if(values==null)return null;foreach(var pair in values)if(String.Equals(pair.Key,key,StringComparison.OrdinalIgnoreCase)&&pair.Value!=null)return Convert.ToString(pair.Value);return null;}
    internal static OgbgIdentity Parse(string json){
        var root=new JavaScriptSerializer().DeserializeObject(json) as IDictionary<string,object>;object value;
        var user=root!=null&&root.TryGetValue("user",out value)?value as IDictionary<string,object>:null;
        var session=root!=null&&root.TryGetValue("session",out value)?value as IDictionary<string,object>:null;
        string name=Field(user,"username"),account=Field(session,"AccountGuid"),id=Field(session,"SessionId");
        if(!LobbyWire.ValidName(name))throw new InvalidDataException("The account name cannot be used as a player name (1–24 letters, numbers, underscores or hyphens).");
        if(String.IsNullOrWhiteSpace(account)||String.IsNullOrWhiteSpace(id))throw new InvalidDataException("The account server did not return a valid session.");
        string bearer=Field(root,"token");if(bearer!=null&&(bearer.Length>4096||bearer.IndexOfAny(new[]{'\r','\n'})>=0))throw new InvalidDataException("Invalid account token.");
        return new OgbgIdentity(name,account,id){BearerToken=bearer};
    }
}
internal sealed class OgbgAuthClient {
    readonly Uri endpoint;readonly CookieContainer cookies=new CookieContainer();
    internal OgbgAuthClient(Uri server){if(server==null||(server.Scheme!=Uri.UriSchemeHttp&&server.Scheme!=Uri.UriSchemeHttps)||!String.IsNullOrEmpty(server.UserInfo))throw new ArgumentException("Invalid account server.");endpoint=server;}
    internal OgbgIdentity Login(string username,string password){
        if(String.IsNullOrWhiteSpace(username)||String.IsNullOrEmpty(password))throw new ArgumentException("Enter your username and password.");
        var request=(HttpWebRequest)WebRequest.Create(new Uri(endpoint,"/api/auth/login"));request.Method="POST";request.ContentType="application/json";request.Accept="application/json";request.CookieContainer=cookies;request.AllowAutoRedirect=false;request.Timeout=15000;request.ReadWriteTimeout=15000;
        byte[] body=Encoding.UTF8.GetBytes(new JavaScriptSerializer().Serialize(new {email=username,password=password}));
        try{request.ContentLength=body.Length;using(var stream=request.GetRequestStream())stream.Write(body,0,body.Length);}finally{Array.Clear(body,0,body.Length);}
        try{using(var response=(HttpWebResponse)request.GetResponse())using(var stream=response.GetResponseStream())using(var reader=new StreamReader(stream,Encoding.UTF8)){
            if(response.StatusCode!=HttpStatusCode.OK)throw new InvalidDataException("Login failed.");var text=new StringBuilder();var buffer=new char[4096];int n;while((n=reader.Read(buffer,0,buffer.Length))>0){if(text.Length+n>65536)throw new InvalidDataException("The account server response is too large.");text.Append(buffer,0,n);}var identity=OgbgIdentity.Parse(text.ToString());identity.CookieHeader=cookies.GetCookieHeader(new Uri(endpoint,"/api/auth/me"));return identity;
        }}catch(WebException e){var response=e.Response as HttpWebResponse;string status=response==null?e.Status.ToString():"HTTP "+(int)response.StatusCode;string reason=null;if(response!=null&&response.StatusCode==HttpStatusCode.Forbidden){try{using(var reader=new StreamReader(response.GetResponseStream(),Encoding.UTF8)){char[] b=new char[2049];int n=reader.ReadBlock(b,0,b.Length);if(n>0&&n<=2048){var details=new JavaScriptSerializer().DeserializeObject(new string(b,0,n)) as IDictionary<string,object>;object value;if(details!=null&&details.TryGetValue("error",out value)&&value is string){string candidate=((string)value).Trim();if(candidate.Length>0&&candidate.Length<=200&&!System.Text.RegularExpressions.Regex.IsMatch(candidate,@"[\p{C}]")&&(String.IsNullOrEmpty(password)||candidate.IndexOf(password,StringComparison.OrdinalIgnoreCase)<0))reason=candidate;}}}}catch(Exception){}}if(e.Response!=null)e.Response.Dispose();throw new InvalidOperationException("Login failed ("+status+")."+(reason==null?"":" "+reason));}
    }
}
internal sealed class OgbgCredentialVault {
    readonly string path;
    internal OgbgCredentialVault(string dir){path=Path.Combine(dir,"Saved","PUBG2019NetCredentials.dpapi");}
    internal void Save(string username,string password){byte[] plain=Encoding.UTF8.GetBytes(new JavaScriptSerializer().Serialize(new {username=username,password=password})),encrypted=null;try{encrypted=ProtectedData.Protect(plain,null,DataProtectionScope.CurrentUser);}finally{Array.Clear(plain,0,plain.Length);}Directory.CreateDirectory(Path.GetDirectoryName(path));string temp=path+"."+Guid.NewGuid().ToString("N")+".tmp",backup=temp+".bak";try{File.WriteAllBytes(temp,encrypted);if(File.Exists(path))File.Replace(temp,path,backup);else File.Move(temp,path);}finally{Array.Clear(encrypted,0,encrypted.Length);if(File.Exists(temp))File.Delete(temp);if(File.Exists(backup))File.Delete(backup);}}
    internal bool TryLoad(out string username,out string password){username=password=null;if(!File.Exists(path))return false;byte[] plain=null;try{plain=ProtectedData.Unprotect(File.ReadAllBytes(path),null,DataProtectionScope.CurrentUser);var values=new JavaScriptSerializer().DeserializeObject(Encoding.UTF8.GetString(plain)) as IDictionary<string,object>;object u,p;if(values==null||!values.TryGetValue("username",out u)||!values.TryGetValue("password",out p))return false;username=u as string;password=p as string;return !String.IsNullOrEmpty(username)&&!String.IsNullOrEmpty(password);}catch(CryptographicException){return false;}catch(InvalidOperationException){return false;}catch(ArgumentException){return false;}catch(IOException){return false;}catch(UnauthorizedAccessException){return false;}finally{if(plain!=null)Array.Clear(plain,0,plain.Length);}}
    internal void Remove(){if(File.Exists(path))File.Delete(path);}
}
internal sealed partial class LauncherForm {
    private bool _netMode,_netBusy;private int _netGeneration;private OgbgIdentity _onlineIdentity;private Uri _netEndpoint=new Uri("http://217.199.220.41:3001/");
    private async void BeginNetLogin(){
        if(_roomClient!=null||_finished||_netBusy||_netMode)return;
        if(_onlineIdentity!=null){_netMode=true;NativePublish();return;}
        var vault=new OgbgCredentialVault(_dir);string username="",password="";bool remember=vault.TryLoad(out username,out password);if(!remember){username="";password="";}
        using(var form=new Form{Text="OG:BG — "+T("NET LOGIN"),ClientSize=new Size(480,340),StartPosition=FormStartPosition.CenterParent,FormBorderStyle=FormBorderStyle.None,BackColor=NativeArt.Panel,ForeColor=NativeArt.Text,ShowInTaskbar=false}){
            form.Paint+=delegate(object sender,PaintEventArgs e){using(var pen=new Pen(NativeArt.Border))e.Graphics.DrawRectangle(pen,0,0,form.ClientSize.Width-1,form.ClientSize.Height-1);};
            var title=new Label{Text="OG:BG  •  "+T("NET LOGIN"),Font=NativeArt.Font(20,true,true),ForeColor=NativeArt.Text,Bounds=new Rectangle(25,9,420,40),TextAlign=ContentAlignment.MiddleLeft};form.Controls.Add(title);form.Controls.Add(new Panel{BackColor=NativeArt.Yellow,Bounds=new Rectangle(1,56,478,2)});
            var userLabel=new Label{Text=T("USERNAME"),Font=NativeArt.Font(14,true),Bounds=new Rectangle(28,70,420,22)};var passLabel=new Label{Text=T("PASSWORD"),Font=NativeArt.Font(14,true),Bounds=new Rectangle(28,145,420,22)};
            var userBox=new Panel{Name="NetUserBorder",BackColor=NativeArt.Field,Bounds=new Rectangle(28,95,424,34)};
            var passBox=new Panel{Name="NetPasswordBorder",BackColor=NativeArt.Field,Bounds=new Rectangle(28,170,424,34)};
            var user=new TextBox{Name="NetLoginUser",Text=username,BorderStyle=BorderStyle.None,BackColor=NativeArt.Field,ForeColor=NativeArt.Text,Font=NativeArt.Font(17),Bounds=new Rectangle(10,7,404,22),MaxLength=254};
            var pass=new TextBox{Name="NetLoginPassword",Text=password,UseSystemPasswordChar=true,BorderStyle=BorderStyle.None,BackColor=NativeArt.Field,ForeColor=NativeArt.Text,Font=NativeArt.Font(17),Bounds=new Rectangle(10,7,404,22),MaxLength=512};
            userBox.Paint+=delegate(object sender,PaintEventArgs e){using(var pen=new Pen(user.Focused?NativeArt.Yellow:NativeArt.Border))e.Graphics.DrawRectangle(pen,0,0,userBox.Width-1,userBox.Height-1);};
            passBox.Paint+=delegate(object sender,PaintEventArgs e){using(var pen=new Pen(pass.Focused?NativeArt.Yellow:NativeArt.Border))e.Graphics.DrawRectangle(pen,0,0,passBox.Width-1,passBox.Height-1);};
            user.GotFocus+=delegate{userBox.Invalidate();};user.LostFocus+=delegate{userBox.Invalidate();};pass.GotFocus+=delegate{passBox.Invalidate();};pass.LostFocus+=delegate{passBox.Invalidate();};userBox.Click+=delegate{user.Focus();};passBox.Click+=delegate{pass.Focus();};userBox.Controls.Add(user);passBox.Controls.Add(pass);
            var save=new CheckBox{Name="NetLoginRemember",Text=T("Remember login"),Checked=remember,AutoSize=true,FlatStyle=FlatStyle.Flat,Font=NativeArt.Font(14),ForeColor=NativeArt.Text,Location=new Point(29,222)};
            var cancel=new Button{Text=T("CANCEL"),DialogResult=DialogResult.Cancel,FlatStyle=FlatStyle.Flat,Font=NativeArt.Font(16,true,true),ForeColor=NativeArt.Text,BackColor=Color.FromArgb(40,41,45),Bounds=new Rectangle(28,275,201,40)};
            var ok=new Button{Text=T("LOG IN"),DialogResult=DialogResult.OK,FlatStyle=FlatStyle.Flat,Font=NativeArt.Font(16,true,true),ForeColor=Color.Black,BackColor=NativeArt.Yellow,Bounds=new Rectangle(251,275,201,40),UseVisualStyleBackColor=false};
            form.Controls.Add(userLabel);form.Controls.Add(passLabel);form.Controls.Add(userBox);form.Controls.Add(passBox);form.Controls.Add(save);form.Controls.Add(cancel);form.Controls.Add(ok);form.AcceptButton=ok;form.CancelButton=cancel;form.Shown+=delegate{if(String.IsNullOrEmpty(user.Text)){user.Focus();user.SelectionStart=user.TextLength;}else{pass.Focus();pass.SelectionStart=pass.TextLength;pass.SelectionLength=0;}};
            if(form.ShowDialog(this)!=DialogResult.OK)return;username=user.Text.Trim();password=pass.Text;remember=save.Checked;pass.Clear();
        }
        _netBusy=true;int generation=++_netGeneration;try{string u=username,p=password;var client=new OgbgAuthClient(_netEndpoint);var identity=await Task.Run(()=>client.Login(u,p));if(IsDisposed||_exitRequested||generation!=_netGeneration)return;if(remember)vault.Save(username,password);else vault.Remove();_onlineIdentity=identity;_netMode=true;_webNotice="";_webNoticeRevision++;}
        catch(Exception ex){if(!IsDisposed&&generation==_netGeneration)MessageBox.Show(this,ex.Message,"OG:BG login",MessageBoxButtons.OK,MessageBoxIcon.Error);}
        finally{password=null;_netBusy=false;if(!IsDisposed)NativePublish();}
    }
    private void SwitchToLan(){if(_roomClient!=null||_finished)return;_netGeneration++;_netMode=false;_netChatConnected=false;_globalHistory.Clear();_onlineNames=_globalNetwork==null?new string[0]:_globalNetwork.Names();_netRooms.Clear();_netRoomsByMaster.Clear();NativePublish();}
}
