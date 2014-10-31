#include <string.h>

#include <rtk_common.h>
#include <rtk_burn.h>
#include <rtk_fwdesc.h>
#include <rtk_tagflow.h>
#include <rtk_imgdesc.h>
#include <rtk_boottable.h>
#include <rtk_mtd.h>
#include <rtk_customer.h>

extern u32 gDebugPrintfLogLevel;

int tagflow3(S_BOOTTABLE* pboottable, struct t_rtkimgdesc* prtkimgdesc, S_BOOTTABLE* pbt)
{
   int ret = 0, etc_index = 0;
   //char cmd[128] = {0};
   enum FWTYPE efwtype;

   switch(pboottable->tag) {
      case TAG_UNKNOWN:
         pboottable->tag = TAG_UPDATE_ETC;
         break;

      case TAG_UPDATE_ETC:
         if (prtkimgdesc->fw[FW_USR_LOCAL_ETC].img_size == 0) {
            install_info("There is no USR_LOCAL_ETC partition\n");
            pboottable->tag = TAG_UPDATE_NL;
            break;
         }

         if (prtkimgdesc->update_etc == 1) {
            // umount
            install_log("update_etc=y, start update etc\r\n");

#ifndef __OFFLINE_GENERATE_BIN__
            //snprintf(cmd, sizeof(cmd), "umount %s", prtkimgdesc->fw[FW_USR_LOCAL_ETC].mount_point);
            //rtk_command(cmd, __LINE__, __FILE__);
#endif
            if((ret = rtk_burn_single_part(prtkimgdesc, FW_USR_LOCAL_ETC)) == 0) {
               remove_part_by_partname(pboottable, prtkimgdesc->fw[FW_USR_LOCAL_ETC].part_name);
               add_part(pboottable, &prtkimgdesc->fw[FW_USR_LOCAL_ETC] );
            }
            else {
               install_fail("Burn ETC fail\r\n");
               return -_eRTK_TAG_UPDATE_ETC_FAIL;
            }
         }
         else {
            //check if the etc partition is already in boottable. if not, try to burn ETC.
            if ((etc_index = get_index_by_partname(pboottable, prtkimgdesc->fw[FW_USR_LOCAL_ETC].part_name)) < 0) {
               //etc partition is not in boottable.
               install_log("update_etc=n, but cannot find etc partition in boottable, start burn etc\r\n");
               // force update etc
               prtkimgdesc->update_etc = 1;
               if((ret = rtk_burn_single_part(prtkimgdesc, FW_USR_LOCAL_ETC)) == 0) {
                  remove_part_by_partname(pboottable, prtkimgdesc->fw[FW_USR_LOCAL_ETC].part_name);
                  add_part(pboottable, &prtkimgdesc->fw[FW_USR_LOCAL_ETC] );
               }
               else {
                  install_fail("Burn ETC fail\r\n");
                  return -_eRTK_TAG_UPDATE_ETC_FAIL;
               }
            }
            else if (((pboottable->part.list[etc_index].loc.offset != prtkimgdesc->fw[FW_USR_LOCAL_ETC].flash_offset) ||
               (pboottable->part.list[etc_index].loc.size != prtkimgdesc->fw[FW_USR_LOCAL_ETC].flash_allo_size)) &&
               prtkimgdesc->flash_type != MTD_NANDFLASH) {
               install_log("update_etc=n, move partition\r\n");
               if((ret = rtk_burn_single_part(prtkimgdesc, FW_USR_LOCAL_ETC)) == 0) {
                  remove_part_by_partname(pboottable, prtkimgdesc->fw[FW_USR_LOCAL_ETC].part_name);
                  add_part(pboottable, &prtkimgdesc->fw[FW_USR_LOCAL_ETC]);
               }
               else {
                  install_fail("Burn ETC fail\r\n");
                  return -_eRTK_TAG_UPDATE_ETC_FAIL;
               }
            }
            else {
               install_log("update_etc=n, skip update etc\r\n");
            }
         }
         pboottable->tag = TAG_UPDATE_NL;
         break;

      case TAG_UPDATE_NL:
         if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_KERNEL])) == 0) {
            update_fw(pboottable, FWTYPE_KERNEL, &prtkimgdesc->fw[FW_KERNEL]);
         }
         else {
            install_fail("Burn KERNEL fail\r\n");
            return -_eRTK_TAG_UPDATE_NL_FAIL;
         }
         pboottable->tag = TAG_UPDATE_RL_DT;
         break;

	  case TAG_UPDATE_RL_DT:
	  	 if( prtkimgdesc->install_dtb == 1 ) {
	         if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_RESCUE_DT])) == 0) {
	            update_fw(pboottable, FWTYPE_RESCUE_DT, &prtkimgdesc->fw[FW_RESCUE_DT]);
	         }
	         else {
	            install_fail("Burn RSCUE_DT fail\r\n");
	            return -_eRTK_TAG_UPDATE_RL_DT_FAIL;
	         }
	  	 }
         pboottable->tag = TAG_UPDATE_NL_DT;
         break;

	  case TAG_UPDATE_NL_DT:
	  	 if( prtkimgdesc->install_dtb == 1 ) {
	         if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_KERNEL_DT])) == 0) {
	            update_fw(pboottable, FWTYPE_KERNEL_DT, &prtkimgdesc->fw[FW_KERNEL_DT]);
	         }
	         else {
	            install_fail("Burn KERNEL_DT fail\r\n");
	            return -_eRTK_TAG_UPDATE_NL_DT_FAIL;
	         }
	  	 }
         pboottable->tag = TAG_UPDATE_RL_ROOTFS;
         break;

	  case TAG_UPDATE_RL_ROOTFS:
         if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_RESCUE_ROOTFS])) == 0) {
            update_fw(pboottable, FWTYPE_RESCUE_ROOTFS, &prtkimgdesc->fw[FW_RESCUE_ROOTFS]);
         }
         else {
            install_fail("Burn RESCUE_ROOTFS fail\r\n");
            return -_eRTK_TAG_UPDATE_RL_ROOTFS_FAIL;
         }
         pboottable->tag = TAG_UPDATE_NL_ROOTFS;
         break;

	  case TAG_UPDATE_NL_ROOTFS:
