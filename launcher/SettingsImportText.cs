using System;
using System.Collections.Generic;

internal static class SettingsImportText {
    // Button, picker title, completion caption, summary format, first-run note.
    static readonly Dictionary<string,string[]> Rows=new Dictionary<string,string[]>(StringComparer.OrdinalIgnoreCase) {
        {"en",new[]{"IMPORT PUBG SETTINGS","Select the newer PUBG GameUserSettings.ini","Settings import","Settings changed: {0}  •  Mouse: {1}  •  Key bindings: {2}  •  Skipped: {3}","To import compatible controls, start PUBG 2019 once and then import again."}},
        {"de",new[]{"PUBG-EINSTELLUNGEN IMPORTIEREN","GameUserSettings.ini des neueren PUBG auswählen","Einstellungen importieren","Geändert: {0}  •  Maus: {1}  •  Tasten: {2}  •  Übersprungen: {3}","Für kompatible Steuerungen PUBG 2019 einmal starten und erneut importieren."}},
        {"ar",new[]{"استيراد إعدادات PUBG","اختر ملف GameUserSettings.ini للعبة الأحدث","استيراد الإعدادات","الإعدادات: {0}  •  الفأرة: {1}  •  المفاتيح: {2}  •  المتجاوز: {3}","لاستيراد أدوات التحكم المتوافقة، شغّل PUBG 2019 مرة ثم أعد الاستيراد."}},
        {"es",new[]{"IMPORTAR AJUSTES DE PUBG","Selecciona GameUserSettings.ini del PUBG más reciente","Importar ajustes","Ajustes: {0}  •  Ratón: {1}  •  Teclas: {2}  •  Omitidos: {3}","Para importar controles compatibles, inicia PUBG 2019 una vez y vuelve a importar."}},
        {"fr",new[]{"IMPORTER LES PARAMÈTRES PUBG","Choisissez GameUserSettings.ini du PUBG récent","Importer les paramètres","Paramètres : {0}  •  Souris : {1}  •  Touches : {2}  •  Ignorés : {3}","Pour importer les commandes compatibles, lancez PUBG 2019 une fois puis réimportez."}},
        {"id",new[]{"IMPOR PENGATURAN PUBG","Pilih GameUserSettings.ini PUBG yang lebih baru","Impor pengaturan","Pengaturan: {0}  •  Mouse: {1}  •  Tombol: {2}  •  Dilewati: {3}","Untuk mengimpor kontrol yang cocok, jalankan PUBG 2019 sekali lalu impor lagi."}},
        {"it",new[]{"IMPORTA IMPOSTAZIONI PUBG","Seleziona GameUserSettings.ini del PUBG più recente","Importa impostazioni","Impostazioni: {0}  •  Mouse: {1}  •  Tasti: {2}  •  Ignorati: {3}","Per importare i comandi compatibili, avvia PUBG 2019 una volta e ripeti l’importazione."}},
        {"uk",new[]{"ІМПОРТУВАТИ НАЛАШТУВАННЯ PUBG","Виберіть GameUserSettings.ini новішої версії PUBG","Імпорт налаштувань","Налаштування: {0}  •  Миша: {1}  •  Клавіші: {2}  •  Пропущено: {3}","Щоб імпортувати сумісне керування, запустіть PUBG 2019 один раз і повторіть імпорт."}},
        {"ga",new[]{"IOMPÓRTÁIL SOCRUITHE PUBG","Roghnaigh GameUserSettings.ini ón leagan níos nuaí de PUBG","Iompórtáil socruithe","Socruithe: {0}  •  Luch: {1}  •  Eochracha: {2}  •  Fágtha ar lár: {3}","Chun rialuithe comhoiriúnacha a iompórtáil, oscail PUBG 2019 uair amháin agus iompórtáil arís."}},
        {"ja",new[]{"PUBG設定をインポート","新しいPUBGのGameUserSettings.iniを選択","設定のインポート","設定: {0}  •  マウス: {1}  •  キー: {2}  •  スキップ: {3}","対応する操作を読み込むには、PUBG 2019を一度起動してから再インポートしてください。"}},
        {"ko",new[]{"PUBG 설정 가져오기","최신 PUBG의 GameUserSettings.ini 선택","설정 가져오기","설정: {0}  •  마우스: {1}  •  키: {2}  •  건너뜀: {3}","호환되는 조작을 가져오려면 PUBG 2019을 한 번 실행한 뒤 다시 가져오세요."}},
        {"pl",new[]{"IMPORTUJ USTAWIENIA PUBG","Wybierz GameUserSettings.ini nowszego PUBG","Import ustawień","Ustawienia: {0}  •  Mysz: {1}  •  Klawisze: {2}  •  Pominięto: {3}","Aby zaimportować zgodne sterowanie, uruchom PUBG 2019 i ponów import."}},
        {"pt",new[]{"IMPORTAR CONFIGURAÇÕES PUBG","Selecione GameUserSettings.ini do PUBG mais recente","Importar configurações","Configurações: {0}  •  Mouse: {1}  •  Teclas: {2}  •  Ignoradas: {3}","Para importar controles compatíveis, execute PUBG 2019 uma vez e importe novamente."}},
        {"ru",new[]{"ИМПОРТ НАСТРОЕК PUBG","Выберите GameUserSettings.ini новой версии PUBG","Импорт настроек","Настройки: {0}  •  Мышь: {1}  •  Клавиши: {2}  •  Пропущено: {3}","Чтобы импортировать совместимое управление, запустите PUBG 2019 и повторите импорт."}},
        {"th",new[]{"นำเข้าการตั้งค่า PUBG","เลือก GameUserSettings.ini ของ PUBG รุ่นใหม่","นำเข้าการตั้งค่า","การตั้งค่า: {0}  •  เมาส์: {1}  •  ปุ่ม: {2}  •  ข้าม: {3}","หากต้องการนำเข้าปุ่มที่รองรับ ให้เปิด PUBG 2019 หนึ่งครั้งแล้วนำเข้าอีกครั้ง"}},
        {"tr",new[]{"PUBG AYARLARINI AKTAR","Yeni PUBG sürümünün GameUserSettings.ini dosyasını seç","Ayarları aktar","Ayar: {0}  •  Fare: {1}  •  Tuş: {2}  •  Atlanan: {3}","Uyumlu kontrolleri aktarmak için PUBG 2019'yi bir kez açıp tekrar aktarın."}},
        {"vi",new[]{"NHẬP CÀI ĐẶT PUBG","Chọn GameUserSettings.ini của PUBG mới hơn","Nhập cài đặt","Cài đặt: {0}  •  Chuột: {1}  •  Phím: {2}  •  Bỏ qua: {3}","Để nhập điều khiển tương thích, hãy chạy PUBG 2019 một lần rồi nhập lại."}},
        {"zh",new[]{"导入 PUBG 设置","选择新版 PUBG 的 GameUserSettings.ini","导入设置","设置：{0}  •  鼠标：{1}  •  按键：{2}  •  跳过：{3}","要导入兼容的控制设置，请先运行一次 PUBG 2019，然后重新导入。"}}
    };
    public static string Get(string language,int index) {string[] row;if(!Rows.TryGetValue(language??"",out row)){string baseCode=(language??"").Split('-')[0];if(!Rows.TryGetValue(baseCode,out row))row=Rows["en"];}return row[index];}
}

