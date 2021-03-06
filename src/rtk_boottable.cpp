#include <string.h>


#include <rtk_boottable.h>
#include <rtk_common.h>
#include <rtk_factory.h>
#include <rtk_mtd.h>
#include <rtk_def.h>
#include <rtk_config.h>

extern struct t_PARTDESC rtk_part_list[NUM_RTKPART];

#ifdef EMMC_SUPPORT
S_BOOTTABLE* read_boottable_emmc(S_BOOTTABLE* boottable, struct t_rtkimgdesc* prtkimg)
{
#if (0)
	struct S_MBR_TABLE mbr;
    int size;
    FILE* pfile = NULL;
#endif

	boottable->mbr_matched = 0;
#if (0)	//Chuck TODO, now we always redo partition
	rtk_get_mbr(0);
    pfile = fopen(MBR_FILE_TEMP,"rb");
    if(pfile == NULL)
    {
    	install_log("Can't open %s fail\r\n", MBR_FILE_TEMP);
      	return boottable;
   	}

   	install_log("parsing %s\r\n", MBR_FILE_TEMP);
	size = fread(&mbr, 1, sizeof(mbr), pfile);
	if (size != sizeof(mbr)) {
		fclose(pfile);
		return boottable;
	}

	if (mbr.signature != 0xaa55) {
		install_log("MBR is invalid\r\n");
		fclose(pfile);
		return boottable;
	}
	if (((mbr.part[0].rel_sector_start == 131008) && (mbr.part[0].sector_num == 1802304)) &&
		((mbr.part[1].rel_sector_start == 1933312) && (mbr.part[1].sector_num == 1933312)) &&
		((mbr.part[2].rel_sector_start == 3866624) && (mbr.part[2].sector_num == 1933312)) &&
		((mbr.part[3].rel_sector_start == 5799936) && (mbr.part[3].sector_num == 1933312))) {
		boottable->mbr_matched = 1;
		install_log("MBR is exactly the same. Skip re-partition.\r\n");
	}

	printf("mbr part %d at %d size %d\n", mbr.part[0].id, mbr.part[0].rel_sector_start, mbr.part[0].sector_num);
	printf("mbr part %d at %d size %d\n", mbr.part[1].id, mbr.part[1].rel_sector_start, mbr.part[1].sector_num);
	printf("mbr part %d at %d size %d\n", mbr.part[2].id, mbr.part[2].rel_sector_start, mbr.part[2].sector_num);
	printf("mbr part %d at %d size %d\n", mbr.part[3].id, mbr.part[3].rel_sector_start, mbr.part[3].sector_num);

	fclose(pfile);
    install_debug("read boottable success\r\n");
#endif
    return boottable;
}


#ifdef __OFFLINE_GENERATE_BIN__
static int rtk_set_mbr_offline(unsigned int sector, char *table, struct t_rtkimgdesc* prtkimg)
{
	unsigned long long offset =  (unsigned long long)sector*512;
	enum FWTYPE efwtype;
	char tmp_file_name[128];
	unsigned long long tmp_part_size;

	for(efwtype=FW_ROOTFS; efwtype<=FW_USR_LOCAL_ETC; efwtype=FWTYPE(efwtype+1)) {
	   	if(prtkimg->fw[efwtype].flash_allo_size == 0) {
      		continue;
	   	}
		if (prtkimg->fw[efwtype].flash_offset == offset) {
			sprintf(tmp_file_name, "%s/%s.ext4img", DEFAULT_OUTPUT_DIR, inv_by_fwtype(efwtype));
			prepare_empty_file(tmp_file_name, MBR_RESERVE_SIZE, (unsigned char)(prtkimg->erased_content & 0xff));
			rtk_ptr_to_file(tmp_file_name, 0, (void *)table, sizeof(struct S_MBR_TABLE));
			return 0;
		}
	}

	install_fail("Cannot find corresponding image file for this MBR\n");
	return -1;
}
#endif	//__OFFLINE_GENERATE_BIN__

