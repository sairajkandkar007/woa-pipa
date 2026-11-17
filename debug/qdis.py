import capstone,struct,pefile,sys
img=open('/tmp/qcpep.img','rb').read()
pe=pefile.PE('/mnt/user-data/uploads/win11-pipa/claude/kona_bsp/Drivers/SOC/PowerManagement/qcpep8250.sys')
imp={}
for e in pe.DIRECTORY_ENTRY_IMPORT:
  for i in e.imports: imp[i.address-pe.OPTIONAL_HEADER.ImageBase]=i.name.decode()
md=capstone.Cs(capstone.CS_ARCH_ARM64,capstone.CS_MODE_ARM); md.skipdata=True
a=int(sys.argv[1],16); b=int(sys.argv[2],16)
prev=None
for i in md.disasm(img[a:b],a):
  s=f"{i.address:#x}: {i.mnemonic} {i.op_str}"
  if prev is not None and prev.mnemonic=='adrp' and i.mnemonic in('ldr','add'):
    try:
      pg=int(prev.op_str.split('#')[1],16); off=int(i.op_str.split('#')[-1].rstrip(']'),16); t=pg+off
      if t in imp: s+='   ; '+imp[t]
      elif i.mnemonic=='add':
        st=img[t:t+40].split(b'\0')[0]
        if len(st)>3 and all(32<=c<127 for c in st): s+='   ; "'+st.decode()+'"'
    except: pass
  print(s); prev=i
