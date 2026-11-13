using System;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Reflection;

// Product V0.71 follows V0.7; encode the Windows minor as 71
// to preserve the existing sortable release ordering.
[assembly: AssemblyVersion("0.71.0.1")]
[assembly: AssemblyFileVersion("0.71.0.1")]
[assembly: AssemblyInformationalVersion("V0.71")]

internal static class ReleaseGuard
{
    internal const long ReleaseId = 4000000007100000001L;
    private const long VersionBase = 4000000000000000000L;

    internal static long Parse(string text)
    {
        long value;
        if (!Int64.TryParse((text ?? "").Trim(), NumberStyles.None,
            CultureInfo.InvariantCulture, out value) || value <= 0)
            throw new InvalidDataException("The release version is missing or invalid.");
        return value;
    }

    internal static long BinaryVersion(string path)
    {
        if (!File.Exists(path)) return 0;
        var v = FileVersionInfo.GetVersionInfo(path);
        if (String.IsNullOrEmpty(v.FileVersion)) return 0; // Legacy builds.
        int[] parts = { v.FileMajorPart, v.FileMinorPart, v.FileBuildPart, v.FilePrivatePart };
        foreach (int part in parts)
            if (part < 0 || part > 9999) throw new InvalidDataException("Invalid launcher file version.");
        return checked(VersionBase + parts[0] * 1000000000000L +
            parts[1] * 100000000L + parts[2] * 10000L + parts[3]);
    }

    internal static long InstalledVersion(string directory)
    {
        // The running build is authoritative even if an old ZIP overwrote or
        // removed the external marker. Never trust only release.version.
        long version = Math.Max(ReleaseId, BinaryVersion(Path.Combine(directory, "PUBG2019Launcher.exe")));
        string marker = Path.Combine(directory, "release.version");
        if (File.Exists(marker)) version = Math.Max(version, Parse(File.ReadAllText(marker)));
        return version;
    }

    internal static bool IsNewer(string directory, string stage)
    {
        long incoming = Parse(File.ReadAllText(Path.Combine(stage, "release.version")));
        if (incoming <= InstalledVersion(directory)) return false;
        // Future packages must identify the same build in the executable and
        // the signed marker. A mislabeled old binary must not become an update.
        if (BinaryVersion(Path.Combine(stage, "PUBG2019Launcher.exe")) != incoming)
            throw new InvalidDataException("The update version does not match the launcher executable.");
        if (BinaryVersion(Path.Combine(stage, "PUBG2019UpdateApply.exe")) != incoming)
            throw new InvalidDataException("The update version does not match the update helper.");
        return true;
    }

    internal static void RequireNewer(string directory, string stage)
    {
        if (!IsNewer(directory, stage))
            throw new InvalidOperationException("Update cancelled: this package is the same version or older than the installed launcher.");
    }
}