int write_boottable_emmc
(S_BOOTTABLE* boottable, unsigned int factory_start, unsigned int factory_size, struct t_rtkimgdesc* prtkimg)
{
    struct S_MBR_TABLE mbr[8];
    unsigned long  total_number_of_sectors;
    enum FWTYPE efwtype;
    unsigned long  parted_sectors=0, kernel_sectors=0, acc_sectors=0;
    int partition_index=0, total_partition_count=0, partition_count=1;
    int table_index=0;
    struct t_PARTDESC* rtk_part = NULL;

    // 512 bytes per sector
    total_number_of_sectors = prtkimg->flash_size/prtkimg->mtd_erasesize;

    // partition mount point, size from rtk_part_list
    for(efwtype=FW_ROOTFS; efwtype<=FW_USR_LOCAL_ETC; efwtype=FWTYPE(efwtype+1))
    {
        if(prtkimg->fw[efwtype].flash_allo_size == 0)
        {
            continue;
        }

        parted_sectors += (prtkimg->fw[efwtype].flash_allo_size/512);
        total_partition_count++;
    }

    install_info("We have %d eMMC partitions\n", total_partition_count);
    if (total_number_of_sectors <= parted_sectors)
    {
        install_fail("eMMC partition size %llu are over capacity %llu\n", parted_sectors*512, total_number_of_sectors*512);
        return -1;
    }

    kernel_sectors = total_number_of_sectors-parted_sectors;
    install_info("Reserved %llu bytes for kernel image\n", kernel_sectors*512);

    memset(mbr, 0, sizeof(mbr));

    // reserved sectors for kenrel image
    mbr[table_index].part[0].rel_sector_start = kernel_sectors;

    for(int i=0; i<NUM_RTKPART; i++)
    {
        if( ! rtk_part_list_sort[i] )
            break;

        efwtype = rtk_part_list_sort[i]->efwtype;
        rtk_part = rtk_part_list_sort[i];

        printf("Check tbl_idx:%d, part_idx:%d\n", table_index, partition_index);
        mbr[table_index].part[partition_index].head_start = 0x3;
        mbr[table_index].part[partition_index].sector_start = 0xd0;
        mbr[table_index].part[partition_index].cylinder_start = 0xff;
        mbr[table_index].part[partition_index].head_end = 0x03;
        mbr[table_index].part[partition_index].sector_end = 0xd0;
        mbr[table_index].part[partition_index].cylinder_end = 0xff;

        parted_sectors = (prtkimg->fw[efwtype].flash_allo_size/512);

        // if total partition is over 4, we force to do an extended partition in 4th.
        // Then, later partitions also will be as extended.
        if ((partition_count>=4) && (total_partition_count>4))
        {
            // do a extended partition
            mbr[table_index].part[partition_index].id = 0x05;
            if (partition_count == 4)
            {
                // assign rest sectors to a extended partition
                mbr[table_index].part[partition_index].sector_num = total_number_of_sectors-acc_sectors-kernel_sectors;
            }
            else
            {
                mbr[table_index].part[partition_index].sector_num = parted_sectors;
            }

            install_info("table %d, partition %d, size %d\n", table_index, partition_index, mbr[table_index].part[partition_index].sector_num);
            mbr[table_index].signature = 0xaa55;
            table_index++;
            partition_index=0;

            // do the origianl partition
            mbr[table_index].part[partition_index].head_start = 0x3;
            mbr[table_index].part[partition_index].sector_start = 0xd0;
            mbr[table_index].part[partition_index].cylinder_start = 0xff;
            mbr[table_index].part[partition_index].head_end = 0x03;
            mbr[table_index].part[partition_index].sector_end = 0xd0;
            mbr[table_index].part[partition_index].cylinder_end = 0xff;
            // reserve 0x10 for partition formatting. If no reservation, mbr will be cleared.
            mbr[table_index].part[partition_index].rel_sector_start = MBR_RESERVE_SIZE/512;
        }

        mbr[table_index].part[partition_index].id = 0x83;
        mbr[table_index].part[partition_index].sector_num = parted_sectors-(MBR_RESERVE_SIZE/512);
        install_log("table %d, partition %d, size %d at 0x%08x\n", table_index, partition_index, mbr[table_index].part[partition_index].sector_num, mbr[table_index].part[partition_index].rel_sector_start);
        acc_sectors += parted_sectors;

        if (partition_count < total_partition_count)    // check if it is the last partition
        {
            if (table_index > 1)
            {
                // From table1, all partitions belong to the extended partition of table0.
                // From table2, we need to add rel_sector_start in previois table.
                mbr[table_index].part[partition_index+1].rel_sector_start = parted_sectors + mbr[table_index-1].part[1].rel_sector_start;
            }
            else if (table_index == 1)
            {
                mbr[table_index].part[1].rel_sector_start = mbr[table_index].part[0].rel_sector_start + mbr[table_index].part[0].sector_num;
            }
            else
            {
                mbr[table_index].part[partition_index+1].rel_sector_start = mbr[table_index].part[partition_index].rel_sector_start + parted_sectors;
            }
        }

        if (partition_count < 4)
        {
#ifdef __OFFLINE_GENERATE_BIN__
			if (prtkimg->bAndroid_path) {
				sprintf(rtk_part->mount_dev, "/dev/block/mmcblk0p%d",partition_count);
				sprintf(prtkimg->fw[efwtype].dev_path, "/dev/block/mmcblk0p%d", partition_count);
			} else {
				sprintf(rtk_part->mount_dev, "/dev/mmcblk0p%d",partition_count);
				sprintf(prtkimg->fw[efwtype].dev_path, "/dev/mmcblk0p%d", partition_count);
			}
#else
			// rtk_part->mount_dev[] is used for mke2fs command inside install_a.
			// prtkimg->fw[efwtype].dev_path[] is stored in layout.txt, for recovery mode only.
			sprintf(rtk_part->mount_dev, "%sp%d", prtkimg->mtdblock_path, partition_count);
            sprintf(prtkimg->fw[efwtype].dev_path, "/dev/block/mmcblk0p%d", partition_count);
#endif
            prtkimg->fw[efwtype].emmc_partIdx = partition_count;

        }
        else
        {
            if (total_partition_count > 4)      // skip mmcblk0p4 extended partition
            {
#ifdef __OFFLINE_GENERATE_BIN__
				if (prtkimg->bAndroid_path) {
					sprintf(rtk_part->mount_dev, "/dev/block/mmcblk0p%d", partition_count+1);
                		sprintf(prtkimg->fw[efwtype].dev_path, "/dev/block/mmcblk0p%d", partition_count+1);
				} else {
					sprintf(rtk_part->mount_dev, "/dev/mmcblk0p%d", partition_count+1);
                		sprintf(prtkimg->fw[efwtype].dev_path, "/dev/mmcblk0p%d", partition_count+1);
				}
#else
				// rtk_part->mount_dev[] is used for mke2fs command inside install_a.
				// prtkimg->fw[efwtype].dev_path[] is stored in layout.txt, for recovery mode only.
				sprintf(rtk_part->mount_dev, "%sp%d", prtkimg->mtdblock_path, partition_count+1);
				sprintf(prtkimg->fw[efwtype].dev_path, "/dev/block/mmcblk0p%d", partition_count+1);
#endif
                prtkimg->fw[efwtype].emmc_partIdx = partition_count+1;
            }
            else
            {
#ifdef __OFFLINE_GENERATE_BIN__
				if (prtkimg->bAndroid_path) {
					sprintf(rtk_part->mount_dev, "/dev/block/mmcblk0p%d", partition_count);
					sprintf(prtkimg->fw[efwtype].dev_path, "/dev/block/mmcblk0p%d", partition_count);
				} else {
					sprintf(rtk_part->mount_dev, "/dev/mmcblk0p%d", partition_count);
					sprintf(prtkimg->fw[efwtype].dev_path, "/dev/mmcblk0p%d", partition_count);
				}
#else
				// rtk_part->mount_dev[] is used for mke2fs command inside install_a.
				// prtkimg->fw[efwtype].dev_path[] is stored in layout.txt, for recovery mode only.
				sprintf(rtk_part->mount_dev, "%sp%d", prtkimg->mtdblock_path, partition_count);
				sprintf(prtkimg->fw[efwtype].dev_path, "/dev/block/mmcblk0p%d", partition_count);
#endif
                prtkimg->fw[efwtype].emmc_partIdx = partition_count;
            }
        }
        partition_index++;
        partition_count++;
    }

    mbr[table_index].signature = 0xaa55;

    unsigned char * addr;
    int i,j,k;
    for (k=0; k<8; k++)
    {
        install_ui("\e[1;32mmbr\e[0m     %d\n",k);
        addr=(unsigned char *)(&mbr[k]);
        j=0;
        for(i=446; i<512; i++)
        {
            j++;
            install_ui("%02x ",addr[i]);
            if (j%16==0)
            {
                install_ui("\n");
            }
        }
        install_ui("\n");
    }
    install_ui("\n\n");


#ifndef __OFFLINE_GENERATE_BIN__
	int ret=0;
	table_index = 0;
   while (mbr[table_index].signature == 0xaa55) {
      if (table_index == 0)
        ret = rtk_set_mbr(0, (char *)&mbr[table_index]);
      else if (table_index == 1)
        ret = rtk_set_mbr(mbr[0].part[3].rel_sector_start, (char *)&mbr[table_index]);
      else
        ret = rtk_set_mbr(mbr[table_index-1].part[1].rel_sector_start+mbr[0].part[3].rel_sector_start, (char *)&mbr[table_index]);

      if( ret < 0 ) {
	  	install_fail("rtk_set_mbr() failed\n");
		return ret;
      }

	#ifdef PC_SIMULATE
	char imgname[32];
	rtk_find_dir_path( prtkimg->fw[FW_KERNEL].filename, imgname, sizeof(imgname));
	sprintf(&imgname[strlen(imgname)], "mbr_%02d.bin", table_index);
	if (table_index == 0)
		add_mbr(boottable, imgname, 0, sizeof(mbr[0]));
	else if (table_index == 1)
		add_mbr(boottable, imgname, mbr[0].part[3].rel_sector_start, sizeof(mbr[0]));
	else
		add_mbr(boottable, imgname, mbr[table_index-1].part[1].rel_sector_start+mbr[0].part[3].rel_sector_start, sizeof(mbr[0]));
	#endif

      table_index++;
   }
#else
   table_index = 0;
	while (mbr[table_index].signature == 0xaa55) {
   	if (table_index == 0)
   		rtk_set_mbr(0, (char *)&mbr[table_index]);
   	else if (table_index == 1) {
			if (prtkimg->all_in_one == 0)
				rtk_set_mbr_offline(mbr[0].part[3].rel_sector_start, (char *)&mbr[table_index], prtkimg);
			else
				rtk_set_mbr(mbr[0].part[3].rel_sector_start, (char *)&mbr[table_index]);
		}
   	else {
			if (prtkimg->all_in_one == 0)
				rtk_set_mbr_offline(mbr[table_index-1].part[1].rel_sector_start+mbr[0].part[3].rel_sector_start, (char *)&mbr[table_index], prtkimg);
			else
				rtk_set_mbr(mbr[table_index-1].part[1].rel_sector_start+mbr[0].part[3].rel_sector_start, (char *)&mbr[table_index]);
		}

   	table_index++;
   }

	if (prtkimg->all_in_one == 0) {
		char tmp_file_name[128];
		FILE *fd;
		unsigned long long flash_bottom_high_limit = 0;

		flash_bottom_high_limit = prtkimg->fw[FW_VIDEO_CLOGO2].flash_offset + prtkimg->fw[FW_VIDEO_CLOGO2].flash_allo_size;
		sprintf(tmp_file_name, "%s/layout.txt", DEFAULT_OUTPUT_DIR);
		fd = fopen(tmp_file_name, "w");
		if (fd == NULL) {
			install_fail("Cannot create %s\n", tmp_file_name);
			goto final;
		}

		fprintf(fd, "filename start size\n");
		fprintf(fd, "%s 0x00000000 0x%08x\n", prtkimg->output_path, flash_bottom_high_limit);
		for(efwtype=FW_ROOTFS; efwtype<=FW_USR_LOCAL_ETC; efwtype=FWTYPE(efwtype+1)) {
        if(prtkimg->fw[efwtype].flash_allo_size == 0)
            continue;

			fprintf(fd, "%s.ext4img 0x%08llx 0x%08llx\n", inv_by_fwtype(efwtype), prtkimg->fw[efwtype].flash_offset, prtkimg->fw[efwtype].flash_allo_size-8192);
    	}
		fclose(fd);
	}
final:
#endif

    return 0;
}

