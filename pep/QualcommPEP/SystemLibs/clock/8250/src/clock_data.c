/* SM8250 GCC data generated from Linux drivers/clk/qcom/gcc-sm8250.c (tools gen_gcc.py).
 * RCG domains are READ_ONLY (rate left as UEFI set it) except the non-DFS UFS/USB/PCIe-aux/SDCC2 domains in WRITABLE. */
#include "gcc.h"

const gcc_freq_t gcc_freqs[] = {
    {25000000u, 6u, 24u, 0u, 0u, 255u, 1u}, /* gcc_ufs_phy_axi_clk_src P_GPLL0_OUT_EVEN */
    {37500000u, 6u, 16u, 0u, 0u, 255u, 1u}, /* gcc_ufs_phy_axi_clk_src P_GPLL0_OUT_EVEN */
    {75000000u, 6u, 8u, 0u, 0u, 255u, 1u}, /* gcc_ufs_phy_axi_clk_src P_GPLL0_OUT_EVEN */
    {150000000u, 1u, 8u, 0u, 0u, 255u, 1u}, /* gcc_ufs_phy_axi_clk_src P_GPLL0_OUT_MAIN */
    {300000000u, 1u, 4u, 0u, 0u, 255u, 1u}, /* gcc_ufs_phy_axi_clk_src P_GPLL0_OUT_MAIN */
    {33333333u, 6u, 18u, 0u, 0u, 255u, 1u}, /* gcc_usb30_prim_master_clk_src P_GPLL0_OUT_EVEN */
    {66666667u, 6u, 9u, 0u, 0u, 255u, 1u}, /* gcc_usb30_prim_master_clk_src P_GPLL0_OUT_EVEN */
    {133333333u, 1u, 9u, 0u, 0u, 255u, 1u}, /* gcc_usb30_prim_master_clk_src P_GPLL0_OUT_MAIN */
    {200000000u, 1u, 6u, 0u, 0u, 255u, 1u}, /* gcc_usb30_prim_master_clk_src P_GPLL0_OUT_MAIN */
    {240000000u, 1u, 5u, 0u, 0u, 255u, 1u}, /* gcc_usb30_prim_master_clk_src P_GPLL0_OUT_MAIN */
    {9600000u, 0u, 4u, 0u, 0u, 255u, 0u}, /* gcc_pcie_0_aux_clk_src P_BI_TCXO */
    {19200000u, 0u, 2u, 0u, 0u, 255u, 0u}, /* gcc_pcie_0_aux_clk_src P_BI_TCXO */
    {37500000u, 6u, 16u, 0u, 0u, 255u, 1u}, /* gcc_ufs_phy_ice_core_clk_src P_GPLL0_OUT_EVEN */
    {75000000u, 6u, 8u, 0u, 0u, 255u, 1u}, /* gcc_ufs_phy_ice_core_clk_src P_GPLL0_OUT_EVEN */
    {150000000u, 1u, 8u, 0u, 0u, 255u, 1u}, /* gcc_ufs_phy_ice_core_clk_src P_GPLL0_OUT_MAIN */
    {300000000u, 1u, 4u, 0u, 0u, 255u, 1u}, /* gcc_ufs_phy_ice_core_clk_src P_GPLL0_OUT_MAIN */
    {9600000u, 0u, 4u, 0u, 0u, 255u, 0u}, /* gcc_ufs_phy_phy_aux_clk_src P_BI_TCXO */
    {19200000u, 0u, 2u, 0u, 0u, 255u, 0u}, /* gcc_ufs_phy_phy_aux_clk_src P_BI_TCXO */
    {37500000u, 6u, 16u, 0u, 0u, 255u, 1u}, /* gcc_ufs_phy_unipro_core_clk_src P_GPLL0_OUT_EVEN */
    {75000000u, 6u, 8u, 0u, 0u, 255u, 1u}, /* gcc_ufs_phy_unipro_core_clk_src P_GPLL0_OUT_EVEN */
    {150000000u, 1u, 8u, 0u, 0u, 255u, 1u}, /* gcc_ufs_phy_unipro_core_clk_src P_GPLL0_OUT_MAIN */
    {300000000u, 1u, 4u, 0u, 0u, 255u, 1u}, /* gcc_ufs_phy_unipro_core_clk_src P_GPLL0_OUT_MAIN */
    {19200000u, 0u, 2u, 0u, 0u, 255u, 0u}, /* gcc_usb3_prim_phy_aux_clk_src P_BI_TCXO */
    {33333333u, 6u, 18u, 0u, 0u, 255u, 1u}, /* gcc_usb30_sec_master_clk_src P_GPLL0_OUT_EVEN */
    {66666667u, 6u, 9u, 0u, 0u, 255u, 1u}, /* gcc_usb30_sec_master_clk_src P_GPLL0_OUT_EVEN */
    {133333333u, 1u, 9u, 0u, 0u, 255u, 1u}, /* gcc_usb30_sec_master_clk_src P_GPLL0_OUT_MAIN */
    {200000000u, 1u, 6u, 0u, 0u, 255u, 1u}, /* gcc_usb30_sec_master_clk_src P_GPLL0_OUT_MAIN */
    {240000000u, 1u, 5u, 0u, 0u, 255u, 1u}, /* gcc_usb30_sec_master_clk_src P_GPLL0_OUT_MAIN */
    {9600000u, 0u, 4u, 0u, 0u, 255u, 0u}, /* gcc_pcie_1_aux_clk_src P_BI_TCXO */
    {19200000u, 0u, 2u, 0u, 0u, 255u, 0u}, /* gcc_pcie_1_aux_clk_src P_BI_TCXO */
    {19200000u, 0u, 2u, 0u, 0u, 255u, 0u}, /* gcc_usb3_sec_phy_aux_clk_src P_BI_TCXO */
};
const unsigned int gcc_freq_cnt = 31u;

