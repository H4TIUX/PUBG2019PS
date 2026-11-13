using System;
using System.Collections.Generic;
internal static class PlayerNameNotice {
    static readonly Dictionary<string,string> Messages=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase){
        {"en","Please set a player name in Settings."},
        {"de","Bitte lege in den Einstellungen einen Spielernamen fest."},
        {"fr","Définissez un nom de joueur dans les paramètres."},
        {"es","Configura un nombre de jugador en Ajustes."},
        {"es-MX","Configura un nombre de jugador en Ajustes."},
        {"pt","Define um nome de jogador nas Definições."},
        {"pt-BR","Defina um nome de jogador nas Configurações."},
        {"it","Imposta un nome giocatore nelle impostazioni."},
        {"pl","Ustaw nazwę gracza w ustawieniach."},
        {"tr","Ayarlar bölümünden bir oyuncu adı belirle."},
        {"ru","Укажите имя игрока в настройках."},
        {"uk","Укажіть ім’я гравця в налаштуваннях."},
        {"ko","설정에서 플레이어 이름을 지정하세요."},
        {"zh-CN","请在设置中设置玩家名称。"},
        {"zh-TW","請在設定中設定玩家名稱。"},
        {"ar","يرجى تعيين اسم اللاعب في الإعدادات."},
        {"ja","設定でプレイヤー名を設定してください。"},
        {"th","โปรดตั้งชื่อผู้เล่นในการตั้งค่า"},
        {"vi","Hãy đặt tên người chơi trong phần Cài đặt."},
        {"id","Atur nama pemain di Pengaturan."},
        {"ga-IE","Socraigh ainm imreora sna Socruithe."}
    };
    internal static string Get(string language){string message;return Messages.TryGetValue(language??"en",out message)?message:Messages["en"];}
}
