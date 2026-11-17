import sys,struct,pefile,bisect,capstone
img=open('/tmp/qcpep.img','rb').read()
pe=pefile.PE('/mnt/user-data/uploads/win11-pipa/claude/kona_bsp/Drivers/SOC/PowerManagement/qcpep8250.sys')
d=pe.OPTIONAL_HEADER.DATA_DIRECTORY[3]
st=sorted(set(struct.unpack('<I',img[d.VirtualAddress+i*8:d.VirtualAddress+i*8+4])[0] for i in range(d.Size//8)))
md=capstone.Cs(capstone.CS_ARCH_ARM64,capstone.CS_MODE_ARM); md.skipdata=True
for a in sys.argv[1:]:
    f=int(a,16); end=st[bisect.bisect_right(st,f)]
    c=[(hex(i.address),i.op_str) for i in md.disasm(img[f:end],f) if i.mnemonic in('bl','blr')]
    print(a,'end',hex(end),c)
