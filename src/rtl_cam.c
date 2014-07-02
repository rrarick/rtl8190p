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
#include <linux/interrupt.h>
#include "rtl_core.h"
#include "r819xE_phy.h"
#include "r819xE_phyreg.h"
#include "r8190_rtl8256.h" /* RTL8225 Radio frontend */
#include "r819xE_cmdpkt.h"

extern int hwwep;
void CamResetAllEntry(struct net_device *dev)
{
	//u8 ucIndex;
	u32 ulcommand = 0;

	ulcommand |= BIT31|BIT30;
	write_nic_dword(dev, RWCAM, ulcommand);
}

#ifdef _RTL8192_EXT_PATCH_
//YJ,add,090213, del Hw Security CAM entry
void CamDeleteOneEntry(struct net_device *dev, u8 EntryNo)
{
	u32 ulCommand = EntryNo * CAM_CONTENT_COUNT;
	u32 ulContent = 0;

	ulCommand = ulCommand | BIT31 | BIT16;

	write_nic_dword(dev,WCAMI,ulContent);
	write_nic_dword(dev,RWCAM,ulCommand);
}
//YJ,add,090213,end

//AMY add 090302 //restore each interface entry
void CamRestoreEachIFEntry(struct net_device* dev,u8 is_mesh)
{
	u32 i;
	struct r8192_priv *priv = rtllib_priv(dev);
	struct rtllib_device *ieee = priv->rtllib;
	for( i = 0 ; i<	TOTAL_CAM_ENTRY; i++) {

		if (is_mesh) {
#ifdef _RTL8192_EXT_PATCH_
			if(ieee->swmeshcamtable[i].bused )
			{
				setKey(dev,
						i,
						ieee->swmeshcamtable[i].key_index,
						ieee->swmeshcamtable[i].key_type,
						ieee->swmeshcamtable[i].macaddr,
						ieee->swmeshcamtable[i].useDK,
						(u32*)(&ieee->swmeshcamtable[i].key_buf[0])
				      );
			}
#endif
		} else {
			if(ieee->swcamtable[i].bused )
			{
				setKey(dev,
						i,
						ieee->swcamtable[i].key_index,
						ieee->swcamtable[i].key_type,
						ieee->swcamtable[i].macaddr,
						ieee->swcamtable[i].useDK,
						(u32*)(&ieee->swcamtable[i].key_buf[0]));
			}
		}
	}
}
//AMY add 090302 end
#endif

void write_cam(struct net_device *dev, u8 addr, u32 data)
{
	write_nic_dword(dev, WCAMI, data);
	write_nic_dword(dev, RWCAM, BIT31|BIT16|(addr&0xff) );
}

u32 read_cam(struct net_device *dev, u8 addr)
{
	write_nic_dword(dev, RWCAM, 0x80000000|(addr&0xff) );
	return read_nic_dword(dev, 0xa8);
}

void EnableHWSecurityConfig8192(struct net_device *dev)
{
	u8 SECR_value = 0x0;
	// struct rtllib_device* ieee1 = container_of(&dev, struct rtllib_device, dev);
	//printk("==>ieee1:%p, dev:%p\n", ieee1, dev);
	struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);
	struct rtllib_device* ieee = priv->rtllib;
	//printk("==>ieee:%p, dev:%p\n", ieee, dev);
	SECR_value = SCR_TxEncEnable | SCR_RxDecEnable;
#ifdef _RTL8192_EXT_PATCH_
	if ((((KEY_TYPE_WEP40 == ieee->pairwise_key_type) || (KEY_TYPE_WEP104 == ieee->pairwise_key_type)) && (priv->rtllib->auth_mode != 2))
			&&(ieee->iw_mode != IW_MODE_MESH))
#else
		if (((KEY_TYPE_WEP40 == ieee->pairwise_key_type) || (KEY_TYPE_WEP104 == ieee->pairwise_key_type)) && (priv->rtllib->auth_mode != 2))
#endif
		{
			SECR_value |= SCR_RxUseDK;
			SECR_value |= SCR_TxUseDK;
		}
		else if ((ieee->iw_mode == IW_MODE_ADHOC) && (ieee->pairwise_key_type & (KEY_TYPE_CCMP | KEY_TYPE_TKIP)))
		{
			SECR_value |= SCR_RxUseDK;
			SECR_value |= SCR_TxUseDK;
		}


	//add HWSec active enable here.
	//default using hwsec. when peer AP is in N mode only and pairwise_key_type is none_aes(which HT_IOT_ACT_PURE_N_MODE indicates it), use software security. when peer AP is in b,g,n mode mixed and pairwise_key_type is none_aes, use g mode hw security. WB on 2008.7.4
	ieee->hwsec_active = 1;
