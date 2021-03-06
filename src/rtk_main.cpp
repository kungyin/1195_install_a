#include <sys/ioctl.h> //for andy mtd speed up 20120516
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <rtk_main.h>
#include <rtk_tar.h>
#include <rtk_burn.h>   // burn bootcode & patition
#include <rtk_fwdesc.h> // burn fwtab
#include <rtk_imgdesc.h>
#include <rtk_config.h>
#include <rtk_mtd.h>
#include <rtk_boottable.h>
#include <rtk_tagflow.h>
#include <rtk_def.h>
#include <rtk_customer.h>
#include <rtk_factory.h>
#include <rtk_efuse.h>


#if defined(__OFFLINE_GENERATE_BIN__) || defined(PC_SIMULATE)
#include "rtk_programmer.h"

char gsettingpath[128] = {0};
#endif

extern u32 gDebugPrintfLogLevel;

// burn all partition
static int rtk_burn_part(struct t_rtkimgdesc* prtkimgdesc)
{
   enum FWTYPE efwtype;
   int ret;
   char msg[256];
   for(efwtype = FWTYPE(FW_VIDEO_BOOTFILE+1); efwtype < FW_BOOTCODE;efwtype = FWTYPE(efwtype+1))
   {
      // check if pass
      if(prtkimgdesc->fw[efwtype].pass[prtkimgdesc->mode] == PARTIAL_PASS)
      {
         install_info("PASS Burn %s\r\n", inv_by_fwtype(efwtype));
         continue;
      }

      ret = rtk_burn_single_part(prtkimgdesc, efwtype);
      // TODO: SA here
      // when burn fail, continue or stop?
      if(ret < 0)
      {
         install_debug("Burn %s fail\r\n", inv_by_fwtype(efwtype));

         // ROOTFS/ETC must stop when burn fail
         if(efwtype == FW_ROOTFS||efwtype == FW_USR_LOCAL_ETC)
         {
            sprintf(msg, "Burn %s fail", inv_by_fwtype(efwtype));
            return -1;
         }
      }
   }
   return 0;
}

// burn all fw
static int rtk_burn_fw(struct t_rtkimgdesc* prtkimgdesc)
{
   enum FWTYPE efwtype;
   int ret;
   char msg[256];
   for(efwtype = FW_KERNEL; efwtype <= FW_VIDEO_BOOTFILE;efwtype = FWTYPE(efwtype+1))
   {
      // check if pass
      if(prtkimgdesc->fw[efwtype].pass[prtkimgdesc->mode] == PARTIAL_PASS)
      {
         install_info("PASS Burn %s\r\n", inv_by_fwtype(efwtype));
         continue;
      }
      ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[efwtype]);
      // TODO: SA here
      // when burn fail, continue or stop?
      if(ret < 0)
      {
         install_debug("Burn %s fail\r\n", inv_by_fwtype(efwtype));
         if((efwtype == FW_KERNEL) || \
            (efwtype == FW_AUDIO) || \
            (efwtype == FW_VIDEO))
         {
            sprintf(msg, "Burn %s fail", inv_by_fwtype(efwtype));
            return -1;
         }
      }
   }
   return 0;
}

// burn all fw and all part
static int rtk_burn_rtkimg_seq(struct t_rtkimgdesc* prtkimgdesc)
{
   if(rtk_burn_fw(prtkimgdesc) < 0)
   {
      install_debug("burn Partition fail\r\n");
      return -1;
   }

   if(rtk_burn_part(prtkimgdesc) < 0)
   {
      install_debug("burn Partition fail\r\n");
      return -1;
   }
   return 0;
}
#ifdef BURN_BOOTCODE
// burn bootcode
static int rtk_burn_bootcode_reboot(struct t_rtkimgdesc* prtkimgdesc)
{
   int ret;
   ret = rtk_burn_bootcode(prtkimgdesc);
   if(ret == 1)
   {
      install_log("In NOR FLASH, so we only install bootcode and then reboot\r\n");
      return 1;
   }
   else if(ret == 0)
   {
      install_log("In NAND FLASH\r\n");
   }
   else if(ret == -1)
   {
      install_log("Bootcode none\r\n");
   }
   return 0;
}
#endif

// OLD NETWORK UPGRADE
int rtk_burn_rtkimg_from_local_flash(struct t_rtkimgdesc* prtkimgdesc)
{
   int ret;

   // initial rtkimgdesc's meminfo
   ret = fill_rtkimgdesc_meminfo(prtkimgdesc);
   if(ret < 0)
   {
      install_debug("fill_rtkimgdesc_meminfo fail\r\n");
      return -1;
   }

   // get backup area address
   //ret = findNandReservedSpaceStartEndAddress(&startAddress, &endAddress);
   ret = findNandBackupAreaStartEndAddress(&prtkimgdesc->startAddress, &prtkimgdesc->endAddress, prtkimgdesc->reserved_boot_size);
   if(ret < 0)
   {
      install_debug("findNandReservedSpaceStartEndAddress fail\r\n");
      return -1;
   }

   // parsing flashtar
   prtkimgdesc->startAddress = prtkimgdesc->startAddress + prtkimgdesc->mtd_erasesize;
   install_log("startAddress:0x%08x\r\n", prtkimgdesc->startAddress);
   ret = parse_flashtar(prtkimgdesc->startAddress, prtkimgdesc->endAddress, prtkimgdesc);
   if(ret < 0)
   {
      install_debug("parse_tar fail\r\n");
      return -1;
   }
   fill_rtk_part_list_by_config(prtkimgdesc);
   ret = parse_flashtar(prtkimgdesc->startAddress, prtkimgdesc->endAddress, prtkimgdesc);
   if(ret < 0)
   {
      install_debug("parse_tar fail\r\n");
      return -1;
   }

   // fill rtkimgdesc's offset
   ret = fill_rtkimgdesc_layout(prtkimgdesc);
   if(ret < 0)
   {
      install_debug("fill_rtkimgdesc_layout fail\r\n");
      return -1;
   }

#ifdef RTK_GUI
   if(prtkimgdesc->egui == GUI_ENABLE)
   {
      // UI start
      // extract video and font
      if(rtk_extract_file(prtkimgdesc, &prtkimgdesc->video_firmware, "/video_firmware.install.bin") < 0 || \
      rtk_extract_file(prtkimgdesc, &prtkimgdesc->arial, "/arial.ttf") < 0)
      {
         install_debug("Can't extract file about UI\r\n");
         return -1;
      }

      // start ui
       ret = rtk_start_ui(get_version(prtkimgdesc));
      if(ret < 0)
      {
         install_debug("rtk_start_ui fail\r\n");
         return -1;
      }
   // UI
   }
#endif
#ifdef BURN_BOOTCODE
   // optional: burn bootfw
   rtk_burn_bootcode_reboot(prtkimgdesc);
#endif
   // modify the signature (first 8 bytes of boot table)
   ret = modify_addr_signature(prtkimgdesc->startAddress, prtkimgdesc->reserved_boot_size);
   if(ret < 0)
   {
      install_debug("modify addr signature fail\r\n");
      return -1;
   }

   ret = rtk_burn_rtkimg_seq(prtkimgdesc);
   if(ret < 0)
   {
      install_debug("rtk_burn_rtkimg_seq fail\r\n");
      return -1;
   }

   if(rtk_burn_fwdesc(prtkimgdesc) < 0)
   {
      install_debug("burn boottable fail\r\n");
      return -1;
   }

#ifdef RTK_GUI
   if(prtkimgdesc->egui == GUI_ENABLE)
   {
      rtk_finish_ui();
   }
#endif

   return 0;
}

// NEW NETWORK UPGRADE
int rtk_burn_rtkimg_from_urltar_by_downloading_tarfile(struct t_rtkimgdesc* prtkimgdesc)
{
   int ret;
#ifdef VERIFY_IMAGE
   bool bret;
#endif
   //char command[128] = {0};

   // sanity-check
   if((prtkimgdesc->url.url==NULL)||(strlen(prtkimgdesc->url.url)==0))
   {
      install_debug("prtkimgdesc->url.url is NULL\r\n");
      return -1;
   }

   // initial rtkimgdesc's meminfo
   ret = fill_rtkimgdesc_meminfo(prtkimgdesc);
   if(ret < 0)
   {
      install_debug("fill_rtkimgdesc_meminfo fail\r\n");
      return -1;
   }

   rtk_command("umount /tmp/usbmounts/sda1", __LINE__, __FILE__);

   // wget install.img
#if 0
   sprintf(command, "wget %s -O %s", prtkimgdesc->url.url, "/tmp/install.img");
   ret = rtk_command(command, __LINE__, __FILE__);
#else
   ret = rtk_urlwget(prtkimgdesc->url.url, "/tmp/install.img");
#endif

   if(ret < 0)
   {
      install_debug("Can't urlwget %s\r\n", prtkimgdesc->url.url);
      return -1;
   }
#ifdef VERIFY_IMAGE
   // verify install.img
   // not implemented
   bret = rtk_verify_wrapper("/tmp/install.img");
   if(bret == false)
   {
      install_debug("rtk_verify_wrapper fail\r\n");
      return -1;
   }
#endif

   // parsing tarfile
   sprintf(prtkimgdesc->tarinfo.tarfile_path, "%s", "/tmp/install.img");
   ret = parse_tar(prtkimgdesc);
   if(ret < 0)
   {
      install_debug("parse_tar fail\r\n");
      return -1;
   }
   fill_rtk_part_list_by_config(prtkimgdesc);
   ret = parse_tar(prtkimgdesc);
   if(ret < 0)
   {
      install_debug("parse_tar fail\r\n");
      return -1;
   }

   // fill rtkimgdesc's offset
   ret = fill_rtkimgdesc_layout(prtkimgdesc);
   if(ret < 0)
   {
      install_debug("fill_rtkimgdesc_layout fail\r\n");
      return -1;
   }

#ifdef BURN_BOOTCODE
   // optional: burn bootfw
   rtk_burn_bootcode_reboot(prtkimgdesc);
#endif
   // modify the signature (first 8 bytes of boot table)
   ret = modify_signature(prtkimgdesc->reserved_boot_size, prtkimgdesc->flash_type);
   if(ret < 0)
   {
      install_debug("modify signature fail\r\n");
      return -1;
   }

   ret = rtk_burn_rtkimg_seq(prtkimgdesc);
   if(ret < 0)
   {
      install_debug("rtk_burn_rtkimg_seq fail\r\n");
      return -1;
   }

   if(rtk_burn_fwdesc(prtkimgdesc) < 0)
   {
      install_debug("burn boottable fail\r\n");
      return -1;
   }

   return 0;
}