int rtk_mount_format_emmc(S_BOOTTABLE* pboottable, struct t_rtkimgdesc* prtkimg)
{
    enum FWTYPE efwtype;
    struct t_PARTDESC* rtk_part = NULL;
    char command[128];
    int ret=0;
#ifdef __OFFLINE_GENERATE_BIN__
	char tmp_file_name[128];
	unsigned long long tmp_part_size;
	struct t_imgdesc* pimgdesc;
#endif

    // partition mount point, size from rtk_part_list
    for(efwtype=FW_ROOTFS;efwtype<=FW_USR_LOCAL_ETC;efwtype=FWTYPE(efwtype+1)) {
        if(prtkimg->fw[efwtype].flash_allo_size == 0) continue;

        // find internal partition info data structure
        rtk_part = find_part_by_efwtype((struct t_PARTDESC*)&rtk_part_list, efwtype);

		// we ONLY support extX file system
		if (!strncmp(inv_efs_to_str(rtk_part->efs), "ext", 3)) {
			// if the partition needs to be installed some things, we will format it in burning stage.
			if (strncmp(rtk_part->filename,"null", 4) && strncmp(rtk_part->filename,"NULL", 4))
				continue;

#ifdef __OFFLINE_GENERATE_BIN__
			pimgdesc = &prtkimg->fw[efwtype];
			sprintf(tmp_file_name, "%s/%s.ext4img", DEFAULT_OUTPUT_DIR, inv_by_fwtype(efwtype));
			// reserve 0x10 sectors for partition formatting. If no reservation, mbr will be cleared.
			tmp_part_size = prtkimg->fw[efwtype].flash_allo_size-MBR_RESERVE_SIZE;
			if (!strncmp(inv_by_fwtype(efwtype), "system", 6) || !strncmp(inv_by_fwtype(efwtype), "data", 4))
				sprintf(command, "./make_ext4fs -l %lld -a %s %s", tmp_part_size, inv_by_fwtype(efwtype), DEFAULT_EXT4_OUTPUT);
			else //if (prtkimg->fw[efwtype].flash_allo_size > 512*1024*1024)
				sprintf(command, "./make_ext4fs -l %lld %s", tmp_part_size, DEFAULT_EXT4_OUTPUT);

         ret = rtk_command(command, __LINE__, __FILE__);
         if (ret < 0)
            goto failed;

      	//if we don't generate a one big image, we can skip this
      	if (prtkimg->all_in_one) {
         	install_info("Burning %s to %s at %llx with %lld bytes\n", tmp_file_name, DEFAULT_TEMP_OUTPUT, pimgdesc->flash_offset, tmp_part_size);
         	if (pimgdesc->emmc_partIdx <= 4)
            	ret = rtk_file_to_flash(DEFAULT_EXT4_OUTPUT, 0, DEFAULT_TEMP_OUTPUT, pimgdesc->flash_offset, tmp_part_size, NULL);
         	else
            	ret = rtk_file_to_flash(DEFAULT_EXT4_OUTPUT, 0, DEFAULT_TEMP_OUTPUT, pimgdesc->flash_offset+MBR_RESERVE_SIZE, tmp_part_size, NULL);

         	if(ret < 0) {
            	install_fail("burn %s into %s fail\r\n", DEFAULT_EXT4_OUTPUT, DEFAULT_TEMP_OUTPUT);
            	goto failed;
         	}

         	//remove temp file
         	sprintf(command, "rm -rf %s %s", DEFAULT_EXT4_OUTPUT, pimgdesc->mount_point);
         	if((ret = rtk_command(command, __LINE__, __FILE__)) < 0) {
            	install_debug("Exec command fail\r\n");
            	return -1;
         	}
      	}
      	else {   // not all_in_one mode
         	if (pimgdesc->emmc_partIdx <= 4) {
            	sprintf(command, "mv %s %s", DEFAULT_EXT4_OUTPUT, tmp_file_name);
            	if((ret = rtk_command(command, __LINE__, __FILE__)) < 0)
               	goto failed;
         	}
         	else {
            	ret = rtk_file_to_flash(DEFAULT_EXT4_OUTPUT, 0, tmp_file_name, 8192, tmp_part_size, NULL);
            	if(ret < 0) {
               	install_fail("burn %s into %s fail\r\n", DEFAULT_EXT4_OUTPUT, tmp_file_name);
               	goto failed;
            	}
         	}
      	}
#else	// !__OFFLINE_GENERATE_BIN__
#ifdef ENABLE_ERASE_CHECK
        if (rtk_part->bErase) {
#endif
        	install_info("eMMC partition %s formating...\n", inv_by_fwtype(efwtype));
        	sprintf(command, "mke2fs -t %s %s > /dev/null", inv_efs_to_str(rtk_part->efs), rtk_part->mount_dev);
        	ret = rtk_command(command, __LINE__, __FILE__);
			if (ret < 0)
				goto failed;
#ifdef ENABLE_ERASE_CHECK
        } else {
            install_info("ERASE_CHECK:eMMC partition %s not formating...\n", inv_by_fwtype(efwtype));
        }
#endif

        	install_info("eMMC partition %s mounting...\n", inv_by_fwtype(efwtype));
			// in some cases on android, mkdir -p will return error if there is a existed special link.
    		// We add rm before mkdir.
        	sprintf(command, "rm -rf %s;mkdir -p %s", rtk_part->mount_point, rtk_part->mount_point);
        	ret = rtk_command(command, __LINE__, __FILE__);
			if (ret < 0)
				goto failed;

        	sprintf(command, "mount -t %s %s %s", inv_efs_to_str(rtk_part->efs), rtk_part->mount_dev, rtk_part->mount_point);
        	ret = rtk_command(command, __LINE__, __FILE__);
			if (ret < 0)
				goto failed;
#endif	//__OFFLINE_GENERATE_BIN__

			remove_part_by_partname(pboottable, prtkimg->fw[efwtype].part_name);
            add_part(pboottable  \
                    , prtkimg->fw[efwtype].part_name \
                    , prtkimg->fw[efwtype].mount_point   \
                    , prtkimg->fw[efwtype].dev_path   \
                    , string_inv_to_efs(prtkimg->fw[efwtype].fs_name)  \
                    , prtkimg->fw[efwtype].flash_offset   \
                    , prtkimg->fw[efwtype].flash_allo_size);

		}
		else {
			ret = -1;
			install_fail("We cannot support %s filesystem\n", inv_efs_to_str(rtk_part->efs));
			goto failed;
		}
    }
failed:

    return ret;
}