const gcc_domain_t gcc_domains[] = {
    {"gcc_ufs_phy_axi_clk_src", 0x77024u, 8u, 0u, 5u, 0u, 0u},
    {"gcc_usb30_prim_master_clk_src", 0xf020u, 8u, 5u, 5u, 0u, 0u},
    {"gcc_gp1_clk_src", 0x64004u, 8u, 0u, 0u, GCC_DOMAIN_READ_ONLY, 0u},
    {"gcc_gp2_clk_src", 0x65004u, 8u, 0u, 0u, GCC_DOMAIN_READ_ONLY, 0u},
    {"gcc_gp3_clk_src", 0x66004u, 8u, 0u, 0u, GCC_DOMAIN_READ_ONLY, 0u},
    {"gcc_pcie_0_aux_clk_src", 0x6b038u, 16u, 10u, 2u, 0u, 0u},
    {"gcc_pdm2_clk_src", 0x33010u, 0u, 0u, 0u, GCC_DOMAIN_READ_ONLY, 0u},
    {"gcc_qupv3_wrap1_s0_clk_src", 0x18010u, 16u, 0u, 0u, GCC_DOMAIN_READ_ONLY, 0u},
    {"gcc_qupv3_wrap1_s1_clk_src", 0x18140u, 16u, 0u, 0u, GCC_DOMAIN_READ_ONLY, 0u},
    {"gcc_qupv3_wrap1_s2_clk_src", 0x18270u, 16u, 0u, 0u, GCC_DOMAIN_READ_ONLY, 0u},
    {"gcc_qupv3_wrap1_s3_clk_src", 0x183a0u, 16u, 0u, 0u, GCC_DOMAIN_READ_ONLY, 0u},
    {"gcc_qupv3_wrap1_s4_clk_src", 0x184d0u, 16u, 0u, 0u, GCC_DOMAIN_READ_ONLY, 0u},
    {"gcc_qupv3_wrap1_s5_clk_src", 0x18600u, 16u, 0u, 0u, GCC_DOMAIN_READ_ONLY, 0u},
    {"gcc_qupv3_wrap2_s0_clk_src", 0x1e010u, 16u, 0u, 0u, GCC_DOMAIN_READ_ONLY, 0u},
    {"gcc_qupv3_wrap2_s1_clk_src", 0x1e140u, 16u, 0u, 0u, GCC_DOMAIN_READ_ONLY, 0u},
    {"gcc_qupv3_wrap2_s2_clk_src", 0x1e270u, 16u, 0u, 0u, GCC_DOMAIN_READ_ONLY, 0u},
    {"gcc_qupv3_wrap2_s3_clk_src", 0x1e3a0u, 16u, 0u, 0u, GCC_DOMAIN_READ_ONLY, 0u},
    {"gcc_qupv3_wrap2_s4_clk_src", 0x1e4d0u, 16u, 0u, 0u, GCC_DOMAIN_READ_ONLY, 0u},
    {"gcc_qupv3_wrap2_s5_clk_src", 0x1e600u, 16u, 0u, 0u, GCC_DOMAIN_READ_ONLY, 0u},
    {"gcc_sdcc2_apps_clk_src", 0x1400cu, 8u, 0u, 0u, GCC_DOMAIN_READ_ONLY, 0u},
    {"gcc_ufs_phy_ice_core_clk_src", 0x7706cu, 0u, 12u, 4u, 0u, 0u},
    {"gcc_ufs_phy_phy_aux_clk_src", 0x770a0u, 0u, 16u, 2u, 0u, 0u},
    {"gcc_ufs_phy_unipro_core_clk_src", 0x77084u, 0u, 18u, 4u, 0u, 0u},
    {"gcc_usb3_prim_phy_aux_clk_src", 0xf064u, 0u, 22u, 1u, 0u, 0u},
    {"gcc_usb30_sec_master_clk_src", 0x10020u, 8u, 23u, 5u, 0u, 0u},
    {"gcc_pcie_phy_refgen_clk_src", 0x6f014u, 0u, 0u, 0u, GCC_DOMAIN_READ_ONLY, 0u},
    {"gcc_pcie_1_aux_clk_src", 0x8d038u, 16u, 28u, 2u, 0u, 0u},
    {"gcc_qupv3_wrap0_s3_clk_src", 0x173a0u, 16u, 0u, 0u, GCC_DOMAIN_READ_ONLY, 0u},
    {"gcc_qupv3_wrap0_s4_clk_src", 0x174d0u, 16u, 0u, 0u, GCC_DOMAIN_READ_ONLY, 0u},
    {"gcc_qupv3_wrap0_s7_clk_src", 0x17860u, 16u, 0u, 0u, GCC_DOMAIN_READ_ONLY, 0u},
    {"gcc_usb3_sec_phy_aux_clk_src", 0x10064u, 0u, 30u, 1u, 0u, 0u},
};
const unsigned int gcc_domain_cnt = 31u;

