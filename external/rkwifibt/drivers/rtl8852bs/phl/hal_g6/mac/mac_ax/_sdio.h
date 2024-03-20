/** @file */
/******************************************************************************
 *
 * Copyright(c) 2019 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 ******************************************************************************/

#ifndef _MAC_AX_SDIO_H_
#define _MAC_AX_SDIO_H_

#include "../type.h"
#include "pwr.h"

#if MAC_AX_SDIO_SUPPORT
/* SDIO CMD address mapping */
#define SDIO_8BYTE_LEN_MASK	0x0FFF
#define SDIO_4BYTE_LEN_MASK	0x03FF
#define SDIO_LOCAL_MSK		0x00000FFF
#define SDIO_LOCAL_SHIFT	0x00001000
#define	WLAN_IOREG_MSK		0xFFFE0000

/* IO Bus domain address mapping */
#define SDIO_LOCAL_BASE		0x80000000
#define WLAN_IOREG_BASE		0x00000000
#define SDIO_TX_BASE		0x00010000

#define SDIO_CMD_ADDR_TXFF_SHIFT	12
#define SDIO_CMD_ADDR_TXFF_0	0
#define SDIO_CMD_ADDR_TXFF_1	1
#define SDIO_CMD_ADDR_TXFF_2	2
#define SDIO_CMD_ADDR_TXFF_3	3
#define SDIO_CMD_ADDR_TXFF_4	4
#define SDIO_CMD_ADDR_TXFF_5	5
#define SDIO_CMD_ADDR_TXFF_6	6
#define SDIO_CMD_ADDR_TXFF_7	7
#define SDIO_CMD_ADDR_TXFF_8	8
#define SDIO_CMD_ADDR_TXFF_9	9
#define SDIO_CMD_ADDR_TXFF_10	10
#define SDIO_CMD_ADDR_TXFF_11	11
#define SDIO_CMD_ADDR_TXFF_12	12
#define SDIO_CMD_ADDR_RXFF	0x1F00

#define SDIO_REG_LOCAL		0
#define SDIO_REG_WLAN_REG	1
#define SDIO_REG_WLAN_PLTFM	2

#define SDIO_PWR_ON		0
#define SDIO_PWR_OFF		1

#define SDIO_WAIT_CNT		50

#define SDIO_LOCAL_REG_START		0x1000
#define SDIO_LOCAL_REG_END		0x1F00
#define SDIO_LOCAL_REG_END_PATCH	0x2000
#define SDIO_WLAN_REG_END		0x1FFFF
#define SDIO_WLAN_REG_END_PATCH		0xFFFF

#define SDIO_BYTE_MODE_SIZE_MAX 512
#define CMAC_CLK_ALLEN 0xFFFFFFFF
#define SDIO_DEFAULT_AGG_NUM	0x40

/**
 * @struct mac_sdio_tbl
 * @brief mac_sdio_tbl
 *
 * @var mac_sdio_tbl::lock
 * Please Place Description here.
 */
struct mac_sdio_tbl {
	mac_ax_mutex lock;
};

/**
 * @struct mac_sdio_ch_thr
 * @brief mac_sdio_ch_thr
 *
 * @var mac_sdio_ch_thr::thr
 * Please Place Description here.
 * @var mac_sdio_ch_thr::intrpt_en
 * Please Place Description here.
 * @var mac_sdio_ch_thr::wp_sh
 * Please Place Description here.
 * @var mac_sdio_ch_thr::wp_msk
 * Please Place Description here.
 * @var mac_sdio_ch_thr::wd_sh
 * Please Place Description here.
 * @var mac_sdio_ch_thr::wd_msk
 * Please Place Description here.
 */
struct mac_sdio_ch_thr {
	u16 thr;
	u32 intrpt_en;
	u8 wp_sh;
	u16 wp_msk;
	u8 wd_sh;
	u16 wd_msk;
};

/**
 * @enum sdio_io_size
 *
 * @brief sdio_io_size
 *
 * @var sdio_io_size::SDIO_IO_BYTE
 * Please Place Description here.
 * @var sdio_io_size::SDIO_IO_WORD
 * Please Place Description here.
 * @var sdio_io_size::SDIO_IO_DWORD
 * Please Place Description here.
 * @var sdio_io_size::SDIO_IO_LAST
 * Please Place Description here.
 * @var sdio_io_size::SDIO_IO_MAX
 * Please Place Description here.
 * @var sdio_io_size::SDIO_IO_INVALID
 * Please Place Description here.
 */
enum sdio_io_size {
	SDIO_IO_BYTE,
	SDIO_IO_WORD,
	SDIO_IO_DWORD,

	/* keep last */
	SDIO_IO_LAST,
	SDIO_IO_MAX = SDIO_IO_LAST,
	SDIO_IO_INVALID = SDIO_IO_LAST,
};

/**
 * @enum sdio_tx_byte_cnt
 *
 * @brief sdio_tx_byte_cnt
 *
 * @var sdio_tx_byte_cnt::SDIO_IO_BYTE
 * Please Place Description here.
 * @var sdio_tx_byte_cnt::SDIO_IO_WORD
 * Please Place Description here.
 */