#ifdef NAS_ENABLE
		 if (prtkimgdesc->fw[FW_KERNEL_ROOTFS].img_size == 0) {
			install_info("There is no KERNEL_ROOTFS partition\n");
		 }
		 else
#endif
         {
         if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_KERNEL_ROOTFS])) == 0) {
            update_fw(pboottable, FWTYPE_KERNEL_ROOTFS, &prtkimgdesc->fw[FW_KERNEL_ROOTFS]);
         }
         else {
            install_fail("Burn KERNEL_ROOTFS fail\r\n");
            return -_eRTK_TAG_UPDATE_NL_ROOTFS_FAIL;
         }
         }
         pboottable->tag = TAG_UPDATE_AK;
         break;

      case TAG_UPDATE_AK:
#ifdef NAS_ENABLE
		 if (prtkimgdesc->fw[FW_AUDIO].img_size == 0) {
			install_info("There is no AUDIO FW\n");
		 }
		 else
#endif
         {
         if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_AUDIO])) == 0) {
            update_fw(pboottable, FWTYPE_AKERNEL, &prtkimgdesc->fw[FW_AUDIO]);
         }
         else {
            install_fail("Burn AUDIO FW fail\r\n");
            return -_eRTK_TAG_UPDATE_AK_FAIL;
         }
         }
         pboottable->tag = TAG_UPDATE_DALOGO;
         break;

      /*
      case TAG_UPDATE_VK:
         if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_VIDEO])) == 0) {
            update_fw(pboottable, FWTYPE_VKERNEL   \
            , prtkimgdesc->fw[FW_VIDEO].mem_offset \
            , prtkimgdesc->fw[FW_VIDEO].flash_offset \
            , prtkimgdesc->fw[FW_VIDEO].img_size \
            , prtkimgdesc->fw[FW_VIDEO].compress_type);
         }
         else {
            install_fail("Burn VIDEO FW fail\r\n");
            return -_eRTK_TAG_UPDATE_VK_FAIL;
         }
         pboottable->tag = TAG_UPDATE_DALOGO;
         break;
      */
      case TAG_UPDATE_DALOGO:
         if ( prtkimgdesc->fw[FW_AUDIO_BOOTFILE].img_size == 0 ) {
             install_info("There are no Audio bootfile and logo\n");
             update_fw(pboottable, FWTYPE_DALOGO, 0,0,0,NULL);
             pboottable->tag = TAG_UPDATE_DILOGO;
             break;
         }

         if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_AUDIO_BOOTFILE])) == 0) {
            update_fw(pboottable, FWTYPE_DALOGO, &prtkimgdesc->fw[FW_AUDIO_BOOTFILE]);
         }
         else {
            install_debug("Burn AUDIO LOGO fail\r\nSet DALOGO target, offset & size to 0\r\n");
            update_fw(pboottable, FWTYPE_DALOGO \
            , 0 \
            , 0 \
            , 0 \
            , NULL);
         }
#if 0
         if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_AUDIO_CLOGO1])) == 0) {
            update_fw(pboottable, FWTYPE_CALOGO1 \
            , prtkimgdesc->fw[FW_AUDIO_CLOGO1].mem_offset \
            , prtkimgdesc->fw[FW_AUDIO_CLOGO1].flash_offset \
            , prtkimgdesc->fw[FW_AUDIO_CLOGO1].img_size \
            , NULL);
         }
         else {
            install_debug("Burn AUDIO LOGO fail\r\nSet CALOGO1 target, offset & size to 0\r\n");
            update_fw(pboottable, FWTYPE_CALOGO1 \
            , 0 \
            , 0 \
            , 0 \
            , NULL);
         }

         if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_AUDIO_CLOGO2])) == 0) {
            update_fw(pboottable, FWTYPE_CALOGO2 \
            , prtkimgdesc->fw[FW_AUDIO_CLOGO2].mem_offset \
            , prtkimgdesc->fw[FW_AUDIO_CLOGO2].flash_offset \
            , prtkimgdesc->fw[FW_AUDIO_CLOGO2].img_size \
            , NULL);
         }
         else {
            install_debug("Burn AUDIO LOGO fail\r\nSet CALOGO2 target, offset & size to 0\r\n");
            update_fw(pboottable, FWTYPE_CALOGO2 \
            , 0 \
            , 0 \
            , 0 \
            , NULL);
         }
