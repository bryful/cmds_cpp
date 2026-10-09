// Minimal stored RAR4 writer for deterministic test fixtures; no RAR encoder dependency.
using System;
using System.IO;
using System.Text;
public static class RarFixture {
    public static uint Crc(byte[] bytes) {
        uint crc=0xffffffff;
        foreach(byte b in bytes) { crc^=b; for(int i=0;i<8;i++) crc=(crc>>1)^((crc&1)!=0?0xedb88320u:0); }
        return ~crc;
    }
    static void Header(BinaryWriter output, byte type, ushort flags, byte[] fields) {
        using(var ms=new MemoryStream()) using(var w=new BinaryWriter(ms)) {
            w.Write(type); w.Write(flags); w.Write((ushort)(7+fields.Length)); w.Write(fields);
            byte[] header=ms.ToArray(); output.Write((ushort)Crc(header)); output.Write(header);
        }
    }
    public static void Write(string path, string[] names, string[] contents, bool badCrc=false) {
        using(var output=new BinaryWriter(File.Create(path))) {
            output.Write(new byte[]{82,97,114,33,26,7,0}); Header(output,0x73,0,new byte[6]);
            for(int i=0;i<names.Length;i++) {
                bool dir=names[i].EndsWith("/"); byte[] name=Encoding.ASCII.GetBytes(names[i].Replace('/','\\').TrimEnd('\\'));
                byte[] data=dir?new byte[0]:Encoding.UTF8.GetBytes(contents[i]);
                using(var ms=new MemoryStream()) using(var w=new BinaryWriter(ms)) {
                    w.Write((uint)data.Length); w.Write((uint)data.Length); w.Write((byte)2);
                    w.Write(Crc(data) ^ (badCrc && i==0?1u:0u)); w.Write(0u); w.Write((byte)20); w.Write((byte)0x30);
                    w.Write((ushort)name.Length); w.Write(dir?0x10u:0x20u); w.Write(name);
                    Header(output,0x74,(ushort)(0x8000|(dir?0xe0:0)),ms.ToArray());
                }
                output.Write(data);
            }
            Header(output,0x7b,0,new byte[0]);
        }
    }
}