enum sdio_tx_byte_cnt {
	SDIO_TX_AGG_8_BYTE_CNT,
	SDIO_TX_DUMMY_4_BYTE_CNT,
};

/**
 * @addtogroup HCI
 * @{
 * @addtogroup BasicIO
 * @{
 */
/**
 * @brief reg_read8_sdio
 *
 * @param *adapter
 * @param addr
 * @return Please Place Description here.
 * @retval u8
 */
u8 reg_read8_sdio(struct mac_ax_adapter *adapter, u32 addr);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup BasicIO
 * @{
 */
/**
 * @brief reg_write8_sdio
 *
 * @param *adapter
 * @param addr
 * @param val
 * @return Please Place Description here.
 * @retval void
 */
void reg_write8_sdio(struct mac_ax_adapter *adapter, u32 addr, u8 val);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup BasicIO
 * @{
 */
/**
 * @brief reg_read16_sdio
 *
 * @param *adapter
 * @param addr
 * @return Please Place Description here.
 * @retval u16
 */
u16 reg_read16_sdio(struct mac_ax_adapter *adapter, u32 addr);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup BasicIO
 * @{
 */
/**
 * @brief reg_write16_sdio
 *
 * @param *adapter
 * @param addr
 * @param val
 * @return Please Place Description here.
 * @retval void
 */
void reg_write16_sdio(struct mac_ax_adapter *adapter, u32 addr, u16 val);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup BasicIO
 * @{
 */

/**
 * @brief reg_read32_sdio
 *
 * @param *adapter
 * @param addr
 * @return Please Place Description here.
 * @retval u32
 */
u32 reg_read32_sdio(struct mac_ax_adapter *adapter, u32 addr);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup BasicIO
 * @{
 */
/**
 * @brief reg_write32_sdio
 *
 * @param *adapter
 * @param addr
 * @param val
 * @return Please Place Description here.
 * @retval void
 */
void reg_write32_sdio(struct mac_ax_adapter *adapter, u32 addr, u32 val);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup SDIO
 * @{
 */
/**
 * @brief reg_read_n_sdio
 *
 * @param *adapter
 * @param adr
 * @param size
 * @param *val
 * @return Please Place Description here.
 * @retval u32
 */
u32 reg_read_n_sdio(struct mac_ax_adapter *adapter, u32 adr, u32 size, u8 *val);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup SDIO
 * @{
 */

/**
 * @brief tx_allow_sdio
 *
 * @param *adapter
 * @param *info
 * @return Please Place Description here.
 * @retval u32
 */
u32 tx_allow_sdio(struct mac_ax_adapter *adapter,
		  struct mac_ax_sdio_tx_info *info);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup SDIO
 * @{
 */

/**
 * @brief tx_cmd_addr_sdio
 *
 * @param *adapter
 * @param *info
 * @param *cmd_addr
 * @return Please Place Description here.
 * @retval u32
 */
u32 tx_cmd_addr_sdio(struct mac_ax_adapter *adapter,
		     struct mac_ax_sdio_tx_info *info, u32 *cmd_addr);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup SDIO
 * @{
 */

/**
 * @brief sdio_pre_init
 *
 * @param *adapter
 * @param *param
 * @return Please Place Description here.
 * @retval u32
 */
u32 sdio_pre_init(struct mac_ax_adapter *adapter, void *param);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup SDIO
 * @{
 */

/**
 * @brief sdio_init
 *
 * @param *adapter
 * @param *param
 * @return Please Place Description here.
 * @retval u32
 */
u32 sdio_init(struct mac_ax_adapter *adapter, void *param);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup SDIO
 * @{
 */

/**
 * @brief sdio_deinit
 *
 * @param *adapter
 * @param *param
 * @return Please Place Description here.
 * @retval u32
 */
u32 sdio_deinit(struct mac_ax_adapter *adapter, void *param);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup SDIO
 * @{
 */

/**
 * @brief r_indir_sdio
 *
 * @param *adapter
 * @param adr
 * @param size
 * @return Please Place Description here.
 * @retval u32
 */

u32 r_indir_sdio(struct mac_ax_adapter *adapter, u32 adr,
		 enum sdio_io_size size);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup SDIO
 * @{
 */

/**
 * @brief w_indir_sdio
 *
 * @param *adapter
 * @param adr
 * @param val
 * @param size
 * @return Please Place Description here.
 * @retval void
 */
void w_indir_sdio(struct mac_ax_adapter *adapter, u32 adr, u32 val,
		  enum sdio_io_size size);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup SDIO
 * @{
 */

/**
 * @brief set_info_sdio
 *
 * @param *adapter
 * @param *info
 * @return Please Place Description here.
 * @retval u32
 */
u32 set_info_sdio(struct mac_ax_adapter *adapter,
		  struct mac_ax_sdio_info *info);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup SDIO
 * @{
 */

/**
 * @brief tx_mode_cfg_sdio
 *
 * @param *adapter
 * @param mode
 * @return Please Place Description here.
 * @retval u32
 */
u32 tx_mode_cfg_sdio(struct mac_ax_adapter *adapter,
		     enum mac_ax_sdio_tx_mode mode);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup SDIO
 * @{
 */

/**
 * @brief rx_agg_cfg_sdio
 *
 * @param *adapter
 * @param *cfg
 * @return Please Place Description here.
 * @retval void
 */
void rx_agg_cfg_sdio(struct mac_ax_adapter *adapter,
		     struct mac_ax_rx_agg_cfg *cfg);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup SDIO
 * @{
 */

/**
 * @brief tx_agg_cfg_sdio
 *
 * @param *adapter
 * @param *cfg
 * @return Please Place Description here.
 * @retval u32
 */
u32 tx_agg_cfg_sdio(struct mac_ax_adapter *adapter,
		    struct mac_ax_sdio_txagg_cfg *cfg);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup SDIO
 * @{
 */

/**
 * @brief aval_page_cfg_sdio
 *
 * @param *adapter
 * @param *cfg
 * @return Please Place Description here.
 * @retval void
 */
void aval_page_cfg_sdio(struct mac_ax_adapter *adapter,
			struct mac_ax_aval_page_cfg *cfg);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup SDIO
 * @{
 */

/**
 * @brief leave_suspend_sdio
 *
 * @param *adapter
 * @return Please Place Description here.
 * @retval u32
 */
u32 leave_suspend_sdio(struct mac_ax_adapter *adapter);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup SDIO
 * @{
 */

/**
 * @brief get_int_latency_sdio
 *
 * @param *adapter
 * @return Please Place Description here.
 * @retval u32
 */
u32 get_int_latency_sdio(struct mac_ax_adapter *adapter);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup SDIO
 * @{
 */

/**
 * @brief get_clk_cnt_sdio
 *
 * @param *adapter
 * @param *cnt
 * @return Please Place Description here.
 * @retval u32
 */
u32 get_clk_cnt_sdio(struct mac_ax_adapter *adapter, u32 *cnt);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup SDIO
 * @{
 */

/**
 * @brief set_wt_cfg_sdio
 *
 * @param *adapter
 * @param en
 * @return Please Place Description here.
 * @retval u32
 */
u32 set_wt_cfg_sdio(struct mac_ax_adapter *adapter, u8 en);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup SDIO
 * @{
 */

/**
 * @brief set_clk_mon_sdio
 *
 * @param *adapter
 * @param *cfg
 * @return Please Place Description here.
 * @retval u32
 */
u32 set_clk_mon_sdio(struct mac_ax_adapter *adapter,
		     struct mac_ax_sdio_clk_mon_cfg *cfg);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup SDIO
 * @{
 */

/**
 * @brief sdio_tbl_init
 *
 * @param *adapter
 * @return Please Place Description here.
 * @retval u32
 */
u32 sdio_tbl_init(struct mac_ax_adapter *adapter);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup SDIO
 * @{
 */

/**
 * @brief sdio_tbl_exit
 *
 * @param *adapter
 * @return Please Place Description here.
 * @retval u32
 */
u32 sdio_tbl_exit(struct mac_ax_adapter *adapter);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup SDIO
 * @{
 */

/**
 * @brief sdio_pwr_switch
 *
 * @param *vadapter
 * @param pre_switch
 * @param on
 * @return Please Place Description here.
 * @retval u32
 */
u32 sdio_pwr_switch(void *vadapter,
		    u8 pre_switch, u8 on);
/**
 * @}
 * @}
 */

/**
 * @brief sdio_pwr_switch
 *
 * @param *vadapter
 * @param mac_ax_wow_ctrl
 * @return Please Place Description here.
 * @retval u32
 */
u32 set_sdio_wowlan(struct mac_ax_adapter *adapter, enum mac_ax_wow_ctrl w_c);
/**
 * @}
 * @}
 */

/**
 * @brief sdio_get_txagg_num
 *
 * @param *adapter
 * @param band
 * @return Please Place Description here.
 * @retval u32
 */
u32 sdio_get_txagg_num(struct mac_ax_adapter *adapter, u8 band);
/**
 * @}
 * @}
 */

/**
 * @brief sdio_autok_counter_avg
 *
 * @param *adapter
 * @return Please Place Description here.
 * @retval u32
 */
u32 sdio_autok_counter_avg(struct mac_ax_adapter *adapter);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup SDIO
 * @{
 */

/**
 * @brief dbcc_hci_ctrl_sdio
 *
 * @param *adapter
 * @param *info
 * @return Please Place Description here.
 * @retval u32
 */
u32 dbcc_hci_ctrl_sdio(struct mac_ax_adapter *adapter,
		       struct mac_ax_dbcc_hci_ctrl *info);
/**
 * @}
 * @}
 */
#endif /*MAC_AX_SDIO_SUPPORT*/
#endif
