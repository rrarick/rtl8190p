/******************************************************************************
 * Copyright(c) 2008 - 2010 Realtek Corporation. All rights reserved.
 *
 * Based on the r8180 driver, which is:
 * Copyright 2004-2005 Andrea Merello <andreamrl@tiscali.it>, et al.
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
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
#undef LOOP_TEST
#undef RX_DONT_PASS_UL
#undef DEBUG_EPROM
#undef DEBUG_RX_VERBOSE
#undef DUMMY_RX
#undef DEBUG_ZERO_RX
#undef DEBUG_RX_SKB
#undef DEBUG_TX_FRAG
#undef DEBUG_RX_FRAG
#undef DEBUG_TX_FILLDESC
#undef DEBUG_TX
#undef DEBUG_IRQ
#undef DEBUG_RX
#undef DEBUG_RXALLOC
#undef DEBUG_REGISTERS
#undef DEBUG_RING
#undef DEBUG_IRQ_TASKLET
#undef DEBUG_TX_ALLOC
#undef DEBUG_TX_DESC

//#define CONFIG_RTL8192_IO_MAP
#include <asm/uaccess.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include "rtl_core.h"
#if defined RTL8192CE
#include "rtl8192c/r8192C_phy.h"
#include "rtl8192c/r8192C_phyreg.h"
#include "rtl8192c/r8192C_rtl6052.h"
#include "rtl8192c/r8192C_Efuse.h"
#include "rtl8192c/r8192C_dm.h"
#elif defined RTL8192SE
#include "r8192S_phy.h"
#include "r8192S_phyreg.h"
#include "r8192S_rtl6052.h"
#include "r8192S_Efuse.h"
#else
#include "r819xE_phy.h" //added by WB 4.30.2008
#include "r819xE_phyreg.h"
#include "r8190_rtl8256.h" // RTL8225 Radio frontend */
#include "r819xE_cmdpkt.h"
#endif

#include "rtl_wx.h"
#ifndef RTL8192CE
#include "rtl_dm.h"
#endif

#ifdef CONFIG_PM_RTL
#include "rtl_pm.h"
#endif

#ifdef _RTL8192_EXT_PATCH_
#include "../../mshclass/msh_class.h"
#include "rtl_mesh.h"
#endif

int hwwep = 1; //default use hw. set 0 to use software security
static int channels = 0x3fff;
#ifdef _RTL8192_EXT_PATCH_
char* ifname = "ra%d";
#else
char* ifname = "wlan%d";
#endif

//set here to open your trace code. //WB
u32 rt_global_debug_component = \
//				COMP_INIT	|
//				COMP_EPROM	|
//				COMP_PHY	|
//				COMP_RF		|
				COMP_FIRMWARE	|
				//COMP_TRACE	|
//				COMP_DOWN	|
		//		COMP_SWBW	|
//				COMP_SEC	|
//				COMP_MLME	|
			//	COMP_PS		|
			//	COMP_LPS	|
//				COMP_CMD	|
//				COMP_CH	|
		//		COMP_RECV	|
		//		COMP_SEND	|
		//		COMP_POWER	|
			//	COMP_EVENTS	|
			//	COMP_RESET	|
			//	COMP_CMDPKT	|
			//	COMP_POWER_TRACKING	|
                        //	COMP_INTR       |
//				COMP_RATE       |
				COMP_ERR ; //always open err flags on

#ifdef RTL8192SE
struct rtl819x_ops rtl8192se_ops = {
	.nic_type = NIC_8192SE,
//	.init_priv_variable = rtl8192se_init_priv_variable,
	.get_eeprom_size = rtl8192se_get_eeprom_size,
//	.read_eeprom_info = rtl8192se_read_eeprom_info,
//	.read_adapter_info = rtl8192se_adapter_start,
	.initialize_adapter = rtl8192se_adapter_start,
	.link_change = rtl8192se_link_change,
	.tx_fill_descriptor = rtl8192se_tx_fill_desc,
	.tx_fill_cmd_descriptor = rtl8192se_tx_fill_cmd_desc,
	.rx_query_status_descriptor = rtl8192se_rx_query_status_desc,
	.stop_adapter = rtl8192se_halt_adapter,
	.update_ratr_table = rtl8192se_update_ratr_table,
};
#elif defined RTL8192CE
struct rtl819x_ops rtl8192ce_ops = {
	.nic_type					= NIC_8192CE,
	.get_eeprom_size			= rtl8192ce_get_eeprom_size,
	.initialize_adapter			= rtl8192ce_adapter_start,
	.link_change				= rtl8192ce_link_change,
	.tx_fill_descriptor			= rtl8192ce_tx_fill_desc,
	.tx_fill_cmd_descriptor			= rtl8192ce_tx_fill_cmd_desc,
	.rx_query_status_descriptor	= rtl8192ce_rx_query_status_desc,
	.stop_adapter				= rtl8192ce_halt_adapter,
	.update_ratr_table			= rtl8192ce_update_ratr_table,
};
#else
struct rtl819x_ops rtl819xp_ops = {
	.nic_type = NIC_UNKNOWN,
//	.init_priv_variable = rtl8192_init_priv_variable,
	.get_eeprom_size = rtl8192_get_eeprom_size,
//	.read_eeprom_info = rtl8192_read_eeprom_info,
//	.read_adapter_info =
	.initialize_adapter = rtl8192_adapter_start,
	.link_change = rtl8192_link_change,
	.tx_fill_descriptor = rtl8192_tx_fill_desc,
	.tx_fill_cmd_descriptor = rtl8192_tx_fill_cmd_desc,
	.rx_query_status_descriptor = rtl8192_rx_query_status_desc,
	.stop_adapter = rtl8192_halt_adapter,
	.update_ratr_table = rtl8192_update_ratr_table,
};
#endif

static struct pci_device_id rtl8192_pci_id_tbl[] = {
#ifdef RTL8190P
	/* Realtek */
	/* Dlink */
	{RTL_PCI_DEVICE(0x10ec, 0x8190, rtl819xp_ops)},
	/* Corega */
	{RTL_PCI_DEVICE(0x07aa, 0x0045, rtl819xp_ops)},
	{RTL_PCI_DEVICE(0x07aa, 0x0046, rtl819xp_ops)},
#elif defined(RTL8192E)
	/* Realtek */
	{RTL_PCI_DEVICE(0x10ec, 0x8192, rtl819xp_ops)},
	/* Corega */
	{RTL_PCI_DEVICE(0x07aa, 0x0044, rtl819xp_ops)},
	{RTL_PCI_DEVICE(0x07aa, 0x0047, rtl819xp_ops)},
#elif defined(RTL8192SE)	/*8192SE*/
	{RTL_PCI_DEVICE(0x10ec, 0x8171, rtl8192se_ops)},
	{RTL_PCI_DEVICE(0x10ec, 0x8172, rtl8192se_ops)},
	{RTL_PCI_DEVICE(0x10ec, 0x8173, rtl8192se_ops)},
	{RTL_PCI_DEVICE(0x10ec, 0x8174, rtl8192se_ops)},
#elif defined(RTL8192CE)	/*8192CE*/
	{RTL_PCI_DEVICE(0x10ec, 0x8191, rtl8192ce_ops)},
	{RTL_PCI_DEVICE(0x10ec, 0x8178, rtl8192ce_ops)},
	{RTL_PCI_DEVICE(0x10ec, 0x8177, rtl8192ce_ops)},
	{RTL_PCI_DEVICE(0x10ec, 0x8176, rtl8192ce_ops)},
	{RTL_PCI_DEVICE(0x10ec, 0x092D, rtl8192ce_ops)},
#endif
	{}
};
MODULE_DEVICE_TABLE(pci, rtl8192_pci_id_tbl);

static int rtl8192_pci_probe(struct pci_dev *pdev,
			 const struct pci_device_id *id);
static void rtl8192_pci_disconnect(struct pci_dev *pdev);

static struct pci_driver rtl8192_pci_driver = {
	.name		= DRV_NAME,	          /* Driver name   */
	.id_table	= rtl8192_pci_id_tbl,	          /* PCI_ID table  */
	.probe		= rtl8192_pci_probe,	          /* probe fn      */
	.remove		= rtl8192_pci_disconnect,	  /* remove fn     */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 5, 0)
#ifdef CONFIG_PM_RTL
	.suspend	= rtl8192E_suspend,	          /* PM suspend fn */
	.resume		= rtl8192E_resume,                 /* PM resume fn  */
#else
	.suspend	= NULL,			          /* PM suspend fn */
	.resume		= NULL,			          /* PM resume fn  */
#endif
#endif
};

#ifdef CONFIG_CRDA
static struct ieee80211_rate ipw2200_rates[] = {
	{ .bitrate = 10 },
	{ .bitrate = 20, .flags = IEEE80211_RATE_SHORT_PREAMBLE },
	{ .bitrate = 55, .flags = IEEE80211_RATE_SHORT_PREAMBLE },
	{ .bitrate = 110, .flags = IEEE80211_RATE_SHORT_PREAMBLE },
	{ .bitrate = 60 },
	{ .bitrate = 90 },
	{ .bitrate = 120 },
	{ .bitrate = 180 },
	{ .bitrate = 240 },
	{ .bitrate = 360 },
	{ .bitrate = 480 },
	{ .bitrate = 540 }
};

#define ipw2200_bg_rates        (ipw2200_rates + 0)
#define ipw2200_num_bg_rates    12
static const struct rtllib_geo rtl_geos[] = {
	{			/* Restricted */
	 "---",
	 .bg_channels = 11,
	 .bg = {{2412, 1}, {2417, 2}, {2422, 3},
		{2427, 4}, {2432, 5}, {2437, 6},
		{2442, 7}, {2447, 8}, {2452, 9},
		{2457, 10}, {2462, 11}},
	 },

	{			/* Custom US/Canada */
	 "ZZF",
	 .bg_channels = 11,
	 .bg = {{2412, 1}, {2417, 2}, {2422, 3},
		{2427, 4}, {2432, 5}, {2437, 6},
		{2442, 7}, {2447, 8}, {2452, 9},
		{2457, 10}, {2462, 11}},
	 },

	{			/* Rest of World */
	 "ZZD",
	 .bg_channels = 13,
	 .bg = {{2412, 1}, {2417, 2}, {2422, 3},
		{2427, 4}, {2432, 5}, {2437, 6},
		{2442, 7}, {2447, 8}, {2452, 9},
		{2457, 10}, {2462, 11}, {2467, 12},
		{2472, 13}},
	 },

	{			/* Custom USA & Europe & High */
	 "ZZA",
	 .bg_channels = 11,
	 .bg = {{2412, 1}, {2417, 2}, {2422, 3},
		{2427, 4}, {2432, 5}, {2437, 6},
		{2442, 7}, {2447, 8}, {2452, 9},
		{2457, 10}, {2462, 11}},
	 },

	{			/* Custom NA & Europe */
	 "ZZB",
	 .bg_channels = 11,
	 .bg = {{2412, 1}, {2417, 2}, {2422, 3},
		{2427, 4}, {2432, 5}, {2437, 6},
		{2442, 7}, {2447, 8}, {2452, 9},
		{2457, 10}, {2462, 11}},
	 },

	{			/* Custom Japan */
	 "ZZC",
	 .bg_channels = 11,
	 .bg = {{2412, 1}, {2417, 2}, {2422, 3},
		{2427, 4}, {2432, 5}, {2437, 6},
		{2442, 7}, {2447, 8}, {2452, 9},
		{2457, 10}, {2462, 11}},
	 },

	{			/* Custom */
	 "ZZM",
	 .bg_channels = 11,
	 .bg = {{2412, 1}, {2417, 2}, {2422, 3},
		{2427, 4}, {2432, 5}, {2437, 6},
		{2442, 7}, {2447, 8}, {2452, 9},
		{2457, 10}, {2462, 11}},
	 },

	{			/* Europe */
	 "ZZE",
	 .bg_channels = 13,
	 .bg = {{2412, 1}, {2417, 2}, {2422, 3},
		{2427, 4}, {2432, 5}, {2437, 6},
		{2442, 7}, {2447, 8}, {2452, 9},
		{2457, 10}, {2462, 11}, {2467, 12},
		{2472, 13}},
	 },

	{			/* Custom Japan */
	 "ZZJ",
	 .bg_channels = 14,
	 .bg = {{2412, 1}, {2417, 2}, {2422, 3},
		{2427, 4}, {2432, 5}, {2437, 6},
		{2442, 7}, {2447, 8}, {2452, 9},
		{2457, 10}, {2462, 11}, {2467, 12},
		{2472, 13}, {2484, 14, LIBIPW_CH_B_ONLY}},
	 },

	{			/* Rest of World */
	 "ZZR",
	 .bg_channels = 14,
	 .bg = {{2412, 1}, {2417, 2}, {2422, 3},
		{2427, 4}, {2432, 5}, {2437, 6},
		{2442, 7}, {2447, 8}, {2452, 9},
		{2457, 10}, {2462, 11}, {2467, 12},
		{2472, 13}, {2484, 14, LIBIPW_CH_B_ONLY |
			     LIBIPW_CH_PASSIVE_ONLY}},
	 },

	{			/* High Band */
	 "ZZH",
	 .bg_channels = 13,
	 .bg = {{2412, 1}, {2417, 2}, {2422, 3},
		{2427, 4}, {2432, 5}, {2437, 6},
		{2442, 7}, {2447, 8}, {2452, 9},
		{2457, 10}, {2462, 11},
		{2467, 12, LIBIPW_CH_PASSIVE_ONLY},
		{2472, 13, LIBIPW_CH_PASSIVE_ONLY}},
	 },

	{			/* Custom Europe */
	 "ZZG",
	 .bg_channels = 13,
	 .bg = {{2412, 1}, {2417, 2}, {2422, 3},
		{2427, 4}, {2432, 5}, {2437, 6},
		{2442, 7}, {2447, 8}, {2452, 9},
		{2457, 10}, {2462, 11},
		{2467, 12}, {2472, 13}},
	 },

	{			/* Europe */
	 "ZZK",
	 .bg_channels = 13,
	 .bg = {{2412, 1}, {2417, 2}, {2422, 3},
		{2427, 4}, {2432, 5}, {2437, 6},
		{2442, 7}, {2447, 8}, {2452, 9},
		{2457, 10}, {2462, 11},
		{2467, 12, LIBIPW_CH_PASSIVE_ONLY},
		{2472, 13, LIBIPW_CH_PASSIVE_ONLY}},
	 },

	{			/* Europe */
	 "ZZL",
	 .bg_channels = 11,
	 .bg = {{2412, 1}, {2417, 2}, {2422, 3},
		{2427, 4}, {2432, 5}, {2437, 6},
		{2442, 7}, {2447, 8}, {2452, 9},
		{2457, 10}, {2462, 11}},
	 }
};



int rtllib_set_geo(struct rtllib_device *ieee,
		      const struct rtllib_geo *geo)
{
	memcpy(ieee->geo.name, geo->name, 3);
	ieee->geo.name[3] = '\0';
	ieee->geo.bg_channels = geo->bg_channels;
	memcpy(ieee->geo.bg, geo->bg, geo->bg_channels *
	       sizeof(struct rtllib_channel));
	return 0;
}



#endif

/****************************************************************************
   -----------------------------IO STUFF-------------------------
*****************************************************************************/
#ifdef CONFIG_RTL8192_IO_MAP
u8 read_nic_byte(struct net_device *dev, int x)
{
        return 0xff&inb(dev->base_addr +x);
}

u32 read_nic_dword(struct net_device *dev, int x)
{
        return inl(dev->base_addr +x);
}

u16 read_nic_word(struct net_device *dev, int x)
{
        return inw(dev->base_addr +x);
}

void write_nic_byte(struct net_device *dev, int x,u8 y)
{
        outb(y&0xff,dev->base_addr +x);
}

void write_nic_word(struct net_device *dev, int x,u16 y)
{
        outw(y,dev->base_addr +x);
}

void write_nic_dword(struct net_device *dev, int x,u32 y)
{
        outl(y,dev->base_addr +x);
}
#else /* RTL_IO_MAP */
u8 read_nic_byte(struct net_device *dev, int x)
{
#ifdef CONFIG_LOCK_READ_AND_WRITE
	struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);
	unsigned long flags;
	u8 val = 0;

	spin_lock_irqsave(&priv->rw_lock, flags);
	val = 0xff&readb((u8*)dev->mem_start +x);
	spin_unlock_irqrestore(&priv->rw_lock, flags);

	return val;
#else
        return 0xff&readb((u8*)dev->mem_start +x);
#endif
}

u32 read_nic_dword(struct net_device *dev, int x)
{
#ifdef CONFIG_LOCK_READ_AND_WRITE
	struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);
	unsigned long flags;
	u32 val = 0;

	spin_lock_irqsave(&priv->rw_lock, flags);
	val = readl((u8*)dev->mem_start +x);
	spin_unlock_irqrestore(&priv->rw_lock, flags);

	return val;
#else
        return readl((u8*)dev->mem_start +x);
#endif
}

u16 read_nic_word(struct net_device *dev, int x)
{
#ifdef CONFIG_LOCK_READ_AND_WRITE
	struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);
	unsigned long flags;
	u16 val = 0;

	spin_lock_irqsave(&priv->rw_lock, flags);
	val = readw((u8*)dev->mem_start +x);
	spin_unlock_irqrestore(&priv->rw_lock, flags);

	return val;
#else
        return readw((u8*)dev->mem_start +x);
#endif
}

void write_nic_byte(struct net_device *dev, int x,u8 y)
{
#ifdef CONFIG_LOCK_READ_AND_WRITE
	struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);
	unsigned long flags;

	spin_lock_irqsave(&priv->rw_lock, flags);
        writeb(y,(u8*)dev->mem_start +x);
	spin_unlock_irqrestore(&priv->rw_lock, flags);
#else
        writeb(y,(u8*)dev->mem_start +x);
#if !(defined RTL8192SE || defined RTL8192CE)
	udelay(20);
#endif
#endif
}

void write_nic_dword(struct net_device *dev, int x,u32 y)
{
#ifdef CONFIG_LOCK_READ_AND_WRITE
	struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);
	unsigned long flags;

	spin_lock_irqsave(&priv->rw_lock, flags);
        writel(y,(u8*)dev->mem_start +x);
	spin_unlock_irqrestore(&priv->rw_lock, flags);
#else
        writel(y,(u8*)dev->mem_start +x);
#if !(defined RTL8192SE || defined RTL8192CE)
	udelay(20);
#endif
#endif
}

void write_nic_word(struct net_device *dev, int x,u16 y)
{
#ifdef CONFIG_LOCK_READ_AND_WRITE
	struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);
	unsigned long flags;

	spin_lock_irqsave(&priv->rw_lock, flags);
        writew(y,(u8*)dev->mem_start +x);
	spin_unlock_irqrestore(&priv->rw_lock, flags);
#else
        writew(y,(u8*)dev->mem_start +x);
#if !(defined RTL8192SE || defined RTL8192CE)
	udelay(20);
#endif
#endif
}
#endif /* RTL_IO_MAP */

/****************************************************************************
   -----------------------------GENERAL FUNCTION-------------------------
*****************************************************************************/

void
MgntDisconnectIBSS(
	struct net_device* dev
)
{
	struct r8192_priv *priv = rtllib_priv(dev);
	u8	OpMode;
	u8			i;
	bool	bFilterOutNonAssociatedBSSID = false;

	//IEEE80211_DEBUG(RTLLIB_DL_TRACE, "XXXXXXXXXX MgntDisconnect IBSS\n");

	priv->rtllib->state = RTLLIB_NOLINK;

//	PlatformZeroMemory( pMgntInfo->Bssid, 6 );
	for(i=0;i<6;i++)  priv->rtllib->current_network.bssid[i]= 0x55;
	priv->OpMode = RT_OP_MODE_NO_LINK;
	//write_nic_word(dev, BSSIDR, ((u16*)priv->rtllib->current_network.bssid)[0]);
	//write_nic_dword(dev, BSSIDR+2, ((u32*)(priv->rtllib->current_network.bssid+2))[0]);
	priv->rtllib->SetHwRegHandler(dev, HW_VAR_BSSID, priv->rtllib->current_network.bssid);
#if 1
	OpMode = RT_OP_MODE_NO_LINK;
	priv->rtllib->SetHwRegHandler(dev, HW_VAR_MEDIA_STATUS, &OpMode);
#else
	{
			RT_OP_MODE	OpMode = priv->OpMode;
			//LED_CTL_MODE	LedAction = LED_CTL_NO_LINK;
			u8	btMsr = read_nic_byte(dev, MSR);

			btMsr &= 0xfc;

			switch(OpMode)
			{
			case RT_OP_MODE_INFRASTRUCTURE:
				btMsr |= MSR_LINK_MANAGED;
				//LedAction = LED_CTL_LINK;
				break;

			case RT_OP_MODE_IBSS:
				btMsr |= MSR_LINK_ADHOC;
				// led link set seperate
				break;

			case RT_OP_MODE_AP:
				btMsr |= MSR_LINK_MASTER;
				//LedAction = LED_CTL_LINK;
				break;

			default:
				btMsr |= MSR_LINK_NONE;
				break;
			}

			write_nic_byte(dev, MSR, btMsr);

			// LED control
			//dev->HalFunc.LedControlHandler(dev, LedAction);
	}
#endif
	rtllib_stop_send_beacons(priv->rtllib);

	// If disconnect, clear RCR CBSSID bit
	bFilterOutNonAssociatedBSSID = false;
#if 1
	priv->rtllib->SetHwRegHandler(dev, HW_VAR_CECHK_BSSID, (u8*)(&bFilterOutNonAssociatedBSSID));
#else
	{
			u32 RegRCR, Type;
			Type = bFilterOutNonAssociatedBSSID;
			RegRCR = read_nic_dword(dev,RCR);
			priv->ReceiveConfig = RegRCR;
			if (Type == true)
				RegRCR |= (RCR_CBSSID);
			else if (Type == false)
				RegRCR &= (~RCR_CBSSID);

			{
				write_nic_dword(dev, RCR,RegRCR);
				priv->ReceiveConfig = RegRCR;
			}

		}
#endif
	//MgntIndicateMediaStatus( dev, RT_MEDIA_DISCONNECT, GENERAL_INDICATE );
	notify_wx_assoc_event(priv->rtllib);

}

void
MlmeDisassociateRequest(
	struct net_device* dev,
	u8*		asSta,
	u8		asRsn
	)
{
	struct r8192_priv *priv = rtllib_priv(dev);
	u8 i;
	u8	OpMode;

	RemovePeerTS(priv->rtllib, asSta);

	//SendDisassociation( priv->rtllib, 0, asRsn );

	if(memcpy(priv->rtllib->current_network.bssid,asSta,6) == 0)
	{
		//ShuChen TODO: change media status.
		//ShuChen TODO: What to do when disassociate.
		priv->rtllib->state = RTLLIB_NOLINK;

		//pMgntInfo->AsocTimestamp = 0;
		for(i=0;i<6;i++)  priv->rtllib->current_network.bssid[i] = 0x22;
//		pMgntInfo->mBrates.Length = 0;
		OpMode = RT_OP_MODE_NO_LINK;
		priv->OpMode = RT_OP_MODE_NO_LINK;
#if 1
		priv->rtllib->SetHwRegHandler(dev, HW_VAR_MEDIA_STATUS, (u8 *)(&OpMode) );
#else
		{
			RT_OP_MODE	OpMode = priv->OpMode;
			LED_CTL_MODE	LedAction = LED_CTL_NO_LINK;
			u8 btMsr = read_nic_byte(dev, MSR);

			btMsr &= 0xfc;

			switch(OpMode)
			{
			case RT_OP_MODE_INFRASTRUCTURE:
				btMsr |= MSR_LINK_MANAGED;
				LedAction = LED_CTL_LINK;
				break;

			case RT_OP_MODE_IBSS:
				btMsr |= MSR_LINK_ADHOC;
				// led link set seperate
				break;

			case RT_OP_MODE_AP:
				btMsr |= MSR_LINK_MASTER;
				LedAction = LED_CTL_LINK;
				break;

			default:
				btMsr |= MSR_LINK_NONE;
				break;
			}

			write_nic_byte(dev, MSR, btMsr);

			// For 92SE test we must reset TST after setting MSR
			{
				u32	temp = read_nic_dword(dev, TCR);
				write_nic_dword(dev, TCR, temp&(~BIT8));
				write_nic_dword(dev, TCR, temp|BIT8);
			}

			// LED control
			priv->rtllib->LedControlHandler(dev, LedAction);
		}
#endif
		rtllib_disassociate(priv->rtllib);

#if 1
		priv->rtllib->SetHwRegHandler(dev, HW_VAR_BSSID, priv->rtllib->current_network.bssid);
#else
		write_nic_word(dev, BSSIDR, ((u16*)priv->rtllib->current_network.bssid)[0]);
		write_nic_dword(dev, BSSIDR+2, ((u32*)(priv->rtllib->current_network.bssid+2))[0]);
#endif

	}

}

void
MgntDisconnectAP(
	struct net_device* dev,
	u8 asRsn
)
{
	struct r8192_priv *priv = rtllib_priv(dev);
	bool bFilterOutNonAssociatedBSSID = false;

//
// Commented out by rcnjko, 2005.01.27:
// I move SecClearAllKeys() to MgntActSet_802_11_DISASSOCIATE().
//
//	//2004/09/15, kcwu, the key should be cleared, or the new handshaking will not success
//	SecClearAllKeys(dev);

	// In WPA WPA2 need to Clear all key ... because new key will set after new handshaking.
#ifdef TO_DO
	if(   pMgntInfo->SecurityInfo.AuthMode > RT_802_11AuthModeAutoSwitch ||
		(pMgntInfo->bAPSuportCCKM && pMgntInfo->bCCX8021xenable) )	// In CCKM mode will Clear key
	{
		SecClearAllKeys(dev);
		RT_TRACE(COMP_SEC, DBG_LOUD,("======>CCKM clear key..."))
	}
#endif
	// If disconnect, clear RCR CBSSID bit
	bFilterOutNonAssociatedBSSID = false;
#if 1
	priv->rtllib->SetHwRegHandler(dev, HW_VAR_CECHK_BSSID, (u8*)(&bFilterOutNonAssociatedBSSID));
#else
	{
			u32 RegRCR, Type;

			Type = bFilterOutNonAssociatedBSSID;
			//dev->HalFunc.GetHwRegHandler(dev, HW_VAR_RCR, (pu1Byte)(&RegRCR));
			RegRCR = read_nic_dword(dev,RCR);
			priv->ReceiveConfig = RegRCR;

			if (Type == true)
				RegRCR |= (RCR_CBSSID);
			else if (Type == false)
				RegRCR &= (~RCR_CBSSID);

			write_nic_dword(dev, RCR,RegRCR);
			priv->ReceiveConfig = RegRCR;


	}
#endif
	// 2004.10.11, by rcnjko.
	//MlmeDisassociateRequest( dev, pMgntInfo->Bssid, disas_lv_ss );
	MlmeDisassociateRequest( dev, priv->rtllib->current_network.bssid, asRsn );

	priv->rtllib->state = RTLLIB_NOLINK;
	//pMgntInfo->AsocTimestamp = 0;
}

bool
MgntDisconnect(
	struct net_device* dev,
	u8 asRsn
)
{
	struct r8192_priv *priv = rtllib_priv(dev);

	//
	// Schedule an workitem to wake up for ps mode, 070109, by rcnjko.
	//
#if 1
	if(priv->rtllib->ps != RTLLIB_PS_DISABLED)
	{
		//
		// Using AwkaeTimer to prevent mismatch ps state.
		// In the timer the state will be changed according to the RF is being awoke or not. By Bruce, 2007-10-31.
		//
		// PlatformScheduleWorkItem( &(pMgntInfo->AwakeWorkItem) );
		//PlatformSetTimer( dev, &(pMgntInfo->AwakeTimer), 0 );
#ifndef RTL8190P
                rtl8192_hw_wakeup(dev);
#endif
	}
#endif
	// Follow 8180 AP mode, 2005.05.30, by rcnjko.
#ifdef TO_DO
	if(pMgntInfo->mActingAsAp)
	{
		RT_TRACE(COMP_MLME, DBG_LOUD, ("MgntDisconnect() ===> AP_DisassociateAllStation\n"));
		AP_DisassociateAllStation(dev, unspec_reason);
		return true;
	}
#endif
	// Indication of disassociation event.
	//DrvIFIndicateDisassociation(dev, asRsn);

	// In adhoc mode, update beacon frame.
	if( priv->rtllib->state == RTLLIB_LINKED )
	{
		if( priv->rtllib->iw_mode == IW_MODE_ADHOC )
		{
			//RT_TRACE(COMP_MLME, "MgntDisconnect() ===> MgntDisconnectIBSS\n");
			MgntDisconnectIBSS(dev);
		}
#ifdef _RTL8192_EXT_PATCH_
		if((priv->ieee80211->iw_mode == IW_MODE_INFRA ) || ((priv->ieee80211->iw_mode == IW_MODE_MESH) && (priv->ieee80211->only_mesh == 0)))
#else
		if( priv->rtllib->iw_mode == IW_MODE_INFRA )
#endif
		{
			// We clear key here instead of MgntDisconnectAP() because that
			// MgntActSet_802_11_DISASSOCIATE() is an interface called by OS,
			// e.g. OID_802_11_DISASSOCIATE in Windows while as MgntDisconnectAP() is
			// used to handle disassociation related things to AP, e.g. send Disassoc
			// frame to AP.  2005.01.27, by rcnjko.
#ifdef TO_DO_LIST
			SecClearAllKeys(Adapter);
#endif
			//RTLLIB_DEBUG(RTLLIB_DL_TRACE,"MgntDisconnect() ===> MgntDisconnectAP\n");
			MgntDisconnectAP(dev, asRsn);
		}

		// Inidicate Disconnect, 2005.02.23, by rcnjko.
		//MgntIndicateMediaStatus( dev, RT_MEDIA_DISCONNECT, GENERAL_INDICATE);
	}

	return true;
}
//
//
//	Description:
//		Chang RF Power State.
//		Note that, only MgntActSet_RF_State() is allowed to set HW_VAR_RF_STATE.
//
//	Assumption:
//		PASSIVE LEVEL.
//
bool
MgntActSet_RF_State(
	struct net_device* dev,
	RT_RF_POWER_STATE	StateToSet,
	RT_RF_CHANGE_SOURCE ChangeSource
	)
{
	struct r8192_priv *priv = rtllib_priv(dev);
	struct rtllib_device * ieee = priv->rtllib;
	bool			bActionAllowed = false;
	bool			bConnectBySSID = false;
	RT_RF_POWER_STATE	rtState;
	u16			RFWaitCounter = 0;
	unsigned long flag;
	RT_TRACE((COMP_PS | COMP_RF), "===>MgntActSet_RF_State(): StateToSet(%d)\n",StateToSet);

	//1//
	//1//<1>Prevent the race condition of RF state change.
	//1//
	// Only one thread can change the RF state at one time, and others should wait to be executed. By Bruce, 2007-11-28.

	while(true)
	{
		spin_lock_irqsave(&priv->rf_ps_lock,flag);
		if(priv->RFChangeInProgress)
		{
			spin_unlock_irqrestore(&priv->rf_ps_lock,flag);
			RT_TRACE((COMP_PS | COMP_RF), "MgntActSet_RF_State(): RF Change in progress! Wait to set..StateToSet(%d).\n", StateToSet);
			printk("MgntActSet_RF_State(): RF Change in progress! Wait to set..StateToSet(%d).\n", StateToSet);
			#if 1
			// Set RF after the previous action is done.
			while(priv->RFChangeInProgress)
			{
				RFWaitCounter ++;
				RT_TRACE((COMP_PS | COMP_RF), "MgntActSet_RF_State(): Wait 1 ms (%d times)...\n", RFWaitCounter);
				//udelay(1000); // 1 ms
				mdelay(1);

				// Wait too long, return false to avoid to be stuck here.
				if(RFWaitCounter > 100)
				{
					RT_TRACE(COMP_ERR, "MgntActSet_RF_State(): Wait too logn to set RF\n");
					// TODO: Reset RF state?
					return false;
				}
			}
			#endif
			//return false;//do not return here
		}
		else
		{
			priv->RFChangeInProgress = true;
			spin_unlock_irqrestore(&priv->rf_ps_lock,flag);
			break;
		}
	}

	rtState = priv->rtllib->eRFPowerState;

	switch(StateToSet)
	{
	case eRfOn:
		//
		// Turn On RF no matter the IPS setting because we need to update the RF state to Ndis under Vista, or
		// the Windows does not allow the driver to perform site survey any more. By Bruce, 2007-10-02.
		//

		priv->rtllib->RfOffReason &= (~ChangeSource);

		if((ChangeSource == RF_CHANGE_BY_HW) && (priv->bHwRadioOff == true)){
			priv->bHwRadioOff = false;
		}

		if(! priv->rtllib->RfOffReason)
		{
			priv->rtllib->RfOffReason = 0;
			bActionAllowed = true;


			if(rtState == eRfOff && ChangeSource >=RF_CHANGE_BY_HW )
			{
				bConnectBySSID = true;
			}
		}
		else{
			RT_TRACE((COMP_PS | COMP_RF), "MgntActSet_RF_State - eRfon reject pMgntInfo->RfOffReason= 0x%x, ChangeSource=0x%X\n", priv->rtllib->RfOffReason, ChangeSource);
                }

		break;

	case eRfOff:

		if(priv->rtllib->iw_mode != IW_MODE_MASTER)
		{
			if ((priv->rtllib->RfOffReason > RF_CHANGE_BY_IPS) || (ChangeSource > RF_CHANGE_BY_IPS))
			{
				//
				// 060808, Annie:
				// Disconnect to current BSS when radio off. Asked by QuanTa.
				//
				// Set all link status falg, by Bruce, 2007-06-26.
				//MgntActSet_802_11_DISASSOCIATE( dev, disas_lv_ss );
				if(ieee->state == RTLLIB_LINKED)
					priv->blinked_ingpio = true;
				else
					priv->blinked_ingpio = false;
				MgntDisconnect(dev, disas_lv_ss);


				// Clear content of bssDesc[] and bssDesc4Query[] to avoid reporting old bss to UI.
				// 2007.05.28, by shien chang.

			}
		}
		if((ChangeSource == RF_CHANGE_BY_HW) && (priv->bHwRadioOff == false)){
			priv->bHwRadioOff = true;
		}
		priv->rtllib->RfOffReason |= ChangeSource;
		bActionAllowed = true;
		break;

	case eRfSleep:
		priv->rtllib->RfOffReason |= ChangeSource;
		bActionAllowed = true;
		break;

	default:
		break;
	}

	if(bActionAllowed)
	{
		RT_TRACE((COMP_PS | COMP_RF), "MgntActSet_RF_State(): Action is allowed.... StateToSet(%d), RfOffReason(%#X)\n", StateToSet, priv->rtllib->RfOffReason);
				// Config HW to the specified mode.
		PHY_SetRFPowerState(dev, StateToSet);
		// Turn on RF.
		if(StateToSet == eRfOn)
		{
			//dev->HalFunc.HalEnableRxHandler(dev);

			if(bConnectBySSID && (priv->blinked_ingpio == true))
			{
				//MgntActSet_802_11_SSID(dev, dev->MgntInfo.Ssid.Octet, dev->MgntInfo.Ssid.Length, true );
				queue_delayed_work_rsl(ieee->wq, &ieee->associate_procedure_wq, 0);
				priv->blinked_ingpio = false;

			}
		}
		// Turn off RF.
		else if(StateToSet == eRfOff)
		{
			//dev->HalFunc.HalDisableRxHandler(dev);
		}
	}
	else
	{
		RT_TRACE((COMP_PS | COMP_RF), "MgntActSet_RF_State(): Action is rejected.... StateToSet(%d), ChangeSource(%#X), RfOffReason(%#X)\n", StateToSet, ChangeSource, priv->rtllib->RfOffReason);
	}

	// Release RF spinlock
	spin_lock_irqsave(&priv->rf_ps_lock,flag);
	priv->RFChangeInProgress = false;
	spin_unlock_irqrestore(&priv->rf_ps_lock,flag);

	RT_TRACE((COMP_PS && COMP_RF), "<===MgntActSet_RF_State()\n");
	return bActionAllowed;
}


short get_nic_desc_num(struct net_device *dev, int prio)
{
    struct r8192_priv *priv = rtllib_priv(dev);
    struct rtl8192_tx_ring *ring = &priv->tx_ring[prio];

    /* For now, we reserved two free descriptor as a safety boundary
     * between the tail and the head
     */
    if((prio == MGNT_QUEUE) &&(skb_queue_len(&ring->queue)>10))
	printk("-----[%d]---------ring->idx=%d queue_len=%d---------\n",
			prio,ring->idx, skb_queue_len(&ring->queue));
   // if((prio == BE_QUEUE) &&(skb_queue_len(&ring->queue)>55))
    //	printk("------[%d]--------ring->idx=%d queue_len=%d---------\n",prio,ring->idx, skb_queue_len(&ring->queue));
    return skb_queue_len(&ring->queue);
}

short check_nic_enough_desc(struct net_device *dev, int prio)
{
    struct r8192_priv *priv = rtllib_priv(dev);
    struct rtl8192_tx_ring *ring = &priv->tx_ring[prio];

    // for now we reserve two free descriptor as a safety boundary
    // between the tail and the head
    //
    if (ring->entries - skb_queue_len(&ring->queue) >= 2) {
        return 1;
    } else {
        return 0;
    }
}

void tx_timeout(struct net_device *dev)
{
    struct r8192_priv *priv = rtllib_priv(dev);

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0))
    schedule_work(&priv->reset_wq);
#else
    schedule_task(&priv->reset_wq);
#endif
    printk("TXTIMEOUT");
}

//notice INTA_MASK in 92SE will have 2 DW now. if second DW is used, add it here.
void rtl8192_irq_enable(struct net_device *dev)
{
	struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);
	priv->irq_enabled = 1;

#ifdef RTL8192CE
	write_nic_dword(dev, REG_HIMR, priv->irq_mask[0]&0xFFFFFFFF);
#else
	write_nic_dword(dev,INTA_MASK, priv->irq_mask[0]);
#endif

#ifdef RTL8192SE
	// Support Bit 32-37(Assign as Bit 0-5) interrupt setting now */
	write_nic_dword(dev,INTA_MASK+4, priv->irq_mask[1]&0x3F);
#endif

}

void rtl8192_irq_disable(struct net_device *dev)
{
	struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);

#ifdef RTL8192CE
	write_nic_dword(dev, REG_HIMR, IMR8190_DISABLED);
#else
	write_nic_dword(dev,INTA_MASK,0);
#endif

#ifdef RTL8192SE
	write_nic_dword(dev,INTA_MASK + 4,0);//added by amy 090407
#endif
	priv->irq_enabled = 0;
}

void rtl8192_irq_clear(struct net_device *dev)
{
	u32 tmp = 0;
#ifdef RTL8192CE
	tmp = read_nic_dword(dev, REG_HISR);
	write_nic_dword(dev, REG_HISR, tmp);
#else
	tmp = read_nic_dword(dev, ISR);
	write_nic_dword(dev, ISR, tmp);
#endif

#ifdef RTL8192SE
	tmp = read_nic_dword(dev, ISR+4);
	write_nic_dword(dev, ISR+4, tmp);
#endif
}


void rtl8192_set_chan(struct net_device *dev,short ch)
{
    struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);

    RT_TRACE(COMP_CH, "=====>%s()====ch:%d\n", __FUNCTION__, ch);
    //These are for MP
    if (priv->chan_forced)
		return;
    //These are for MP

    priv->chan = ch;

    // this hack should avoid frame TX during channel setting*/
    //	tx = read_nic_dword(dev,TX_CONF);
    //	tx &= ~TX_LOOPBACK_MASK;

#ifndef LOOP_TEST
    //TODO
    //	write_nic_dword(dev,TX_CONF, tx |( TX_LOOPBACK_MAC<<TX_LOOPBACK_SHIFT));

    //need to implement rf set channel here WB

    if (priv->rf_set_chan)
        priv->rf_set_chan(dev,priv->chan);
    //	mdelay(10);
    //	write_nic_dword(dev,TX_CONF,tx | (TX_LOOPBACK_NONE<<TX_LOOPBACK_SHIFT));
#endif
}

#ifndef RTL8192CE
void rtl8192_update_msr(struct net_device *dev)
{
	struct r8192_priv *priv = rtllib_priv(dev);
	u8 msr;
	LED_CTL_MODE	LedAction = LED_CTL_NO_LINK;//added by amy 090316 for LED
	msr  = read_nic_byte(dev, MSR);
	msr &= ~ MSR_LINK_MASK;

	// do not change in link_state != WLAN_LINK_ASSOCIATED.
	// msr must be updated if the state is ASSOCIATING.
	// this is intentional and make sense for ad-hoc and
	// master (see the create BSS/IBSS func)
	//
	switch (priv->rtllib->iw_mode) {
	case IW_MODE_INFRA:
		if (priv->rtllib->state == RTLLIB_LINKED)
			msr |= (MSR_LINK_MANAGED << MSR_LINK_SHIFT);
		else
			msr |= (MSR_LINK_NONE << MSR_LINK_SHIFT);
		LedAction = LED_CTL_LINK;
		break;
	case IW_MODE_ADHOC:
		if (priv->rtllib->state == RTLLIB_LINKED)
			msr |= (MSR_LINK_ADHOC << MSR_LINK_SHIFT);
		else
			msr |= (MSR_LINK_NONE << MSR_LINK_SHIFT);
		break;
	case IW_MODE_MASTER:
		if (priv->rtllib->state == RTLLIB_LINKED)
			msr |= (MSR_LINK_MASTER << MSR_LINK_SHIFT);
		else
			msr |= (MSR_LINK_NONE << MSR_LINK_SHIFT);
		break;
#ifdef _RTL8192_EXT_PATCH_
	case IW_MODE_MESH:
		printk("%s: only_mesh=%d state=%d\n", __FUNCTION__,
				priv->rtllib->only_mesh, priv->rtllib->mesh_state);
		if (priv->rtllib->only_mesh) {
			if (priv->rtllib->mesh_state == RTLLIB_MESH_LINKED)
				msr |= (MSR_LINK_MASTER<<MSR_LINK_SHIFT); //AMY,090505
			else
				msr |= (MSR_LINK_NONE<<MSR_LINK_SHIFT);
		} else {
			if (priv->rtllib->mesh_state == RTLLIB_MESH_LINKED) {
				msr |= (MSR_LINK_ADHOC << MSR_LINK_SHIFT);
				if (priv->rtllib->state == RTLLIB_LINKED)
					msr |= (MSR_LINK_MANAGED << MSR_LINK_SHIFT);
			} else {
				msr |= (MSR_LINK_NONE << MSR_LINK_SHIFT);
			}
		}
		break;
#endif
	default:
		break;
	}

	write_nic_byte(dev, MSR, msr);
	//added by amy for LED 090317
	if(priv->rtllib->LedControlHandler)
		priv->rtllib->LedControlHandler(dev, LedAction);
}
#endif

void rtl8192_update_cap(struct net_device* dev, u16 cap)
{
	struct r8192_priv *priv = rtllib_priv(dev);
	struct rtllib_network *net = &priv->rtllib->current_network;

	priv->short_preamble = cap & WLAN_CAPABILITY_SHORT_PREAMBLE;

#ifdef RTL8192CE
	// Check preamble mode, 2005.01.06, by rcnjko.
	// Mark to update preamble value forever, 2008.03.18 by lanhsin
	{
		bool		ShortPreamble;

		if(cap & WLAN_CAPABILITY_SHORT_PREAMBLE)
		{ // Short Preamble
			//if(pMgntInfo->dot11CurrentPreambleMode != PREAMBLE_SHORT) // PREAMBLE_LONG or PREAMBLE_AUTO
			{
				ShortPreamble = true;
				//pMgntInfo->dot11CurrentPreambleMode = PREAMBLE_SHORT;
				priv->rtllib->SetHwRegHandler( dev, HW_VAR_ACK_PREAMBLE, (unsigned char *)&ShortPreamble );
			}
		}
		else
		{ // Long Preamble
			//if(pMgntInfo->dot11CurrentPreambleMode != PREAMBLE_LONG)  // PREAMBLE_SHORT or PREAMBLE_AUTO
			{
				ShortPreamble = false;
				//pMgntInfo->dot11CurrentPreambleMode = PREAMBLE_LONG;
				priv->rtllib->SetHwRegHandler( dev, HW_VAR_ACK_PREAMBLE, (unsigned char *)&ShortPreamble );
			}
		}
	}

	// STA's shall set MAC variable aSlotTime to short slot value upon transmission or
	// reception of Beacon, ProbeRsp, AssocRsp, and ReAssocRsp from the BSS that the
	// STA has joined or started... (ref 802.11g/D8.2, sec. 7.3.1, p12.).
	if( net->mode & IEEE_G)
	{
		u8	slot_time_val;
		u8	CurSlotTime;
		priv->rtllib->GetHwRegHandler( dev, HW_VAR_SLOT_TIME, &(CurSlotTime) );

		if( (cap & WLAN_CAPABILITY_SHORT_SLOT_TIME) && (!(priv->rtllib->pHTInfo->RT2RT_HT_Mode & RT_HT_CAP_USE_LONG_PREAMBLE)))
		{ // Short Slot Time
			if(CurSlotTime != SHORT_SLOT_TIME)
			{
				slot_time_val = SHORT_SLOT_TIME;
				priv->rtllib->SetHwRegHandler( dev, HW_VAR_SLOT_TIME, &slot_time_val );
			}
		}
		else
		{ // Long Slot Time
			if(CurSlotTime != NON_SHORT_SLOT_TIME)
			{
				slot_time_val = NON_SHORT_SLOT_TIME;
				priv->rtllib->SetHwRegHandler( dev, HW_VAR_SLOT_TIME, &slot_time_val );
			}
		}
	}

#else //will done like RTL8192CE
	{

		u32 tmp = 0;
	tmp = priv->basic_rate;
	if (priv->short_preamble)
		tmp |= BRSR_AckShortPmb;
#ifndef RTL8192SE	//as 92se RRSR is no longer dw align
	write_nic_dword(dev, RRSR, tmp);
#endif

#define SHORT_SLOT_TIME 9
#define NON_SHORT_SLOT_TIME 20
	if (net->mode & (IEEE_G|IEEE_N_24G)) {
		u8 slot_time = 0;
		if ((cap & WLAN_CAPABILITY_SHORT_SLOT_TIME) &&
		    (!priv->rtllib->pHTInfo->bCurrentRT2RTLongSlotTime)) {
			//short slot time
			slot_time = SHORT_SLOT_TIME;
		} else {
			//long slot time
			slot_time = NON_SHORT_SLOT_TIME;
		}
		priv->slot_time = slot_time;
		write_nic_byte(dev, SLOT_TIME, slot_time);
	}
	}
#endif

}

static struct rtllib_qos_parameters def_qos_parameters = {
        {3,3,3,3},// cw_min */
        {7,7,7,7},// cw_max */
        {2,2,2,2},// aifs */
        {0,0,0,0},// flags */
        {0,0,0,0} // tx_op_limit */
};

void rtl8192_update_beacon(void *data)
{
#if LINUX_VERSION_CODE >=KERNEL_VERSION(2,6,20)
	struct r8192_priv *priv = container_of_work_rsl(data, struct r8192_priv, update_beacon_wq.work);
	struct net_device *dev = priv->rtllib->dev;
#else
	struct net_device *dev = (struct net_device *)data;
        struct r8192_priv *priv = rtllib_priv(dev);
#endif
	struct rtllib_device* ieee = priv->rtllib;
	struct rtllib_network* net = &ieee->current_network;

	if (ieee->pHTInfo->bCurrentHTSupport)
		HTUpdateSelfAndPeerSetting(ieee, net);
	ieee->pHTInfo->bCurrentRT2RTLongSlotTime = net->bssht.bdRT2RTLongSlotTime;
	// Joseph test for turbo mode with AP
	ieee->pHTInfo->RT2RT_HT_Mode = net->bssht.RT2RT_HT_Mode;
	rtl8192_update_cap(dev, net->capability);
}

//
//background support to run QoS activate functionality
//
#ifdef RTL8192CE
int WDCAPARA_ADD[] = {REG_EDCA_BE_PARAM,REG_EDCA_BK_PARAM,REG_EDCA_VI_PARAM,REG_EDCA_VO_PARAM};
#else
int WDCAPARA_ADD[] = {EDCAPARA_BE,EDCAPARA_BK,EDCAPARA_VI,EDCAPARA_VO};
#endif
void rtl8192_qos_activate(void *data)
{
#if LINUX_VERSION_CODE >=KERNEL_VERSION(2,6,20)
	struct r8192_priv *priv = container_of_work_rsl(data, struct r8192_priv, qos_activate);
	struct net_device *dev = priv->rtllib->dev;
#else
	struct net_device *dev = (struct net_device *)data;
	struct r8192_priv *priv = rtllib_priv(dev);
#endif
#ifndef RTL8192CE
        struct rtllib_qos_parameters *qos_parameters = &priv->rtllib->current_network.qos_data.parameters;
        u8 mode = priv->rtllib->current_network.mode;
	u8  u1bAIFS;
	u32 u4bAcParam;
#endif
        int i;

        if (priv == NULL)
                return;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16))
	down(&priv->mutex);
#else
        mutex_lock(&priv->mutex);
#endif
        if(priv->rtllib->state != RTLLIB_LINKED)
		goto success;
	RT_TRACE(COMP_QOS,"qos active process with associate response received\n");
	// It better set slot time at first
	// For we just support b/g mode at present, let the slot time at 9/20 selection
	// update the ac parameter to related registers

	for (i = 0; i <  QOS_QUEUE_NUM; i++) {
#ifdef RTL8192CE
		priv->rtllib->SetHwRegHandler(dev, HW_VAR_AC_PARAM, (u8*)(&i));
#else //will done like RTL8192CE
		//Mode G/A: slotTimeTimer = 9; Mode B: 20
		u1bAIFS = qos_parameters->aifs[i] * ((mode&(IEEE_G|IEEE_N_24G)) ?9:20) + aSifsTime;
		u4bAcParam = ((((u32)(qos_parameters->tx_op_limit[i]))<< AC_PARAM_TXOP_LIMIT_OFFSET)|
				(((u32)(qos_parameters->cw_max[i]))<< AC_PARAM_ECW_MAX_OFFSET)|
				(((u32)(qos_parameters->cw_min[i]))<< AC_PARAM_ECW_MIN_OFFSET)|
				((u32)u1bAIFS << AC_PARAM_AIFS_OFFSET));
		printk("===>u4bAcParam:%x, ", u4bAcParam);
		write_nic_dword(dev, WDCAPARA_ADD[i], u4bAcParam);
#endif
	}

success:
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16))
	up(&priv->mutex);
#else
        mutex_unlock(&priv->mutex);
#endif
}

static int rtl8192_qos_handle_probe_response(struct r8192_priv *priv,
		int active_network,
		struct rtllib_network *network)
{
	int ret = 0;
	u32 size = sizeof(struct rtllib_qos_parameters);

	if(priv->rtllib->state !=RTLLIB_LINKED)
                return ret;

#ifdef _RTL8192_EXT_PATCH_
	if (!((priv->rtllib->iw_mode == IW_MODE_INFRA ) ||
	      ((priv->rtllib->iw_mode == IW_MODE_MESH) && (priv->rtllib->only_mesh == 0))))
#else
	if ((priv->rtllib->iw_mode != IW_MODE_INFRA))
#endif
		return ret;

	if (network->flags & NETWORK_HAS_QOS_MASK) {
		if (active_network &&
				(network->flags & NETWORK_HAS_QOS_PARAMETERS))
			network->qos_data.active = network->qos_data.supported;

		if ((network->qos_data.active == 1) && (active_network == 1) &&
				(network->flags & NETWORK_HAS_QOS_PARAMETERS) &&
				(network->qos_data.old_param_count !=
				 network->qos_data.param_count)) {
			network->qos_data.old_param_count =
				network->qos_data.param_count;
                         priv->rtllib->wmm_acm = network->qos_data.wmm_acm;
			queue_work_rsl(priv->priv_wq, &priv->qos_activate);
			RT_TRACE (COMP_QOS, "QoS parameters change call "
					"qos_activate\n");
		}
	} else {
		memcpy(&priv->rtllib->current_network.qos_data.parameters,\
		       &def_qos_parameters, size);

		if ((network->qos_data.active == 1) && (active_network == 1)) {
			queue_work_rsl(priv->priv_wq, &priv->qos_activate);
			RT_TRACE(COMP_QOS, "QoS was disabled call qos_activate \n");
		}
		network->qos_data.active = 0;
		network->qos_data.supported = 0;
	}

	return 0;
}

// handle manage frame frame beacon and probe response */
static int rtl8192_handle_beacon(struct net_device * dev,
                              struct rtllib_beacon * beacon,
                              struct rtllib_network * network)
{
	struct r8192_priv *priv = rtllib_priv(dev);

	rtl8192_qos_handle_probe_response(priv,1,network);

	queue_delayed_work_rsl(priv->priv_wq, &priv->update_beacon_wq, 0);
	return 0;

}

//
//handling the beaconing responses. if we get different QoS setting
//off the network from the associated setting, adjust the QoS
//setting
static int rtl8192_qos_association_resp(struct r8192_priv *priv,
                                    struct rtllib_network *network)
{
        int ret = 0;
        unsigned long flags;
        u32 size = sizeof(struct rtllib_qos_parameters);
        int set_qos_param = 0;

        if ((priv == NULL) || (network == NULL))
                return ret;

	if(priv->rtllib->state !=RTLLIB_LINKED)
                return ret;

#ifdef _RTL8192_EXT_PATCH_
	if (!((priv->rtllib->iw_mode == IW_MODE_INFRA ) ||
	      ((priv->rtllib->iw_mode == IW_MODE_MESH) && (priv->rtllib->only_mesh == 0))))
#else
	if ((priv->rtllib->iw_mode != IW_MODE_INFRA))
#endif
                return ret;

        spin_lock_irqsave(&priv->rtllib->lock, flags);
	if (network->flags & NETWORK_HAS_QOS_PARAMETERS) {
		memcpy(&priv->rtllib->current_network.qos_data.parameters,\
			 &network->qos_data.parameters,\
			sizeof(struct rtllib_qos_parameters));
		priv->rtllib->current_network.qos_data.active = 1;
                priv->rtllib->wmm_acm = network->qos_data.wmm_acm;
#if 0
		if((priv->rtllib->current_network.qos_data.param_count != \
					network->qos_data.param_count))
#endif
		 {
                        set_qos_param = 1;
			// update qos parameter for current network */
			priv->rtllib->current_network.qos_data.old_param_count = \
				 priv->rtllib->current_network.qos_data.param_count;
			priv->rtllib->current_network.qos_data.param_count = \
				 network->qos_data.param_count;
		}
        } else {
		memcpy(&priv->rtllib->current_network.qos_data.parameters,\
		       &def_qos_parameters, size);
		priv->rtllib->current_network.qos_data.active = 0;
		priv->rtllib->current_network.qos_data.supported = 0;
                set_qos_param = 1;
        }

        spin_unlock_irqrestore(&priv->rtllib->lock, flags);

	RT_TRACE(COMP_QOS, "%s: network->flags = %d,%d\n", __FUNCTION__,
			network->flags ,priv->rtllib->current_network.qos_data.active);
	if (set_qos_param == 1) {
		dm_init_edca_turbo(priv->rtllib->dev);//YJ,add,090320, when roaming to another ap, bcurrent_turbo_EDCA should be resetted so that we can update edca tubo.
		queue_work_rsl(priv->priv_wq, &priv->qos_activate);
	}
        return ret;
}

static int rtl8192_handle_assoc_response(struct net_device *dev,
                                     struct rtllib_assoc_response_frame *resp,
                                     struct rtllib_network *network)
{
        struct r8192_priv *priv = rtllib_priv(dev);
        rtl8192_qos_association_resp(priv, network);
        return 0;
}

void rtl8192_prepare_beacon(struct r8192_priv *priv)
{
#ifdef _RTL8192_EXT_PATCH_
	struct net_device *dev = priv->rtllib->dev;
#endif
	struct sk_buff *skb;
	//unsigned long flags;
	cb_desc *tcb_desc;

	skb = rtllib_get_beacon(priv->rtllib);
	tcb_desc = (cb_desc *)(skb->cb + 8);
#ifdef _RTL8192_EXT_PATCH_
	memset(skb->cb, 0, sizeof(skb->cb));
#endif
        //printk("===========> %s\n", __FUNCTION__);
	//spin_lock_irqsave(&priv->tx_lock,flags);
	// prepare misc info for the beacon xmit */
	tcb_desc->queue_index = BEACON_QUEUE;
	// IBSS does not support HT yet, use 1M defautly */
	tcb_desc->data_rate = 2;
	tcb_desc->RATRIndex = 7;
	tcb_desc->bTxDisableRateFallBack = 1;
	tcb_desc->bTxUseDriverAssingedRate = 1;
#ifdef _RTL8192_EXT_PATCH_
	tcb_desc->bTxEnableFwCalcDur = 0;
	memcpy((unsigned char *)(skb->cb),&dev,sizeof(dev));
#endif
	skb_push(skb, priv->rtllib->tx_headroom);
	if(skb){
		rtl8192_tx(priv->rtllib->dev,skb);
	}
	//spin_unlock_irqrestore (&priv->tx_lock, flags);
}

void rtl8192_stop_beacon(struct net_device *dev)
{
	//rtl8192_beacon_disable(dev);
}

void rtl8192_config_rate(struct net_device* dev, u16* rate_config)
{
	 struct r8192_priv *priv = rtllib_priv(dev);
	 struct rtllib_network *net;
	 u8 i=0, basic_rate = 0;
#ifdef _RTL8192_EXT_PATCH_
	if(priv->rtllib->iw_mode == IW_MODE_MESH)
		net = & priv->rtllib->current_mesh_network;
	else
		net = & priv->rtllib->current_network;
#else
	net = & priv->rtllib->current_network;
#endif

	 for (i = 0; i < net->rates_len; i++) {
		 basic_rate = net->rates[i] & 0x7f;
		 switch (basic_rate) {
		 case MGN_1M:
			 *rate_config |= RRSR_1M;
			 break;
		 case MGN_2M:
			 *rate_config |= RRSR_2M;
			 break;
		 case MGN_5_5M:
			 *rate_config |= RRSR_5_5M;
			 break;
		 case MGN_11M:
			 *rate_config |= RRSR_11M;
			 break;
		 case MGN_6M:
			 *rate_config |= RRSR_6M;
			 break;
		 case MGN_9M:
			 *rate_config |= RRSR_9M;
			 break;
		 case MGN_12M:
			 *rate_config |= RRSR_12M;
			 break;
		 case MGN_18M:
			 *rate_config |= RRSR_18M;
			 break;
		 case MGN_24M:
			 *rate_config |= RRSR_24M;
			 break;
		 case MGN_36M:
			 *rate_config |= RRSR_36M;
			 break;
		 case MGN_48M:
			 *rate_config |= RRSR_48M;
			 break;
		 case MGN_54M:
			 *rate_config |= RRSR_54M;
			 break;
		 }
	 }

	 for (i = 0; i < net->rates_ex_len; i++) {
		 basic_rate = net->rates_ex[i] & 0x7f;
		 switch (basic_rate) {
		 case MGN_1M:
			 *rate_config |= RRSR_1M;
			 break;
		 case MGN_2M:
			 *rate_config |= RRSR_2M;
			 break;
		 case MGN_5_5M:
			 *rate_config |= RRSR_5_5M;
			 break;
		 case MGN_11M:
			 *rate_config |= RRSR_11M;
			 break;
		 case MGN_6M:
			 *rate_config |= RRSR_6M;
			 break;
		 case MGN_9M:
			 *rate_config |= RRSR_9M;
			 break;
		 case MGN_12M:
			 *rate_config |= RRSR_12M;
			 break;
		 case MGN_18M:
			 *rate_config |= RRSR_18M;
			 break;
		 case MGN_24M:
			 *rate_config |= RRSR_24M;
			 break;
		 case MGN_36M:
			 *rate_config |= RRSR_36M;
			 break;
		 case MGN_48M:
			 *rate_config |= RRSR_48M;
			 break;
		 case MGN_54M:
			 *rate_config |= RRSR_54M;
			 break;
		 }
	 }
}

bool GetNmodeSupportBySecCfg8190Pci(struct net_device *dev)
{
#ifdef RTL8192SE
	return true;
#else
	struct r8192_priv *priv = rtllib_priv(dev);
	struct rtllib_device *ieee = priv->rtllib;
	if (ieee->rtllib_ap_sec_type &&
	   (ieee->rtllib_ap_sec_type(priv->rtllib)&(SEC_ALG_WEP|SEC_ALG_TKIP))) {
		return false;
	} else {
		return true;
	}
#endif
}

void rtl8192_refresh_supportrate(struct r8192_priv* priv)
{
	struct rtllib_device* ieee = priv->rtllib;
	//we donot consider set support rate for ABG mode, only HT MCS rate is set here.
	if (ieee->mode == WIRELESS_MODE_N_24G || ieee->mode == WIRELESS_MODE_N_5G) {
		memcpy(ieee->Regdot11HTOperationalRateSet, ieee->RegHTSuppRateSet, 16);
		memcpy(ieee->Regdot11TxHTOperationalRateSet, ieee->RegHTSuppRateSet, 16);

#ifdef RTL8192CE
		if(priv->rf_type == RF_1T1R) {
			ieee->Regdot11HTOperationalRateSet[1] = 0;
		}
#endif

#ifdef RTL8192SE
		if(priv->rf_type == RF_1T1R) {
			ieee->Regdot11HTOperationalRateSet[1] = 0;
		}
		if(priv->rf_type == RF_1T1R || priv->rf_type == RF_1T2R)
		{
			ieee->Regdot11TxHTOperationalRateSet[1] = 0;
		}

            if(priv->rtllib->b1SSSupport == true) {
                ieee->Regdot11HTOperationalRateSet[1] = 0;
            }
#endif
		//RT_DEBUG_DATA(COMP_INIT, ieee->RegHTSuppRateSet, 16);
		//RT_DEBUG_DATA(COMP_INIT, ieee->Regdot11HTOperationalRateSet, 16);
	} else {
		memset(ieee->Regdot11HTOperationalRateSet, 0, 16);
	}
	return;
}

u8 rtl8192_getSupportedWireleeMode(struct net_device*dev)
{
	struct r8192_priv *priv = rtllib_priv(dev);
	u8 ret = 0;

	switch(priv->rf_chip) {
	case RF_8225:
	case RF_8256:
	case RF_6052:
	case RF_PSEUDO_11N:
		ret = (WIRELESS_MODE_N_24G|WIRELESS_MODE_G | WIRELESS_MODE_B);
		break;
	case RF_8258:
		ret = (WIRELESS_MODE_A | WIRELESS_MODE_N_5G);
		break;
	default:
		ret = WIRELESS_MODE_B;
		break;
	}
	return ret;
}

void rtl8192_SetWirelessMode(struct net_device* dev, u8 wireless_mode)
{
	struct r8192_priv *priv = rtllib_priv(dev);
	u8 bSupportMode = rtl8192_getSupportedWireleeMode(dev);

	printk("===>%s(), wireless_mode:0x%x, support_mode:0x%x\n", __FUNCTION__, wireless_mode, bSupportMode);
//lzm remove for we set B/G mode.
#if 0
	if(	(wireless_mode != WIRELESS_MODE_B) &&
		(wireless_mode != WIRELESS_MODE_G) &&
		(wireless_mode != WIRELESS_MODE_A) &&
		(wireless_mode != WIRELESS_MODE_AUTO) &&
		(wireless_mode != WIRELESS_MODE_N_24G) &&
		(wireless_mode != WIRELESS_MODE_N_5G) )
	{
		wireless_mode = WIRELESS_MODE_AUTO;
	}
#endif
	if ((wireless_mode == WIRELESS_MODE_AUTO) || ((wireless_mode & bSupportMode) == 0)) {
		if (bSupportMode & WIRELESS_MODE_N_24G) {
			wireless_mode = WIRELESS_MODE_N_24G;
		} else if (bSupportMode & WIRELESS_MODE_N_5G) {
			wireless_mode = WIRELESS_MODE_N_5G;
		} else if((bSupportMode & WIRELESS_MODE_A)) {
			wireless_mode = WIRELESS_MODE_A;
		} else if((bSupportMode & WIRELESS_MODE_G)) {
			wireless_mode = WIRELESS_MODE_G;
		} else if((bSupportMode & WIRELESS_MODE_B)) {
			wireless_mode = WIRELESS_MODE_B;
		} else {
			RT_TRACE(COMP_ERR, "%s(), No valid wireless mode supported (%x)!!!\n",
					__FUNCTION__, bSupportMode);
			wireless_mode = WIRELESS_MODE_B;
		}
	}

#ifdef _RTL8192_EXT_PATCH_
	if ((wireless_mode & WIRELESS_MODE_N_24G) == WIRELESS_MODE_N_24G)
		wireless_mode = WIRELESS_MODE_N_24G;
	else if((wireless_mode & WIRELESS_MODE_N_5G) == WIRELESS_MODE_N_5G)
		wireless_mode = WIRELESS_MODE_N_5G;
	else if ((wireless_mode & WIRELESS_MODE_A) == WIRELESS_MODE_A)
		wireless_mode = WIRELESS_MODE_A;
	else if ((wireless_mode & WIRELESS_MODE_G) == WIRELESS_MODE_G)
		wireless_mode = WIRELESS_MODE_G;
	else
		wireless_mode = WIRELESS_MODE_B;
#else
//set to G mode if AP is b,g mixed.
	if ((wireless_mode & (WIRELESS_MODE_B | WIRELESS_MODE_G)) == (WIRELESS_MODE_G | WIRELESS_MODE_B))
		wireless_mode = WIRELESS_MODE_G;
#endif

#ifdef RTL8192SE
	write_nic_word(dev, SIFS_OFDM, 0x0e0e); //
#endif
	priv->rtllib->mode = wireless_mode;

	if ((wireless_mode == WIRELESS_MODE_N_24G) ||  (wireless_mode == WIRELESS_MODE_N_5G)){
		priv->rtllib->pHTInfo->bEnableHT = 1;
                printk("<===%s(), wireless_mode:%x, bEnableHT = 1\n", __FUNCTION__,wireless_mode);
        }else{
		priv->rtllib->pHTInfo->bEnableHT = 0;
                printk("<===%s(), wireless_mode:%x, bEnableHT = 0\n", __FUNCTION__,wireless_mode);
        }
	RT_TRACE(COMP_INIT, "Current Wireless Mode is %x\n", wireless_mode);
	rtl8192_refresh_supportrate(priv);
}

bool GetHalfNmodeSupportByAPs819xPci(struct net_device* dev)
{
#ifdef RTL8192SE
	return false;
#else
	bool			Reval;
	struct r8192_priv* priv = rtllib_priv(dev);
	struct rtllib_device* ieee = priv->rtllib;

	if(ieee->bHalfWirelessN24GMode == true)
		Reval = true;
	else
		Reval =  false;

	return Reval;
#endif
}

//YJ,add,090518,for Keep Alive
#ifdef _RTL8192_EXT_PATCH_
#define KEEP_ALIVE_INTERVAL				20 // in seconds.
#define DEFAULT_KEEP_ALIVE_LEVEL			1
#endif

#if 1//def RTL8192CE
static void rtl8192_init_priv_handler(struct net_device* dev)
{
	struct r8192_priv *priv = rtllib_priv(dev);

	priv->rtllib->softmac_hard_start_xmit	= rtl8192_hard_start_xmit;
	priv->rtllib->set_chan				= rtl8192_set_chan;
	priv->rtllib->link_change			= priv->ops->link_change;
	priv->rtllib->softmac_data_hard_start_xmit = rtl8192_hard_data_xmit;
	priv->rtllib->data_hard_stop		= rtl8192_data_hard_stop;
	priv->rtllib->data_hard_resume		= rtl8192_data_hard_resume;
	priv->rtllib->check_nic_enough_desc	= check_nic_enough_desc;
	priv->rtllib->get_nic_desc_num		= get_nic_desc_num;
#ifdef _RTL8192_EXT_PATCH_
	priv->rtllib->set_mesh_key			= r8192_mesh_set_enc_ext;
#endif


	priv->rtllib->handle_assoc_response	= rtl8192_handle_assoc_response;
	priv->rtllib->handle_beacon		= rtl8192_handle_beacon;
	priv->rtllib->SetWirelessMode		= rtl8192_SetWirelessMode;

#ifdef ENABLE_LPS
	priv->rtllib->LeisurePSLeave		= LeisurePSLeave;
#endif//ENABLE_LPS

#ifdef RTL8192CE
	priv->rtllib->SetBWModeHandler		= PHY_SetBWMode8192C;
	priv->rf_set_chan						= PHY_SwChnl8192C;

	priv->rtllib->start_send_beacons	= rtl8192ce_SetBeaconRelatedRegisters;//+by david 081107
	priv->rtllib->stop_send_beacons		= rtl8192_stop_beacon;//+by david 081107

	priv->rtllib->sta_wake_up			= rtl8192_hw_wakeup;
	priv->rtllib->enter_sleep_state			= rtl8192_hw_to_sleep;
	priv->rtllib->ps_is_queue_empty		= rtl8192_is_tx_queue_empty;

	priv->rtllib->GetNmodeSupportBySecCfg = rtl8192ce_GetNmodeSupportBySecCfg;
	priv->rtllib->GetHalfNmodeSupportByAPsHandler = rtl8192ce_GetHalfNmodeSupportByAPs;

	//priv->rtllib->Adhoc_InitRateAdaptive	= Adhoc_InitRateAdaptive;
	//priv->rtllib->check_ht_cap			= NULL;//rtl8192se_check_ht_cap;
	priv->rtllib->SetHwRegHandler		= rtl8192ce_SetHwReg;
	priv->rtllib->GetHwRegHandler		= rtl8192ce_GetHwReg;
	priv->rtllib->SetFwCmdHandler		= rtl8192ce_phy_SetFwCmdIO;
	priv->rtllib->UpdateHalRAMaskHandler = rtl8192ce_UpdateHalRAMaskDummy;
	priv->rtllib->rtl_11n_user_show_rates = rtl8192_11n_user_show_rates;
#ifdef ENABLE_IPS
	priv->rtllib->rtllib_ips_leave_wq = rtllib_ips_leave_wq;//added by amy for IPS 090331
	priv->rtllib->rtllib_ips_leave	= rtllib_ips_leave;//added by amy for IPS 090331
#endif

	priv->rtllib->LedControlHandler			= LedControl8192CE;
#endif//RTL8192CE

#ifdef RTL8192SE
	priv->rtllib->SetBWModeHandler		= rtl8192_SetBWMode;
	priv->rf_set_chan						= rtl8192_phy_SwChnl;

#ifdef _RTL8192_EXT_PATCH_
	priv->rtllib->start_send_beacons = NULL;//rtl819xusb_beacon_tx;//-by amy 080604
	priv->rtllib->stop_send_beacons = NULL;//rtl8192_beacon_stop;//-by amy 080604
#else
	priv->rtllib->start_send_beacons = rtl8192_start_beacon;//+by david 081107
	priv->rtllib->stop_send_beacons = rtl8192_stop_beacon;//+by david 081107
#endif

	priv->rtllib->sta_wake_up = rtl8192_hw_wakeup;
	priv->rtllib->enter_sleep_state = rtl8192_hw_to_sleep;
	priv->rtllib->ps_is_queue_empty = rtl8192_is_tx_queue_empty;

	priv->rtllib->GetNmodeSupportBySecCfg = GetNmodeSupportBySecCfg8190Pci;
	priv->rtllib->GetHalfNmodeSupportByAPsHandler = GetHalfNmodeSupportByAPs819xPci;

	priv->rtllib->SetBeaconRelatedRegistersHandler = SetBeaconRelatedRegisters8192SE;
	priv->rtllib->Adhoc_InitRateAdaptive = Adhoc_InitRateAdaptive;
	priv->rtllib->check_ht_cap = rtl8192se_check_ht_cap;
	priv->rtllib->SetHwRegHandler = SetHwReg8192SE;
#ifndef _RTL8192_EXT_PATCH_
	priv->rtllib->GetHwRegHandler = GetHwReg8192SE;
#endif
	priv->rtllib->SetFwCmdHandler = rtl8192se_set_fw_cmd;
	priv->rtllib->UpdateHalRAMaskHandler = UpdateHalRAMask8192SE;
	priv->rtllib->rtl_11n_user_show_rates = rtl8192_11n_user_show_rates;
#ifdef ENABLE_IPS
	priv->rtllib->rtllib_ips_leave_wq = rtllib_ips_leave_wq;//added by amy for IPS 090331
	priv->rtllib->rtllib_ips_leave = rtllib_ips_leave;//added by amy for IPS 090331
#endif

	priv->rtllib->LedControlHandler = LedControl8192SE;
#endif//RTL8192SE

#ifdef RTL8192E
	priv->rtllib->SetBWModeHandler		= rtl8192_SetBWMode;
	priv->rf_set_chan						= rtl8192_phy_SwChnl;

	priv->rtllib->start_send_beacons = rtl8192_start_beacon;//+by david 081107
	priv->rtllib->stop_send_beacons = rtl8192_stop_beacon;//+by david 081107

	priv->rtllib->sta_wake_up = rtl8192_hw_wakeup;
	priv->rtllib->enter_sleep_state = rtl8192_hw_to_sleep;
	priv->rtllib->ps_is_queue_empty = rtl8192_is_tx_queue_empty;

	priv->rtllib->GetNmodeSupportBySecCfg = GetNmodeSupportBySecCfg8190Pci;
	priv->rtllib->GetHalfNmodeSupportByAPsHandler = GetHalfNmodeSupportByAPs819xPci;

	priv->rtllib->SetHwRegHandler = rtl8192e_SetHwReg;
	priv->rtllib->SetFwCmdHandler = NULL;
	priv->rtllib->InitialGainHandler = InitialGain819xPci;
#ifdef ENABLE_IPS
	priv->rtllib->rtllib_ips_leave_wq = rtllib_ips_leave_wq;//added by amy for IPS 090331
	priv->rtllib->rtllib_ips_leave = rtllib_ips_leave;//added by amy for IPS 090331
#endif

	priv->rtllib->LedControlHandler = NULL;
#endif//RTL8192E

#ifdef RTL8190P
	priv->rtllib->SetBWModeHandler		= rtl8192_SetBWMode;
	priv->rf_set_chan						= rtl8192_phy_SwChnl;

	priv->rtllib->start_send_beacons = rtl8192_start_beacon;//+by david 081107
	priv->rtllib->stop_send_beacons = rtl8192_stop_beacon;//+by david 081107

	priv->rtllib->GetNmodeSupportBySecCfg = GetNmodeSupportBySecCfg8190Pci;
	priv->rtllib->GetHalfNmodeSupportByAPsHandler = GetHalfNmodeSupportByAPs819xPci;

	priv->rtllib->SetHwRegHandler = rtl8192e_SetHwReg;
	priv->rtllib->SetFwCmdHandler = NULL;
	priv->rtllib->InitialGainHandler = InitialGain819xPci;
#ifdef ENABLE_IPS
	priv->rtllib->rtllib_ips_leave_wq = rtllib_ips_leave_wq;//added by amy for IPS 090331
	priv->rtllib->rtllib_ips_leave = rtllib_ips_leave;//added by amy for IPS 090331
#endif

	priv->rtllib->LedControlHandler = NULL;
#endif//RTL8190P
}
#endif

static void rtl8192_init_priv_variable(struct net_device* dev)
{
	struct r8192_priv *priv = rtllib_priv(dev);
#ifdef RTL8192SE
	PRT_POWER_SAVE_CONTROL	pPSC = (PRT_POWER_SAVE_CONTROL)(&(priv->rtllib->PowerSaveControl));
	int pos;
	u8 value;
#endif
	u8 i;
	priv->bDisableFrameBursting = 0;
	priv->bDMInitialGainEnable = 1;	// 1 turn on dm control initial gain, 0 turn off
//#ifdef POLLING_METHOD_FOR_RADIO
	priv->polling_timer_on = 0;//add for S3/S4
	priv->up_first_time = 1;
//#endif
	priv->blinked_ingpio = false;
	priv->bDriverIsGoingToUnload = false;
//	priv->card_8192 = type; //recored device type
	priv->being_init_adapter = false;
        priv->sw_radio_on = true;
	priv->bdisable_nic = false;//added by amy for IPS 090408
	priv->bfirst_init = false;
//	priv->txbuffsize = 1600;//1024;
//	priv->txfwbuffersize = 4096;
	priv->txringcount = 64;//32;
	//priv->txbeaconcount = priv->txringcount;
//	priv->txbeaconcount = 2;
	priv->rxbuffersize = 9100;//2048;//1024;
	priv->rxringcount = MAX_RX_COUNT;//64;
	priv->irq_enabled=0;
//	priv->rx_skb_complete = 1;
	priv->chan = 1; //set to channel 1
	priv->RegWirelessMode = WIRELESS_MODE_AUTO;
	priv->RegChannelPlan = 0xf;
	priv->nrxAMPDU_size = 0;
	priv->nrxAMPDU_aggr_num = 0;
	priv->last_rxdesc_tsf_high = 0;
	priv->last_rxdesc_tsf_low = 0;
	priv->rtllib->mode = WIRELESS_MODE_AUTO; //SET AUTO
	priv->rtllib->iw_mode = IW_MODE_INFRA;
	priv->rtllib->ieee_up=0;
	priv->retry_rts = DEFAULT_RETRY_RTS;
	priv->retry_data = DEFAULT_RETRY_DATA;
	priv->rtllib->rts = DEFAULT_RTS_THRESHOLD;
	priv->rtllib->rate = 110; //11 mbps
	priv->rtllib->short_slot = 1;
	priv->promisc = (dev->flags & IFF_PROMISC) ? 1:0;
	priv->bcck_in_ch14 = false;
	priv->bfsync_processing  = false;
	priv->CCKPresentAttentuation = 0;
	priv->rfa_txpowertrackingindex = 0;
	priv->rfc_txpowertrackingindex = 0;
	priv->CckPwEnl = 6;
	priv->ScanDelay = 50;//for Scan TODO
	//added by amy for silent reset
	priv->ResetProgress = RESET_TYPE_NORESET;
	priv->bForcedSilentReset = 0;
	priv->bDisableNormalResetCheck = false;
	priv->force_reset = false;
	memset(priv->rtllib->swcamtable,0,sizeof(SW_CAM_TABLE)*32);//added by amy for silent reset 090415
#ifdef _RTL8192_EXT_PATCH_
	priv->rtllib->mesh_security_setting = 0;
	memset(priv->rtllib->swmeshcamtable,0,sizeof(SW_CAM_TABLE)*32);//added by amy for silent reset 090415
	priv->rtllib->mesh_sec_type = 0;
#endif
	memset(&priv->InterruptLog,0,sizeof(LOG_INTERRUPT_8190_T));//added by amy for 92se silent reset 090414
	priv->RxCounter = 0;//added by amy for 92se silent reset 090414
	//added by amy for power save
        priv->rtllib->wx_set_enc = 0;
	priv->bHwRadioOff = false;
	priv->RegRfOff = 0;
	priv->isRFOff = false;//added by amy for IPS 090331
	priv->bInPowerSaveMode = false;//added by amy for IPS 090331
	priv->rtllib->RfOffReason = 0;
	priv->RFChangeInProgress = false;
	priv->bHwRfOffAction = 0;
	priv->SetRFPowerStateInProgress = false;
	priv->rtllib->PowerSaveControl.bInactivePs = true;
	priv->rtllib->PowerSaveControl.bIPSModeBackup = false;
	priv->rtllib->PowerSaveControl.bLeisurePs = true;//added by amy for Leisure PS 090402
	priv->rtllib->PowerSaveControl.bFwCtrlLPS = false;//added by amy for Leisure PS 090402
	priv->rtllib->LPSDelayCnt = 0;
        priv->rtllib->sta_sleep = 0;
	priv->rtllib->eRFPowerState = eRfOn;
	//YJ,add,090518,for Keep Alive
#ifdef _RTL8192_EXT_PATCH_
	priv->NumTxUnicast = 0;
	priv->keepAliveLevel = DEFAULT_KEEP_ALIVE_LEVEL;
#endif
	//YJ,add,090518,end
	//just for debug
	priv->txpower_checkcnt = 0;
	priv->thermal_readback_index =0;
	priv->txpower_tracking_callback_cnt = 0;
	priv->ccktxpower_adjustcnt_ch14 = 0;
	priv->ccktxpower_adjustcnt_not_ch14 = 0;
	priv->pwrGroupCnt = 0;
#ifdef _RTL8192_EXT_PATCH_
	priv->FwCmdIOMap = 0;//added by amy 090709 for FW CMD
	priv->FwCmdIOParam = 0;//added by amy 090709 for FW CMD
	priv->ThermalValue = 0;//added by amy 090709 for FW CMD
	priv->DMFlag = 0;
	priv->rssi_level = 0;
	priv->rtllib->bUseRAMask = 0;
#endif
	// Power save, enalbe L0/L1 level ASPM for rtl8191se */
#if defined RTL8192SE
	//added by amy for adhoc 090402
	for(i = 0; i<PEER_MAX_ASSOC; i++)
		priv->rtllib->peer_assoc_list[i]=NULL;
	priv->RATRTableBitmap = 0;
	priv->rtllib->amsdu_in_process = 0;
	//added by amy for adhoc 090402 end

	// check the bridge vendor of pcie bridge */
	priv->bridge_pdev = priv->pdev->bus->self;
	switch (priv->bridge_pdev->vendor) {
	case PCI_VENDOR_ID_INTEL:
		priv->pci_bridge_vendor = PCI_BRIDGE_VENDOR_INTEL;
			break;
	case PCI_VENDOR_ID_SI:
		priv->pci_bridge_vendor = PCI_BRIDGE_VENDOR_SIS;
			break;
	default:
		priv->pci_bridge_vendor = 0;
		break;
	}

#ifdef RTL8192SE_CONFIG_ASPM_OR_D3
	if (priv->pci_bridge_vendor & (PCI_BRIDGE_VENDOR_INTEL | PCI_BRIDGE_VENDOR_SIS)) {
		priv->aspm_clkreq_enable = true;
		// Setting for PCI-E bridge */
		priv->RegHostPciASPMSetting = 0x02;
		pos = pci_find_capability(priv->bridge_pdev, PCI_CAP_ID_EXP);
		priv->PciBridgeASPMRegOffset = pos + PCI_EXP_LNKCTL;
		pci_read_config_byte(priv->bridge_pdev, pos + PCI_EXP_LNKCTL, &value);
		priv->PciBridgeLinkCtrlReg = value;

		// Setting for PCI-E device */
		priv->RegDevicePciASPMSetting = 0x03;
		pos = pci_find_capability(priv->pdev, PCI_CAP_ID_EXP);
		priv->ASPMRegOffset = pos + PCI_EXP_LNKCTL;
		priv->ClkReqOffset = pos + PCI_EXP_LNKCTL + 1;
		pci_read_config_byte(priv->pdev, pos + PCI_EXP_LNKCTL, &value);
		priv->LinkCtrlReg = value;
		//printk("pos = %x, PlatformSwitchASPM= %x\n",pos+PCI_EXP_LNKCTL, value);
	} else {
		priv->aspm_clkreq_enable = false;
	}
#endif
#endif
	priv->rtllib->current_network.beacon_interval = DEFAULT_BEACONINTERVAL;
	priv->rtllib->iw_mode = IW_MODE_INFRA;
#ifdef _ENABLE_SW_BEACON
	priv->rtllib->softmac_features  = IEEE_SOFTMAC_SCAN |
		IEEE_SOFTMAC_ASSOCIATE | IEEE_SOFTMAC_PROBERQ |
		IEEE_SOFTMAC_PROBERS | IEEE_SOFTMAC_TX_QUEUE  |
		IEEE_SOFTMAC_BEACONS;
#else
	priv->rtllib->softmac_features  = IEEE_SOFTMAC_SCAN |
		IEEE_SOFTMAC_ASSOCIATE | IEEE_SOFTMAC_PROBERQ |
		IEEE_SOFTMAC_PROBERS | IEEE_SOFTMAC_TX_QUEUE /* |
		IEEE_SOFTMAC_BEACONS*/;//added by amy 080604 //|  //IEEE_SOFTMAC_SINGLE_QUEUE;
#endif
	priv->rtllib->active_scan = 1;
	//added by amy 090313
	priv->rtllib->be_scan_inprogress = false;
	priv->rtllib->modulation = RTLLIB_CCK_MODULATION | RTLLIB_OFDM_MODULATION;
	priv->rtllib->host_encrypt = 1;
	priv->rtllib->host_decrypt = 1;

#if defined RTL8192SE || defined RTL8192CE
	priv->rtllib->tx_headroom = 0;
#else
	priv->rtllib->tx_headroom = sizeof(TX_FWINFO_8190PCI);
#endif
	priv->rtllib->qos_support = 1;
	priv->rtllib->dot11PowerSaveMode = eActive;

	priv->rtllib->fts = DEFAULT_FRAG_THRESHOLD;
	priv->rtllib->MaxMssDensity = 0;//added by amy 090330
	priv->rtllib->MinSpaceCfg = 0;//added by amy 090330

#if 1//def RTL8192CE
	rtl8192_init_priv_handler(dev);
#else
	priv->rtllib->softmac_hard_start_xmit = rtl8192_hard_start_xmit;
	priv->rtllib->set_chan = rtl8192_set_chan;
	priv->rtllib->link_change = priv->ops->link_change;
	priv->rtllib->softmac_data_hard_start_xmit = rtl8192_hard_data_xmit;
	priv->rtllib->data_hard_stop = rtl8192_data_hard_stop;
	priv->rtllib->data_hard_resume = rtl8192_data_hard_resume;

	priv->rtllib->check_nic_enough_desc = check_nic_enough_desc;
	priv->rtllib->get_nic_desc_num = get_nic_desc_num;

#ifdef _RTL8192_EXT_PATCH_
	priv->rtllib->set_mesh_key = r8192_mesh_set_enc_ext;
#endif

//	priv->rtllib->SwChnlByTimerHandler = rtl8192_phy_SwChnl;
	priv->rtllib->SetBWModeHandler = rtl8192_SetBWMode;
	priv->rtllib->handle_assoc_response = rtl8192_handle_assoc_response;
	priv->rtllib->handle_beacon = rtl8192_handle_beacon;
#ifndef RTL8190P
	priv->rtllib->sta_wake_up = rtl8192_hw_wakeup;
//	priv->rtllib->ps_request_tx_ack = rtl8192_rq_tx_ack;
	priv->rtllib->enter_sleep_state = rtl8192_hw_to_sleep;
	priv->rtllib->ps_is_queue_empty = rtl8192_is_tx_queue_empty;
#else
	priv->rtllib->sta_wake_up = NULL;//modified by amy for Legacy PS 090401
//	priv->rtllib->ps_request_tx_ack = rtl8192_rq_tx_ack;
	priv->rtllib->enter_sleep_state = NULL;//modified by amy for Legacy PS 090401
	priv->rtllib->ps_is_queue_empty = NULL;//modified by amy for Legacy PS 090401
#endif

#ifdef _RTL8192_EXT_PATCH_
	priv->rtllib->start_send_beacons = NULL;//rtl819xusb_beacon_tx;//-by amy 080604
	priv->rtllib->stop_send_beacons = NULL;//rtl8192_beacon_stop;//-by amy 080604
#else
	priv->rtllib->start_send_beacons = rtl8192_start_beacon;//+by david 081107
	priv->rtllib->stop_send_beacons = rtl8192_stop_beacon;//+by david 081107
#endif

	priv->rtllib->GetNmodeSupportBySecCfg = GetNmodeSupportBySecCfg8190Pci;
	priv->rtllib->SetWirelessMode = rtl8192_SetWirelessMode;
	priv->rtllib->GetHalfNmodeSupportByAPsHandler = GetHalfNmodeSupportByAPs819xPci;

	//added by amy
#ifdef RTL8192SE
	priv->rtllib->SetBeaconRelatedRegistersHandler = SetBeaconRelatedRegisters8192SE;
	priv->rtllib->Adhoc_InitRateAdaptive = Adhoc_InitRateAdaptive;
	priv->rtllib->check_ht_cap = rtl8192se_check_ht_cap;
	priv->rtllib->SetHwRegHandler = SetHwReg8192SE;
#ifndef _RTL8192_EXT_PATCH_
	priv->rtllib->GetHwRegHandler = GetHwReg8192SE;
#endif
	priv->rtllib->SetFwCmdHandler = rtl8192se_set_fw_cmd;
	priv->rtllib->UpdateHalRAMaskHandler = UpdateHalRAMask8192SE;
	priv->rtllib->rtl_11n_user_show_rates = rtl8192_11n_user_show_rates;
#ifdef ENABLE_IPS
	priv->rtllib->rtllib_ips_leave_wq = rtllib_ips_leave_wq;//added by amy for IPS 090331
	priv->rtllib->rtllib_ips_leave = rtllib_ips_leave;//added by amy for IPS 090331
#endif
	//priv->rtllib->InitialGainHandler = PHY_InitialGain8192S;
#else
	priv->rtllib->SetHwRegHandler = NULL;
	priv->rtllib->SetFwCmdHandler = NULL;
	priv->rtllib->InitialGainHandler = InitialGain819xPci;
#ifdef ENABLE_IPS
	priv->rtllib->rtllib_ips_leave_wq = rtllib_ips_leave_wq;//added by amy for IPS 090331
	priv->rtllib->rtllib_ips_leave = rtllib_ips_leave;//added by amy for IPS 090331
#endif
#endif
#ifdef ENABLE_LPS
	priv->rtllib->LeisurePSLeave = LeisurePSLeave;
#endif
	//priv->rtllib->is_ap_in_wep_tkip = is_ap_in_wep_tkip;
#ifdef RTL8192SE
        priv->rtllib->LedControlHandler = LedControl8192SE;
#else
        priv->rtllib->LedControlHandler = NULL;
#endif

	priv->rf_set_chan = rtl8192_phy_SwChnl;

#endif//RTL8192CE
	priv->MidHighPwrTHR_L1 = 0x3B;
	priv->MidHighPwrTHR_L2 = 0x40;

	priv->card_type = PCI;

	priv->ShortRetryLimit = 0x30;
	priv->LongRetryLimit = 0x30;
	priv->EarlyRxThreshold = 7;
	priv->enable_gpio0 = 0;

#ifdef RTL8192CE
	priv->TransmitConfig = CFENDFORM;
#else
	priv->TransmitConfig = 0;
#endif
#ifdef RTL8192SE
	// Default Halt the NIC if RF is OFF.
	pPSC->RegRfPsLevel |= RT_RF_OFF_LEVL_HALT_NIC;
	pPSC->RegRfPsLevel |= RT_RF_OFF_LEVL_CLK_REQ;
	pPSC->RegRfPsLevel |= RT_RF_OFF_LEVL_ASPM;
	pPSC->RegRfPsLevel |= RT_RF_LPS_LEVEL_ASPM;

	pPSC->RegMaxLPSAwakeIntvl = 5;

	priv->ReceiveConfig =
	RCR_APPFCS | RCR_APWRMGT | /*RCR_ADD3 |*/
	RCR_AMF	| RCR_ADF | RCR_APP_MIC | RCR_APP_ICV |
       RCR_AICV	| RCR_ACRC32	|				// Accept ICV error, CRC32 Error
	RCR_AB		| RCR_AM		|				// Accept Broadcast, Multicast
	RCR_APM		|								// Accept Physical match
	/*RCR_AAP		|*/							// Accept Destination Address packets
	RCR_APP_PHYST_STAFF | RCR_APP_PHYST_RXFF |	// Accept PHY status
	(priv->EarlyRxThreshold<<RCR_FIFO_OFFSET)	;
	// Make reference from WMAC code 2006.10.02, maybe we should disable some of the interrupt. by Emily

#ifdef _ENABLE_SW_BEACON
	priv->irq_mask[0] =
	(IMR_ROK | IMR_VODOK | IMR_VIDOK | IMR_BEDOK | IMR_BKDOK |		\
	IMR_HCCADOK | IMR_MGNTDOK | IMR_COMDOK | IMR_HIGHDOK |					\
	IMR_BDOK | IMR_RXCMDOK | /*IMR_TIMEOUT0 |*/ IMR_RDU | IMR_RXFOVW/*	|			\
	IMR_BcnInt| IMR_TXFOVW | IMR_TBDOK | IMR_TBDER*/);
#else
	priv->irq_mask[0] =
	(IMR_ROK | IMR_VODOK | IMR_VIDOK | IMR_BEDOK | IMR_BKDOK |		\
	IMR_HCCADOK | IMR_MGNTDOK | IMR_COMDOK | IMR_HIGHDOK |					\
	IMR_BDOK | IMR_RXCMDOK | /*IMR_TIMEOUT0 |*/ IMR_RDU | IMR_RXFOVW	|		\
	 IMR_BcnInt/*| IMR_TXFOVW*/ /*| IMR_TBDOK | IMR_TBDER*/);
#endif
	priv->irq_mask[1] = 0;/* IMR_TBDOK | IMR_TBDER*/
#elif defined RTL8192E || defined RTL8190P
	priv->ReceiveConfig = RCR_ADD3	|
		RCR_AMF | RCR_ADF |		//accept management/data
		RCR_AICV |	//accept control frame for SW AP needs PS-poll, 2005.07.07, by rcnjko.
		RCR_AB | RCR_AM | RCR_APM |	//accept BC/MC/UC
		RCR_AAP | ((u32)7<<RCR_MXDMA_OFFSET) |
		((u32)7 << RCR_FIFO_OFFSET) | RCR_ONLYERLPKT;

	priv->irq_mask[0] =	(u32)(IMR_ROK | IMR_VODOK | IMR_VIDOK | IMR_BEDOK | IMR_BKDOK |\
				IMR_HCCADOK | IMR_MGNTDOK | IMR_COMDOK | IMR_HIGHDOK |\
				IMR_BDOK | IMR_RXCMDOK | IMR_TIMEOUT0 | IMR_RDU | IMR_RXFOVW	|\
				IMR_TXFOVW | IMR_BcnInt | IMR_TBDOK | IMR_TBDER);

#elif defined RTL8192CE
	priv->ReceiveConfig = (\
					RCR_APPFCS
					// | RCR_APWRMGT
					// |RCR_ADD3
					| RCR_AMF | RCR_ADF| RCR_APP_MIC| RCR_APP_ICV
					| RCR_AICV | RCR_ACRC32	// Accept ICV error, CRC32 Error
					| RCR_AB | RCR_AM			// Accept Broadcast, Multicast
					| RCR_APM					// Accept Physical match
					//| RCR_AAP					// Accept Destination Address packets
					| RCR_APP_PHYST_RXFF		// Accept PHY status
					| RCR_HTC_LOC_CTRL
					//(pHalData->EarlyRxThreshold<<RCR_FIFO_OFFSET)	);
					);

	priv->irq_mask[0] = (u32)(IMR_ROK | IMR_VODOK | IMR_VIDOK | IMR_BEDOK | IMR_BKDOK |		\
					IMR_MGNTDOK | IMR_HIGHDOK |					\
					IMR_BDOK | /*IMR_TIMEOUT0 |*/ IMR_RDU | IMR_RXFOVW	|			\
					/*IMR_BcnInt | */IMR_TBDOK | IMR_TBDER/* | IMR_TXFOVW*/ /*| IMR_TBDOK | IMR_TBDER*/);

	priv->irq_mask[1] = 0;/* IMR_TBDOK | IMR_TBDER*/
#endif

	priv->AcmControl = 0;
	priv->pFirmware = (rt_firmware*)vmalloc(sizeof(rt_firmware));
	if (priv->pFirmware)
	memset(priv->pFirmware, 0, sizeof(rt_firmware));

	// rx related queue */
        skb_queue_head_init(&priv->rx_queue);
	skb_queue_head_init(&priv->skb_queue);

	// Tx related queue */
	for(i = 0; i < MAX_QUEUE_SIZE; i++) {
		skb_queue_head_init(&priv->rtllib->skb_waitQ [i]);
	}
	for(i = 0; i < MAX_QUEUE_SIZE; i++) {
		skb_queue_head_init(&priv->rtllib->skb_aggQ [i]);
	}
	//priv->rf_set_chan = rtl8192_phy_SwChnl;

#ifdef _RTL8192_EXT_PATCH_
	priv->rtllib->set_key_for_AP = rtl8192_set_key_for_AP;
	memset(priv->rtllib->swmeshratrtable,0,8*(sizeof(SW_RATR_TABLE)));
	priv->rtllib->mesh_amsdu_in_process = 0;
	priv->rtllib->HwSecCamBitMap = 0;
	memset(priv->rtllib->HwSecCamStaAddr,0,TOTAL_CAM_ENTRY * ETH_ALEN);
	priv->rtllib->LinkingPeerBitMap = 0;
	memset(priv->rtllib->LinkingPeerAddr,0,TOTAL_CAM_ENTRY * ETH_ALEN);
	memset(priv->rtllib->peer_AID_Addr,0,30 * ETH_ALEN);
	priv->rtllib->peer_AID_bitmap = 0;
	priv->rtllib->backup_channel = 1;//AMY test 090220
	//added by amy
	priv->rtllib->del_hwsec_cam_entry = rtl8192_del_hwsec_cam_entry; //YJ,add,090213,for hw sec
	priv->rtllib->set_key_for_peer = meshdev_set_key_for_peer;//YJ,add,090213,for hw sec
	priv->rtllib->meshscanning = 0;
	priv->rtllib->hostname_len = 0;
	memset(priv->rtllib->hostname, 0, sizeof(priv->rtllib->hostname));
	priv->rtllib->meshScanMode = 0;
	priv->rtllib->currentRate = 0xffffffff;
	priv->mshobj = alloc_mshobj(priv);
	printk("priv is %p,mshobj is %p\n",priv,priv->mshobj);

	if (priv->mshobj) {
		priv->rtllib->ext_patch_rtllib_start_protocol =
			priv->mshobj->ext_patch_rtllib_start_protocol;
		priv->rtllib->ext_patch_rtllib_stop_protocol =
			priv->mshobj->ext_patch_rtllib_stop_protocol;
		priv->rtllib->ext_patch_rtllib_start_mesh =
			priv->mshobj->ext_patch_rtllib_start_mesh;
		priv->rtllib->ext_patch_rtllib_probe_req_1 =
			priv->mshobj->ext_patch_rtllib_probe_req_1;
		priv->rtllib->ext_patch_rtllib_probe_req_2 =
			priv->mshobj->ext_patch_rtllib_probe_req_2;
		priv->rtllib->ext_patch_rtllib_rx_frame_softmac_on_auth =
			priv->mshobj->ext_patch_rtllib_rx_frame_softmac_on_auth;
		priv->rtllib->ext_patch_rtllib_rx_frame_softmac_on_deauth =
			priv->mshobj->ext_patch_rtllib_rx_frame_softmac_on_deauth;
		priv->rtllib->ext_patch_rtllib_rx_frame_softmac_on_peerlink_open =
			priv->mshobj->ext_patch_rtllib_rx_frame_softmac_on_peerlink_open;
		priv->rtllib->ext_patch_rtllib_rx_frame_softmac_on_peerlink_confirm =
			priv->mshobj->ext_patch_rtllib_rx_frame_softmac_on_peerlink_confirm;
		priv->rtllib->ext_patch_rtllib_rx_frame_softmac_on_peerlink_close =
			priv->mshobj->ext_patch_rtllib_rx_frame_softmac_on_peerlink_close;
		priv->rtllib->ext_patch_rtllib_close_all_peerlink =
			priv->mshobj->ext_patch_rtllib_close_all_peerlink;
		priv->rtllib->ext_patch_rtllib_rx_frame_softmac_on_linkmetric_report =
			priv->mshobj->ext_patch_rtllib_rx_frame_softmac_on_linkmetric_report;
		priv->rtllib->ext_patch_rtllib_rx_frame_softmac_on_linkmetric_req =
			priv->mshobj->ext_patch_rtllib_rx_frame_softmac_on_linkmetric_req;
		priv->rtllib->ext_patch_rtllib_rx_frame_softmac_on_pathselect_preq =
			priv->mshobj->ext_patch_rtllib_rx_frame_softmac_on_pathselect_preq;
		priv->rtllib->ext_patch_rtllib_rx_frame_softmac_on_pathselect_prep =
			priv->mshobj->ext_patch_rtllib_rx_frame_softmac_on_pathselect_prep;
		priv->rtllib->ext_patch_rtllib_rx_frame_softmac_on_pathselect_perr =
			priv->mshobj->ext_patch_rtllib_rx_frame_softmac_on_pathselect_perr;
		priv->rtllib->ext_patch_rtllib_rx_frame_softmac_on_pathselect_rann =
			priv->mshobj->ext_patch_rtllib_rx_frame_softmac_on_pathselect_rann;
		priv->rtllib->ext_patch_rtllib_rx_frame_softmac_on_pathselect_pann =
			priv->mshobj->ext_patch_rtllib_rx_frame_softmac_on_pathselect_pann;
		priv->rtllib->ext_patch_rtllib_ext_stop_scan_wq_set_channel =
			priv->mshobj->ext_patch_rtllib_ext_stop_scan_wq_set_channel;
		priv->rtllib->ext_patch_r819x_wx_set_mesh_chan =
			priv->mshobj->ext_patch_r819x_wx_set_mesh_chan;
		priv->rtllib->ext_patch_r819x_wx_set_channel =
			priv->mshobj->ext_patch_r819x_wx_set_channel;
		priv->rtllib->ext_patch_rtllib_process_probe_response_1 =
			priv->mshobj->ext_patch_rtllib_process_probe_response_1;
		priv->rtllib->ext_patch_rtllib_rx_mgt_on_probe_req =
			priv->mshobj->ext_patch_rtllib_rx_mgt_on_probe_req;
		priv->rtllib->ext_patch_rtllib_rx_mgt_update_expire =
			priv->mshobj->ext_patch_rtllib_rx_mgt_update_expire;
		priv->rtllib->ext_patch_get_beacon_get_probersp =
			priv->mshobj->ext_patch_get_beacon_get_probersp;
		priv->rtllib->ext_patch_rtllib_rx_on_rx =
			priv->mshobj->ext_patch_rtllib_rx_on_rx;
		priv->rtllib->ext_patch_rtllib_rx_frame_get_hdrlen =
			priv->mshobj->ext_patch_rtllib_rx_frame_get_hdrlen;
		priv->rtllib->ext_patch_rtllib_rx_frame_get_mac_hdrlen =
			priv->mshobj->ext_patch_rtllib_rx_frame_get_mac_hdrlen;
		priv->rtllib->ext_patch_rtllib_rx_frame_get_mesh_hdrlen_llc =
			priv->mshobj->ext_patch_rtllib_rx_frame_get_mesh_hdrlen_llc;
		priv->rtllib->ext_patch_rtllib_rx_is_valid_framectl =
			priv->mshobj->ext_patch_rtllib_rx_is_valid_framectl;
		//priv->rtllib->ext_patch_rtllib_rx_process_dataframe =
		//      priv->mshobj->ext_patch_rtllib_rx_process_dataframe;
		// priv->rtllib->ext_patch_is_duplicate_packet =
		//      priv->mshobj->ext_patch_is_duplicate_packet;
		priv->rtllib->ext_patch_rtllib_softmac_xmit_get_rate =
			priv->mshobj->ext_patch_rtllib_softmac_xmit_get_rate;
		/* added by david for setting acl dynamically */
		priv->rtllib->ext_patch_rtllib_acl_query =
			priv->mshobj->ext_patch_rtllib_acl_query;
		priv->rtllib->ext_patch_rtllib_tx_data =
			priv->mshobj->ext_patch_rtllib_tx_data;
		priv->rtllib->ext_patch_rtllib_is_mesh =
			priv->mshobj->ext_patch_rtllib_is_mesh;
		priv->rtllib->ext_patch_rtllib_create_crypt_for_peer =
			priv->mshobj->ext_patch_rtllib_create_crypt_for_peer;
		priv->rtllib->ext_patch_rtllib_get_peermp_htinfo =
			priv->mshobj->ext_patch_rtllib_get_peermp_htinfo;
		priv->rtllib->ext_patch_rtllib_update_ratr_mask =
			priv->mshobj->ext_patch_rtllib_update_ratr_mask;
		priv->rtllib->ext_patch_rtllib_send_ath_commit =
			priv->mshobj->ext_patch_rtllib_send_ath_commit;
		priv->rtllib->ext_patch_rtllib_send_ath_confirm =
			priv->mshobj->ext_patch_rtllib_send_ath_confirm;
		priv->rtllib->ext_patch_rtllib_rx_ath_commit =
			priv->mshobj->ext_patch_rtllib_rx_ath_commit;
		priv->rtllib->ext_patch_rtllib_rx_ath_confirm =
			priv->mshobj->ext_patch_rtllib_rx_ath_confirm;
	}
	//added by amy for mesh AMSDU
	for (i = 0; i < MAX_QUEUE_SIZE; i++) {
		skb_queue_head_init(&priv->rtllib->skb_meshaggQ[i]);
	}
#endif // _RTL8187_EXT_PATCH_

}

//init lock here
static void rtl8192_init_priv_lock(struct r8192_priv* priv)
{
	spin_lock_init(&priv->tx_lock);
	spin_lock_init(&priv->irq_lock);//added by thomas
	spin_lock_init(&priv->irq_th_lock);
	spin_lock_init(&priv->rf_ps_lock);
	spin_lock_init(&priv->ps_lock);
	spin_lock_init(&priv->rf_lock);
#ifdef RTL8192SE_CONFIG_ASPM_OR_D3
	spin_lock_init(&priv->D3_lock);
#endif
	sema_init(&priv->wx_sem,1);
	sema_init(&priv->rf_sem,1);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16))
	sema_init(&priv->mutex, 1);
#else
	mutex_init(&priv->mutex);
#endif
}

//init tasklet and wait_queue here. only 2.6 above kernel is considered
static void rtl8192_init_priv_task(struct net_device* dev)
{
	struct r8192_priv *priv = rtllib_priv(dev);

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
#ifdef PF_SYNCTHREAD
	priv->priv_wq = create_workqueue(DRV_NAME,0);
#else
	priv->priv_wq = create_workqueue(DRV_NAME);
#endif
#endif
	INIT_WORK_RSL(&priv->reset_wq,  (void*)rtl8192_restart, dev);
#ifdef ENABLE_IPS
	INIT_WORK_RSL(&priv->rtllib->ips_leave_wq, (void*)IPSLeave_wq, dev);
#endif
	INIT_DELAYED_WORK_RSL(&priv->watch_dog_wq, (void*)rtl819x_watchdog_wqcallback, dev);
	INIT_DELAYED_WORK_RSL(&priv->txpower_tracking_wq,  (void*)dm_txpower_trackingcallback, dev);
	INIT_DELAYED_WORK_RSL(&priv->rfpath_check_wq,  (void*)dm_rf_pathcheck_workitemcallback, dev);
	INIT_DELAYED_WORK_RSL(&priv->update_beacon_wq, (void*)rtl8192_update_beacon, dev);
	INIT_WORK_RSL(&priv->qos_activate, (void*)rtl8192_qos_activate, dev);
#ifndef RTL8190P
	INIT_DELAYED_WORK_RSL(&priv->rtllib->hw_wakeup_wq,(void*) rtl8192_hw_wakeup_wq, dev);
	INIT_DELAYED_WORK_RSL(&priv->rtllib->hw_sleep_wq,(void*) rtl8192_hw_sleep_wq, dev);
#endif
#if defined RTL8192SE
	INIT_DELAYED_WORK_RSL(&priv->rtllib->check_tsf_wq,(void*)rtl8192se_check_tsf_wq, dev);
	INIT_DELAYED_WORK_RSL(&priv->rtllib->update_assoc_sta_info_wq,
			(void*)rtl8192se_update_peer_ratr_table_wq, dev);
#endif
#ifdef _RTL8192_EXT_PATCH_
	INIT_WORK_RSL(&priv->rtllib->ext_create_crypt_for_peers_wq, (void*)msh_create_crypt_for_peers_wq, dev);
	INIT_WORK_RSL(&priv->rtllib->ext_path_sel_ops_wq,(void*) path_sel_ops_wq, dev);
	INIT_WORK_RSL(&priv->rtllib->ext_update_extchnloffset_wq,
			(void*) meshdev_update_ext_chnl_offset_as_client, dev);
	INIT_DELAYED_WORK_RSL(&priv->rtllib->ext_wx_set_key_wq, (void*)ext_mesh_set_key_wq,priv->rtllib);
#endif
	tasklet_init(&priv->irq_rx_tasklet,
	     (void(*)(unsigned long))rtl8192_irq_rx_tasklet,
	     (unsigned long)priv);
	tasklet_init(&priv->irq_tx_tasklet,
	     (void(*)(unsigned long))rtl8192_irq_tx_tasklet,
	     (unsigned long)priv);
        tasklet_init(&priv->irq_prepare_beacon_tasklet,
                (void(*)(unsigned long))rtl8192_prepare_beacon,
                (unsigned long)priv);
}

//
//	Note:	dev->EEPROMAddressSize should be set before this function call.
//			EEPROM address size can be got through GetEEPROMSize8185()
//
short rtl8192_get_channel_map(struct net_device * dev)
{
	struct r8192_priv *priv = rtllib_priv(dev);
#ifdef ENABLE_DOT11D
	//acturally 8225 & 8256 rf chip only support B,G,24N mode
	if ((priv->rf_chip != RF_8225) && (priv->rf_chip != RF_8256)
			&& (priv->rf_chip != RF_6052)) {
		RT_TRACE(COMP_ERR, "%s: unknown rf chip, can't set channel map\n", __FUNCTION__);
		return -1;
	}

	if (priv->ChannelPlan > COUNTRY_CODE_MAX) {
		printk("rtl819x_init:Error channel plan! Set to default.\n");
		priv->ChannelPlan= COUNTRY_CODE_FCC;
	}
	RT_TRACE(COMP_INIT, "Channel plan is %d\n",priv->ChannelPlan);

	Dot11d_Init(priv->rtllib);
	Dot11d_Channelmap(priv->ChannelPlan, priv->rtllib);
#else
	int ch,i;
	//Set Default Channel Plan
	if(!channels){
		DMESG("No channels, aborting");
		return -1;
	}

	ch = channels;
	priv->ChannelPlan = 0;//hikaru
	 // set channels 1..14 allowed in given locale
	for (i = 1; i <= 14; i++) {
		(priv->rtllib->channel_map)[i] = (u8)(ch & 0x01);
		ch >>= 1;
	}
	priv->rtllib->IbssStartChnl= 10;
	priv->rtllib->ibss_maxjoin_chal = 14;
#endif
	return 0;
}

void check_rfctrl_gpio_timer(unsigned long data);
short rtl8192_init(struct net_device *dev)
{
	struct r8192_priv *priv = rtllib_priv(dev);

	memset(&(priv->stats),0,sizeof(struct Stats));
#ifdef CONFIG_MP
	rtl8192_init_mp(dev);
#endif
	rtl8192_init_priv_variable(dev);
	rtl8192_init_priv_lock(priv);
	rtl8192_init_priv_task(dev);
	priv->ops->get_eeprom_size(dev);
#ifdef RTL8190P
	strcpy(priv->nick, "rtl8190p");
#elif defined(RTL8192E)
	strcpy(priv->nick, "rtl8192E");
#elif defined RTL8192SE
	if (priv->rf_type == RF_1T1R)
		strcpy(priv->nick, "rtl8191SEVA1");
	else if (priv->rf_type == RF_1T2R)
		strcpy(priv->nick, "rtl8191SEVA2");
	else
		strcpy(priv->nick, "rtl8192SE");
#elif defined Rtl8192CE
	strcpy(priv->nick, "rtl8192CE");
#endif
	rtl8192_get_channel_map(dev);

#ifdef CONFIG_CRDA
	/* channel map setting for the cfg80211 style */
	{
		struct r8192_priv* priv = rtllib_priv(dev);
		struct rtllib_device* ieee = priv->rtllib;
		rtllib_set_geo(ieee, &rtl_geos[2]);
	}

#endif

	init_hal_dm(dev);

#if defined RTL8192SE || defined Rtl8192CE
	InitSwLeds(dev);
#endif
	setup_timer(&priv->watch_dog_timer,
		    watch_dog_timer_callback,
		    (unsigned long) dev);

	setup_timer(&priv->gpio_polling_timer,
		    check_rfctrl_gpio_timer,
		    (unsigned long)dev);

	rtl8192_irq_disable(dev);
#if defined(IRQF_SHARED)
        if (request_irq(dev->irq, (void*)rtl8192_interrupt, IRQF_SHARED, dev->name, dev)) {
#else
        if (request_irq(dev->irq, (void *)rtl8192_interrupt, SA_SHIRQ, dev->name, dev)) {
#endif
		printk("Error allocating IRQ %d",dev->irq);
		return -1;
	} else {
		priv->irq=dev->irq;
		printk("IRQ %d",dev->irq);
	}

	if (rtl8192_pci_initdescring(dev) != 0) {
		printk("Endopoints initialization failed");
		return -1;
	}

	return 0;
}

// this configures registers for beacon tx and enables it via
// rtl8192_beacon_tx_enable(). rtl8192_beacon_tx_disable() might
// be used to stop beacon transmission
//
#ifndef RTL8192CE
void rtl8192_start_beacon(struct net_device *dev)
{
	struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);
	struct rtllib_network *net = &priv->rtllib->current_network;
	u16 BcnTimeCfg = 0;
        u16 BcnCW = 6;
        u16 BcnIFS = 0xf;

	DMESG("Enabling beacon TX");
	//rtl8192_prepare_beacon(dev);
	rtl8192_irq_disable(dev);
	//rtl8192_beacon_tx_enable(dev);

	// ATIM window */
	write_nic_word(dev, ATIMWND, 2);

	// Beacon interval (in unit of TU) */
	write_nic_word(dev, BCN_INTERVAL, net->beacon_interval);
#ifdef _RTL8192_EXT_PATCH_
	PHY_SetBeaconHwReg(dev, net->beacon_interval);
#endif
	//
	// DrvErlyInt (in unit of TU).
	// (Time to send interrupt to notify driver to c
	// hange beacon content)
	//
#ifdef RTL8192SE
	write_nic_word(dev, BCN_DRV_EARLY_INT, 10<<4);
#else
	write_nic_word(dev, BCN_DRV_EARLY_INT, 10);
#endif
	//
	// BcnDMATIM(in unit of us).
	// Indicates the time before TBTT to perform beacon queue DMA
	//
	write_nic_word(dev, BCN_DMATIME, 256);

	//
	// Force beacon frame transmission even after receiving
	// beacon frame from other ad hoc STA
	//
	write_nic_byte(dev, BCN_ERR_THRESH, 100);

	// Set CW and IFS */
	BcnTimeCfg |= BcnCW<<BCN_TCFG_CW_SHIFT;
	BcnTimeCfg |= BcnIFS<<BCN_TCFG_IFS;
	write_nic_word(dev, BCN_TCFG, BcnTimeCfg);
	// enable the interrupt for ad-hoc process */
	rtl8192_irq_enable(dev);
}
#endif

/***************************************************************************
    -------------------------------WATCHDOG STUFF---------------------------
***************************************************************************/
short rtl8192_is_tx_queue_empty(struct net_device *dev)
{
	int i=0;
	struct r8192_priv *priv = rtllib_priv(dev);
	for (i=0; i<=MGNT_QUEUE; i++)
	{
		if ((i== TXCMD_QUEUE) || (i == HCCA_QUEUE) )
			continue;
		if (skb_queue_len(&(&priv->tx_ring[i])->queue) > 0){
			printk("===>tx queue is not empty:%d, %d\n", i, skb_queue_len(&(&priv->tx_ring[i])->queue));
			return 0;
		}
	}
	return 1;
}

bool HalTxCheckStuck819xPci(struct net_device *dev)
{
	struct r8192_priv *priv = rtllib_priv(dev);
	bool	bStuck = false;
#if defined(RTL8192E) || defined(RTL8190P)
	u16    RegTxCounter = read_nic_word(dev, 0x128);
#elif defined (RTL8192SE) || defined (RTL8192CE)
	u16	RegTxCounter = read_nic_word(dev, 0x366);
#else
	u16	RegTxCounter = priv->TxCounter + 1;
	WARN_ON(1);
#endif

	RT_TRACE(COMP_RESET, "%s():RegTxCounter is %d,TxCounter is %d\n",
			__FUNCTION__,RegTxCounter,priv->TxCounter);

	if(priv->TxCounter == RegTxCounter)
		bStuck = true;

	priv->TxCounter = RegTxCounter;

	return bStuck;
}
//
//      <Assumption: RT_TX_SPINLOCK is acquired.>
//      First added: 2006.11.19 by emily
//
RESET_TYPE
TxCheckStuck(struct net_device *dev)
{
	struct r8192_priv *priv = rtllib_priv(dev);
	u8			QueueID;
	//ptx_ring		head=NULL,tail=NULL,txring = NULL;
	u8			ResetThreshold = NIC_SEND_HANG_THRESHOLD_POWERSAVE;
	bool			bCheckFwTxCnt = false;
	struct rtl8192_tx_ring  *ring = NULL;
	struct sk_buff* skb = NULL;
	cb_desc * tcb_desc = NULL;
	unsigned long flags = 0;

	//
	// Decide Stuch threshold according to current power save mode
	//
	//printk("++++++++++++>%s()\n",__FUNCTION__);
#if 0
	switch (priv->rtllib->dot11PowerSaveMode)
	{
		// The threshold value  may required to be adjusted .
		case eActive:		// Active/Continuous access.
			ResetThreshold = NIC_SEND_HANG_THRESHOLD_NORMAL;
			break;
		case eMaxPs:		// Max power save mode.
			ResetThreshold = NIC_SEND_HANG_THRESHOLD_POWERSAVE;
			break;
		case eFastPs:	// Fast power save mode.
			ResetThreshold = NIC_SEND_HANG_THRESHOLD_POWERSAVE;
			break;
		default:
			break;
	}
#else
	switch (priv->rtllib->ps)
	{
		// The threshold value  may required to be adjusted .
		case RTLLIB_PS_DISABLED:		// Active/Continuous access.
			ResetThreshold = NIC_SEND_HANG_THRESHOLD_NORMAL;
			break;
		case (RTLLIB_PS_MBCAST|RTLLIB_PS_UNICAST):		// Max power save mode.
			ResetThreshold = NIC_SEND_HANG_THRESHOLD_POWERSAVE;
			break;
		default:
			ResetThreshold = NIC_SEND_HANG_THRESHOLD_POWERSAVE;
			break;
	}
#endif
	//
	// Check whether specific tcb has been queued for a specific time
	//
	spin_lock_irqsave(&priv->irq_th_lock,flags);
	for(QueueID = 0; QueueID < MAX_TX_QUEUE; QueueID++)
	{


		if(QueueID == TXCMD_QUEUE)
			continue;

		ring = &priv->tx_ring[QueueID];

		if(skb_queue_len(&ring->queue) == 0)
			continue;
		else
		{
			//txring->nStuckCount++;
			skb = (&ring->queue)->next;
			tcb_desc = (cb_desc *)(skb->cb + MAX_DEV_ADDR_SIZE);
			tcb_desc->nStuckCount++;
			bCheckFwTxCnt = true;
			//AMY 090414
#if defined RTL8192SE || defined RTL8192CE
			if(tcb_desc->nStuckCount > ResetThreshold)
			{
				RT_TRACE( COMP_RESET, "TxCheckStuck(): Need silent reset because nStuckCount > ResetThreshold.\n" );
                                spin_unlock_irqrestore(&priv->irq_th_lock,flags);
				return RESET_TYPE_SILENT;
			}
			//92SE rm CheckFwCnt
			bCheckFwTxCnt = false;
			#endif
		}
	}
	spin_unlock_irqrestore(&priv->irq_th_lock,flags);
	if(bCheckFwTxCnt) {
		if (HalTxCheckStuck819xPci(dev))
		{
			RT_TRACE(COMP_RESET, "TxCheckStuck(): Fw indicates no Tx condition! \n");
			return RESET_TYPE_SILENT;
		}
	}

	return RESET_TYPE_NORESET;
}

#if defined RTL8192SE || defined RTL8192CE
bool HalRxCheckStuck8192SE(struct net_device *dev)
{

	struct r8192_priv *priv = rtllib_priv(dev);
	u16				RegRxCounter = (u16)(priv->InterruptLog.nIMR_ROK&0xffff);
	bool				bStuck = false;
	u32				SlotIndex = 0, TotalRxStuckCount = 0;
	u8				i;


	SlotIndex = (priv->SilentResetRxSlotIndex++)%priv->SilentResetRxSoltNum;

	// Indicate Rx Stuck only if driver detect contineous 4 times of no rx packets.
	// 2009.3.11, by Emily
	if(priv->RxCounter==RegRxCounter)
	{
		priv->SilentResetRxStuckEvent[SlotIndex] = 1;

		for( i = 0; i < priv->SilentResetRxSoltNum ; i++ )
			TotalRxStuckCount += priv->SilentResetRxStuckEvent[i];

		if(TotalRxStuckCount  == priv->SilentResetRxSoltNum)
		{
			bStuck = true;
			for( i = 0; i < priv->SilentResetRxSoltNum ; i++ )
				TotalRxStuckCount += priv->SilentResetRxStuckEvent[i];
		}


	} else {
		priv->SilentResetRxStuckEvent[SlotIndex] = 0;
	}

	priv->RxCounter = RegRxCounter;

	return bStuck;
}
#endif
#if defined(RTL8192E) || defined(RTL8190P)
bool HalRxCheckStuck8190Pci(struct net_device *dev)
{
	struct r8192_priv *priv = rtllib_priv(dev);
	u16		  RegRxCounter = read_nic_word(dev, 0x130);
	bool		  bStuck = false;
	static u8	  rx_chk_cnt = 0;

	RT_TRACE(COMP_RESET,"%s(): RegRxCounter is %d,RxCounter is %d\n",
			__FUNCTION__, RegRxCounter,priv->RxCounter);
	// If rssi is small, we should check rx for long time because of bad rx.
	// or maybe it will continuous silent reset every 2 seconds.
	rx_chk_cnt++;
	if(priv->undecorated_smoothed_pwdb >= (RateAdaptiveTH_High+5))
	{
		rx_chk_cnt = 0;	//high rssi, check rx stuck right now.
	} else if ((priv->undecorated_smoothed_pwdb < (RateAdaptiveTH_High+5)) &&
		(((priv->CurrentChannelBW != HT_CHANNEL_WIDTH_20) &&
		  (priv->undecorated_smoothed_pwdb >= RateAdaptiveTH_Low_40M)) ||
		((priv->CurrentChannelBW == HT_CHANNEL_WIDTH_20) &&
		 (priv->undecorated_smoothed_pwdb>=RateAdaptiveTH_Low_20M)))) {
		if (rx_chk_cnt < 2) {
			return bStuck;
		} else {
			rx_chk_cnt = 0;
		}
	} else if((((priv->CurrentChannelBW != HT_CHANNEL_WIDTH_20) &&
		  (priv->undecorated_smoothed_pwdb < RateAdaptiveTH_Low_40M)) ||
		((priv->CurrentChannelBW == HT_CHANNEL_WIDTH_20) &&
		 (priv->undecorated_smoothed_pwdb < RateAdaptiveTH_Low_20M))) &&
		priv->undecorated_smoothed_pwdb >= VeryLowRSSI) {
		if (rx_chk_cnt < 4) {
			//DbgPrint("RSSI < %d && RSSI >= %d, no check this time \n", RateAdaptiveTH_Low, VeryLowRSSI);
			return bStuck;
		} else {
			rx_chk_cnt = 0;
			//DbgPrint("RSSI < %d && RSSI >= %d, check this time \n", RateAdaptiveTH_Low, VeryLowRSSI);
		}
	} else {
		if(rx_chk_cnt < 8) {
			//DbgPrint("RSSI <= %d, no check this time \n", VeryLowRSSI);
			return bStuck;
		} else {
			rx_chk_cnt = 0;
			//DbgPrint("RSSI <= %d, check this time \n", VeryLowRSSI);
		}
	}

	if(priv->RxCounter==RegRxCounter)
		bStuck = true;

	priv->RxCounter = RegRxCounter;

	return bStuck;
}
#endif
RESET_TYPE RxCheckStuck(struct net_device *dev)
{
#if defined RTL8192SE ||defined RTL8192CE
	if(HalRxCheckStuck8192SE(dev))
#else
	if(HalRxCheckStuck8190Pci(dev))
#endif
	{
		RT_TRACE(COMP_RESET, "RxStuck Condition\n");
		return RESET_TYPE_SILENT;
	}

	return RESET_TYPE_NORESET;
}

RESET_TYPE
rtl819x_ifcheck_resetornot(struct net_device *dev)
{
	struct r8192_priv *priv = rtllib_priv(dev);
	RESET_TYPE	TxResetType = RESET_TYPE_NORESET;
	RESET_TYPE	RxResetType = RESET_TYPE_NORESET;
	RT_RF_POWER_STATE	rfState;

	rfState = priv->rtllib->eRFPowerState;

	if(rfState == eRfOn)
	TxResetType = TxCheckStuck(dev);
	if( rfState == eRfOn &&
		/*ADAPTER_TEST_STATUS_FLAG(dev, ADAPTER_STATUS_FW_DOWNLOAD_FAILURE)) &&*/
		(priv->rtllib->iw_mode == IW_MODE_INFRA) && (priv->rtllib->state == RTLLIB_LINKED)) {
		// If driver is in the status of firmware download failure , driver skips RF initialization and RF is
		// in turned off state. Driver should check whether Rx stuck and do silent reset. And
		// if driver is in firmware download failure status, driver should initialize RF in the following
		// silent reset procedure Emily, 2008.01.21

		// Driver should not check RX stuck in IBSS mode because it is required to
		// set Check BSSID in order to send beacon, however, if check BSSID is
		// set, STA cannot hear any packet a all. Emily, 2008.04.12
		RxResetType = RxCheckStuck(dev);
	}
	RT_TRACE(COMP_RESET,"%s(): TxResetType is %d, RxResetType is %d\n",__FUNCTION__,TxResetType,RxResetType);
	if(TxResetType==RESET_TYPE_NORMAL || RxResetType==RESET_TYPE_NORMAL)
		return RESET_TYPE_NORMAL;
	else if(TxResetType==RESET_TYPE_SILENT || RxResetType==RESET_TYPE_SILENT)
		return RESET_TYPE_SILENT;
	else
		return RESET_TYPE_NORESET;

}

//
// This function will do "system reset" to NIC when Tx or Rx is stuck.
// The method checking Tx/Rx stuck of this function is supported by FW,
// which reports Tx and Rx counter to register 0x128 and 0x130.
//
void rtl819x_ifsilentreset(struct net_device *dev)
{
	struct r8192_priv *priv = rtllib_priv(dev);
	u8	reset_times = 0;
	int reset_status = 0;
	struct rtllib_device *ieee = priv->rtllib;
	unsigned long flag;
#ifdef _RTL8192_EXT_PATCH_
	bool wlansilentreset = false,meshsilentreset = false;
	u8 backup_channel_wlan = 1,backup_channel_mesh = 1;
	u8 updateBW = 0;
	u8 bserverHT = 0;
	int i=0;
#endif
	// 2007.07.20. If we need to check CCK stop, please uncomment this line.
	//bStuck = dev->HalFunc.CheckHWStopHandler(dev);

	if(priv->ResetProgress==RESET_TYPE_NORESET) {

		RT_TRACE(COMP_RESET,"=========>Reset progress!! \n");

		// Set the variable for reset.
		priv->ResetProgress = RESET_TYPE_SILENT;
		spin_lock_irqsave(&priv->rf_ps_lock,flag);
		if(priv->RFChangeInProgress)
		{
			spin_unlock_irqrestore(&priv->rf_ps_lock,flag);
			//	Adapter->ResetProgress = RESET_TYPE_SILENT;
			goto END;
			//return;
		}
		priv->RFChangeInProgress = true;
		priv->bResetInProgress = true;
		spin_unlock_irqrestore(&priv->rf_ps_lock,flag);
//		rtl8192_close(dev);
RESET_START:
		down(&priv->wx_sem);
#ifdef ENABLE_LPS
                //LZM for PS-Poll AID issue. 090429
                if(priv->rtllib->state == RTLLIB_LINKED)
                    LeisurePSLeave(dev);
#endif
#ifdef _RTL8192_EXT_PATCH_
		if((priv->up == 0) && (priv->mesh_up == 0))
#else
		if(priv->up == 0)
#endif
		{
			RT_TRACE(COMP_ERR,"%s():the driver is not up! return\n",__FUNCTION__);
			up(&priv->wx_sem);
			return ;
		}
#ifdef _RTL8192_EXT_PATCH_
		if(priv->up == 1)
		{
			printk("================>wlansilentreset is true\n");
			wlansilentreset = true;
			priv->up = 0;
		}
		if(priv->mesh_up == 1)
		{
			printk("================>meshsilentreset is true\n");
			meshsilentreset = true;
			priv->mesh_up = 0;
		}
#else
		priv->up = 0;
#endif
		RT_TRACE(COMP_RESET,"%s():======>start to down the driver\n",__FUNCTION__);
		mdelay(1000);
		RT_TRACE(COMP_RESET,"%s():111111111111111111111111======>start to down the driver\n",__FUNCTION__);
		if(!netif_queue_stopped(dev))
			netif_stop_queue(dev);
#if !(defined RTL8192SE || defined Rtl8192CE)
		dm_backup_dynamic_mechanism_state(dev);
#endif
		rtl8192_irq_disable(dev);
		del_timer_sync(&priv->watch_dog_timer);
		rtl8192_cancel_deferred_work(priv);
		deinit_hal_dm(dev);
		ieee->sync_scan_hurryup = 1;
#ifdef _RTL8192_EXT_PATCH_
		backup_channel_wlan = ieee->current_network.channel;
		backup_channel_mesh = ieee->current_mesh_network.channel;
		if((ieee->state == RTLLIB_LINKED) && ((ieee->iw_mode == IW_MODE_INFRA) || (ieee->iw_mode == IW_MODE_ADHOC)))
		{
			printk("====>down, infra or adhoc\n");
			SEM_DOWN_IEEE_WX(&ieee->wx_sem);
			printk("ieee->state is RTLLIB_LINKED\n");
			rtllib_stop_send_beacons(priv->rtllib);
			del_timer_sync(&ieee->associate_timer);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
			cancel_delayed_work(&ieee->associate_retry_wq);
#endif
			rtllib_stop_scan(ieee);
			netif_carrier_off(dev);
			SEM_UP_IEEE_WX(&ieee->wx_sem);
		}
		else if((ieee->state == RTLLIB_LINKED) && (ieee->iw_mode == IW_MODE_MESH) && (!ieee->only_mesh))
		{
			printk("====>down, wlan server\n");
			SEM_DOWN_IEEE_WX(&ieee->wx_sem);
			printk("ieee->state is RTLLIB_LINKED\n");
			rtllib_stop_send_beacons(priv->rtllib);
			del_timer_sync(&ieee->associate_timer);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
			cancel_delayed_work(&ieee->associate_retry_wq);
#endif
			rtllib_stop_scan(ieee);
			netif_carrier_off(dev);
			SEM_UP_IEEE_WX(&ieee->wx_sem);
			if(priv->mshobj->ext_patch_rtllib_stop_protocol)
				priv->mshobj->ext_patch_rtllib_stop_protocol(ieee,1);
		}
		else if((ieee->iw_mode == IW_MODE_MESH) && (!ieee->only_mesh))
		{
			printk("====>down, eth0 server\n");
			if(priv->mshobj->ext_patch_rtllib_stop_protocol)
				priv->mshobj->ext_patch_rtllib_stop_protocol(ieee,1);
		}
		else if((ieee->iw_mode == IW_MODE_MESH) && (ieee->only_mesh))
		{
			printk("====>down, p2p or client\n");
			if(priv->mshobj->ext_patch_rtllib_stop_protocol)
				priv->mshobj->ext_patch_rtllib_stop_protocol(ieee,1);
		}
		else{
			printk("====>down, no link\n");
			printk("ieee->state is NOT LINKED\n");
			rtllib_softmac_stop_protocol(priv->rtllib,0,true);
		}
#else
		if(ieee->state == RTLLIB_LINKED)
		{
			SEM_DOWN_IEEE_WX(&ieee->wx_sem);
			printk("ieee->state is RTLLIB_LINKED\n");
			rtllib_stop_send_beacons(priv->rtllib);
			del_timer_sync(&ieee->associate_timer);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
			cancel_delayed_work(&ieee->associate_retry_wq);
#endif
			rtllib_stop_scan(ieee);
			netif_carrier_off(dev);
			SEM_UP_IEEE_WX(&ieee->wx_sem);
		}
		else{
			printk("ieee->state is NOT LINKED\n");
			rtllib_softmac_stop_protocol(priv->rtllib,true);
		}
#endif
#ifdef RTL8190P
		priv->ops->stop_adapter(dev, true);//AMY 090415
#endif
		up(&priv->wx_sem);
		RT_TRACE(COMP_RESET,"%s():<==========down process is finished\n",__FUNCTION__);
		RT_TRACE(COMP_RESET,"%s():===========>start to up the driver\n",__FUNCTION__);
#ifdef _RTL8192_EXT_PATCH_
		if(wlansilentreset == true){
			reset_status = _rtl8192_up(dev,true);
		}
		if(meshsilentreset == true)
			reset_status = meshdev_up(ieee->meshdev,true);
#else
		reset_status = _rtl8192_up(dev);
#endif
		RT_TRACE(COMP_RESET,"%s():<===========up process is finished\n",__FUNCTION__);
		if(reset_status == -1)
		{
			if(reset_times < 3)
			{
				reset_times++;
				goto RESET_START;
			}
			else
			{
				RT_TRACE(COMP_ERR," ERR!!! %s():  Reset Failed!!\n",__FUNCTION__);
			}
		}
		ieee->is_silent_reset = 1;
		spin_lock_irqsave(&priv->rf_ps_lock,flag);
		priv->RFChangeInProgress = false;
		spin_unlock_irqrestore(&priv->rf_ps_lock,flag);
		EnableHWSecurityConfig8192(dev);
#ifdef _RTL8192_EXT_PATCH_
	//	if(wlansilentreset == true)
		ieee->current_network.channel = backup_channel_wlan;
		//if(meshsilentreset == true)
		ieee->current_mesh_network.channel = backup_channel_mesh;
#endif
		if(ieee->state == RTLLIB_LINKED && ieee->iw_mode == IW_MODE_INFRA)
		{
			ieee->set_chan(ieee->dev, ieee->current_network.channel);

			queue_work_rsl(ieee->wq, &ieee->associate_complete_wq);

		}
		else if(ieee->state == RTLLIB_LINKED && ieee->iw_mode == IW_MODE_ADHOC)
		{
			ieee->set_chan(ieee->dev, ieee->current_network.channel);
			ieee->link_change(ieee->dev);

			notify_wx_assoc_event(ieee);

			rtllib_start_send_beacons(ieee);

			if (ieee->data_hard_resume)
				ieee->data_hard_resume(ieee->dev);
			netif_carrier_on(ieee->dev);
		}
#ifdef _RTL8192_EXT_PATCH_
		else if((ieee->state == RTLLIB_LINKED) && (ieee->iw_mode == IW_MODE_MESH) && (!ieee->only_mesh))
		{
			printk("===>up, wlan0 server\n");
			ieee->set_chan(ieee->dev, ieee->current_network.channel);

			queue_work_rsl(ieee->wq, &ieee->associate_complete_wq);
			if (ieee->current_mesh_network.beacon_interval == 0)
				ieee->current_mesh_network.beacon_interval = 100;
			ieee->link_change(ieee->dev);
			if(priv->mshobj->ext_patch_rtllib_start_protocol)
				priv->mshobj->ext_patch_rtllib_start_protocol(ieee);
		}
		else if((ieee->iw_mode == IW_MODE_MESH) && (!ieee->only_mesh))
		{
			printk("===>up, eth0 server\n");
			if (ieee->current_mesh_network.beacon_interval == 0)
				ieee->current_mesh_network.beacon_interval = 100;
			ieee->link_change(ieee->dev);
			if(priv->mshobj->ext_patch_rtllib_start_protocol)
				priv->mshobj->ext_patch_rtllib_start_protocol(ieee);
			ieee->current_network.channel = ieee->current_mesh_network.channel; //YJ,add,090216
			if(ieee->pHTInfo->bCurBW40MHz)
				HTSetConnectBwMode(ieee, HT_CHANNEL_WIDTH_20_40, (ieee->current_mesh_network.channel<=6)?HT_EXTCHNL_OFFSET_UPPER:HT_EXTCHNL_OFFSET_LOWER);  //20/40M
			else
				HTSetConnectBwMode(ieee, HT_CHANNEL_WIDTH_20, (ieee->current_mesh_network.channel<=6)?HT_EXTCHNL_OFFSET_UPPER:HT_EXTCHNL_OFFSET_LOWER);  //20M
		}
		else if((ieee->iw_mode == IW_MODE_MESH) && (ieee->only_mesh))
		{
			printk("===>up, p2p or client\n");
			if (ieee->current_mesh_network.beacon_interval == 0)
				ieee->current_mesh_network.beacon_interval = 100;
			ieee->link_change(ieee->dev);
			if(priv->mshobj->ext_patch_rtllib_start_protocol)
				priv->mshobj->ext_patch_rtllib_start_protocol(ieee);
			if(ieee->p2pmode)
			{
				ieee->current_network.channel = ieee->current_mesh_network.channel; //YJ,add,090216
				if(ieee->pHTInfo->bCurBW40MHz)
					HTSetConnectBwMode(ieee, HT_CHANNEL_WIDTH_20_40, (ieee->current_mesh_network.channel<=6)?HT_EXTCHNL_OFFSET_UPPER:HT_EXTCHNL_OFFSET_LOWER);  //20/40M
				else
					HTSetConnectBwMode(ieee, HT_CHANNEL_WIDTH_20, (ieee->current_mesh_network.channel<=6)?HT_EXTCHNL_OFFSET_UPPER:HT_EXTCHNL_OFFSET_LOWER);  //20M
			}
			else
			{
				updateBW=priv->mshobj->ext_patch_r819x_wx_update_beacon(dev,&bserverHT);
				printk("$$$$$$ Cur_networ.chan=%d, cur_mesh_net.chan=%d,bserverHT=%d\n", ieee->current_network.channel,ieee->current_mesh_network.channel,bserverHT);
				if(updateBW == 1)
				{
					if(bserverHT == 0)
					{
						printk("===>server is not HT supported,set 20M\n");
						HTSetConnectBwMode(ieee, HT_CHANNEL_WIDTH_20, HT_EXTCHNL_OFFSET_NO_EXT);  //20M
					}
					else
					{
						printk("===>updateBW is 1,bCurBW40MHz is %d,ieee->serverExtChlOffset is %d\n",ieee->pHTInfo->bCurBW40MHz,ieee->serverExtChlOffset);
						if(ieee->pHTInfo->bCurBW40MHz)
							HTSetConnectBwMode(ieee, HT_CHANNEL_WIDTH_20_40, ieee->serverExtChlOffset);  //20/40M
						else
							HTSetConnectBwMode(ieee, HT_CHANNEL_WIDTH_20, ieee->serverExtChlOffset);  //20M
					}
				}
				else
				{
					printk("===>there is no same hostname server, ERR!!!\n");
				}
			}

		}
#endif
#ifdef TO_DO_LIST
		else if(Adapter->MgntInfo.mActingAsAp)
		{
		//	AP_Reset(Adapter);
		//	ULONG i = 0;
		//	PMGNT_INFO pMgntInfo = &Adapter->MgntInfo;
		//	PRT_WLAN_STA	AsocEntry = pMgntInfo->AsocEntry;
		//	PRT_WLAN_STA pEntry = NULL;
			AP_StartApRequest((PVOID)Adapter);
			Adapter->HalFunc.ResetHalRATRTableHandler(Adapter);
		}
#endif

#ifdef TO_DO_LIST
		if(Adapter->MgntInfo.mActingAsAp)
			AP_CamRestoreAllEntry(Adapter);
		else
#endif
#ifdef _RTL8192_EXT_PATCH_
		if(wlansilentreset){
			printk("==========>wlansilentreset\n");
			CamRestoreEachIFEntry(dev,0);
		}
		if(meshsilentreset){
			printk("==========>meshsilentreset\n");
			CamRestoreEachIFEntry(dev,1);
			for(i=0;i<8;i++)
			{
				if(ieee->swmeshratrtable[i].bused == true)
				{
					printk("====>restore ratr table: index=%d,value=%x\n",i,ieee->swmeshratrtable[i].ratr_value);
					write_nic_dword(dev,ARFR0+i*4,ieee->swmeshratrtable[i].ratr_value);
				}
			}
		}
#else
		CamRestoreAllEntry(dev);
#endif
		// Restore the previous setting for all dynamic mechanism
#if !(defined RTL8192SE || defined RTL8192CE)
		dm_restore_dynamic_mechanism_state(dev);
#endif
END:
		priv->ResetProgress = RESET_TYPE_NORESET;
		priv->reset_count++;

		priv->bForcedSilentReset =false;
		priv->bResetInProgress = false;

#ifdef RTL8190P
		// For test --> force write UFWP.
		write_nic_byte(dev, UFWP, 1);	//AMY 090415
#endif
		RT_TRACE(COMP_RESET, "Reset finished!! ====>[%d]\n", priv->reset_count);
	}
}

void rtl819x_update_rxcounts(struct r8192_priv *priv,
			     u32 *TotalRxBcnNum,
			     u32 *TotalRxDataNum)
{
	u16			SlotIndex;
	u8			i;

	*TotalRxBcnNum = 0;
	*TotalRxDataNum = 0;

	SlotIndex = (priv->rtllib->LinkDetectInfo.SlotIndex++)%(priv->rtllib->LinkDetectInfo.SlotNum);
	priv->rtllib->LinkDetectInfo.RxBcnNum[SlotIndex] = priv->rtllib->LinkDetectInfo.NumRecvBcnInPeriod;
	priv->rtllib->LinkDetectInfo.RxDataNum[SlotIndex] = priv->rtllib->LinkDetectInfo.NumRecvDataInPeriod;
	for (i = 0; i < priv->rtllib->LinkDetectInfo.SlotNum; i++) {
		*TotalRxBcnNum += priv->rtllib->LinkDetectInfo.RxBcnNum[i];
		*TotalRxDataNum += priv->rtllib->LinkDetectInfo.RxDataNum[i];
	}
}

#ifdef _RTL8192_EXT_PATCH_
//YJ,add,090518,for KeepAlive
static void MgntLinkKeepAlive(struct r8192_priv *priv )
{
	if (priv->keepAliveLevel == 0)
		return;

	if((priv->rtllib->state == RTLLIB_LINKED) && (!priv->rtllib->is_roaming))
	{
		//
		// Keep-Alive.
		//
		//printk("Tx:%d Rx:%d Idle:%d\n",(priv->NumTxUnicast - priv->rtllib->LinkDetectInfo.LastNumTxUnicast), priv->rtllib->LinkDetectInfo.NumRxUnicastOkInPeriod, priv->rtllib->LinkDetectInfo.IdleCount);

		if ( (priv->keepAliveLevel== 2) ||
			(priv->rtllib->LinkDetectInfo.LastNumTxUnicast == priv->NumTxUnicast &&
			priv->rtllib->LinkDetectInfo.NumRxUnicastOkInPeriod == 0)
			)
		{
			priv->rtllib->LinkDetectInfo.IdleCount++;

			//
			// Send a Keep-Alive packet packet to AP if we had been idle for a while.
			//
			if(priv->rtllib->LinkDetectInfo.IdleCount >= ((KEEP_ALIVE_INTERVAL / RT_CHECK_FOR_HANG_PERIOD)-1) )
			{
				priv->rtllib->LinkDetectInfo.IdleCount = 0;
				//printk("%s: Send NULL Frame to AP indicating activity\n", __FUNCTION__);
				rtllib_sta_ps_send_null_frame(priv->rtllib, false);
			}
		}
		else
		{
			priv->rtllib->LinkDetectInfo.IdleCount = 0;
		}
		priv->rtllib->LinkDetectInfo.LastNumTxUnicast = priv->NumTxUnicast;
		priv->rtllib->LinkDetectInfo.LastNumRxUnicast = priv->rtllib->LinkDetectInfo.NumRxUnicastOkInPeriod;
	}
}
//YJ,add,090518,for KeepAlive,end
#endif

void	rtl819x_watchdog_wqcallback(void *data)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20))
	struct r8192_priv *priv = container_of_dwork_rsl(data,struct r8192_priv,watch_dog_wq);
	struct net_device *dev = priv->rtllib->dev;
#else
	struct net_device *dev = (struct net_device *)data;
	struct r8192_priv *priv = rtllib_priv(dev);
#endif
	struct rtllib_device* ieee = priv->rtllib;
	RESET_TYPE	ResetType = RESET_TYPE_NORESET;
	static u8	check_reset_cnt = 0;
#ifdef _RTL8192_EXT_PATCH_
	static u8	last_reset_count = 0;
#endif
	unsigned long flags;
	PRT_POWER_SAVE_CONTROL pPSC = (PRT_POWER_SAVE_CONTROL)(&(priv->rtllib->PowerSaveControl));//added by amy 090414
	bool bBusyTraffic = false;
	bool bEnterPS = false;
#ifdef _RTL8192_EXT_PATCH_
	if(((!priv->up) && (!priv->mesh_up)) || (priv->bHwRadioOff == true))//AMY modify 090220
#else
	if((!priv->up) || (priv->bHwRadioOff == true))
#endif
		return;
#ifdef RTL8192CE
        return;
#endif
	hal_dm_watchdog(dev);
#ifdef ENABLE_IPS
//	printk("watch_dog ENABLE_IPS\n");
	if(ieee->actscanning == false){
		if((ieee->iw_mode == IW_MODE_INFRA) && (ieee->state == RTLLIB_NOLINK) &&\
		    (ieee->eRFPowerState == eRfOn)&&!ieee->is_set_key &&\
		    (!ieee->proto_stoppping) && !ieee->wx_set_enc
#ifdef CONFIG_RTLWIFI_DEBUGFS
		    && (!priv->debug->hw_holding)
#endif
		 ){
			if(ieee->PowerSaveControl.ReturnPoint == IPS_CALLBACK_NONE){
				printk("====================>haha:IPSEnter()\n");
				IPSEnter(dev);
				//rtllib_stop_scan(priv->rtllib);
			}
		}
	}
#endif
#ifdef _RTL8192_EXT_PATCH_
	//YJ,add,090518,for KeepAlive
	MgntLinkKeepAlive(priv);
#endif
	{//to get busy traffic condition
		if((ieee->state == RTLLIB_LINKED) && (ieee->iw_mode == IW_MODE_INFRA))
		{
			if(	ieee->LinkDetectInfo.NumRxOkInPeriod> 100 ||
				ieee->LinkDetectInfo.NumTxOkInPeriod> 100 ) {
				bBusyTraffic = true;
			}
			//added by amy for Leisure PS
			if(	((ieee->LinkDetectInfo.NumRxUnicastOkInPeriod + ieee->LinkDetectInfo.NumTxOkInPeriod) > 8 ) ||
				(ieee->LinkDetectInfo.NumRxUnicastOkInPeriod > 2) )
			{
				//printk("ieee->LinkDetectInfo.NumRxUnicastOkInPeriod is %d,ieee->LinkDetectInfo.NumTxOkInPeriod is %d\n",ieee->LinkDetectInfo.NumRxUnicastOkInPeriod,ieee->LinkDetectInfo.NumTxOkInPeriod);
				bEnterPS= false;
			}
			else
			{
				bEnterPS= true;
			}

			RT_TRACE(COMP_LPS,"***bEnterPS = %d\n", bEnterPS);
#ifdef ENABLE_LPS
			// LeisurePS only work in infra mode.
			if(bEnterPS)
			{
				LeisurePSEnter(dev);
			}
			else
			{
				LeisurePSLeave(dev);
			}

		}
		else
		{
			RT_TRACE(COMP_LPS,"====>no link LPS leave\n");
			LeisurePSLeave(dev);
		}
#else
		}
#endif
	       ieee->LinkDetectInfo.NumRxOkInPeriod = 0;
	       ieee->LinkDetectInfo.NumTxOkInPeriod = 0;
	       ieee->LinkDetectInfo.NumRxUnicastOkInPeriod = 0;
	       ieee->LinkDetectInfo.bBusyTraffic = bBusyTraffic;
	}
	//added by amy for AP roaming
	{
#if defined RTL8192SE || defined Rtl8192CE
		if(priv->rtllib->iw_mode == IW_MODE_ADHOC)
			IbssAgeFunction(ieee);
#endif

		if(ieee->state == RTLLIB_LINKED && ieee->iw_mode == IW_MODE_INFRA)
		{
			u32	TotalRxBcnNum = 0;
			u32	TotalRxDataNum = 0;

			rtl819x_update_rxcounts(priv, &TotalRxBcnNum, &TotalRxDataNum);
			if((TotalRxBcnNum+TotalRxDataNum) == 0)
			{
				if( ieee->eRFPowerState == eRfOff)
					RT_TRACE(COMP_ERR,"========>%s()\n",__FUNCTION__);
				printk("===>%s(): AP is power off,connect another one\n",__FUNCTION__);
		//		Dot11d_Reset(dev);
				ieee->state = RTLLIB_ASSOCIATING;
				//notify_wx_assoc_event(priv->rtllib);
				RemovePeerTS(priv->rtllib,priv->rtllib->current_network.bssid);
				ieee->is_roaming = true;
				ieee->is_set_key = false;
                             ieee->link_change(dev);
			     if(ieee->LedControlHandler)
				ieee->LedControlHandler(ieee->dev, LED_CTL_START_TO_LINK); //added by amy for LED 090318
				queue_delayed_work_rsl(ieee->wq, &ieee->associate_procedure_wq, 0);

			}
		}
	      ieee->LinkDetectInfo.NumRecvBcnInPeriod=0;
              ieee->LinkDetectInfo.NumRecvDataInPeriod=0;

	}
//	CAM_read_entry(dev,0);
	//check if reset the driver
	spin_lock_irqsave(&priv->tx_lock,flags);
	if((check_reset_cnt++ >= 3) && (!ieee->is_roaming) &&
			(!priv->RFChangeInProgress) && (!pPSC->bSwRfProcessing))
	{
		ResetType = rtl819x_ifcheck_resetornot(dev);
#ifdef _RTL8192_EXT_PATCH_
		if(check_reset_cnt == 0xFF)
#endif
		check_reset_cnt = 3;
		//DbgPrint("Start to check silent reset\n");
	}
	spin_unlock_irqrestore(&priv->tx_lock,flags);
	if(!priv->bDisableNormalResetCheck && ResetType == RESET_TYPE_NORMAL)
	{
		priv->ResetProgress = RESET_TYPE_NORMAL;
		RT_TRACE(COMP_RESET,"%s(): NOMAL RESET\n",__FUNCTION__);
		return;
	}
	// disable silent reset temply 2008.9.11*/
#ifdef _RTL8192_EXT_PATCH_
	if( ((priv->force_reset) || (!priv->bDisableNormalResetCheck && ResetType==RESET_TYPE_SILENT))) // This is control by OID set in Pomelo
	{
			if(check_reset_cnt != (last_reset_count + 1)){
				printk("=======================>%s: Resume firmware\n", __FUNCTION__);
				r8192se_resume_firm(dev);
				last_reset_count = check_reset_cnt;
			}else{
				printk("=======================>%s: Silent Reset\n", __FUNCTION__);
				rtl819x_ifsilentreset(dev);
			}
	}
#else
	if( ((priv->force_reset) || (!priv->bDisableNormalResetCheck && ResetType==RESET_TYPE_SILENT))) // This is control by OID set in Pomelo
	{
		rtl819x_ifsilentreset(dev);
	}
#endif
	priv->force_reset = false;
	priv->bForcedSilentReset = false;
	priv->bResetInProgress = false;
	RT_TRACE(COMP_TRACE, " <==RtUsbCheckForHangWorkItemCallback()\n");

}

void watch_dog_timer_callback(unsigned long data)
{
	struct r8192_priv *priv = rtllib_priv((struct net_device *) data);
	queue_delayed_work_rsl(priv->priv_wq,&priv->watch_dog_wq,0);
	mod_timer(&priv->watch_dog_timer, jiffies + MSECS(RTLLIB_WATCH_DOG_TIME));
}

/****************************************************************************
 ---------------------------- NIC TX/RX STUFF---------------------------
*****************************************************************************/
void rtl8192_rx_enable(struct net_device *dev)
{
    struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);
#ifdef RTL8192CE
    write_nic_dword(dev, REG_RX_DESA,priv->rx_ring_dma);
#else
    write_nic_dword(dev, RDQDA,priv->rx_ring_dma);
#endif
}

// the TX_DESC_BASE setting is according to the following queue index
//  BK_QUEUE       ===>                        0
//  BE_QUEUE       ===>                        1
//  VI_QUEUE       ===>                        2
//  VO_QUEUE       ===>                        3
//  HCCA_QUEUE     ===>                        4
//  TXCMD_QUEUE    ===>                        5
//  MGNT_QUEUE     ===>                        6
//  HIGH_QUEUE     ===>                        7
//  BEACON_QUEUE   ===>                        8
//  */
#if defined RTL8192CE //FIX MERGE RTL8192CE not neen  HCCA_QUEUE and TXCMD_QUEUE.
u32 TX_DESC_BASE[] = {REG_BKQ_DESA, REG_BEQ_DESA, REG_VIQ_DESA,
					  REG_VOQ_DESA, REG_HDAQ_DESA_NODEF, REG_CMDQ_DESA_NODEF,
					  REG_MGQ_DESA, REG_HQ_DESA, REG_BCNQ_DESA};
#elif defined RTL8192SE
u32 TX_DESC_BASE[] = {TBKDA, TBEDA, TVIDA, TVODA, TBDA, TCDA, TMDA, THPDA, HDA};
 #else
u32 TX_DESC_BASE[] = {BKQDA, BEQDA, VIQDA, VOQDA, HCCAQDA, CQDA, MQDA, HQDA, BQDA};
 #endif
void rtl8192_tx_enable(struct net_device *dev)
{
    struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);
    u32 i;

#ifdef RTL8192CE
    for (i = 0; i < MAX_TX_QUEUE_COUNT; i++)
		if((i != 4) && (i != 5))
			write_nic_dword(dev, TX_DESC_BASE[i], priv->tx_ring[i].dma);
#else
    for (i = 0; i < MAX_TX_QUEUE_COUNT; i++)
        write_nic_dword(dev, TX_DESC_BASE[i], priv->tx_ring[i].dma);
#endif

    rtllib_reset_queue(priv->rtllib);
}

#if 0
void rtl8192_beacon_tx_enable(struct net_device *dev)
{
	struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);
	u32 reg;

	reg = read_nic_dword(priv->rtllib->dev,INTA_MASK);

	// enable Beacon realted interrupt signal */
	reg |= (IMR_BcnInt | IMR_BcnInt | IMR_TBDOK | IMR_TBDER);
	write_nic_byte(dev,reg);
}
#endif

static void rtl8192_free_rx_ring(struct net_device *dev)
{
    struct r8192_priv *priv = rtllib_priv(dev);
    int i;

    for (i = 0; i < priv->rxringcount; i++) {
        struct sk_buff *skb = priv->rx_buf[i];
        if (!skb)
            continue;

        pci_unmap_single(priv->pdev,
                *((dma_addr_t *)skb->cb),
                priv->rxbuffersize, PCI_DMA_FROMDEVICE);
        kfree_skb(skb);
    }

    pci_free_consistent(priv->pdev, sizeof(*priv->rx_ring) * priv->rxringcount,
            priv->rx_ring, priv->rx_ring_dma);
    priv->rx_ring = NULL;
}

static void rtl8192_free_tx_ring(struct net_device *dev, unsigned int prio)
{
    struct r8192_priv *priv = rtllib_priv(dev);
    struct rtl8192_tx_ring *ring = &priv->tx_ring[prio];

    while (skb_queue_len(&ring->queue)) {
        tx_desc *entry = &ring->desc[ring->idx];
        struct sk_buff *skb = __skb_dequeue(&ring->queue);

        pci_unmap_single(priv->pdev, le32_to_cpu(entry->TxBuffAddr),
                skb->len, PCI_DMA_TODEVICE);
        kfree_skb(skb);
        ring->idx = (ring->idx + 1) % ring->entries;
    }

    pci_free_consistent(priv->pdev, sizeof(*ring->desc)*ring->entries,
            ring->desc, ring->dma);
    ring->desc = NULL;
}


void rtl8192_beacon_disable(struct net_device *dev)
{
	struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);
	u32 reg;

#ifdef RTL8192CE
	reg = read_nic_dword(priv->rtllib->dev,REG_HIMR);

	// disable Beacon realted interrupt signal */
	reg &= ~(IMR_BcnInt | IMR_BcnInt | IMR_TBDOK | IMR_TBDER);
	write_nic_dword(priv->rtllib->dev, REG_HIMR, reg);
#else
	reg = read_nic_dword(priv->rtllib->dev,INTA_MASK);

	// disable Beacon realted interrupt signal */
	reg &= ~(IMR_BcnInt | IMR_BcnInt | IMR_TBDOK | IMR_TBDER);
	write_nic_dword(priv->rtllib->dev, INTA_MASK, reg);
#endif
}


void rtl8192_data_hard_stop(struct net_device *dev)
{
	//FIXME !!
	#if 0
	struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);
	priv->dma_poll_mask |= (1<<TX_DMA_STOP_LOWPRIORITY_SHIFT);
	rtl8192_set_mode(dev,EPROM_CMD_CONFIG);
	write_nic_byte(dev,TX_DMA_POLLING,priv->dma_poll_mask);
	rtl8192_set_mode(dev,EPROM_CMD_NORMAL);
	#endif
}


void rtl8192_data_hard_resume(struct net_device *dev)
{
	// FIXME !!
	#if 0
	struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);
	priv->dma_poll_mask &= ~(1<<TX_DMA_STOP_LOWPRIORITY_SHIFT);
	rtl8192_set_mode(dev,EPROM_CMD_CONFIG);
	write_nic_byte(dev,TX_DMA_POLLING,priv->dma_poll_mask);
	rtl8192_set_mode(dev,EPROM_CMD_NORMAL);
	#endif
}

// this function TX data frames when the rtllib stack requires this.
// It checks also if we need to stop the ieee tx queue, eventually do it
//
void rtl8192_hard_data_xmit(struct sk_buff *skb, struct net_device *dev, int rate)
{
	struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);
	int ret;
	//unsigned long flags;
	cb_desc *tcb_desc = (cb_desc *)(skb->cb + MAX_DEV_ADDR_SIZE);
	u8 queue_index = tcb_desc->queue_index;

#ifdef _RTL8192_EXT_PATCH_
	if((priv->bHwRadioOff == true)||((!priv->up)&& (!priv->mesh_up)))
#else
	if((priv->bHwRadioOff == true)||(!priv->up))
#endif
	{
		kfree_skb(skb);
		return;
	}

	// shall not be referred by command packet */
	assert(queue_index != TXCMD_QUEUE);

	//spin_lock_irqsave(&priv->tx_lock,flags);

        memcpy((unsigned char *)(skb->cb),&dev,sizeof(dev));
#if 0
	tcb_desc->RATRIndex = 7;
	tcb_desc->bTxDisableRateFallBack = 1;
	tcb_desc->bTxUseDriverAssingedRate = 1;
	tcb_desc->bTxEnableFwCalcDur = 1;
#endif
	skb_push(skb, priv->rtllib->tx_headroom);
	ret = rtl8192_tx(dev, skb);
	if(ret != 0) {
		kfree_skb(skb);
	};

//
	if(queue_index!=MGNT_QUEUE) {
	priv->rtllib->stats.tx_bytes+=(skb->len - priv->rtllib->tx_headroom);
	priv->rtllib->stats.tx_packets++;
	}

	//spin_unlock_irqrestore(&priv->tx_lock,flags);

//	return ret;
	return;
}

// This is a rough attempt to TX a frame
// This is called by the ieee 80211 stack to TX management frames.
// If the ring is full packet are dropped (for data frame the queue
// is stopped before this can happen).
//
int rtl8192_hard_start_xmit(struct sk_buff *skb,struct net_device *dev)
{
	struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);


	int ret;
	//unsigned long flags;
        cb_desc *tcb_desc = (cb_desc *)(skb->cb + MAX_DEV_ADDR_SIZE);
        u8 queue_index = tcb_desc->queue_index;

        if(queue_index != TXCMD_QUEUE){
#ifdef _RTL8192_EXT_PATCH_
		if((priv->bHwRadioOff == true)||((!priv->up)&& (!priv->mesh_up)))
#else
		if((priv->bHwRadioOff == true)||(!priv->up))
#endif
		{
                kfree_skb(skb);
                return 0;
            }
        }

	//spin_lock_irqsave(&priv->tx_lock,flags);

        memcpy((unsigned char *)(skb->cb),&dev,sizeof(dev));
	if(queue_index == TXCMD_QUEUE) {
	//	skb_push(skb, USB_HWDESC_HEADER_LEN);
		rtl819xE_tx_cmd(dev, skb);
		ret = 0;
	        //spin_unlock_irqrestore(&priv->tx_lock,flags);
		return ret;
	} else {
	//	RT_TRACE(COMP_SEND, "To send management packet\n");
		tcb_desc->RATRIndex = 7;
		tcb_desc->bTxDisableRateFallBack = 1;
		tcb_desc->bTxUseDriverAssingedRate = 1;
		tcb_desc->bTxEnableFwCalcDur = 1;
		skb_push(skb, priv->rtllib->tx_headroom);
		ret = rtl8192_tx(dev, skb);
		if(ret != 0) {
			kfree_skb(skb);
		};
	}

//	priv->rtllib->stats.tx_bytes+=skb->len;
//	priv->rtllib->stats.tx_packets++;

	//spin_unlock_irqrestore(&priv->tx_lock,flags);

	return ret;

}

void rtl8192_tx_isr(struct net_device *dev, int prio)
{
    struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);

    struct rtl8192_tx_ring *ring = &priv->tx_ring[prio];

    while (skb_queue_len(&ring->queue)) {
        tx_desc *entry = &ring->desc[ring->idx];
        struct sk_buff *skb;

        //  beacon packet will only use the first descriptor defautly,
        //  and the OWN may not be cleared by the hardware
        //
        if(prio != BEACON_QUEUE) {
            if(entry->OWN)
                return;
            ring->idx = (ring->idx + 1) % ring->entries;
        }

        skb = __skb_dequeue(&ring->queue);
        pci_unmap_single(priv->pdev, le32_to_cpu(entry->TxBuffAddr),
                skb->len, PCI_DMA_TODEVICE);

        kfree_skb(skb);
    }
    if(prio == MGNT_QUEUE)
             ;//printk("====>%s(): skb_queue_len(&ring->queue) is %d,ring->idx is %d\n",__FUNCTION__,skb_queue_len(&ring->queue),ring->idx);
    if(prio != BEACON_QUEUE) {
        // try to deal with the pending packets  */
        tasklet_schedule(&priv->irq_tx_tasklet);
    }

}


//
// Mapping Software/Hardware descriptor queue id to "Queue Select Field"
// in TxFwInfo data structure
// 2006.10.30 by Emily
//
// \param QUEUEID       Software Queue
//
u8 MapHwQueueToFirmwareQueue(u8 QueueID, u8 priority)
{
	u8 QueueSelect = 0x0;       //defualt set to

	switch(QueueID) {
#if defined RTL8192E || defined RTL8190P
		case BE_QUEUE:
			QueueSelect = QSLT_BE;  //or QSelect = pTcb->priority;
			break;

		case BK_QUEUE:
			QueueSelect = QSLT_BK;  //or QSelect = pTcb->priority;
			break;

		case VO_QUEUE:
			QueueSelect = QSLT_VO;  //or QSelect = pTcb->priority;
			break;

		case VI_QUEUE:
			QueueSelect = QSLT_VI;  //or QSelect = pTcb->priority;
			break;
		case MGNT_QUEUE:
			QueueSelect = QSLT_MGNT;
			break;
		case BEACON_QUEUE:
			QueueSelect = QSLT_BEACON;
			break;
		case TXCMD_QUEUE:
			QueueSelect = QSLT_CMD;
			break;
		case HIGH_QUEUE:
			QueueSelect = QSLT_HIGH;
			break;
#elif defined RTL8192SE
		case BE_QUEUE:
			QueueSelect = priority; // 3;
			break;
		case BK_QUEUE:
			QueueSelect = priority; // 1;
			break;
		case VO_QUEUE:
			QueueSelect = priority; //7;
			break;
		case VI_QUEUE:
			QueueSelect = priority; //5;
			break;
		case MGNT_QUEUE:
			QueueSelect = QSLT_BE;
			break;
		case BEACON_QUEUE:
			QueueSelect = QSLT_BEACON;
			break;
		case TXCMD_QUEUE:
			QueueSelect = QSLT_CMD;
			break;
		case HIGH_QUEUE:
			QueueSelect = QSLT_HIGH;
			break;
#endif
		default:
			RT_TRACE(COMP_ERR, "TransmitTCB(): Impossible Queue Selection: %d \n", QueueID);
			break;
	}
	return QueueSelect;
}

//
// The tx procedure is just as following,
// skb->cb will contain all the following information,
// priority, morefrag, rate, &dev.
//
void rtl819xE_tx_cmd(struct net_device *dev, struct sk_buff *skb)
{
    struct r8192_priv *priv = rtllib_priv(dev);
    struct rtl8192_tx_ring *ring;
    tx_desc_cmd* entry;
    unsigned int idx;
    dma_addr_t mapping;
    cb_desc *tcb_desc;
    unsigned long flags;

    spin_lock_irqsave(&priv->irq_th_lock,flags);
    ring = &priv->tx_ring[TXCMD_QUEUE];
    mapping = pci_map_single(priv->pdev, skb->data, skb->len, PCI_DMA_TODEVICE);

    idx = (ring->idx + skb_queue_len(&ring->queue)) % ring->entries;
    entry = (tx_desc_cmd*) &ring->desc[idx];

    tcb_desc = (cb_desc *)(skb->cb + MAX_DEV_ADDR_SIZE);

    priv->ops->tx_fill_cmd_descriptor(dev, entry, tcb_desc, skb);

    __skb_queue_tail(&ring->queue, skb);
    spin_unlock_irqrestore(&priv->irq_th_lock,flags);

#if !(defined RTL8192SE || defined RTL8192CE)
    write_nic_byte(dev, TPPoll, TPPoll_CQ);
#endif
    return;
}

short rtl8192_tx(struct net_device *dev, struct sk_buff* skb)
{
    struct r8192_priv *priv = rtllib_priv(dev);
    struct rtl8192_tx_ring  *ring;
    unsigned long flags;
    cb_desc *tcb_desc = (cb_desc *)(skb->cb + MAX_DEV_ADDR_SIZE);
    tx_desc *pdesc = NULL;
    struct rtllib_hdr_1addr * header = NULL;
    u16 fc=0, type=0,stype=0;
    dma_addr_t mapping;
    bool  multi_addr=false,broad_addr=false,uni_addr=false;
    u8*   pda_addr = NULL;
    int   idx;
    u32 fwinfo_size = 0;
    mapping = pci_map_single(priv->pdev, skb->data, skb->len, PCI_DMA_TODEVICE);

    priv->rtllib->bAwakePktSent = true;

    // collect the tx packets statitcs */
#if defined RTL8192SE || defined RTL8192CE
    fwinfo_size = 0;
#else
    fwinfo_size = sizeof(TX_FWINFO_8190PCI);
#endif
    header = (struct rtllib_hdr_1addr *)(((u8*)skb->data) + fwinfo_size);
    fc = header->frame_ctl;
    type = WLAN_FC_GET_TYPE(fc);
    stype = WLAN_FC_GET_STYPE(fc);
    pda_addr = header->addr1;
    if(is_multicast_ether_addr(pda_addr))
        multi_addr = true;
    else if(is_broadcast_ether_addr(pda_addr))
        broad_addr = true;
    else {
#ifdef _RTL8192_EXT_PATCH_
        //YJ,add,090518,for Keep alive
        priv->NumTxUnicast++;
#endif
        uni_addr = true;
    }
    if(uni_addr)
        priv->stats.txbytesunicast += skb->len - fwinfo_size;
    else if(multi_addr)
        priv->stats.txbytesmulticast += skb->len - fwinfo_size;
    else
        priv->stats.txbytesbroadcast += skb->len - fwinfo_size;

    spin_lock_irqsave(&priv->irq_th_lock,flags);
    ring = &priv->tx_ring[tcb_desc->queue_index];
    if (tcb_desc->queue_index != BEACON_QUEUE) {
        idx = (ring->idx + skb_queue_len(&ring->queue)) % ring->entries;
    } else {
        idx = 0;
    }

    pdesc = &ring->desc[idx];
    if((pdesc->OWN == 1) && (tcb_desc->queue_index != BEACON_QUEUE)) {
	    RT_TRACE(COMP_ERR,"No more TX desc@%d, ring->idx = %d,idx = %d,%x", \
			    tcb_desc->queue_index,ring->idx, idx,skb->len);
            //crash here if not
            spin_unlock_irqrestore(&priv->irq_th_lock,flags);
	    return skb->len;
    }
    if(tcb_desc->queue_index == MGNT_QUEUE)
    {
//	    printk("===>rtl8192_tx():skb_queue_len(&ring->queue) is %d,ring->idx is %d\n",skb_queue_len(&ring->queue),idx);
//	    printk("===>stype is %d\n",stype);
    }
    //added by amy for LED 090318
    if(type == RTLLIB_FTYPE_DATA){
	    if(priv->rtllib->LedControlHandler)
			priv->rtllib->LedControlHandler(dev, LED_CTL_TX);
	    //printk("=================== Tx: Len = %d\n", skb->len);
    }
    priv->ops->tx_fill_descriptor(dev, pdesc, tcb_desc, skb);
    __skb_queue_tail(&ring->queue, skb);
    pdesc->OWN = 1;
    spin_unlock_irqrestore(&priv->irq_th_lock,flags);
    dev->trans_start = jiffies;
#ifdef _RTL8192_EXT_PATCH_
    if(tcb_desc->queue_index != BEACON_QUEUE)
#endif
#ifdef RTL8192CE
    write_nic_word(dev, REG_PCIE_CTRL_REG, BIT0<<(tcb_desc->queue_index));
#else
    write_nic_word(dev,TPPoll,0x01<<tcb_desc->queue_index);
#endif
    return 0;
}

short rtl8192_alloc_rx_desc_ring(struct net_device *dev)
{
    struct r8192_priv *priv = rtllib_priv(dev);
    rx_desc *entry = NULL;
    int i;

    priv->rx_ring = pci_alloc_consistent(priv->pdev,
            sizeof(*priv->rx_ring) * priv->rxringcount, &priv->rx_ring_dma);

    if (!priv->rx_ring || (unsigned long)priv->rx_ring & 0xFF) {
        RT_TRACE(COMP_ERR,"Cannot allocate RX ring\n");
        return -ENOMEM;
    }

    memset(priv->rx_ring, 0, sizeof(*priv->rx_ring) * priv->rxringcount);
    priv->rx_idx = 0;

    for (i = 0; i < priv->rxringcount; i++) {
        struct sk_buff *skb = dev_alloc_skb(priv->rxbuffersize);
        dma_addr_t *mapping;
        entry = &priv->rx_ring[i];
        if (!skb)
            return 0;
        skb->dev = dev;
        priv->rx_buf[i] = skb;
        mapping = (dma_addr_t *)skb->cb;
        *mapping = pci_map_single(priv->pdev, skb_tail_pointer(skb),
                priv->rxbuffersize, PCI_DMA_FROMDEVICE);

        entry->BufferAddress = cpu_to_le32(*mapping);

        entry->Length = priv->rxbuffersize;
        entry->OWN = 1;
    }

    entry->EOR = 1;
    return 0;
}

static int rtl8192_alloc_tx_desc_ring(struct net_device *dev,
        unsigned int prio, unsigned int entries)
{
    struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);
    tx_desc *ring;
    dma_addr_t dma;
    int i;

    ring = pci_alloc_consistent(priv->pdev, sizeof(*ring) * entries, &dma);
    if (!ring || (unsigned long)ring & 0xFF) {
        RT_TRACE(COMP_ERR, "Cannot allocate TX ring (prio = %d)\n", prio);
        return -ENOMEM;
    }

    memset(ring, 0, sizeof(*ring)*entries);
    priv->tx_ring[prio].desc = ring;
    priv->tx_ring[prio].dma = dma;
    priv->tx_ring[prio].idx = 0;
    priv->tx_ring[prio].entries = entries;
    skb_queue_head_init(&priv->tx_ring[prio].queue);

    for (i = 0; i < entries; i++)
        ring[i].NextDescAddress =
            cpu_to_le32((u32)dma + ((i + 1) % entries) * sizeof(*ring));

    return 0;
}


short rtl8192_pci_initdescring(struct net_device *dev)
{
    u32 ret;
    int i;
    struct r8192_priv *priv = rtllib_priv(dev);

    ret = rtl8192_alloc_rx_desc_ring(dev);
    if (ret) {
        return ret;
    }


    // general process for other queue */
    for (i = 0; i < MAX_TX_QUEUE_COUNT; i++) {
        if ((ret = rtl8192_alloc_tx_desc_ring(dev, i, priv->txringcount)))
            goto err_free_rings;
    }

#if 0
    // specific process for hardware beacon process */
    if ((ret = rtl8192_alloc_tx_desc_ring(dev, MAX_TX_QUEUE_COUNT - 1, 2)))
        goto err_free_rings;
#endif

    return 0;

err_free_rings:
    rtl8192_free_rx_ring(dev);
    for (i = 0; i < MAX_TX_QUEUE_COUNT; i++)
        if (priv->tx_ring[i].desc)
            rtl8192_free_tx_ring(dev, i);
    return 1;
}

void rtl8192_pci_resetdescring(struct net_device *dev)
{
    struct r8192_priv *priv = rtllib_priv(dev);
    int i;
    unsigned long flags = 0;
    // force the rx_idx to the first one */
    if(priv->rx_ring) {
        rx_desc *entry = NULL;
        for (i = 0; i < priv->rxringcount; i++) {
            entry = &priv->rx_ring[i];
            entry->OWN = 1;
        }
        priv->rx_idx = 0;
    }

    // after reset, release previous pending packet, and force the
    // tx idx to the first one */
    spin_lock_irqsave(&priv->irq_th_lock,flags);
    for (i = 0; i < MAX_TX_QUEUE_COUNT; i++) {
        if (priv->tx_ring[i].desc) {
            struct rtl8192_tx_ring *ring = &priv->tx_ring[i];

            while (skb_queue_len(&ring->queue)) {
                tx_desc *entry = &ring->desc[ring->idx];
                struct sk_buff *skb = __skb_dequeue(&ring->queue);

                pci_unmap_single(priv->pdev, le32_to_cpu(entry->TxBuffAddr),
                        skb->len, PCI_DMA_TODEVICE);
                kfree_skb(skb);
                ring->idx = (ring->idx + 1) % ring->entries;
            }
            ring->idx = 0;
        }
    }
    spin_unlock_irqrestore(&priv->irq_th_lock,flags);
}

//Function:     UpdateRxPktTimeStamp
//Overview:     Recored down the TSF time stamp when receiving a packet
//
//Input:
//      PADAPTER        dev
//      PRT_RFD         pRfd,
//
//Output:
//      PRT_RFD         pRfd
//                              (pRfd->Status.TimeStampHigh is updated)
//                              (pRfd->Status.TimeStampLow is updated)
//Return:
//              None
//
void UpdateRxPktTimeStamp8190 (struct net_device *dev, struct rtllib_rx_stats *stats)
{
	struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);

	if(stats->bIsAMPDU && !stats->bFirstMPDU) {
		stats->mac_time[0] = priv->LastRxDescTSFLow;
		stats->mac_time[1] = priv->LastRxDescTSFHigh;
	} else {
		priv->LastRxDescTSFLow = stats->mac_time[0];
		priv->LastRxDescTSFHigh = stats->mac_time[1];
	}
}

#ifdef _RTL8192_EXT_PATCH_
long rtl819x_translate_todbm(struct r8192_priv * priv, u8 signal_strength_index	)// 0-100 index.
#else
long rtl819x_translate_todbm(u8 signal_strength_index	)// 0-100 index.
#endif
{
	long	signal_power; // in dBm.

#ifdef _RTL8192_EXT_PATCH_
	if(priv->CustomerID == RT_CID_819x_Lenovo)
	{	// Modify for Lenovo
		signal_power = (long)signal_strength_index;
		if(signal_power >= 45)
			signal_power -= 110;
		else
		{
			signal_power = ((signal_power*6)/10);
			signal_power -= 93;
		}
		//DbgPrint("Lenovo Translate to dbm = %d = %d\n", SignalStrengthIndex ,SignalPower);
		return signal_power;
	}
#endif
	// Translate to dBm (x=0.5y-95).
	signal_power = (long)((signal_strength_index + 1) >> 1);
	signal_power -= 95;

	return signal_power;
}

//
//	Description:
//		Update Rx signal related information in the packet reeived
//		to RxStats. User application can query RxStats to realize
//		current Rx signal status.
//
//	Assumption:
//		In normal operation, user only care about the information of the BSS
//		and we shall invoke this function if the packet received is from the BSS.
//
#ifdef _RTL8192_EXT_PATCH_
void rtl819x_update_rxsignalstatistics8192S(
	struct r8192_priv * priv,
	struct rtllib_rx_stats * pstats
	)
{
	//HAL_DATA_TYPE			*pHalData = GET_HAL_DATA(Adapter);
	int	weighting = 0;

	//2 <ToDo> Update Rx Statistics (such as signal strength and signal quality).

	// Initila state
	if(priv->stats.recv_signal_power == 0)
		priv->stats.recv_signal_power = pstats->RecvSignalPower;

	// To avoid the past result restricting the statistics sensitivity, weight the current power (5/6) to speed up the
	// reaction of smoothed Signal Power.
	if(pstats->RecvSignalPower > priv->stats.recv_signal_power)
		weighting = 5;
	else if(pstats->RecvSignalPower < priv->stats.recv_signal_power)
		weighting = (-5);
	//
	// We need more correct power of received packets and the  "SignalStrength" of RxStats have been beautified or translated,
	// so we record the correct power in Dbm here. By Bruce, 2008-03-07.
	//
	priv->stats.recv_signal_power = (priv->stats.recv_signal_power * 5 + pstats->RecvSignalPower + weighting) / 6;

}	// UpdateRxSignalStatistics8192S
#endif
void
rtl819x_update_rxsignalstatistics8190pci(
	struct r8192_priv * priv,
	struct rtllib_rx_stats * pprevious_stats
	)
{
	int weighting = 0;

	//2 <ToDo> Update Rx Statistics (such as signal strength and signal quality).

	// Initila state
	if(priv->stats.recv_signal_power == 0)
		priv->stats.recv_signal_power = pprevious_stats->RecvSignalPower;

	// To avoid the past result restricting the statistics sensitivity, weight the current power (5/6) to speed up the
	// reaction of smoothed Signal Power.
	if(pprevious_stats->RecvSignalPower > priv->stats.recv_signal_power)
		weighting = 5;
	else if(pprevious_stats->RecvSignalPower < priv->stats.recv_signal_power)
		weighting = (-5);
	//
	// We need more correct power of received packets and the  "SignalStrength" of RxStats have been beautified or translated,
	// so we record the correct power in Dbm here. By Bruce, 2008-03-07.
	//
	priv->stats.recv_signal_power = (priv->stats.recv_signal_power * 5 + pprevious_stats->RecvSignalPower + weighting) / 6;
}

void
rtl8190_process_cck_rxpathsel(
	struct r8192_priv * priv,
	struct rtllib_rx_stats * pprevious_stats
	)
{
#ifdef RTL8190P	//Only 90P 2T4R need to check
	char				last_cck_adc_pwdb[4]={0,0,0,0};
	u8				i;
//cosa add for Rx path selection
		if(priv->rf_type == RF_2T4R && DM_RxPathSelTable.Enable)
		{
			if(pprevious_stats->bIsCCK &&
				(pprevious_stats->bPacketToSelf ||pprevious_stats->bPacketBeacon))
			{
				// record the cck adc_pwdb to the sliding window. */
				if(priv->stats.cck_adc_pwdb.TotalNum++ >= PHY_RSSI_SLID_WIN_MAX)
				{
					priv->stats.cck_adc_pwdb.TotalNum = PHY_RSSI_SLID_WIN_MAX;
					for(i=RF90_PATH_A; i<RF90_PATH_MAX; i++)
					{
						last_cck_adc_pwdb[i] = priv->stats.cck_adc_pwdb.elements[i][priv->stats.cck_adc_pwdb.index];
						priv->stats.cck_adc_pwdb.TotalVal[i] -= last_cck_adc_pwdb[i];
					}
				}
				for(i=RF90_PATH_A; i<RF90_PATH_MAX; i++)
				{
					priv->stats.cck_adc_pwdb.TotalVal[i] += pprevious_stats->cck_adc_pwdb[i];
					priv->stats.cck_adc_pwdb.elements[i][priv->stats.cck_adc_pwdb.index] = pprevious_stats->cck_adc_pwdb[i];
				}
				priv->stats.cck_adc_pwdb.index++;
				if(priv->stats.cck_adc_pwdb.index >= PHY_RSSI_SLID_WIN_MAX)
					priv->stats.cck_adc_pwdb.index = 0;

				for(i=RF90_PATH_A; i<RF90_PATH_MAX; i++)
				{
					DM_RxPathSelTable.cck_pwdb_sta[i] = priv->stats.cck_adc_pwdb.TotalVal[i]/priv->stats.cck_adc_pwdb.TotalNum;
				}

				for(i=RF90_PATH_A; i<RF90_PATH_MAX; i++)
				{
					if(pprevious_stats->cck_adc_pwdb[i]  > (char)priv->undecorated_smoothed_cck_adc_pwdb[i])
					{
						priv->undecorated_smoothed_cck_adc_pwdb[i] =
							( (priv->undecorated_smoothed_cck_adc_pwdb[i]*(Rx_Smooth_Factor-1)) +
							(pprevious_stats->cck_adc_pwdb[i])) /(Rx_Smooth_Factor);
						priv->undecorated_smoothed_cck_adc_pwdb[i] = priv->undecorated_smoothed_cck_adc_pwdb[i] + 1;
					}
					else
					{
						priv->undecorated_smoothed_cck_adc_pwdb[i] =
							( (priv->undecorated_smoothed_cck_adc_pwdb[i]*(Rx_Smooth_Factor-1)) +
							(pprevious_stats->cck_adc_pwdb[i])) /(Rx_Smooth_Factor);
					}
				}
			}
		}
#endif
}

#ifdef _RTL8192_EXT_PATCH_
void Process_UI_RSSI_8192S(struct r8192_priv * priv,struct rtllib_rx_stats * pstats)
{
	u8			rfPath;
	u32			last_rssi, tmpVal;

	if(pstats->bPacketToSelf || pstats->bPacketBeacon)
	{
		priv->stats.RssiCalculateCnt++;	//For antenna Test
		if(priv->stats.ui_rssi.TotalNum++ >= PHY_RSSI_SLID_WIN_MAX)
		{
			priv->stats.ui_rssi.TotalNum = PHY_RSSI_SLID_WIN_MAX;
			last_rssi = priv->stats.ui_rssi.elements[priv->stats.ui_rssi.index];
			priv->stats.ui_rssi.TotalVal -= last_rssi;
		}
		priv->stats.ui_rssi.TotalVal += pstats->SignalStrength;

		priv->stats.ui_rssi.elements[priv->stats.ui_rssi.index++] = pstats->SignalStrength;
		if(priv->stats.ui_rssi.index >= PHY_RSSI_SLID_WIN_MAX)
			priv->stats.ui_rssi.index = 0;

		// <1> Showed on UI for user, in dbm
		tmpVal = priv->stats.ui_rssi.TotalVal/priv->stats.ui_rssi.TotalNum;
		priv->stats.signal_strength = rtl819x_translate_todbm(priv, (u8)tmpVal);

	}

	// <2> Showed on UI for engineering
	// hardware does not provide rssi information for each rf path in CCK
	if(!pstats->bIsCCK && pstats->bPacketToSelf)
	{
		for (rfPath = RF90_PATH_A; rfPath < priv->NumTotalRFPath; rfPath++)
		{
			if (!rtl8192_phy_CheckIsLegalRFPath(priv->rtllib->dev, rfPath))
				continue;
			//printk("pRfd->Status.RxMIMOSignalStrength[%d]  = %d \n", rfPath, pstats->RxMIMOSignalStrength[rfPath] );
			RT_TRACE(COMP_DBG, "pstats->RxMIMOSignalStrength[%d]  = %d \n", rfPath, pstats->RxMIMOSignalStrength[rfPath] );

			if(priv->stats.rx_rssi_percentage[rfPath] == 0)	// initialize
			{
				priv->stats.rx_rssi_percentage[rfPath] = pstats->RxMIMOSignalStrength[rfPath];
				//printk("MIMO RSSI initialize \n");
			}

			if(pstats->RxMIMOSignalStrength[rfPath]  > priv->stats.rx_rssi_percentage[rfPath])
			{
				priv->stats.rx_rssi_percentage[rfPath] =
					( (priv->stats.rx_rssi_percentage[rfPath]*(Rx_Smooth_Factor-1)) +
					(pstats->RxMIMOSignalStrength[rfPath])) /(Rx_Smooth_Factor);
				priv->stats.rx_rssi_percentage[rfPath] = priv->stats.rx_rssi_percentage[rfPath] + 1;
			}
			else
			{
				priv->stats.rx_rssi_percentage[rfPath] =
					( (priv->stats.rx_rssi_percentage[rfPath]*(Rx_Smooth_Factor-1)) +
					(pstats->RxMIMOSignalStrength[rfPath])) /(Rx_Smooth_Factor);
			}
			RT_TRACE(COMP_DBG, "priv->stats.rx_rssi_percentage[%d]  = %d \n",rfPath, priv->stats.rx_rssi_percentage[rfPath] );
			//DbgPrint("Adapter->RxStats.RxRSSIPercentage[%d]  = %d \n",rfPath, Adapter->RxStats.RxRSSIPercentage[rfPath] );
		}
	}

}	// Process_UI_RSSI_8192S

void Process_PWDB_8192S(struct r8192_priv * priv,struct rtllib_rx_stats * pstats,struct rtllib_network* pnet)
{
	long	UndecoratedSmoothedPWDB=0;

	if(pnet){
		if(priv->mshobj->ext_patch_get_peermp_rssi_param)
		UndecoratedSmoothedPWDB = priv->mshobj->ext_patch_get_peermp_rssi_param(pnet);//pEntry->rssi_stat.UndecoratedSmoothedPWDB;
	}
	else
		UndecoratedSmoothedPWDB = priv->undecorated_smoothed_pwdb;

	if(pstats->bPacketToSelf || pstats->bPacketBeacon)
	{
		if(UndecoratedSmoothedPWDB < 0)	// initialize
		{
			UndecoratedSmoothedPWDB = pstats->RxPWDBAll;
		}

		if(pstats->RxPWDBAll > (u32)UndecoratedSmoothedPWDB)
		{
			UndecoratedSmoothedPWDB =
					( ((UndecoratedSmoothedPWDB)*(Rx_Smooth_Factor-1)) +
					(pstats->RxPWDBAll)) /(Rx_Smooth_Factor);
			UndecoratedSmoothedPWDB = UndecoratedSmoothedPWDB + 1;
		}
		else
		{
			UndecoratedSmoothedPWDB =
					( ((UndecoratedSmoothedPWDB)*(Rx_Smooth_Factor-1)) +
					(pstats->RxPWDBAll)) /(Rx_Smooth_Factor);
		}
		if(pnet)
		{
			if(priv->mshobj->ext_patch_set_peermp_rssi_param)
				priv->mshobj->ext_patch_set_peermp_rssi_param(pnet,UndecoratedSmoothedPWDB);
			//DbgPrint("pEntry->rssi_stat.UndecoratedSmoothedPWDB = 0x%x(%d)\n",
			//	pEntry->rssi_stat.UndecoratedSmoothedPWDB,
			//	pEntry->rssi_stat.UndecoratedSmoothedPWDB);
		}
		else
		{
			priv->undecorated_smoothed_pwdb = UndecoratedSmoothedPWDB;
			//DbgPrint("pHalData->UndecoratedSmoothedPWDB = 0x%x(%d)\n",
			//	pHalData->UndecoratedSmoothedPWDB,
			//	pHalData->UndecoratedSmoothedPWDB);
		}
		rtl819x_update_rxsignalstatistics8192S(priv, pstats);
	}
}

void Process_UiLinkQuality8192S(struct r8192_priv * priv,struct rtllib_rx_stats * pstats)
{
	u32	last_evm=0, nSpatialStream, tmpVal;

	if(pstats->SignalQuality != 0)
	{
		if (pstats->bPacketToSelf || pstats->bPacketBeacon)
		{
			//
			// 1. Record the general EVM to the sliding window.
			//
			if(priv->stats.ui_link_quality.TotalNum++ >= PHY_LINKQUALITY_SLID_WIN_MAX)
			{
				priv->stats.ui_link_quality.TotalNum = PHY_LINKQUALITY_SLID_WIN_MAX;
				last_evm = priv->stats.ui_link_quality.elements[priv->stats.ui_link_quality.index];
				priv->stats.ui_link_quality.TotalVal -= last_evm;
			}
			priv->stats.ui_link_quality.TotalVal += pstats->SignalQuality;

			priv->stats.ui_link_quality.elements[priv->stats.ui_link_quality.index++] = pstats->SignalQuality;
			if(priv->stats.ui_link_quality.index >= PHY_LINKQUALITY_SLID_WIN_MAX)
				priv->stats.ui_link_quality.index = 0;

			//RTPRINT(FRX, RX_PHY_SQ, ("Total SQ=%ld PreRfd->SQ = %d\n",
			//Adapter->RxStats.ui_link_quality.TotalVal, pRfd->Status.SignalQuality));

			// <1> Showed on UI for user, in percentage.
			tmpVal = priv->stats.ui_link_quality.TotalVal/priv->stats.ui_link_quality.TotalNum;
			priv->stats.signal_quality = tmpVal;
			//cosa add 10/11/2007, Showed on UI for user in Windows Vista, for Link quality.
			priv->stats.last_signal_strength_inpercent = tmpVal;

			//
			// <2> Showed on UI for engineering
			//
			for(nSpatialStream = 0; nSpatialStream<2 ; nSpatialStream++)	// 2 spatial stream
			{
				if(pstats->RxMIMOSignalQuality[nSpatialStream] != -1)
				{
					if(priv->stats.rx_evm_percentage[nSpatialStream] == 0)	// initialize
					{
						priv->stats.rx_evm_percentage[nSpatialStream] = pstats->RxMIMOSignalQuality[nSpatialStream];
					}
					priv->stats.rx_evm_percentage[nSpatialStream] =
					( (priv->stats.rx_evm_percentage[nSpatialStream]*(Rx_Smooth_Factor-1)) +
					(pstats->RxMIMOSignalQuality[nSpatialStream]* 1)) /(Rx_Smooth_Factor);
				}
			}
		}
	}
	else
		;//printk(" PreRfd->SQ=%d\n", pstats->SignalQuality);

}	// Process_UiLinkQuality8192S
#endif
// 2008/01/22 MH We can not delcare RSSI/EVM total value of sliding window to
//	be a local static. Otherwise, it may increase when we return from S3/S4. The
//	value will be kept in memory or disk. We must delcare the value in adapter
//	and it will be reinitialized when return from S3/S4. */
#ifdef _RTL8192_EXT_PATCH_
void rtl8192_process_phyinfo(struct r8192_priv * priv, u8* buffer,struct rtllib_rx_stats * pcurrent_stats,struct rtllib_network * pnet)
#else
void rtl8192_process_phyinfo(struct r8192_priv * priv, u8* buffer,struct rtllib_rx_stats * pprevious_stats, struct rtllib_rx_stats * pcurrent_stats)
#endif
{
#ifdef _RTL8192_EXT_PATCH_
	//
	// If the packet does not match the criteria, neglect it
	//
	if(!pcurrent_stats->bPacketMatchBSSID)
		return;
	//
	// Check RSSI
	//
	Process_UI_RSSI_8192S(priv, pcurrent_stats);
	//
	// Check PWDB.
	//
	Process_PWDB_8192S(priv, pcurrent_stats, pnet);
	//
	// Check EVM
	//
	Process_UiLinkQuality8192S(priv, pcurrent_stats);
#else
	bool bcheck = false;
	u8	rfpath;
	u32 nspatial_stream, tmp_val;
	//u8	i;
	static u32 slide_rssi_index=0, slide_rssi_statistics=0;
	static u32 slide_evm_index=0, slide_evm_statistics=0;
	static u32 last_rssi=0, last_evm=0;
	//cosa add for rx path selection
//	static long slide_cck_adc_pwdb_index=0, slide_cck_adc_pwdb_statistics=0;
//	static char last_cck_adc_pwdb[4]={0,0,0,0};
	//cosa add for beacon rssi smoothing
	static u32 slide_beacon_adc_pwdb_index=0, slide_beacon_adc_pwdb_statistics=0;
	static u32 last_beacon_adc_pwdb=0;

	struct rtllib_hdr_3addr *hdr;
	u16 sc ;
	unsigned int frag,seq;
	hdr = (struct rtllib_hdr_3addr *)buffer;
	sc = le16_to_cpu(hdr->seq_ctl);
	frag = WLAN_GET_SEQ_FRAG(sc);
	seq = WLAN_GET_SEQ_SEQ(sc);
	//cosa add 04292008 to record the sequence number
	pcurrent_stats->Seq_Num = seq;
	//
	// Check whether we should take the previous packet into accounting
	//
	if(!pprevious_stats->bIsAMPDU)
	{
		// if previous packet is not aggregated packet
		bcheck = true;
	}else
	{
//remve for that we don't use AMPDU to calculate PWDB,because the reported PWDB of some AP is fault.
#if 0
		// if previous packet is aggregated packet, and current packet
		//	(1) is not AMPDU
		//	(2) is the first packet of one AMPDU
		// that means the previous packet is the last one aggregated packet
		if( !pcurrent_stats->bIsAMPDU || pcurrent_stats->bFirstMPDU)
			bcheck = true;
#endif
	}

	if(slide_rssi_statistics++ >= PHY_RSSI_SLID_WIN_MAX)
	{
		slide_rssi_statistics = PHY_RSSI_SLID_WIN_MAX;
		last_rssi = priv->stats.slide_signal_strength[slide_rssi_index];
		priv->stats.slide_rssi_total -= last_rssi;
	}
	priv->stats.slide_rssi_total += pprevious_stats->SignalStrength;

	priv->stats.slide_signal_strength[slide_rssi_index++] = pprevious_stats->SignalStrength;
	if(slide_rssi_index >= PHY_RSSI_SLID_WIN_MAX)
		slide_rssi_index = 0;

	// <1> Showed on UI for user, in dbm
	tmp_val = priv->stats.slide_rssi_total/slide_rssi_statistics;
	priv->stats.signal_strength = rtl819x_translate_todbm((u8)tmp_val);
	pcurrent_stats->rssi = priv->stats.signal_strength;
	//
	// If the previous packet does not match the criteria, neglect it
	//
	if(!pprevious_stats->bPacketMatchBSSID)
	{
		if(!pprevious_stats->bToSelfBA)
			return;
	}

	if(!bcheck)
		return;

	rtl8190_process_cck_rxpathsel(priv,pprevious_stats);

	//
	// Check RSSI
	//
	priv->stats.num_process_phyinfo++;
#if 0
	// record the general signal strength to the sliding window. */
	if(slide_rssi_statistics++ >= PHY_RSSI_SLID_WIN_MAX)
	{
		slide_rssi_statistics = PHY_RSSI_SLID_WIN_MAX;
		last_rssi = priv->stats.slide_signal_strength[slide_rssi_index];
		priv->stats.slide_rssi_total -= last_rssi;
	}
	priv->stats.slide_rssi_total += pprevious_stats->SignalStrength;

	priv->stats.slide_signal_strength[slide_rssi_index++] = pprevious_stats->SignalStrength;
	if(slide_rssi_index >= PHY_RSSI_SLID_WIN_MAX)
		slide_rssi_index = 0;

	// <1> Showed on UI for user, in dbm
	tmp_val = priv->stats.slide_rssi_total/slide_rssi_statistics;
	priv->stats.signal_strength = rtl819x_translate_todbm((u8)tmp_val);

#endif
	// <2> Showed on UI for engineering
	// hardware does not provide rssi information for each rf path in CCK
	if(!pprevious_stats->bIsCCK && pprevious_stats->bPacketToSelf)
	{
		for (rfpath = RF90_PATH_A; rfpath < RF90_PATH_C; rfpath++)
		{
			if (!rtl8192_phy_CheckIsLegalRFPath(priv->rtllib->dev, rfpath))
				continue;
			RT_TRACE(COMP_DBG,"Jacken -> pPreviousstats->RxMIMOSignalStrength[rfpath]  = %d \n" ,pprevious_stats->RxMIMOSignalStrength[rfpath] );
			//Fixed by Jacken 2008-03-20
			if(priv->stats.rx_rssi_percentage[rfpath] == 0)
			{
				priv->stats.rx_rssi_percentage[rfpath] = pprevious_stats->RxMIMOSignalStrength[rfpath];
				//DbgPrint("MIMO RSSI initialize \n");
			}
			if(pprevious_stats->RxMIMOSignalStrength[rfpath]  > priv->stats.rx_rssi_percentage[rfpath])
			{
				priv->stats.rx_rssi_percentage[rfpath] =
					( (priv->stats.rx_rssi_percentage[rfpath]*(Rx_Smooth_Factor-1)) +
					(pprevious_stats->RxMIMOSignalStrength[rfpath])) /(Rx_Smooth_Factor);
				priv->stats.rx_rssi_percentage[rfpath] = priv->stats.rx_rssi_percentage[rfpath]  + 1;
			}
			else
			{
				priv->stats.rx_rssi_percentage[rfpath] =
					( (priv->stats.rx_rssi_percentage[rfpath]*(Rx_Smooth_Factor-1)) +
					(pprevious_stats->RxMIMOSignalStrength[rfpath])) /(Rx_Smooth_Factor);
			}
			RT_TRACE(COMP_DBG,"Jacken -> priv->RxStats.RxRSSIPercentage[rfPath]  = %d \n" ,priv->stats.rx_rssi_percentage[rfpath] );
		}
	}


	//
	// Check PWDB.
	//
	//cosa add for beacon rssi smoothing by average.
	if(pprevious_stats->bPacketBeacon)
	{
		// record the beacon pwdb to the sliding window. */
		if(slide_beacon_adc_pwdb_statistics++ >= PHY_Beacon_RSSI_SLID_WIN_MAX)
		{
			slide_beacon_adc_pwdb_statistics = PHY_Beacon_RSSI_SLID_WIN_MAX;
			last_beacon_adc_pwdb = priv->stats.Slide_Beacon_pwdb[slide_beacon_adc_pwdb_index];
			priv->stats.Slide_Beacon_Total -= last_beacon_adc_pwdb;
			//DbgPrint("slide_beacon_adc_pwdb_index = %d, last_beacon_adc_pwdb = %d, dev->RxStats.Slide_Beacon_Total = %d\n",
			//	slide_beacon_adc_pwdb_index, last_beacon_adc_pwdb, dev->RxStats.Slide_Beacon_Total);
		}
		priv->stats.Slide_Beacon_Total += pprevious_stats->RxPWDBAll;
		priv->stats.Slide_Beacon_pwdb[slide_beacon_adc_pwdb_index] = pprevious_stats->RxPWDBAll;
		//DbgPrint("slide_beacon_adc_pwdb_index = %d, pPreviousRfd->Status.RxPWDBAll = %d\n", slide_beacon_adc_pwdb_index, pPreviousRfd->Status.RxPWDBAll);
		slide_beacon_adc_pwdb_index++;
		if(slide_beacon_adc_pwdb_index >= PHY_Beacon_RSSI_SLID_WIN_MAX)
			slide_beacon_adc_pwdb_index = 0;
		pprevious_stats->RxPWDBAll = priv->stats.Slide_Beacon_Total/slide_beacon_adc_pwdb_statistics;
		if(pprevious_stats->RxPWDBAll >= 3)
			pprevious_stats->RxPWDBAll -= 3;
	}

	RT_TRACE(COMP_RXDESC, "Smooth %s PWDB = %d\n",
				pprevious_stats->bIsCCK? "CCK": "OFDM",
				pprevious_stats->RxPWDBAll);

	if(pprevious_stats->bPacketToSelf || pprevious_stats->bPacketBeacon || pprevious_stats->bToSelfBA)
	{
		if(priv->undecorated_smoothed_pwdb < 0)	// initialize
		{
			priv->undecorated_smoothed_pwdb = pprevious_stats->RxPWDBAll;
			//DbgPrint("First pwdb initialize \n");
		}
#if 1
		if(pprevious_stats->RxPWDBAll > (u32)priv->undecorated_smoothed_pwdb)
		{
			priv->undecorated_smoothed_pwdb =
					( ((priv->undecorated_smoothed_pwdb)*(Rx_Smooth_Factor-1)) +
					(pprevious_stats->RxPWDBAll)) /(Rx_Smooth_Factor);
			priv->undecorated_smoothed_pwdb = priv->undecorated_smoothed_pwdb + 1;
		}
		else
		{
			priv->undecorated_smoothed_pwdb =
					( ((priv->undecorated_smoothed_pwdb)*(Rx_Smooth_Factor-1)) +
					(pprevious_stats->RxPWDBAll)) /(Rx_Smooth_Factor);
		}
#else
		//Fixed by Jacken 2008-03-20
		if(pPreviousRfd->Status.RxPWDBAll > (u32)pHalData->UndecoratedSmoothedPWDB)
		{
			pHalData->UndecoratedSmoothedPWDB =
					( ((pHalData->UndecoratedSmoothedPWDB)* 5) + (pPreviousRfd->Status.RxPWDBAll)) / 6;
			pHalData->UndecoratedSmoothedPWDB = pHalData->UndecoratedSmoothedPWDB + 1;
		}
		else
		{
			pHalData->UndecoratedSmoothedPWDB =
					( ((pHalData->UndecoratedSmoothedPWDB)* 5) + (pPreviousRfd->Status.RxPWDBAll)) / 6;
		}
#endif
		rtl819x_update_rxsignalstatistics8190pci(priv,pprevious_stats);
	}

	//
	// Check EVM
	//
	// record the general EVM to the sliding window. */
	if(pprevious_stats->SignalQuality == 0)
	{
	}
	else
	{
		if(pprevious_stats->bPacketToSelf || pprevious_stats->bPacketBeacon || pprevious_stats->bToSelfBA){
			if(slide_evm_statistics++ >= PHY_RSSI_SLID_WIN_MAX){
				slide_evm_statistics = PHY_RSSI_SLID_WIN_MAX;
				last_evm = priv->stats.slide_evm[slide_evm_index];
				priv->stats.slide_evm_total -= last_evm;
			}

			priv->stats.slide_evm_total += pprevious_stats->SignalQuality;

			priv->stats.slide_evm[slide_evm_index++] = pprevious_stats->SignalQuality;
			if(slide_evm_index >= PHY_RSSI_SLID_WIN_MAX)
				slide_evm_index = 0;

			// <1> Showed on UI for user, in percentage.
			tmp_val = priv->stats.slide_evm_total/slide_evm_statistics;
			priv->stats.signal_quality = tmp_val;
			//cosa add 10/11/2007, Showed on UI for user in Windows Vista, for Link quality.
			priv->stats.last_signal_strength_inpercent = tmp_val;
		}

		// <2> Showed on UI for engineering
		if(pprevious_stats->bPacketToSelf || pprevious_stats->bPacketBeacon || pprevious_stats->bToSelfBA)
		{
			for(nspatial_stream = 0; nspatial_stream<2 ; nspatial_stream++) // 2 spatial stream
			{
				if(pprevious_stats->RxMIMOSignalQuality[nspatial_stream] != -1)
				{
					if(priv->stats.rx_evm_percentage[nspatial_stream] == 0)	// initialize
					{
						priv->stats.rx_evm_percentage[nspatial_stream] = pprevious_stats->RxMIMOSignalQuality[nspatial_stream];
					}
					priv->stats.rx_evm_percentage[nspatial_stream] =
						( (priv->stats.rx_evm_percentage[nspatial_stream]* (Rx_Smooth_Factor-1)) +
						(pprevious_stats->RxMIMOSignalQuality[nspatial_stream]* 1)) / (Rx_Smooth_Factor);
				}
			}
		}
	}
#endif

}

//-----------------------------------------------------------------------------
// Function:	rtl819x_query_rxpwrpercentage()
//
// Overview:
//
// Input:		char		antpower
//
// Output:		NONE
//
// Return:		0-100 percentage
//
// Revised History:
//	When		Who	Remark
//	05/26/2008	amy	Create Version 0 porting from windows code.
//
//---------------------------------------------------------------------------*/
//#ifndef RTL8192SE
static u8 rtl819x_query_rxpwrpercentage(
	char		antpower
	)
{
	if ((antpower <= -100) || (antpower >= 20))
	{
		return	0;
	}
	else if (antpower >= 0)
	{
		return	100;
	}
	else
	{
		return	(100+antpower);
	}

}	/* QueryRxPwrPercentage */

static u8
rtl819x_evm_dbtopercentage(
	char value
	)
{
	char ret_val;

	ret_val = value;

	if(ret_val >= 0)
		ret_val = 0;
	if(ret_val <= -33)
		ret_val = -33;
	ret_val = 0 - ret_val;
	ret_val*=3;
	if(ret_val == 99)
		ret_val = 100;
	return(ret_val);
}
//#endif
//
//	Description:
//	We want good-looking for signal strength/quality
//	2007/7/19 01:09, by cosa.
//
long
rtl819x_signal_scale_mapping(struct r8192_priv * priv,
	long currsig
	)
{
	long retsig;

#if defined RTL8192SE || defined RTL8192CE
	if(priv->CustomerID == RT_CID_819x_Lenovo)
	{
		return currsig;
	}
	else if(priv->CustomerID == RT_CID_819x_Netcore)
	{	// Easier for 100%, 2009.04.13, by bohn
		if(currsig >= 31 && currsig <= 100)
		{
			retsig = 100;
		}
		else if(currsig >= 21 && currsig <= 30)
		{
			retsig = 90 + ((currsig - 20) / 1);
		}
		else if(currsig >= 11 && currsig <= 20)
		{
			retsig = 80 + ((currsig - 10) / 1);
		}
		else if(currsig >= 7 && currsig <= 10)
		{
			retsig = 69 + (currsig - 7);
		}
		else if(currsig == 6)
		{
			retsig = 54;
		}
		else if(currsig == 5)
		{
			retsig = 45;
		}
		else if(currsig == 4)
		{
			retsig = 36;
		}
		else if(currsig == 3)
		{
			retsig = 27;
		}
		else if(currsig == 2)
		{
			retsig = 18;
		}
		else if(currsig == 1)
		{
			retsig = 9;
		}
		else
		{
			retsig = currsig;
		}
		return retsig;
	}
#endif

	// Step 1. Scale mapping.
	if(currsig >= 61 && currsig <= 100)
	{
		retsig = 90 + ((currsig - 60) / 4);
	}
	else if(currsig >= 41 && currsig <= 60)
	{
		retsig = 78 + ((currsig - 40) / 2);
	}
	else if(currsig >= 31 && currsig <= 40)
	{
		retsig = 66 + (currsig - 30);
	}
	else if(currsig >= 21 && currsig <= 30)
	{
		retsig = 54 + (currsig - 20);
	}
	else if(currsig >= 5 && currsig <= 20)
	{
		retsig = 42 + (((currsig - 5) * 2) / 3);
	}
	else if(currsig == 4)
	{
		retsig = 36;
	}
	else if(currsig == 3)
	{
		retsig = 27;
	}
	else if(currsig == 2)
	{
		retsig = 18;
	}
	else if(currsig == 1)
	{
		retsig = 9;
	}
	else
	{
		retsig = currsig;
	}

	return retsig;
}
#if defined RTL8192SE || defined RTL8192CE
#define		rx_hal_is_cck_rate(_pdesc)\
			(_pdesc->RxMCS == DESC92S_RATE1M ||\
			 _pdesc->RxMCS == DESC92S_RATE2M ||\
			 _pdesc->RxMCS == DESC92S_RATE5_5M ||\
			 _pdesc->RxMCS == DESC92S_RATE11M)
#ifdef _RTL8192_EXT_PATCH_
void rtl8192_query_rxphystatus(
	struct r8192_priv * priv,
	struct rtllib_rx_stats * pstats,
	prx_desc  pdesc,
	prx_fwinfo   pDrvInfo,
	bool bpacket_match_bssid,
	bool bpacket_toself,
	bool bPacketBeacon
	)
{
	bool is_cck_rate;
	phy_sts_cck_8192s_t* cck_buf;
	u8 rx_pwr_all=0, rx_pwr[4], rf_rx_num=0, EVM, PWDB_ALL;
	u8 i, max_spatial_stream;
	u32 rssi, total_rssi = 0;
	u8 cck_highpwr = 0;
	is_cck_rate = rx_hal_is_cck_rate(pdesc);

	pstats->bPacketMatchBSSID = bpacket_match_bssid;
	pstats->bPacketToSelf = bpacket_toself;
	pstats->bIsCCK = is_cck_rate;//RX_HAL_IS_CCK_RATE(pDrvInfo);
	pstats->bPacketBeacon = bPacketBeacon;

	pstats->RxMIMOSignalQuality[0] = -1;
	pstats->RxMIMOSignalQuality[1] = -1;

	if (is_cck_rate){
		u8 report;
		//s8 cck_adc_pwdb[4];

		cck_buf = (phy_sts_cck_8192s_t*)pDrvInfo;

		if(priv->rtllib->eRFPowerState == eRfOn)
			cck_highpwr = (u8)priv->bCckHighPower;
		else
			cck_highpwr = false;
		if (!cck_highpwr){
			report = cck_buf->cck_agc_rpt & 0xc0;
			report = report >> 6;
			switch(report){
				case 0x3:
					rx_pwr_all = -40 - (cck_buf->cck_agc_rpt&0x3e);
					break;
				case 0x2:
					rx_pwr_all = -20 - (cck_buf->cck_agc_rpt&0x3e);
					break;
				case 0x1:
					rx_pwr_all = -2 - (cck_buf->cck_agc_rpt&0x3e);
					break;
				case 0x0:
					rx_pwr_all = 14 - (cck_buf->cck_agc_rpt&0x3e);
					break;
			}
		}
		else{
			report = pDrvInfo->cfosho[0] & 0x60;
			report = report >> 5;
			switch(report){
				case 0x3:
					rx_pwr_all = -40 - ((cck_buf->cck_agc_rpt & 0x1f)<<1);
					break;
				case 0x2:
					rx_pwr_all = -20 - ((cck_buf->cck_agc_rpt & 0x1f)<<1);
					break;
				case 0x1:
					rx_pwr_all = -2 - ((cck_buf->cck_agc_rpt & 0x1f)<<1);
					break;
				case 0x0:
					rx_pwr_all = 14 - ((cck_buf->cck_agc_rpt & 0x1f)<<1);
					break;
			}
		}

		PWDB_ALL= rtl819x_query_rxpwrpercentage(rx_pwr_all);
		//if(pMgntInfo->CustomerID == RT_CID_819x_Lenovo)
		{
			// CCK gain is smaller than OFDM/MCS gain,
			// so we add gain diff by experiences, the val is 6
			PWDB_ALL+=6;
			if(PWDB_ALL > 100)
				PWDB_ALL = 100;
			// modify the offset to make the same gain index with OFDM.
			if(PWDB_ALL > 34 && PWDB_ALL <= 42)
				PWDB_ALL -= 2;
			else if(PWDB_ALL > 26 && PWDB_ALL <= 34)
				PWDB_ALL -= 6;
			else if(PWDB_ALL > 14 && PWDB_ALL <= 26)
				PWDB_ALL -= 8;
			else if(PWDB_ALL > 4 && PWDB_ALL <= 14)
				PWDB_ALL -= 4;
		}
		pstats->RxPWDBAll = PWDB_ALL;
		pstats->RecvSignalPower = rx_pwr_all;

		//get Signal Quality(EVM)
		if (bpacket_match_bssid){
			u8 sq;
			if(priv->CustomerID == RT_CID_819x_Lenovo)
			{
				// mapping to 5 bars for vista signal strength
				// signal quality in driver will be displayed to signal strength
				// in vista.
				if(PWDB_ALL >= 50)
					sq = 100;
				else if(PWDB_ALL >= 35 && PWDB_ALL < 50)
					sq = 80;
				else if(PWDB_ALL >= 22 && PWDB_ALL < 35)
					sq = 60;
				else if(PWDB_ALL >= 18 && PWDB_ALL < 22)
					sq = 40;
				else
					sq = 20;
				//printk("cck PWDB_ALL=%d\n", PWDB_ALL);
			}
			else
			{
			if (pstats->RxPWDBAll > 40)
				sq = 100;
			else{
				sq = cck_buf->sq_rpt;
				if (sq > 64)
					sq = 0;
				else if(sq < 20)
					sq = 100;
				else
					sq = ((64-sq)*100)/44;
			}
			}
			pstats->SignalQuality = sq;
			pstats->RxMIMOSignalQuality[0] = sq;
			pstats->RxMIMOSignalQuality[1] = -1;
		}
	}
	else{
		priv->brfpath_rxenable[0] = priv->brfpath_rxenable[1] = true;

		for (i=RF90_PATH_A; i<RF90_PATH_MAX; i++){
			if (priv->brfpath_rxenable[i])
				rf_rx_num ++;
			//else
			//	break;

			rx_pwr[i] = ((pDrvInfo->gain_trsw[i]&0x3f)*2) - 110;
			rssi = rtl819x_query_rxpwrpercentage(rx_pwr[i]);
			total_rssi += rssi;

			priv->stats.rxSNRdB[i] = (long)(pDrvInfo->rxsnr[i]/2);

			if (bpacket_match_bssid){
				pstats->RxMIMOSignalStrength[i] = (u8)rssi;
				//The following is for lenovo signal strength in vista
				if(priv->CustomerID == RT_CID_819x_Lenovo)
				{
					u8	SQ;

					if(i == 0)
					{
						// mapping to 5 bars for vista signal strength
						// signal quality in driver will be displayed to signal strength
						// in vista.
						if(rssi >= 50)
							SQ = 100;
						else if(rssi >= 35 && rssi < 50)
							SQ = 80;
						else if(rssi >= 22 && rssi < 35)
							SQ = 60;
						else if(rssi >= 18 && rssi < 22)
							SQ = 40;
						else
							SQ = 20;
						//DbgPrint("ofdm/mcs RSSI=%d\n", RSSI);
						pstats->SignalQuality = SQ;
						//DbgPrint("ofdm/mcs SQ = %d\n", pRfd->Status.SignalQuality);
			}
		}
			}
		}
		//
		// (2)PWDB, Average PWDB cacluated by hardware (for rate adaptive)
		//
		rx_pwr_all = ((pDrvInfo->pwdb_all >> 1) & 0x7f) - 106;
		PWDB_ALL = rtl819x_query_rxpwrpercentage(rx_pwr_all);

		pstats->RxPWDBAll = PWDB_ALL;
		pstats->RxPower = rx_pwr_all;
		pstats->RecvSignalPower = rx_pwr_all;

		//EVM of HT rate
		if(priv->CustomerID != RT_CID_819x_Lenovo){
		if (pdesc->RxHT && pdesc->RxMCS >= DESC92S_RATEMCS8 && pdesc->RxMCS <= DESC92S_RATEMCS15)
			max_spatial_stream = 2;
		else
			max_spatial_stream = 1;

		for(i=0; i<max_spatial_stream; i++){
			EVM = rtl819x_evm_dbtopercentage(pDrvInfo->rxevm[i]);

			if (bpacket_match_bssid)
			{
				if (i==0)
						pstats->SignalQuality = (u8)(EVM & 0xff);
					pstats->RxMIMOSignalQuality[i] = (u8)(EVM&0xff);
				}
			}
		}
#if 1
		if (pdesc->BandWidth)
			priv->stats.received_bwtype[1+pDrvInfo->rxsc]++;
		else
			priv->stats.received_bwtype[0]++;
#endif
	}

	if (is_cck_rate){
		pstats->SignalStrength = (u8)(rtl819x_signal_scale_mapping(priv,PWDB_ALL));
	}
	else {
		if (rf_rx_num != 0)
			pstats->SignalStrength = (u8)(rtl819x_signal_scale_mapping(priv,total_rssi/=rf_rx_num));
	}
}
#else
void rtl8192_query_rxphystatus(
	struct r8192_priv * priv,
	struct rtllib_rx_stats * pstats,
	prx_desc  pdesc,
	prx_fwinfo   pDrvInfo,
	struct rtllib_rx_stats * precord_stats,
	bool bpacket_match_bssid,
	bool bpacket_toself,
	bool bPacketBeacon,
	bool bToSelfBA
	)
{
	bool is_cck_rate;
	phy_sts_cck_8192s_t* cck_buf;
	s8 rx_pwr_all=0, rx_pwr[4];
	u8 rf_rx_num=0, EVM, PWDB_ALL;
	u8 i, max_spatial_stream;
	u32 rssi, total_rssi = 0;

	is_cck_rate = rx_hal_is_cck_rate(pdesc);

	memset(precord_stats, 0, sizeof(struct rtllib_rx_stats));
	pstats->bPacketMatchBSSID = precord_stats->bPacketMatchBSSID = bpacket_match_bssid;
	pstats->bPacketToSelf = precord_stats->bPacketToSelf = bpacket_toself;
	pstats->bIsCCK = precord_stats->bIsCCK = is_cck_rate;//RX_HAL_IS_CCK_RATE(pDrvInfo);
	pstats->bPacketBeacon = precord_stats->bPacketBeacon = bPacketBeacon;
	pstats->bToSelfBA = precord_stats->bToSelfBA = bToSelfBA;
	pstats->bIsCCK = precord_stats->bIsCCK = is_cck_rate;

	pstats->RxMIMOSignalQuality[0] = -1;
	pstats->RxMIMOSignalQuality[1] = -1;
	precord_stats->RxMIMOSignalQuality[0] = -1;
	precord_stats->RxMIMOSignalQuality[1] = -1;

	if (is_cck_rate){
		u8 report, cck_highpwr;
		//s8 cck_adc_pwdb[4];

		cck_buf = (phy_sts_cck_8192s_t*)pDrvInfo;

		if(!priv->bInPowerSaveMode)
		cck_highpwr = (u8)rtl8192_QueryBBReg(priv->rtllib->dev, rFPGA0_XA_HSSIParameter2, BIT9);
		else
			cck_highpwr = false;
		if (!cck_highpwr){
			report = cck_buf->cck_agc_rpt & 0xc0;
			report = report >> 6;
			switch(report){
				case 0x3:
					rx_pwr_all = -35 - (cck_buf->cck_agc_rpt&0x3e);
					break;
				case 0x2:
					rx_pwr_all = -23 - (cck_buf->cck_agc_rpt&0x3e);
					break;
				case 0x1:
					rx_pwr_all = -11 - (cck_buf->cck_agc_rpt&0x3e);
					break;
				case 0x0:
					rx_pwr_all = 8 - (cck_buf->cck_agc_rpt&0x3e);
					break;
			}
		}
		else{
			report = pDrvInfo->cfosho[0] & 0x60;
			report = report >> 5;
			switch(report){
				case 0x3:
					rx_pwr_all = -35 - ((cck_buf->cck_agc_rpt & 0x1f)<<1);
					break;
				case 0x2:
					rx_pwr_all = -23 - ((cck_buf->cck_agc_rpt & 0x1f)<<1);
					break;
				case 0x1:
					rx_pwr_all = -11 - ((cck_buf->cck_agc_rpt & 0x1f)<<1);
					break;
				case 0x0:
					rx_pwr_all = -8 - ((cck_buf->cck_agc_rpt & 0x1f)<<1);
					break;
			}
		}

		PWDB_ALL= rtl819x_query_rxpwrpercentage(rx_pwr_all);
		pstats->RxPWDBAll = precord_stats->RxPWDBAll = PWDB_ALL;
		pstats->RecvSignalPower = rx_pwr_all;

		//get Signal Quality(EVM)
		if (bpacket_match_bssid){
			u8 sq;
			if (pstats->RxPWDBAll > 40)
				sq = 100;
			else{
				sq = cck_buf->sq_rpt;
				if (sq > 64)
					sq = 0;
				else if(sq < 20)
					sq = 100;
				else
					sq = ((64-sq)*100)/44;
			}
			pstats->SignalQuality = precord_stats->SignalQuality = sq;
			pstats->RxMIMOSignalQuality[0] = precord_stats->RxMIMOSignalQuality[0] = sq;
			pstats->RxMIMOSignalQuality[1] = precord_stats->RxMIMOSignalQuality[1] = -1;
		}
	}
	else{
		priv->brfpath_rxenable[0] = priv->brfpath_rxenable[1] = true;

		for (i=RF90_PATH_A; i<RF90_PATH_MAX; i++){
			if (priv->brfpath_rxenable[i])
				rf_rx_num ++;
			//else
			//	break;

			rx_pwr[i] = ((pDrvInfo->gain_trsw[i]&0x3f)*2) - 110;
			rssi = rtl819x_query_rxpwrpercentage(rx_pwr[i]);
			total_rssi += rssi;

			priv->stats.rxSNRdB[i] = (long)(pDrvInfo->rxsnr[i]/2);

			if (bpacket_match_bssid){
				pstats->RxMIMOSignalStrength[i] = (u8)rssi;
				precord_stats->RxMIMOSignalStrength [i] = (u8)rssi;
			}
		}

		rx_pwr_all = ((pDrvInfo->pwdb_all >> 1) & 0x7f) - 0x106;
		PWDB_ALL = rtl819x_query_rxpwrpercentage(rx_pwr_all);

		pstats->RxPWDBAll = precord_stats->RxPWDBAll = PWDB_ALL;
		pstats->RxPower = precord_stats->RxPower = rx_pwr_all;
		pstats->RecvSignalPower = precord_stats->RecvSignalPower = rx_pwr_all;

		//EVM of HT rate
		if (pdesc->RxHT && pdesc->RxMCS >= DESC92S_RATEMCS8 && pdesc->RxMCS <= DESC92S_RATEMCS15)
			max_spatial_stream = 2;
		else
			max_spatial_stream = 1;

		for(i=0; i<max_spatial_stream; i++){
			EVM = rtl819x_evm_dbtopercentage(pDrvInfo->rxevm[i]);

			if (bpacket_match_bssid)
			{
				if (i==0)
					pstats->SignalQuality =
					precord_stats->SignalQuality = (u8)(EVM&0xff);
				pstats->RxMIMOSignalQuality[i] =
				precord_stats->RxMIMOSignalQuality[i] = (u8)(EVM&0xff);
			}
		}
#if 0
		if (pdesc->BW)
			priv->stats.received_bwtype[1+pDrvInfo->rxsc]++;
		else
			priv->stats.received_bwtype[0]++;
#endif
	}

	if (is_cck_rate)
		pstats->SignalStrength =
		precord_stats->SignalStrength = (u8)(rtl819x_signal_scale_mapping(priv,PWDB_ALL));
	else
		if (rf_rx_num != 0)
			pstats->SignalStrength =
			precord_stats->SignalStrength = (u8)(rtl819x_signal_scale_mapping(priv,total_rssi/=rf_rx_num));

}
#endif
#else //RTL8192SE
#define		rx_hal_is_cck_rate(_pdrvinfo)\
			(_pdrvinfo->RxRate == DESC90_RATE1M ||\
			_pdrvinfo->RxRate == DESC90_RATE2M ||\
			_pdrvinfo->RxRate == DESC90_RATE5_5M ||\
			_pdrvinfo->RxRate == DESC90_RATE11M) &&\
			!_pdrvinfo->RxHT
void rtl8192_query_rxphystatus(
	struct r8192_priv * priv,
	struct rtllib_rx_stats * pstats,
	prx_desc  pdesc,
	prx_fwinfo   pdrvinfo,
	struct rtllib_rx_stats * precord_stats,
	bool bpacket_match_bssid,
	bool bpacket_toself,
	bool bPacketBeacon,
	bool bToSelfBA
	)
{
	//PRT_RFD_STATUS		pRtRfdStatus = &(pRfd->Status);
	phy_sts_ofdm_819xpci_t* pofdm_buf;
	phy_sts_cck_819xpci_t	*	pcck_buf;
	phy_ofdm_rx_status_rxsc_sgien_exintfflag* prxsc;
	u8				*prxpkt;
	u8				i,max_spatial_stream, tmp_rxsnr, tmp_rxevm, rxsc_sgien_exflg;
	char				rx_pwr[4], rx_pwr_all=0;
	//long				rx_avg_pwr = 0;
	char				rx_snrX, rx_evmX;
	u8				evm, pwdb_all;
	u32			RSSI, total_rssi=0;//, total_evm=0;
//	long				signal_strength_index = 0;
	u8				is_cck_rate=0;
	u8				rf_rx_num = 0;

	// 2007/07/04 MH For OFDM RSSI. For high power or not. */
	static	u8		check_reg824 = 0;
	static	u32		reg824_bit9 = 0;

	priv->stats.numqry_phystatus++;


	is_cck_rate = rx_hal_is_cck_rate(pdrvinfo);
	// Record it for next packet processing
	memset(precord_stats, 0, sizeof(struct rtllib_rx_stats));
	pstats->bPacketMatchBSSID = precord_stats->bPacketMatchBSSID = bpacket_match_bssid;
	pstats->bPacketToSelf = precord_stats->bPacketToSelf = bpacket_toself;
	pstats->bIsCCK = precord_stats->bIsCCK = is_cck_rate;//RX_HAL_IS_CCK_RATE(pDrvInfo);
	pstats->bPacketBeacon = precord_stats->bPacketBeacon = bPacketBeacon;
	pstats->bToSelfBA = precord_stats->bToSelfBA = bToSelfBA;
	//2007.08.30 requested by SD3 Jerry */
	if(check_reg824 == 0)
	{
		reg824_bit9 = rtl8192_QueryBBReg(priv->rtllib->dev, rFPGA0_XA_HSSIParameter2, 0x200);
		check_reg824 = 1;
	}


	prxpkt = (u8*)pdrvinfo;

	// Move pointer to the 16th bytes. Phy status start address. */
	prxpkt += sizeof(rx_fwinfo);

	// Initial the cck and ofdm buffer pointer */
	pcck_buf = (phy_sts_cck_819xpci_t *)prxpkt;
	pofdm_buf = (phy_sts_ofdm_819xpci_t *)prxpkt;

	pstats->RxMIMOSignalQuality[0] = -1;
	pstats->RxMIMOSignalQuality[1] = -1;
	precord_stats->RxMIMOSignalQuality[0] = -1;
	precord_stats->RxMIMOSignalQuality[1] = -1;

	if(is_cck_rate)
	{
		//
		// (1)Hardware does not provide RSSI for CCK
		//

		//
		// (2)PWDB, Average PWDB cacluated by hardware (for rate adaptive)
		//
		u8 report;//, cck_agc_rpt;
#ifdef RTL8190P
		u8 tmp_pwdb;
		char cck_adc_pwdb[4];
#endif
		priv->stats.numqry_phystatusCCK++;

#ifdef RTL8190P	//Only 90P 2T4R need to check
		if(priv->rf_type == RF_2T4R && DM_RxPathSelTable.Enable && bpacket_match_bssid)
		{
			for(i=RF90_PATH_A; i<RF90_PATH_MAX; i++)
			{
				tmp_pwdb = pcck_buf->adc_pwdb_X[i];
				cck_adc_pwdb[i] = (char)tmp_pwdb;
				cck_adc_pwdb[i] /= 2;
				pstats->cck_adc_pwdb[i] = precord_stats->cck_adc_pwdb[i] = cck_adc_pwdb[i];
				//DbgPrint("RF-%d tmp_pwdb = 0x%x, cck_adc_pwdb = %d", i, tmp_pwdb, cck_adc_pwdb[i]);
			}
		}
#endif

		if(!reg824_bit9)
		{
			report = pcck_buf->cck_agc_rpt & 0xc0;
			report = report>>6;
			switch(report)
			{
				//Fixed by Jacken from Bryant 2008-03-20
				//Original value is -38 , -26 , -14 , -2
				//Fixed value is -35 , -23 , -11 , 6
				case 0x3:
					rx_pwr_all = -35 - (pcck_buf->cck_agc_rpt & 0x3e);
					break;
				case 0x2:
					rx_pwr_all = -23 - (pcck_buf->cck_agc_rpt & 0x3e);
					break;
				case 0x1:
					rx_pwr_all = -11 - (pcck_buf->cck_agc_rpt & 0x3e);
					break;
				case 0x0:
					rx_pwr_all = 8 - (pcck_buf->cck_agc_rpt & 0x3e);
					break;
			}
		}
		else
		{
			report = pcck_buf->cck_agc_rpt & 0x60;
			report = report>>5;
			switch(report)
			{
				case 0x3:
					rx_pwr_all = -35 - ((pcck_buf->cck_agc_rpt & 0x1f)<<1) ;
					break;
				case 0x2:
					rx_pwr_all = -23 - ((pcck_buf->cck_agc_rpt & 0x1f)<<1);
					break;
				case 0x1:
					rx_pwr_all = -11 - ((pcck_buf->cck_agc_rpt & 0x1f)<<1) ;
					break;
				case 0x0:
					rx_pwr_all = -8 - ((pcck_buf->cck_agc_rpt & 0x1f)<<1) ;
					break;
			}
		}

		pwdb_all = rtl819x_query_rxpwrpercentage(rx_pwr_all);
		pstats->RxPWDBAll = precord_stats->RxPWDBAll = pwdb_all;
		pstats->RecvSignalPower = rx_pwr_all;

		//
		// (3) Get Signal Quality (EVM)
		//
		if(bpacket_match_bssid)
		{
			u8	sq;

			if(pstats->RxPWDBAll > 40)
			{
				sq = 100;
			}else
			{
				sq = pcck_buf->sq_rpt;

				if(pcck_buf->sq_rpt > 64)
					sq = 0;
				else if (pcck_buf->sq_rpt < 20)
					sq = 100;
				else
					sq = ((64-sq) * 100) / 44;
			}
			pstats->SignalQuality = precord_stats->SignalQuality = sq;
			pstats->RxMIMOSignalQuality[0] = precord_stats->RxMIMOSignalQuality[0] = sq;
			pstats->RxMIMOSignalQuality[1] = precord_stats->RxMIMOSignalQuality[1] = -1;
		}
	}
	else
	{
		priv->stats.numqry_phystatusHT++;
		//
		// (1)Get RSSI for HT rate
		//
		for(i=RF90_PATH_A; i<RF90_PATH_MAX; i++)
		{
			// 2008/01/30 MH we will judge RF RX path now.
			if (priv->brfpath_rxenable[i])
				rf_rx_num++;
			//else
				//continue;

			//Fixed by Jacken from Bryant 2008-03-20
			//Original value is 106
#ifdef RTL8190P	   //Modify by Jacken 2008/03/31
			rx_pwr[i] = ((pofdm_buf->trsw_gain_X[i]&0x3F)*2) - 106;
#else
			rx_pwr[i] = ((pofdm_buf->trsw_gain_X[i]&0x3F)*2) - 110;
#endif

			//Get Rx snr value in DB
			tmp_rxsnr = pofdm_buf->rxsnr_X[i];
			rx_snrX = (char)(tmp_rxsnr);
			rx_snrX /= 2;
			priv->stats.rxSNRdB[i] = (long)rx_snrX;

			// Translate DBM to percentage. */
			RSSI = rtl819x_query_rxpwrpercentage(rx_pwr[i]);
			if (priv->brfpath_rxenable[i])
				total_rssi += RSSI;

			// Record Signal Strength for next packet */
			if(bpacket_match_bssid)
			{
				pstats->RxMIMOSignalStrength[i] =(u8) RSSI;
				precord_stats->RxMIMOSignalStrength[i] =(u8) RSSI;
			}
		}


		//
		// (2)PWDB, Average PWDB cacluated by hardware (for rate adaptive)
		//
		//Fixed by Jacken from Bryant 2008-03-20
		//Original value is 106
		rx_pwr_all = (((pofdm_buf->pwdb_all ) >> 1 )& 0x7f) -106;
		pwdb_all = rtl819x_query_rxpwrpercentage(rx_pwr_all);

		pstats->RxPWDBAll = precord_stats->RxPWDBAll = pwdb_all;
		pstats->RxPower = precord_stats->RxPower =	rx_pwr_all;
		pstats->RecvSignalPower = rx_pwr_all;
		//
		// (3)EVM of HT rate
		//
		if(pdrvinfo->RxHT && pdrvinfo->RxRate>=DESC90_RATEMCS8 &&
			pdrvinfo->RxRate<=DESC90_RATEMCS15)
			max_spatial_stream = 2; //both spatial stream make sense
		else
			max_spatial_stream = 1; //only spatial stream 1 makes sense

		for(i=0; i<max_spatial_stream; i++)
		{
			tmp_rxevm = pofdm_buf->rxevm_X[i];
			rx_evmX = (char)(tmp_rxevm);

			// Do not use shift operation like "rx_evmX >>= 1" because the compilor of free build environment
			// fill most significant bit to "zero" when doing shifting operation which may change a negative
			// value to positive one, then the dbm value (which is supposed to be negative)  is not correct anymore.
			rx_evmX /= 2;	//dbm

			evm = rtl819x_evm_dbtopercentage(rx_evmX);
#if 0
			EVM = SignalScaleMapping(EVM);//make it good looking, from 0~100
#endif
			if(bpacket_match_bssid)
			{
				if(i==0) // Fill value in RFD, Get the first spatial stream only
					pstats->SignalQuality = precord_stats->SignalQuality = (u8)(evm & 0xff);
				pstats->RxMIMOSignalQuality[i] = precord_stats->RxMIMOSignalQuality[i] = (u8)(evm & 0xff);
			}
		}


		// record rx statistics for debug */
		rxsc_sgien_exflg = pofdm_buf->rxsc_sgien_exflg;
		prxsc = (phy_ofdm_rx_status_rxsc_sgien_exintfflag *)&rxsc_sgien_exflg;
		if(pdrvinfo->BW)	//40M channel
			priv->stats.received_bwtype[1+prxsc->rxsc]++;
		else				//20M channel
			priv->stats.received_bwtype[0]++;
	}

	//UI BSS List signal strength(in percentage), make it good looking, from 0~100.
	//It is assigned to the BSS List in GetValueFromBeaconOrProbeRsp().
	if(is_cck_rate)
	{
		pstats->SignalStrength = precord_stats->SignalStrength = (u8)(rtl819x_signal_scale_mapping(priv,(long)pwdb_all));//PWDB_ALL;

	}
	else
	{
		//pRfd->Status.SignalStrength = pRecordRfd->Status.SignalStrength = (u8)(SignalScaleMapping(total_rssi/=RF90_PATH_MAX));//(u8)(total_rssi/=RF90_PATH_MAX);
		// We can judge RX path number now.
		if (rf_rx_num != 0)
			pstats->SignalStrength = precord_stats->SignalStrength = (u8)(rtl819x_signal_scale_mapping(priv,(long)(total_rssi/=rf_rx_num)));
	}
}	// QueryRxPhyStatus8190Pci */

#endif //92SE

void
rtl8192_record_rxdesc_forlateruse(
	struct rtllib_rx_stats * psrc_stats,
	struct rtllib_rx_stats * ptarget_stats
)
{
	ptarget_stats->bIsAMPDU = psrc_stats->bIsAMPDU;
	ptarget_stats->bFirstMPDU = psrc_stats->bFirstMPDU;
	//ptarget_stats->Seq_Num = psrc_stats->Seq_Num;
}


void TranslateRxSignalStuff819xpci(struct net_device *dev,
        struct sk_buff *skb,
        struct rtllib_rx_stats * pstats,
        prx_desc pdesc,
        prx_fwinfo pdrvinfo)
{
    // TODO: We must only check packet for current MAC address. Not finish
    struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);
    bool bpacket_match_bssid, bpacket_toself;
    bool bPacketBeacon=false;
    struct rtllib_hdr_3addr *hdr;
#ifdef _RTL8192_EXT_PATCH_
    struct rtllib_network* pnet=NULL;
#else
    bool bToSelfBA=false;
    static struct rtllib_rx_stats  previous_stats;
#endif
    u16 fc,type;

    // Get Signal Quality for only RX data queue (but not command queue)

    u8* tmp_buf;
    u8	*praddr;
#ifdef _RTL8192_EXT_PATCH_
    u8	*psaddr;
#endif

    // Get MAC frame start address. */
    tmp_buf = skb->data + pstats->RxDrvInfoSize + pstats->RxBufShift;

    hdr = (struct rtllib_hdr_3addr *)tmp_buf;
    fc = le16_to_cpu(hdr->frame_ctl);
    type = WLAN_FC_GET_TYPE(fc);
    praddr = hdr->addr1;

#ifdef _RTL8192_EXT_PATCH_
    psaddr = hdr->addr2;
    if((priv->rtllib->iw_mode == IW_MODE_MESH) && (priv->mshobj->ext_patch_get_mpinfo))
		pnet = priv->mshobj->ext_patch_get_mpinfo(dev,psaddr);
#endif
    // Check if the received packet is acceptabe. */
//YJ,modified for MESH,090824
#ifdef _RTL8192_EXT_PATCH_
    bpacket_match_bssid = ((RTLLIB_FTYPE_CTL != type) && (!pstats->bHwError) && (!pstats->bCRC)&& (!pstats->bICV));
    //printk(""MAC_FMT": pnet=%p\n", MAC_ARG(psaddr), pnet);
    if(pnet){
        bpacket_match_bssid = bpacket_match_bssid;
        //printk("Mesh packet: bpacket_match_bssid=%d \n", bpacket_match_bssid);
    }
    else{
        bpacket_match_bssid = bpacket_match_bssid &&
            (!compare_ether_addr(priv->rtllib->current_network.bssid,
				 (fc & RTLLIB_FCTL_TODS)? hdr->addr1 :
				 (fc & RTLLIB_FCTL_FROMDS )? hdr->addr2 : hdr->addr3));
        //printk("Not mesh packet: bpacket_match_bssid=%d\n", bpacket_match_bssid);
    }
#else
    bpacket_match_bssid = ((RTLLIB_FTYPE_CTL != type) &&
            (!compare_ether_addr(priv->rtllib->current_network.bssid,
		       (fc & RTLLIB_FCTL_TODS)? hdr->addr1 :
		       (fc & RTLLIB_FCTL_FROMDS )? hdr->addr2 : hdr->addr3))
            && (!pstats->bHwError) && (!pstats->bCRC)&& (!pstats->bICV));
#endif
    bpacket_toself =  bpacket_match_bssid & (!compare_ether_addr(praddr, priv->rtllib->dev->dev_addr));
    if(WLAN_FC_GET_FRAMETYPE(fc)== RTLLIB_STYPE_BEACON)
    {
        bPacketBeacon = true;
        //DbgPrint("Beacon 2, MatchBSSID = %d, ToSelf = %d \n", bPacketMatchBSSID, bPacketToSelf);
    }
#ifndef _RTL8192_EXT_PATCH_
    if(WLAN_FC_GET_FRAMETYPE(fc) == RTLLIB_STYPE_BLOCKACK)
    {
        if ((!compare_ether_addr(praddr,dev->dev_addr)))
            bToSelfBA = true;
        //DbgPrint("BlockAck, MatchBSSID = %d, ToSelf = %d \n", bPacketMatchBSSID, bPacketToSelf);
    }

#endif
    if(bpacket_match_bssid)
    {
        priv->stats.numpacket_matchbssid++;
    }
    if(bpacket_toself){
        priv->stats.numpacket_toself++;
    }
    //
    // Process PHY information for previous packet (RSSI/PWDB/EVM)
    //
    // Because phy information is contained in the last packet of AMPDU only, so driver
    // should process phy information of previous packet
#ifdef _RTL8192_EXT_PATCH_
    rtl8192_query_rxphystatus(priv, pstats, pdesc, pdrvinfo, bpacket_match_bssid,
            bpacket_toself ,bPacketBeacon);
    rtl8192_process_phyinfo(priv, tmp_buf,pstats,pnet);

    //rtl8192_record_rxdesc_forlateruse(pstats, &previous_stats);
#else
    rtl8192_process_phyinfo(priv, tmp_buf,&previous_stats, pstats);
    rtl8192_query_rxphystatus(priv, pstats, pdesc, pdrvinfo, &previous_stats, bpacket_match_bssid,
            bpacket_toself ,bPacketBeacon, bToSelfBA);
    rtl8192_record_rxdesc_forlateruse(pstats, &previous_stats);
#endif

}


//#endif


//
//Function:	UpdateReceivedRateHistogramStatistics
//Overview:	Recored down the received data rate
//
//Input:
//	PADAPTER	dev
//      PRT_RFD		pRfd,
//
//Output:
//      PRT_TCB		dev
//				(dev->RxStats.ReceivedRateHistogram[] is updated)
//Return:
//		None
//
void UpdateReceivedRateHistogramStatistics8190(
	struct net_device *dev,
	struct rtllib_rx_stats* pstats
	)
{
	struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);
	u32 rcvType=1;   //0: Total, 1:OK, 2:CRC, 3:ICV
	u32 rateIndex;
	u32 preamble_guardinterval;  //1: short preamble/GI, 0: long preamble/GI

	// 2007/03/09 MH We will not update rate of packet from rx cmd queue. */
	#if 0
	if (pRfd->queue_id == CMPK_RX_QUEUE_ID)
		return;
	#endif
	if(pstats->bCRC)
		rcvType = 2;
	else if(pstats->bICV)
		rcvType = 3;

	if(pstats->bShortPreamble)
		preamble_guardinterval = 1;// short
	else
		preamble_guardinterval = 0;// long

	switch(pstats->rate)
	{
		//
		// CCK rate
		//
		case MGN_1M:    rateIndex = 0;  break;
		case MGN_2M:    rateIndex = 1;  break;
		case MGN_5_5M:  rateIndex = 2;  break;
		case MGN_11M:   rateIndex = 3;  break;
		//
		// Legacy OFDM rate
		//
		case MGN_6M:    rateIndex = 4;  break;
		case MGN_9M:    rateIndex = 5;  break;
		case MGN_12M:   rateIndex = 6;  break;
		case MGN_18M:   rateIndex = 7;  break;
		case MGN_24M:   rateIndex = 8;  break;
		case MGN_36M:   rateIndex = 9;  break;
		case MGN_48M:   rateIndex = 10; break;
		case MGN_54M:   rateIndex = 11; break;
		//
		// 11n High throughput rate
		//
		case MGN_MCS0:  rateIndex = 12; break;
		case MGN_MCS1:  rateIndex = 13; break;
		case MGN_MCS2:  rateIndex = 14; break;
		case MGN_MCS3:  rateIndex = 15; break;
		case MGN_MCS4:  rateIndex = 16; break;
		case MGN_MCS5:  rateIndex = 17; break;
		case MGN_MCS6:  rateIndex = 18; break;
		case MGN_MCS7:  rateIndex = 19; break;
		case MGN_MCS8:  rateIndex = 20; break;
		case MGN_MCS9:  rateIndex = 21; break;
		case MGN_MCS10: rateIndex = 22; break;
		case MGN_MCS11: rateIndex = 23; break;
		case MGN_MCS12: rateIndex = 24; break;
		case MGN_MCS13: rateIndex = 25; break;
		case MGN_MCS14: rateIndex = 26; break;
		case MGN_MCS15: rateIndex = 27; break;
		default:        rateIndex = 28; break;
	}
	priv->stats.received_preamble_GI[preamble_guardinterval][rateIndex]++;
	priv->stats.received_rate_histogram[0][rateIndex]++; //total
	priv->stats.received_rate_histogram[rcvType][rateIndex]++;
}

void rtl8192_rx(struct net_device *dev)
{
	struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);
	struct rtllib_hdr_1addr *rtllib_hdr = NULL;
	bool unicast_packet = false;
	bool bLedBlinking=true;
	u16 fc=0, type=0;
	u32 skb_len = 0;
	struct rtllib_rx_stats stats = {
		.signal = 0,
		.noise = -98,
		.rate = 0,
		.freq = RTLLIB_24GHZ_BAND,
	};
	unsigned int count = priv->rxringcount;

	stats.nic_type = NIC_8192E;

	while (count--) {
		rx_desc *pdesc = &priv->rx_ring[priv->rx_idx];//rx descriptor
		struct sk_buff *skb = priv->rx_buf[priv->rx_idx];//rx pkt

		if (pdesc->OWN){
			// wait data to be filled by hardware */
			return;
		} else {

			//RT_DEBUG_DATA(COMP_ERR, pdesc, sizeof(*pdesc));
			struct sk_buff *new_skb = NULL;
			if (!priv->ops->rx_query_status_descriptor(dev, &stats, pdesc, skb))
				goto done;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
			pci_dma_sync_single_for_cpu(priv->pdev,
						*((dma_addr_t *)skb->cb),
						priv->rxbuffersize,
						PCI_DMA_FROMDEVICE);
#else
			pci_unmap_single(priv->pdev,
					*((dma_addr_t *)skb->cb),
					priv->rxbuffersize,
					PCI_DMA_FROMDEVICE);
#endif

			skb_put(skb, pdesc->Length);
			skb_reserve(skb, stats.RxDrvInfoSize + stats.RxBufShift);
			skb_trim(skb, skb->len - 4/*sCrcLng*/);
			rtllib_hdr = (struct rtllib_hdr_1addr *)skb->data;
			if(is_broadcast_ether_addr(rtllib_hdr->addr1)) {
				//TODO
			}else if(is_multicast_ether_addr(rtllib_hdr->addr1)){
				//TODO
			}else {
				/* unicast packet */
				unicast_packet = true;
			}
			fc = le16_to_cpu(rtllib_hdr->frame_ctl);
			type = WLAN_FC_GET_TYPE(fc);
			if(type == RTLLIB_FTYPE_MGMT)
			{
				bLedBlinking = false;
			}
			if(bLedBlinking)
				if(priv->rtllib->LedControlHandler)
				priv->rtllib->LedControlHandler(dev, LED_CTL_RX);
			skb_len = skb->len;
			if(!rtllib_rx(priv->rtllib, skb, &stats)){
				dev_kfree_skb_any(skb);
			} else {
				priv->stats.rxok++;
				if(unicast_packet) {
					priv->stats.rxbytesunicast += skb_len;
				}
			}
#if 1
			new_skb = dev_alloc_skb(priv->rxbuffersize);
			if (unlikely(!new_skb))
			{
				printk("==========>can't alloc skb for rx\n");
				goto done;
			}
			skb=new_skb;
                        skb->dev = dev;
#endif
			priv->rx_buf[priv->rx_idx] = skb;
			*((dma_addr_t *) skb->cb) = pci_map_single(priv->pdev, skb_tail_pointer(skb), priv->rxbuffersize, PCI_DMA_FROMDEVICE);
			//                *((dma_addr_t *) skb->cb) = pci_map_single(priv->pdev, skb_tail_pointer(skb), priv->rxbuffersize, PCI_DMA_FROMDEVICE);

		}
done:
		pdesc->BufferAddress = cpu_to_le32(*((dma_addr_t *)skb->cb));
		pdesc->OWN = 1;
		pdesc->Length = priv->rxbuffersize;
		if (priv->rx_idx == priv->rxringcount-1)
			pdesc->EOR = 1;
		priv->rx_idx = (priv->rx_idx + 1) % priv->rxringcount;
	}

}



void rtl8192_tx_resume(struct net_device *dev)
{
	struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);
	struct rtllib_device *ieee = priv->rtllib;
	struct sk_buff *skb;
	int queue_index;

	for(queue_index = BK_QUEUE; queue_index < TXCMD_QUEUE;queue_index++) {
		//printk("===>queue:%d, %d, %d\n", queue_index, skb_queue_empty(&ieee->skb_aggQ[queue_index]),  priv->rtllib->check_nic_enough_desc(dev,queue_index));
		while((!skb_queue_empty(&ieee->skb_waitQ[queue_index]))&&
				(priv->rtllib->check_nic_enough_desc(dev,queue_index) > 0)) {
			// 1. dequeue the packet from the wait queue */
			skb = skb_dequeue(&ieee->skb_waitQ[queue_index]);
			// 2. tx the packet directly */
			ieee->softmac_data_hard_start_xmit(skb,dev,0/* rate useless now*/);
			#if 0
			if(queue_index!=MGNT_QUEUE) {
				ieee->stats.tx_packets++;
				ieee->stats.tx_bytes += skb->len;
			}
			#endif
		}
#ifdef ENABLE_AMSDU
		while((skb_queue_len(&priv->rtllib->skb_aggQ[queue_index]) > 0)&&\
				(!(priv->rtllib->queue_stop)) && \
				(priv->rtllib->check_nic_enough_desc(dev,queue_index) > 0)){

			struct sk_buff_head pSendList;
			u8 dst[ETH_ALEN];
			cb_desc *tcb_desc = NULL;
			int qos_actived = priv->rtllib->current_network.qos_data.active;//YJ,add,081103
			struct sta_info *psta = NULL;
			u8 bIsSptAmsdu = false;

#ifdef WIFI_TEST // kenny: queue select
			if (queue_index <= VO_QUEUE)
				queue_index = wmm_queue_select(priv, queue_index, priv->rtllib->skb_aggQ);
#endif
			//printk("In %s: AggrQ len %d QIndex %d\n",__FUNCTION__, skb_queue_len(&priv->rtllib->skb_aggQ[queue_index]), queue_index);
			//YJ,add,090409
			priv->rtllib->amsdu_in_process = true;

			skb = skb_dequeue(&(priv->rtllib->skb_aggQ[queue_index]));
			if(skb == NULL)
			{
				printk("In %s:Skb is NULL\n",__FUNCTION__);
				return;
			}
			//printk("!!!!!dequeue skb len %d &&&skb=%p\n",skb->len,skb);
			tcb_desc = (pcb_desc)(skb->cb + MAX_DEV_ADDR_SIZE);
			if(tcb_desc->bFromAggrQ)
			{
#ifdef HAVE_NET_DEVICE_OPS
				if(dev->netdev_ops->ndo_start_xmit)
					dev->netdev_ops->ndo_start_xmit(skb, dev);
#else
				dev->hard_start_xmit(skb, dev);
#endif
				return;
			}

			memcpy(dst, skb->data, ETH_ALEN);
			if(priv->rtllib->iw_mode == IW_MODE_ADHOC)
			{
				psta = GetStaInfo(priv->rtllib, dst);
				if(psta) {
					if(psta->htinfo.bEnableHT)
						bIsSptAmsdu = true;
				}
			}
			else if(priv->rtllib->iw_mode == IW_MODE_INFRA)
				bIsSptAmsdu = true;
			else
				bIsSptAmsdu = true;
			bIsSptAmsdu = true;

			bIsSptAmsdu = bIsSptAmsdu && priv->rtllib->pHTInfo->bCurrent_AMSDU_Support && qos_actived;

			//printk("@@@qos_actived=%d dst:"MAC_FMT" AMSDU_spt=%d\n", qos_actived, MAC_ARG(dst), priv->rtllib->pHTInfo->bCurrent_AMSDU_Support);
			if(qos_actived &&       !is_broadcast_ether_addr(dst) &&
					!is_multicast_ether_addr(dst) &&
					bIsSptAmsdu)
			{
				skb_queue_head_init(&pSendList);
				if(AMSDU_GetAggregatibleList(priv->rtllib, skb, &pSendList,queue_index))
				{
					struct sk_buff * pAggrSkb = AMSDU_Aggregation(priv->rtllib, &pSendList);
					if(NULL != pAggrSkb)
#ifdef HAVE_NET_DEVICE_OPS
						if(dev->netdev_ops->ndo_start_xmit)
							dev->netdev_ops->ndo_start_xmit(pAggrSkb, dev);
#else
						dev->hard_start_xmit(pAggrSkb, dev);
#endif
				}
			}
			else
			{
				memset(skb->cb,0,sizeof(skb->cb));
				tcb_desc = (cb_desc *)(skb->cb + MAX_DEV_ADDR_SIZE);
				tcb_desc->bFromAggrQ = true;
#ifdef HAVE_NET_DEVICE_OPS
				if(dev->netdev_ops->ndo_start_xmit)
					dev->netdev_ops->ndo_start_xmit(skb, dev);
#else
				dev->hard_start_xmit(skb, dev);
#endif
			}
		}
#endif
#ifdef _RTL8192_EXT_PATCH_
		// MP <--> MP AMSDU
		while((!skb_queue_empty(&priv->rtllib->skb_meshaggQ[queue_index]) )&&\
			(priv->rtllib->check_nic_enough_desc(dev,queue_index)> 0))
		{
			struct sk_buff_head pSendList;
			u8 dst[ETH_ALEN];
			cb_desc *tcb_desc = NULL;
			u8 IsHTEnable = false;
			int qos_actived = 1; //FIX ME
			//printk("In %s: meshAggrQ len %d QIndex %d\n",__FUNCTION__, skb_queue_len(&priv->rtllib->skb_meshaggQ[queue_index]), queue_index);
			priv->rtllib->mesh_amsdu_in_process = true;
			skb = skb_dequeue(&(priv->rtllib->skb_meshaggQ[queue_index]));
			if(skb == NULL)
			{
				priv->rtllib->mesh_amsdu_in_process = false;
				printk("In %s:Skb is NULL\n",__FUNCTION__);
				return;
			}
			tcb_desc = (pcb_desc)(skb->cb + MAX_DEV_ADDR_SIZE);
			if(tcb_desc->bFromAggrQ)
			{
				rtllib_mesh_xmit(skb, dev);
				continue;
			}
			memcpy(dst, skb->data, ETH_ALEN);

#if 0
			ppeerMP_htinfo phtinfo = NULL;
			bool is_mesh = false;
			if(priv->mshobj->ext_patch_rtllib_is_mesh)
				is_mesh = priv->mshobj->ext_patch_rtllib_is_mesh(priv->rtllib,dst);
			if(is_mesh){
				if(priv->rtllib->ext_patch_rtllib_get_peermp_htinfo)
				{
					phtinfo = priv->rtllib->ext_patch_rtllib_get_peermp_htinfo(ieee,dst);
					if(phtinfo == NULL)
					{
						RT_TRACE(COMP_ERR,"%s(): No htinfo\n",__FUNCTION__);
					}
					else
					{
						if(phtinfo->bEnableHT)
							IsHTEnable = true;
					}
				}
			}
			else //not mesh entry
			{
				printk("===>%s():tx AMSDU data has not entry,dst: "MAC_FMT"\n",__FUNCTION__,MAC_ARG(dst));
				IsHTEnable = true;
			}
#else
			IsHTEnable = true;
#endif
			IsHTEnable = (IsHTEnable && ieee->pHTInfo->bCurrent_Mesh_AMSDU_Support && qos_actived);
			if( !is_broadcast_ether_addr(dst) &&
				!is_multicast_ether_addr(dst) &&
				IsHTEnable)
			{
				//printk("%s: start amsdu\n", __FUNCTION__);
				skb_queue_head_init(&pSendList);
				if(msh_AMSDU_GetAggregatibleList(priv->rtllib, skb, &pSendList,queue_index))
				{
					struct sk_buff * pAggrSkb = msh_AMSDU_Aggregation(priv->rtllib, &pSendList);
					if(NULL != pAggrSkb)
						rtllib_mesh_xmit(pAggrSkb, dev);
				}else{
					priv->rtllib->mesh_amsdu_in_process = false;
					return;
				}
			}
			else
			{
				memset(skb->cb,0,sizeof(skb->cb));
				tcb_desc = (cb_desc *)(skb->cb + MAX_DEV_ADDR_SIZE);
				tcb_desc->bFromAggrQ = true;
				rtllib_mesh_xmit(skb, dev);
			}
		}
#endif
	}
}

void rtl8192_irq_tx_tasklet(struct r8192_priv *priv)
{
       rtl8192_tx_resume(priv->rtllib->dev);
}

void rtl8192_irq_rx_tasklet(struct r8192_priv *priv)
{
       rtl8192_rx(priv->rtllib->dev);
	// unmask RDU */
#ifndef RTL8192CE
       write_nic_dword(priv->rtllib->dev, INTA_MASK,read_nic_dword(priv->rtllib->dev, INTA_MASK) | IMR_RDU);
#endif
}

/****************************************************************************
 ---------------------------- NIC START/CLOSE STUFF---------------------------
*****************************************************************************/
// detach all the work and timer structure declared or inititialized
// in r8192_init function.
//
void rtl8192_cancel_deferred_work(struct r8192_priv* priv)
{
	// call cancel_work_sync instead of cancel_delayed_work if and only if Linux_version_code
        //  is  or is newer than 2.6.20 and work structure is defined to be struct work_struct.
        //  Otherwise call cancel_delayed_work is enough.
        //  FIXME (2.6.20 shoud 2.6.22, work_struct shoud not cancel)
        //
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
	cancel_delayed_work(&priv->watch_dog_wq);
	cancel_delayed_work(&priv->update_beacon_wq);
#ifndef RTL8190P
//	cancel_delayed_work(&priv->rtllib->hw_wakeup_wq);
	cancel_delayed_work(&priv->rtllib->hw_sleep_wq);
	//cancel_delayed_work(&priv->gpio_change_rf_wq);//lzm 090323
#endif
#if defined RTL8192SE
	cancel_delayed_work(&priv->rtllib->update_assoc_sta_info_wq);
	cancel_delayed_work(&priv->rtllib->check_tsf_wq);
#endif
#endif

#if LINUX_VERSION_CODE >=KERNEL_VERSION(2,6,22)
	cancel_work_sync(&priv->reset_wq);
	cancel_work_sync(&priv->qos_activate);
#elif ((LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)))
	cancel_delayed_work(&priv->reset_wq);
	cancel_delayed_work(&priv->qos_activate);
#if defined RTL8192SE
	cancel_delayed_work(&priv->rtllib->update_assoc_sta_info_wq);
	cancel_delayed_work(&priv->rtllib->check_tsf_wq);
#endif
#endif

}

#ifdef CONFIG_CRDA
bool rtl8192_register_wiphy_dev(struct net_device *dev)
{
	struct r8192_priv *priv = rtllib_priv(dev);
	const struct rtllib_geo *geo = &priv->rtllib->geo;
	struct wireless_dev *wdev = &priv->rtllib->wdev;

	memcpy(wdev->wiphy->perm_addr, dev->dev_addr, ETH_ALEN);
	/* fill-out priv->ieee->bg_band */
	if (geo->bg_channels) {
		u8 i;
		struct ieee80211_supported_band *bg_band = &priv->rtllib->bg_band;

		bg_band->band = IEEE80211_BAND_2GHZ;
		bg_band->n_channels = geo->bg_channels;
		bg_band->channels =
			kzalloc(geo->bg_channels *
					sizeof(struct ieee80211_channel), GFP_KERNEL);
#if 1
		/* translate geo->bg to bg_band.channels */
		for (i = 0; i < geo->bg_channels; i++) {
			bg_band->channels[i].band = IEEE80211_BAND_2GHZ;
			bg_band->channels[i].center_freq = geo->bg[i].freq;
			bg_band->channels[i].hw_value = geo->bg[i].channel;
			bg_band->channels[i].max_power = geo->bg[i].max_power;
			if (geo->bg[i].flags & LIBIPW_CH_PASSIVE_ONLY)
				bg_band->channels[i].flags |=
					IEEE80211_CHAN_PASSIVE_SCAN;
			if (geo->bg[i].flags & LIBIPW_CH_NO_IBSS)
				bg_band->channels[i].flags |=
					IEEE80211_CHAN_NO_IBSS;
			if (geo->bg[i].flags & LIBIPW_CH_RADAR_DETECT)
				bg_band->channels[i].flags |=
					IEEE80211_CHAN_RADAR;
			/* No equivalent for LIBIPW_CH_80211H_RULES,
			 * LIBIPW_CH_UNIFORM_SPREADING, or
			 * LIBIPW_CH_B_ONLY... */
		}
		/* point at bitrate info */
		bg_band->bitrates = ipw2200_bg_rates;
		bg_band->n_bitrates = ipw2200_num_bg_rates;
#endif

		wdev->wiphy->bands[IEEE80211_BAND_2GHZ] = bg_band;
	}

	set_wiphy_dev(wdev->wiphy, &priv->pdev->dev);

	/* With that information in place, we can now register the wiphy... */
	if (wiphy_register(wdev->wiphy)) {
		return false;
	}

	return true;
}
#endif

#ifdef _RTL8192_EXT_PATCH_
int _rtl8192_up(struct net_device *dev,bool is_silent_reset)
#else
int _rtl8192_up(struct net_device *dev)
#endif
{
	struct r8192_priv *priv = rtllib_priv(dev);
	//int i;
	PRT_POWER_SAVE_CONTROL pPSC = (PRT_POWER_SAVE_CONTROL)(&(priv->rtllib->PowerSaveControl));
	bool init_status = true;
	priv->bDriverIsGoingToUnload = false;
#ifdef _RTL8192_EXT_PATCH_
	//YJ,add,090714. If mesh dev is already up, forbid to open ra0.
	if(priv->mesh_up){
		RT_TRACE(COMP_ERR,"%s(): since mesh0 is already up, ra0 is forbidden to open.\n",__FUNCTION__);
		return -1;
	}
	RT_TRACE(COMP_DOWN, "==========>%s()\n", __FUNCTION__);
	if(!is_silent_reset)
		priv->rtllib->iw_mode = IW_MODE_INFRA;
	if(priv->up){
		RT_TRACE(COMP_ERR,"%s():%s is up,return\n",__FUNCTION__,DRV_NAME);
		return -1;
	}
#ifdef RTL8192SE
	priv->ReceiveConfig =
				RCR_APPFCS | RCR_APWRMGT | /*RCR_ADD3 |*/
				RCR_AMF | RCR_ADF | RCR_APP_MIC | RCR_APP_ICV |
				RCR_AICV | RCR_ACRC32    |                               // Accept ICV error, CRC32 Error
				RCR_AB          | RCR_AM                |                               // Accept Broadcast, Multicast
				RCR_APM         |                                                               // Accept Physical match
				/*RCR_AAP               |*/                                                     // Accept Destination Address packets
				RCR_APP_PHYST_STAFF | RCR_APP_PHYST_RXFF |      // Accept PHY status
				(priv->EarlyRxThreshold<<RCR_FIFO_OFFSET)       ;
	// Make reference from WMAC code 2006.10.02, maybe we should disable some of the interrupt. by Emily
#elif defined RTL8192E || defined RTL8190P
	priv->ReceiveConfig = RCR_ADD3  |
			RCR_AMF | RCR_ADF |             //accept management/data
			RCR_AICV |                      //accept control frame for SW AP needs PS-poll, 2005.07.07, by rcnjko.
			RCR_AB | RCR_AM | RCR_APM |     //accept BC/MC/UC
			RCR_AAP | ((u32)7<<RCR_MXDMA_OFFSET) |
			((u32)7 << RCR_FIFO_OFFSET) | RCR_ONLYERLPKT;
#elif defined RTL8192CE
	priv->ReceiveConfig = (\
					RCR_APPFCS
					// | RCR_APWRMGT
					// |RCR_ADD3
					| RCR_AMF | RCR_ADF| RCR_APP_MIC| RCR_APP_ICV
					| RCR_AICV | RCR_ACRC32	// Accept ICV error, CRC32 Error
					| RCR_AB | RCR_AM			// Accept Broadcast, Multicast
					| RCR_APM					// Accept Physical match
					| RCR_AAP					// Accept Destination Address packets
					| RCR_APP_PHYST_RXFF		// Accept PHY status
					| RCR_HTC_LOC_CTRL
					//(pHalData->EarlyRxThreshold<<RCR_FIFO_OFFSET)	);
					);
#endif

	if(!priv->mesh_up)//AMY modify 090220
	{
		RT_TRACE(COMP_INIT, "Bringing up iface");
		priv->bfirst_init = true;
		init_status = priv->ops->initialize_adapter(dev);
		if(init_status != true)
		{
			RT_TRACE(COMP_ERR,"ERR!!! %s(): initialization is failed!\n",__FUNCTION__);
			return -1;
		}
		RT_TRACE(COMP_INIT, "start adapter finished\n");
		RT_CLEAR_PS_LEVEL(pPSC, RT_RF_OFF_LEVL_HALT_NIC);
		priv->rtllib->ieee_up=1;
		priv->bfirst_init = false;
#ifdef ENABLE_GPIO_RADIO_CTL
		if(priv->polling_timer_on == 0){//add for S3/S4
			check_rfctrl_gpio_timer((unsigned long)dev);
		}
#endif
		priv->rtllib->current_network.channel = INIT_DEFAULT_CHAN;
		priv->rtllib->current_mesh_network.channel = INIT_DEFAULT_CHAN;
		if((priv->mshobj->ext_patch_r819x_wx_set_mesh_chan) && (!is_silent_reset))
			priv->mshobj->ext_patch_r819x_wx_set_mesh_chan(dev,INIT_DEFAULT_CHAN);
		if((priv->mshobj->ext_patch_r819x_wx_set_channel) && (!is_silent_reset))
		{
			priv->mshobj->ext_patch_r819x_wx_set_channel(priv->rtllib, INIT_DEFAULT_CHAN);
		}
		printk("%s():set chan %d\n",__FUNCTION__,INIT_DEFAULT_CHAN);
		priv->rtllib->set_chan(dev, INIT_DEFAULT_CHAN);
		dm_InitRateAdaptiveMask(dev);
		watch_dog_timer_callback((unsigned long) dev);
	}
	else
	{
		priv->rtllib->sync_scan_hurryup = 1;
	}
	priv->up=1;
	priv->up_first_time = 0;
	if(!priv->rtllib->proto_started)
	{
#ifdef RTL8192E
		if(priv->rtllib->eRFPowerState!=eRfOn)
			MgntActSet_RF_State(dev, eRfOn, priv->rtllib->RfOffReason);
#endif
		if(priv->rtllib->state != RTLLIB_LINKED)
			rtllib_softmac_start_protocol(priv->rtllib, 0);
	}
	rtllib_reset_queue(priv->rtllib);
	if(!netif_queue_stopped(dev))
		netif_start_queue(dev);
	else
		netif_wake_queue(dev);
	RT_TRACE(COMP_DOWN, "<==========%s()\n", __FUNCTION__);
#else
	priv->up=1;
	priv->up_first_time = 0;
	priv->rtllib->ieee_up=1;
	RT_TRACE(COMP_INIT, "Bringing up iface");
//	printk("======>%s()\n",__FUNCTION__);
	priv->bfirst_init = true;
	init_status = priv->ops->initialize_adapter(dev);
	if(init_status != true)
	{
		RT_TRACE(COMP_ERR,"ERR!!! %s(): initialization is failed!\n",__FUNCTION__);
		return -1;
	}
	RT_TRACE(COMP_INIT, "start adapter finished\n");
	RT_CLEAR_PS_LEVEL(pPSC, RT_RF_OFF_LEVL_HALT_NIC);
	priv->bfirst_init = false;
#if defined RTL8192SE || defined RTL8192CE
	if(priv->rtllib->eRFPowerState!=eRfOn)
		MgntActSet_RF_State(dev, eRfOn, priv->rtllib->RfOffReason);
#endif

#ifdef ENABLE_GPIO_RADIO_CTL
	if(priv->polling_timer_on == 0){//add for S3/S4
		check_rfctrl_gpio_timer((unsigned long)dev);
	}
#endif

	if(priv->rtllib->state != RTLLIB_LINKED)
#ifndef CONFIG_MP
	rtllib_softmac_start_protocol(priv->rtllib);
#endif
	rtllib_reset_queue(priv->rtllib);
#ifndef CONFIG_MP
	watch_dog_timer_callback((unsigned long) dev);
#endif

#ifdef CONFIG_CRDA
	if(!rtl8192_register_wiphy_dev(dev))
		return -1;
#endif

	if(!netif_queue_stopped(dev))
		netif_start_queue(dev);
	else
		netif_wake_queue(dev);
#endif
	return 0;
}


int rtl8192_open(struct net_device *dev)
{
	struct r8192_priv *priv = rtllib_priv(dev);
	int ret;

	down(&priv->wx_sem);
	ret = rtl8192_up(dev);
	up(&priv->wx_sem);
	return ret;

}


int rtl8192_up(struct net_device *dev)
{
#ifndef _RTL8192_EXT_PATCH_
	struct r8192_priv *priv = rtllib_priv(dev);

	if (priv->up == 1) return -1;
	return _rtl8192_up(dev);
#else
	return _rtl8192_up(dev,false);
#endif
}


int rtl8192_close(struct net_device *dev)
{
	struct r8192_priv *priv = rtllib_priv(dev);
	int ret;

	down(&priv->wx_sem);

	ret = rtl8192_down(dev,true);

	up(&priv->wx_sem);

	return ret;

}

int rtl8192_down(struct net_device *dev, bool shutdownrf)
{
	struct r8192_priv *priv = rtllib_priv(dev);
	unsigned long flags = 0;
	u8 RFInProgressTimeOut = 0;

#ifdef _RTL8192_EXT_PATCH_
	if (priv->up == 0)
		return -1;

	RT_TRACE(COMP_DOWN, "==========>%s()\n", __FUNCTION__);
#ifdef ENABLE_LPS
	//LZM for PS-Poll AID issue. 090429
	if(priv->rtllib->state == RTLLIB_LINKED)
		LeisurePSLeave(dev);
#endif
	/* FIXME */
	if (!netif_queue_stopped(dev))
		netif_stop_queue(dev);
	//if(!priv->rtllib->mesh_started) //AMY modify 090220
	if(!priv->mesh_up)
	{
		printk("===>%s():mesh is not up\n",__FUNCTION__);
		priv->bDriverIsGoingToUnload = true;
		priv->up=0;
		priv->rtllib->ieee_up = 0;
		/* mesh stack has also be closed, then disalbe the hardware function at
		 * the same time */
		priv->rtllib->wpa_ie_len = 0;
		if(priv->rtllib->wpa_ie)
			kfree(priv->rtllib->wpa_ie);
		priv->rtllib->wpa_ie = NULL;
		CamResetAllEntry(dev);
		CamRestoreEachIFEntry(dev,1);
		memset(priv->rtllib->swcamtable,0,sizeof(SW_CAM_TABLE)*32);//added by amy for silent reset 090415
		rtl8192_irq_disable(dev);
		rtl8192_cancel_deferred_work(priv);
#ifndef RTL8190P
		cancel_delayed_work(&priv->rtllib->hw_wakeup_wq);
#endif
		deinit_hal_dm(dev);
		del_timer_sync(&priv->watch_dog_timer);

		rtllib_softmac_stop_protocol(priv->rtllib, 0, true);
		//added by amy 090420
		SPIN_LOCK_PRIV_RFPS(&priv->rf_ps_lock);
		while(priv->RFChangeInProgress)
		{
			SPIN_UNLOCK_PRIV_RFPS(&priv->rf_ps_lock);
			if(RFInProgressTimeOut > 100)
			{
				SPIN_LOCK_PRIV_RFPS(&priv->rf_ps_lock);
				break;
			}
			printk("===>%s():RF is in progress, need to wait until rf chang is done.\n",__FUNCTION__);
			mdelay(1);
			RFInProgressTimeOut ++;
			SPIN_LOCK_PRIV_RFPS(&priv->rf_ps_lock);
		}
		printk("=====>%s(): priv->RFChangeInProgress = true\n",__FUNCTION__);
		priv->RFChangeInProgress = true;
		SPIN_UNLOCK_PRIV_RFPS(&priv->rf_ps_lock);
		priv->ops->stop_adapter(dev, false);
		SPIN_LOCK_PRIV_RFPS(&priv->rf_ps_lock);
		printk("=====>%s(): priv->RFChangeInProgress = false\n",__FUNCTION__);
		priv->RFChangeInProgress = false;//true ?? FIX ME amy
		SPIN_UNLOCK_PRIV_RFPS(&priv->rf_ps_lock);
		//	RT_SET_PS_LEVEL(pPSC, RT_RF_OFF_LEVL_HALT_NIC);
		udelay(100);
		memset(&priv->rtllib->current_network, 0 , offsetof(struct rtllib_network, list));
		priv->rtllib->wap_set = 0; //YJ,add,090727
		priv->rtllib->current_network.channel = INIT_DEFAULT_CHAN;//AMY test 090218
		//priv->isRFOff = false;
#ifdef RTL8192SE_CONFIG_ASPM_OR_D3
		RT_ENABLE_ASPM(dev);
#endif
	} else {
		rtllib_softmac_stop_protocol(priv->rtllib, 0, true);
		memset(&priv->rtllib->current_network, 0 , offsetof(struct rtllib_network, list));
		priv->rtllib->current_network.channel = INIT_DEFAULT_CHAN;//AMY test 090218
		priv->rtllib->wap_set = 0; //YJ,add,090727
	}

	RT_TRACE(COMP_DOWN, "<==========%s()\n", __FUNCTION__);
	priv->up=0;
#else
	if (priv->up == 0) return -1;

#ifdef ENABLE_LPS
	//LZM for PS-Poll AID issue. 090429
	if(priv->rtllib->state == RTLLIB_LINKED)
		LeisurePSLeave(dev);
#endif

	priv->bDriverIsGoingToUnload = true;
	priv->up=0;
	priv->rtllib->ieee_up = 0;
	RT_TRACE(COMP_DOWN, "==========>%s()\n", __FUNCTION__);
	//	printk("==========>%s()\n", __FUNCTION__);
	// FIXME */
	if (!netif_queue_stopped(dev))
		netif_stop_queue(dev);

	//YJ,add for security,090323
	priv->rtllib->wpa_ie_len = 0;
	if(priv->rtllib->wpa_ie)
		kfree(priv->rtllib->wpa_ie);
	priv->rtllib->wpa_ie = NULL;
	CamResetAllEntry(dev);
	memset(priv->rtllib->swcamtable,0,sizeof(SW_CAM_TABLE)*32);//added by amy for silent reset 090415
	rtl8192_irq_disable(dev);
#if 0
	if(!priv->rtllib->bSupportRemoteWakeUp) {
		MgntActSet_RF_State(dev, eRfOff, RF_CHANGE_BY_INIT);
		// 2006.11.30. System reset bit
		ulRegRead = read_nic_dword(dev, CPU_GEN);
		ulRegRead|=CPU_GEN_SYSTEM_RESET;
		write_nic_dword(dev, CPU_GEN, ulRegRead);
	} else {
		//2008.06.03 for WOL
		write_nic_dword(dev, WFCRC0, 0xffffffff);
		write_nic_dword(dev, WFCRC1, 0xffffffff);
		write_nic_dword(dev, WFCRC2, 0xffffffff);
#ifdef RTL8190P
		//GPIO 0 = true
		ucRegRead = read_nic_byte(dev, GPO);
		ucRegRead |= BIT0;
		write_nic_byte(dev, GPO, ucRegRead);
#endif
		//Write PMR register
		write_nic_byte(dev, PMR, 0x5);
		//Disable tx, enanble rx
		write_nic_byte(dev, MacBlkCtrl, 0xa);
	}
#endif
	//	flush_scheduled_work();
	del_timer_sync(&priv->watch_dog_timer);
	rtl8192_cancel_deferred_work(priv);
#ifndef RTL8190P
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
	cancel_delayed_work(&priv->rtllib->hw_wakeup_wq);
#endif
#endif

	rtllib_softmac_stop_protocol(priv->rtllib,true);
	//added by amy 090420
	spin_lock_irqsave(&priv->rf_ps_lock,flags);
	while(priv->RFChangeInProgress)
	{
		spin_unlock_irqrestore(&priv->rf_ps_lock,flags);
		if(RFInProgressTimeOut > 100)
		{
			spin_lock_irqsave(&priv->rf_ps_lock,flags);
			break;
		}
		printk("===>%s():RF is in progress, need to wait until rf chang is done.\n",__FUNCTION__);
		mdelay(1);
		RFInProgressTimeOut ++;
		spin_lock_irqsave(&priv->rf_ps_lock,flags);
	}
	priv->RFChangeInProgress = true;
	spin_unlock_irqrestore(&priv->rf_ps_lock,flags);
	priv->ops->stop_adapter(dev, false);
	spin_lock_irqsave(&priv->rf_ps_lock,flags);
	priv->RFChangeInProgress = false;//true ?? FIX ME amy
	spin_unlock_irqrestore(&priv->rf_ps_lock,flags);
	//	RT_SET_PS_LEVEL(pPSC, RT_RF_OFF_LEVL_HALT_NIC);
	udelay(100);
	memset(&priv->rtllib->current_network, 0 , offsetof(struct rtllib_network, list));
	//priv->isRFOff = false;
#ifdef RTL8192SE_CONFIG_ASPM_OR_D3
	RT_ENABLE_ASPM(dev);
#endif
	RT_TRACE(COMP_DOWN, "<==========%s()\n", __FUNCTION__);
#endif

#ifdef CONFIG_MP
	if (priv->bCckContTx) {
		printk("####RTL819X MP####stop single cck continious TX\n");
		mpt_StopCckCoNtTx(dev);
	}
	if (priv->bOfdmContTx) {
		printk("####RTL819X MP####stop single ofdm continious TX\n");
		mpt_StopOfdmContTx(dev);
	}
	if (priv->bSingleCarrier) {
		printk("####RTL819X MP####stop single carrier mode\n");
		MPT_ProSetSingleCarrier(dev, false);
	}
#endif
	return 0;
}

void rtl8192_commit(struct net_device *dev)
{
	struct r8192_priv *priv = rtllib_priv(dev);

#ifdef _RTL8192_EXT_PATCH_
	if ((priv->up == 0) && (priv->mesh_up == 0)) return ;
	rtllib_softmac_stop_protocol(priv->rtllib,0,true);
	rtl8192_irq_disable(dev);
	priv->ops->stop_adapter(dev, true);
	priv->up = 0;
	_rtl8192_up(dev,false);
#else
	if (priv->up == 0) return ;
	rtllib_softmac_stop_protocol(priv->rtllib,true);
	rtl8192_irq_disable(dev);
	priv->ops->stop_adapter(dev, true);
	_rtl8192_up(dev);
#endif

}

void rtl8192_restart(void *data)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20))
	struct r8192_priv *priv = container_of_work_rsl(data, struct r8192_priv, reset_wq);
	struct net_device *dev = priv->rtllib->dev;
#else
	struct net_device *dev = (struct net_device *)data;
        struct r8192_priv *priv = rtllib_priv(dev);
#endif

	down(&priv->wx_sem);

	rtl8192_commit(dev);

	up(&priv->wx_sem);
}

#ifndef HAVE_NET_DEVICE_OPS
static void r8192_set_multicast(struct net_device *dev)
{
	struct r8192_priv *priv = rtllib_priv(dev);
	short promisc;

	//down(&priv->wx_sem);

	// FIXME FIXME */

	promisc = (dev->flags & IFF_PROMISC) ? 1:0;

	if (promisc != priv->promisc) {
		;
	//	rtl8192_commit(dev);
	}

	priv->promisc = promisc;

	//schedule_work(&priv->reset_wq);
	//up(&priv->wx_sem);
}
#endif

int r8192_set_mac_adr(struct net_device *dev, void *mac)
{
	struct r8192_priv *priv = rtllib_priv(dev);
	struct sockaddr *addr = mac;

	down(&priv->wx_sem);

	memcpy(dev->dev_addr, addr->sa_data, ETH_ALEN);

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0))
	schedule_work(&priv->reset_wq);
#else
	schedule_task(&priv->reset_wq);
#endif
	up(&priv->wx_sem);

	return 0;
}


/* based on ipw2200 driver */
int rtl8192_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);
	struct iwreq *wrq = (struct iwreq *)rq;
	int ret=-1;
	struct rtllib_device *ieee = priv->rtllib;
	u32 key[4];
	u8 broadcast_addr[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
	u8 zero_addr[6] = {0};
	struct iw_point *p = &wrq->u.data;
	struct ieee_param *ipw = NULL;//(struct ieee_param *)wrq->u.data.pointer;

	down(&priv->wx_sem);


	if (p->length < sizeof(struct ieee_param) || !p->pointer){
		ret = -EINVAL;
		goto out;
	}

	ipw = (struct ieee_param *)kmalloc(p->length, GFP_KERNEL);
	if (ipw == NULL){
		ret = -ENOMEM;
		goto out;
	}
	if (copy_from_user(ipw, p->pointer, p->length)) {
		kfree(ipw);
		ret = -EFAULT;
		goto out;
	}

	switch (cmd) {
		case RTL_IOCTL_WPA_SUPPLICANT:
			//parse here for HW security
			if (ipw->cmd == IEEE_CMD_SET_ENCRYPTION)
			{
				if (ipw->u.crypt.set_tx)
				{
					if (strcmp(ipw->u.crypt.alg, "CCMP") == 0)
						ieee->pairwise_key_type = KEY_TYPE_CCMP;
					else if (strcmp(ipw->u.crypt.alg, "TKIP") == 0)
						ieee->pairwise_key_type = KEY_TYPE_TKIP;
					else if (strcmp(ipw->u.crypt.alg, "WEP") == 0)
					{
						if (ipw->u.crypt.key_len == 13)
							ieee->pairwise_key_type = KEY_TYPE_WEP104;
						else if (ipw->u.crypt.key_len == 5)
							ieee->pairwise_key_type = KEY_TYPE_WEP40;
					}
					else
						ieee->pairwise_key_type = KEY_TYPE_NA;

					if (ieee->pairwise_key_type)
					{
						//      FIXME:these two lines below just to fix ipw interface bug, that is, it will never set mode down to driver. So treat it as ADHOC mode, if no association procedure. WB. 2009.02.04
						if (memcmp(ieee->ap_mac_addr, zero_addr, 6) == 0)
							ieee->iw_mode = IW_MODE_ADHOC;

						memcpy((u8*)key, ipw->u.crypt.key, 16);
						EnableHWSecurityConfig8192(dev);
						//we fill both index entry and 4th entry for pairwise key as in IPW interface, adhoc will only get here, so we need index entry for its default key serching!
						//added by WB.
#ifdef _RTL8192_EXT_PATCH_
						set_swcam(dev, 4, ipw->u.crypt.idx, ieee->pairwise_key_type, (u8*)ieee->ap_mac_addr, 0, key,0);
#else
						set_swcam(dev, 4, ipw->u.crypt.idx, ieee->pairwise_key_type, (u8*)ieee->ap_mac_addr, 0, key);
#endif
						setKey(dev, 4, ipw->u.crypt.idx, ieee->pairwise_key_type, (u8*)ieee->ap_mac_addr, 0, key);
						if (ieee->iw_mode == IW_MODE_ADHOC){  //LEAP WEP will never set this.
#ifdef _RTL8192_EXT_PATCH_
							set_swcam(dev, ipw->u.crypt.idx, ipw->u.crypt.idx, ieee->pairwise_key_type, (u8*)ieee->ap_mac_addr, 0, key,0);
#else
							set_swcam(dev, ipw->u.crypt.idx, ipw->u.crypt.idx, ieee->pairwise_key_type, (u8*)ieee->ap_mac_addr, 0, key);
#endif
							setKey(dev, ipw->u.crypt.idx, ipw->u.crypt.idx, ieee->pairwise_key_type, (u8*)ieee->ap_mac_addr, 0, key);
						}
					}
#ifdef RTL8192E
					if ((ieee->pairwise_key_type == KEY_TYPE_CCMP) && ieee->pHTInfo->bCurrentHTSupport){
						write_nic_byte(dev, 0x173, 1); //fix aes bug
					}
#endif

				}
				else //if (ipw->u.crypt.idx) //group key use idx > 0
				{
					memcpy((u8*)key, ipw->u.crypt.key, 16);
					if (strcmp(ipw->u.crypt.alg, "CCMP") == 0)
						ieee->group_key_type= KEY_TYPE_CCMP;
					else if (strcmp(ipw->u.crypt.alg, "TKIP") == 0)
						ieee->group_key_type = KEY_TYPE_TKIP;
					else if (strcmp(ipw->u.crypt.alg, "WEP") == 0)
					{
						if (ipw->u.crypt.key_len == 13)
							ieee->group_key_type = KEY_TYPE_WEP104;
						else if (ipw->u.crypt.key_len == 5)
							ieee->group_key_type = KEY_TYPE_WEP40;
					}
					else
						ieee->group_key_type = KEY_TYPE_NA;

					if (ieee->group_key_type)
					{
#ifdef _RTL8192_EXT_PATCH_
						set_swcam(	dev,
								ipw->u.crypt.idx,
								ipw->u.crypt.idx,		//KeyIndex
								ieee->group_key_type,	//KeyType
								broadcast_addr,	//MacAddr
								0,		//DefaultKey
								key,		//KeyContent
								0);
#else
						set_swcam(	dev,
								ipw->u.crypt.idx,
								ipw->u.crypt.idx,		//KeyIndex
								ieee->group_key_type,	//KeyType
								broadcast_addr,	//MacAddr
								0,		//DefaultKey
								key);		//KeyContent
#endif
						setKey(	dev,
								ipw->u.crypt.idx,
								ipw->u.crypt.idx,		//KeyIndex
								ieee->group_key_type,	//KeyType
								broadcast_addr,	//MacAddr
								0,		//DefaultKey
								key);		//KeyContent
					}
				}
			}
#ifdef JOHN_DEBUG
			//john's test 0711
			{
				int i;
				printk("@@ wrq->u pointer = ");
				for(i=0;i<wrq->u.data.length;i++){
					if(i%10==0) printk("\n");
					printk( "%8x|", ((u32*)wrq->u.data.pointer)[i] );
				}
				printk("\n");
			}
#endif //JOHN_DEBUG*/
#ifdef _RTL8192_EXT_PATCH_
			ret = rtllib_wpa_supplicant_ioctl(priv->rtllib, &wrq->u.data,0);
#else
			ret = rtllib_wpa_supplicant_ioctl(priv->rtllib, &wrq->u.data);
#endif
			break;

		default:
			ret = -EOPNOTSUPP;
			break;
	}

	kfree(ipw);
out:
	up(&priv->wx_sem);

	return ret;
}

//warning message WB
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
void rtl8192_interrupt(int irq, void *netdev, struct pt_regs *regs)
#else
irqreturn_t rtl8192_interrupt(int irq, void *netdev, struct pt_regs *regs)
#endif
#else
irqreturn_t rtl8192_interrupt(int irq, void *netdev)
#endif
{
    struct net_device *dev = (struct net_device *) netdev;
    struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);
#ifdef _RTL8192_EXT_PATCH_
    struct net_device *meshdev = ((struct rtllib_device *)netdev_priv_rsl(dev))->meshdev;
#endif
    unsigned long flags;
    u32 inta;//, intb;
#ifdef RTL8192SE
    u32 intb;
    intb = 0;
#endif
    // We should return IRQ_NONE, but for now let me keep this */
    if(priv->irq_enabled == 0){
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
        return;
#else
        return IRQ_HANDLED;
#endif
    }

    spin_lock_irqsave(&priv->irq_th_lock,flags);

    //ISR: 4bytes
#ifdef RTL8192SE
    inta = read_nic_dword(dev, ISR) & priv->irq_mask[0];
#else
    inta = read_nic_dword(dev, ISR) ;//& priv->irq_mask[0];
#endif
    write_nic_dword(dev,ISR,inta); // reset int situation
#ifdef RTL8192SE
    intb = read_nic_dword(dev, ISR+4);
    write_nic_dword(dev, ISR+4, intb);
#endif
    priv->stats.shints++;
   // printk("Enter interrupt, ISR value = 0x%08x\n", inta);
    if (!inta) {
        spin_unlock_irqrestore(&priv->irq_th_lock,flags);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
        return;
#else
        return IRQ_HANDLED;
#endif
        /*
           most probably we can safely return IRQ_NONE,
           but for now is better to avoid problems
           */
    }

    if (inta == 0xffff) {
        // HW disappared */
        spin_unlock_irqrestore(&priv->irq_th_lock,flags);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
        return;
#else
        return IRQ_HANDLED;
#endif
    }

    priv->stats.ints++;
#ifdef DEBUG_IRQ
    DMESG("NIC irq %x",inta);
#endif

#ifdef _RTL8192_EXT_PATCH_
    if (!netif_running(dev) && !netif_running(meshdev)) {
#else
    if (!netif_running(dev)) {
#endif
        spin_unlock_irqrestore(&priv->irq_th_lock,flags);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
        return;
#else
        return IRQ_HANDLED;
#endif
    }

#ifdef RTL8192SE
    if(intb & IMR_TBDOK){
        RT_TRACE(COMP_INTR, "beacon ok interrupt!\n");
        priv->stats.txbeaconokint++;
    }

    if(intb & IMR_TBDER){
        RT_TRACE(COMP_INTR, "beacon ok interrupt!\n");
        priv->stats.txbeaconerr++;
    }
#else
    if(inta & IMR_TBDOK){
        RT_TRACE(COMP_INTR, "beacon ok interrupt!\n");
        priv->stats.txbeaconokint++;
    }

    if(inta & IMR_TBDER){
        RT_TRACE(COMP_INTR, "beacon ok interrupt!\n");
        priv->stats.txbeaconerr++;
    }
#endif

    if(inta & IMR_BDOK) {
        RT_TRACE(COMP_INTR, "beacon interrupt!\n");
        rtl8192_tx_isr(dev, BEACON_QUEUE);
    }

    if(inta  & IMR_MGNTDOK ) {
        RT_TRACE(COMP_INTR, "Manage ok interrupt!\n");
        priv->stats.txmanageokint++;
        rtl8192_tx_isr(dev,MGNT_QUEUE);
	 spin_unlock_irqrestore(&priv->irq_th_lock,flags);
         {
             if (priv->rtllib->ack_tx_to_ieee){
                 if (rtl8192_is_tx_queue_empty(dev)){
                     priv->rtllib->ack_tx_to_ieee = 0;
                     rtllib_ps_tx_ack(priv->rtllib, 1);
                 }
             }
         }
	 spin_lock_irqsave(&priv->irq_th_lock,flags);
    }

#ifndef RTL8192CE
    if (inta & IMR_COMDOK) {
        priv->stats.txcmdpktokint++;
        rtl8192_tx_isr(dev,TXCMD_QUEUE);
    }
#endif

    if (inta & IMR_HIGHDOK) {
        //printk("=========================>%s()HIGH_QUEUE\n", __func__);
        rtl8192_tx_isr(dev,HIGH_QUEUE);
    }

    if (inta & IMR_ROK){
#ifdef DEBUG_RX
        DMESG("Frame arrived !");
#endif
        RT_TRACE(COMP_INTR, "Rx ok interrupt!\n");
        priv->stats.rxint++;
	priv->InterruptLog.nIMR_ROK++;
        tasklet_schedule(&priv->irq_rx_tasklet);
    }

    if (inta & IMR_BcnInt) {
        RT_TRACE(COMP_INTR, "prepare beacon for interrupt!\n");
        tasklet_schedule(&priv->irq_prepare_beacon_tasklet);
    }

    if (inta & IMR_RDU) {
        RT_TRACE(COMP_INTR, "rx descriptor unavailable!\n");
        priv->stats.rxrdu++;
        // reset int situation */
#ifndef RTL8192CE
        write_nic_dword(dev,INTA_MASK,read_nic_dword(dev, INTA_MASK) & ~IMR_RDU);
#endif
        tasklet_schedule(&priv->irq_rx_tasklet);
    }

    if (inta & IMR_RXFOVW) {
        RT_TRACE(COMP_INTR, "rx overflow !\n");
        priv->stats.rxoverflow++;
        tasklet_schedule(&priv->irq_rx_tasklet);
    }

    if (inta & IMR_TXFOVW) priv->stats.txoverflow++;

    if (inta & IMR_BKDOK) {
        RT_TRACE(COMP_INTR, "BK Tx OK interrupt!\n");
        priv->stats.txbkokint++;
        priv->rtllib->LinkDetectInfo.NumTxOkInPeriod++;
        rtl8192_tx_isr(dev,BK_QUEUE);
    //    rtl8192_try_wake_queue(dev, BK_QUEUE);
    }

    if (inta & IMR_BEDOK) {
        RT_TRACE(COMP_INTR, "BE TX OK interrupt!\n");
        priv->stats.txbeokint++;
        priv->rtllib->LinkDetectInfo.NumTxOkInPeriod++;
        rtl8192_tx_isr(dev,BE_QUEUE);
   //     rtl8192_try_wake_queue(dev, BE_QUEUE);
    }

    if (inta & IMR_VIDOK) {
        RT_TRACE(COMP_INTR, "VI TX OK interrupt!\n");
        priv->stats.txviokint++;
        priv->rtllib->LinkDetectInfo.NumTxOkInPeriod++;
        rtl8192_tx_isr(dev,VI_QUEUE);
    //    rtl8192_try_wake_queue(dev, VI_QUEUE);
    }

    if (inta & IMR_VODOK) {
        priv->stats.txvookint++;
        RT_TRACE(COMP_INTR, "Vo TX OK interrupt!\n");
        priv->rtllib->LinkDetectInfo.NumTxOkInPeriod++;
        rtl8192_tx_isr(dev,VO_QUEUE);
  //      rtl8192_try_wake_queue(dev, VO_QUEUE);
    }

    spin_unlock_irqrestore(&priv->irq_th_lock,flags);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
    return;
#else
    return IRQ_HANDLED;
#endif
}

//find vid of pci card from config space
bool rtl8192_pci_findadapter(struct pci_dev *pdev, struct net_device *dev)
{
	//u32  size;
	u16  DeviceID;
	u8 RevisionID = 0;

	struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);

	pci_read_config_word(pdev, 0x2, &DeviceID);
	pci_read_config_byte(pdev, 0x8, &RevisionID);
	//
	// Decide hardware type here.
	//
	if (DeviceID == HAL_HW_PCI_8190_DEVICE_ID ||DeviceID == HAL_HW_PCI_0045_DEVICE_ID ||
		DeviceID == HAL_HW_PCI_0046_DEVICE_ID ||DeviceID == HAL_HW_PCI_DLINK_DEVICE_ID){
		printk("Adapter(8190 PCI) is found - DeviceID=%x\n", DeviceID);
		//priv->HardwareType=HARDWARE_TYPE_RTL8190P;
		priv->ops->nic_type = priv->card_8192 = NIC_8190P;
	} else if (DeviceID == HAL_HW_PCI_8192_DEVICE_ID ||DeviceID == HAL_HW_PCI_0044_DEVICE_ID ||
		DeviceID == HAL_HW_PCI_0047_DEVICE_ID || DeviceID == HAL_HW_PCI_8192SE_DEVICE_ID ||
		DeviceID == HAL_HW_PCI_8174_DEVICE_ID || DeviceID == HAL_HW_PCI_8173_DEVICE_ID ||
		DeviceID == HAL_HW_PCI_8172_DEVICE_ID || DeviceID == HAL_HW_PCI_8171_DEVICE_ID) {

		// 8192e and and 8192se may have the same device ID 8192. However, their Revision
		// ID is different
		switch(RevisionID)
		{
			case HAL_HW_PCI_REVISION_ID_8192PCIE:
				printk("Adapter(8192 PCI-E) is found - DeviceID=%x\n", DeviceID);
				//priv->HardwareType = HARDWARE_TYPE_RTL8192E;
				priv->ops->nic_type = priv->card_8192 = NIC_8192E;
				break;
			case HAL_HW_PCI_REVISION_ID_8192SE:
				printk("Adapter(8192SE) is found - DeviceID=%x\n", DeviceID);
				//priv->HardwareType = HARDWARE_TYPE_RTL8192SE;
				priv->card_8192 = NIC_8192SE;
				break;
			default:
				printk("UNKNOWN nic type(%4x:%4x)\n", pdev->vendor, pdev->device);
				priv->card_8192 = NIC_UNKNOWN;

			return false;
			break;

		}
	} else
		if (DeviceID == HAL_HW_PCI_8192CET_DEVICE_ID ||DeviceID == HAL_HW_PCI_8192CE_DEVICE_ID ||
		DeviceID == HAL_HW_PCI_8191CE_DEVICE_ID ||DeviceID == HAL_HW_PCI_8188CE_DEVICE_ID) {
			printk("Adapter(8192CE) is found - DeviceID=%x\n", DeviceID);
		//priv->HardwareType = HARDWARE_TYPE_RTL8192CE;
			priv->ops->nic_type = priv->card_8192 = NIC_8192CE;
	} else {
		printk("Unknown device - DeviceID=%x\n", DeviceID);
		//priv->HardwareType = HAL_DEFAULT_HARDWARE_TYPE;
		priv->ops->nic_type = priv->card_8192 = NIC_8192DE;
	}

	return true;
}

/****************************************************************************
     ---------------------------- PCI_STUFF---------------------------
*****************************************************************************/
#ifdef HAVE_NET_DEVICE_OPS
static const struct net_device_ops rtl8192_netdev_ops = {
	.ndo_open = rtl8192_open,
	.ndo_stop = rtl8192_close,
//	.ndo_get_stats = rtl8192_stats,
	.ndo_tx_timeout = tx_timeout,
	.ndo_do_ioctl = rtl8192_ioctl,
	.ndo_set_mac_address = r8192_set_mac_adr,
	.ndo_validate_addr = eth_validate_addr,
	.ndo_change_mtu = eth_change_mtu,
	.ndo_start_xmit = rtllib_xmit,
};
#endif

static int rtl8192_pci_probe(struct pci_dev *pdev,
			 const struct pci_device_id *id)
{
	unsigned long ioaddr = 0;
	struct net_device *dev = NULL;
	struct r8192_priv *priv= NULL;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
	struct rtl819x_ops *ops = (struct rtl819x_ops *)(id->driver_data);
#endif

#ifdef CONFIG_RTL8192_IO_MAP
	unsigned long pio_start, pio_len, pio_flags;
#else
	unsigned long pmem_start, pmem_len, pmem_flags;
#endif //end #ifdef RTL_IO_MAP
	int err = 0;
#ifdef _RTL8192_EXT_PATCH_
	int result;
	struct net_device *meshdev = NULL;
	struct meshdev_priv *mpriv;
	char meshifname[]="mesh0";
#endif

	RT_TRACE(COMP_INIT,"Configuring chip resources");

	if( pci_enable_device (pdev) ){
		RT_TRACE(COMP_ERR,"Failed to enable PCI device");
		return -EIO;
	}

	pci_set_master(pdev);
	//pci_set_wmi(pdev);
	pci_set_dma_mask(pdev, 0xffffff00ULL);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
	pci_set_consistent_dma_mask(pdev,0xffffff00ULL);
#endif
	dev = alloc_rtllib(sizeof(struct r8192_priv));
	if (!dev)
		return -ENOMEM;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	SET_MODULE_OWNER(dev);
#endif

	pci_set_drvdata(pdev, dev);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
	SET_NETDEV_DEV(dev, &pdev->dev);
#endif
	priv = rtllib_priv(dev);
	priv->rtllib = (struct rtllib_device *)netdev_priv_rsl(dev);
	priv->pdev=pdev;
	priv->rtllib->pdev=pdev;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
	if((pdev->subsystem_vendor == PCI_VENDOR_ID_DLINK)&&(pdev->subsystem_device == 0x3304)){
		priv->rtllib->bSupportRemoteWakeUp = 1;
	} else
#endif
	{
		priv->rtllib->bSupportRemoteWakeUp = 0;
	}

#ifdef CONFIG_RTL8192_IO_MAP
	pio_start = (unsigned long)pci_resource_start (pdev, 0);
	pio_len = (unsigned long)pci_resource_len (pdev, 0);
	pio_flags = (unsigned long)pci_resource_flags (pdev, 0);

	if (!(pio_flags & IORESOURCE_IO)) {
		RT_TRACE(COMP_ERR,"region #0 not a PIO resource, aborting");
		goto fail;
	}

	//DMESG("IO space @ 0x%08lx", pio_start );
	if( ! request_region( pio_start, pio_len, DRV_NAME ) ){
		RT_TRACE(COMP_ERR,"request_region failed!");
		goto fail;
	}

	ioaddr = pio_start;
	dev->base_addr = ioaddr; // device I/O address
#else
#ifdef RTL8192CE
	pmem_start = pci_resource_start(pdev, 2);
	pmem_len = pci_resource_len(pdev, 2);
	pmem_flags = pci_resource_flags (pdev, 2);
#else
	pmem_start = pci_resource_start(pdev, 1);
	pmem_len = pci_resource_len(pdev, 1);
	pmem_flags = pci_resource_flags (pdev, 1);
#endif

	if (!(pmem_flags & IORESOURCE_MEM)) {
		RT_TRACE(COMP_ERR,"region #1 not a MMIO resource, aborting");
		goto fail;
	}

	//DMESG("Memory mapped space @ 0x%08lx ", pmem_start);
	if (!request_mem_region(pmem_start, pmem_len, DRV_NAME)) {
		RT_TRACE(COMP_ERR,"request_mem_region failed!");
		goto fail;
	}


	ioaddr = (unsigned long)ioremap_nocache( pmem_start, pmem_len);
	if( ioaddr == (unsigned long)NULL ){
		RT_TRACE(COMP_ERR,"ioremap failed!");
	       // release_mem_region( pmem_start, pmem_len );
		goto fail1;
	}

	dev->mem_start = ioaddr; // shared mem start
	dev->mem_end = ioaddr + pci_resource_len(pdev, 0); // shared mem end

#endif //end #ifdef RTL_IO_MAP
#ifdef RTL8192SE
        // Disable Clk Request */
        pci_write_config_byte(pdev, 0x81,0);
        // leave D3 mode */
        pci_write_config_byte(pdev,0x44,0);
        pci_write_config_byte(pdev,0x04,0x06);
        pci_write_config_byte(pdev,0x04,0x07);
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
	priv->ops = ops;
#else
#if defined RTL8190P || defined RTL8192E
	priv->ops = &rtl819xp_ops;
#else
	priv->ops = &rtl8192se_ops;
#endif
#endif

	if(rtl8192_pci_findadapter(pdev, dev) == false)
		goto fail1;

	dev->irq = pdev->irq;
	priv->irq = 0;

#ifdef HAVE_NET_DEVICE_OPS
	dev->netdev_ops = &rtl8192_netdev_ops;
#else
	dev->netdev_ops.ndo_open = rtl8192_open;
	dev->stop = rtl8192_close;
	dev->tx_timeout = tx_timeout;
	dev->do_ioctl = rtl8192_ioctl;
	dev->set_multicast_list = r8192_set_multicast;
	dev->set_mac_address = r8192_set_mac_adr;
	dev->hard_start_xmit = rtllib_xmit;
#endif

#if WIRELESS_EXT >= 12
#if WIRELESS_EXT < 17
        dev->get_wireless_stats = r8192_get_wireless_stats;
#endif
        dev->wireless_handlers = (struct iw_handler_def *) &r8192_wx_handlers_def;
#endif
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
	dev->ethtool_ops = &rtl819x_ethtool_ops;
#endif

	dev->type = ARPHRD_ETHER;
	dev->watchdog_timeo = HZ*3;	//modified by john, 0805

	if (dev_alloc_name(dev, ifname) < 0){
                RT_TRACE(COMP_INIT, "Oops: devname already taken! Trying wlan%%d...\n");
//		ifname = "wlan%d";
		dev_alloc_name(dev, ifname);
        }

	RT_TRACE(COMP_INIT, "Driver probe completed1\n");
	if(rtl8192_init(dev)!=0){
		RT_TRACE(COMP_ERR, "Initialization failed");
		goto fail1;
	}

	netif_carrier_off(dev);
	netif_stop_queue(dev);

	register_netdev(dev);
	RT_TRACE(COMP_INIT, "dev name=======> %s\n",dev->name);
	err = rtl_debug_module_init(priv, dev->name);
	if (err) {
		RT_TRACE(COMP_DBG, "failed to create debugfs files. Ignoring error: %d\n", err);
	}

#ifdef ENABLE_GPIO_RADIO_CTL
	if(priv->polling_timer_on == 0){//add for S3/S4
		check_rfctrl_gpio_timer((unsigned long)dev);
	}
#endif
#ifdef _RTL8192_EXT_PATCH_
	meshdev = alloc_netdev(sizeof(struct meshdev_priv),meshifname,meshdev_init);
	netif_stop_queue(meshdev);
	memcpy(meshdev->dev_addr, dev->dev_addr, ETH_ALEN);
	DMESG("Card MAC address for meshdev is "MAC_FMT, MAC_ARG(meshdev->dev_addr));

	meshdev->base_addr = dev->base_addr;
	meshdev->irq = dev->irq;
	meshdev->mem_start = dev->mem_start;
	meshdev->mem_end = dev->mem_end;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21)
	SET_NETDEV_DEV(meshdev, dev->dev.parent);
#endif

	if ((result = register_netdev(meshdev)))
	{
		printk("Error %i registering device %s",result, meshdev->name);
		goto fail;
	}
	else
	{
		mpriv = (struct meshdev_priv *)netdev_priv_rsl(meshdev);
		priv->rtllib->meshdev=meshdev;
		priv->rtllib->meshstats=meshdev_stats(meshdev);
		priv->rtllib->only_mesh = 1;  //YJ,test,090211
		priv->rtllib->p2pmode = 0;
		priv->rtllib->is_server_eth0 = 0;
		priv->rtllib->serverExtChlOffset = 0;
		priv->rtllib->APExtChlOffset = 0;
		mpriv->rtllib = priv->rtllib;
		mpriv->priv = priv;
	//	mpriv->priv->up = 0;
	}
#endif//for _RTL8192_EXT_PATCH_

	RT_TRACE(COMP_INIT, "Driver probe completed\n");
	return 0;

fail1:
#ifdef CONFIG_RTL8192_IO_MAP

	if( dev->base_addr != 0 ){

		release_region(dev->base_addr,
	       pci_resource_len(pdev, 0) );
	}
#else
	if( dev->mem_start != (unsigned long)NULL ){
		iounmap( (void *)dev->mem_start );
#ifdef RTL8192CE
		release_mem_region( pci_resource_start(pdev, 2),
				    pci_resource_len(pdev, 2) );
#else
		release_mem_region( pci_resource_start(pdev, 1),
				    pci_resource_len(pdev, 1) );
#endif
	}
#endif //end #ifdef RTL_IO_MAP

fail:
	if(dev){

		if (priv->irq) {
			free_irq(dev->irq, dev);
			dev->irq=0;
		}
		free_rtllib(dev);
	}

	pci_disable_device(pdev);

	DMESG("wlan driver load failed\n");
	pci_set_drvdata(pdev, NULL);
	return -ENODEV;

}

static void rtl8192_pci_disconnect(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct r8192_priv *priv ;
#ifdef _RTL8192_EXT_PATCH_
	struct net_device *meshdev;
#endif

	if(dev){

		unregister_netdev(dev);

		priv = rtllib_priv(dev);
#ifdef _RTL8192_EXT_PATCH_
		if(priv && priv->mshobj)
		{
			if(priv->mshobj->ext_patch_remove_proc)
				priv->mshobj->ext_patch_remove_proc(priv);
			priv->rtllib->ext_patch_rtllib_start_protocol = 0;
			priv->rtllib->ext_patch_rtllib_stop_protocol = 0;
			priv->rtllib->ext_patch_rtllib_probe_req_1 = 0;
			priv->rtllib->ext_patch_rtllib_probe_req_2 = 0;
			priv->rtllib->ext_patch_rtllib_rx_frame_softmac_on_auth =0;
			priv->rtllib->ext_patch_rtllib_rx_frame_softmac_on_deauth =0;
			priv->rtllib->ext_patch_rtllib_rx_frame_softmac_on_peerlink_open = 0;
			priv->rtllib->ext_patch_rtllib_rx_frame_softmac_on_peerlink_confirm = 0;
			priv->rtllib->ext_patch_rtllib_rx_frame_softmac_on_peerlink_close = 0;
			priv->rtllib->ext_patch_rtllib_rx_frame_softmac_on_linkmetric_report= 0;
			priv->rtllib->ext_patch_rtllib_rx_frame_softmac_on_linkmetric_req= 0;
			priv->rtllib->ext_patch_rtllib_rx_frame_softmac_on_pathselect_preq = 0;
			priv->rtllib->ext_patch_rtllib_rx_frame_softmac_on_pathselect_prep=0;
			priv->rtllib->ext_patch_rtllib_rx_frame_softmac_on_pathselect_perr = 0;
			priv->rtllib->ext_patch_rtllib_rx_frame_softmac_on_pathselect_rann=0;
			priv->rtllib->ext_patch_rtllib_rx_frame_softmac_on_pathselect_pann=0;
			priv->rtllib->ext_patch_rtllib_ext_stop_scan_wq_set_channel = 0;
			priv->rtllib->ext_patch_rtllib_process_probe_response_1 = 0;
			priv->rtllib->ext_patch_rtllib_rx_mgt_on_probe_req = 0;
			priv->rtllib->ext_patch_rtllib_rx_mgt_update_expire = 0;
			priv->rtllib->ext_patch_rtllib_rx_on_rx = 0;
			priv->rtllib->ext_patch_get_beacon_get_probersp = 0;
			priv->rtllib->ext_patch_rtllib_rx_frame_get_hdrlen = 0;
			priv->rtllib->ext_patch_rtllib_rx_frame_get_mac_hdrlen = 0;
			priv->rtllib->ext_patch_rtllib_rx_frame_get_mesh_hdrlen_llc = 0;
			priv->rtllib->ext_patch_rtllib_rx_is_valid_framectl = 0;
			priv->rtllib->ext_patch_rtllib_softmac_xmit_get_rate = 0;
			priv->rtllib->ext_patch_rtllib_tx_data = 0;
			priv->rtllib->ext_patch_rtllib_is_mesh = 0;
			priv->rtllib->ext_patch_rtllib_create_crypt_for_peer = 0;
			priv->rtllib->ext_patch_rtllib_get_peermp_htinfo = 0;
			priv->rtllib->ext_patch_rtllib_update_ratr_mask = 0;
		        priv->rtllib->ext_patch_rtllib_send_ath_commit = 0;
		        priv->rtllib->ext_patch_rtllib_send_ath_confirm = 0;
		        priv->rtllib->ext_patch_rtllib_rx_ath_commit = 0;
		        priv->rtllib->ext_patch_rtllib_rx_ath_confirm = 0;
                        free_mshobj(&priv->mshobj);
		}
#endif // _RTL8187_EXT_PATCH_
#ifdef ENABLE_GPIO_RADIO_CTL
		del_timer_sync(&priv->gpio_polling_timer);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
		cancel_delayed_work(&priv->gpio_change_rf_wq);
#endif
		priv->polling_timer_on = 0;//add for S3/S4
#endif
		rtl_debug_module_remove(priv);
#ifdef _RTL8192_EXT_PATCH_
		rtl8192_down(dev,true);
		if(priv && priv->rtllib->meshdev)
		{
			meshdev = priv->rtllib->meshdev;
			priv->rtllib->meshdev = NULL;

			if(meshdev){
				meshdev_down(meshdev);
				unregister_netdev(meshdev);
				//dev->destruct will call free_netdev
			}
		}
#else
		rtl8192_down(dev,true);
#endif
		deinit_hal_dm(dev);
#ifdef RTL8192SE
#ifdef RTL8192SE_CONFIG_ASPM_OR_D3
		PlatformDisableASPM(dev);
#endif
		//added by amy for LED 090313
		DeInitSwLeds(dev);
#endif
		if (priv->pFirmware)
		{
			vfree(priv->pFirmware);
			priv->pFirmware = NULL;
		}
	//	priv->rf_close(dev);
	//	rtl8192_usb_deleteendpoints(dev);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
		destroy_workqueue(priv->priv_wq);
#endif
                {
                    u32 i;
                    // free tx/rx rings */
                    rtl8192_free_rx_ring(dev);
                    for (i = 0; i < MAX_TX_QUEUE_COUNT; i++) {
#ifdef RTL8192CE
                        if((i != 4) && (i != 5))
#endif
                        rtl8192_free_tx_ring(dev, i);
                    }
                }

		if(priv->irq){

			printk("Freeing irq %d\n",dev->irq);
			free_irq(dev->irq, dev);
			priv->irq=0;

		}
#ifdef CONFIG_RTL8192_IO_MAP
		if( dev->base_addr != 0 ){

			release_region(dev->base_addr,
				       pci_resource_len(pdev, 0) );
		}
#else
		if( dev->mem_start != (unsigned long)NULL ){
			iounmap( (void *)dev->mem_start );
#ifdef RTL8192CE
			release_mem_region( pci_resource_start(pdev, 2),
				    pci_resource_len(pdev, 2) );
#else
			release_mem_region( pci_resource_start(pdev, 1),
					    pci_resource_len(pdev, 1) );
#endif
		}
#endif /*end #ifdef RTL_IO_MAP*/
		free_rtllib(dev);

	} else{
		priv=rtllib_priv(dev);
        }

	pci_disable_device(pdev);
#ifdef RTL8192SE
        // Enable clk request, and set device to D3 mode */
        pci_write_config_byte(pdev, 0x81,1);
        pci_write_config_byte(pdev,0x44,3);
#endif
	RT_TRACE(COMP_DOWN, "wlan driver removed\n");
}

bool NicIFEnableNIC(struct net_device* dev)
{
	bool init_status = true;
	struct r8192_priv* priv = rtllib_priv(dev);
	PRT_POWER_SAVE_CONTROL pPSC = (PRT_POWER_SAVE_CONTROL)(&(priv->rtllib->PowerSaveControl));

	// <1> Reset memory: descriptor, buffer,..
	//NicIFResetMemory(Adapter);

	// <2> Enable Adapter
	printk("===========>%s()\n",__FUNCTION__);
	priv->bfirst_init = true;
	init_status = priv->ops->initialize_adapter(dev);
	if (init_status != true) {
		RT_TRACE(COMP_ERR,"ERR!!! %s(): initialization is failed!\n",__FUNCTION__);
		return -1;
	}
	RT_TRACE(COMP_INIT, "start adapter finished\n");
	RT_CLEAR_PS_LEVEL(pPSC, RT_RF_OFF_LEVL_HALT_NIC);
	priv->bfirst_init = false;

	// <3> Enable Interrupt
	rtl8192_irq_enable(dev);
	priv->bdisable_nic = false;
	return init_status;
}
bool NicIFDisableNIC(struct net_device* dev)
{
	bool	status = true;
	struct r8192_priv* priv = rtllib_priv(dev);
	u8 tmp_state = 0;
	// <1> Disable Interrupt
	printk("=========>%s()\n",__FUNCTION__);
	tmp_state = priv->rtllib->state;
#ifdef _RTL8192_EXT_PATCH_
	rtllib_softmac_stop_protocol(priv->rtllib,0,false);
#else
	rtllib_softmac_stop_protocol(priv->rtllib,false);
#endif
	priv->rtllib->state = tmp_state;
	rtl8192_cancel_deferred_work(priv);
	rtl8192_irq_disable(dev);

	// <2> Stop all timer

	// <3> Disable Adapter
	priv->ops->stop_adapter(dev, false);
	priv->bdisable_nic = true;

	return status;
}
#ifdef BUILT_IN_RTLLIB
/* fun with the built-in rtllib stack... */
extern int rtllib_init(void);
extern int rtllib_exit(void);
extern int rtllib_crypto_init(void);
extern void rtllib_crypto_deinit(void);
extern int rtllib_crypto_tkip_init(void);
extern void rtllib_crypto_tkip_exit(void);
extern int rtllib_crypto_ccmp_init(void);
extern void rtllib_crypto_ccmp_exit(void);
extern int rtllib_crypto_wep_init(void);
extern void rtllib_crypto_wep_exit(void);
#endif
#ifdef BUILT_IN_MSHCLASS
extern int msh_init(void);
extern int msh_exit(void);
#endif

static int __init rtl8192_pci_module_init(void)
{
	int ret;
	int error;

#ifdef BUILT_IN_CRYPTO
        ret = arc4_init();
        if (ret) {
                printk(KERN_ERR "arc4_init() failed %d\n", ret);
                return ret;
        }


        ret = michael_mic_init();
        if (ret) {
                printk(KERN_ERR "michael_mic_init() failed %d\n", ret);
                return ret;
        }

        ret = aes_init();
        if (ret) {
                printk(KERN_ERR "aes_init() failed %d\n", ret);
                return ret;
        }
#endif
#ifdef BUILT_IN_RTLLIB
	ret = rtllib_init();
	if (ret) {
		printk(KERN_ERR "rtllib_init() failed %d\n", ret);
		return ret;
	}
	ret = rtllib_crypto_init();
	if (ret) {
		printk(KERN_ERR "rtllib_crypto_init() failed %d\n", ret);
		return ret;
	}
	ret = rtllib_crypto_tkip_init();
	if (ret) {
		printk(KERN_ERR "rtllib_crypto_tkip_init() failed %d\n", ret);
		return ret;
	}
	ret = rtllib_crypto_ccmp_init();
	if (ret) {
		printk(KERN_ERR "rtllib_crypto_ccmp_init() failed %d\n", ret);
		return ret;
	}
	ret = rtllib_crypto_wep_init();
	if (ret) {
		printk(KERN_ERR "rtllib_crypto_wep_init() failed %d\n", ret);
		return ret;
	}
#endif
#ifdef BUILT_IN_MSHCLASS
	ret = msh_init();
	if (ret) {
		printk(KERN_ERR "msh_init() failed %d\n", ret);
		return ret;
	}
#endif
	printk(KERN_INFO "\nLinux kernel driver for RTL8192 based WLAN cards\n");
	printk(KERN_INFO "Copyright (c) 2007-2008, Realsil Wlan\n");
	RT_TRACE(COMP_INIT, "Initializing module");
	RT_TRACE(COMP_INIT, "Wireless extensions version %d", WIRELESS_EXT);

	error = rtl_create_debugfs_root();
	if (error) {
		RT_TRACE(COMP_DBG, "Create debugfs root fail: %d\n", error);
		goto err_out;
	}

#if(LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22))
      if(0!=pci_module_init(&rtl8192_pci_driver))
#else
      if(0!=pci_register_driver(&rtl8192_pci_driver))
#endif
	{
		DMESG("No device found");
		/*pci_unregister_driver (&rtl8192_pci_driver);*/
		return -ENODEV;
	}
	return 0;
err_out:
        return error;

}

static void __exit rtl8192_pci_module_exit(void)
{
	pci_unregister_driver(&rtl8192_pci_driver);

	RT_TRACE(COMP_DOWN, "Exiting");
	rtl_remove_debugfs_root();
#ifdef BUILT_IN_RTLLIB
	rtllib_crypto_tkip_exit();
	rtllib_crypto_ccmp_exit();
	rtllib_crypto_wep_exit();
	rtllib_crypto_deinit();
	rtllib_exit();
#endif
#ifdef BUILT_IN_CRYPTO
        arc4_exit();
        michael_mic_exit();
        aes_fini();
#endif
#ifdef BUILT_IN_MSHCLASS
	msh_exit();
#endif

}

u8 QueryIsShort(u8 TxHT, u8 TxRate, cb_desc *tcb_desc)
{
	u8   tmp_Short;

	tmp_Short = (TxHT==1)?((tcb_desc->bUseShortGI)?1:0):((tcb_desc->bUseShortPreamble)?1:0);
#if defined RTL8192SE || defined RTL8192CE
	if(TxHT==1 && TxRate != DESC92S_RATEMCS15)
#elif defined RTL8192E || defined RTL8190P
	if(TxHT==1 && TxRate != DESC90_RATEMCS15)
#endif
		tmp_Short = 0;

	return tmp_Short;
}

void check_rfctrl_gpio_timer(unsigned long data)
{
	struct r8192_priv* priv = rtllib_priv((struct net_device *)data);

	priv->polling_timer_on = 1;//add for S3/S4

	//if(priv->driver_upping == 0){
	queue_delayed_work_rsl(priv->priv_wq,&priv->gpio_change_rf_wq,0);
        //}

	mod_timer(&priv->gpio_polling_timer, jiffies + MSECS(RTLLIB_WATCH_DOG_TIME));
}

/***************************************************************************
     ------------------- module init / exit stubs ----------------
****************************************************************************/
module_init(rtl8192_pci_module_init);
module_exit(rtl8192_pci_module_exit);


MODULE_DESCRIPTION("Linux driver for Realtek RTL819x WiFi cards");
MODULE_AUTHOR(DRV_COPYRIGHT " " DRV_AUTHOR);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0))
MODULE_VERSION(DRV_VERSION);
#endif
MODULE_LICENSE("GPL");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9)
module_param(ifname, charp, S_IRUGO|S_IWUSR );
//module_param(hwseqnum,int, S_IRUGO|S_IWUSR);
module_param(hwwep,int, S_IRUGO|S_IWUSR);
module_param(channels,int, S_IRUGO|S_IWUSR);
#else
MODULE_PARM(ifname, "s");
//MODULE_PARM(hwseqnum,"i");
MODULE_PARM(hwwep,"i");
MODULE_PARM(channels,"i");
#endif

MODULE_PARM_DESC(ifname," Net interface name, wlan%d=default");
//MODULE_PARM_DESC(hwseqnum," Try to use hardware 802.11 header sequence numbers. Zero=default");
MODULE_PARM_DESC(hwwep," Try to use hardware WEP support(default use hw. set 0 to use software security)");
MODULE_PARM_DESC(channels," Channel bitmask for specific locales. NYI");