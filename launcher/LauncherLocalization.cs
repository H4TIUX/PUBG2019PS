using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;

internal static class LauncherLocalization
{
    public static string WindowsLanguage(string cultureName)
    {
        string culture = (cultureName ?? "").Replace('_', '-');
        if (culture.Equals("zh", StringComparison.OrdinalIgnoreCase) || culture.StartsWith("zh-", StringComparison.OrdinalIgnoreCase))
            return culture.IndexOf("Hant", StringComparison.OrdinalIgnoreCase) >= 0 ||
                culture.IndexOf("-TW", StringComparison.OrdinalIgnoreCase) >= 0 ||
                culture.IndexOf("-HK", StringComparison.OrdinalIgnoreCase) >= 0 ||
                culture.IndexOf("-MO", StringComparison.OrdinalIgnoreCase) >= 0 ? "zh-TW" : "zh-CN";
        if (culture.Equals("es-MX", StringComparison.OrdinalIgnoreCase) || culture.StartsWith("es-MX-", StringComparison.OrdinalIgnoreCase)) return "es-MX";
        if (culture.Equals("pt-BR", StringComparison.OrdinalIgnoreCase) || culture.StartsWith("pt-BR-", StringComparison.OrdinalIgnoreCase)) return "pt-BR";
        int dash = culture.IndexOf('-');
        string language = dash >= 0 ? culture.Substring(0, dash) : culture;
        if (language.Equals("ga", StringComparison.OrdinalIgnoreCase)) return "ga-IE";
        foreach (string code in PlayerProfile.GameLanguages)
            if (code.Equals(language, StringComparison.OrdinalIgnoreCase)) return code;
        return "en";
    }
    private static readonly Dictionary<string, string> German = new Dictionary<string, string> {
        {"PROFILE", "PROFIL"}, {"Exit", "Beenden"}, {"Player profile", "Spielerprofil"}, {"MAP", "KARTE"}, {"SERVER ADDRESS", "SERVERADRESSE"},
        {"PLAYER PROFILE", "SPIELERPROFIL"}, {"Your player name and unique ID", "Dein Spielername und deine eindeutige ID"},
        {"PLAYER NAME", "SPIELERNAME"}, {"PLAYER ID", "SPIELER-ID"}, {"GAME LANGUAGE", "SPIELSPRACHE"},
        {"Use game setting", "Spieleinstellung verwenden"}, {"COPY ID", "ID KOPIEREN"},
        {"GENERATE NEW ID", "NEUE ID ERSTELLEN"}, {"SAVE", "SPEICHERN"},
        {"Language applies when the client next starts.\nKeep PUBG2019Profile.ini when moving to another PC.", "Die Sprache gilt ab dem nächsten Spielstart.\nBei einem PC-Wechsel PUBG2019Profile.ini mitnehmen."},
        {"Generate a new player ID? Your current ID will be replaced. Any future online data linked to it may not transfer.", "Neue Spieler-ID erstellen? Deine bisherige ID wird ersetzt. Künftig damit verknüpfte Online-Daten werden möglicherweise nicht übertragen."},
        {"Generate a new ID", "Neue ID erstellen"}, {"Profile error", "Profilfehler"}, {"Copy failed", "Kopieren fehlgeschlagen"},
        {"HOST SERVER", "SERVER STARTEN"}, {"JOIN SERVER", "SERVER BEITRETEN"}, {"JOIN MY SERVER", "EIGENEM SERVER BEITRETEN"},
        {"STOP SERVER", "SERVER STOPPEN"}, {"Start a local server", "Lokalen Server starten"}, {"Join automatically", "Automatisch beitreten"},
        {"READY", "BEREIT"}, {"STARTING SERVER — WAITING FOR MAP AND BOTS", "SERVER STARTET – KARTE UND BOTS"},
        {"LAUNCHING CLIENT", "SPIEL WIRD GESTARTET"}, {"SERVER READY — CONNECTING CLIENT", "SERVER BEREIT – SPIEL VERBINDET"},
        {"STOPPING SERVER", "SERVER WIRD GESTOPPT"}, {"SERVER READY — WAITING FOR PLAYERS", "SERVER BEREIT – WARTE AUF SPIELER"},
        {"SERVER STOPPED", "SERVER GESTOPPT"}, {"SERVER RUNNING", "SERVER LÄUFT"}, {"CLIENT STARTED — SERVER IS RUNNING", "SPIEL GESTARTET – SERVER LÄUFT"},
        {"CLIENT LAUNCH REQUEST FINISHED", "SPIELSTART ABGESCHLOSSEN"}, {"START FAILED", "START FEHLGESCHLAGEN"},
        {"CLIENT COULD NOT START — TRY JOINING AGAIN", "SPIELSTART FEHLGESCHLAGEN"},
        {"Enter the address of the server you want to join.", "Adresse des Servers eingeben, dem du beitreten möchtest."},
        {"Experimental map. Availability depends on the installed game.", "Experimentelle Karte. Verfügbarkeit hängt vom installierten Spiel ab."},
        {"Choose a map and bot count for your local match.", "Wähle eine Karte und die Anzahl der Bots für dein lokales Spiel."},
        {"Enter 1–24 English letters, numbers, or underscores for the player name.", "Der Spielername muss aus 1–24 englischen Buchstaben, Zahlen oder Unterstrichen bestehen."},
        {"Invalid player name", "Ungültiger Spielername"}, {"Invalid server address", "Ungültige Serveradresse"},
        {"Enter a hostname or IPv4 address, optionally followed by :port (for example 192.168.1.10:8888).", "Gib einen Hostnamen oder eine IPv4-Adresse ein, optional mit :Port (z. B. 192.168.1.10:8888)."},
        {"A local server is already running from this game folder.", "Aus diesem Spielordner läuft bereits ein lokaler Server."},
        {"Connecting to your local server.", "Verbindung zum lokalen Server wird hergestellt."},
        {"Stopping the local server.", "Lokaler Server wird gestoppt."},
        {"Server is still initializing; try STOP SERVER again in a moment.", "Server wird noch initialisiert; versuche SERVER STOPPEN gleich erneut."},
        {"Server ready. Automatic joining is off; use JOIN MY SERVER whenever you are ready.", "Server bereit. Automatischer Beitritt ist aus; wähle EIGENEM SERVER BEITRETEN."},
        {"The host launcher exited before the server became ready. Check PUBG2019Launcher.log and PUBG2019PS_Server.log.", "Der Host wurde beendet, bevor der Server bereit war. Prüfe PUBG2019Launcher.log und PUBG2019PS_Server.log."},
        {"Server startup timed out. Check the host log.", "Zeitüberschreitung beim Serverstart. Prüfe das Host-Protokoll."},
        {"Cannot identify the server started by this launcher.", "Der von diesem Launcher gestartete Server konnte nicht identifiziert werden."},
        {"The matching local server process is no longer available.", "Der zugehörige lokale Serverprozess ist nicht mehr verfügbar."}
    };