const gcc_clock_t gcc_clocks[] = {
    {"gcc_aggre_ufs_phy_axi_clk", GCC_KIND_BRANCH, 0x770CCu, 0x0u, 0x0u, 0, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_aggre_usb3_prim_axi_clk", GCC_KIND_BRANCH, 0xF080u, 0x0u, 0x0u, 1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_boot_rom_ahb_clk", GCC_KIND_VOTE, 0x38004u, 0x52000u, 0x400u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_camera_hf_axi_clk", GCC_KIND_BRANCH, 0xB02Cu, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_camera_sf_axi_clk", GCC_KIND_BRANCH, 0xB030u, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_camera_xo_clk", GCC_KIND_BRANCH, 0xB040u, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_cfg_noc_usb3_prim_axi_clk", GCC_KIND_BRANCH, 0xF07Cu, 0x0u, 0x0u, 1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_disp_hf_axi_clk", GCC_KIND_BRANCH, 0xB034u, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_disp_sf_axi_clk", GCC_KIND_BRANCH, 0xB038u, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_gp1_clk", GCC_KIND_BRANCH, 0x64000u, 0x0u, 0x0u, 2, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_gp2_clk", GCC_KIND_BRANCH, 0x65000u, 0x0u, 0x0u, 3, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_gp3_clk", GCC_KIND_BRANCH, 0x66000u, 0x0u, 0x0u, 4, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_pcie_0_aux_clk", GCC_KIND_VOTE, 0x6B028u, 0x52008u, 0x8u, 5, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_pcie_0_cfg_ahb_clk", GCC_KIND_VOTE, 0x6B024u, 0x52008u, 0x4u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_pcie_0_gdsc", GCC_KIND_GDSC, 0x0u, 0x0u, 0x0u, -1, 0x6b004u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_pcie_0_mstr_axi_clk", GCC_KIND_VOTE, 0x6B01Cu, 0x52008u, 0x2u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_pcie_0_pipe_clk", GCC_KIND_REQ, 0x6B02Cu, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_pcie_0_slv_axi_clk", GCC_KIND_VOTE, 0x6B014u, 0x52008u, 0x1u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_pcie_0_slv_q2a_axi_clk", GCC_KIND_VOTE, 0x6B010u, 0x52008u, 0x20u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_pdm2_clk", GCC_KIND_BRANCH, 0x3300Cu, 0x0u, 0x0u, 6, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_pdm_ahb_clk", GCC_KIND_BRANCH, 0x33004u, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_pdm_xo4_clk", GCC_KIND_BRANCH, 0x33008u, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qmip_camera_nrt_ahb_clk", GCC_KIND_BRANCH, 0xB018u, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qmip_camera_rt_ahb_clk", GCC_KIND_BRANCH, 0xB01Cu, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qmip_video_cvp_ahb_clk", GCC_KIND_BRANCH, 0xB010u, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qmip_video_vcodec_ahb_clk", GCC_KIND_BRANCH, 0xB014u, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qupv3_wrap1_core_2x_clk", GCC_KIND_VOTE, 0x23140u, 0x52008u, 0x40000u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qupv3_wrap1_core_clk", GCC_KIND_VOTE, 0x23138u, 0x52008u, 0x80000u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qupv3_wrap1_s0_clk", GCC_KIND_VOTE, 0x1800Cu, 0x52008u, 0x400000u, 7, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qupv3_wrap1_s1_clk", GCC_KIND_VOTE, 0x1813Cu, 0x52008u, 0x800000u, 8, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qupv3_wrap1_s2_clk", GCC_KIND_VOTE, 0x1826Cu, 0x52008u, 0x1000000u, 9, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qupv3_wrap1_s3_clk", GCC_KIND_VOTE, 0x1839Cu, 0x52008u, 0x2000000u, 10, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qupv3_wrap1_s4_clk", GCC_KIND_VOTE, 0x184CCu, 0x52008u, 0x4000000u, 11, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qupv3_wrap1_s5_clk", GCC_KIND_VOTE, 0x185FCu, 0x52008u, 0x8000000u, 12, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qupv3_wrap2_core_2x_clk", GCC_KIND_VOTE, 0x23278u, 0x52010u, 0x8u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qupv3_wrap2_core_clk", GCC_KIND_VOTE, 0x23270u, 0x52010u, 0x1u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qupv3_wrap2_s0_clk", GCC_KIND_VOTE, 0x1E00Cu, 0x52010u, 0x10u, 13, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qupv3_wrap2_s1_clk", GCC_KIND_VOTE, 0x1E13Cu, 0x52010u, 0x20u, 14, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qupv3_wrap2_s2_clk", GCC_KIND_VOTE, 0x1E26Cu, 0x52010u, 0x40u, 15, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qupv3_wrap2_s3_clk", GCC_KIND_VOTE, 0x1E39Cu, 0x52010u, 0x80u, 16, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qupv3_wrap2_s4_clk", GCC_KIND_VOTE, 0x1E4CCu, 0x52010u, 0x100u, 17, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qupv3_wrap2_s5_clk", GCC_KIND_VOTE, 0x1E5FCu, 0x52010u, 0x200u, 18, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qupv3_wrap_1_s_ahb_clk", GCC_KIND_VOTE, 0x18008u, 0x52008u, 0x200000u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qupv3_wrap_2_m_ahb_clk", GCC_KIND_VOTE, 0x1E004u, 0x52010u, 0x4u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qupv3_wrap_2_s_ahb_clk", GCC_KIND_VOTE, 0x1E008u, 0x52010u, 0x2u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_sdcc2_ahb_clk", GCC_KIND_BRANCH, 0x14008u, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_sdcc2_apps_clk", GCC_KIND_BRANCH, 0x14004u, 0x0u, 0x0u, 19, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_ufs_phy_ahb_clk", GCC_KIND_BRANCH, 0x77018u, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_ufs_phy_axi_clk", GCC_KIND_BRANCH, 0x77010u, 0x0u, 0x0u, 0, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_ufs_phy_gdsc", GCC_KIND_GDSC, 0x0u, 0x0u, 0x0u, -1, 0x77004u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_ufs_phy_ice_core_clk", GCC_KIND_BRANCH, 0x77064u, 0x0u, 0x0u, 20, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_ufs_phy_phy_aux_clk", GCC_KIND_BRANCH, 0x7709Cu, 0x0u, 0x0u, 21, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_ufs_phy_rx_symbol_0_clk", GCC_KIND_REQ, 0x77020u, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_ufs_phy_rx_symbol_1_clk", GCC_KIND_REQ, 0x770B8u, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_ufs_phy_tx_symbol_0_clk", GCC_KIND_REQ, 0x7701Cu, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_ufs_phy_unipro_core_clk", GCC_KIND_BRANCH, 0x7705Cu, 0x0u, 0x0u, 22, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_usb30_prim_gdsc", GCC_KIND_GDSC, 0x0u, 0x0u, 0x0u, -1, 0xf004u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_usb30_prim_master_clk", GCC_KIND_BRANCH, 0xF010u, 0x0u, 0x0u, 1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_usb30_prim_mock_utmi_clk", GCC_KIND_BRANCH, 0xF01Cu, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_usb30_prim_sleep_clk", GCC_KIND_BRANCH, 0xF018u, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_usb3_prim_phy_aux_clk", GCC_KIND_BRANCH, 0xF054u, 0x0u, 0x0u, 23, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_usb3_prim_phy_com_aux_clk", GCC_KIND_BRANCH, 0xF058u, 0x0u, 0x0u, 23, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_usb3_prim_phy_pipe_clk", GCC_KIND_REQ, 0xF05Cu, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_video_axi0_clk", GCC_KIND_BRANCH, 0xB024u, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_video_axi1_clk", GCC_KIND_BRANCH, 0xB028u, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_video_xo_clk", GCC_KIND_BRANCH, 0xB03Cu, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_aggre_noc_pcie_tbu_clk", GCC_KIND_BRANCH, 0x9000Cu, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_aggre_usb3_sec_axi_clk", GCC_KIND_BRANCH, 0x10080u, 0x0u, 0x0u, 24, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_cfg_noc_usb3_sec_axi_clk", GCC_KIND_BRANCH, 0x1007Cu, 0x0u, 0x0u, 24, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_ddrss_gpu_axi_clk", GCC_KIND_BRANCH, 0x71154u, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_disp_xo_clk", GCC_KIND_BRANCH, 0xB044u, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_gpu_memnoc_gfx_clk", GCC_KIND_BRANCH, 0x7100Cu, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_pcie0_phy_refgen_clk", GCC_KIND_BRANCH, 0x6F02Cu, 0x0u, 0x0u, 25, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_pcie1_phy_refgen_clk", GCC_KIND_BRANCH, 0x6F030u, 0x0u, 0x0u, 25, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_pcie_1_aux_clk", GCC_KIND_VOTE, 0x8D028u, 0x52000u, 0x20000000u, 26, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_pcie_1_cfg_ahb_clk", GCC_KIND_VOTE, 0x8D024u, 0x52000u, 0x10000000u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_pcie_1_mstr_axi_clk", GCC_KIND_VOTE, 0x8D01Cu, 0x52000u, 0x8000000u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_pcie_1_pipe_clk", GCC_KIND_REQ, 0x8D02Cu, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_pcie_1_slv_axi_clk", GCC_KIND_VOTE, 0x8D014u, 0x52000u, 0x4000000u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_pcie_1_slv_q2a_axi_clk", GCC_KIND_VOTE, 0x8D010u, 0x52000u, 0x2000000u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_pcie_phy_aux_clk", GCC_KIND_BRANCH, 0x6F004u, 0x0u, 0x0u, 5, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qupv3_wrap0_s3_clk", GCC_KIND_VOTE, 0x1739Cu, 0x52008u, 0x2000u, 27, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qupv3_wrap0_s4_clk", GCC_KIND_VOTE, 0x174CCu, 0x52008u, 0x4000u, 28, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qupv3_wrap0_s7_clk", GCC_KIND_VOTE, 0x1785Cu, 0x52008u, 0x20000u, 29, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qupv3_wrap_0_m_ahb_clk", GCC_KIND_VOTE, 0x17004u, 0x52008u, 0x40u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_qupv3_wrap_1_m_ahb_clk", GCC_KIND_VOTE, 0x18004u, 0x52008u, 0x100000u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_usb30_sec_master_clk", GCC_KIND_BRANCH, 0x10010u, 0x0u, 0x0u, 24, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_usb30_sec_mock_utmi_clk", GCC_KIND_BRANCH, 0x1001Cu, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_usb30_sec_sleep_clk", GCC_KIND_BRANCH, 0x10018u, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_usb3_sec_clkref_en", GCC_KIND_BRANCH, 0x8C010u, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_usb3_sec_phy_aux_clk", GCC_KIND_BRANCH, 0x10054u, 0x0u, 0x0u, 30, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_usb3_sec_phy_com_aux_clk", GCC_KIND_BRANCH, 0x10058u, 0x0u, 0x0u, 30, 0x0u, 0x0u, CLOCK_CTL_GCC},
    {"gcc_usb3_sec_phy_pipe_clk", GCC_KIND_REQ, 0x1005Cu, 0x0u, 0x0u, -1, 0x0u, 0x0u, CLOCK_CTL_GCC},
};
const unsigned int gcc_clock_cnt = sizeof(gcc_clocks) / sizeof(gcc_clocks[0]);

/* GPLL0/GPLL4 mode registers (read only) and APCS PLL vote (Linux gcc-sm8250.c). */
const unsigned int gcc_gpll0_mode_offset = 0x0u;
const unsigned int gcc_gpll4_mode_offset = 0x76000u;
const unsigned int gcc_apcs_pll_vote_offset = 0x52018u;

/* dropped (no SM8250 GCC equivalent or non-GCC controller): gcc_aggre_noc_pcie_axi_clk, gcc_cam_bist_mclk_ahb_clk, gcc_camera_ahb_clk, gcc_camera_hf_axi_cloned_clk, gcc_cfg_noc_pcie_anoc_ahb_clk, gcc_cnoc_pcie_sf_axi_clk, gcc_ddrss_pcie_sf_qtb_clk, gcc_disp_ahb_clk, gcc_eva_ahb_clk, gcc_eva_axi0_clk, gcc_eva_axi0c_clk, gcc_eva_xo_clk, gcc_gpll0_ufs_phy_rx_symbol_0_clk, gcc_gpll0_ufs_phy_rx_symbol_1_clk, gcc_gpll0_ufs_phy_tx_symbol_0_clk, gcc_gpu_cfg_ahb_clk, gcc_gpu_gemnoc_clk_en, gcc_gpu_gemnoc_gfx_clk, gcc_gpu_smmu_vote_clk, gcc_gpu_smmu_vote_gds, gcc_hlos1_vote_aggre_noc_mmu_pcie_qtb_clk, gcc_hlos1_vote_aggre_noc_mmu_pcie_qtb_gds, gcc_hlos1_vote_aggre_noc_mmu_qtb1_clk, gcc_hlos1_vote_aggre_noc_mmu_qtb1_gds, gcc_hlos1_vote_aggre_noc_mmu_qtb2_clk, gcc_hlos1_vote_aggre_noc_mmu_qtb2_gds, gcc_hlos1_vote_all_smmu_mmu_clk, gcc_hlos1_vote_all_smmu_mmu_gds, gcc_hlos1_vote_gpu_smmu_clk, gcc_hlos1_vote_gpu_smmu_gds, gcc_hlos1_vote_lpass_qtb_clk, gcc_hlos1_vote_lpass_qtb_gds, gcc_hlos1_vote_mmnoc_mmu_qtb_hf01_clk, gcc_hlos1_vote_mmnoc_mmu_qtb_hf01_gds, gcc_hlos1_vote_mmnoc_mmu_qtb_hf23_clk, gcc_hlos1_vote_mmnoc_mmu_qtb_hf23_gds, gcc_hlos1_vote_mmnoc_mmu_qtb_sf_clk, gcc_hlos1_vote_mmnoc_mmu_qtb_sf_gds, gcc_hlos1_vote_mmu_tcu_clk, gcc_hlos1_vote_mmu_tcu_gds, gcc_hlos1_vote_turing_mmu_qtb0_clk, gcc_hlos1_vote_turing_mmu_qtb0_gds, gcc_mmu_tcu_vote_clk, gcc_mmu_tcu_vote_gds, gcc_pcie_0_phy_aux_clk, gcc_pcie_0_phy_gdsc, gcc_pcie_0_phy_rchng_clk, gcc_pcie_0_pipe_div2_clk, gcc_pcie_rscc_ahb_clk, gcc_pcie_rscc_xo_clk, gcc_qmip_camera_cmd_ahb_clk, gcc_qmip_disp_dcp_sf_ahb_clk, gcc_qmip_gpu_ahb_clk, gcc_qmip_pcie_ahb_clk, gcc_qmip_video_cv_cpu_ahb_clk, gcc_qmip_video_v_cpu_ahb_clk, gcc_qupv3_i2c_core_clk, gcc_qupv3_i2c_s0_clk, gcc_qupv3_i2c_s1_clk, gcc_qupv3_i2c_s2_clk, gcc_qupv3_i2c_s3_clk, gcc_qupv3_i2c_s4_clk, gcc_qupv3_i2c_s_ahb_clk, gcc_qupv3_wrap1_qspi_ref_clk, gcc_qupv3_wrap1_s6_clk, gcc_qupv3_wrap1_s7_clk, gcc_qupv3_wrap2_s6_clk, gcc_qupv3_wrap2_s7_clk, gcc_qupv3_wrap3_core_2x_clk, gcc_qupv3_wrap3_core_clk, gcc_qupv3_wrap3_ibi_ctrl_1_clk, gcc_qupv3_wrap3_ibi_ctrl_2_clk, gcc_qupv3_wrap3_s0_clk, gcc_qupv3_wrap3_s1_clk, gcc_qupv3_wrap3_s2_clk, gcc_qupv3_wrap3_s3_clk, gcc_qupv3_wrap3_s4_clk, gcc_qupv3_wrap3_s5_clk, gcc_qupv3_wrap_1_m_axi_clk, gcc_qupv3_wrap_3_ibi_1_ahb_clk, gcc_qupv3_wrap_3_ibi_2_ahb_clk, gcc_qupv3_wrap_3_m_ahb_clk, gcc_qupv3_wrap_3_s_ahb_clk, gcc_sdcc2_at_clk, gcc_ufs_mem_phy_gdsc, gcc_usb3_phy_gdsc, gcc_video_ahb_clk, gpu_cc_gpu_smmu_vote_clk, gpu_cc_hlos1_vote_gpu_smmu_clk, disp_cc_debug_clk, disp_cc_mdss_spdm_debug_clk, disp_cc_sleep_clk, disp_cc_esync0_clk, disp_cc_esync1_clk, disp_cc_mdss_ahb1_clk, disp_cc_mdss_ahb_clk, disp_cc_mdss_ahb_swi_clk, disp_cc_mdss_rscc_ahb_clk, disp_cc_mdss_byte0_clk, disp_cc_mdss_byte0_intf_clk, disp_cc_mdss_byte1_clk, disp_cc_mdss_byte1_intf_clk, disp_cc_mdss_dptx0_aux_clk, disp_cc_mdss_dptx0_link_clk, disp_cc_mdss_dptx1_aux_clk, disp_cc_mdss_dptx1_link_clk, disp_cc_mdss_dptx2_aux_clk, disp_cc_mdss_dptx2_link_clk, disp_cc_mdss_dptx3_aux_clk, disp_cc_mdss_dptx3_link_clk, disp_cc_mdss_esc0_clk, disp_cc_mdss_esc1_clk, disp_cc_mdss_mdp1_clk, disp_cc_mdss_mdp_clk, disp_cc_mdss_mdp_lut1_clk, disp_cc_mdss_mdp_lut_clk, disp_cc_mdss_mdp_ss_ip_clk, disp_cc_mdss_spdm_mdp_clk, disp_cc_mdss_pclk0_clk, disp_cc_mdss_spdm_pclk0_clk, disp_cc_mdss_pclk1_clk, disp_cc_mdss_spdm_pclk1_clk, disp_cc_mdss_pclk2_clk, disp_cc_mdss_spdm_pclk2_clk, disp_cc_mdss_rscc_vsync_clk, disp_cc_mdss_vsync1_clk, disp_cc_mdss_vsync_clk, disp_cc_osc_clk, disp_cc_pll_test_clk, disp_cc_mdss_accu_shift_clk, disp_cc_xo_clk, tcsr_pcie_0_clkref_en, gcc_rx2_qlink_clkref_en, gcc_ufs_card_clkref_en, gcc_ufs_mem_clkref_en, gcc_usb3_prim_clkref_en, gcc_video_axic_clk */
