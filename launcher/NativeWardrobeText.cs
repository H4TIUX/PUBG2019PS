using System;
using System.Collections.Generic;
internal static class NativeWardrobeText {
    private static readonly Dictionary<string,string> Choose=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase){
        {"en","CHOOSE APPEARANCE"},{"ko","외형 선택"},{"zh-CN","选择外观"},{"zh-TW","選擇外觀"},{"ar","اختر مظهرك"},{"de","AUSSEHEN WÄHLEN"},{"es","ELEGIR APARIENCIA"},{"es-MX","ELEGIR APARIENCIA"},{"fr","CHOISIR L’APPARENCE"},{"it","SCEGLI L’ASPETTO"},{"pl","WYBIERZ WYGLĄD"},{"pt","ESCOLHER APARÊNCIA"},{"pt-BR","ESCOLHER APARÊNCIA"},{"tr","GÖRÜNÜM SEÇ"},{"ru","ВЫБРАТЬ ВНЕШНОСТЬ"},{"ja","外見を選択"},{"th","เลือกรูปลักษณ์"},{"vi","CHỌN DIỆN MẠO"},{"id","PILIH PENAMPILAN"},{"uk","ВИБРАТИ ЗОВНІШНІСТЬ"},{"ga-IE","ROGHNAIGH CUMA"}
    };
    internal static string Get(string language){string value;return Choose.TryGetValue(language??"en",out value)?value:Choose["en"];}
}
