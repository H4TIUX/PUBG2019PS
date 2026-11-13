using System;
using System.Collections.Generic;

// Hand-written launcher text. These resources are embedded in the launcher;
// no game data or log messages are sent to a translation service.
internal static class LauncherTranslations
{
    private static readonly string[] Keys = {
        "profile", "exit", "profile_title", "subtitle", "player_name", "player_id", "game_language", "game_default", "copy_id", "new_id", "save",
        "info", "renew_warning", "profile_error", "copy_error", "host", "join", "join_my", "stop", "local", "auto", "ready",
        "starting", "launching", "connecting", "stopping", "waiting", "stopped", "started", "failed", "address_hint", "experimental", "map_hint",
        "invalid_name", "invalid_address", "already_running", "map", "address", "new", "classic", "error", "requested_language", "requested_name", "terrain_loading", "terrain_ready", "module_loaded", "process_exited", "bot_jump", "starting_log", "joining_log"
    };

    private static readonly Dictionary<string, string[]> Rows = new Dictionary<string, string[]>(StringComparer.OrdinalIgnoreCase) {
        { "ko", ("프로필|종료|플레이어 프로필|플레이어 이름과 고유 ID|플레이어 이름|플레이어 ID|게임 언어|게임 설정 사용|ID 복사|새 ID 만들기|저장|언어는 다음 게임 시작부터 적용됩니다.~다른 PC로 옮길 때 PUBG2019Profile.ini를 보관하세요.|새 플레이어 ID를 만들까요? 현재 ID가 교체되며 연결된 향후 온라인 데이터는 이전되지 않을 수 있습니다.|프로필 오류|복사 실패|서버 시작|서버 참가|내 서버 참가|서버 중지|로컬 서버 시작|자동 참가|준비 완료|서버 시작 중|게임 시작 중|서버 연결 중|서버 중지 중|플레이어 대기 중|서버 중지됨|게임 시작됨|시작 실패|참가할 서버 주소를 입력하세요.|실험적인 맵입니다. 설치된 게임에 따라 이용 가능 여부가 다릅니다.|로컬 게임의 맵과 봇 수를 선택하세요.|플레이어 이름이 올바르지 않습니다.|서버 주소가 올바르지 않습니다.|이 폴더에서 로컬 서버가 이미 실행 중입니다.|맵|서버 주소|신규|클래식|오류|선택한 게임 언어|선택한 플레이어 이름|서버 지형 로딩 중|서버 지형 준비 완료|모듈을 불러왔습니다|프로세스 종료 코드|비행기에서 뛰어내렸습니다|{0} 시작 중, 봇 {1}명|{0}에 연결 중").Split('|') },
        { "zh-CN", ("资料|退出|玩家资料|玩家名称和唯一 ID|玩家名称|玩家 ID|游戏语言|使用游戏设置|复制 ID|生成新 ID|保存|语言将在下次启动游戏时生效。~更换电脑时请保留 PUBG2019Profile.ini。|生成新的玩家 ID？当前 ID 将被替换，今后关联的在线数据可能无法转移。|资料错误|复制失败|启动服务器|加入服务器|加入我的服务器|停止服务器|启动本地服务器|自动加入|就绪|正在启动服务器|正在启动游戏|正在连接服务器|正在停止服务器|等待玩家|服务器已停止|游戏已启动|启动失败|输入要加入的服务器地址。|实验性地图，能否使用取决于已安装的游戏。|选择本地游戏的地图和机器人数量。|玩家名称无效。|服务器地址无效。|此游戏目录中已有本地服务器运行。|地图|服务器地址|新版|经典版|错误|所选游戏语言|所选玩家名称|正在加载服务器地形|服务器地形已就绪|模块加载成功|进程退出代码|跳出了飞机|正在启动 {0}，机器人数量：{1}|正在连接 {0}").Split('|') },
        { "zh-TW", ("資料|結束|玩家資料|玩家名稱與專屬 ID|玩家名稱|玩家 ID|遊戲語言|使用遊戲設定|複製 ID|產生新 ID|儲存|語言將於下次啟動遊戲時生效。~更換電腦時請保留 PUBG2019Profile.ini。|產生新的玩家 ID？目前的 ID 將被取代，未來關聯的線上資料可能無法轉移。|資料錯誤|複製失敗|啟動伺服器|加入伺服器|加入我的伺服器|停止伺服器|啟動本機伺服器|自動加入|準備就緒|正在啟動伺服器|正在啟動遊戲|正在連線伺服器|正在停止伺服器|等待玩家|伺服器已停止|遊戲已啟動|啟動失敗|輸入要加入的伺服器位址。|實驗性地圖，能否使用取決於已安裝的遊戲。|選擇本機遊戲的地圖及機器人數量。|玩家名稱無效。|伺服器位址無效。|此遊戲目錄中已有本機伺服器執行中。|地圖|伺服器位址|新版|經典版|錯誤|選取的遊戲語言|選取的玩家名稱|正在載入伺服器地形|伺服器地形已就緒|模組載入成功|程序結束代碼|跳出了飛機|正在啟動 {0}，機器人數量：{1}|正在連線 {0}").Split('|') },
        { "ar", ("الملف|خروج|ملف اللاعب|اسم اللاعب والمعرّف الفريد|اسم اللاعب|معرّف اللاعب|لغة اللعبة|استخدام إعداد اللعبة|نسخ المعرّف|إنشاء معرّف جديد|حفظ|تُطبّق اللغة عند تشغيل اللعبة مجددًا.~احتفظ بملف PUBG2019Profile.ini عند نقل اللعبة.|إنشاء معرّف لاعب جديد؟ سيُستبدل المعرّف الحالي وقد لا تنتقل البيانات المرتبطة به مستقبلًا.|خطأ في الملف|تعذّر النسخ|بدء الخادم|الانضمام للخادم|الانضمام لخادمي|إيقاف الخادم|بدء خادم محلي|الانضمام تلقائيًا|جاهز|جارٍ بدء الخادم|جارٍ بدء اللعبة|جارٍ الاتصال بالخادم|جارٍ إيقاف الخادم|بانتظار اللاعبين|توقف الخادم|بدأت اللعبة|فشل البدء|أدخل عنوان الخادم للانضمام إليه.|خريطة تجريبية؛ يعتمد توفرها على اللعبة المثبتة.|اختر الخريطة وعدد الروبوتات للمباراة المحلية.|اسم اللاعب غير صالح.|عنوان الخادم غير صالح.|يوجد خادم محلي يعمل بالفعل من مجلد اللعبة هذا.|الخريطة|عنوان الخادم|الجديدة|الكلاسيكية|خطأ|لغة اللعبة المحددة|اسم اللاعب المحدد|جارٍ تحميل تضاريس الخادم|تضاريس الخادم جاهزة|تم تحميل الوحدة|رمز انتهاء العملية|قفز من الطائرة|بدء {0} مع {1} روبوتًا|جارٍ الاتصال بـ {0}").Split('|') },
        { "es", ("PERFIL|Salir|Perfil del jugador|Tu nombre y tu ID único|NOMBRE DEL JUGADOR|ID DEL JUGADOR|IDIOMA DEL JUEGO|Usar ajuste del juego|COPIAR ID|CREAR NUEVO ID|GUARDAR|El idioma se aplica al iniciar el juego de nuevo.~Conserva PUBG2019Profile.ini al cambiar de PC.|¿Crear un nuevo ID? Se sustituirá el ID actual y puede que los futuros datos en línea asociados no se transfieran.|Error de perfil|Error al copiar|INICIAR SERVIDOR|UNIRSE AL SERVIDOR|UNIRSE A MI SERVIDOR|DETENER SERVIDOR|Iniciar servidor local|Unirse automáticamente|LISTO|INICIANDO SERVIDOR|INICIANDO JUEGO|CONECTANDO AL SERVIDOR|DETENIENDO SERVIDOR|ESPERANDO JUGADORES|SERVIDOR DETENIDO|JUEGO INICIADO|ERROR AL INICIAR|Introduce la dirección del servidor.|Mapa experimental. Su disponibilidad depende del juego instalado.|Elige el mapa y el número de bots para la partida local.|Nombre de jugador no válido.|Dirección del servidor no válida.|Ya se está ejecutando un servidor local desde esta carpeta.|MAPA|DIRECCIÓN DEL SERVIDOR|nuevo|clásico|ERROR|Idioma del juego solicitado|Nombre de jugador solicitado|Cargando terreno del servidor|Terreno del servidor listo|Módulo cargado|Código de salida del proceso|salta del avión|Iniciando {0} con {1} bots|Conectando a {0}").Split('|') },
        { "fr", ("PROFIL|Quitter|Profil du joueur|Ton nom et ton identifiant unique|NOM DU JOUEUR|ID DU JOUEUR|LANGUE DU JEU|Utiliser le réglage du jeu|COPIER L’ID|CRÉER UN NOUVEL ID|ENREGISTRER|La langue s’applique au prochain lancement du jeu.~Conserve PUBG2019Profile.ini lors d’un changement de PC.|Créer un nouvel ID ? L’ID actuel sera remplacé et les futures données en ligne associées pourraient ne pas être transférées.|Erreur de profil|Échec de la copie|LANCER LE SERVEUR|REJOINDRE UN SERVEUR|REJOINDRE MON SERVEUR|ARRÊTER LE SERVEUR|Lancer un serveur local|Rejoindre automatiquement|PRÊT|DÉMARRAGE DU SERVEUR|LANCEMENT DU JEU|CONNEXION AU SERVEUR|ARRÊT DU SERVEUR|EN ATTENTE DE JOUEURS|SERVEUR ARRÊTÉ|JEU LANCÉ|ÉCHEC DU DÉMARRAGE|Saisis l’adresse du serveur à rejoindre.|Carte expérimentale : sa disponibilité dépend du jeu installé.|Choisis la carte et le nombre de bots pour la partie locale.|Nom de joueur invalide.|Adresse du serveur invalide.|Un serveur local est déjà lancé depuis ce dossier.|CARTE|ADRESSE DU SERVEUR|nouvelle|classique|ERREUR|Langue du jeu demandée|Nom de joueur demandé|Chargement du terrain du serveur|Terrain du serveur prêt|Module chargé|Code de sortie du processus|saute de l’avion|Lancement de {0} avec {1} bots|Connexion à {0}").Split('|') },
        { "it", ("PROFILO|Esci|Profilo giocatore|Il tuo nome e ID univoco|NOME GIOCATORE|ID GIOCATORE|LINGUA DEL GIOCO|Usa impostazione del gioco|COPIA ID|CREA NUOVO ID|SALVA|La lingua verrà applicata al prossimo avvio.~Conserva PUBG2019Profile.ini quando cambi PC.|Creare un nuovo ID? L’ID attuale sarà sostituito e i futuri dati online associati potrebbero non essere trasferiti.|Errore profilo|Copia non riuscita|AVVIA SERVER|UNISCITI AL SERVER|UNISCITI AL MIO SERVER|ARRESTA SERVER|Avvia un server locale|Unisciti automaticamente|PRONTO|AVVIO DEL SERVER|AVVIO DEL GIOCO|CONNESSIONE AL SERVER|ARRESTO DEL SERVER|IN ATTESA DI GIOCATORI|SERVER ARRESTATO|GIOCO AVVIATO|AVVIO NON RIUSCITO|Inserisci l’indirizzo del server a cui unirti.|Mappa sperimentale: la disponibilità dipende dal gioco installato.|Scegli la mappa e il numero di bot per la partita locale.|Nome giocatore non valido.|Indirizzo del server non valido.|Un server locale è già in esecuzione da questa cartella.|MAPPA|INDIRIZZO SERVER|nuova|classica|ERRORE|Lingua del gioco richiesta|Nome giocatore richiesto|Caricamento del terreno del server|Terreno del server pronto|Modulo caricato|Codice di uscita del processo|salta dall’aereo|Avvio di {0} con {1} bot|Connessione a {0}").Split('|') },
        { "pl", ("PROFIL|Wyjdź|Profil gracza|Nazwa gracza i unikalny identyfikator|NAZWA GRACZA|ID GRACZA|JĘZYK GRY|Użyj ustawień gry|KOPIUJ ID|UTWÓRZ NOWE ID|ZAPISZ|Język zmieni się po następnym uruchomieniu gry.~Przy zmianie komputera zachowaj PUBG2019Profile.ini.|Utworzyć nowe ID gracza? Obecne ID zostanie zastąpione, a przyszłe dane online mogą nie zostać przeniesione.|Błąd profilu|Nie udało się skopiować|URUCHOM SERWER|DOŁĄCZ DO SERWERA|DOŁĄCZ DO MOJEGO SERWERA|ZATRZYMAJ SERWER|Uruchom lokalny serwer|Dołącz automatycznie|GOTOWE|URUCHAMIANIE SERWERA|URUCHAMIANIE GRY|ŁĄCZENIE Z SERWEREM|ZATRZYMYWANIE SERWERA|OCZEKIWANIE NA GRACZY|SERWER ZATRZYMANY|GRA URUCHOMIONA|BŁĄD URUCHAMIANIA|Wpisz adres serwera, do którego chcesz dołączyć.|Mapa eksperymentalna. Dostępność zależy od zainstalowanej gry.|Wybierz mapę i liczbę botów do lokalnej rozgrywki.|Nieprawidłowa nazwa gracza.|Nieprawidłowy adres serwera.|Z tego folderu gry działa już lokalny serwer.|MAPA|ADRES SERWERA|nowa|klasyczna|BŁĄD|Wybrany język gry|Wybrana nazwa gracza|Ładowanie terenu serwera|Teren serwera gotowy|Moduł załadowany|Kod zakończenia procesu|wyskakuje z samolotu|Uruchamianie {0}, liczba botów: {1}|Łączenie z {0}").Split('|') },
        { "pt", ("PERFIL|Sair|Perfil do jogador|O teu nome e identificador único|NOME DO JOGADOR|ID DO JOGADOR|IDIOMA DO JOGO|Usar definição do jogo|COPIAR ID|CRIAR NOVO ID|GUARDAR|O idioma é aplicado na próxima vez que iniciares o jogo.~Guarda PUBG2019Profile.ini ao mudar de PC.|Criar um novo ID? O ID atual será substituído e os futuros dados online associados poderão não ser transferidos.|Erro do perfil|Falha ao copiar|INICIAR SERVIDOR|ENTRAR NO SERVIDOR|ENTRAR NO MEU SERVIDOR|PARAR SERVIDOR|Iniciar servidor local|Entrar automaticamente|PRONTO|A INICIAR SERVIDOR|A INICIAR O JOGO|A LIGAR AO SERVIDOR|A PARAR O SERVIDOR|À ESPERA DE JOGADORES|SERVIDOR PARADO|JOGO INICIADO|FALHA AO INICIAR|Introduz o endereço do servidor onde queres entrar.|Mapa experimental. A disponibilidade depende do jogo instalado.|Escolhe o mapa e o número de bots para a partida local.|Nome de jogador inválido.|Endereço do servidor inválido.|Já está a ser executado um servidor local a partir desta pasta.|MAPA|ENDEREÇO DO SERVIDOR|novo|clássico|ERRO|Idioma do jogo pedido|Nome do jogador pedido|A carregar o terreno do servidor|Terreno do servidor pronto|Módulo carregado|Código de saída do processo|salta do avião|A iniciar {0} com {1} bots|A ligar a {0}").Split('|') },
        { "pt-BR", ("PERFIL|Sair|Perfil do jogador|Seu nome e identificador único|NOME DO JOGADOR|ID DO JOGADOR|IDIOMA DO JOGO|Usar configuração do jogo|COPIAR ID|CRIAR NOVO ID|SALVAR|O idioma será aplicado na próxima vez que o jogo iniciar.~Guarde PUBG2019Profile.ini ao trocar de computador.|Criar um novo ID? O ID atual será substituído e futuros dados online associados talvez não sejam transferidos.|Erro no perfil|Falha ao copiar|INICIAR SERVIDOR|ENTRAR NO SERVIDOR|ENTRAR NO MEU SERVIDOR|PARAR SERVIDOR|Iniciar servidor local|Entrar automaticamente|PRONTO|INICIANDO SERVIDOR|INICIANDO JOGO|CONECTANDO AO SERVIDOR|PARANDO SERVIDOR|AGUARDANDO JOGADORES|SERVIDOR PARADO|JOGO INICIADO|FALHA AO INICIAR|Digite o endereço do servidor ao qual deseja entrar.|Mapa experimental. A disponibilidade depende do jogo instalado.|Escolha o mapa e a quantidade de bots para a partida local.|Nome de jogador inválido.|Endereço do servidor inválido.|Já existe um servidor local em execução nesta pasta do jogo.|MAPA|ENDEREÇO DO SERVIDOR|novo|clássico|ERRO|Idioma do jogo solicitado|Nome do jogador solicitado|Carregando terreno do servidor|Terreno do servidor pronto|Módulo carregado|Código de saída do processo|salta do avião|Iniciando {0} com {1} bots|Conectando a {0}").Split('|') },
        { "tr", ("PROFİL|Çıkış|Oyuncu profili|Oyuncu adın ve benzersiz kimliğin|OYUNCU ADI|OYUNCU KİMLİĞİ|OYUN DİLİ|Oyun ayarını kullan|KİMLİĞİ KOPYALA|YENİ KİMLİK OLUŞTUR|KAYDET|Dil, oyun bir sonraki kez başladığında uygulanır.~Bilgisayar değiştirirken PUBG2019Profile.ini dosyasını sakla.|Yeni oyuncu kimliği oluşturulsun mu? Mevcut kimlik değiştirilir ve gelecekte ilişkili çevrimiçi veriler aktarılmayabilir.|Profil hatası|Kopyalama başarısız|SUNUCU BAŞLAT|SUNUCUYA KATIL|SUNUCUMA KATIL|SUNUCUYU DURDUR|Yerel sunucu başlat|Otomatik katıl|HAZIR|SUNUCU BAŞLATILIYOR|OYUN BAŞLATILIYOR|SUNUCUYA BAĞLANILIYOR|SUNUCU DURDURULUYOR|OYUNCULAR BEKLENİYOR|SUNUCU DURDURULDU|OYUN BAŞLADI|BAŞLATMA BAŞARISIZ|Katılmak istediğin sunucunun adresini gir.|Deneysel harita. Kullanılabilirlik kurulu oyuna bağlıdır.|Yerel maç için haritayı ve bot sayısını seç.|Geçersiz oyuncu adı.|Geçersiz sunucu adresi.|Bu oyun klasöründen zaten bir yerel sunucu çalışıyor.|HARİTA|SUNUCU ADRESİ|yeni|klasik|HATA|Seçilen oyun dili|Seçilen oyuncu adı|Sunucu arazisi yükleniyor|Sunucu arazisi hazır|Modül yüklendi|İşlem çıkış kodu|uçaktan atlıyor|{0}, {1} bot ile başlatılıyor|{0} adresine bağlanılıyor").Split('|') },
        { "ru", ("ПРОФИЛЬ|Выход|Профиль игрока|Ваше имя и уникальный идентификатор|ИМЯ ИГРОКА|ID ИГРОКА|ЯЗЫК ИГРЫ|Использовать настройки игры|КОПИРОВАТЬ ID|СОЗДАТЬ НОВЫЙ ID|СОХРАНИТЬ|Язык изменится при следующем запуске игры.~При переносе на другой ПК сохраните PUBG2019Profile.ini.|Создать новый ID? Текущий ID будет заменён, а будущие сетевые данные могут не перенестись.|Ошибка профиля|Не удалось скопировать|ЗАПУСТИТЬ СЕРВЕР|ПОДКЛЮЧИТЬСЯ|ПОДКЛЮЧИТЬСЯ К МОЕМУ СЕРВЕРУ|ОСТАНОВИТЬ СЕРВЕР|Запустить локальный сервер|Подключаться автоматически|ГОТОВО|ЗАПУСК СЕРВЕРА|ЗАПУСК ИГРЫ|ПОДКЛЮЧЕНИЕ К СЕРВЕРУ|ОСТАНОВКА СЕРВЕРА|ОЖИДАНИЕ ИГРОКОВ|СЕРВЕР ОСТАНОВЛЕН|ИГРА ЗАПУЩЕНА|ОШИБКА ЗАПУСКА|Введите адрес сервера для подключения.|Экспериментальная карта. Доступность зависит от установленной игры.|Выберите карту и число ботов для локального матча.|Недопустимое имя игрока.|Недопустимый адрес сервера.|Локальный сервер из этой папки уже запущен.|КАРТА|АДРЕС СЕРВЕРА|новая|классическая|ОШИБКА|Выбранный язык игры|Выбранное имя игрока|Загрузка местности сервера|Местность сервера готова|Модуль загружен|Код завершения процесса|выпрыгивает из самолёта|Запуск {0}, ботов: {1}|Подключение к {0}").Split('|') },
        { "uk", ("ПРОФІЛЬ|Вийти|Профіль гравця|Ваше ім’я та унікальний ідентифікатор|ІМ’Я ГРАВЦЯ|ID ГРАВЦЯ|МОВА ГРИ|Використати налаштування гри|КОПІЮВАТИ ID|СТВОРИТИ НОВИЙ ID|ЗБЕРЕГТИ|Мова застосовується після наступного запуску гри.~При перенесенні на інший ПК збережіть PUBG2019Profile.ini.|Створити новий ID? Поточний ID буде замінено, а майбутні пов’язані онлайн-дані можуть не перенестися.|Помилка профілю|Не вдалося скопіювати|ЗАПУСТИТИ СЕРВЕР|ПРИЄДНАТИСЯ ДО СЕРВЕРА|ПРИЄДНАТИСЯ ДО МОГО СЕРВЕРА|ЗУПИНИТИ СЕРВЕР|Запустити локальний сервер|Приєднуватися автоматично|ГОТОВО|ЗАПУСК СЕРВЕРА|ЗАПУСК ГРИ|ПІДКЛЮЧЕННЯ ДО СЕРВЕРА|ЗУПИНКА СЕРВЕРА|ОЧІКУВАННЯ ГРАВЦІВ|СЕРВЕР ЗУПИНЕНО|ГРУ ЗАПУЩЕНО|ПОМИЛКА ЗАПУСКУ|Введіть адресу сервера для приєднання.|Експериментальна мапа. Доступність залежить від установленої гри.|Виберіть мапу та кількість ботів для локального матчу.|Неприпустиме ім’я гравця.|Неприпустима адреса сервера.|Локальний сервер із цієї теки вже запущено.|МАПА|АДРЕСА СЕРВЕРА|нова|класична|ПОМИЛКА|Вибрана мова гри|Вибране ім’я гравця|Завантаження місцевості сервера|Місцевість сервера готова|Модуль завантажено|Код завершення процесу|вистрибує з літака|Запуск {0}, ботів: {1}|Підключення до {0}").Split('|') },
        { "ja", ("プロフィール|終了|プレイヤープロフィール|プレイヤー名と固有 ID|プレイヤー名|プレイヤー ID|ゲーム言語|ゲーム設定を使用|ID をコピー|新しい ID を作成|保存|言語は次回のゲーム起動時に適用されます。~PC を移行する際は PUBG2019Profile.ini を保存してください。|新しいプレイヤー ID を作成しますか？現在の ID は置き換えられ、今後関連付けられるオンラインデータは移行されない場合があります。|プロフィールエラー|コピーできませんでした|サーバーを起動|サーバーに参加|自分のサーバーに参加|サーバーを停止|ローカルサーバーを起動|自動的に参加|準備完了|サーバーを起動中|ゲームを起動中|サーバーに接続中|サーバーを停止中|プレイヤーを待機中|サーバー停止|ゲーム起動完了|起動失敗|参加するサーバーのアドレスを入力してください。|実験的なマップです。利用可能かどうかはインストール済みのゲームによって異なります。|ローカルマッチのマップとボット数を選択してください。|プレイヤー名が無効です。|サーバーアドレスが無効です。|このゲームフォルダーからローカルサーバーが既に起動しています。|マップ|サーバーアドレス|新|クラシック|エラー|選択したゲーム言語|選択したプレイヤー名|サーバー地形を読み込み中|サーバー地形の準備完了|モジュールを読み込みました|プロセス終了コード|飛行機から降下しました|{0} を起動中、ボット {1} 人|{0} に接続中").Split('|') },
        { "th", ("โปรไฟล์|ออก|โปรไฟล์ผู้เล่น|ชื่อผู้เล่นและ ID เฉพาะของคุณ|ชื่อผู้เล่น|ID ผู้เล่น|ภาษาเกม|ใช้การตั้งค่าของเกม|คัดลอก ID|สร้าง ID ใหม่|บันทึก|ภาษาจะมีผลเมื่อเริ่มเกมครั้งถัดไป~เก็บ PUBG2019Profile.ini ไว้เมื่อย้ายเครื่อง|สร้าง ID ผู้เล่นใหม่หรือไม่? ID ปัจจุบันจะถูกแทนที่ และข้อมูลออนไลน์ที่ผูกไว้ในอนาคตอาจไม่ถูกโอน|ข้อผิดพลาดโปรไฟล์|คัดลอกไม่สำเร็จ|เริ่มเซิร์ฟเวอร์|เข้าร่วมเซิร์ฟเวอร์|เข้าร่วมเซิร์ฟเวอร์ของฉัน|หยุดเซิร์ฟเวอร์|เริ่มเซิร์ฟเวอร์ภายในเครื่อง|เข้าร่วมอัตโนมัติ|พร้อม|กำลังเริ่มเซิร์ฟเวอร์|กำลังเริ่มเกม|กำลังเชื่อมต่อเซิร์ฟเวอร์|กำลังหยุดเซิร์ฟเวอร์|กำลังรอผู้เล่น|หยุดเซิร์ฟเวอร์แล้ว|เริ่มเกมแล้ว|เริ่มไม่สำเร็จ|ป้อนที่อยู่เซิร์ฟเวอร์ที่ต้องการเข้าร่วม|แผนที่ทดลอง ความพร้อมใช้งานขึ้นอยู่กับเกมที่ติดตั้ง|เลือกแผนที่และจำนวนบอตสำหรับแมตช์ภายในเครื่อง|ชื่อผู้เล่นไม่ถูกต้อง|ที่อยู่เซิร์ฟเวอร์ไม่ถูกต้อง|มีเซิร์ฟเวอร์ภายในเครื่องทำงานจากโฟลเดอร์เกมนี้แล้ว|แผนที่|ที่อยู่เซิร์ฟเวอร์|ใหม่|คลาสสิก|ข้อผิดพลาด|ภาษาเกมที่เลือก|ชื่อผู้เล่นที่เลือก|กำลังโหลดภูมิประเทศเซิร์ฟเวอร์|ภูมิประเทศเซิร์ฟเวอร์พร้อมแล้ว|โหลดโมดูลแล้ว|รหัสการสิ้นสุดโปรเซส|กระโดดออกจากเครื่องบิน|กำลังเริ่ม {0} พร้อมบอต {1} ตัว|กำลังเชื่อมต่อ {0}").Split('|') },
        { "vi", ("HỒ SƠ|Thoát|Hồ sơ người chơi|Tên và ID duy nhất của bạn|TÊN NGƯỜI CHƠI|ID NGƯỜI CHƠI|NGÔN NGỮ TRÒ CHƠI|Dùng cài đặt trò chơi|SAO CHÉP ID|TẠO ID MỚI|LƯU|Ngôn ngữ có hiệu lực ở lần khởi động trò chơi tiếp theo.~Hãy giữ PUBG2019Profile.ini khi chuyển sang máy khác.|Tạo ID người chơi mới? ID hiện tại sẽ bị thay thế và dữ liệu trực tuyến liên kết sau này có thể không được chuyển.|Lỗi hồ sơ|Không thể sao chép|KHỞI ĐỘNG MÁY CHỦ|THAM GIA MÁY CHỦ|VÀO MÁY CHỦ CỦA TÔI|DỪNG MÁY CHỦ|Khởi động máy chủ cục bộ|Tự động tham gia|SẴN SÀNG|ĐANG KHỞI ĐỘNG MÁY CHỦ|ĐANG KHỞI ĐỘNG TRÒ CHƠI|ĐANG KẾT NỐI MÁY CHỦ|ĐANG DỪNG MÁY CHỦ|ĐANG CHỜ NGƯỜI CHƠI|MÁY CHỦ ĐÃ DỪNG|TRÒ CHƠI ĐÃ KHỞI ĐỘNG|KHỞI ĐỘNG THẤT BẠI|Nhập địa chỉ máy chủ muốn tham gia.|Bản đồ thử nghiệm; tính khả dụng tùy thuộc vào trò chơi đã cài.|Chọn bản đồ và số bot cho trận đấu cục bộ.|Tên người chơi không hợp lệ.|Địa chỉ máy chủ không hợp lệ.|Đã có máy chủ cục bộ chạy từ thư mục trò chơi này.|BẢN ĐỒ|ĐỊA CHỈ MÁY CHỦ|mới|cổ điển|LỖI|Ngôn ngữ trò chơi đã chọn|Tên người chơi đã chọn|Đang tải địa hình máy chủ|Địa hình máy chủ đã sẵn sàng|Đã tải mô-đun|Mã kết thúc tiến trình|nhảy khỏi máy bay|Đang khởi động {0} với {1} bot|Đang kết nối tới {0}").Split('|') },
        { "id", ("PROFIL|Keluar|Profil pemain|Nama dan ID unik Anda|NAMA PEMAIN|ID PEMAIN|BAHASA GAME|Gunakan pengaturan game|SALIN ID|BUAT ID BARU|SIMPAN|Bahasa berlaku saat game dimulai kembali.~Simpan PUBG2019Profile.ini saat pindah PC.|Buat ID pemain baru? ID saat ini akan diganti dan data daring terkait di masa depan mungkin tidak dipindahkan.|Kesalahan profil|Gagal menyalin|MULAI SERVER|GABUNG SERVER|GABUNG SERVER SAYA|HENTIKAN SERVER|Mulai server lokal|Gabung otomatis|SIAP|MEMULAI SERVER|MEMULAI GAME|MENGHUBUNGKAN KE SERVER|MENGHENTIKAN SERVER|MENUNGGU PEMAIN|SERVER DIHENTIKAN|GAME DIMULAI|GAGAL MEMULAI|Masukkan alamat server yang ingin Anda ikuti.|Peta eksperimental. Ketersediaan bergantung pada game yang terpasang.|Pilih peta dan jumlah bot untuk pertandingan lokal.|Nama pemain tidak valid.|Alamat server tidak valid.|Server lokal dari folder game ini sudah berjalan.|PETA|ALAMAT SERVER|baru|klasik|KESALAHAN|Bahasa game yang dipilih|Nama pemain yang dipilih|Memuat medan server|Medan server siap|Modul dimuat|Kode keluar proses|melompat dari pesawat|Memulai {0} dengan {1} bot|Menghubungkan ke {0}").Split('|') },
        { "ga-IE", ("PRÓIFÍL|Scoir|Próifíl an imreora|D’ainm agus d’aitheantas uathúil|AINM AN IMREORA|AITHEANTAS AN IMREORA|TEANGA AN CHLUICHE|Úsáid socrú an chluiche|CÓIPEÁIL ID|CRUTHAIGH ID NUA|SÁBHÁIL|Cuirfear an teanga i bhfeidhm nuair a thosófar an cluiche arís.~Coinnigh PUBG2019Profile.ini agus tú ag bogadh go ríomhaire eile.|Aitheantas nua a chruthú? Cuirfear an t-aitheantas reatha ina ionad agus seans nach n-aistreofar sonraí ar líne a bheidh nasctha leis amach anseo.|Earráid phróifíle|Theip ar an gcóipeáil|TOSAIGH FREASTALAÍ|GLAC PÁIRT SA FHREASTALAÍ|GLAC PÁIRT I MO FHREASTALAÍ|STOP AN FREASTALAÍ|Tosaigh freastalaí áitiúil|Glac páirt go huathoibríoch|RÉIDH|AN FREASTALAÍ Á THOSÚ|AN CLUICHE Á THOSÚ|AG CEANGAL LEIS AN bhFREASTALAÍ|AN FREASTALAÍ Á STOPADH|AG FANACHT AR IMREOIRÍ|FREASTALAÍ STOPTHA|CLUICHE TOSAITHE|THEIP AR AN TOSÚ|Cuir isteach seoladh an fhreastalaí ar mhaith leat páirt a ghlacadh ann.|Léarscáil thurgnamhach; braitheann a hinfhaighteacht ar an gcluiche suiteáilte.|Roghnaigh léarscáil agus líon na mbotanna don chluiche áitiúil.|Ainm imreora neamhbhailí.|Seoladh freastalaí neamhbhailí.|Tá freastalaí áitiúil ón bhfillteán cluiche seo ag rith cheana féin.|LÉARSCÁIL|SEOLADH AN FHREASTALAÍ|nua|clasaiceach|EARRÁID|Teanga roghnaithe an chluiche|Ainm roghnaithe an imreora|Tír-raon an fhreastalaí á luchtú|Tír-raon an fhreastalaí réidh|Modúl luchtaithe|Cód scoir an phróisis|léimeann sé ón eitleán|{0} á thosú le {1} bot|Ag ceangal le {0}").Split('|') },
    };

