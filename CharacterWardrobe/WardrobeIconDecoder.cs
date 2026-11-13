using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
public static class WardrobeIconDecoder
{
    static string Line(Stream s){var b=new StringBuilder();for(int i=0;i<1024;i++){int x=s.ReadByte();if(x<0)throw new InvalidDataException("Unexpected HDR end.");if(x==10)return b.ToString();if(x!=13)b.Append((char)x);}throw new InvalidDataException("HDR header too long.");}
    static int Read(Stream s){int v=s.ReadByte();if(v<0)throw new InvalidDataException("Truncated HDR image.");return v;}
    static readonly byte[,] lut=BuildLut();
    static byte[,] BuildLut(){var table=new byte[256,256];for(int e=1;e<256;e++){double factor=Math.Pow(2,e-136);for(int v=1;v<256;v++){double linear=Math.Min(1.0,v*factor);double srgb=linear<=0.0031308?12.92*linear:1.055*Math.Pow(linear,1.0/2.4)-0.055;table[e,v]=(byte)Math.Max(0,Math.Min(255,(int)(srgb*255+0.5)));}}return table;}
    public static void Convert(string source,string destination){
        using(var s=File.OpenRead(source)){
            if(s.Length>4*1024*1024 || Line(s)!="#?RADIANCE")throw new InvalidDataException("Unsupported image.");
            bool format=false;string line;while((line=Line(s)).Length!=0){if(line=="FORMAT=32-bit_rle_rgbe")format=true;}
            if(!format)throw new InvalidDataException("Unsupported HDR encoding.");
            var parts=Line(s).Split(new[]{' '},StringSplitOptions.RemoveEmptyEntries);int width,height;
            if(parts.Length!=4||parts[0]!="-Y"||parts[2]!="+X"||!int.TryParse(parts[1],NumberStyles.None,CultureInfo.InvariantCulture,out height)||!int.TryParse(parts[3],NumberStyles.None,CultureInfo.InvariantCulture,out width)||width<8||width>512||height<1||height>512)throw new InvalidDataException("Unsupported image size.");
            var values=new byte[width*4];var pixels=new byte[width*height*4];
            for(int y=0;y<height;y++){
                if(Read(s)!=2||Read(s)!=2||((Read(s)<<8)|Read(s))!=width)throw new InvalidDataException("Unsupported HDR scanline.");
                for(int c=0;c<4;c++){int x=0;while(x<width){int n=Read(s);if(n>128){n-=128;if(n==0||x+n>width)throw new InvalidDataException("Invalid HDR run.");byte v=(byte)Read(s);for(int i=0;i<n;i++)values[c*width+x++]=v;}else{if(n==0||x+n>width)throw new InvalidDataException("Invalid HDR run.");for(int i=0;i<n;i++)values[c*width+x++]=(byte)Read(s);}}}
                for(int x=0;x<width;x++){int e=values[3*width+x],p=(y*width+x)*4;pixels[p]=lut[e,values[2*width+x]];pixels[p+1]=lut[e,values[width+x]];pixels[p+2]=lut[e,values[x]];pixels[p+3]=255;}
            }
            using(var image=new Bitmap(width,height,PixelFormat.Format32bppArgb)){var data=image.LockBits(new Rectangle(0,0,width,height),ImageLockMode.WriteOnly,PixelFormat.Format32bppArgb);try{for(int y=0;y<height;y++)Marshal.Copy(pixels,y*width*4,IntPtr.Add(data.Scan0,y*data.Stride),width*4);}finally{image.UnlockBits(data);}image.Save(destination,ImageFormat.Png);}
        }
    }
}