#endif //EMMC_SUPPORT

S_BOOTTABLE* read_boottable(S_BOOTTABLE* boottable, struct t_rtkimgdesc* prtkimg)
{
   int ret;
   int part_num;
   unsigned long long offset;
   int target, size;
   char newline[256] = {0}, mount_point[128] = {0}, mount_dev[128]={0}, filesystem[128] = {0}, partname[128] = {0}, path[128] = {0}, compress_type[16] = {0};
   char boottype[256] = {0};
   FILE* pfile = NULL;
   char key[128] = {0};
   enum E_FWTYPE fwtype;

   if((ret = factory_load(NULL, prtkimg)) < 0)
   {
      install_debug("read_boottable fail\r\n");
      return NULL;
   }

   snprintf(path, sizeof(path), "%s/%s", factory_dir, LAYOUT_FILENAME);
   pfile = fopen(path,"r");
   if(pfile == NULL)
   {
      install_log("Can't open layout.txt, try to read %s fail\r\n", LAYOUT_FILENAME);
      return NULL;
   }

   install_log("parsing %s\r\n", LAYOUT_FILENAME);
   part_num = 0;
   while(NULL != fgets(newline, sizeof(newline), pfile))
   {
      sscanf(newline, "#define %s ", key);
      memset(compress_type, 0, sizeof(compress_type));

      // BOOTTYPE
      if(!strcmp(key, "BOOTTYPE"))
      {
         sscanf(newline+strlen("#define BOOTTYPE "), "\" %s \"", boottype);
         boottable->boottype = string_inv_to_boottype(boottype);
         continue;
      }

      // IMGCHECKSUM
      if(!strcmp(key, "IMGCHECKSUM"))
      {
         sscanf(newline+strlen("#define IMGCHECKSUM "), "\" %s \"", boottable->imgcksum);
         continue;
      }

      // URL
      if(!strcmp(key, "URL"))
      {
         sscanf(newline+strlen("#define URL "), "\" %s \"", boottable->url);
         continue;
      }

      // TAG
      if(!strcmp(key, "TAG"))
      {
         sscanf(newline+strlen("#define TAG "), " %d ", (int *)&boottable->tag);
         continue;
      }

      // SSU
      if(!strcmp(key, "SSUWORKPART"))
      {
         sscanf(newline+strlen("#define SSUWORKPART "), " %d ", &boottable->ssu_work_part);
         continue;
      }

	  if(!strcmp(key, "BOOTPART"))
      {
         sscanf(newline+strlen("#define BOOTPART "), " %d ", &boottable->bootpart);
         continue;
      }

      // FW
      fwtype = string_inv_to_fwtype(key);
      if (FWTYPE_END != fwtype) {
         sscanf(newline+strlen("#define ")+strlen(key)+1, "\" target=%x offset=%llx size=%x type=%[a-zA-Z0-9_] \"", &target, &offset, &size, compress_type);
         //install_test("firmware:%s target:%x offset:%x size:%x\r\n", key, target, offset, size);
         boottable->fw.list[fwtype].loc.target = target;
         boottable->fw.list[fwtype].loc.offset = offset;
         boottable->fw.list[fwtype].loc.size = size;
         snprintf(boottable->fw.list[fwtype].loc.type, sizeof(boottable->fw.list[fwtype].loc.type), "%s", compress_type);
         continue;
      }

      // PART
      if(!strncmp(key, "PART", 4))
      {
         sscanf(newline+strlen("#define ")+strlen(key)+1, "\" offset=%llx size=%x mount_point=%s mount_dev=%s filesystem=%s partname=%s type=%[a-zA-Z0-9_] \"", &offset, &size, mount_point, mount_dev, filesystem, partname, compress_type);
         //install_test("part:%s offset:%x size:%x mount_point:%s filesystem:%s partname:%s\r\n", key, offset, size, mount_point, filesystem, partname);
         sprintf(boottable->part.list[part_num].mount_point, "%s", mount_point);
         sprintf(boottable->part.list[part_num].mount_dev, "%s", mount_dev);
         sprintf(boottable->part.list[part_num].partname, "%s", partname);
         boottable->part.list[part_num].efs = string_inv_to_efs(filesystem);
         boottable->part.list[part_num].loc.offset = offset;
         boottable->part.list[part_num].loc.size = size;
         snprintf(boottable->part.list[part_num].loc.type, sizeof(boottable->part.list[part_num].loc.type), "%s", compress_type);
         part_num++;
         continue;
      }
   }
   //boottable->part.partcount = part_num;
   install_debug("boottable->part.partcount:%d\r\n", part_num);
   fclose(pfile);
   install_debug("read boottable success\r\n");
   return boottable;
}