    private static readonly Dictionary<string, string> Aliases = new Dictionary<string, string> {
        { "PROFILE", "profile" }, { "Exit", "exit" }, { "Player profile", "profile_title" }, { "PLAYER PROFILE", "profile_title" },
        { "Your player name and unique ID", "subtitle" }, { "PLAYER NAME", "player_name" }, { "PLAYER ID", "player_id" }, { "GAME LANGUAGE", "game_language" },
        { "Use game setting", "game_default" }, { "COPY ID", "copy_id" }, { "GENERATE NEW ID", "new_id" }, { "Generate a new ID", "new_id" }, { "SAVE", "save" },
        { "Language applies when the client next starts.\nKeep PUBG2019Profile.ini when moving to another PC.", "info" },
        { "Generate a new player ID? Your current ID will be replaced. Any future online data linked to it may not transfer.", "renew_warning" },
        { "Profile error", "profile_error" }, { "Copy failed", "copy_error" },
        { "HOST SERVER", "host" }, { "JOIN SERVER", "join" }, { "JOIN MY SERVER", "join_my" }, { "STOP SERVER", "stop" },
        { "Start a local server", "local" }, { "Join automatically", "auto" }, { "READY", "ready" },
        { "STARTING SERVER — WAITING FOR MAP AND BOTS", "starting" }, { "LAUNCHING CLIENT", "launching" },
        { "SERVER READY — CONNECTING CLIENT", "connecting" }, { "STOPPING SERVER", "stopping" },
        { "SERVER READY — WAITING FOR PLAYERS", "waiting" }, { "SERVER STOPPED", "stopped" },
        { "CLIENT STARTED — SERVER IS RUNNING", "started" }, { "CLIENT LAUNCH REQUEST FINISHED", "started" },
        { "START FAILED", "failed" }, { "CLIENT COULD NOT START — TRY JOINING AGAIN", "failed" },
        { "Enter the address of the server you want to join.", "address_hint" },
        { "Experimental map. Availability depends on the installed game.", "experimental" },
        { "Choose a map and bot count for your local match.", "map_hint" },
        { "Invalid player name", "invalid_name" }, { "Invalid server address", "invalid_address" },
        { "A local server is already running from this game folder.", "already_running" },
        { "Connecting to your local server.", "connecting" }, { "Stopping the local server.", "stopping" },
        { "Server is still initializing; try STOP SERVER again in a moment.", "starting" },
        { "Server ready. Automatic joining is off; use JOIN MY SERVER whenever you are ready.", "waiting" },
        { "The host launcher exited before the server became ready. Check PUBG2019Launcher.log and PUBG2019PS_Server.log.", "failed" },
        { "Server startup timed out. Check the host log.", "failed" },
        { "Cannot identify the server started by this launcher.", "failed" }, { "The matching local server process is no longer available.", "stopped" },
        { "MAP", "map" }, { "SERVER ADDRESS", "address" }, { "new", "new" }, { "classic", "classic" }
    };

