using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Text;
using System.Web.Script.Serialization;

// Static UI strings only. Player names, messages and server addresses are never translated.
internal static class NativeUiText {
    static readonly Dictionary<string, Dictionary<string,string>> rows = Load();
    static Dictionary<string, Dictionary<string,string>> Load() {
        var result = new Dictionary<string, Dictionary<string,string>>(StringComparer.OrdinalIgnoreCase);
        using(var stream=Assembly.GetExecutingAssembly().GetManifestResourceStream("Launcher.NativeUiTranslations")) {
            if(stream==null)return result;
            using(var reader=new StreamReader(stream,Encoding.UTF8)) {
                var json=new JavaScriptSerializer { MaxJsonLength=4000000 };
                var raw=json.Deserialize<Dictionary<string,Dictionary<string,string>>>(reader.ReadToEnd());
                foreach(var row in raw)result[row.Key]=row.Value;
            }
        }
        return result;
    }
    internal static string Get(string lang,string key) {
        if(String.IsNullOrEmpty(key)||String.IsNullOrEmpty(lang)||lang.Equals("en",StringComparison.OrdinalIgnoreCase))return key;
        Dictionary<string,string> row; string result;
        if(rows.TryGetValue(lang,out row)&&row.TryGetValue(key,out result)&&!String.IsNullOrWhiteSpace(result))return result;
        if(key=="SERVER DETAILS (SELECTED)")return Get(lang,"SERVER DETAILS");
        if(key=="GLOBAL (EVERYONE)")return Get(lang,"GLOBAL CHAT");
        if(key=="Server name ⌃")return Get(lang,"Server name")+" ⌃";
        if(key=="SLOTS")return Get(lang,"Slot");
        if(key=="BOTS")return Get(lang,"Bots");
        if(key=="YES")return Get(lang,"Yes");
        if(key=="NO")return Get(lang,"No");
        string translated=LanText.Get(lang,key);
        if(translated!=key)return translated;
        // Reuse the existing multiplayer dictionary for capitalization variants.
        string upper=key.ToUpperInvariant();
        translated=LanText.Get(lang,upper);
        return translated!=upper?translated:key;
    }
    internal static string ConfirmQuestion(string lang) {
        string[] codes={"en","de","fr","es","es-MX","pt","pt-BR","it","pl","tr","ru","uk","ko","zh-CN","zh-TW","ar","ja","th","vi","id","ga-IE"};
        string[] values={"Open the character studio?","Charakterstudio öffnen?","Ouvrir le studio de personnage ?","¿Abrir el estudio de personaje?","¿Abrir el estudio de personaje?","Abrir o estúdio de personagem?","Abrir o estúdio de personagem?","Aprire lo studio del personaggio?","Otworzyć studio postaci?","Karakter stüdyosu açılsın mı?","Открыть студию персонажа?","Відкрити студію персонажа?","캐릭터 스튜디오를 열까요?","打开角色工作室？","開啟角色工作室？","هل تريد فتح استوديو الشخصية؟","キャラクタースタジオを開きますか？","เปิดสตูดิโอตัวละครหรือไม่?","Mở phòng tạo nhân vật?","Buka studio karakter?","Oscail stiúideo an charachtair?"};
        for(int i=0;i<codes.Length;i++)if(String.Equals(lang,codes[i],StringComparison.OrdinalIgnoreCase))return values[i];
        return values[0];
    }
}