S_BOOTTABLE* write_boottable(S_BOOTTABLE* boottable, unsigned int factory_start, unsigned int factory_size, struct t_rtkimgdesc* prtkimg, bool bFlush)
{
   int ret;
   int i;
   FILE* filep = NULL;
   char cmd[512] = {0}, path[128] = {0};
   enum E_FWTYPE efwtype;

   if((ret = factory_load(NULL, prtkimg)) < 0)
   {
      install_debug("factory_load fail\r\n");
      //return NULL;
   }

   sprintf(path, "%s/%s", factory_dir, LAYOUT_FILENAME);
   filep = fopen(path, "w");
   if(filep == NULL)
   {
      install_log("Can't open %s\r\n", path);
      return 0;
   }

   // DATE
   fprintf(filep, "#define CREATE_DATE \" %s \"\n", boottable->date);

   // TIME
   fprintf(filep, "#define CREATE_TIME \" %s \"\n", boottable->time);

   // BOOTTYPE
   fprintf(filep, "#define BOOTTYPE \" %s \"\n", inv_boottype(boottable->boottype));

   // IMGCHECKSUM
   if(strlen(boottable->imgcksum)!=0)
      fprintf(filep, "#define IMGCHECKSUM \" %s \"\n", boottable->imgcksum);

   // URL
   if(strlen(boottable->url)!=0)
      fprintf(filep, "#define URL \" %s \"\n", boottable->url);

   // SSU
   fprintf(filep, "#define SSUWORKPART %d\n", boottable->ssu_work_part);

   // BOOTPART
   fprintf(filep, "#define BOOTPART %d\n", boottable->bootpart);

   // FWTYPE
   for(efwtype=FWTYPE_KERNEL;efwtype<FWTYPE_DALOGO;efwtype=E_FWTYPE(efwtype+1))
   {
      // FW_KERNEL, FW_AKERNEL, FW_VKERNEL
	  if (boottable->fw.list[efwtype].loc.size != 0) {
      	fprintf(filep, "#define %s \" target=%x offset=%llx size=%llx type=%s name=%s \"\n"
         	, inv_fwtype(efwtype)
         	, boottable->fw.list[efwtype].loc.target
         	, boottable->fw.list[efwtype].loc.offset
         	, boottable->fw.list[efwtype].loc.size
         	, boottable->fw.list[efwtype].loc.type
         	, boottable->fw.list[efwtype].imgname);
	  }
   }

   for(efwtype=FWTYPE_DALOGO;efwtype<FWTYPE_END;efwtype=E_FWTYPE(efwtype+1))
   {
      // FW_DLOGO, FW_CLOGO, FW_RESCUE
	  if (boottable->fw.list[efwtype].loc.size != 0) {
      	fprintf(filep, "#define %s \" target=%x offset=%llx size=%llx type=%s name=%s \"\n"
         	, inv_fwtype(efwtype)
         	, boottable->fw.list[efwtype].loc.target
         	, boottable->fw.list[efwtype].loc.offset
         	, boottable->fw.list[efwtype].loc.size
         	, boottable->fw.list[efwtype].loc.type
        	, boottable->fw.list[efwtype].imgname);
	  }
   }

   for(i = 0; i < (int)boottable->part.partcount; i++)
   {
      // PART%d
	  if (boottable->part.list[i].loc.size != 0) {
      		fprintf(filep, "#define PART%u \" offset=%llx size=%llx mount_point=%s mount_dev=%s filesystem=%s partname=%s type=%s name=%s \"\n"
         	,i
         	, boottable->part.list[i].loc.offset
         	, boottable->part.list[i].loc.size
         	, boottable->part.list[i].mount_point
         	, boottable->part.list[i].mount_dev
         	, inv_efs(boottable->part.list[i].efs)
         	, boottable->part.list[i].partname
         	, boottable->part.list[i].type
         	, boottable->part.list[i].imgname);
	  }
   }

#ifdef EMMC_SUPPORT
	for(i = 0; i < (int)boottable->mbr.mbrcount; i++)
	{
	   // MBR%d
	   if (boottable->part.list[i].loc.size != 0) {
			 fprintf(filep, "#define MBR%u \" offset=%llx size=%x name=%s \"\n"
			 ,i
			 , boottable->mbr.list[i].offset
			 , boottable->mbr.list[i].size
			 , boottable->mbr.list[i].imgname);
	   }
	}
#endif

   // cat layout.txt file
   //snprintf(path, sizeof(path), "%s/%s", factory_dir, LAYOUT_FILENAME);
   //snprintf(cmd, sizeof(cmd), "dd if=%s", path);
   //ret = rtk_command(cmd, __LINE__, __FILE__, 0);
   //snprintf(cmd, sizeof(cmd) , "md5sum %s > %s_md5", path, path);
   //ret = rtk_command(cmd, __LINE__, __FILE__, 0);

   // TAG
   fprintf(filep, "#define TAG %d\n", boottable->tag);

   fclose(filep);

   ret = factory_flush(factory_start, factory_size, prtkimg, bFlush);
   if(ret < 0)
   {
      install_debug("factory_flush fail\r\n");
      return NULL;
   }
   return boottable;
}