#endif
         pboottable->tag = TAG_UPDATE_DILOGO;
         break;

      case TAG_UPDATE_DILOGO:
         if ( prtkimgdesc->fw[FW_IMAGE_BOOTFILE].img_size == 0 ) {
             install_info("There are no Image bootfile\n");
             update_fw(pboottable, FWTYPE_DILOGO, 0,0,0,NULL);
             pboottable->tag = TAG_UPDATE_DVLOGO;
             break;
         }

         if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_IMAGE_BOOTFILE])) == 0) {
            update_fw(pboottable, FWTYPE_DILOGO, &prtkimgdesc->fw[FW_IMAGE_BOOTFILE]);
         }
         else {
            install_debug("Burn IMAGE LOGO fail\r\nSet DILOGO target, offset & size to 0\r\n");
            update_fw(pboottable, FWTYPE_DILOGO \
            , 0 \
            , 0 \
            , 0 \
            , NULL);
         }
         pboottable->tag = TAG_UPDATE_DVLOGO;
         break;

      case TAG_UPDATE_DVLOGO:
         if ( prtkimgdesc->fw[FW_VIDEO_BOOTFILE].img_size == 0 ) {
            install_info("There are no Video bootfile and logo\n");
            update_fw(pboottable, FWTYPE_DVLOGO,0,0,0,NULL);
#ifdef NAS_ENABLE
            pboottable->tag = TAG_UPDATE_ROOTFS;
#else
            pboottable->tag = TAG_UPDATE_RES;
#endif
            break;
         }

         if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_VIDEO_BOOTFILE])) == 0) {
            update_fw(pboottable, FWTYPE_DVLOGO, &prtkimgdesc->fw[FW_VIDEO_BOOTFILE]);
         }
         else {
            install_debug("Burn VIDEO LOGO fail\r\nSet DVLOGO target, offset & size to 0\r\n");
            update_fw(pboottable, FWTYPE_DVLOGO \
            , 0 \
            , 0 \
            , 0 \
            , NULL);
         }
#if 0
         if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_VIDEO_CLOGO1])) == 0) {
            update_fw(pboottable, FWTYPE_CVLOGO1 \
            , prtkimgdesc->fw[FW_VIDEO_CLOGO1].mem_offset \
            , prtkimgdesc->fw[FW_VIDEO_CLOGO1].flash_offset \
            , prtkimgdesc->fw[FW_VIDEO_CLOGO1].img_size \
            , NULL);
         }
         else {
            install_debug("Burn VIDEO LOGO fail\r\nSet CVLOGO1 target, offset & size to 0\r\n");
            update_fw(pboottable, FWTYPE_CVLOGO1 \
            , 0 \
            , 0 \
            , 0 \
            , NULL);
         }

         if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_VIDEO_CLOGO2])) == 0) {
            update_fw(pboottable, FWTYPE_CVLOGO2 \
            , prtkimgdesc->fw[FW_VIDEO_CLOGO2].mem_offset \
            , prtkimgdesc->fw[FW_VIDEO_CLOGO2].flash_offset \
            , prtkimgdesc->fw[FW_VIDEO_CLOGO2].img_size \
            , NULL);
         }
         else {
            install_debug("Burn VIDEO LOGO fail\r\nSet CVLOGO2 target, offset & size to 0\r\n");
            update_fw(pboottable, FWTYPE_CVLOGO2 \
            , 0 \
            , 0 \
            , 0 \
            , NULL);
         }
#endif
#ifdef NAS_ENABLE
         pboottable->tag = TAG_UPDATE_ROOTFS;
#else
         pboottable->tag = TAG_UPDATE_RES;
#endif
         break;

#ifdef NAS_ENABLE
      case TAG_UPDATE_ROOTFS:
		 if (prtkimgdesc->fw[FW_ROOTFS].img_size == 0) {
			install_info("There is no ROOTFS partition\n");
		 }
		 else {
         	if ((ret = rtk_burn_single_part(prtkimgdesc, FW_ROOTFS)) == 0) {
            	remove_part_by_partname(pboottable, prtkimgdesc->fw[FW_ROOTFS].part_name);
            	add_part(pboottable  \
            	, prtkimgdesc->fw[FW_ROOTFS].part_name \
            	, prtkimgdesc->fw[FW_ROOTFS].mount_point   \
            	, prtkimgdesc->fw[FW_ROOTFS].dev_path   \
            	, string_inv_to_efs(prtkimgdesc->fw[FW_ROOTFS].fs_name)  \
            	, prtkimgdesc->fw[FW_ROOTFS].flash_offset   \
            	, prtkimgdesc->fw[FW_ROOTFS].flash_allo_size
            	, prtkimgdesc->fw[FW_ROOTFS].compress_type);
         	}
         	else {
            	install_fail("Burn ROOTFS fail\r\n");
         	}
		 }
         pboottable->tag = TAG_UPDATE_RES;
         break;
