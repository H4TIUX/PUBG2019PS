using System;
using System.IO;
using System.Reflection;
using System.Windows.Forms;
class TestProfileUI {
 [STAThread] static int Main(){string dir=AppDomain.CurrentDomain.BaseDirectory;string result=Path.Combine(dir,"profile-ui-result.txt");
 try {var asm=Assembly.LoadFrom(Path.Combine(dir,"PUBG2019Launcher.exe"));var type=asm.GetType("LauncherForm",true);var form=(Form)Activator.CreateInstance(type,true);var timer=new Timer{Interval=100};bool ok=false;string error="Profile did not open.";timer.Tick+=(s,e)=>{foreach(Form window in Application.OpenForms){if(window==form)continue;var import=window.Controls.Find("ImportSettings",true);var save=window.Controls.Find("SaveProfile",true);var update=window.Controls.Find("CheckUpdateTest",true);ok=import.Length==1 && save.Length==1 && update.Length==1 && import[0].Enabled && import[0].Bottom<save[0].Top && save[0].Bottom<=window.ClientSize.Height;error=ok?"PASS: import, save and update controls fit in the profile.":"Profile controls missing or overlapping.";timer.Stop();window.Close();break;}};
 timer.Start();type.GetMethod("ShowProfile",BindingFlags.Instance|BindingFlags.NonPublic).Invoke(form,null);timer.Dispose();form.Dispose();File.WriteAllText(result,error);return ok?0:1;
 } catch(Exception e){File.WriteAllText(result,e.ToString());return 1;}}
}