#ifdef _RTL8192_EXT_PATCH_
	if ((ieee->pHTInfo->IOTAction&HT_IOT_ACT_PURE_N_MODE) || !hwwep )//!ieee->hwsec_support) //add hwsec_support flag to totol control hw_sec on/off
	{
		ieee->hwsec_active = 0;
		SECR_value &= ~SCR_RxDecEnable;
		SECR_value &= ~SCR_TxUseDK;
		SECR_value &= ~SCR_RxUseDK;
		SECR_value &= ~SCR_TxEncEnable;
	}
#else
	if ((ieee->pHTInfo->IOTAction&HT_IOT_ACT_PURE_N_MODE) || !hwwep)//!ieee->hwsec_support) //add hwsec_support flag to totol control hw_sec on/off
	{
		ieee->hwsec_active = 0;
		SECR_value &= ~SCR_RxDecEnable;
	}
#endif
	RT_TRACE(COMP_SEC,"%s:, hwsec:%d, pairwise_key:%d, SECR_value:%x\n", __FUNCTION__, \
			ieee->hwsec_active, ieee->pairwise_key_type, SECR_value);
	{
		write_nic_byte(dev, SECR,  SECR_value);//SECR_value |  SCR_UseDK );
	}

}
//AMY added 090415
#ifdef _RTL8192_EXT_PATCH_
void set_swcam(struct net_device *dev,
		u8 EntryNo,
		u8 KeyIndex,
		u16 KeyType,
		u8 *MacAddr,
		u8 DefaultKey,
		u32 *KeyContent,
		u8 is_mesh)
#else
void set_swcam(struct net_device *dev,
		u8 EntryNo,
		u8 KeyIndex,
		u16 KeyType,
		u8 *MacAddr,
		u8 DefaultKey,
		u32 *KeyContent)