internal static class SettingsCloseText {
    private static readonly System.Collections.Generic.Dictionary<string,string[]> Rows = new System.Collections.Generic.Dictionary<string,string[]>(System.StringComparer.OrdinalIgnoreCase) {
        {"en",new[]{"WHEN X IS CLICKED","Minimize to tray","Exit launcher"}},
        {"de",new[]{"BEIM KLICK AUF X","In den Infobereich minimieren","Launcher beenden"}},
        {"fr",new[]{"AU CLIC SUR X","Réduire dans la zone de notification","Quitter le lanceur"}},
        {"es",new[]{"AL PULSAR X","Minimizar a la bandeja","Salir del lanzador"}},
        {"es-MX",new[]{"AL PULSAR X","Minimizar a la bandeja","Salir del lanzador"}},
        {"it",new[]{"ALLA PRESSIONE DI X","Riduci a icona nell'area di notifica","Esci dal launcher"}},
        {"pt",new[]{"AO CLICAR EM X","Minimizar para a bandeja","Sair do launcher"}},
        {"pt-BR",new[]{"AO CLICAR EM X","Minimizar para a bandeja","Sair do launcher"}},
        {"pl",new[]{"PO KLIKNIĘCIU X","Minimalizuj do zasobnika","Zamknij launcher"}},
        {"tr",new[]{"X TIKLANDIĞINDA","Sistem tepsisine küçült","Başlatıcıdan çık"}},
        {"ru",new[]{"ПРИ НАЖАТИИ X","Свернуть в область уведомлений","Закрыть лаунчер"}},
        {"uk",new[]{"ПРИ НАТИСКАННІ X","Згорнути в область сповіщень","Закрити лаунчер"}},
        {"ja",new[]{"Xを押したとき","通知領域に最小化","ランチャーを終了"}},
        {"ko",new[]{"X 클릭 시","시스템 트레이로 최소화","런처 종료"}},
        {"zh-CN",new[]{"点击 X 时","最小化到系统托盘","退出启动器"}},
        {"zh-TW",new[]{"點擊 X 時","最小化至系統匣","結束啟動器"}},
        {"ar",new[]{"عند الضغط على X","تصغير إلى علبة النظام","الخروج من المشغل"}},
        {"th",new[]{"เมื่อกด X","ย่อไปยังถาดระบบ","ออกจากตัวเปิดเกม"}},
        {"vi",new[]{"KHI NHẤN X","Thu nhỏ vào khay hệ thống","Thoát trình khởi chạy"}},
        {"id",new[]{"SAAT X DIKLIK","Minimalkan ke baki sistem","Keluar dari peluncur"}},
        {"ga-IE",new[]{"NUAIR A BHRÚITEAR X","Íoslaghdaigh go tráidire","Scoir den tosaitheoir"}}
    };
    internal static string Get(string language,int index){string[] row;if(!Rows.TryGetValue(language??"",out row))row=Rows["en"];return row[index];}
}