#endif

	  case TAG_UPDATE_RES:
		if (prtkimgdesc->fw[FW_RES].img_size == 0) {
			install_info("There is no RES partition\n");
			pboottable->tag = TAG_UPDATE_CACHE;
			break;
		}

		 if ((ret = rtk_burn_single_part(prtkimgdesc, FW_RES)) == 0) {
			remove_part_by_partname(pboottable, prtkimgdesc->fw[FW_RES].part_name);
			add_part(pboottable, &prtkimgdesc->fw[FW_RES]);
		 }
		 else {
			install_fail("Burn RES fail\r\n");
			return -_eRTK_TAG_UPDATE_PARTITION_FAIL;
		 }
		 pboottable->tag = TAG_UPDATE_CACHE;
		 break;

	  case TAG_UPDATE_CACHE:
#ifdef NAS_ENABLE
		if (prtkimgdesc->fw[FW_CACHE].img_size == 0) {
			install_info("There is no CACHE partition\n");
			pboottable->tag = TAG_UPDATE_DATA;
			break;
		}
#endif
		 if ((ret = rtk_burn_single_part(prtkimgdesc, FW_CACHE)) == 0) {
			remove_part_by_partname(pboottable, prtkimgdesc->fw[FW_CACHE].part_name);
			add_part(pboottable, &prtkimgdesc->fw[FW_CACHE]);
		 }
		 else {
			install_fail("Burn CACHE fail\r\n");
			return -_eRTK_TAG_UPDATE_PARTITION_FAIL;
		 }
		 pboottable->tag = TAG_UPDATE_DATA;
		 break;

	  case TAG_UPDATE_DATA:
#ifdef NAS_ENABLE
		if (prtkimgdesc->fw[FW_DATA].img_size == 0) {
			install_info("There is no DATA partition\n");
			pboottable->tag = TAG_UPDATE_SYSTEM;
			break;
		}
#endif
		 if ((ret = rtk_burn_single_part(prtkimgdesc, FW_DATA)) == 0) {
			remove_part_by_partname(pboottable, prtkimgdesc->fw[FW_DATA].part_name);
			add_part(pboottable, &prtkimgdesc->fw[FW_DATA]);
		 }
		 else {
			install_fail("Burn DATA fail\r\n");
			return -_eRTK_TAG_UPDATE_PARTITION_FAIL;
		 }
		 pboottable->tag = TAG_UPDATE_SYSTEM;
		 break;

	  case TAG_UPDATE_SYSTEM:
#ifdef NAS_ENABLE
		if (prtkimgdesc->fw[FW_SYSTEM].img_size == 0) {
			install_info("There is no SYSTEM partition\n");
			pboottable->tag = TAG_UPDATE_OTHER_PARTITION1;
			break;
		}
#endif
		 if ((ret = rtk_burn_single_part(prtkimgdesc, FW_SYSTEM)) == 0) {
			remove_part_by_partname(pboottable, prtkimgdesc->fw[FW_SYSTEM].part_name);
			add_part(pboottable, &prtkimgdesc->fw[FW_SYSTEM]);
		 }
		 else {
			install_fail("Burn SYSTEM fail\r\n");
			return -_eRTK_TAG_UPDATE_PARTITION_FAIL;
		 }
		 pboottable->tag = TAG_UPDATE_OTHER_PARTITION1;
		 break;

      case TAG_UPDATE_OTHER_PARTITION1:
         for (efwtype = P_PARTITION1; efwtype <= P_PARTITION8; efwtype=FWTYPE(efwtype+1)) {
            if (prtkimgdesc->fw[efwtype].flash_allo_size != 0 && prtkimgdesc->fw[efwtype].img_size!=0 && strlen(prtkimgdesc->fw[efwtype].part_name) != 0) {

	            /*
	            if (gDebugPrintfLogLevel&INSTALL_MEM_LEVEL)
	               system("cat /proc/meminfo");
	            */
	            if ((ret = rtk_burn_single_part(prtkimgdesc, efwtype)) == 0) {
	               add_part(pboottable, &prtkimgdesc->fw[efwtype]);
	            } else {
	               install_fail("Burn Partition(%s) fail\r\n", prtkimgdesc->fw[efwtype].part_name );
	               return -_eRTK_TAG_UPDATE_PARTITION_FAIL;
	            }
            }
         }

#if 0
         if (prtkimgdesc->fw[FW_AUDIO_CLOGO1].flash_allo_size != 0)
            update_fw(pboottable, FWTYPE_CALOGO1 \
            , prtkimgdesc->fw[FW_AUDIO_CLOGO1].mem_offset \
            , prtkimgdesc->fw[FW_AUDIO_CLOGO1].flash_offset \
            , prtkimgdesc->fw[FW_AUDIO_CLOGO1].flash_allo_size\
            , NULL);
         else
            update_fw(pboottable, FWTYPE_CALOGO1 \
            , 0 \
            , 0 \
            , 0 \
            , NULL);

         if (prtkimgdesc->fw[FW_VIDEO_CLOGO1].flash_allo_size != 0)
            update_fw(pboottable, FWTYPE_CVLOGO1 \
            , prtkimgdesc->fw[FW_VIDEO_CLOGO1].mem_offset \
            , prtkimgdesc->fw[FW_VIDEO_CLOGO1].flash_offset \
            , prtkimgdesc->fw[FW_VIDEO_CLOGO1].flash_allo_size\
            , NULL);
         else
            update_fw(pboottable, FWTYPE_CVLOGO1 \
            , 0 \
            , 0 \
            , 0 \
            , NULL);

         if (prtkimgdesc->fw[FW_AUDIO_CLOGO2].flash_allo_size != 0)
            update_fw(pboottable, FWTYPE_CALOGO2 \
            , prtkimgdesc->fw[FW_AUDIO_CLOGO2].mem_offset \
            , prtkimgdesc->fw[FW_AUDIO_CLOGO2].flash_offset \
            , prtkimgdesc->fw[FW_AUDIO_CLOGO2].flash_allo_size\
            , NULL);
         else
            update_fw(pboottable, FWTYPE_CALOGO2 \
            , 0 \
            , 0 \
            , 0 \
            , NULL);

         if (prtkimgdesc->fw[FW_VIDEO_CLOGO2].flash_allo_size != 0)
            update_fw(pboottable, FWTYPE_CVLOGO2 \
            , prtkimgdesc->fw[FW_VIDEO_CLOGO2].mem_offset \
            , prtkimgdesc->fw[FW_VIDEO_CLOGO2].flash_offset \
            , prtkimgdesc->fw[FW_VIDEO_CLOGO2].flash_allo_size\
            , NULL);
         else
            update_fw(pboottable, FWTYPE_CVLOGO2 \
            , 0 \
            , 0 \
            , 0 \
            , NULL);
