/******************************************************************************
 * Copyright(c) 2008 - 2010 Realtek Corporation. All rights reserved.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 * wlanfae <wlanfae@realtek.com>
******************************************************************************/

#include "rtl_core.h"
#include "r8192E_hw.h"
#include "r8190P_hwimg.h"
#include "r819xE_firmware.h"
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 5, 0)
#include <linux/firmware.h>
#endif

void firmware_init_param(struct net_device *dev)
{
	struct r8192_priv	*priv = rtllib_priv(dev);
	rt_firmware		*pfirmware = priv->pFirmware;

	pfirmware->cmdpacket_frag_thresold = GET_COMMAND_PACKET_FRAG_THRESHOLD(MAX_TRANSMIT_BUFFER_SIZE);
}

static bool fw_download_code(struct net_device *dev, u8 *code_virtual_address, u32 buffer_len)
{
	struct r8192_priv   *priv = rtllib_priv(dev);
	bool		    rt_status = true;
	u16		    frag_threshold;
	u16		    frag_length, frag_offset = 0;
	int		    i;

	rt_firmware	    *pfirmware = priv->pFirmware;
	struct sk_buff	    *skb;
	unsigned char	    *seg_ptr;
	cb_desc		    *tcb_desc;
	u8                  bLastIniPkt;

	firmware_init_param(dev);
	frag_threshold = pfirmware->cmdpacket_frag_thresold;
	do {
		if ((buffer_len - frag_offset) > frag_threshold) {
			frag_length = frag_threshold ;
			bLastIniPkt = 0;

		} else {
			frag_length = buffer_len - frag_offset;
			bLastIniPkt = 1;

		}

		skb  = dev_alloc_skb(frag_length + 4);
		memcpy((unsigned char *)(skb->cb),&dev, sizeof(dev));
		tcb_desc = (cb_desc*)(skb->cb + MAX_DEV_ADDR_SIZE);
		tcb_desc->queue_index = TXCMD_QUEUE;
		tcb_desc->bCmdOrInit = DESC_PACKET_TYPE_INIT;
		tcb_desc->bLastIniPkt = bLastIniPkt;

		seg_ptr = skb->data;
		for (i = 0 ; i < frag_length; i+= 4) {
			*seg_ptr++ = ((i+0)<frag_length)?code_virtual_address[i+3]:0;
			*seg_ptr++ = ((i+1)<frag_length)?code_virtual_address[i+2]:0;
			*seg_ptr++ = ((i+2)<frag_length)?code_virtual_address[i+1]:0;
			*seg_ptr++ = ((i+3)<frag_length)?code_virtual_address[i+0]:0;
		}
		tcb_desc->txbuf_size = (u16)i;
		skb_put(skb, i);

		if (!priv->rtllib->check_nic_enough_desc(dev, tcb_desc->queue_index)||
			(!skb_queue_empty(&priv->rtllib->skb_waitQ[tcb_desc->queue_index]))||\
			(priv->rtllib->queue_stop) ) {
			RT_TRACE(COMP_FIRMWARE, "===================> tx full!\n");
			skb_queue_tail(&priv->rtllib->skb_waitQ[tcb_desc->queue_index], skb);
		} else {
		priv->rtllib->softmac_hard_start_xmit(skb, dev);
		}

		code_virtual_address += frag_length;
		frag_offset += frag_length;

	} while (frag_offset < buffer_len);

	return rt_status;
}

static bool fwSendNullPacket(struct net_device *dev, u32 Length)
{
	bool	rtStatus = true;
	struct r8192_priv   *priv = rtllib_priv(dev);
	struct sk_buff	    *skb;
	cb_desc		    *tcb_desc;
	unsigned char	    *ptr_buf;
	bool	bLastInitPacket = false;


	skb  = dev_alloc_skb(Length+ 4);
	memcpy((unsigned char *)(skb->cb),&dev, sizeof(dev));
	tcb_desc = (cb_desc*)(skb->cb + MAX_DEV_ADDR_SIZE);
	tcb_desc->queue_index = TXCMD_QUEUE;
	tcb_desc->bCmdOrInit = DESC_PACKET_TYPE_INIT;
	tcb_desc->bLastIniPkt = bLastInitPacket;
	ptr_buf = skb_put(skb, Length);
	memset(ptr_buf, 0, Length);
	tcb_desc->txbuf_size = (u16)Length;

	if (!priv->rtllib->check_nic_enough_desc(dev, tcb_desc->queue_index)||
			(!skb_queue_empty(&priv->rtllib->skb_waitQ[tcb_desc->queue_index]))||\
			(priv->rtllib->queue_stop) ) {
			RT_TRACE(COMP_FIRMWARE,"=================== NULL packet ================> tx full!\n");
			skb_queue_tail(&priv->rtllib->skb_waitQ[tcb_desc->queue_index], skb);
		} else {
			priv->rtllib->softmac_hard_start_xmit(skb, dev);
		}

	return rtStatus;
}

