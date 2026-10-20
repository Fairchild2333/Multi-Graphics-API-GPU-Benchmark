# Read-only: list DXGI adapters with their LUIDs and output count.
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
public struct DXGI_ADAPTER_DESC1 {
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)] public string Description;
    public uint VendorId, DeviceId, SubSysId, Revision;
    public UIntPtr DedicatedVideoMemory, DedicatedSystemMemory, SharedSystemMemory;
    public uint LuidLow; public int LuidHigh; public uint Flags;
}
[ComImport, Guid("29038f61-3839-4626-91fd-086879011a05"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IDXGIAdapter1 {
    void SetPrivateData(); void SetPrivateDataInterface(); void GetPrivateData(); void GetParent();
    [PreserveSig] int EnumOutputs(uint index, out IntPtr output);
    void GetDesc(); void CheckInterfaceSupport();
    [PreserveSig] int GetDesc1(out DXGI_ADAPTER_DESC1 desc);
}
[ComImport, Guid("770aae78-f26f-4dba-a829-253c83d1b387"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IDXGIFactory1 {
    void SetPrivateData(); void SetPrivateDataInterface(); void GetPrivateData(); void GetParent();
    void EnumAdapters(); void MakeWindowAssociation(); void GetWindowAssociation(); void CreateSwapChain(); void CreateSoftwareAdapter();
    [PreserveSig] int EnumAdapters1(uint index, out IDXGIAdapter1 adapter);
    [PreserveSig] bool IsCurrent();
}
public static class Dxgi {
    [DllImport("dxgi.dll")] static extern int CreateDXGIFactory1(ref Guid riid, [MarshalAs(UnmanagedType.Interface)] out IDXGIFactory1 factory);
    public static string List() {
        var g = new Guid("770aae78-f26f-4dba-a829-253c83d1b387");
        IDXGIFactory1 f; int hr = CreateDXGIFactory1(ref g, out f);
        if (hr != 0) return "CreateDXGIFactory1 failed " + hr;
        var sb = new System.Text.StringBuilder();
        for (uint i = 0; ; ++i) {
            IDXGIAdapter1 a; if (f.EnumAdapters1(i, out a) != 0) break;
            DXGI_ADAPTER_DESC1 d; a.GetDesc1(out d);
            int outputs = 0; IntPtr o;
            while (a.EnumOutputs((uint)outputs, out o) == 0) { Marshal.Release(o); ++outputs; }
            sb.AppendFormat("[{0}] {1} | vendor 0x{2:x4} device 0x{3:x4} | LUID {4:x8}-{5:x8} | VRAM {6} MB | outputs {7} | flags {8}\n",
                i, d.Description, d.VendorId, d.DeviceId, d.LuidHigh, d.LuidLow,
                (ulong)d.DedicatedVideoMemory / (1024 * 1024), outputs, d.Flags);
        }
        return sb.ToString();
    }
}
"@
[Dxgi]::List()
