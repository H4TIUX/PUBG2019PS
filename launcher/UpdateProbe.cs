using System;
using System.IO;
using System.Net;
using System.Security.Cryptography;
using System.Windows.Forms;

internal static class UpdateProbe
{
    // The public verification key identifies this experimental build only.
    private const string PublicKey = "<RSAKeyValue><Modulus>qBPcPG2+/OQiqxw5wc1fY0fpqFxsbbb7IwL4rNtHD+j4eyt1NVXAN8u8fkdFf/x0RDGk9erlMqL3UOSa4RWAYqz72Ia3FpkOdE43FZcWeg4SAqCRjiLC+OfWHFR4q9LHuouV7RWcdQwqN9vXuSvfItRrHBrCl4zGnRghezGZJOsHBJtGA1bj1LjpdWXQsaBNKVcLa9gWNcDMWamd6osdowvlb1L+MUaAo+e3BCCSzPjzs7SixzEk/uN6tAJSKwpWswhhw8QDEBO8wGkYsc/lHwHElrMSIGH93yAQeBk+ZjVeuGegl1ADxPnQQK6EByNKvWziWTIR0GhVchmUINIXmw==</Modulus><Exponent>AQAB</Exponent></RSAKeyValue>";
    private static byte[] DownloadBounded(Uri address, int limit)
    {
        HttpWebRequest request = (HttpWebRequest)WebRequest.Create(address);
        request.Timeout = 15000; request.ReadWriteTimeout = 15000;
        request.AllowAutoRedirect = false;
        using (WebResponse response = request.GetResponse())
        using (Stream input = response.GetResponseStream())
        using (MemoryStream output = new MemoryStream())
        {
            byte[] buffer = new byte[16384]; int count;
            while ((count = input.Read(buffer, 0, buffer.Length)) > 0) {
                if (output.Length + count > limit) throw new InvalidDataException("Download exceeds the test size limit.");
                output.Write(buffer, 0, count);
            }
            return output.ToArray();
        }
    }
    public static void Check(IWin32Window owner, string folder)
    {
        try {
            string config = Path.Combine(folder, "UpdateUrl.txt");
            string text = File.ReadAllText(config).Trim(); Uri baseUri;
            if (!Uri.TryCreate(text, UriKind.Absolute, out baseUri) || baseUri.Scheme != Uri.UriSchemeHttps ||
                !String.IsNullOrEmpty(baseUri.UserInfo) || !String.IsNullOrEmpty(baseUri.Query) || !String.IsNullOrEmpty(baseUri.Fragment))
                throw new InvalidDataException("UpdateUrl.txt must contain an HTTPS base URL without credentials.");
            if (!baseUri.AbsoluteUri.EndsWith("/")) baseUri = new Uri(baseUri.AbsoluteUri + "/");
            ServicePointManager.SecurityProtocol |= SecurityProtocolType.Tls12;
            byte[] zip = DownloadBounded(new Uri(baseUri, "launcher.zip"), 10 * 1024 * 1024);
            byte[] sig = DownloadBounded(new Uri(baseUri, "launcher.sig"), 1024);
            using (var rsa = new RSACryptoServiceProvider()) {
                rsa.FromXmlString(PublicKey);
                if (!rsa.VerifyData(zip, CryptoConfig.MapNameToOID("SHA256"), sig))
                    throw new CryptographicException("The update signature is invalid.");
            }
            if (zip.Length < 4 || zip[0] != 0x50 || zip[1] != 0x4b || zip[2] != 0x03 || zip[3] != 0x04)
                throw new InvalidDataException("The signed file is not a ZIP archive.");
            MessageBox.Show(owner, "Test successful: the update package was downloaded and its signature verified. No files were changed.",
                "Update test", MessageBoxButtons.OK, MessageBoxIcon.Information);
        } catch (Exception ex) {
            MessageBox.Show(owner, "Update test failed: " + ex.Message, "Update test", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }
}