static bool CPUcheck_maincodeok_turnonCPU(struct net_device *dev)
{
	bool		rt_status = true;
	u32		CPU_status = 0;
	unsigned long   timeout;

	timeout = jiffies + MSECS(20);
	while (time_before(jiffies, timeout)) {
		CPU_status = read_nic_dword(dev, CPU_GEN);
		if (CPU_status & CPU_GEN_PUT_CODE_OK)
			break;
		msleep(2);
	}

	if (!(CPU_status&CPU_GEN_PUT_CODE_OK)) {
		RT_TRACE(COMP_ERR, "Download Firmware: Put code fail!\n");
		goto CPUCheckMainCodeOKAndTurnOnCPU_Fail;
	} else {
		RT_TRACE(COMP_FIRMWARE, "Download Firmware: Put code ok!\n");
	}

	CPU_status = read_nic_dword(dev, CPU_GEN);
	write_nic_byte(dev, CPU_GEN, (u8)((CPU_status|CPU_GEN_PWR_STB_CPU)&0xff));
	mdelay(1);

	timeout = jiffies + MSECS(20);
	while (time_before(jiffies, timeout)) {
		CPU_status = read_nic_dword(dev, CPU_GEN);
		if (CPU_status&CPU_GEN_BOOT_RDY)
			break;
		msleep(2);
	}

	if (!(CPU_status&CPU_GEN_BOOT_RDY)) {
		goto CPUCheckMainCodeOKAndTurnOnCPU_Fail;
	} else {
		RT_TRACE(COMP_FIRMWARE, "Download Firmware: Boot ready!\n");
	}

	return rt_status;

CPUCheckMainCodeOKAndTurnOnCPU_Fail:
	RT_TRACE(COMP_ERR, "ERR in %s()\n", __FUNCTION__);
	rt_status = false;
	return rt_status;
}

static bool CPUcheck_firmware_ready(struct net_device *dev)
{

	bool	rt_status = true;
	u32	CPU_status = 0;
	unsigned long timeout;

	timeout = jiffies + MSECS(20);
	while (time_before(jiffies, timeout)) {
		CPU_status = read_nic_dword(dev, CPU_GEN);
		if (CPU_status&CPU_GEN_FIRM_RDY)
			break;
		msleep(2);
	}

	if (!(CPU_status&CPU_GEN_FIRM_RDY))
		goto CPUCheckFirmwareReady_Fail;
	else
		RT_TRACE(COMP_FIRMWARE, "Download Firmware: Firmware ready!\n");

	return rt_status;

CPUCheckFirmwareReady_Fail:
	RT_TRACE(COMP_ERR, "ERR in %s()\n", __FUNCTION__);
	rt_status = false;
	return rt_status;

}

inline static bool firmware_check_ready(struct net_device *dev, u8 load_fw_status)
{
	struct r8192_priv	*priv = rtllib_priv(dev);
	rt_firmware *pfirmware = priv->pFirmware;
	bool rt_status  = true;

	switch (load_fw_status) {
	case FW_INIT_STEP0_BOOT:
		pfirmware->firmware_status = FW_STATUS_1_MOVE_BOOT_CODE;
		rt_status = fwSendNullPacket(dev, RTL8190_CPU_START_OFFSET);
		if (!rt_status) {
			RT_TRACE(COMP_INIT, "fwSendNullPacket() fail ! \n");
		}
		break;

	case FW_INIT_STEP1_MAIN:
		pfirmware->firmware_status = FW_STATUS_2_MOVE_MAIN_CODE;

		rt_status = CPUcheck_maincodeok_turnonCPU(dev);
		if (rt_status) {
			pfirmware->firmware_status = FW_STATUS_3_TURNON_CPU;
		} else {
			RT_TRACE(COMP_FIRMWARE, "CPUcheck_maincodeok_turnonCPU fail!\n");
		}

		break;

	case FW_INIT_STEP2_DATA:
		pfirmware->firmware_status = FW_STATUS_4_MOVE_DATA_CODE;
		mdelay(1);

		rt_status = CPUcheck_firmware_ready(dev);
		if (rt_status) {
			pfirmware->firmware_status = FW_STATUS_5_READY;
		} else {
			RT_TRACE(COMP_FIRMWARE, "CPUcheck_firmware_ready fail(%d)!\n", rt_status);
		}

		break;
	default:
		rt_status = false;
		RT_TRACE(COMP_FIRMWARE, "Unknown firware status");
		break;
	}

	return rt_status;
}