#endif
{
	struct r8192_priv *priv = rtllib_priv(dev);
	struct rtllib_device *ieee = priv->rtllib;
#ifdef _RTL8192_EXT_PATCH_
	printk("===========>%s():EntryNo is %d,KeyIndex is %d,KeyType is %d,is_mesh is %d\n",__FUNCTION__,EntryNo,KeyIndex,KeyType,is_mesh);
	if(is_mesh){
		ieee->swmeshcamtable[EntryNo].bused=true;
		ieee->swmeshcamtable[EntryNo].key_index=KeyIndex;
		ieee->swmeshcamtable[EntryNo].key_type=KeyType;
		memcpy(ieee->swmeshcamtable[EntryNo].macaddr,MacAddr,6);
		ieee->swmeshcamtable[EntryNo].useDK=DefaultKey;
		memcpy(ieee->swmeshcamtable[EntryNo].key_buf,(u8*)KeyContent,16);
	}
	else
#endif
	{
		ieee->swcamtable[EntryNo].bused=true;
		ieee->swcamtable[EntryNo].key_index=KeyIndex;
		ieee->swcamtable[EntryNo].key_type=KeyType;
		memcpy(ieee->swcamtable[EntryNo].macaddr,MacAddr,6);
		ieee->swcamtable[EntryNo].useDK=DefaultKey;
		memcpy(ieee->swcamtable[EntryNo].key_buf,(u8*)KeyContent,16);
	}
}
//AMY added 090415 end
#ifdef _RTL8192_EXT_PATCH_
//AMY added 090302
void reset_IFswcam(struct net_device *dev, u8 is_mesh)
{
	struct r8192_priv *priv = rtllib_priv(dev);
	struct rtllib_device *ieee = priv->rtllib;
	if(is_mesh){
#ifdef _RTL8192_EXT_PATCH_
		memset(ieee->swmeshcamtable,0,sizeof(SW_CAM_TABLE)*32);
#endif
	}
	else{
		memset(ieee->swcamtable,0,sizeof(SW_CAM_TABLE)*32);
	}
}
//AMY added 090302 end
#endif
void setKey(struct net_device *dev,
		u8 EntryNo,
		u8 KeyIndex,
		u16 KeyType,
		u8 *MacAddr,
		u8 DefaultKey,
		u32 *KeyContent )
{
	u32 TargetCommand = 0;
	u32 TargetContent = 0;
	u16 usConfig = 0;
	u8 i;
#ifdef ENABLE_IPS
	struct r8192_priv *priv = (struct r8192_priv *)rtllib_priv(dev);
	RT_RF_POWER_STATE	rtState;
	rtState = priv->rtllib->eRFPowerState;
	if(priv->rtllib->PowerSaveControl.bInactivePs){
		if(rtState == eRfOff){
			if(priv->rtllib->RfOffReason > RF_CHANGE_BY_IPS)
			{
				RT_TRACE(COMP_ERR, "%s(): RF is OFF.\n",__FUNCTION__);
				return ;
			}
			else{
				down(&priv->rtllib->ips_sem);
				IPSLeave(dev);
				up(&priv->rtllib->ips_sem);			}
		}
	}
	priv->rtllib->is_set_key = true;
#endif
	if (EntryNo >= TOTAL_CAM_ENTRY)
		RT_TRACE(COMP_ERR, "cam entry exceeds in setKey()\n");

	RT_TRACE(COMP_SEC, "====>to setKey(), dev:%p, EntryNo:%d, KeyIndex:%d, KeyType:%d, MacAddr"MAC_FMT"\n", dev,EntryNo, KeyIndex, KeyType, MAC_ARG(MacAddr));

	if (DefaultKey)
		usConfig |= BIT15 | (KeyType<<2);
	else
		usConfig |= BIT15 | (KeyType<<2) | KeyIndex;
	//	usConfig |= BIT15 | (KeyType<<2) | (DefaultKey<<5) | KeyIndex;


	for(i=0 ; i<CAM_CONTENT_COUNT; i++){
		TargetCommand  = i+CAM_CONTENT_COUNT*EntryNo;
		TargetCommand |= BIT31|BIT16;

		if(i==0){//MAC|Config
			TargetContent = (u32)(*(MacAddr+0)) << 16|
				(u32)(*(MacAddr+1)) << 24|
				(u32)usConfig;

			write_nic_dword(dev, WCAMI, TargetContent);
			write_nic_dword(dev, RWCAM, TargetCommand);
			//		printk("setkey cam =%8x\n", read_cam(dev, i+6*EntryNo));
		}
		else if(i==1){//MAC
			TargetContent = (u32)(*(MacAddr+2))	 |
				(u32)(*(MacAddr+3)) <<  8|
				(u32)(*(MacAddr+4)) << 16|
				(u32)(*(MacAddr+5)) << 24;
			write_nic_dword(dev, WCAMI, TargetContent);
			write_nic_dword(dev, RWCAM, TargetCommand);
		}
		else {	//Key Material
			if(KeyContent != NULL)
			{
				write_nic_dword(dev, WCAMI, (u32)(*(KeyContent+i-2)) );
				write_nic_dword(dev, RWCAM, TargetCommand);
			}
		}
	}
	RT_TRACE(COMP_SEC,"=========>after set key, usconfig:%x\n", usConfig);
	//	CAM_read_entry(dev, 0);
}
// This function seems not ready! WB
#if 0
void CamPrintDbgReg(struct net_device* dev)
{
	unsigned long rvalue;
	unsigned char ucValue;
	write_nic_dword(dev, DCAM, 0x80000000);
	msleep(40);
	rvalue = read_nic_dword(dev, DCAM);	//delay_ms(40);
	RT_TRACE(COMP_SEC, " TX CAM=%8lX ",rvalue);
	if((rvalue & 0x40000000) != 0x4000000)
		RT_TRACE(COMP_SEC, "-->TX Key Not Found      ");
	msleep(20);
	write_nic_dword(dev, DCAM, 0x00000000);	//delay_ms(40);
	rvalue = read_nic_dword(dev, DCAM);	//delay_ms(40);
	RT_TRACE(COMP_SEC, "RX CAM=%8lX ",rvalue);
	if((rvalue & 0x40000000) != 0x4000000)
		RT_TRACE(COMP_SEC, "-->CAM Key Not Found   ");
	ucValue = read_nic_byte(dev, SECR);
	RT_TRACE(COMP_SEC, "WPA_Config=%x \n",ucValue);
}
#endif
void CAM_read_entry(struct net_device *dev, u32 iIndex)
{
	u32 target_command=0;
	u32 target_content=0;
	u8 entry_i=0;
	u32 ulStatus;
	s32 i=100;
	//	printk("=======>start read CAM\n");
	for(entry_i=0;entry_i<CAM_CONTENT_COUNT;entry_i++)
	{
		// polling bit, and No Write enable, and address
		target_command= entry_i+CAM_CONTENT_COUNT*iIndex;
		target_command= target_command | BIT31;

		//Check polling bit is clear
		//	mdelay(1);
#if 1
		while((i--)>=0)
		{
			ulStatus = read_nic_dword(dev, RWCAM);
			if(ulStatus & BIT31){
				continue;
			}
			else{
				break;
			}
		}
#endif
		write_nic_dword(dev, RWCAM, target_command);
		RT_TRACE(COMP_SEC,"CAM_read_entry(): WRITE A0: %x \n",target_command);
		//	printk("CAM_read_entry(): WRITE A0: %lx \n",target_command);
		target_content = read_nic_dword(dev, RCAMO);
		RT_TRACE(COMP_SEC, "CAM_read_entry(): WRITE A8: %x \n",target_content);
		//	printk("CAM_read_entry(): WRITE A8: %lx \n",target_content);
	}
	printk("\n");
}