#endif
         //update SSUWORKPART
         update_ssu_work_part(pboottable, prtkimgdesc->next_ssu_work_part);

         pboottable->tag = TAG_COMPLETE;
         pboottable->boottype = BOOTTYPE_COMPLETE;
         break;

      case TAG_COMPLETE:
         pboottable->tag = TAG_UPDATE_ETC;
         break;

      default:
         install_test("Error! should not happened TAG:%d\r\n", pboottable->tag);
   }

   return _eRTK_SUCCESS;
}

#ifdef EMMC_SUPPORT
int tagflow3_emmc(S_BOOTTABLE* pboottable, struct t_rtkimgdesc* prtkimgdesc, S_BOOTTABLE* pbt)
{
   int ret = 0;
   //char cmd[128] = {0};
   enum FWTYPE efwtype;

   switch(pboottable->tag) {
      case TAG_UNKNOWN:
         pboottable->tag = TAG_UPDATE_ETC;
         break;

      case TAG_UPDATE_ETC:
		if (prtkimgdesc->fw[FW_USR_LOCAL_ETC].img_size == 0) {
			install_info("There is no USR_LOCAL_ETC partition\n");
		}
		else {
      		if((ret = rtk_burn_single_part(prtkimgdesc, FW_USR_LOCAL_ETC)) == 0) {
        		remove_part_by_partname(pboottable, prtkimgdesc->fw[FW_USR_LOCAL_ETC].part_name);
            	add_part(pboottable, &prtkimgdesc->fw[FW_USR_LOCAL_ETC]);
        	}
        	else {
        		install_fail("Burn ETC fail\r\n");
        	}
		}
        pboottable->tag = TAG_UPDATE_NL;
        break;
      case TAG_UPDATE_NL:
         if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_KERNEL])) == 0) {
            update_fw(pboottable, FWTYPE_KERNEL, &prtkimgdesc->fw[FW_KERNEL]);
         }
         else {
            install_fail("Burn KERNEL fail\r\n");
            return -_eRTK_TAG_UPDATE_NL_FAIL;
         }
         pboottable->tag = TAG_UPDATE_RL_DT;
         break;

      case TAG_UPDATE_RL_DT:
	  	 if( prtkimgdesc->install_dtb == 1 ) {
	         if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_RESCUE_DT])) == 0) {
	            update_fw(pboottable, FWTYPE_RESCUE_DT, &prtkimgdesc->fw[FW_RESCUE_DT]);
	         }
	         else {
	            install_fail("Burn RSCUE_DT fail\r\n");
	            return -_eRTK_TAG_UPDATE_RL_DT_FAIL;
	         }
	  	 }
         pboottable->tag = TAG_UPDATE_NL_DT;
         break;

      case TAG_UPDATE_NL_DT:
 	  	 if( prtkimgdesc->install_dtb == 1 ) {
	         if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_KERNEL_DT])) == 0) {
	            update_fw(pboottable, FWTYPE_KERNEL_DT, &prtkimgdesc->fw[FW_KERNEL_DT]);
	         }
	         else {
	            install_fail("Burn KERNEL_DT fail\r\n");
	            return -_eRTK_TAG_UPDATE_NL_DT_FAIL;
	         }
 	  	 }
         pboottable->tag = TAG_UPDATE_RL_ROOTFS;
         break;

      case TAG_UPDATE_RL_ROOTFS:
         if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_RESCUE_ROOTFS])) == 0) {
            update_fw(pboottable, FWTYPE_RESCUE_ROOTFS, &prtkimgdesc->fw[FW_RESCUE_ROOTFS]);
         }
         else {
            install_fail("Burn RESCUE_ROOTFS fail\r\n");
            return -_eRTK_TAG_UPDATE_RL_ROOTFS_FAIL;
         }
         pboottable->tag = TAG_UPDATE_NL_ROOTFS;
         break;

      case TAG_UPDATE_NL_ROOTFS:
#ifdef NAS_ENABLE
		 if (prtkimgdesc->fw[FW_KERNEL_ROOTFS].img_size == 0) {
			install_info("There is no KERNEL_ROOTFS partition\n");
		 }
		 else
