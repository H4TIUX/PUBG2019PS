using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Windows.Forms;

internal static class WardrobeStarter
{
    [STAThread]
    private static int Main()
    {
        bool created;
        using (var mutex = new Mutex(true, @"Local\PUBG2019CharacterWardrobe", out created))
        {
            if (!created) { MessageBox.Show("The character editor is already open.", "Character editor"); return 1; }
            try
            {
                string bin = AppDomain.CurrentDomain.BaseDirectory.TrimEnd(Path.DirectorySeparatorChar);
                string script = Path.Combine(bin, "CharacterWardrobe", "EngineWardrobe.ps1");
                if (!File.Exists(script) || !File.Exists(Path.Combine(bin, "TslGame.exe")))
                    throw new FileNotFoundException("Place the character editor next to TslGame.exe with its CharacterWardrobe folder.");
                var start = new ProcessStartInfo {
                    FileName = Path.Combine(Environment.SystemDirectory, @"WindowsPowerShell\v1.0\powershell.exe"),
                    Arguments = "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" + script + "\"",
                    WorkingDirectory = bin, UseShellExecute = false, CreateNoWindow = true
                };
                using (var process = Process.Start(start)) { process.WaitForExit(); return process.ExitCode; }
            }
            catch (Exception e) { MessageBox.Show(e.Message, "Character editor", MessageBoxButtons.OK, MessageBoxIcon.Error); return 1; }
        }
    }
}