    private static readonly string[] DiagnosticKeys = { "BOT GOAL", "BOT FIRE", "BOT AIM", "BOT SHOT TRACE", "BOT ZONE BOOST", "BOT LOOT readiness", "SKY PLAN", "SKY FLIGHT", "SKY START", "PRIVATE GROUND", "PRIVATE JUMP", "PRIVATE ALT", "BOT SIGHT snapshot" };
    private static readonly Dictionary<string, string[]> DiagnosticRows = new Dictionary<string, string[]>(StringComparer.OrdinalIgnoreCase) {
        { "ko", "봇 목표|봇 사격|봇 조준|봇 사격 판정|봇 구역 가속|봇 아이템 준비|강하 계획|자유 낙하|강하 시작|착지|강하|고도|봇 시야 현황".Split('|') },
        { "zh-CN", "机器人目标|机器人开火|机器人瞄准|机器人射击检测|机器人毒圈加速|机器人拾取准备|跳伞计划|自由落体|开始跳伞|落地|跳伞|高度|机器人视野概况".Split('|') },
        { "zh-TW", "機器人目標|機器人開火|機器人瞄準|機器人射擊檢測|機器人毒圈加速|機器人拾取準備|跳傘計畫|自由落體|開始跳傘|落地|跳傘|高度|機器人視野概況".Split('|') },
        { "ar", "هدف الروبوت|إطلاق الروبوت|تصويب الروبوت|فحص طلقة الروبوت|تعزيز منطقة الروبوت|جاهزية غنائم الروبوت|خطة القفز|السقوط الحر|بدء القفز|الهبوط|القفز|الارتفاع|ملخص رؤية الروبوت".Split('|') },
        { "es", "OBJETIVO BOT|DISPARO BOT|PUNTERÍA BOT|TRAZADO DISPARO BOT|IMPULSO DE ZONA BOT|PREPARACIÓN BOTÍN BOT|PLAN DE SALTO|CAÍDA LIBRE|INICIO DE SALTO|ATERRIZAJE|SALTO|ALTURA|VISTA BOT".Split('|') },
        { "fr", "OBJECTIF BOT|TIR BOT|VISÉE BOT|TRAJECTOIRE DE TIR BOT|ACCÉLÉRATION ZONE BOT|BUTIN BOT PRÊT|PLAN DE SAUT|CHUTE LIBRE|DÉBUT DU SAUT|ATTERRISSAGE|SAUT|ALTITUDE|VUE BOT".Split('|') },
        { "it", "OBIETTIVO BOT|FUOCO BOT|MIRA BOT|TRAIETTORIA COLPO BOT|BOOST ZONA BOT|BOTTINO BOT PRONTO|PIANO DI LANCIO|CADUTA LIBERA|INIZIO LANCIO|ATTERRAGGIO|LANCIO|ALTITUDINE|VISUALE BOT".Split('|') },
        { "pl", "CEL BOTA|OGIEŃ BOTA|CELOWANIE BOTA|TOR STRZAŁU BOTA|PRZYSPIESZENIE BOTA W STREFIE|GOTOWOŚĆ ŁUPÓW BOTA|PLAN SKOKU|SPADANIE|POCZĄTEK SKOKU|LĄDOWANIE|SKOK|WYSOKOŚĆ|WIDZENIE BOTA".Split('|') },
        { "pt", "OBJETIVO DO BOT|DISPARO DO BOT|MIRA DO BOT|TRAJETÓRIA DO TIRO DO BOT|IMPULSO DE ZONA DO BOT|PILHAGEM DO BOT PRONTA|PLANO DE SALTO|QUEDA LIVRE|INÍCIO DO SALTO|ATERRAGEM|SALTO|ALTITUDE|VISÃO DO BOT".Split('|') },
        { "pt-BR", "OBJETIVO DO BOT|DISPARO DO BOT|MIRA DO BOT|TRAJETÓRIA DO TIRO DO BOT|IMPULSO DE ZONA DO BOT|SAQUE DO BOT PRONTO|PLANO DE SALTO|QUEDA LIVRE|INÍCIO DO SALTO|POUSO|SALTO|ALTITUDE|VISÃO DO BOT".Split('|') },
        { "tr", "BOT HEDEFİ|BOT ATEŞİ|BOT NİŞANI|BOT ATIŞ İZİ|BOT BÖLGE HIZI|BOT GANİMET HAZIRLIĞI|ATLAMA PLANI|SERBEST DÜŞÜŞ|ATLAMA BAŞLANGICI|İNİŞ|ATLAMA|İRTİFA|BOT GÖRÜŞÜ".Split('|') },
        { "ru", "ЦЕЛЬ БОТА|ОГОНЬ БОТА|ПРИЦЕЛИВАНИЕ БОТА|ТРАЕКТОРИЯ ВЫСТРЕЛА БОТА|УСКОРЕНИЕ БОТА В ЗОНЕ|ГОТОВНОСТЬ ДОБЫЧИ БОТА|ПЛАН ПРЫЖКА|СВОБОДНОЕ ПАДЕНИЕ|НАЧАЛО ПРЫЖКА|ПРИЗЕМЛЕНИЕ|ПРЫЖОК|ВЫСОТА|ОБЗОР БОТА".Split('|') },
        { "uk", "ЦІЛЬ БОТА|ВОГОНЬ БОТА|ПРИЦІЛЮВАННЯ БОТА|ТРАЄКТОРІЯ ПОСТРІЛУ БОТА|ПРИСКОРЕННЯ БОТА В ЗОНІ|ГОТОВНІСТЬ ЗДОБИЧІ БОТА|ПЛАН СТРИБКА|ВІЛЬНЕ ПАДІННЯ|ПОЧАТОК СТРИБКА|ПРИЗЕМЛЕННЯ|СТРИБОК|ВИСОТА|ОГЛЯД БОТА".Split('|') },
        { "ja", "ボット目標|ボット発砲|ボット照準|ボット弾道判定|ボットエリア加速|ボット物資準備|降下計画|自由落下|降下開始|着地|降下|高度|ボット視界".Split('|') },
        { "th", "เป้าหมายบอต|บอตยิง|บอตเล็ง|ตรวจวิถีกระสุนบอต|ความเร็วบอตในวง|ความพร้อมของไอเทมบอต|แผนกระโดด|ดิ่งพสุธา|เริ่มกระโดด|ลงพื้น|กระโดด|ความสูง|มุมมองบอต".Split('|') },
        { "vi", "MỤC TIÊU BOT|BOT BẮN|BOT NGẮM|ĐƯỜNG ĐẠN BOT|TĂNG TỐC VÙNG CỦA BOT|ĐỒ BOT SẴN SÀNG|KẾ HOẠCH NHẢY|RƠI TỰ DO|BẮT ĐẦU NHẢY|TIẾP ĐẤT|NHẢY|ĐỘ CAO|TẦM NHÌN BOT".Split('|') },
        { "id", "TUJUAN BOT|TEMBAKAN BOT|BIDIKAN BOT|JEJAK TEMBAKAN BOT|PERCEPATAN ZONA BOT|KESIAPAN BARANG BOT|RENCANA LOMPAT|TERJUN BEBAS|MULAI LOMPAT|MENDARAT|LOMPAT|KETINGGIAN|PANDANGAN BOT".Split('|') },
        { "ga-IE", "SPRIOC BHOIT|URCHAR BHOIT|AIDHMIÚ BHOIT|CONAIR URCHAIR BHOIT|LUAS CRIOSA BHOIT|ULLMHACHT EARDRÁIN BHOIT|PLEAN LÉIME|SAORTHITIM|TÚS NA LÉIME|TUIRLINGT|LÉIM|AIRDE|AMHARC BHOIT".Split('|') }
    };