// NOT IMPLEMENTED
int rtk_burn_rtkimg_from_urltar(struct t_rtkimgdesc* prtkimgdesc)
{
   int ret;

   install_debug("NOT IMPLEMENTED\r\n");
   return -1;

   // initial rtkimgdesc's meminfo
   if((ret = fill_rtkimgdesc_meminfo(prtkimgdesc)) < 0)
   {
      install_debug("fill_rtkimgdesc_meminfo(%d) fail\r\n", ret);
      return ret;
   }

   // parsing the url
   if((ret = urlparse(prtkimgdesc->url.url, &prtkimgdesc->url)) < 0)
   {
      install_debug("urlparse(%d) fail\r\n", ret);
      return ret;
   }

   // parsing urltar
   if((ret = parse_urltar(prtkimgdesc)) < 0)
   {
      install_debug("parse_urltar(%d) fail\r\n", ret);
      return ret;
   }
   // rtk_wget video or from local

   // fill rtkimgdesc's offset
   if((ret = fill_rtkimgdesc_layout(prtkimgdesc)) < 0)
   {
      install_debug("fill_rtkimgdesc_layout(%d) fail\r\n", ret);
      return ret;
   }

   // modify the signature (first 8 bytes of boot table)
   if((ret = modify_signature(prtkimgdesc->reserved_boot_size, prtkimgdesc->flash_type)) < 0)
   {
      install_debug("modify_signature(%d) fail\r\n", ret);
      return ret;
   }


   // burn firmware
   prtkimgdesc->tarinfo.fd = -1;
   if((rtk_burn_img_from_net(prtkimgdesc, &prtkimgdesc->fw[FW_KERNEL]) < 0) || \
   (rtk_burn_img_from_net(prtkimgdesc, &prtkimgdesc->fw[FW_AUDIO]) < 0) || \
   (rtk_burn_img_from_net(prtkimgdesc, &prtkimgdesc->fw[FW_VIDEO]) < 0) || \
   (rtk_burn_img_from_net(prtkimgdesc, &prtkimgdesc->fw[FW_AUDIO_BOOTFILE]) < 0) || \
   (rtk_burn_img_from_net(prtkimgdesc, &prtkimgdesc->fw[FW_VIDEO_BOOTFILE]) < 0) || \
   (rtk_burn_img_from_net(prtkimgdesc, &prtkimgdesc->fw[FW_ROOTFS]) < 0) || \
   (rtk_burn_etcimg(prtkimgdesc, &prtkimgdesc->fw[FW_USR_LOCAL_ETC]) < 0))
   {
      install_debug("burn fail\r\n");

      return -1;
   }

   if(rtk_burn_fwdesc(prtkimgdesc) < 0)
   {
      install_debug("burn boottable fail\r\n");
      return -1;
   }

   return 0;
}

typedef void* (*fun_type)(void*);

#ifdef TV005_USB_AUTO_SCRIPT
extern struct tv005_usb_auto_script g_script;
extern int rtk_install_apq(struct t_rtkimgdesc* prtkimgdesc, struct TV005_USB_UPDATE_FLAG* pupdateflag,int apqflag);
extern int rtk_install_tsb(struct t_rtkimgdesc* prtkimgdesc, struct TV005_USB_UPDATE_FLAG* pupdateflag,int tsbflag);
extern long filesize(FILE *stream);