void CamRestoreAllEntry(	struct net_device *dev)
{
	u8 EntryId = 0;
	struct r8192_priv *priv = rtllib_priv(dev);
	u8*	MacAddr = priv->rtllib->current_network.bssid;

	static u8	CAM_CONST_ADDR[4][6] = {
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x01},
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x02},
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x03}};
	static u8	CAM_CONST_BROAD[] =
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	RT_TRACE(COMP_SEC, "CamRestoreAllEntry: \n");


	if ((priv->rtllib->pairwise_key_type == KEY_TYPE_WEP40)||
			(priv->rtllib->pairwise_key_type == KEY_TYPE_WEP104))
	{

		for(EntryId=0; EntryId<4; EntryId++)
		{
			{
				MacAddr = CAM_CONST_ADDR[EntryId];
				if(priv->rtllib->swcamtable[EntryId].bused )
				{
					setKey(dev,
							EntryId ,
							EntryId,
							priv->rtllib->pairwise_key_type,
							MacAddr,
							0,
							(u32*)(&priv->rtllib->swcamtable[EntryId].key_buf[0])
					      );
				}
			}
		}

	}
	else if(priv->rtllib->pairwise_key_type == KEY_TYPE_TKIP)
	{

		{
			if(priv->rtllib->iw_mode == IW_MODE_ADHOC)
			{
				setKey(dev,
						4,
						0,
						priv->rtllib->pairwise_key_type,
						(u8*)dev->dev_addr,
						0,
						(u32*)(&priv->rtllib->swcamtable[4].key_buf[0])
				      );
			}
			else
			{
				setKey(dev,
						4,
						0,
						priv->rtllib->pairwise_key_type,
						MacAddr,
						0,
						(u32*)(&priv->rtllib->swcamtable[4].key_buf[0])
				      );
			}

		}
	}
	else if(priv->rtllib->pairwise_key_type == KEY_TYPE_CCMP)
	{

		{
			if(priv->rtllib->iw_mode == IW_MODE_ADHOC)
			{
				setKey(dev,
						4,
						0,
						priv->rtllib->pairwise_key_type,
						(u8*)dev->dev_addr,
						0,
						(u32*)(&priv->rtllib->swcamtable[4].key_buf[0])
				      );
			}
			else
			{
				setKey(dev,
						4,
						0,
						priv->rtllib->pairwise_key_type,
						MacAddr,
						0,
						(u32*)(&priv->rtllib->swcamtable[4].key_buf[0])
				      );
			}
		}
	}



	if(priv->rtllib->group_key_type == KEY_TYPE_TKIP)
	{
		MacAddr = CAM_CONST_BROAD;
		for(EntryId=1 ; EntryId<4 ; EntryId++)
		{
			if(priv->rtllib->swcamtable[EntryId].bused )
			{
				setKey(dev,
						EntryId,
						EntryId,
						priv->rtllib->group_key_type,
						MacAddr,
						0,
						(u32*)(&priv->rtllib->swcamtable[EntryId].key_buf[0])
				      );
			}
		}
		if(priv->rtllib->iw_mode == IW_MODE_ADHOC)
		{
			if(priv->rtllib->swcamtable[0].bused ){
				setKey(dev,
						0,
						0,
						priv->rtllib->group_key_type,
						CAM_CONST_ADDR[0],
						0,
						(u32*)(&priv->rtllib->swcamtable[0].key_buf[0])
				      );
			}
			else
			{
				RT_TRACE(COMP_ERR,"===>%s():ERR!! ADHOC TKIP ,but 0 entry is have no data\n",__FUNCTION__);
				return;
			}
		}
	} else if(priv->rtllib->group_key_type == KEY_TYPE_CCMP) {
		MacAddr = CAM_CONST_BROAD;
		for(EntryId=1; EntryId<4 ; EntryId++)
		{
			if(priv->rtllib->swcamtable[EntryId].bused )
			{
				setKey(dev,
				       EntryId ,
				       EntryId,
				       priv->rtllib->group_key_type,
				       MacAddr,
				       0,
				       (u32*)(&priv->rtllib->swcamtable[EntryId].key_buf[0]));
			}
		}

		if (priv->rtllib->iw_mode == IW_MODE_ADHOC) {
			if (priv->rtllib->swcamtable[0].bused) {
				setKey(dev,
					0 ,
					0,
					priv->rtllib->group_key_type,
					CAM_CONST_ADDR[0],
					0,
					(u32*)(&priv->rtllib->swcamtable[0].key_buf[0]));
			} else {
				RT_TRACE(COMP_ERR,"===>%s():ERR!! ADHOC CCMP ,but 0 entry is have no data\n",
						__FUNCTION__);
				return;
			}
		}
	}
}