    public static string Text(bool german, string text)
    {
        string value;
        return german && German.TryGetValue(text, out value) ? value : text;
    }

    public static string Text(string language, string text)
    {
        string permission = LauncherPermissionText.Format(language, text);
        if (permission != text) return permission;
        if (String.Equals(language, "de", StringComparison.OrdinalIgnoreCase)) return Text(true, text);
        return LauncherTranslations.Text(language, text);
    }

    public static string Log(string language, string message)
    {
        string permission = LauncherPermissionText.Format(language, message);
        if (permission != message) return permission;
        if (String.Equals(language, "de", StringComparison.OrdinalIgnoreCase)) return Log(true, message);
        if (!LauncherTranslations.HasLanguage(language)) return message;
        string translated = Text(language, message);
        if (translated != message) return translated;
        Match start = Regex.Match(message, @"^Starting (.+) with (\d+) bots\.$");
        if (start.Success) return String.Format(LauncherTranslations.Get(language, "starting_log", message), start.Groups[1].Value, start.Groups[2].Value);
        Match join = Regex.Match(message, @"^Joining (.+)\.$");
        if (join.Success) return String.Format(LauncherTranslations.Get(language, "joining_log", message), join.Groups[1].Value);
        string[] originals = { "Requested game language:", "Requested player name:", "Server terrain loading:", "Server terrain ready:", "Successfully injected module!", "process exited with code", "jumps out of the plane", "ERROR:" };
        string[] keys = { "requested_language", "requested_name", "terrain_loading", "terrain_ready", "module_loaded", "process_exited", "bot_jump", "error" };
        for (int i = 0; i < originals.Length; i++)
            if (translated.Contains(originals[i])) translated = translated.Replace(originals[i], LauncherTranslations.Get(language, keys[i], originals[i]) + (originals[i].EndsWith(":") ? ":" : ""));
        return LauncherTranslations.TranslateDiagnostics(language, translated);
    }

