using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;

internal static class LauncherPermissionText {
    internal const string MemoryFailureMessage = "Not enough memory to run the server. Windows or PUBG reported a memory allocation failure. Free memory or increase the Windows paging file, then try again.";
    internal static bool IsMemoryFailureCode(int code) {
        // Explicit allocation/commit failures, not arbitrary crashes or low free RAM.
        uint status=unchecked((uint)code);
        return code==8 || code==14 || code==1455 || status==0x8007000E || status==0xC0000017 || status==0xC000012D;
    }
    private static readonly Dictionary<string,string[]> Rows=new Dictionary<string,string[]>(StringComparer.OrdinalIgnoreCase) {
        {"en",new[]{"Permission was not granted for {0} (Windows {1}). Close the game and launcher, right-click PUBG2019Launcher.exe, choose Run as administrator and approve the prompt.","Could not load {0} (exit code {1}). Check the injector output and Windows file permissions."}},
        {"de",new[]{"Keine Berechtigung für {0} (Windows {1}). Spiel und Launcher schließen, PUBG2019Launcher.exe rechtsklicken, Als Administrator ausführen wählen und bestätigen.","{0} konnte nicht geladen werden (Code {1}). Injector-Ausgabe und Windows-Dateiberechtigungen prüfen."}},
        {"ko",new[]{"{0} 권한이 없습니다 (Windows {1}). 게임과 런처를 닫고 PUBG2019Launcher.exe를 우클릭해 관리자 권한으로 실행을 선택한 뒤 승인하세요.","{0} 로드 실패 (코드 {1}). 인젝터 출력과 Windows 파일 권한을 확인하세요."}},
        {"zh-CN",new[]{"无权访问 {0}（Windows {1}）。关闭游戏和启动器，右键 PUBG2019Launcher.exe，选择以管理员身份运行并确认。","无法加载 {0}（代码 {1}）。请检查注入器输出及 Windows 文件权限。"}},
        {"zh-TW",new[]{"無權存取 {0}（Windows {1}）。關閉遊戲和啟動器，右鍵 PUBG2019Launcher.exe，選擇以系統管理員身分執行並確認。","無法載入 {0}（代碼 {1}）。請檢查注入器輸出及 Windows 檔案權限。"}},
        {"ar",new[]{"لم يُمنح الإذن لـ {0} (Windows {1}). أغلق اللعبة والمشغل، وانقر بزر الفأرة الأيمن على PUBG2019Launcher.exe واختر تشغيل كمسؤول ثم وافق.","تعذر تحميل {0} (الرمز {1}). تحقق من مخرجات المحقن وأذونات ملفات Windows."}},
        {"es",new[]{"No se concedió permiso para {0} (Windows {1}). Cierra el juego y el lanzador, haz clic derecho en PUBG2019Launcher.exe, elige Ejecutar como administrador y confirma.","No se pudo cargar {0} (código {1}). Revisa la salida del inyector y los permisos de Windows."}},
        {"fr",new[]{"Autorisation refusée pour {0} (Windows {1}). Fermez le jeu et le lanceur, faites un clic droit sur PUBG2019Launcher.exe, choisissez Exécuter en tant qu’administrateur et confirmez.","Impossible de charger {0} (code {1}). Vérifiez la sortie de l’injecteur et les autorisations Windows."}},
        {"it",new[]{"Autorizzazione negata per {0} (Windows {1}). Chiudi gioco e launcher, fai clic destro su PUBG2019Launcher.exe, scegli Esegui come amministratore e conferma.","Impossibile caricare {0} (codice {1}). Controlla l'output dell'iniettore e i permessi Windows."}},
        {"pl",new[]{"Brak uprawnień do {0} (Windows {1}). Zamknij grę i launcher, kliknij PUBG2019Launcher.exe prawym przyciskiem, wybierz Uruchom jako administrator i potwierdź.","Nie można załadować {0} (kod {1}). Sprawdź komunikaty injectora i uprawnienia plików Windows."}},
        {"pt",new[]{"Permissão negada para {0} (Windows {1}). Feche o jogo e o launcher, clique com o botão direito em PUBG2019Launcher.exe, escolha Executar como administrador e confirme.","Não foi possível carregar {0} (código {1}). Verifique a saída do injetor e as permissões do Windows."}},
        {"tr",new[]{"{0} için izin verilmedi (Windows {1}). Oyunu ve başlatıcıyı kapatın, PUBG2019Launcher.exe dosyasına sağ tıklayıp Yönetici olarak çalıştır seçeneğini seçin ve onaylayın.","{0} yüklenemedi (kod {1}). Enjektör çıktısını ve Windows dosya izinlerini kontrol edin."}},
        {"ru",new[]{"Нет разрешения для {0} (Windows {1}). Закройте игру и лаунчер, щёлкните правой кнопкой по PUBG2019Launcher.exe, выберите Запуск от имени администратора и подтвердите.","Не удалось загрузить {0} (код {1}). Проверьте вывод инжектора и разрешения файлов Windows."}},
        {"ja",new[]{"{0} の権限がありません（Windows {1}）。ゲームとランチャーを閉じ、PUBG2019Launcher.exe を右クリックして「管理者として実行」を選び、確認してください。","{0} を読み込めません（コード {1}）。インジェクターの出力と Windows のファイル権限を確認してください。"}},
        {"th",new[]{"ไม่ได้รับสิทธิ์สำหรับ {0} (Windows {1}) ปิดเกมและตัวเปิด คลิกขวา PUBG2019Launcher.exe เลือกเรียกใช้ในฐานะผู้ดูแลระบบแล้วกดยืนยัน","โหลด {0} ไม่ได้ (รหัส {1}) ตรวจสอบข้อความของตัวฉีดและสิทธิ์ไฟล์ Windows"}},
        {"vi",new[]{"Không được cấp quyền cho {0} (Windows {1}). Đóng trò chơi và trình khởi chạy, nhấp phải PUBG2019Launcher.exe, chọn Chạy với tư cách quản trị viên và xác nhận.","Không thể tải {0} (mã {1}). Kiểm tra đầu ra của bộ nạp và quyền tệp Windows."}},
        {"id",new[]{"Izin untuk {0} tidak diberikan (Windows {1}). Tutup game dan peluncur, klik kanan PUBG2019Launcher.exe, pilih Jalankan sebagai administrator lalu setujui.","Tidak dapat memuat {0} (kode {1}). Periksa keluaran injector dan izin berkas Windows."}},
        {"uk",new[]{"Немає дозволу для {0} (Windows {1}). Закрийте гру й лаунчер, клацніть правою кнопкою PUBG2019Launcher.exe, виберіть Запуск від імені адміністратора та підтвердьте.","Не вдалося завантажити {0} (код {1}). Перевірте вивід інжектора та дозволи файлів Windows."}},
        {"ga-IE",new[]{"Níor tugadh cead do {0} (Windows {1}). Dún an cluiche agus an tosaitheoir, deaschliceáil PUBG2019Launcher.exe, roghnaigh Rith mar riarthóir agus deimhnigh.","Níorbh fhéidir {0} a luchtú (cód {1}). Seiceáil aschur an insteallóra agus ceadanna comhad Windows."}}
    };
    public static string Format(string language,string input) {
        Match match=Regex.Match(input??"",@"\[PUBG2019_(ACCESS_DENIED|ELEVATION_CANCELLED|INJECTOR_FAILED)\|(-?\d+)\|([^\]\r\n|]+)\]");
        if(!match.Success)return input;
        string key=(language??"en");if(key.Equals("es-MX",StringComparison.OrdinalIgnoreCase))key="es";if(key.Equals("pt-BR",StringComparison.OrdinalIgnoreCase))key="pt";
        string[] values;if(!Rows.TryGetValue(key,out values))values=Rows["en"];
        return String.Format(values[match.Groups[1].Value=="INJECTOR_FAILED"?1:0],match.Groups[3].Value,match.Groups[2].Value);
    }
    public static string FromException(Exception exception,string component) {
        for(Exception e=exception;e!=null;e=e.InnerException) {
            if(e is OutOfMemoryException || IsMemoryFailureCode(e.HResult) ||
                (e is System.ComponentModel.Win32Exception && IsMemoryFailureCode(((System.ComponentModel.Win32Exception)e).NativeErrorCode)))return MemoryFailureMessage;
            int code=e is System.ComponentModel.Win32Exception?((System.ComponentModel.Win32Exception)e).NativeErrorCode:(e is UnauthorizedAccessException?5:(e.HResult & 0xffff));
            if(code==5 || code==740 || code==1223)return "[PUBG2019_"+(code==1223?"ELEVATION_CANCELLED":"ACCESS_DENIED")+"|"+code+"|"+component+"]";
        }
        return exception.Message;
    }
}