char* hash_file(const char* filepath)
{
   static char hash_val[36];
   int ret;
   char cmd[128] = {0}, path[128] = {0}, newline[256] = {0};
   FILE* filep = NULL;

   snprintf(path, sizeof(path), "%s/md5.txt", PKG_TEMP);
   snprintf(cmd, sizeof(cmd), "md5sum %s > %s", filepath, path);
   ret = rtk_command(cmd, __LINE__, __FILE__, 0);
   if(ret < 0) return NULL;
   filep = fopen(path,"r");
   if(filep == NULL)
   {
      install_log("Can't open %s\r\n", path);
      return NULL;
   }

   if(NULL != fgets(newline, sizeof(newline), filep))
   {
      sscanf(newline, "%s", hash_val);
   }
   fclose(filep);
   return hash_val;
}

const char* inv_boottype(enum E_BOOTTYPE boottype)
{
   switch(boottype)
   {
      case BOOTTYPE_UNKNOWN_BOOTTYPE:
         return "BOOTTYPE_UNKNOWN_BOOTTYPE";
      case BOOTTYPE_NL_BIG_ROOTFS:
         return "BOOTTYPE_NL_BIG_ROOTFS";
      case BOOTTYPE_NL_SMALL_ROOTFS:
         return "BOOTTYPE_NL_SMALL_ROOTFS";
      case BOOTTYPE_TMP_RESCUE:
         return "BOOTTYPE_TMP_RESCUE";
      case BOOTTYPE_NATIVE_RESCUE:
         return "BOOTTYPE_NATIVE_RESCUE";
      case BOOTTYPE_COMPLETE:
         return "BOOTTYPE_COMPLETE";
	  default:
		 return "BOOTTYPE_UNKNOWN_BOOTTYPE";
   }
   return "BOOTTYPE_UNKNOWN_BOOTTYPE";
}