#endif
         {
         if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_KERNEL_ROOTFS])) == 0) {
            update_fw(pboottable, FWTYPE_KERNEL_ROOTFS, &prtkimgdesc->fw[FW_KERNEL_ROOTFS]);
         }
         else {
            install_fail("Burn KERNEL_ROOTFS fail\r\n");
            return -_eRTK_TAG_UPDATE_NL_ROOTFS_FAIL;
         }
         }
         pboottable->tag = TAG_UPDATE_AK;
         break;

      case TAG_UPDATE_AK:
#ifdef NAS_ENABLE
		 if (prtkimgdesc->fw[FW_AUDIO].img_size == 0) {
			install_info("There is no AUDIO FW\n");
		 }
		 else
#endif
         {
         if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_AUDIO])) == 0) {
            update_fw(pboottable, FWTYPE_AKERNEL, &prtkimgdesc->fw[FW_AUDIO]);
         }
         else {
            install_fail("Burn AUDIO FW fail\r\n");
            return -_eRTK_TAG_UPDATE_AK_FAIL;
         }
         }
         pboottable->tag = TAG_UPDATE_DALOGO;
         break;

#if 0
      case TAG_UPDATE_VK:
         if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_VIDEO])) == 0) {
            update_fw(pboottable, FWTYPE_VKERNEL   \
            , prtkimgdesc->fw[FW_VIDEO].mem_offset \
            , prtkimgdesc->fw[FW_VIDEO].flash_offset \
            , prtkimgdesc->fw[FW_VIDEO].img_size \
            , prtkimgdesc->fw[FW_VIDEO].compress_type);
         }
         else {
            install_fail("Burn VIDEO FW fail\r\n");
            return -_eRTK_TAG_UPDATE_VK_FAIL;
         }
         pboottable->tag = TAG_UPDATE_VK2;
         break;
	  case TAG_UPDATE_VK2:
		 if (prtkimgdesc->fw[FW_VIDEO2].img_size == 0) {
			install_info("There is no video2 FW\n");
		 }
		 else {
         	if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_VIDEO2])) == 0) {
            	update_fw(pboottable, FWTYPE_VKERNEL2   \
            	, prtkimgdesc->fw[FW_VIDEO2].mem_offset \
            	, prtkimgdesc->fw[FW_VIDEO2].flash_offset \
            	, prtkimgdesc->fw[FW_VIDEO2].img_size \
            	, prtkimgdesc->fw[FW_VIDEO2].compress_type);
         	}
         	else {
            	install_fail("Burn VIDEO2 FW fail\r\n");
         	}
		 }
         pboottable->tag = TAG_UPDATE_ECPUK;
         break;
	  case TAG_UPDATE_ECPUK:
		 if (prtkimgdesc->fw[FW_ECPU].img_size == 0) {
			install_info("There is no ECPU FW\n");
		 }
		 else {
         	if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_ECPU])) == 0) {
            	update_fw(pboottable, FWTYPE_ECPU   \
            	, prtkimgdesc->fw[FW_ECPU].mem_offset \
            	, prtkimgdesc->fw[FW_ECPU].flash_offset \
            	, prtkimgdesc->fw[FW_ECPU].img_size \
            	, prtkimgdesc->fw[FW_ECPU].compress_type);
         	}
         	else {
            	install_fail("Burn ECPU FW fail\r\n");
         	}
		 }
         pboottable->tag = TAG_UPDATE_DALOGO;
         break;
#endif
      case TAG_UPDATE_DALOGO:
		 if ((prtkimgdesc->fw[FW_AUDIO_BOOTFILE].img_size == 0) &&
				(prtkimgdesc->fw[FW_AUDIO_CLOGO1].img_size == 0) &&
				(prtkimgdesc->fw[FW_AUDIO_CLOGO1].img_size == 0)) {
			install_info("There are no Audio bootfile and logo\n");
			update_fw(pboottable, FWTYPE_DALOGO, 0,0,0,NULL);
		 }
		 else {
         	if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_AUDIO_BOOTFILE])) == 0) {
            	update_fw(pboottable, FWTYPE_DALOGO, &prtkimgdesc->fw[FW_AUDIO_BOOTFILE]);
         	}
         	else {
            	install_debug("Burn AUDIO LOGO fail\r\nSet DALOGO target, offset & size to 0\r\n");
            	update_fw(pboottable, FWTYPE_DALOGO \
            	, 0 \
            	, 0 \
            	, 0 \
            	, NULL);
         	}
#if 0
         	if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_AUDIO_CLOGO1])) == 0) {
            	update_fw(pboottable, FWTYPE_CALOGO1 \
            	, prtkimgdesc->fw[FW_AUDIO_CLOGO1].mem_offset \
            	, prtkimgdesc->fw[FW_AUDIO_CLOGO1].flash_offset \
            	, prtkimgdesc->fw[FW_AUDIO_CLOGO1].img_size \
            	, NULL);
         	}
         	else {
            	install_debug("Burn AUDIO LOGO fail\r\nSet CALOGO1 target, offset & size to 0\r\n");
            	update_fw(pboottable, FWTYPE_CALOGO1 \
            	, 0 \
            	, 0 \
            	, 0 \
            	, NULL);
         	}

         	if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_AUDIO_CLOGO2])) == 0) {
            	update_fw(pboottable, FWTYPE_CALOGO2 \
            	, prtkimgdesc->fw[FW_AUDIO_CLOGO2].mem_offset \
            	, prtkimgdesc->fw[FW_AUDIO_CLOGO2].flash_offset \
            	, prtkimgdesc->fw[FW_AUDIO_CLOGO2].img_size \
            	, NULL);
         	}
         	else {
            	install_debug("Burn AUDIO LOGO fail\r\nSet CALOGO2 target, offset & size to 0\r\n");
            	update_fw(pboottable, FWTYPE_CALOGO2 \
            	, 0 \
            	, 0 \
            	, 0 \
            	, NULL);
         	}