    // Display-only translations. The original diagnostic log remains unchanged.
    public static string Log(bool german, string message)
    {
        if (!german) return message;
        string value = Text(true, message);
        if (value != message) return value;
        var phrases = new[] {
            new[] { "JOIN ERROR:", "BEITRITTSFEHLER:" }, new[] { "STOP ERROR:", "STOPPFEHLER:" }, new[] { "ERROR:", "FEHLER:" },
            new[] { "Server startup failed:", "Serverstart fehlgeschlagen:" }, new[] { "Client launch failed:", "Spielstart fehlgeschlagen:" },
            new[] { "Missing required file:", "Erforderliche Datei fehlt:" },
            new[] { "The client join script failed with exit code", "Das Beitrittsskript wurde beendet mit Code" },
            new[] { "Check PUBG2019Launcher.log.", "Prüfe PUBG2019Launcher.log." },
            new[] { "Requested game language:", "Gewählte Spielsprache:" },
            new[] { "(game settings and client startup)", "(Spieleinstellungen und Spielstart)" },
            new[] { "Requested player name:", "Gewählter Spielername:" },
            new[] { "Waiting for the local server to finish starting...", "Warte, bis der lokale Server gestartet ist..." },
            new[] { "Player name request enabled. If the name is not shown, check PUBG2019ClientName.log.", "Spielername aktiviert. Falls er nicht angezeigt wird, prüfe PUBG2019ClientName.log." },
            new[] { "Successfully injected module!", "Modul erfolgreich geladen!" },
            new[] { "Native match accounting ready: native FinishMatch/EndMatch guarded; result notifications preserved", "Spielstatus bereit: FinishMatch/EndMatch geschützt; Ergebnismeldungen bleiben erhalten" },
            new[] { "Server terrain loading:", "Servergelände wird geladen:" }, new[] { "detailed, LOD selections reset=", "Detailpakete, LOD-Auswahl zurückgesetzt=" },
            new[] { "Server terrain ready:", "Servergelände bereit:" }, new[] { "original heightmap packages loaded.", "ursprüngliche Höhenkartenpakete geladen." },
            new[] { "Match startup ready: native airborne preparer, aircraft=", "Spielstart bereit: Flugvorbereitung, Flugzeug=" },
            new[] { "countdown=60 s; waiting for a real player.", "Countdown=60 s; warte auf einen Spieler." },
            new[] { "Keep this window open while the server runs.", "Dieses Fenster offen lassen, solange der Server läuft." },
            new[] { "Spawn ready:", "Startpunkt bereit:" }, new[] { "Lobby:", "Lobby:" },
            new[] { "bots joining now so they board the plane", "Bots treten bei und steigen ins Flugzeug" },
            new[] { "jumps out of the plane", "springt aus dem Flugzeug" },
            new[] { "Lobby empty - countdown held at 60 s until a player joins", "Lobby leer – Countdown bleibt bei 60 s, bis ein Spieler beitritt" },
            new[] { "server PID", "Server-PID" }, new[] { "waiting for map loading.", "Karte wird geladen." },
            new[] { "server exited:", "Server beendet:" }, new[] { "process exited with code", "Prozess beendet mit Code" },
            new[] { "STARTUP", "START" }, new[] { "BOT LOOT readiness", "BOT-BEUTE Bereitschaft" },
            new[] { "SKY PLAN", "SPRUNGPLAN" }, new[] { "SKY START", "SPRUNGSTART" },
            new[] { "SKY FLIGHT", "FREIFLUG" }, new[] { "PRIVATE JUMP", "SPRUNG" }, new[] { "PRIVATE ALT", "HÖHE" },
            new[] { "BOT GOAL", "BOT-ZIEL" }, new[] { "rotate-zone", "zur Zone" }, new[] { "zone-vehicle", "Fahrzeug zur Zone" },
            new[] { "PRIVATE GROUND", "LANDUNG" }, new[] { "BOT AIM", "BOT-ZIELEN" },
            new[] { "BOT ZONE BOOST", "BOT-ZONENBONUS" }, new[] { "BOT ZONE EMERGENCY", "BOT-ZONENNOTFALL" },
            new[] { "BOT SHOT TRACE", "BOT-SCHUSSPRÜFUNG" }, new[] { "BOT FIRE", "BOT-FEUER" },
            new[] { "BOT SIGHT snapshot", "BOT-SICHT Übersicht" },
            new[] { "BOT DAMAGE missing weapon trajectory data", "BOT-SCHADEN: Waffentrajektorie fehlt" },
            new[] { "player name assigned:", "Spielername zugewiesen:" }, new[] { "Decrypted team:", "Entschlüsseltes Team:" },
            new[] { "registered with the game as team", "im Spiel registriert als Team" },
            new[] { "death cause:", "Todesursache:" }, new[] { "got shot by", "wurde getroffen von" },
            new[] { " - fighting back", " – erwidert das Feuer" },
            new[] { "Client exited before the name helper loaded.", "Spiel beendet, bevor die Namenshilfe geladen wurde." },
            new[] { "Client exited before the name helper loaded", "Spiel beendet, bevor die Namenshilfe geladen wurde" },
            new[] { "server was stopped", "Server wurde gestoppt" }, new[] { " is still initializing", " wird noch initialisiert" },
            new[] { "Server exited while loading; code", "Server beim Laden beendet; Code" },
            new[] { "Server exited during initialization; code", "Server bei der Initialisierung beendet; Code" },
            new[] { "Runtime: humans=", "Laufzeit: Spieler=" }, new[] { "bots=", "Bots=" }, new[] { "teams=", "Teams=" },
            new[] { "state=", "Zustand=" }, new[] { "target=", "Ziel=" }, new[] { "distance=", "Abstand=" },
            new[] { "weapon=", "Waffe=" }, new[] { "weaponIndex=", "Waffenindex=" }, new[] { "mode=", "Modus=" },
            new[] { "rounds=", "Schüsse=" }, new[] { "deviation=", "Abweichung=" }, new[] { "velocity=", "Geschwindigkeit=" },
            new[] { "above_ground=", "über_Boden=" }, new[] { "deploy_at=", "Öffnungshöhe=" },
            new[] { "items=", "Objekte=" }, new[] { "sites=", "Orte=" }, new[] { "characters=", "Charaktere=" },
            new[] { "humans=", "Spieler=" }, new[] { "enabled", "aktiviert" }, new[] { "disabled", "deaktiviert" }
        };
        foreach (string[] phrase in phrases) value = value.Replace(phrase[0], phrase[1]);
        value = Regex.Replace(value, @"^Starting (.+) with (\d+) bots\.$", "Starte $1 mit $2 Bots.");
        value = Regex.Replace(value, @"^Joining (.+)\.$", "Verbinde mit $1.");
        return value;
    }
}