bool init_firmware(struct net_device *dev)
{
	struct r8192_priv	*priv = rtllib_priv(dev);
	bool			rt_status = true;

	u8	*firmware_img_buf[3] = { &Rtl8190PciFwBootArray[0],
					 &Rtl8190PciFwMainArray[0],
					 &Rtl8190PciFwDataArray[0]};

	u32	firmware_img_len[3] = { sizeof(Rtl8190PciFwBootArray),
					sizeof(Rtl8190PciFwMainArray),
					sizeof(Rtl8190PciFwDataArray)};
	u32	file_length = 0;
	u8	*mapped_file = NULL;
	u8	init_step = 0;
	opt_rst_type_e	rst_opt = OPT_SYSTEM_RESET;
	firmware_init_step_e	starting_state = FW_INIT_STEP0_BOOT;

	rt_firmware		*pfirmware = priv->pFirmware;

	RT_TRACE(COMP_FIRMWARE, " PlatformInitFirmware() ==>\n");

	if (pfirmware->firmware_status == FW_STATUS_0_INIT ) {
		rst_opt = OPT_SYSTEM_RESET;
		starting_state = FW_INIT_STEP0_BOOT;

	} else if (pfirmware->firmware_status == FW_STATUS_5_READY) {
		rst_opt = OPT_FIRMWARE_RESET;
		starting_state = FW_INIT_STEP2_DATA;
	} else {
		 RT_TRACE(COMP_FIRMWARE, "PlatformInitFirmware: undefined firmware state\n");
	}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 5, 0) && defined(USE_FW_SOURCE_IMG_FILE)
	priv->firmware_source = FW_SOURCE_IMG_FILE;
#else
	priv->firmware_source = FW_SOURCE_HEADER_FILE;
#endif
	for (init_step = starting_state; init_step <= FW_INIT_STEP2_DATA; init_step++) {
		if (rst_opt == OPT_SYSTEM_RESET) {
			switch (priv->firmware_source) {
			case FW_SOURCE_IMG_FILE: {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 5, 0) && defined(USE_FW_SOURCE_IMG_FILE)
				const struct firmware	*fw_entry;
				int rc;
				if (pfirmware->firmware_buf_size[init_step] == 0) {
					const char *fw_name[3] = {"rtlwifi/rtl8190p_boot.img",
								  "rtlwifi/rtl8190p_main.img",
								  "rtlwifi/rtl8190p_data.img"
					                         };
					rc = request_firmware(&fw_entry, fw_name[init_step],&priv->pdev->dev);
					if (rc < 0 ) {
						RT_TRACE(COMP_FIRMWARE, "request firmware fail!\n");
						goto download_firmware_fail;
					}
					if (fw_entry->size > sizeof(pfirmware->firmware_buf[init_step])) {
						RT_TRACE(COMP_FIRMWARE, "img file size exceed the container buffer fail!\n");
						goto download_firmware_fail;
					}

					if (init_step != FW_INIT_STEP1_MAIN) {
						memcpy(pfirmware->firmware_buf[init_step], fw_entry->data, fw_entry->size);
						pfirmware->firmware_buf_size[init_step] = fw_entry->size;

					} else {
						memcpy(pfirmware->firmware_buf[init_step], fw_entry->data, fw_entry->size);
						pfirmware->firmware_buf_size[init_step] = fw_entry->size;

					}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 5, 0)
						if (rst_opt == OPT_SYSTEM_RESET) {
							release_firmware(fw_entry);
						}
#endif
					}
					mapped_file = pfirmware->firmware_buf[init_step];
					file_length = pfirmware->firmware_buf_size[init_step];
#endif
					break;
				}
				case FW_SOURCE_HEADER_FILE:
					mapped_file =  firmware_img_buf[init_step];
					file_length  = firmware_img_len[init_step];
					if (init_step == FW_INIT_STEP2_DATA) {
						memcpy(pfirmware->firmware_buf[init_step], mapped_file, file_length);
						pfirmware->firmware_buf_size[init_step] = file_length;
					}
					break;

				default:
					break;
			}


		} else if (rst_opt == OPT_FIRMWARE_RESET ) {
			mapped_file = pfirmware->firmware_buf[init_step];
			file_length = pfirmware->firmware_buf_size[init_step];
		}

		rt_status = fw_download_code(dev, mapped_file, file_length);
		if (rt_status != true) {
			goto download_firmware_fail;
		}

		if (!firmware_check_ready(dev, init_step)) {
			goto download_firmware_fail;
		}
	}

	RT_TRACE(COMP_FIRMWARE, "Firmware Download Success\n");
	return rt_status;

download_firmware_fail:
	RT_TRACE(COMP_ERR, "ERR in %s()\n", __FUNCTION__);
	rt_status = false;
	return rt_status;

}