#endif
		 }
         pboottable->tag = TAG_UPDATE_DILOGO;
         break;

      case TAG_UPDATE_DILOGO:
         if ( prtkimgdesc->fw[FW_IMAGE_BOOTFILE].img_size == 0 ) {
             install_info("There are no Image bootfile\n");
             update_fw(pboottable, FWTYPE_DILOGO, 0,0,0,NULL);
             pboottable->tag = TAG_UPDATE_DVLOGO;
             break;
         }

         if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_IMAGE_BOOTFILE])) == 0) {
            update_fw(pboottable, FWTYPE_DILOGO, &prtkimgdesc->fw[FW_IMAGE_BOOTFILE]);
         }
         else {
            install_debug("Burn IMAGE LOGO fail\r\nSet DILOGO target, offset & size to 0\r\n");
            update_fw(pboottable, FWTYPE_DILOGO \
            , 0 \
            , 0 \
            , 0 \
            , NULL);
         }
         pboottable->tag = TAG_UPDATE_DVLOGO;
         break;

      case TAG_UPDATE_DVLOGO:
		 if ((prtkimgdesc->fw[FW_VIDEO_BOOTFILE].img_size == 0) &&
                (prtkimgdesc->fw[FW_VIDEO_CLOGO1].img_size == 0) &&
                (prtkimgdesc->fw[FW_VIDEO_CLOGO2].img_size == 0)) {
            install_info("There are no Video bootfile and logo\n");
            update_fw(pboottable, FWTYPE_DVLOGO, 0,0,0,NULL);
         }
         else {
         	if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_VIDEO_BOOTFILE])) == 0) {
            	update_fw(pboottable, FWTYPE_DVLOGO, &prtkimgdesc->fw[FW_VIDEO_BOOTFILE]);
         	}
         	else {
            	install_debug("Burn VIDEO LOGO fail\r\nSet DVLOGO target, offset & size to 0\r\n");
            	update_fw(pboottable, FWTYPE_DVLOGO \
            	, 0 \
            	, 0 \
            	, 0 \
            	, NULL);
         	}
#if 0
         	if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_VIDEO_CLOGO1])) == 0) {
            	update_fw(pboottable, FWTYPE_CVLOGO1 \
            	, prtkimgdesc->fw[FW_VIDEO_CLOGO1].mem_offset \
            	, prtkimgdesc->fw[FW_VIDEO_CLOGO1].flash_offset \
            	, prtkimgdesc->fw[FW_VIDEO_CLOGO1].img_size \
            	, NULL);
         	}
         	else {
            	install_debug("Burn VIDEO LOGO fail\r\nSet CVLOGO1 target, offset & size to 0\r\n");
            	update_fw(pboottable, FWTYPE_CVLOGO1 \
            	, 0 \
            	, 0 \
            	, 0 \
            	, NULL);
         	}

         	if ((ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[FW_VIDEO_CLOGO2])) == 0) {
            	update_fw(pboottable, FWTYPE_CVLOGO2 \
            	, prtkimgdesc->fw[FW_VIDEO_CLOGO2].mem_offset \
            	, prtkimgdesc->fw[FW_VIDEO_CLOGO2].flash_offset \
            	, prtkimgdesc->fw[FW_VIDEO_CLOGO2].img_size \
            	, NULL);
         	}
         	else {
            	install_debug("Burn VIDEO LOGO fail\r\nSet CVLOGO2 target, offset & size to 0\r\n");
            	update_fw(pboottable, FWTYPE_CVLOGO2 \
            	, 0 \
            	, 0 \
            	, 0 \
            	, NULL);
         	}
#endif
		 }
#ifdef NAS_ENABLE
         pboottable->tag = TAG_UPDATE_ROOTFS;
#else
         pboottable->tag = TAG_UPDATE_RES;
#endif
         break;

#ifdef NAS_ENABLE
      case TAG_UPDATE_ROOTFS:
		 if (prtkimgdesc->fw[FW_ROOTFS].img_size == 0) {
			install_info("There is no ROOTFS partition\n");
		 }
		 else {
         	if ((ret = rtk_burn_single_part(prtkimgdesc, FW_ROOTFS)) == 0) {
            	remove_part_by_partname(pboottable, prtkimgdesc->fw[FW_ROOTFS].part_name);
            	add_part(pboottable  \
            	, prtkimgdesc->fw[FW_ROOTFS].part_name \
            	, prtkimgdesc->fw[FW_ROOTFS].mount_point   \
            	, prtkimgdesc->fw[FW_ROOTFS].dev_path   \
            	, string_inv_to_efs(prtkimgdesc->fw[FW_ROOTFS].fs_name)  \
            	, prtkimgdesc->fw[FW_ROOTFS].flash_offset   \
            	, prtkimgdesc->fw[FW_ROOTFS].flash_allo_size
            	, prtkimgdesc->fw[FW_ROOTFS].compress_type);
         	}
         	else {
            	install_fail("Burn ROOTFS fail\r\n");
         	}
		 }
         pboottable->tag = TAG_UPDATE_RES;
         break;