enum E_BOOTTYPE string_inv_to_boottype(const char* str)
{
   enum E_BOOTTYPE boottype;
   for(boottype=E_BOOTTYPE(0);boottype<BOOTTYPE_END;boottype=E_BOOTTYPE(boottype+1))
   {
      if(!strcmp(str, inv_boottype(boottype)))
         return boottype;
   }
   return BOOTTYPE_END;
}

const char* inv_fwtype(enum E_FWTYPE fwtype)
{
   switch(fwtype)
   {
      case FWTYPE_KERNEL:
         return "FW_KERNEL";
      case FWTYPE_RESCUE_DT:
         return "FW_RESCUE_DT";
      case FWTYPE_KERNEL_DT:
         return "FW_KERNEL_DT";
      case FWTYPE_RESCUE_ROOTFS:
         return "FW_RESCUE_ROOTFS";
      case FWTYPE_KERNEL_ROOTFS:
         return "FW_KERNEL_ROOTFS";
      case FWTYPE_AKERNEL:
         return "FW_AKERNEL";
      case FWTYPE_VKERNEL:
         return "FW_VKERNEL";
	  case FWTYPE_VKERNEL2:
         return "FW_VKERNEL2";
	  case FWTYPE_ECPU:
         return "FW_ECPU";
      case FWTYPE_DALOGO:
         return "FW_DALOGO";
     case FWTYPE_DILOGO:
        return "FW_DILOGO";
      case FWTYPE_DVLOGO:
         return "FW_DVLOGO";
      case FWTYPE_CALOGO1:
         return "FW_CALOGO1";
      case FWTYPE_CVLOGO1:
         return "FW_CVLOGO1";
      case FWTYPE_CALOGO2:
         return "FW_CALOGO2";
      case FWTYPE_CVLOGO2:
         return "FW_CVLOGO2";
      case FWTYPE_RESCUE:
         return "FW_RESCUE";
      case FWTYPE_NRESCUE:
         return "FW_NRESCUE";
      case FWTYPE_FWTBL:
         return "FW_FWTBL";
	  default:
		 return "FW_UNDEF";
   }
   return "FW_UNDEF";
}

enum E_FWTYPE string_inv_to_fwtype(const char* str)
{
   enum E_FWTYPE fwtype;
   for(fwtype=E_FWTYPE(0);fwtype<FWTYPE_END;fwtype=E_FWTYPE(fwtype+1))
   {
      if(!strcmp(str, inv_fwtype(fwtype)))
         return fwtype;
   }
   return FWTYPE_END;
}

const char* inv_efs(enum E_FS efs)
{
   switch(efs)
   {
      case FS_UBIFS:
         return "ubifs";
      case FS_SQUASH:
         return "squashfs";
      case FS_JFFS2:
         return "jffs2";
      case FS_YAFFS2:
         return "yaffs2";
      case FS_RAWFILE:
         return "rawfile";
	  case FS_EXT4:
		 return "ext4";
	  default:
		 return "fs_undef";
   }
   return "fs_undef";
}