//YJ,add,for MESH Hardware Encryption,090213
#ifdef _RTL8192_EXT_PATCH_
u8 rtl8192_get_free_hwsec_cam_entry(struct rtllib_device *ieee, u8 *sta_addr)
{
	u32 bitmap = (ieee->HwSecCamBitMap)>>4;
	u8 entry_idx = 0;
	u8 i, *addr;

	if ((NULL == ieee) || (NULL == sta_addr)) {
		printk("%s: ieee or sta_addr is NULL.\n", __FUNCTION__);
		return TOTAL_CAM_ENTRY;
	}

	/* Does STA already exist? */
	/* CAM Index 31 is for AP */
	for (i = 4; i < TOTAL_CAM_ENTRY-1; i++) {
		addr = ieee->HwSecCamStaAddr[i];
		if(memcmp(addr, sta_addr, ETH_ALEN) == 0)
			return i;
	}

	/* Get a free CAM entry. */
	for (entry_idx = 4; entry_idx < TOTAL_CAM_ENTRY - 1; entry_idx++) {
		if ((bitmap & BIT0) == 0) {
			printk("-----HwSecCamBitMap: 0x%x entry_idx=%d\n",ieee->HwSecCamBitMap, entry_idx);
			ieee->HwSecCamBitMap |= BIT0<<entry_idx;
			memcpy(ieee->HwSecCamStaAddr[entry_idx], sta_addr, ETH_ALEN);
			return entry_idx;
		}
		bitmap = bitmap >>1;
	}

	return TOTAL_CAM_ENTRY;
}

void rtl8192_del_hwsec_cam_entry(struct rtllib_device *ieee, u8 *sta_addr)
{
	u32 bitmap;
	u8 i, *addr;

	if ((NULL == ieee) || (NULL == sta_addr)) {
		printk("%s: ieee or sta_addr is NULL.\n", __FUNCTION__);
		return;
	}

	if ((sta_addr[0]|sta_addr[1]|sta_addr[2]|sta_addr[3]|\
	     sta_addr[4]|sta_addr[5]) == 0) {
		printk("%s: sta_addr is 00:00:00:00:00:00.\n", __FUNCTION__);
		return;
	}

	/* Does STA already exist? */
	for (i = 4; i < TOTAL_CAM_ENTRY; i++) {
		addr = ieee->HwSecCamStaAddr[i];
		bitmap = (ieee->HwSecCamBitMap)>>i;
		if (((bitmap & BIT0) == BIT0) && (memcmp(addr, sta_addr, ETH_ALEN) == 0)) {
			/* Remove from HW Security CAM */
			CamDeleteOneEntry(ieee->dev, i);
			memset(ieee->HwSecCamStaAddr[i], 0, ETH_ALEN);
			ieee->HwSecCamBitMap &= ~(BIT0<<i);
			//AMY added to del sw cam 090304
			memset(&(ieee->swmeshcamtable[i]), 0, sizeof(SW_CAM_TABLE));
			RT_TRACE(COMP_SEC, "====>del sw entry, EntryNo:%d, MacAddr:"MAC_FMT"\n",
					i, MAC_ARG(sta_addr));
		}
	}
	return;
}
#endif
//YJ,add,for MESH Hardware Encryption,090213,end