#endif

      case TAG_UPDATE_RES:
        if (prtkimgdesc->fw[FW_RES].img_size == 0) {
            install_info("There is no RES partition\n");
            pboottable->tag = TAG_UPDATE_CACHE;
            break;
        }

         if ((ret = rtk_burn_single_part(prtkimgdesc, FW_RES)) == 0) {
            remove_part_by_partname(pboottable, prtkimgdesc->fw[FW_RES].part_name);
            add_part(pboottable, &prtkimgdesc->fw[FW_RES]);
         }
         else {
            install_fail("Burn RES fail\r\n");
            return -_eRTK_TAG_UPDATE_PARTITION_FAIL;
         }
         pboottable->tag = TAG_UPDATE_CACHE;
         break;

      case TAG_UPDATE_CACHE:
#ifdef NAS_ENABLE
		if (prtkimgdesc->fw[FW_CACHE].img_size == 0) {
			install_info("There is no CACHE partition\n");
			pboottable->tag = TAG_UPDATE_DATA;
			break;
		}
#endif
         if ((ret = rtk_burn_single_part(prtkimgdesc, FW_CACHE)) == 0) {
            remove_part_by_partname(pboottable, prtkimgdesc->fw[FW_CACHE].part_name);
            add_part(pboottable, &prtkimgdesc->fw[FW_CACHE]);
         }
         else {
            install_fail("Burn CACHE fail\r\n");
            return -_eRTK_TAG_UPDATE_PARTITION_FAIL;
         }
         pboottable->tag = TAG_UPDATE_DATA;
         break;

      case TAG_UPDATE_DATA:
#ifdef NAS_ENABLE
		if (prtkimgdesc->fw[FW_DATA].img_size == 0) {
			install_info("There is no DATA partition\n");
			pboottable->tag = TAG_UPDATE_SYSTEM;
			break;
		}
#endif
         if ((ret = rtk_burn_single_part(prtkimgdesc, FW_DATA)) == 0) {
            remove_part_by_partname(pboottable, prtkimgdesc->fw[FW_DATA].part_name);
            add_part(pboottable, &prtkimgdesc->fw[FW_DATA]);
         }
         else {
            install_fail("Burn DATA fail\r\n");
            return -_eRTK_TAG_UPDATE_PARTITION_FAIL;
         }
         pboottable->tag = TAG_UPDATE_SYSTEM;
         break;

      case TAG_UPDATE_SYSTEM:
#ifdef NAS_ENABLE
		if (prtkimgdesc->fw[FW_SYSTEM].img_size == 0) {
			install_info("There is no SYSTEM partition\n");
			pboottable->tag = TAG_UPDATE_OTHER_PARTITION1;
			break;
		}
#endif
         if ((ret = rtk_burn_single_part(prtkimgdesc, FW_SYSTEM)) == 0) {
            remove_part_by_partname(pboottable, prtkimgdesc->fw[FW_SYSTEM].part_name);
            add_part(pboottable, &prtkimgdesc->fw[FW_SYSTEM]);
         }
         else {
            install_fail("Burn SYSTEM fail\r\n");
            return -_eRTK_TAG_UPDATE_PARTITION_FAIL;
         }
         pboottable->tag = TAG_UPDATE_OTHER_PARTITION1;
         break;

      case TAG_UPDATE_OTHER_PARTITION1:
         for (efwtype = P_PARTITION1; efwtype <= P_PARTITION8; efwtype=FWTYPE(efwtype+1)) {
            if (prtkimgdesc->fw[efwtype].flash_allo_size != 0 && prtkimgdesc->fw[efwtype].img_size!=0 && strlen(prtkimgdesc->fw[efwtype].part_name) != 0) {

				if (gDebugPrintfLogLevel&INSTALL_MEM_LEVEL)
					system("cat /proc/meminfo");

            	if ((ret = rtk_burn_single_part(prtkimgdesc, efwtype)) == 0) {
               		add_part(pboottable, &prtkimgdesc->fw[efwtype]);
            		} else {
		            	install_fail("Burn Partition(%s) fail\r\n", prtkimgdesc->fw[efwtype].part_name );
					return -_eRTK_TAG_UPDATE_PARTITION_FAIL;
            	}
			}
         }

         //update SSUWORKPART
         update_ssu_work_part(pboottable, prtkimgdesc->next_ssu_work_part);

         pboottable->tag = TAG_COMPLETE;
         pboottable->boottype = BOOTTYPE_COMPLETE;
         break;

      case TAG_COMPLETE:
         pboottable->tag = TAG_UPDATE_ETC;
         break;

      default:
         install_test("Error! should not happened TAG:%d\r\n", pboottable->tag);
   }

   return _eRTK_SUCCESS;
}
#endif //EMMC_SUPPORT