    static LauncherTranslations()
    {
        // Mexican Spanish uses the same launcher wording as the Spanish row;
        // its distinct game culture code is retained for the game itself.
        Rows["es-MX"] = (string[])Rows["es"].Clone();
        Rows["es-MX"][7] = "Usar configuración del juego";
        Rows["es-MX"][11] = "El idioma se aplica al iniciar el juego de nuevo.~Conserva PUBG2019Profile.ini al cambiar de computadora.";
        DiagnosticRows["es-MX"] = (string[])DiagnosticRows["es"].Clone();
        foreach (KeyValuePair<string, string[]> row in Rows)
            if (row.Value.Length != Keys.Length) throw new InvalidOperationException("Invalid launcher translation table: " + row.Key);
        foreach (KeyValuePair<string, string[]> row in DiagnosticRows)
            if (row.Value.Length != DiagnosticKeys.Length) throw new InvalidOperationException("Invalid diagnostic translation table: " + row.Key);
    }

    public static string Get(string language, string key, string fallback)
    {
        string[] row;
        int index = Array.IndexOf(Keys, key);
        if (index < 0 || !Rows.TryGetValue(language ?? "", out row)) return fallback;
        return row[index].Replace("~", "\n");
    }

    private static readonly Dictionary<string, string> ServerRunning = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase) {
        {"ko", "서버 실행 중"}, {"zh-CN", "服务器运行中"}, {"zh-TW", "伺服器運行中"},
        {"ar", "الخادم قيد التشغيل"}, {"es", "SERVIDOR EN MARCHA"}, {"es-MX", "SERVIDOR EN MARCHA"},
        {"fr", "SERVEUR EN COURS"}, {"it", "SERVER IN ESECUZIONE"}, {"pl", "SERWER DZIAŁA"},
        {"pt", "SERVIDOR EM EXECUÇÃO"}, {"pt-BR", "SERVIDOR EM EXECUÇÃO"}, {"tr", "SUNUCU ÇALIŞIYOR"},
        {"ru", "СЕРВЕР РАБОТАЕТ"}, {"uk", "СЕРВЕР ПРАЦЮЄ"}, {"ja", "サーバー稼働中"},
        {"th", "เซิร์ฟเวอร์กำลังทำงาน"}, {"vi", "MÁY CHỦ ĐANG CHẠY"}, {"id", "SERVER BERJALAN"},
        {"ga-IE", "FREASTALAÍ AG RITH"}
    };

    public static string Text(string language, string text)
    {
        string running;
        if (text == "SERVER RUNNING" && ServerRunning.TryGetValue(language ?? "", out running)) return running;
        string key;
        if (Aliases.TryGetValue(text, out key)) return Get(language, key, text);
        if (text.StartsWith("Enter 1–24 English letters", StringComparison.Ordinal)) return Get(language, "invalid_name", text) + "\n1–24: A–Z, 0–9, _, -";
        if (text.StartsWith("Enter a hostname or IPv4", StringComparison.Ordinal)) return Get(language, "invalid_address", text) + "\n192.168.1.10:8888";
        return text;
    }

    public static bool HasLanguage(string language) { return Rows.ContainsKey(language ?? ""); }

    public static string BotLabel(string language)
    {
        switch (language) {
            case "ko": return "봇 (0–95)";
            case "zh-CN": return "机器人 (0–95)";
            case "zh-TW": return "機器人 (0–95)";
            case "ar": return "الروبوتات (0–95)";
            case "pl": return "BOTY (0–95)";
            case "ru": return "БОТЫ (0–95)";
            case "uk": return "БОТИ (0–95)";
            case "ja": return "ボット (0–95)";
            case "th": return "บอต (0–95)";
            case "ga-IE": return "BOTANNA (0–95)";
            default: return "BOTS (0–95)";
        }
    }

    public static string TranslateDiagnostics(string language, string message)
    {
        string[] row;
        if (!DiagnosticRows.TryGetValue(language ?? "", out row)) return message;
        for (int i = 0; i < DiagnosticKeys.Length; i++) message = message.Replace(DiagnosticKeys[i], row[i]);
        return message;
    }
}
