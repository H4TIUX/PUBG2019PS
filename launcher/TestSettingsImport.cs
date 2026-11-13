using System;
using System.IO;
using System.Linq;
using System.Text;
class TestSettingsImport {
 static void Check(bool b,string m){if(!b)throw new Exception(m);}
 static void Main(){string root=Path.Combine(Path.GetTempPath(),"pubg2019-import-test-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(root);
 try{
 string source=Path.Combine(root,"modern.ini"),launcher=Path.Combine(root,"game");Directory.CreateDirectory(launcher);
 string header="[/Script/TslGame.TslGameUserSettings]\r\n";
 string modern=header+"ResolutionSizeX=2560\r\nFpsCameraFov=100\r\nAccountToken=DO_NOT_COPY\r\nCustomInputSettins=(ActionKeyList=((ActionName=\"Jump\",Keys=((Key=SpaceBar),())),(ActionName=\"NewFeature\",Keys=((Key=Q),()))),AxisKeyList=((AxisName=\"MoveForward\",Scale=1.000000,Keys=((Key=Up),()))),SensitiveMap=((Mouse,(Array=((SensitiveName=\"Normal\",Sensitivity=55.0,LastConvertedSensitivity=0.02)))),(GamePad,(Array=((SensitiveName=\"Normal\",Sensitivity=99.0,LastConvertedSensitivity=0.1))))),bInvertMouse=True)\r\n[ScalabilityGroups]\r\nsg.ShadowQuality=4\r\n";
 modern=modern.Replace("SensitiveMap=", "MouseSensitiveList=((MouseSensitiveName=\"Normal\",MouseSensitivity=65.0,LastConvertedMouseSensitivity=0.02)),SensitiveMap=");
 File.WriteAllText(source,modern,new UTF8Encoding(false));string dest=SettingsImport.Destination(launcher);Directory.CreateDirectory(Path.GetDirectoryName(dest));
 string target=header+"ResolutionSizeX=1600\r\nKeepMe=unchanged\r\nCustomInputSettins=(ActionKeyList=((ActionName=\"Jump\",Keys=((Key=J),())),(ActionName=\"Reload\",Keys=((Key=R),()))),AxisKeyList=((AxisName=\"MoveForward\",Scale=1.000000,Keys=((Key=W),()))),SensitiveMap=((Mouse,(Array=((SensitiveName=\"Normal\",Sensitivity=30.0,LastConvertedSensitivity=0.01)))),(GamePad,(Array=((SensitiveName=\"Normal\",Sensitivity=10.0,LastConvertedSensitivity=0.01))))),bInvertMouse=False)\r\n";
 target=target.Replace("SensitiveMap=", "MouseSensitiveList=((MouseSensitiveName=\"Normal\",MouseSensitivity=10.0,LastConvertedMouseSensitivity=0.01,Extra=Keep),(MouseSensitiveName=\"Future\",MouseSensitivity=15.0,LastConvertedMouseSensitivity=0.01)),SensitiveMap=");
 File.WriteAllText(dest,target,new UTF8Encoding(false));byte[] before=File.ReadAllBytes(source);var result=SettingsImport.Import(source,launcher);string actual=File.ReadAllText(dest);
 Check(before.SequenceEqual(File.ReadAllBytes(source)),"source changed");Check(actual.Contains("FpsCameraFov=100")&&actual.Contains("sg.ShadowQuality=4"),"values missing");
 Check(actual.Contains("ActionName=\"Jump\",Keys=((Key=SpaceBar),())")&&actual.Contains("ActionName=\"Reload\",Keys=((Key=R),())"),"bindings");
 Check(actual.Contains("Sensitivity=55.0")&&actual.Contains("Sensitivity=10.0")&&!actual.Contains("Sensitivity=99.0"),"mouse map");
 Check(actual.Contains("MouseSensitivity=65.0,LastConvertedMouseSensitivity=0.02,Extra=Keep")&&actual.Contains("MouseSensitiveName=\"Future\""),"mouse list preserves unrelated settings");
 Check(actual.Contains("KeepMe=unchanged")&&!actual.Contains("DO_NOT_COPY")&&result.Bindings==2,"private data or result");
 byte[] portable=File.ReadAllBytes(dest);SettingsImport.Import(source,launcher);Check(portable.SequenceEqual(File.ReadAllBytes(dest)),"not idempotent");
 string fresh=Path.Combine(root,"fresh");Directory.CreateDirectory(fresh);var first=SettingsImport.Import(source,fresh);Check(first.NeedExistingControls&&!File.ReadAllText(SettingsImport.Destination(fresh)).Contains("NewFeature"),"fresh controls");
 Console.WriteLine("PASS: validated settings, bindings and mouse map; source and unrelated fields preserved.");
 }finally{try{Directory.Delete(root,true);}catch{}}
 }
}
