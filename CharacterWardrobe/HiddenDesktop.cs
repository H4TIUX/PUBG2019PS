using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
class HiddenDesktop {
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct STARTUPINFO {public int cb;public string reserved,desktop,title;public int x,y,xs,ys,xc,yc,attr,flags;public short show,cbReserved;public IntPtr reserved2,input,output,error;}
 [StructLayout(LayoutKind.Sequential)] struct PROCESS_INFORMATION {public IntPtr process,thread;public int pid,tid;}
 [DllImport("user32.dll",SetLastError=true,CharSet=CharSet.Unicode)] static extern IntPtr CreateDesktop(string name,string device,IntPtr devmode,int flags,uint access,IntPtr security);
 [DllImport("user32.dll",SetLastError=true)] static extern bool CloseDesktop(IntPtr desktop);
 [DllImport("kernel32.dll",SetLastError=true,CharSet=CharSet.Unicode)] static extern bool CreateProcess(string app,StringBuilder cmd,IntPtr pa,IntPtr ta,bool inherit,int flags,IntPtr env,string dir,ref STARTUPINFO si,out PROCESS_INFORMATION pi);
 [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
 [DllImport("kernel32.dll")] static extern uint WaitForSingleObject(IntPtr h,uint milliseconds);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetExitCodeProcess(IntPtr h,out uint code);
 static int Main(string[] args){if(args.Length!=3)return 2;string name="Pubg2019Headless_"+Guid.NewGuid().ToString("N");IntPtr desktop=CreateDesktop(name,null,IntPtr.Zero,0,0x000F01FF,IntPtr.Zero);if(desktop==IntPtr.Zero)throw new Win32Exception();
 try{var si=new STARTUPINFO{cb=Marshal.SizeOf(typeof(STARTUPINFO)),desktop=name};PROCESS_INFORMATION pi;string gameArgs=Encoding.UTF8.GetString(Convert.FromBase64String(args[2]));var command=new StringBuilder("\""+args[0]+"\" "+gameArgs);if(!CreateProcess(args[0],command,IntPtr.Zero,IntPtr.Zero,false,0,IntPtr.Zero,args[1],ref si,out pi))throw new Win32Exception();try{Console.WriteLine(pi.pid);Console.Out.Flush();WaitForSingleObject(pi.process,0xffffffff);uint code;if(GetExitCodeProcess(pi.process,out code)){Console.WriteLine("exit="+code);Console.Out.Flush();}}finally{CloseHandle(pi.thread);CloseHandle(pi.process);}return 0;}finally{CloseDesktop(desktop);}}
}
