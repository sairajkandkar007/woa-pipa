# usage: python3 gen_gcc.py <8845 clock_data.c> <out clock_data.c>  (needs /tmp/lx/gcc-sm8250.c from Linux)
import re,sys
lx=open('/tmp/lx/gcc-sm8250.c').read()
br={}
for m in re.finditer(r'static struct clk_branch (\w+) = \{(.*?)\n\};',lx,re.S):
    n,b=m.groups();g=lambda k:(re.search(k+r'\s*=\s*(0x[0-9a-fA-F]+|BIT\(\d+\)|\w+)',b) or [None,None])[1]
    par=re.search(r'parent_hws.*?&(\w+)\.clkr\.hw',b,re.S)
    br[n]=dict(halt=g(r'\.halt_reg'),chk=g(r'\.halt_check'),en=g(r'\.enable_reg'),mask=g(r'\.enable_mask'),par=par.group(1) if par else None)
rcg={}
for m in re.finditer(r'static struct clk_rcg2 (\w+) = \{(.*?)\n\};',lx,re.S):
    n,b=m.groups();rcg[n]=(re.search(r'cmd_rcgr = (0x\w+)',b).group(1),int((re.search(r'mnd_width = (\d+)',b) or [0,0])[1]))
gd={}
for m in re.finditer(r'static struct gdsc (\w+) = \{(.*?)\n\};',lx,re.S):
    gd[m.group(1)]=re.search(r'gdscr = (0x\w+)',m.group(2)).group(1)
src=open(sys.argv[1]).read()
a=src.index('const gcc_domain_t gcc_domains[]');b=src.index('const unsigned int gcc_clock_cnt')
old=re.findall(r'\{"(\w+)", (GCC_KIND_\w+),.*?(CLOCK_CTL_\w+)\}',src[a:b])
import os
_k=os.path.join(os.path.dirname(os.path.abspath(__file__)),'kona_dsdt_gcc_clocks.txt')
_seen={n for n,_,_ in old}
old+=[(n,'GCC_KIND_BRANCH','CLOCK_CTL_GCC') for n in open(_k).read().split() if n not in _seen]
doms=[];dix={};out=[];drop=[]
def bit(m):
    return 1<<int(re.match(r'BIT\((\d+)\)',m).group(1)) if m and m.startswith('BIT') else int(m,0) if m else 0
for name,kind,ctl in old:
    if kind=='GCC_KIND_GDSC':
        key=re.sub(r'_gdsc?$','',name[4:] if name.startswith('gcc_') else name);cand=[k for k in gd if re.sub(r'_gdsc$','',k)==key]
        if cand: out.append(f'    {{"{name}", GCC_KIND_GDSC, 0x0u, 0x0u, 0x0u, -1, {gd[cand[0]]}u, 0x0u, CLOCK_CTL_GCC}},')
        else: drop.append(name)
        continue
    if ctl!='CLOCK_CTL_GCC' or name not in br: drop.append(name); continue
    c=br[name];di=-1
    if c['par'] in rcg:
        if c['par'] not in dix:
            dix[c['par']]=len(doms);r=rcg[c['par']]
            doms.append(f'    {{"{c["par"]}", {r[0]}u, {r[1]}u, 0u, 0u, GCC_DOMAIN_READ_ONLY, 0u}},')
        di=dix[c['par']]
    voted=c['chk']=='BRANCH_HALT_VOTED'
    halt=int(c['halt'],0) if c['halt'] else 0
    en=int(c['en'],0) if c['en'] else 0
    if voted and en!=halt:
        out.append(f'    {{"{name}", GCC_KIND_VOTE, 0x{halt:X}u, 0x{en:X}u, 0x{bit(c["mask"]):X}u, {di}, 0x0u, 0x0u, CLOCK_CTL_GCC}},')
    else:
        k='GCC_KIND_REQ' if c['chk'] in ('BRANCH_HALT_SKIP','BRANCH_HALT_DELAY') else 'GCC_KIND_BRANCH'
        out.append(f'    {{"{name}", {k}, 0x{halt:X}u, 0x0u, 0x0u, {di}, 0x0u, 0x0u, CLOCK_CTL_GCC}},')
body=('/* SM8250 GCC data generated from Linux drivers/clk/qcom/gcc-sm8250.c (tools gen_gcc.py).\n'
 ' * All RCG domains are READ_ONLY: rates stay as UEFI left them; only branch enable/vote is used. */\n'
 '#include "gcc.h"\n\nconst gcc_freq_t gcc_freqs[] = {\n    {19200000u, 0u, 1u, 0u, 0u, 255u, 0u},\n};\nconst unsigned int gcc_freq_cnt = 1u;\n\n'
 'const gcc_domain_t gcc_domains[] = {\n'+'\n'.join(doms)+f'\n}};\nconst unsigned int gcc_domain_cnt = {len(doms)}u;\n\n'
 'const gcc_clock_t gcc_clocks[] = {\n'+'\n'.join(out)+'\n};\nconst unsigned int gcc_clock_cnt = sizeof(gcc_clocks) / sizeof(gcc_clocks[0]);\n\n'
 '/* GPLL0/GPLL4 mode registers (read only) and APCS PLL vote (Linux gcc-sm8250.c). */\n'
 'const unsigned int gcc_gpll0_mode_offset = 0x0u;\nconst unsigned int gcc_gpll4_mode_offset = 0x76000u;\nconst unsigned int gcc_apcs_pll_vote_offset = 0x52018u;\n\n'
 '/* dropped (no SM8250 GCC equivalent or non-GCC controller): '+', '.join(drop)+' */\n')
open(sys.argv[2],'w').write(body)
print(len(out),'clocks',len(doms),'domains',len(drop),'dropped')