enum E_FS string_inv_to_efs(const char* str)
{
   enum E_FS efs;
   for(efs=E_FS(0);efs<FS_END;efs=E_FS(efs+1))
   {
      if(!strcmp(str, inv_efs(efs)))
         return efs;
   }
   return FS_END;
}

int get_index_by_partname(S_BOOTTABLE* pboottable, const char* partname)
{
   int i;
   for(i=0;i<(int)pboottable->part.partcount;i++)
   {
      if(0 == strcmp(partname, pboottable->part.list[i].partname))
      {
         return i;
      }
   }
   return -1;
}

int remove_part_by_partname(S_BOOTTABLE* pboottable, const char* partname)
{
   int index, last;
   index = get_index_by_partname(pboottable, partname);
   if(index < 0)
   {
      install_debug("Can't find the index of %s\r\n", partname);
      return -1;
   }
   printf("Enter remove_part_by_partname \n");
   while(index >= 0)
   {
      last = pboottable->part.partcount - 1;
      if(pboottable->part.partcount == 1)
      {
         pboottable->part.partcount = 0;
         return 0;
      }
      else
      {
         memcpy(&pboottable->part.list[index], &pboottable->part.list[last], sizeof(struct S_PARTDESC));
#if 0
         sprintf(pboottable->part.list[i].partname, partname);
         sprintf(pboottable->part.list[i].mount_point, "%s", mount_point);
         pboottable->part.list[i].efs = efs;
         pboottable->part.list[i].loc.offset = offset;
         pboottable->part.list[i].loc.size = size;
#endif
         pboottable->part.partcount--;
      }
      index = get_index_by_partname(pboottable, partname);
   }
      printf("Exit remove_part_by_partname \n");

   return 0;
}

int update_ssu_work_part(S_BOOTTABLE* pboottable, int next_ssu_work_part){
   pboottable->ssu_work_part = next_ssu_work_part;
   return 0;
}

int add_part(S_BOOTTABLE* pboottable, const char* partname, const char* mount_point, const char* mount_dev, E_FS efs, unsigned long long offset, unsigned long long size, const char *compress_type)
{
   int i;
   i = pboottable->part.partcount;
   snprintf(pboottable->part.list[i].partname, sizeof(pboottable->part.list[i].partname), "%s", partname);
   snprintf(pboottable->part.list[i].mount_point, sizeof(pboottable->part.list[i].mount_point), "%s", mount_point);
   snprintf(pboottable->part.list[i].mount_dev, sizeof(pboottable->part.list[i].mount_dev), "%s", mount_dev);
   pboottable->part.list[i].efs = efs;
   pboottable->part.list[i].loc.offset = offset;
   pboottable->part.list[i].loc.size = size;
   if (compress_type == NULL) {
      *pboottable->part.list[i].type = '\0';
   }
   else {
      snprintf(pboottable->part.list[i].type, sizeof(pboottable->part.list[i].type), "%s", compress_type);
   }
   pboottable->part.partcount++;
   return 0;
}

int add_part(S_BOOTTABLE* pboottable, t_imgdesc *pimgdesc)
{
   int i = pboottable->part.partcount;

   strncpy(pboottable->part.list[i].imgname, pimgdesc->filename, sizeof(pboottable->part.list[i].partname));
   add_part(pboottable,
            pimgdesc->part_name,
            pimgdesc->mount_point,
            pimgdesc->dev_path,
            string_inv_to_efs(pimgdesc->fs_name),
            pimgdesc->flash_offset,
            pimgdesc->flash_allo_size,
            pimgdesc->compress_type);

   return 0;
}

int update_fw(S_BOOTTABLE* pboottable, E_FWTYPE efwtype, unsigned int mem_offset, unsigned long long flash_offset, unsigned int img_size, const char *compress_type)
{
   pboottable->fw.list[efwtype].loc.target = mem_offset;
   pboottable->fw.list[efwtype].loc.offset = flash_offset;
   pboottable->fw.list[efwtype].loc.size = img_size;

   if (compress_type == NULL) {
      *pboottable->fw.list[efwtype].loc.type = '\0';
   }
   else {
      snprintf(pboottable->fw.list[efwtype].loc.type, sizeof(pboottable->fw.list[efwtype].loc.type), "%s", compress_type);
   }
   return 0;
}

int update_fw(S_BOOTTABLE* pboottable, E_FWTYPE efwtype,  t_imgdesc *pimgdesc)
{
   strncpy(pboottable->fw.list[efwtype].imgname, pimgdesc->filename, sizeof(pboottable->fw.list[efwtype].imgname));
   update_fw(pboottable,
            efwtype,
            pimgdesc->mem_offset,
            pimgdesc->flash_offset,
            pimgdesc->img_size,
            pimgdesc->compress_type);

   return 0;
}

#ifdef EMMC_SUPPORT
int add_mbr(S_BOOTTABLE* pboottable, char *imgname, unsigned int blkoffset, unsigned int imgsize)
{
	int i = pboottable->mbr.mbrcount;

	strncpy( pboottable->mbr.list[i].imgname, imgname, sizeof(pboottable->mbr.list[i].imgname));
	pboottable->mbr.list[i].offset = (unsigned long long)blkoffset * 512;
	pboottable->mbr.list[i].size = imgsize;
	pboottable->mbr.mbrcount++;

	return 0;
}
#endif