int rtk_burn_rtkimg_from_usb_tsb(struct t_rtkimgdesc* prtkimgdesc)
{
   int ret = 0, err_code = -1, i;
   S_BOOTTABLE boottable, *pbt = NULL;
   TV005_USB_UPDATE_FLAG updateflags;
   E_TAG current_tag;
   char msg[128] = {0};
   char cmd[128] = {0}, path[128] = {0};
   char workpath[128] = {0};
   FILE* fd;
   // parsing tarfile
   install_debug("prtkimgdesc->tarinfo.tarfile_path:%s(0x%08x)\r\n", prtkimgdesc->tarinfo.tarfile_path, prtkimgdesc->tarinfo.tarfile_path);
   if((ret = parse_tar(prtkimgdesc)) < 0) {
      install_debug("parse_tar(%d) fail\r\n", ret);
      err_code = _ePARSE_TAR_FAIL;
      goto burn_end_usb_tsb;
   }

   // load configuration
   if((ret = fill_rtk_part_list_by_config(prtkimgdesc)) < 0) {
      install_debug("fill_rtk_part_list_by_config(%d) fail\r\n", ret);
      err_code = _eFILL_RTK_PART_LIST_BY_CONFIG_FAIL;
      goto burn_end_usb_tsb;
   }

   // customer
   if(prtkimgdesc->start_customer == 1)
      rtk_customer_init(prtkimgdesc);

   // initial rtkimgdesc's meminfo
   if((ret = fill_rtkimgdesc_meminfo(prtkimgdesc)) < 0) {
      install_debug("fill_rtkimgdesc_meminfo(%d) fail\r\n", ret);
      if (ret == -_eRTK_GENERAL_FAIL)
         err_code = _eFILL_RTKIMGDESC_MEMINFO_FAIL;
      else
         err_code = -ret;
      goto burn_end_usb_tsb;
   }

   //parse tar file
   if((ret = parse_tar(prtkimgdesc)) < 0) {
      install_debug("parse_tar fail\r\n");
      err_code = _ePARSE_TAR_FAIL;
      goto burn_end_usb_tsb;
   }

   //dump_flash
   if(prtkimgdesc->dump_flash == 1) {
#ifdef EMMC_SUPPORT
      if(prtkimgdesc->flash_type == MTD_NANDFLASH || prtkimgdesc->flash_type == MTD_EMMC)
#else
      if(prtkimgdesc->flash_type == MTD_NANDFLASH)
#endif
      {
         install_log("NAND FLASH does not support dump flash option\r\n");
         err_code = _eRTK_GENERAL_FAIL;
      }
      else
      {
         ret = rtk_dump_flash(prtkimgdesc);
         if (ret < 0)
            err_code = _eRTK_GENERAL_FAIL;
      }
      goto burn_end_usb_tsb;
   }

   // fill rtkimgdesc's offset
   if((ret = fill_rtkimgdesc_layout(prtkimgdesc)) < 0) {
      install_debug("fill_rtkimgdesc_layout(%d) fail\r\n", ret);
      if (ret == -_eRTK_GENERAL_FAIL)
         err_code = _eFILL_RTK_IMGDESC_LAYOUT_FAIL;
      else
         err_code = -ret;
      goto burn_end_usb_tsb;
   }

   // unlock flash
#ifdef EMMC_SUPPORT
   if(prtkimgdesc->flash_type == MTD_NANDFLASH || prtkimgdesc->flash_type == MTD_EMMC) {
#else
   if(prtkimgdesc->flash_type == MTD_NANDFLASH) {
#endif
      /* do nothing */
   }
   else {
      install_log("unlock flash\r\n");
      rtk_unlock_mtd(prtkimgdesc, 0, prtkimgdesc->flash_size);
   }

   if (prtkimgdesc->erase_free_space == 1) {
      install_log("erase free space flash offset = 0x%x, length = 0x%x.\r\n"
                  , prtkimgdesc->fw[FW_P_FREE_SPACE].flash_offset
                  , prtkimgdesc->fw[FW_P_FREE_SPACE].flash_allo_size);
      rtk_erase(prtkimgdesc, prtkimgdesc->fw[FW_P_FREE_SPACE].flash_offset, prtkimgdesc->fw[FW_P_FREE_SPACE].flash_allo_size);
   }

   if(prtkimgdesc->fake_burn == 1) {
      install_log("FAKE_BURN");
      return -1;
   }

   memset(&boottable, 0, sizeof(boottable));
   read_boottable(&boottable, prtkimgdesc);
   boottable.boottype = BOOTTYPE_UNKNOWN_BOOTTYPE;
   boottable.tag = TAG_UNKNOWN;
   sprintf(boottable.date, "%s", __DATE__);
   sprintf(boottable.time, "%s", __TIME__);
   install_log("\r\n\r\nWrite boottable\r\n");
   write_boottable(&boottable, prtkimgdesc->factory_start, prtkimgdesc->factory_size, prtkimgdesc);

   install_info("\r\n\r\n");

   if(prtkimgdesc->flash_type == MTD_EMMC) {
      pbt = read_boottable_emmc(&boottable, prtkimgdesc);

      // MBR is exactly the same and skip re-partition.
      if (!pbt->mbr_matched) {
         write_boottable_emmc(&boottable, prtkimgdesc->factory_start, prtkimgdesc->factory_size, prtkimgdesc);
      }
      // mount and format emmc partition
      ret = rtk_mount_format_emmc(&boottable, prtkimgdesc);
      if (ret < 0) {
         err_code = _eRTK_GENERAL_FAIL;
         goto before_burn_end_tsb;
      }
   }
   else
      return -1;

    //load factory from flash
   if ((ret = factory_load(NULL, prtkimgdesc)) < 0) {
      install_debug("factory_load fail\r\n");
   }
   if (!get_factory_tmp_dir()){
      install_debug("can't get factory path\r\n");
      return -1;
   }
   getcwd(workpath, 128);
   chdir(get_factory_tmp_dir());
    install_info("\r\n[enter factory_load@@@@@@@@@@@]\r\n");

   //read updateflags file and init
   memset(&updateflags,0,sizeof(updateflags));
   snprintf(path, sizeof(path), "%s/%s", get_factory_tmp_dir(), USB_UPDATE_FLAG_FILENAME);
   install_debug("\r\n[@@@@@@@@@@@ updateflags.dat path=%s]\r\n",path);

   if (access(path, F_OK)) {
      install_debug("updateflags.dat not found in factory\r\n");
      //return 0;
   }
   else
   {
     	   install_debug("updateflags.dat found in factory\r\n");
	   fd = fopen(path,"r");
	   if(fd!=NULL)
		{
			long size = filesize(fd);
			char* filebuf = (char *)malloc(size+1);
			fread(filebuf, size, 1, fd);
			fclose(fd);
			memcpy(&updateflags,(TV005_USB_UPDATE_FLAG *)filebuf,sizeof(TV005_USB_UPDATE_FLAG));
		}
    }
   //update items
   if (g_script.installimg_flag == 1)
   {
      //install bootcode
      if (prtkimgdesc->bootcode == 1) {
         if (prtkimgdesc->start_customer == 1) {
            rtk_customer_write_burn_partname(prtkimgdesc, TAG_BOOTCODE);
            rtk_customer_write_increase_progressbar(prtkimgdesc, 10);
         }
	 boottable.tag = TAG_BOOTCODE;
         ret = rtk_burn_bootcode_emmc(prtkimgdesc);
         if (ret < 0) {
            err_code = _eRTK_BURN_BOOTCODE_FAIL;
            goto before_burn_end_tsb;
         }
      }

      boottable.tag = TAG_UPDATE_ETC;

      do
      {
         current_tag = boottable.tag;

         if(prtkimgdesc->start_customer == 1)
            rtk_customer_write_increase_progressbar(prtkimgdesc, 11);

         ret = tagflow3_emmc(&boottable, prtkimgdesc, pbt);

         if(ret < 0) {
            err_code = -ret;
            goto before_burn_end_tsb;
            break;
         }
      } while(boottable.tag != TAG_COMPLETE);

      if(rtk_burn_fwdesc(prtkimgdesc) < 0) {
         install_debug("burn boottable fail\r\n");
         goto before_burn_end_tsb;
      }

      //install factory
      if((prtkimgdesc->factory == 1) || (prtkimgdesc->kill_000 == 1)) {
         if(prtkimgdesc->start_customer == 1) {
            rtk_customer_write_burn_partname(prtkimgdesc, TAG_FACTORY);
            rtk_customer_write_increase_progressbar(prtkimgdesc, 10);
         }
	 boottable.tag = TAG_FACTORY;
         ret = rtk_install_factory(prtkimgdesc);
         install_debug("rtk_install_factory(%d)\r\n", ret);
         if (ret < 0)
         {
            err_code = _eRTK_BURN_FACTORY_FAIL;
            goto before_burn_end_tsb;
         }
      }
   } // g_script.installimg_flag == 1
   /////////////aq/pq   update/////////////
   install_info("\r\n[g_script.aq_flag=%d]\r\n",g_script.aq_flag);
   if (g_script.aq_flag >0)
   {
      if(prtkimgdesc->start_customer == 1) {
         rtk_customer_write_burn_partname(prtkimgdesc, TAG_AQ);
         rtk_customer_write_increase_progressbar(prtkimgdesc, 10);
      }
      boottable.tag = TAG_AQ;
	  if(g_script.aq_flag == 1)
	  {
	      ret = rtk_install_apq(prtkimgdesc,&updateflags, 0);
	      if (ret < 0)
	      {
	         err_code = _eRTK_BURN_FACTORY_FAIL;
	         install_debug("AQ:rtk_install_apq(%d) fail\r\n", ret);
	         goto before_burn_end_tsb;
	      }
	  }
	  else if(g_script.aq_flag == 2)
	  {
	  		updateflags.aq_flag=1;
	  }
	  else
	  {
		  	updateflags.aq_flag=0;
			updateflags.aq_first_update=0;
	  }

   }

   install_info("\r\n[g_script.pq_flag=%d]\r\n",g_script.pq_flag);
   if (g_script.pq_flag >0)
   {
      if(prtkimgdesc->start_customer == 1) {
         rtk_customer_write_burn_partname(prtkimgdesc, TAG_PQ);
         rtk_customer_write_increase_progressbar(prtkimgdesc, 10);
      }
      boottable.tag = TAG_PQ;
	  if(g_script.pq_flag == 1)
	  {
	      ret = rtk_install_apq(prtkimgdesc,&updateflags,1);
	      if (ret < 0)
	      {
	         err_code = _eRTK_BURN_FACTORY_FAIL;
	         install_debug("PQ:rtk_install_apq(%d) fail\r\n", ret);
	         goto before_burn_end_tsb;
	      }
	  }
	  else if(g_script.pq_flag == 2)
	  {
	  	updateflags.pq_flag=1;
	  }
	  else
	  {
	  	updateflags.pq_flag=0;
		updateflags.pq_first_update=0;
	  }
   }
   for (i=0; i<TV005_MAX_UPDATE_APK; i++)
   {
      install_info("\r\n[Install APK data]\r\n");
      if (g_script.dynamic_apk_flag[i][0] != 0)
      {
         if(prtkimgdesc->start_customer == 1) {
            //snprintf(msg, sizeof(msg), "APK Data (%s)", g_script.dynamic_apk_flag[i]);
            //printf("msg=%s\n\n", msg);
            //rtk_customer_write_string_tsb(prtkimgdesc, msg);
            rtk_customer_write_burn_partname(prtkimgdesc, TAG_APK);
            rtk_customer_write_increase_progressbar(prtkimgdesc, 10);
         }
         boottable.tag = TAG_APK;
         if (g_script.installimg_flag==0 && i==0)
         {
            snprintf(cmd, sizeof(cmd), "mkdir -p /system;mount /dev/block/mmcblk0p1 /system");
            if((ret = rtk_command(cmd, __LINE__, __FILE__, 1)) < 0) {
               install_debug("install  apks command0 fail!\r\n");
               goto before_burn_end_tsb;
            }
            if (ret < 0)
            {
               err_code = _eRTK_BURN_FACTORY_FAIL;
               install_debug("APK:rtk_install_apk(%d) fail\r\n", ret);
               goto before_burn_end_tsb;
            }
         }
         snprintf(cmd, sizeof(cmd), "cd /;cp -rf %s/%s /system/app/;chmod 664 /system/app/%s", TV005_USB_DATA_PATH, g_script.dynamic_apk_flag[i], g_script.dynamic_apk_flag[i]);
	 rtk_command(cmd, __LINE__, __FILE__, 1);
         snprintf(cmd, sizeof(cmd), "chown 1000:1000 /system/app/%s;sync;umount /system", g_script.dynamic_apk_flag[i]);
         //snprintf(cmd, sizeof(cmd), "adb root;adb remount;cd %s;adb install -r %s;umount /system", TV005_USB_DATA_PATH, g_script.dynamic_apk_flag[i]);
         if((ret = rtk_command(cmd, __LINE__, __FILE__, 1)) < 0) {
            install_debug("install  apks command1 fail!\r\n");
            goto before_burn_end_tsb;
         }
         if (ret < 0)
         {
            err_code = _eRTK_BURN_FACTORY_FAIL;
            install_debug("APK:rtk_install_apk(%d) fail\r\n", ret);
            goto before_burn_end_tsb;
         }
      }
   }
   if (g_script.model_id_flag == 1)
   {
      if(prtkimgdesc->start_customer == 1) {
         rtk_customer_write_burn_partname(prtkimgdesc, TAG_TV005_MID);
         rtk_customer_write_increase_progressbar(prtkimgdesc, 10);
      }
      boottable.tag = TAG_TV005_MID;
      ret = rtk_install_tsb(prtkimgdesc,&updateflags,0);
      if (ret < 0)
      {
         err_code = _eRTK_BURN_FACTORY_FAIL;
         install_debug("model_id.txt:rtk_install_tsb(%d) fail\r\n", ret);
         goto before_burn_end_tsb;
      }
   }
   if (g_script.product_info_flag == 1)
   {
      if(prtkimgdesc->start_customer == 1) {
         rtk_customer_write_burn_partname(prtkimgdesc, TAG_TV005_PINF);
         rtk_customer_write_increase_progressbar(prtkimgdesc, 10);
      }
      boottable.tag = TAG_TV005_PINF;
      ret = rtk_install_tsb(prtkimgdesc,&updateflags, 1);
      if (ret < 0)
      {
         err_code = _eRTK_BURN_FACTORY_FAIL;
         install_debug("product_info.txt:rtk_install_tsb(%d) fail\r\n", ret);
         goto before_burn_end_tsb;
      }
   }
   if (g_script.serial_num_flag == 1)
   {
      if(prtkimgdesc->start_customer == 1) {
         rtk_customer_write_burn_partname(prtkimgdesc, TAG_TV005_SN);
         rtk_customer_write_increase_progressbar(prtkimgdesc, 10);
      }
      boottable.tag = TAG_TV005_SN;
      ret = rtk_install_tsb(prtkimgdesc,&updateflags, 2);
      if (ret < 0)
      {
         err_code = _eRTK_BURN_FACTORY_FAIL;
         install_debug("serial_num.txt:rtk_install_tsb(%d) fail\r\n", ret);
         goto before_burn_end_tsb;
      }
   }
   if (g_script.location_info_flag == 1)
   {
      if(prtkimgdesc->start_customer == 1) {
         rtk_customer_write_burn_partname(prtkimgdesc, TAG_TV005_LOC);
         rtk_customer_write_increase_progressbar(prtkimgdesc, 10);
      }
      boottable.tag = TAG_TV005_LOC;
      ret = rtk_install_tsb(prtkimgdesc,&updateflags, 3);
      if (ret < 0)
      {
         err_code = _eRTK_BURN_FACTORY_FAIL;
         install_debug("location_info.txt:rtk_install_tsb(%d) fail\r\n", ret);
         goto before_burn_end_tsb;
      }
   }

   if (g_script.support_wifi_flag == 1)
   {
      if(prtkimgdesc->start_customer == 1) {
         rtk_customer_write_burn_partname(prtkimgdesc, TAG_TV005_FLAG);
         rtk_customer_write_increase_progressbar(prtkimgdesc, 6);
      }
      boottable.tag = TAG_TV005_FLAG;
      updateflags.support_wifi_flag=1;
   }
   else if (g_script.support_wifi_flag == 2)
   {
      if(prtkimgdesc->start_customer == 1) {
         rtk_customer_write_burn_partname(prtkimgdesc, TAG_TV005_FLAG);
         rtk_customer_write_increase_progressbar(prtkimgdesc, 10);
      }
      boottable.tag = TAG_TV005_FLAG;
      updateflags.support_wifi_flag=0;
   }
    ////////////////hebrew_flag update///////////////////////
   if (g_script.support_hebrew_flag == 1)
   {
      if(prtkimgdesc->start_customer == 1) {
         rtk_customer_write_burn_partname(prtkimgdesc, TAG_TV005_FLAG);
         rtk_customer_write_increase_progressbar(prtkimgdesc, 10);
      }
      boottable.tag = TAG_TV005_FLAG;
	  updateflags.support_hebrew_flag=1;
   }
   else if (g_script.support_hebrew_flag == 2)
   {
      if(prtkimgdesc->start_customer == 1) {
         rtk_customer_write_burn_partname(prtkimgdesc, TAG_TV005_FLAG);
         rtk_customer_write_increase_progressbar(prtkimgdesc, 10);
      }
      boottable.tag = TAG_TV005_FLAG;
      updateflags.support_hebrew_flag=0;
   }
   ////////////////cricket_flag update///////////////////////
   if (g_script.indian_cricket_flag == 1)
   {
      if(prtkimgdesc->start_customer == 1) {
         rtk_customer_write_burn_partname(prtkimgdesc, TAG_TV005_FLAG);
         rtk_customer_write_increase_progressbar(prtkimgdesc, 10);
      }
      boottable.tag = TAG_TV005_FLAG;
	  updateflags.indian_cricket_flag=1;

   }
  else if (g_script.indian_cricket_flag == 2)
   {
      if(prtkimgdesc->start_customer == 1) {
         rtk_customer_write_burn_partname(prtkimgdesc, TAG_TV005_FLAG);
         rtk_customer_write_increase_progressbar(prtkimgdesc, 10);
      }
      boottable.tag = TAG_TV005_FLAG;
	  updateflags.indian_cricket_flag=0;

   }
   ////////////////init_userdata_flag update///////////////////////
   if (g_script.init_userdata_flag == 1)
   {
      if(prtkimgdesc->start_customer == 1) {
         rtk_customer_write_burn_partname(prtkimgdesc, TAG_TV005_FLAG);
         rtk_customer_write_increase_progressbar(prtkimgdesc, 10);
      }
      boottable.tag = TAG_TV005_FLAG;
      updateflags.init_userdata_flag=1;
      // clear data partition
      snprintf(cmd, sizeof(cmd), "cd /;umount /data/;mke2fs /dev/block/mmcblk0p2;");
      rtk_command(cmd, __LINE__, __FILE__, 1);
      snprintf(cmd, sizeof(cmd), "cd /;umount /cache/;mke2fs /dev/block/mmcblk0p3;");
      rtk_command(cmd, __LINE__, __FILE__, 1);
   }
  else if (g_script.init_userdata_flag == 2)
   {
      if(prtkimgdesc->start_customer == 1) {
         rtk_customer_write_burn_partname(prtkimgdesc, TAG_TV005_FLAG);
         rtk_customer_write_increase_progressbar(prtkimgdesc, 10);
      }
      boottable.tag = TAG_TV005_FLAG;
	  updateflags.init_userdata_flag=0;

   }

/*
	//load factory from flash
	if ((ret = factory_load(NULL, prtkimgdesc)) < 0) {
		install_debug("factory_load fail\r\n");
	 }
	if (!get_factory_tmp_dir()){
		install_debug("can't get factory path\r\n");
		return -1;
	 }
	getcwd(workpath, 128);
	chdir(get_factory_tmp_dir());
*/
	install_info("\r\n[ifcmd0  save flags]\r\n");


   ////////////////save flags///////////////////////
  	install_log("save updateflags file!!\n");

  	install_log("updateflags.aq_flag = %d\n",updateflags.aq_flag);
	install_log("updateflags.aq_first_update = %d\n",updateflags.aq_first_update);
	install_log("updateflags.aq_version = %s\n",updateflags.aq_version);

	install_log("updateflags.pq_flag = %d\n",updateflags.pq_flag);
	install_log("updateflags.pq_first_update = %d\n",updateflags.pq_first_update);
	install_log("updateflags.pq_version = %s\n",updateflags.pq_version);

	install_log("updateflags.mode_id = %d\n",updateflags.mode_id);
	install_log("updateflags.support_wifi_flag = %d\n",updateflags.support_wifi_flag);
	install_log("updateflags.support_hebrew_flag = %d\n",updateflags.support_hebrew_flag);
	install_log("updateflags.indian_cricket_flag = %d\n",updateflags.indian_cricket_flag);
	install_log("updateflags.init_userdata_flag = %d\n",updateflags.init_userdata_flag);

	install_log("updateflag->manufacture=%s\n", updateflags.manufacture);
	install_log("updateflag->productname=%s\n", updateflags.productname);
	install_log("updateflag->version=%s\n", updateflags.version);
	install_log("updateflag->oemstring=%s\n", updateflags.oemstring);
	install_log("updateflag->uuid=%s\n", updateflags.uuid);
	install_log("updateflag->country=%s\n", updateflags.country);
	install_log("updateflag->language=%s\n", updateflags.language);
	install_log("updateflag->serialnumber=%s\n", updateflags.serialnumber);

   // create XML file for DMI
   #define XMLSTR  "echo '<?xml version=\"1.0\" encoding=\"UTF-8\" ?>' > tv005_info.xml"
   snprintf(cmd, sizeof(cmd), "cd /tmp/factory/;%s", XMLSTR);
   rtk_command(cmd, __LINE__, __FILE__, 1);
   snprintf(cmd, sizeof(cmd), "cd /tmp/factory/;echo \"<dmi_info>\" >> tv005_info.xml", updateflags.manufacture);
   rtk_command(cmd, __LINE__, __FILE__, 1);
   snprintf(cmd, sizeof(cmd), "cd /tmp/factory/;echo \"<aq_flag>%d</aq_flag>\" >> tv005_info.xml", updateflags.aq_flag);
   rtk_command(cmd, __LINE__, __FILE__, 1);
   snprintf(cmd, sizeof(cmd), "cd /tmp/factory/;echo \"<aq_first_update>%d</aq_first_update>\" >> tv005_info.xml", updateflags.aq_first_update);
   rtk_command(cmd, __LINE__, __FILE__, 1);
   snprintf(cmd, sizeof(cmd), "cd /tmp/factory/;echo \"<aq_version>%s</aq_version>\" >> tv005_info.xml", updateflags.aq_version);
   rtk_command(cmd, __LINE__, __FILE__, 1);
   snprintf(cmd, sizeof(cmd), "cd /tmp/factory/;echo \"<pq_flag>%d</pq_flag>\" >> tv005_info.xml", updateflags.pq_flag);
   rtk_command(cmd, __LINE__, __FILE__, 1);
   snprintf(cmd, sizeof(cmd), "cd /tmp/factory/;echo \"<pq_first_update>%d</pq_first_update>\" >> tv005_info.xml", updateflags.pq_first_update);
   rtk_command(cmd, __LINE__, __FILE__, 1);
   snprintf(cmd, sizeof(cmd), "cd /tmp/factory/;echo \"<pq_version>%s</pq_version>\" >> tv005_info.xml", updateflags.pq_version);
   rtk_command(cmd, __LINE__, __FILE__, 1);
   snprintf(cmd, sizeof(cmd), "cd /tmp/factory/;echo \"<mode_id>%d</mode_id>\" >> tv005_info.xml", updateflags.mode_id);
   rtk_command(cmd, __LINE__, __FILE__, 1);
   snprintf(cmd, sizeof(cmd), "cd /tmp/factory/;echo \"<support_wifi_flag>%d</support_wifi_flag>\" >> tv005_info.xml", updateflags.support_wifi_flag);
   rtk_command(cmd, __LINE__, __FILE__, 1);
   snprintf(cmd, sizeof(cmd), "cd /tmp/factory/;echo \"<support_hebrew_flag>%d</support_hebrew_flag>\" >> tv005_info.xml", updateflags.support_hebrew_flag);
   rtk_command(cmd, __LINE__, __FILE__, 1);
   snprintf(cmd, sizeof(cmd), "cd /tmp/factory/;echo \"<indian_cricket_flag>%d</indian_cricket_flag>\" >> tv005_info.xml", updateflags.indian_cricket_flag);
   rtk_command(cmd, __LINE__, __FILE__, 1);
   snprintf(cmd, sizeof(cmd), "cd /tmp/factory/;echo \"<init_userdata_flag>%d</init_userdata_flag>\" >> tv005_info.xml", updateflags.init_userdata_flag);
   rtk_command(cmd, __LINE__, __FILE__, 1);
   snprintf(cmd, sizeof(cmd), "cd /tmp/factory/;echo \"<manufacture>%s</manufacture>\" >> tv005_info.xml", updateflags.manufacture);
   rtk_command(cmd, __LINE__, __FILE__, 1);
   snprintf(cmd, sizeof(cmd), "cd /tmp/factory/;echo \"<productname>%s</productname>\" >> tv005_info.xml", updateflags.productname);
   rtk_command(cmd, __LINE__, __FILE__, 1);
   snprintf(cmd, sizeof(cmd), "cd /tmp/factory/;echo \"<version>%s</version>\" >> tv005_info.xml", updateflags.version);
   rtk_command(cmd, __LINE__, __FILE__, 1);
   snprintf(cmd, sizeof(cmd), "cd /tmp/factory/;echo \"<uuid>%s</uuid>\" >> tv005_info.xml", updateflags.uuid);
   rtk_command(cmd, __LINE__, __FILE__, 1);
   snprintf(cmd, sizeof(cmd), "cd /tmp/factory/;echo \"<oemstring>%s</oemstring>\" >> tv005_info.xml", updateflags.oemstring);
   rtk_command(cmd, __LINE__, __FILE__, 1);
   snprintf(cmd, sizeof(cmd), "cd /tmp/factory/;echo \"<country>%s</country>\" >> tv005_info.xml", updateflags.country);
   rtk_command(cmd, __LINE__, __FILE__, 1);
   snprintf(cmd, sizeof(cmd), "cd /tmp/factory/;echo \"<language>%s</language>\" >> tv005_info.xml", updateflags.language);
   rtk_command(cmd, __LINE__, __FILE__, 1);
   snprintf(cmd, sizeof(cmd), "cd /tmp/factory/;echo \"<serialnumber>%s</serialnumber>\" >> tv005_info.xml", updateflags.serialnumber);
   rtk_command(cmd, __LINE__, __FILE__, 1);
   snprintf(cmd, sizeof(cmd), "cd /tmp/factory/;echo \"</dmi_info>\" >> tv005_info.xml", updateflags.manufacture);
   rtk_command(cmd, __LINE__, __FILE__, 1);
   snprintf(cmd, sizeof(cmd), "cd /tmp/factory/;chmod 666 tv005_info.xml;chmod 644 updateflags.dat");
   rtk_command(cmd, __LINE__, __FILE__, 1);

   if (updateflags.country != NULL)
   {
      snprintf(cmd, sizeof(cmd), "umount /data;mount /dev/block/mmcblk0p2 /data/");
      rtk_command(cmd, __LINE__, __FILE__, 1);
      snprintf(cmd, sizeof(cmd), "mkdir -p /data/toshiba;echo \"%s\" > /data/toshiba/country;sync;umount /data/", updateflags.country);
      rtk_command(cmd, __LINE__, __FILE__, 1);
   }

   fd = fopen(path,"w");
   if(fd!=NULL)
   	{
   		install_log("sizeof(updateflags) = %d\n",sizeof(updateflags));
   		fwrite(&updateflags,1,sizeof(updateflags),fd);
		fclose(fd);
   	}else
   	install_log("open path = %s failed\n",path);

   install_log("\r\n[ifcmd1]\r\n");
	if((ret = rtk_command("tar cf /tmp/factory.tar /tmp/factory", __LINE__, __FILE__, 1)) < 0) {
	   install_debug("install factory command1 fail!\r\n");
	}
	install_log("\r\n");

	chdir(workpath);
	 //flush factory
	if((ret = factory_flush(prtkimgdesc->factory_start, prtkimgdesc->factory_size, prtkimgdesc)) < 0)
	{
		install_debug("factory_flush fail\r\n");
		return -1;
	}

   install_info("@@@@@@@@@@@@@@@@factory_flush end!!\n");


   // execute postprocess.sh to do some actions after installation
   snprintf(path, sizeof(path), "%s", POSTPROCESS_TEMP);
   if(rtk_extract_file(prtkimgdesc, &prtkimgdesc->postprocess, POSTPROCESS_TEMP) < 0)
      install_fail("Can't extract %s\r\n", POSTPROCESS_TEMP);
   else {
      snprintf(cmd, sizeof(cmd), "chmod 777 %s", POSTPROCESS_TEMP);
      rtk_command(cmd, __LINE__, __FILE__, 0);

      snprintf(cmd, sizeof(cmd), "%s", POSTPROCESS_TEMP);
      if((rtk_command(cmd , __LINE__, __FILE__, 0)) < 0)
         install_fail("Can't execute %s\r\n", POSTPROCESS_TEMP);
   }

   boottable.boottype = BOOTTYPE_COMPLETE;
   boottable.tag = TAG_COMPLETE;
   // if backup is enabled, we use another partition for booting next time.
   if (prtkimgdesc->backup == 1) {
      if (prtkimgdesc->bootpart == 1)
         boottable.bootpart = 0;
      else
         boottable.bootpart = 1;
   }
before_burn_end_tsb:
   install_log("\r\n\r\nWrite boottable ret=%d\r\n", ret);
   write_boottable(&boottable, prtkimgdesc->factory_start, prtkimgdesc->factory_size, prtkimgdesc);

   if (prtkimgdesc->safe_upgrade == 1) {
      // remove upgrade lock file when SSU is set
      rtk_create_remove_file_factory_mac(prtkimgdesc, UPGRADE_LOCK_FILENAME, REMOVE_FILE_FACTORY_MODE);
   }

   rtk_flush_pingpong_factory_mac(prtkimgdesc);

burn_end_usb_tsb:
   if(prtkimgdesc->start_customer == 1) {
      rtk_customer_write_increase_progressbar(prtkimgdesc, 100);
      if (ret == 0)
         err_code = 0;
      rtk_customer_write_burn_result(prtkimgdesc, err_code);
   }

   return ret;
}
#endif // TV005_USB_AUTO_SCRIPT

#ifndef __OFFLINE_GENERATE_BIN__
#ifdef FLASH_OFFLINE_BIN_WRITER
int rtk_burn_offlineBin_preCheck(char* binFilePath, char** getBP1Path, char** getBP2Path)
{
    int idx = 0;
    char* ptr = NULL;
    char chkFilePath[TARFILENAMELEN] = {'\0'};
    ptr  = strstr(binFilePath, ".");

    if (ptr != NULL) {
        printf("Enter burnFlash_Emmc_preCheck1:%s\n", ptr);
        printf("step1= %d\n", (int)(ptr-binFilePath));

       strncpy(chkFilePath, binFilePath, (int)((ptr-binFilePath) +1));
        idx = strlen(chkFilePath);

       printf("step1= %s (len:%d)\n", chkFilePath, idx);
        strcat(chkFilePath, "bp1.bin");
       printf("step2= %s\n", chkFilePath);

        if (access(chkFilePath, F_OK ) != -1) {  // file exists
            *getBP1Path = (char*)malloc(sizeof(char) * TARFILENAMELEN);
            if (*getBP1Path) {
                memset(*getBP1Path, 0, TARFILENAMELEN);
                memcpy(*getBP1Path, chkFilePath, TARFILENAMELEN);
            }
        }
        strcpy(chkFilePath+idx, "bp2.bin");
        printf("step3= %s\n", chkFilePath);
        if (access(chkFilePath, F_OK ) != -1) {  // file exists
            *getBP2Path = (char*)malloc(sizeof(char) * TARFILENAMELEN);
            if (*getBP2Path) {
                memset(*getBP2Path, 0, TARFILENAMELEN);
                memcpy(*getBP2Path, chkFilePath, TARFILENAMELEN);
            }
        }

    } else
        return -1;

    return 0;
}

// This function is for emmc case now. Maybe there are other storage case arrised in the furture.
// At that time, I think this function should be renamed or reorganized in case
int rtk_burn_offlineBin_from_usb(struct t_rtkimgdesc* prtkimgdesc)
{
    int ret = 0, err_code = -1;
    char bootdev[50] = {0};
    char *BP1BinPath = NULL, *BP2BinPath = NULL;
    rtk_customer_init(prtkimgdesc, 1);

    // initial rtkimgdesc's meminfo
    if((ret = fill_rtkimgdesc_meminfo(prtkimgdesc)) < 0) {
        install_debug("fill_rtkimgdesc_meminfo(%d) fail\r\n", ret);
        if (ret == -_eRTK_GENERAL_FAIL)
            err_code = _eFILL_RTKIMGDESC_MEMINFO_FAIL;
        else
            err_code = -ret;
        goto burn_offline_end_usb;
    }
    // getbootpart0 / bootpart1 filepath depends on outputbin
    rtk_burn_offlineBin_preCheck((char*)&(prtkimgdesc->tarinfo.tarfile_path), &BP1BinPath, &BP2BinPath);

    // burn output.bin
    install_info("Starting to Burn output.bin");
    rtk_customer_write_burn_partname(prtkimgdesc->customer_fp, "Output.bin");
    ret = rtk_burn_flashbin(prtkimgdesc->tarinfo.tarfile_path, prtkimgdesc->mtdblock_path, 1,prtkimgdesc->customer_fp);
    if (ret < 0) {
         err_code = _eRTK_GENERAL_FAIL;
         goto before_burn_offline_end;
    }
    sprintf(bootdev, "%sboot0", prtkimgdesc->mtdblock_path );

    // burn bp1.bin
    if (BP1BinPath) {
        install_info("Starting to Burn BP1.bin");
        rtk_customer_write_burn_partname(prtkimgdesc->customer_fp, "BP1.bin");
        ret = rtk_burn_flashbin(BP1BinPath, bootdev, 0,prtkimgdesc->customer_fp);
        if (ret < 0) {
             err_code = _eRTK_GENERAL_FAIL;
        }
    }
    // burn bp2.bin
    sprintf(bootdev, "%sboot1", prtkimgdesc->mtdblock_path );
    if (BP2BinPath) {
        install_info("Starting to Burn BP2.bin");
        rtk_customer_write_burn_partname(prtkimgdesc->customer_fp, "BP2.bin");
        ret = rtk_burn_flashbin(BP2BinPath, bootdev, 0,prtkimgdesc->customer_fp);
        if (ret < 0) {
             err_code = _eRTK_GENERAL_FAIL;
        }
    }

before_burn_offline_end:

burn_offline_end_usb:
   if(prtkimgdesc->start_customer == 1) {
      rtk_customer_write_increase_progressbar(prtkimgdesc, 100);
      if (ret == 0)
         err_code = 0;
      rtk_customer_write_burn_result(prtkimgdesc, err_code);
   }

    return ret;
}
#endif
#endif

// UI UPGRADE and TAB INSTALL
int rtk_burn_rtkimg_from_usb(struct t_rtkimgdesc* prtkimgdesc)
{
   int ret = 0, err_code = -1;
   S_BOOTTABLE boottable, *pbt = NULL;
   char cmd[128] = {0}, path[128] = {0};
   int key_installed = 0;
   int secure_boot = 0;

   // parsing tarfile
   install_debug("prtkimgdesc->tarinfo.tarfile_path:%s\r\n", prtkimgdesc->tarinfo.tarfile_path);
   if((ret = parse_tar(prtkimgdesc)) < 0) {
      install_debug("parse_tar(%d) fail\r\n", ret);
      err_code = _ePARSE_TAR_FAIL;
      goto burn_end_usb;
   }

   // load configuration
   if((ret = fill_rtk_part_list_by_config(prtkimgdesc)) < 0) {
      install_debug("fill_rtk_part_list_by_config(%d) fail\r\n", ret);
      err_code = _eFILL_RTK_PART_LIST_BY_CONFIG_FAIL;
      goto burn_end_usb;
   }

	if (gDebugPrintfLogLevel&INSTALL_MEM_LEVEL)
		system("cat /proc/meminfo");

#ifdef PC_SIMULATE
    prtkimgdesc->start_customer = 0;
    prtkimgdesc->stop_reboot = 1;
#endif

   // customer
   if(prtkimgdesc->start_customer == 1)
      rtk_customer_init(prtkimgdesc);

   // initial rtkimgdesc's meminfo
   if((ret = fill_rtkimgdesc_meminfo(prtkimgdesc)) < 0) {
      install_debug("fill_rtkimgdesc_meminfo(%d) fail\r\n", ret);
      if (ret == -_eRTK_GENERAL_FAIL)
         err_code = _eFILL_RTKIMGDESC_MEMINFO_FAIL;
      else
         err_code = -ret;
      goto burn_end_usb;
   }

	// handle dynamic partition
	fill_rtkpartdesc_by_dynamicTbl(prtkimgdesc);

   if((ret = parse_tar(prtkimgdesc)) < 0) {
      install_debug("parse_tar fail\r\n");
      err_code = _ePARSE_TAR_FAIL;
      goto burn_end_usb;
   }

#ifdef UPDATE_8198C_FW
	  // tftp 8198c FW to router, and wait for it to finish updating the FW..
	  if( (ret=rtk_extract_file(prtkimgdesc, &prtkimgdesc->fw_98c, FW_98C_TEMP)) < 0) {
		 install_fail("Can't extract %s\r\n", FW_98C_TEMP);
		 goto burn_end_usb;
	  }

	  rtk_command("busybox ifconfig eth0 up 192.168.1.10 netmask 255.255.255.0 broadcast 255.255.255.255", __LINE__, __FILE__);
	  sprintf(cmd, "busybox tftp -l %s -r fw_98c.bin -p 192.168.1.6", FW_98C_TEMP);
	  rtk_command(cmd, __LINE__, __FILE__);

	  // sleep for 120 seconds for router to update FW..
	  for( int i=0; i<120; i++ ) {
		 sleep(1);
	  }
	  if(prtkimgdesc->start_customer == 1) {
		 rtk_customer_write_burn_dhc(prtkimgdesc);
	  }
#endif

   if( (ret=otp_init()) < 0 ) {
      install_debug("otp_init() failed\n");
      err_code = _eRTK_TAG_SECURE_FAIL;
      goto burn_end_usb;
   }

   read_efuse_status( &secure_boot, &key_installed );
   install_info("secure-boot(%d) key_installed(%d)\n", secure_boot, key_installed);

   // check whether the image can be upgraded or not.
   if( secure_boot ) {
      if( prtkimgdesc->efuse_key ) {
		 ret = -1;
      } else if( ! prtkimgdesc->secure_boot ) {
		 ret = -2;
      }
   }
   else {
      if( ! prtkimgdesc->efuse_key ) {
	  	if( prtkimgdesc->secure_boot ) {
		 	ret = -3;
	  	}
      } else {
        if( key_installed && prtkimgdesc->secure_boot ) {
			ret = -4;
        }
      }
   }

   // check if key is packed correctly.
   if( ret >=0 ) {
      if( prtkimgdesc->efuse_key ) {
         if( prtkimgdesc->secure_boot ) {
            if( !prtkimgdesc->cipher_key[KEY_RSA_PUBLIC].img_size ||
                !prtkimgdesc->cipher_key[KEY_AES_KH].img_size ||
                !prtkimgdesc->cipher_key[KEY_AES_KS].img_size )
                ret = -5;
         } else {
            if( !prtkimgdesc->cipher_key[KEY_RSA_PUBLIC].img_size ||
                !prtkimgdesc->cipher_key[KEY_AES_KN].img_size )
                ret = -6;
         }
      }
   }
   if( ret < 0 ) {
      install_debug("ret(%d) secure_boot(%d) key_installed(%d) img.secure_boot(%d) img.efuse_key(%d)\n",
	  	ret, secure_boot, key_installed, prtkimgdesc->secure_boot,prtkimgdesc->efuse_key );
      err_code = _eRTK_TAG_SECURE_FAIL;
      goto burn_end_usb;
   }

#ifdef PC_SIMULATE
    memset( &prtkimgdesc->postprocess, 0, sizeof(prtkimgdesc->postprocess));
#endif

   //dump_flash
   if(prtkimgdesc->dump_flash == 1) {
#ifdef EMMC_SUPPORT
      if(prtkimgdesc->flash_type == MTD_NANDFLASH || prtkimgdesc->flash_type == MTD_EMMC)
#else
      if(prtkimgdesc->flash_type == MTD_NANDFLASH)
#endif
      {
         install_log("NAND FLASH does not support dump flash option\r\n");
         ret = -1;
         err_code = _eRTK_GENERAL_FAIL;
      } else {
         ret = rtk_dump_flash(prtkimgdesc);
         if (ret < 0)
            err_code = _eRTK_GENERAL_FAIL;
      }
      goto burn_end_usb;
   }

   //only install factory
   if(prtkimgdesc->only_factory == 1) {
      if(prtkimgdesc->start_customer == 1) {
         rtk_customer_write_burn_partname(prtkimgdesc, TAG_FACTORY);
         rtk_customer_write_increase_progressbar(prtkimgdesc, 10);
      }
      ret = rtk_install_factory(prtkimgdesc);
      install_debug("rtk_install_factory(%d)\r\n", ret);
      goto burn_end_usb;
   }

   //only install bootcode
   if (prtkimgdesc->only_bootcode == 1) {
      if (prtkimgdesc->start_customer == 1) {
         rtk_customer_write_burn_partname(prtkimgdesc, TAG_BOOTCODE);
         rtk_customer_write_increase_progressbar(prtkimgdesc, 10);
      }
      if( prtkimgdesc->flash_type == MTD_NANDFLASH )
          ret = rtk_burn_bootcode_nand(prtkimgdesc);
      else if(prtkimgdesc->flash_type == MTD_EMMC)
          ret = rtk_burn_bootcode_emmc(prtkimgdesc);
      else
          ret = -1;

      if (ret < 0) {
         err_code = _eRTK_BURN_BOOTCODE_FAIL;
         goto before_burn_end;
      }

	  // we have to delete env settings of bootcode
      ret = rtk_install_factory(prtkimgdesc);
      install_debug("rtk_install_factory(%d)\r\n", ret);
      goto burn_end_usb;
   }

   // fill rtkimgdesc's offset
   if((ret = fill_rtkimgdesc_layout(prtkimgdesc)) < 0) {
      install_debug("fill_rtkimgdesc_layout(%d) fail\r\n", ret);
      if (ret == -_eRTK_GENERAL_FAIL)
         err_code = _eFILL_RTK_IMGDESC_LAYOUT_FAIL;
      else
         err_code = -ret;
      goto burn_end_usb;
   }

   // unlock flash
#ifdef EMMC_SUPPORT
   if(prtkimgdesc->flash_type == MTD_NANDFLASH || prtkimgdesc->flash_type == MTD_EMMC) {
#else
   if(prtkimgdesc->flash_type == MTD_NANDFLASH) {
#endif
      /* do nothing */
   	} else {
      install_log("unlock flash\r\n");
      rtk_unlock_mtd(prtkimgdesc, 0, prtkimgdesc->flash_size);
   }

   if (prtkimgdesc->erase_free_space == 1) {
      install_log("erase free space flash offset = 0x%llx, length = 0x%llx.\r\n"
                  , prtkimgdesc->fw[FW_P_FREE_SPACE].flash_offset
                  , prtkimgdesc->fw[FW_P_FREE_SPACE].flash_allo_size);
      rtk_erase(prtkimgdesc, prtkimgdesc->fw[FW_P_FREE_SPACE].flash_offset, prtkimgdesc->fw[FW_P_FREE_SPACE].flash_allo_size);
   }

   if(prtkimgdesc->fake_burn == 1) {
      install_log("FAKE_BURN");
      return -1;
   }

   memset(&boottable, 0, sizeof(boottable));
   read_boottable(&boottable, prtkimgdesc);
   boottable.boottype = BOOTTYPE_UNKNOWN_BOOTTYPE;
   boottable.tag = TAG_UNKNOWN;
   sprintf(boottable.date, "%s", __DATE__);
   sprintf(boottable.time, "%s", __TIME__);
   install_log("\r\n\r\nWrite boottable\r\n");
   install_debug("claire skip one fsync");
   write_boottable(&boottable, prtkimgdesc->factory_start, prtkimgdesc->factory_size, prtkimgdesc, false);

   install_ui("\r\n\r\n");

   modify_signature(prtkimgdesc->reserved_boot_size, prtkimgdesc->flash_type);

   if(prtkimgdesc->flash_type == MTD_NANDFLASH) {
      /*NAND FLASH -start-*/

      //install bootcode
      if (prtkimgdesc->bootcode == 1) {
         if (prtkimgdesc->start_customer == 1) {
            rtk_customer_write_burn_partname(prtkimgdesc, TAG_BOOTCODE);
            rtk_customer_write_increase_progressbar(prtkimgdesc, 10);
         }
         ret = rtk_burn_bootcode_nand(prtkimgdesc);
         if (ret < 0) {
            err_code = _eRTK_BURN_BOOTCODE_FAIL;
            goto burn_end_usb;
         }
      }

      pbt = &boottable;

      boottable.tag = TAG_UPDATE_ETC;
      do {
         if(prtkimgdesc->start_customer == 1) {
            rtk_customer_write_progress(prtkimgdesc, boottable.tag);
         }

         ret = tagflow3(&boottable, prtkimgdesc, pbt);

         if(ret < 0) {
            err_code = -ret;
            goto burn_end_usb;
            break;
         }
      } while(boottable.tag != TAG_COMPLETE);

      if( !key_installed && prtkimgdesc->efuse_key ) {
         ret = rtk_burn_cipher_key(prtkimgdesc);
         if( ret < 0 ) {
            err_code = _eRTK_TAG_SECURE_FAIL;
            goto burn_end_usb;
         }
      }

      if( (ret=rtk_burn_fwdesc(prtkimgdesc, &boottable)) < 0) {
         install_debug("burn boottable fail\r\n");
         err_code = _eRTK_GENERAL_FAIL;
         goto burn_end_usb;
      }

      /*NAND FLASH -end-*/
   }
   else if(prtkimgdesc->flash_type == MTD_NORFLASH) {
      /*SPI NOR FLASH -start-*/
      if (prtkimgdesc->bootcode == 0) {
         if ((ret = rtk_check_update_bootcode(prtkimgdesc)) == 1) {
            install_log("update_bootcode found in factory, start upgrade bootcode.\r\n");
            prtkimgdesc->bootcode = 1;
         }
      }

      //install bootcode
      if (prtkimgdesc->bootcode == 1) {
         if (prtkimgdesc->start_customer == 1) {
            rtk_customer_write_burn_partname(prtkimgdesc, TAG_BOOTCODE);
            rtk_customer_write_increase_progressbar(prtkimgdesc, 10);
         }
         ret = rtk_burn_bootcode_mac_spi(prtkimgdesc);
         install_debug("rtk_burn_bootcode_mac(%d)\r\n", ret);
         if (ret < 0) {
            err_code = _eRTK_BURN_BOOTCODE_FAIL;
            goto burn_end_usb;
         }
      }

      pbt = &boottable;

      boottable.tag = TAG_UPDATE_ETC;

      // enter burning progress
      do {
         if(prtkimgdesc->start_customer == 1) {
            rtk_customer_write_increase_progressbar(prtkimgdesc, 11);
            rtk_customer_write_burn_partname(prtkimgdesc, boottable.tag);
         }

         ret = tagflow3(&boottable, prtkimgdesc, pbt);

         if(ret < 0) {
            err_code = -ret;
            goto burn_end_usb;
            break;
         }
      } while(boottable.tag != TAG_COMPLETE);

      /*SPI NOR FLASH -end-*/
   } else if(prtkimgdesc->flash_type == MTD_EMMC) {
	  pbt = read_boottable_emmc(&boottable, prtkimgdesc);

      // MBR is exactly the same and skip re-partition.
      if (!pbt->mbr_matched) {
          ret = write_boottable_emmc(&boottable, prtkimgdesc->factory_start, prtkimgdesc->factory_size, prtkimgdesc);
		  if( ret < 0 ) {
			  err_code = _eRTK_GENERAL_FAIL;
			  goto before_burn_end;
		  }
      }

      // mount and format emmc partition
      ret = rtk_mount_format_emmc(&boottable, prtkimgdesc);
      if (ret < 0) {
         err_code = _eRTK_GENERAL_FAIL;
         goto before_burn_end;
      }

	  //install bootcode
      if (prtkimgdesc->bootcode == 1) {
         if (prtkimgdesc->start_customer == 1) {
            rtk_customer_write_burn_partname(prtkimgdesc, TAG_BOOTCODE);
            rtk_customer_write_increase_progressbar(prtkimgdesc, 10);
         }
		 boottable.tag = TAG_BOOTCODE;
         ret = rtk_burn_bootcode_emmc(prtkimgdesc);
         if (ret < 0) {
            err_code = _eRTK_BURN_BOOTCODE_FAIL;
            goto before_burn_end;
         }
      }

      boottable.tag = TAG_UPDATE_ETC;
      do {
         if(prtkimgdesc->start_customer == 1) {
            rtk_customer_write_progress(prtkimgdesc, boottable.tag);
         }

         ret = tagflow3_emmc(&boottable, prtkimgdesc, pbt);

         if(ret < 0) {
            err_code = -ret;
            goto before_burn_end;
         }
      } while(boottable.tag != TAG_COMPLETE);

	  if( !key_installed && prtkimgdesc->efuse_key ) {
	     ret = rtk_burn_cipher_key(prtkimgdesc);
	     if( ret < 0 ) {
	        err_code = _eRTK_TAG_SECURE_FAIL;
	        goto before_burn_end;
	     }
	  }

	  if( (ret=rtk_burn_fwdesc(prtkimgdesc, &boottable)) < 0) {
	     install_debug("burn boottable fail\r\n");
	     err_code = _eRTK_GENERAL_FAIL;
	     goto before_burn_end;
	  }
	}
	else
		return -1;

    otp_uninit();

	// execute postprocess.sh to do some actions after installation
    snprintf(path, sizeof(path), "%s", POSTPROCESS_TEMP);
    if(rtk_extract_file(prtkimgdesc, &prtkimgdesc->postprocess, POSTPROCESS_TEMP) < 0)
       install_fail("Can't extract %s\r\n", POSTPROCESS_TEMP);
	else {
		snprintf(cmd, sizeof(cmd), "chmod 777 %s", POSTPROCESS_TEMP);
      rtk_command(cmd, __LINE__, __FILE__, 0);

    	snprintf(cmd, sizeof(cmd), "%s", POSTPROCESS_TEMP);
    	if((ret = rtk_command(cmd , __LINE__, __FILE__, 0)) < 0)
       		install_fail("Can't execute %s\r\n", POSTPROCESS_TEMP);
	}

   //install factory
   if((prtkimgdesc->install_factory == 1) || (prtkimgdesc->kill_000 == 1)) {
      if(prtkimgdesc->start_customer == 1) {
         rtk_customer_write_burn_partname(prtkimgdesc, TAG_FACTORY);
         rtk_customer_write_increase_progressbar(prtkimgdesc, 10);
      }
	  boottable.tag = TAG_FACTORY;
        if(prtkimgdesc->flash_type == MTD_EMMC) {
            ret = rtk_install_factory(prtkimgdesc, false);
        } else {
            ret = rtk_install_factory(prtkimgdesc);  //original
        }

      install_debug("rtk_install_factory(%d)\r\n", ret);
      if (ret < 0)
      {
         err_code = _eRTK_BURN_FACTORY_FAIL;
         goto before_burn_end;
      }
   }

   boottable.boottype = BOOTTYPE_COMPLETE;
   boottable.tag = TAG_COMPLETE;
   // if backup is enabled, we use another partition for booting next time.
   if (prtkimgdesc->backup == 1) {
      if (prtkimgdesc->bootpart == 1)
         boottable.bootpart = 0;
      else
         boottable.bootpart = 1;
   }
before_burn_end:
   install_log("\r\n\r\nWrite boottable\r\n");
   install_debug("claire set fsync false this time");
   write_boottable(&boottable, prtkimgdesc->factory_start, prtkimgdesc->factory_size, prtkimgdesc,false);

   if (prtkimgdesc->safe_upgrade == 1) {
      // remove upgrade lock file when SSU is set
      rtk_create_remove_file_factory_mac(prtkimgdesc, UPGRADE_LOCK_FILENAME, REMOVE_FILE_FACTORY_MODE);
   }

   if(prtkimgdesc->install_factory == 1)
      rtk_flush_pingpong_factory_mac(prtkimgdesc);

burn_end_usb:
   if(prtkimgdesc->start_customer == 1) {
      rtk_customer_write_increase_progressbar(prtkimgdesc, 100);
      if (ret == 0)
         err_code = 0;
      rtk_customer_write_burn_result(prtkimgdesc, err_code);
   }

   return ret;
}

#ifdef __OFFLINE_GENERATE_BIN__

static int endian_swap(struct t_rtkimgdesc* prtkimgdesc)
{
   #define ENDIAN_SWAP_BUFFER_SIZE 1024

   FILE *fp_r,*fp_w;
   char r_buffer[ENDIAN_SWAP_BUFFER_SIZE] = {0};
   char w_buffer[ENDIAN_SWAP_BUFFER_SIZE] = {0};

   unsigned int fp_r_size = 0, bsize = 0;
   int i, j;

   install_debug("[endian swap]\r\n");

   if ((fp_r = fopen(DEFAULT_TEMP_OUTPUT, "rb")) == NULL) {
      install_fail("cannot open file:%s\r\n", DEFAULT_TEMP_OUTPUT);
      return -1;
   }

   if ((fp_w = fopen(DEFAULT_ENDIAN_SWAP_TEMP, "wb")) == NULL) {
      install_fail("cannot open file:%s\r\n", DEFAULT_ENDIAN_SWAP_TEMP);
      fclose(fp_r);
      return -1;
   }

   if (fseek(fp_r, 0 ,SEEK_END) < 0 ) {
      install_fail("%s seek error!\r\n", DEFAULT_TEMP_OUTPUT);
      fclose(fp_r);
      fclose(fp_w);
      return -1;
   }

   fp_r_size = ftell(fp_r);
   rewind(fp_r);

   bsize = fp_r_size / ENDIAN_SWAP_BUFFER_SIZE;

   for(i=0; i<bsize; i++)
   {
      fread(r_buffer, 1, ENDIAN_SWAP_BUFFER_SIZE, fp_r);
      for(j=0; j<(ENDIAN_SWAP_BUFFER_SIZE>>2); j++)
      {
         w_buffer[(j<<2)] = r_buffer[(j<<2)+3];
         w_buffer[(j<<2)+1] = r_buffer[(j<<2)+2];
         w_buffer[(j<<2)+2] = r_buffer[(j<<2)+1];
         w_buffer[(j<<2)+3] = r_buffer[(j<<2)];
      }
      fwrite(w_buffer, 1, ENDIAN_SWAP_BUFFER_SIZE, fp_w);
   }

   fclose(fp_r);
   fclose(fp_w);

   return 0;
}

int rtk_offline_generate_bin(struct t_rtkimgdesc* prtkimgdesc)
{
   int ret = 0;
   char cmd[128] = {0};
   S_BOOTTABLE boottable, *pbt = NULL;
	E_TAG current_tag;

   //install_test("rtk_offline_generate_bin()\r\n");

   //initialize setting.txt file path
   if (prtkimgdesc->setting_path == NULL) {
      snprintf(gsettingpath, sizeof(gsettingpath), "%s", DEFAULT_SETTING_FILE);
      install_log("open default setting file: %s\r\n", gsettingpath);
   }
   else {
      snprintf(gsettingpath, sizeof(gsettingpath), "%s", prtkimgdesc->setting_path);
   }
   //sanity-check
   if (access(gsettingpath, F_OK)) {
      install_fail("setting file not found!: %s\r\n", gsettingpath);
      return -1;
   }

   //check if install.img file exist
   if (access(prtkimgdesc->tarinfo.tarfile_path, F_OK)) {
      install_fail("install image not found!: %s\r\n", prtkimgdesc->tarinfo.tarfile_path);
      return -1;
   }

   //remove and make output dir
   snprintf(cmd, sizeof(cmd), "rm -rf %s; mkdir -p %s", DEFAULT_OUTPUT_DIR, DEFAULT_OUTPUT_DIR);
   if((ret = rtk_command(cmd, __LINE__, __FILE__, 0)) < 0) {
      return -1;
   }

   //remove and make tmp dir
   snprintf(cmd, sizeof(cmd), "rm -rf %s;mkdir -p %s ", PKG_TEMP, PKG_TEMP);
   if((ret = rtk_command(cmd, __LINE__, __FILE__, 0)) < 0) {
      return -1;
   }

   //parse tar file
   if((ret = parse_tar(prtkimgdesc)) < 0) {
      return ret;
   }

   // load configuration
   if((ret = fill_rtk_part_list_by_config(prtkimgdesc)) < 0) {
      return ret;
   }

   //override settings for offline generate bin
   prtkimgdesc->bootcode = 1;
   prtkimgdesc->update_etc = 1;
   prtkimgdesc->update_cavfile = 1;
   prtkimgdesc->only_factory = 0;
   prtkimgdesc->verify = 0;
	prtkimgdesc->start_customer = 0;
	prtkimgdesc->remove_temp_file_off = 0;

   // initial rtkimgdesc's meminfo
   if((ret = fill_rtkimgdesc_meminfo(prtkimgdesc)) < 0) {
      return ret;
   }

	// handle dynamic partition
	fill_rtkpartdesc_by_dynamicTbl(prtkimgdesc);


   // parsing tarfile
   if((ret = parse_tar(prtkimgdesc)) < 0) {
      return ret;
   }

   // fill rtkimgdesc's offset
   if((ret = fill_rtkimgdesc_layout(prtkimgdesc)) < 0) {
      return ret;
   }

   memset(&boottable, 0, sizeof(boottable));
   pbt = read_boottable(&boottable, prtkimgdesc);
	boottable.boottype = BOOTTYPE_UNKNOWN_BOOTTYPE;
   boottable.tag = TAG_UNKNOWN;
   sprintf(boottable.date, "%s", __DATE__);
   sprintf(boottable.time, "%s", __TIME__);

   install_info("\r\n\r\n");

   if (prtkimgdesc->flash_type == MTD_NANDFLASH)
   {
      //install bootcode
      if (prtkimgdesc->bootcode == 1) {
         if ((ret = rtk_burn_bootcode_mac_nand(prtkimgdesc)) < 0)
            return ret;
      }

      do {
         current_tag = boottable.tag;
         if ((ret = tagflow3(&boottable, prtkimgdesc, pbt)) < 0) {
            return ret;
         }
      } while(boottable.tag != TAG_COMPLETE);

      if(rtk_burn_fwdesc(prtkimgdesc) < 0) {
         install_debug("burn boottable fail\r\n");
         return -1;
      }
   }
   else if (prtkimgdesc->flash_type == MTD_NORFLASH)
   {
      //install bootcode
      if (prtkimgdesc->bootcode == 1) {
         if ((ret = rtk_burn_bootcode_mac_spi(prtkimgdesc)) < 0)
            return ret;
      }

      do {
         current_tag = boottable.tag;
         if ((ret = tagflow3(&boottable, prtkimgdesc, pbt)) < 0) {
            return ret;
         }
      } while(boottable.tag != TAG_COMPLETE);
   }
	else if (prtkimgdesc->flash_type == MTD_EMMC) {
      write_boottable_emmc(&boottable, prtkimgdesc->factory_start, prtkimgdesc->factory_size, prtkimgdesc);

      // mount and format emmc partition
      ret = rtk_mount_format_emmc(&boottable, prtkimgdesc);
      if (ret < 0) {
			return ret;
      }

      //install bootcode
      if (prtkimgdesc->bootcode == 1) {
         boottable.tag = TAG_BOOTCODE;
         ret = rtk_burn_bootcode_emmc(prtkimgdesc);
         if (ret < 0) {
				return ret;
         }
      }

		boottable.tag = TAG_UPDATE_ETC;

      do
      {
         current_tag = boottable.tag;

         ret = tagflow3_emmc(&boottable, prtkimgdesc, pbt);

         if(ret < 0) {
				return ret;
         }
      } while(boottable.tag != TAG_COMPLETE);

      if(rtk_burn_fwdesc(prtkimgdesc) < 0) {
         install_debug("burn boottable fail\r\n");
			return -1;
      }
	}
	else
		return -1;

   //install factory
   if (prtkimgdesc->install_factory == 1) {
      if ((ret = rtk_install_factory(prtkimgdesc)) < 0)
         return ret;
   }

   install_log("\r\n\r\nWrite boottable\r\n");
   boottable.boottype = BOOTTYPE_COMPLETE;
   write_boottable(&boottable, prtkimgdesc->factory_start, prtkimgdesc->factory_size, prtkimgdesc);
   install_log("Write tag %d done\r\n", boottable.tag);

   // flush factory ping pong and fix the latest block in the first block.
   rtk_flush_pingpong_factory_mac(prtkimgdesc);

   if(prtkimgdesc->flash_type == MTD_NANDFLASH) {
      // save factory and fill ecc to file
      ret = rtk_factory_to_virt_nand(prtkimgdesc);

      ret = final_programmer_process(prtkimgdesc);

      install_log("\r\n");
      ret = print_programmer_output_info(prtkimgdesc);
   }
   else if (prtkimgdesc->flash_type == MTD_NORFLASH){
      //endian swap
      if ((ret = endian_swap(prtkimgdesc)) < 0) {
         return ret;
      }

      if (prtkimgdesc->byte_swap_off == 0) {
         remove(DEFAULT_TEMP_OUTPUT);
         snprintf(cmd, sizeof(cmd), "mv %s %s", DEFAULT_ENDIAN_SWAP_TEMP, prtkimgdesc->output_path);
         if((ret = rtk_command(cmd, __LINE__, __FILE__, 0)) < 0) {
            return -1;
         }
      }
      else {
         snprintf(cmd, sizeof(cmd), "mv %s %s_noswap", DEFAULT_TEMP_OUTPUT, prtkimgdesc->output_path);
         if((ret = rtk_command(cmd, __LINE__, __FILE__, 0)) < 0) {
            return -1;
         }
         install_log(VT100_LIGHT_GREEN"\r\n%20s: %s_noswap\r\n"VT100_NONE, "TEST FILE", prtkimgdesc->output_path);

         snprintf(cmd, sizeof(cmd), "mv %s %s", DEFAULT_ENDIAN_SWAP_TEMP, prtkimgdesc->output_path);
         if((ret = rtk_command(cmd, __LINE__, __FILE__, 0)) < 0) {
            return -1;
         }
      }
      install_log("\r\n");
      install_log(VT100_LIGHT_GREEN"%20s: %s\r\n"VT100_NONE, "SMT FILE", prtkimgdesc->output_path);
      install_log(VT100_LIGHT_GREEN"%20s: %s\r\n"VT100_NONE, "PARTNAME", prtkimgdesc->flash_partname);
      install_log(VT100_LIGHT_GREEN"%20s: %u MBytes\r\n"VT100_NONE, "SIZE", prtkimgdesc->flash_size/1024/1024);
   }
	else if (prtkimgdesc->flash_type == MTD_EMMC) {
		if (prtkimgdesc->all_in_one)
			snprintf(cmd, sizeof(cmd), "mv %s %s", DEFAULT_TEMP_OUTPUT, prtkimgdesc->output_path);
		else
			snprintf(cmd, sizeof(cmd), "mv %s %s", DEFAULT_TEMP_OUTPUT, DEFAULT_FW_OUTPUT);
      if((ret = rtk_command(cmd, __LINE__, __FILE__, 0)) < 0)
        return -1;
		install_log("\r\n");
      install_log(VT100_LIGHT_GREEN"%20s: %s\r\n"VT100_NONE, "SMT FILE", prtkimgdesc->output_path);
      install_log(VT100_LIGHT_GREEN"%20s: %s\r\n"VT100_NONE, "PARTNAME", prtkimgdesc->flash_partname);
      install_log(VT100_LIGHT_GREEN"%20s: %u MBytes\r\n"VT100_NONE, "SIZE", prtkimgdesc->flash_size/1024/1024);
	}
	else
		return -1;

	if (prtkimgdesc->flash_type == MTD_EMMC && prtkimgdesc->all_in_one) {
		snprintf(cmd, sizeof(cmd), "cp ./../installer/install_a %s", DEFAULT_OUTPUT_DIR);
		rtk_command(cmd, __LINE__, __FILE__, 1);
		snprintf(cmd, sizeof(cmd), "cp ./../installer/customer %s", DEFAULT_OUTPUT_DIR);
		rtk_command(cmd, __LINE__, __FILE__, 1);
		snprintf(cmd, sizeof(cmd), "cp ./../installer/font.ttf %s", DEFAULT_OUTPUT_DIR);
		rtk_command(cmd, __LINE__, __FILE__, 1);
	}

   if (prtkimgdesc->remove_temp_file_off == 0) {
      snprintf(cmd, sizeof(cmd), "rm -rf %s/", PKG_TEMP);
      if((ret = rtk_command(cmd, __LINE__, __FILE__, 0)) < 0) {
         return -1;
      }
   }

   return ret;
}
#endif

