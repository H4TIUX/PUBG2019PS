using System.Windows.Forms;
// The shareable build deliberately has no update endpoint or verification key.
internal static class UpdateManager {
 public static void Start(LauncherForm owner,string dir,bool automatic) {
  if(!automatic)MessageBox.Show(owner,"Automatic updates are unavailable in this shareable build. Install verified updates manually.","Updates",MessageBoxButtons.OK,MessageBoxIcon.Information);
 }
}
