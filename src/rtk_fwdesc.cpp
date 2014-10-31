#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <error.h>
#include <string.h>

#include <rtk_common.h>
#include <rtk_fwdesc.h>
#include <rtk_mtd.h>
#include <rtk_def.h>
#include <rtk_imgdesc.h>
#define VERONA_SIGNATURE "VERONA__"

//#define _DEBUG

int load_fwdesc(struct rtk_fw_header* fw, unsigned int _NAND_BOOTCODE_RESERVED_SIZE)
{
   int ret;
   int dev_fd;
   int i;
   unsigned int fw_len;
   fw_desc_entry_v21_t tmp;


   dev_fd = rtk_open_mtd_block_with_offset(_NAND_BOOTCODE_RESERVED_SIZE);
   if(dev_fd < 0)
   {
      install_debug("rtk_open_mtd_block_with_offset() fail\r\n");
      return -1;
   }

   // read fw_tab
   ret = read(dev_fd, &fw->fw_tab, sizeof(fw_desc_table_v1_t));
   if(ret < 0)
   {
      install_debug("read() fail\r\n");
      return -1;
   }

   // check if secure
   if(fw->fw_tab.version == FIRMWARE_DESCRIPTION_TABLE_VERSION_21)
   {
      // secure
      fw_len = sizeof(fw_desc_entry_v21_t);
   }
   else if(fw->fw_tab.version == FIRMWARE_DESCRIPTION_TABLE_VERSION_01)
   {
      // non-secure
      fw_len = sizeof(fw_desc_entry_v1_t);
   }
   else
   {
      install_debug("Wrong boottable version:0x%02x\r\n", fw->fw_tab.version);
      return -1;

   }

   install_info("version:0x%02x #part:%u #fw:%u checksum:0x%08x\r\n"
   , fw->fw_tab.version
   , get_be32_to_cpu(fw->fw_tab.part_list_len)/sizeof(part_desc_entry_v1_t)
   , get_be32_to_cpu(fw->fw_tab.fw_list_len)/fw_len
   , get_be32_to_cpu(fw->fw_tab.checksum));


   //sanity check
   if(get_be32_to_cpu(fw->fw_tab.part_list_len)/sizeof(part_desc_entry_v1_t) > 10)
   {
      install_debug("part_list_len > 10\r\n");
      return -1;
   }

   // read part_desc
   for(i=0;i<(int)(get_be32_to_cpu(fw->fw_tab.part_list_len)/sizeof(part_desc_entry_v1_t));i++)
   {
      ret = read(dev_fd, &fw->part_desc[i], sizeof(part_desc_entry_v1_t));
      install_log("part type:0x%02x, length=0x%08x, fw_count=0x%02x, fw_type=0x%02x, mount=%s\r\n"   \
         , fw->part_desc[i].type
         , fw->part_desc[i].length
         , fw->part_desc[i].fw_count
         , fw->part_desc[i].fw_type
         , fw->part_desc[i].mount_point);
   }

   // read fw_desc
   FW_CLEAR_ALL(fw);

   for(i=0;i<(int)(get_be32_to_cpu(fw->fw_tab.fw_list_len)/fw_len);i++)
   {
      ret = read(dev_fd, &tmp, fw_len);

      //sanity check
      if(tmp.v1.type >= FW_TYPE_UNKNOWN)
      {
         install_debug("type unknown\r\n");
      }
      else
      {
         FW_SET(fw, tmp.v1.type);
         memcpy(&fw->fw_desc[tmp.v1.type], &tmp, sizeof(fw_desc_entry_v1_t));
         be32_to_cpu(tmp.v1.target_addr);
         be32_to_cpu(tmp.v1.offset);
         be32_to_cpu(tmp.v1.length);
         be32_to_cpu(tmp.v1.paddings);
         install_log("fwtype:0x%02x, target_addr=0x%08x, offset=0x%08x, length=0x%08x, paddings=0x%08x (valid:0x%08x)\r\n"\
            , tmp.v1.type, tmp.v1.target_addr, tmp.v1.offset, tmp.v1.length, tmp.v1.paddings, fw->valid);
      }
   }
   close(dev_fd);
   return 0;
}
int load_fwdesc_nor(struct rtk_fw_header* fw)
{
   int ret;
   int dev_fd;
   int i;
   fw_desc_entry_v1_t tmp;

   dev_fd = rtk_open_mtd_block();
   if(dev_fd < 0)
   {
      install_debug("rtk_open_mtd_block() fail\r\n");
      return -1;
   }

   ret = lseek(dev_fd, 0, SEEK_SET);
   if(dev_fd < 0)
   {
      install_debug("lseek() fail\r\n");
      return -1;
   }

   // read fw_tab
   ret = read(dev_fd, &fw->fw_tab, sizeof(fw_desc_table_v1_t));
   if(ret < 0)
   {
      install_debug("read() fail\r\n");
      return -1;
   }
   install_log("#part:0x%08x #fw:%u checksum:0x%08x\r\n"
   , fw->fw_tab.part_list_len
   , get_be32_to_cpu(fw->fw_tab.fw_list_len)/sizeof(fw_desc_entry_v1_t)
   , get_be32_to_cpu(fw->fw_tab.checksum));

   //sanity check
   /*
   if(get_be32_to_cpu(fw->fw_tab.part_list_len)/sizeof(part_desc_entry_v1_t) > 10)
   {
      install_debug("part_list_len > 10\r\n");
      return -1;
   }
   */

   // read part_desc nor flash no part table
#if 0
   for(i=0;i<get_be32_to_cpu(fw->fw_tab.part_list_len)/sizeof(part_desc_entry_v1_t);i++)
   {
      ret = read(dev_fd, &fw->part_desc[i], sizeof(part_desc_entry_v1_t));
      printf("part type:0x%02x, length=0x%08x, fw_count=0x%02x, fw_type=0x%02x, mount=%s\r\n"   \
         , fw->part_desc[i].type
         , get_be32_to_cpu(fw->part_desc[i].length)
         , fw->part_desc[i].fw_count
         , fw->part_desc[i].fw_type
         , fw->part_desc[i].mount_point);
   }
#endif
   // read fw_desc
   FW_CLEAR_ALL(fw);
   for(i=0;i<(int)(get_be32_to_cpu(fw->fw_tab.fw_list_len)/sizeof(fw_desc_entry_v1_t));i++)
   {
      ret = read(dev_fd, &tmp, sizeof(fw_desc_entry_v1_t));

      //sanity check
      if(tmp.type >= FW_TYPE_UNKNOWN)
      {
         install_debug("type unknown\r\n");
      }
      else
      {
         FW_SET(fw, tmp.type);
         memcpy(&fw->fw_desc[tmp.type], &tmp, sizeof(fw_desc_entry_v1_t));
         printf("fwtype:0x%02x, target_addr=0x%08x, offset=0x%08x, length=0x%08x, paddings=0x%08x (valid:0x%08x)\r\n" \
         , tmp.type, get_be32_to_cpu(tmp.target_addr), get_be32_to_cpu(tmp.offset), get_be32_to_cpu(tmp.length), tmp.paddings, fw->valid);
      }
   }
   close(dev_fd);
   return 0;
}

void dump_rawdata(u8 *p, u32 len);
void dump_rawdata(u8 *p, u32 len)
{
   u32 i;
   for(i=0;i<len;i++)
   {
      printf("%02x ", p[i]);
      if(0==(i+1)%16)
      {
         printf("\r\n");
      }

   }
   printf("\r\n");
}

#if 0
typedef struct {
   u8 signature[8];
   u32   checksum;
   u8 version; //0x01
   u8 reserved[7];
   u32   paddings;
   u32   part_list_len;
   u32   fw_list_len;
} __attribute__((packed)) fw_desc_table_v1_t;

typedef struct {
   u8 signature[8];
   u32   checksum;
   u8 version; //0x00
   u8 reserved[15];
   u32   length;
} __attribute__((packed)) fw_desc_table_t;


#endif
void dump_fw_desc_table_v1(fw_desc_table_v1_t *p)
{
   u32 i;
   printf("[%s]\r\n", __func__);
   printf("%16s: "        , "signature");
   for (i = 0; i < 8; i++) { printf("%02x ", p->signature[i]);};
   printf("\r\n");
   printf("%16s: %08x\r\n", "checksum"     , p->checksum);
   printf("%16s: %02x\r\n", "version"      , p->version);
   printf("%16s: "        , "reserved");
   for (i = 0; i < 7; i++) { printf("%02x ", p->reserved[i]);};
   printf("\r\n");
   printf("%16s: %08x\r\n", "paddings"     , p->paddings);
   printf("%16s: %08x\r\n", "part_list_len", p->part_list_len);
   printf("%16s: %08x\r\n", "fw_list_len"  , p->fw_list_len);

   printf("raw dump:\r\n");
   dump_rawdata((u8 *)p, sizeof(fw_desc_table_v1_t));
   return;
}

void dump_fw_desc_table(fw_desc_table_t *p)
{
   u32 i;
   printf("[%s]\r\n", __func__);
   printf("%16s: "        , "signature");
   for (i = 0; i < 8; i++) { printf("%02x ", p->signature[i]);};
   printf("\r\n");
   printf("%16s: %08x\r\n", "checksum"     , p->checksum);
   printf("%16s: %02x\r\n", "version"      , p->version);
   printf("%16s: "        , "reserved");
   for (i = 0; i < 15; i++) { printf("%02x ", p->reserved[i]);};
   printf("\r\n");
   printf("%16s: %08x\r\n", "length"     , p->length);

   printf("raw dump:\r\n");
   dump_rawdata((u8 *)p, sizeof(fw_desc_table_t));
   return;
}




#if 0
typedef struct {
   u8 type;
#if BYTE_ORDER == LITTLE_ENDIAN
   u8 reserved:7,
      ro:1;
#elif BYTE_ORDER == BIG_ENDIAN
   u8 ro:1,
      reserved:7;
#else
#error "Please fix <asm/byteorder.h>"
#endif
   u32   length;
   u8 fw_count;
   u8 fw_type;
   u8 reserved_1[8];
   u8 mount_point[32];
} __attribute__((packed)) part_desc_entry_v1_t;

#endif
void dump_part_desc_entry_v1(part_desc_entry_v1_t *p)
{
   u32 i;
   printf("[%s]\r\n", __func__);
   printf("%16s: %02x\r\n", "type"     , p->type);
   printf("%16s: %02x\r\n", "ro"           , p->ro);
   printf("%16s: %02x\r\n", "reserved"     , p->reserved);
   printf("%16s: %12lx\r\n", "length"       , p->length);
   printf("%16s: %02x\r\n", "fw_count"     , p->fw_count);
   printf("%16s: %02x\r\n", "fw_type"      , p->fw_type);
   printf("%16s: "        , "reserved_1");
   for (i = 0; i < sizeof(p->reserved_1); i++) { printf("%02x ", p->reserved_1[i]);};
   printf("\r\n");
   printf("%16s: "        , "mount_point");
   for (i = 0; i < 32; i++) { printf("%02x ", p->mount_point[i]);};
   printf("\r\n");

   printf("raw dump:\r\n");
   dump_rawdata((u8 *)p, sizeof(part_desc_entry_v1_t));
   return;


}



#if 0

typedef struct {
   u8 type;
#if BYTE_ORDER == LITTLE_ENDIAN
   u8 reserved:6,
      lzma:1,
      ro:1;
#elif BYTE_ORDER == BIG_ENDIAN
   u8 ro:1,
      lzma:1,
      reserved:6;
#else
#error "Please fix <asm/byteorder.h>"
#endif
   u32   version;
   u32   target_addr;
   u32   offset;
   u32   length;
   u32   paddings;
   u32   checksum;
   u8 reserved_1[6];
} __attribute__((packed)) fw_desc_entry_v1_t;

typedef struct {
    fw_desc_entry_v1_t v1;
    u32 act_size;
    u8  hash[32];
    u8  part_num;
    u8  reserved[27];
} __attribute__((packed)) fw_desc_entry_v11_t;
typedef struct {
    fw_desc_entry_v1_t v1;
    u32 act_size;
    u8  part_num;
    u8  RSA_sign[256];
    u8  reserved[27];
} __attribute__((packed)) fw_desc_entry_v21_t;

#endif

void dump_fw_desc_entry_v1(fw_desc_entry_v1_t *p)
{
   u32 i;
   printf("[%s]\r\n", __func__);
   printf("%16s: %02x\r\n", "type"         , p->type);
   printf("%16s: %02x\r\n", "reserved"     , p->reserved);
   printf("%16s: %02x\r\n", "lzma"         , p->lzma);
   printf("%16s: %02x\r\n", "ro"           , p->ro);
   printf("%16s: %08x\r\n", "version"      , p->version);
   printf("%16s: %08x\r\n", "target_addr"  , p->target_addr);
   printf("%16s: %08x\r\n", "offset"       , p->offset);
   printf("%16s: %08x\r\n", "length"       , p->length);
   printf("%16s: %08x\r\n", "paddings"     , p->paddings);
   printf("%16s: %08x\r\n", "checksum"     , p->checksum);
   printf("%16s: "        , "reserved_1");
   for (i = 0; i < 6; i++) { printf("%02x ", p->reserved_1[i]);};
   printf("\r\n");

   printf("raw dump:\r\n");
   dump_rawdata((u8 *)p, sizeof(fw_desc_entry_v1_t));
   return;
}

void dump_fw_desc_entry_v11(fw_desc_entry_v11_t *p)
{
   u32 i;
   printf("[%s]\r\n", __func__);
   dump_fw_desc_entry_v1(&p->v1);
   printf("%16s: %08x\r\n", "act_size"     , p->act_size);
   printf("%16s: "        , "hash");
   for (i = 0; i < 32; i++) { printf("%02x ", p->hash[i]);};
   printf("\r\n");
   printf("%16s: %02x\r\n", "part_num"     , p->part_num);
   printf("%16s: "        , "reserved");
   for (i = 0; i < 27; i++) { printf("%02x ", p->reserved[i]);};
   printf("\r\n");

   printf("raw dump:\r\n");
   dump_rawdata((u8 *)p, sizeof(fw_desc_entry_v11_t));
   return;
}

void dump_fw_desc_entry_v21(fw_desc_entry_v21_t *p)
{
   u32 i;
   printf("[%s]\r\n", __func__);
   dump_fw_desc_entry_v1(&p->v1);
   printf("%16s: %08x\r\n", "act_size"     , p->act_size);
   printf("%16s: %02x\r\n", "part_num"     , p->part_num);
   printf("%16s: "        , "RSA_sign");
   for (i = 0; i < 256; i++) { printf("%02x ", p->RSA_sign[i]);};
   printf("\r\n");
   printf("%16s: "        , "reserved");
   for (i = 0; i < 27; i++)  { printf("%02x ", p->reserved[i]);};
   printf("\r\n");

   printf("raw dump:\r\n");
   dump_rawdata((u8 *)p, sizeof(fw_desc_entry_v21_t));
   return;
}


#if 0

typedef struct {
   u8 type;
#if BYTE_ORDER == LITTLE_ENDIAN
   u8 reserved:6,
      lzma:1,
      ro:1;
#elif BYTE_ORDER == BIG_ENDIAN
   u8 ro:1,
      lzma:1,
      reserved:6;
#else
#error "Please fix <asm/byteorder.h>"
#endif
   u32   version;
   u32   target_addr;
   u32   offset;
   u32   length;
   u32   paddings;
   u32   checksum;
   u8 reserved_1[6];

    u32 act_size;
    u8  part_num;
    u8  RSA_sign[256];
    u8  reserveded[27];
} __attribute__((packed)) rtk_fw_desc_entry_v21_t;


#endif

int save_fwdesc_nor(struct rtk_fw_header* fw)
{
   int i;
   int dev_fd;
   int ret;

   memset(&fw->fw_tab, 0xff, sizeof(fw_desc_table_v1_t));
   memcpy(fw->fw_tab.signature, VERONA_SIGNATURE, 8);
   fw->fw_tab.checksum = 0;
   fw->fw_tab.version= 0;
   fw->fw_tab.fw_list_len = 0;
   //fw->fw_tab.part_list_len = 0;
   // checksum fw_desc_entry
   for(i=0;i<FW_TYPE_UNKNOWN;i++)
   {
      if(FW_IS_SET(fw, i))
      {
         be32_to_cpu(fw->fw_desc[i].v1.target_addr);
         be32_to_cpu(fw->fw_desc[i].v1.offset);
         be32_to_cpu(fw->fw_desc[i].v1.length);
         be32_to_cpu(fw->fw_desc[i].v1.paddings);
         be32_to_cpu(fw->fw_desc[i].v1.checksum);

         fw->fw_tab.checksum += \
         get_checksum((u8*) &fw->fw_desc[i] \
         , sizeof(fw_desc_entry_v1_t));
         fw->fw_tab.fw_list_len += sizeof(fw_desc_entry_v1_t);
         dump_rawdata((u8*) &fw->fw_desc[i], sizeof(fw_desc_entry_v1_t));
      }
   }

   be32_to_cpu(fw->fw_tab.part_list_len);
   be32_to_cpu(fw->fw_tab.fw_list_len);

   // checksum fw_tab
   fw->fw_tab.checksum += get_checksum((u8*) &fw->fw_tab+12, sizeof(fw_desc_table_v1_t)-12);
   dump_rawdata((u8*) &fw->fw_tab+12, sizeof(fw_desc_table_v1_t)-12);

   install_debug("checksum:0x%08X\r\n", fw->fw_tab.checksum);
   be32_to_cpu(fw->fw_tab.checksum);

   // write
   dev_fd = rtk_open_mtd_block();
   if(dev_fd < 0)
   {
      install_debug("rtk_open_mtd_block() fail\r\n");
      return -1;
   }
   ret = lseek(dev_fd, 0, SEEK_SET);
   if(ret < 0)
   {
      install_debug("lseek() fail\r\n");
      return -1;
   }

   // write fw_tab
   write(dev_fd, &fw->fw_tab, sizeof(fw_desc_table_v1_t));

   // write fw_desc
   if(FW_IS_SET(fw, FW_TYPE_KERNEL))
   {
      //printf("write 0x%02x\r\n", i);
      write(dev_fd, &fw->fw_desc[FW_TYPE_KERNEL], sizeof(fw_desc_entry_v1_t));
   }

   if(FW_IS_SET(fw, FW_TYPE_AUDIO))
   {
      //printf("write 0x%02x\r\n", i);
      write(dev_fd, &fw->fw_desc[FW_TYPE_AUDIO], sizeof(fw_desc_entry_v1_t));
   }

   if(FW_IS_SET(fw, FW_TYPE_VIDEO))
   {
      //printf("write 0x%02x\r\n", i);
      write(dev_fd, &fw->fw_desc[FW_TYPE_VIDEO], sizeof(fw_desc_entry_v1_t));
   }

   if(FW_IS_SET(fw, FW_TYPE_SQUASH))
   {
      //printf("write 0x%02x\r\n", i);
      write(dev_fd, &fw->fw_desc[FW_TYPE_SQUASH], sizeof(fw_desc_entry_v1_t));
   }

   if(FW_IS_SET(fw, FW_TYPE_JFFS2))
   {
      //printf("write 0x%02x\r\n", i);
      write(dev_fd, &fw->fw_desc[FW_TYPE_JFFS2], sizeof(fw_desc_entry_v1_t));
   }

   if(FW_IS_SET(fw, FW_TYPE_BOOTCODE))
   {
      //printf("write 0x%02x\r\n", i);
      write(dev_fd, &fw->fw_desc[FW_TYPE_BOOTCODE], sizeof(fw_desc_entry_v1_t));
   }

   if(FW_IS_SET(fw, FW_TYPE_AUDIO_FILE))
   {
      //printf("write 0x%02x\r\n", i);
      write(dev_fd, &fw->fw_desc[FW_TYPE_AUDIO_FILE], sizeof(fw_desc_entry_v1_t));
   }

   if(FW_IS_SET(fw, FW_TYPE_VIDEO_FILE))
   {
      //printf("write 0x%02x\r\n", i);
      write(dev_fd, &fw->fw_desc[FW_TYPE_VIDEO_FILE], sizeof(fw_desc_entry_v1_t));
   }

   close(dev_fd);
   return 0;

}

int save_fwdesc(struct rtk_fw_header* fw, struct t_rtkimgdesc* prtkimgdesc)
{
   int i;
   int dev_fd;

   fw->fw_tab.checksum = 0;
   fw->fw_tab.part_list_len = fw->part_count*sizeof(part_desc_entry_v1_t);
   if(0 == fw->secure_boot)
   {
      fw->fw_tab.fw_list_len = fw->fw_count*sizeof(fw_desc_entry_v1_t);
   }
   else
   {
      fw->fw_tab.fw_list_len = fw->fw_count*sizeof(fw_desc_entry_v21_t);
   }
   install_debug("version:0x%02x fw_count:%d, part_count:%d\r\n",fw->fw_tab.version , fw->fw_count, fw->part_count);


   // part_count setting, checksum part_desc_entry
   for(i=0;i<(int)fw->part_count;i++)
   {
      fw->fw_tab.checksum += get_checksum((u8*) &fw->part_desc[i], sizeof(part_desc_entry_v1_t));
   }
   // fw_desc setting, checksum fw_desc_entry
   for(i=0;i<(int)fw->fw_count;i++)
   {
      if(0 == fw->secure_boot)
         fw->fw_tab.checksum += get_checksum((u8*) &fw->fw_desc[i], sizeof(fw_desc_entry_v1_t));
      else
         fw->fw_tab.checksum += get_checksum((u8*) &fw->fw_desc[i], sizeof(fw_desc_entry_v21_t));
   }

   // fw_tab setting
   // checksum fw_tab
   fw->fw_tab.checksum += get_checksum((u8*) &fw->fw_tab+12, sizeof(fw_desc_table_v1_t)-12);
   install_debug("checksum:0x%08x\r\n", fw->fw_tab.checksum);

   char path[128] = {0};
   snprintf(path, sizeof(path), "%s/%s", PKG_TEMP, FWDESCTABLE);
   unlink(path);
   if ((dev_fd = open(path, O_RDWR|O_SYNC|O_CREAT, 0644)) < 0) {
      install_fail("error! open file %s fail\r\n", path);
      return -1;
   }

   // write fw_tab
   write(dev_fd, &fw->fw_tab, sizeof(fw_desc_table_v1_t));
   //dump_rawdata((u_int8_t*)&fw->fw_tab, sizeof(fw_desc_table_v1_t));
#ifdef _DEBUG
   install_test("fw table\r\n");
   dump_fw_desc_table_v1(&fw->fw_tab);
#endif

   // write part_desc
   for(i=0;i<(int)fw->part_count;i++) {
      write(dev_fd, &fw->part_desc[i], sizeof(part_desc_entry_v1_t));
      //dump_rawdata((u_int8_t*)&fw->part_desc[i], sizeof(part_desc_entry_v1_t));
#ifdef _DEBUG
      install_test("part desc[%d]\r\n",i);
      dump_part_desc_entry_v1(&fw->part_desc[i]);
#endif
   }

   // write fw_desc
   for(i=0;i<(int)fw->fw_count;i++) {
      if(0 == fw->secure_boot) {
         write(dev_fd, &fw->fw_desc[i], sizeof(fw_desc_entry_v1_t));
         //dump_rawdata((u_int8_t*)&fw->fw_desc[i], sizeof(fw_desc_entry_v1_t));
#ifdef _DEBUG
         install_test("fw desc[%d]\r\n",i);
         dump_fw_desc_entry_v21(&fw->fw_desc[i]);
#endif
      }
      else {
         write(dev_fd, &fw->fw_desc[i], sizeof(fw_desc_entry_v21_t));
         //dump_rawdata((u_int8_t*)&fw->fw_desc[i], sizeof(fw_desc_entry_v21_t));
#ifdef _DEBUG
         install_test("fw desc[%d]\r\n",i);
         dump_fw_desc_entry_v21(&fw->fw_desc[i]);
#endif
      }
   }

   // for mp tools only..
   rtk_find_dir_path( prtkimgdesc->fw[FW_KERNEL].filename,
   		prtkimgdesc->fw[FW_FW_TBL].filename, sizeof(prtkimgdesc->fw[FW_FW_TBL].filename));
   strcat( prtkimgdesc->fw[FW_FW_TBL].filename, FWDESCTABLE);
   prtkimgdesc->fw[FW_FW_TBL].img_size = sizeof(fw->fw_tab) + fw->fw_tab.part_list_len + fw->fw_tab.fw_list_len;

   close(dev_fd);

#ifndef __OFFLINE_GENERATE_BIN__
    struct t_imgdesc *pimgdesc = &prtkimgdesc->fw[FW_FW_TBL];
    char cmd[128];
    // flash_erase
    snprintf(cmd, sizeof(cmd), "%s %s %llu %llu"
                             , FLASHERASE_BIN
                             , prtkimgdesc->mtd_path
                             , pimgdesc->flash_offset
                             , pimgdesc->sector);
    if( rtk_command(cmd, __LINE__, __FILE__) < 0)
    {
       install_debug("Exec command fail\r\n");
       return -1;
    }

    snprintf(cmd, sizeof(cmd), "%s -s %llu -p %s %s"
                      , NANDWRITE_BIN
                      , pimgdesc->flash_offset
                      , prtkimgdesc->mtd_path, path);

    if( rtk_command(cmd, __LINE__, __FILE__) < 0)
    {
       install_debug("Exec command fail\r\n");
       return -1;
    }

    // erase old fw table, it should be removed later...
    if( (0x7c00000 >= prtkimgdesc->fw[FW_P_FREE_SPACE].flash_offset) &&
        (0x7c00000 < (prtkimgdesc->fw[FW_P_FREE_SPACE].flash_offset+prtkimgdesc->fw[FW_P_FREE_SPACE].flash_allo_size)))
    {
	    install_info("[@@@@@]erase old fw table(0x7c00000)[@@@@@]\n");
	    snprintf(cmd, sizeof(cmd), "%s %s %u %u"
	                             , FLASHERASE_BIN
	                             , prtkimgdesc->mtd_path
	                             , 0x7c00000
	                             , 1);
	    if( rtk_command(cmd, __LINE__, __FILE__) < 0)
	    {
	       install_debug("Exec command fail\r\n");
	       return -1;
	    }
    }
#endif

   return 0;
}

#ifdef EMMC_SUPPORT
int save_fwdesc_emmc(struct rtk_fw_header* fw, struct t_rtkimgdesc* prtkimg)
{
   int i, len=0;
   int dev_fd;
   int bytecnt=0;

   fw->fw_tab.checksum = 0;
   fw->fw_tab.part_list_len = fw->part_count*sizeof(part_desc_entry_v1_t);
   if(0 == fw->secure_boot)
   {
      fw->fw_tab.fw_list_len = fw->fw_count*sizeof(fw_desc_entry_v1_t);
   }
   else
   {
      fw->fw_tab.fw_list_len = fw->fw_count*sizeof(fw_desc_entry_v21_t);
   }
   install_debug("version:0x%02x fw_count:%d, part_count:%d\r\n",fw->fw_tab.version , fw->fw_count, fw->part_count);


   // part_count setting, checksum part_desc_entry
   for(i=0;i<(int)fw->part_count;i++) {
      fw->fw_tab.checksum += get_checksum((u8*) &fw->part_desc[i], sizeof(part_desc_entry_v1_t));
	  bytecnt += sizeof(part_desc_entry_v1_t);
   }

   // fw_desc setting, checksum fw_desc_entry
   for(i=0;i<(int)fw->fw_count;i++)
   {
      if(0 == fw->secure_boot) {
         fw->fw_tab.checksum += get_checksum((u8*) &fw->fw_desc[i], sizeof(fw_desc_entry_v1_t));
		 bytecnt += sizeof(fw_desc_entry_v1_t);
	  }
      else {
         fw->fw_tab.checksum += get_checksum((u8*) &fw->fw_desc[i], sizeof(fw_desc_entry_v21_t));
		 bytecnt += sizeof(fw_desc_entry_v21_t);
      }
   }

   // "paddings" is not real paddings. It controls how many bytes the bootcode reads.
   // padding is also calculated into checksum, so we pre-add the bytecnt and give a paddings value
   bytecnt += (sizeof(fw_desc_table_v1_t)-12);
   fw->fw_tab.paddings = SIZE_ALIGN_BOUNDARY_MORE(bytecnt, prtkimg->mtd_erasesize);

   // fw_tab setting
   // checksum fw_tab
   fw->fw_tab.checksum += get_checksum((u8*) &fw->fw_tab+12, sizeof(fw_desc_table_v1_t)-12);

   install_debug("checksum:0x%08x\r\n", fw->fw_tab.checksum);

#ifdef PC_SIMULATE
    char path[128] = {0};
    snprintf(path, sizeof(path), "%s/%s", PKG_TEMP, FWDESCTABLE);
    unlink(path);
    if ((dev_fd = open(path, O_RDWR|O_SYNC|O_CREAT, 0644)) < 0) {
       install_fail("error! open file %s fail\r\n", path);
       return -1;
    }
#else
   if ((dev_fd = open(prtkimg->mtdblock_path, O_RDWR|O_SYNC)) < 0) {
      install_fail("error! open file %s fail\r\n", prtkimg->mtdblock_path);
      return -1;
   }

	lseek(dev_fd, prtkimg->reserved_boot_size, SEEK_SET);
#endif

   // write fw_tab
	write(dev_fd, &fw->fw_tab, sizeof(fw_desc_table_v1_t));
	len += sizeof(fw_desc_table_v1_t);
#ifdef _DEBUG
      install_test("fw table\r\n");
      dump_fw_desc_table_v1(&fw->fw_tab);
#endif

   // write part_desc
   for(i=0;i<(int)fw->part_count;i++) {
		write(dev_fd, &fw->part_desc[i], sizeof(part_desc_entry_v1_t));
		len += sizeof(part_desc_entry_v1_t);
#ifdef _DEBUG
      install_test("part desc[%d]\r\n",i);
      dump_part_desc_entry_v1(&fw->part_desc[i]);
#endif
   }

   // write fw_desc
   for(i=0;i<(int)fw->fw_count;i++) {
      if(0 == fw->secure_boot) {
		write(dev_fd, &fw->fw_desc[i], sizeof(fw_desc_entry_v1_t));
		len += sizeof(fw_desc_entry_v1_t);
#ifdef _DEBUG
         install_test("fw desc[%d]\r\n",i);
         dump_fw_desc_entry_v21(&fw->fw_desc[i]);
#endif
      }
      else {
		write(dev_fd, &fw->fw_desc[i], sizeof(fw_desc_entry_v21_t));
		len += sizeof(fw_desc_entry_v21_t);
#ifdef _DEBUG
         install_test("fw desc[%d]\r\n",i);
         dump_fw_desc_entry_v21(&fw->fw_desc[i]);
#endif
      }
   }

   // for mp tools only..
   rtk_find_dir_path( prtkimg->fw[FW_KERNEL].filename,
   		prtkimg->fw[FW_FW_TBL].filename, sizeof(prtkimg->fw[FW_FW_TBL].filename));
   strcat( prtkimg->fw[FW_FW_TBL].filename, FWDESCTABLE);
   prtkimg->fw[FW_FW_TBL].img_size = sizeof(fw->fw_tab) + fw->fw_tab.part_list_len + fw->fw_tab.fw_list_len;

	close(dev_fd);

   return 0;
}
#endif
int copy_fw_from_local_to_file(struct t_rtkimgdesc* prtkimgdesc, fw_type_code_t efwtype, const char* filename)
{

   int fd, dev_fd, ret=0;
   unsigned int len;
   struct rtk_fw_header fw;
   char lzma_path[128];
   char command[128];
   // sanity-check
   if(NULL == filename)
   {
      install_debug("filename is NULL\r\n");
      return -1;
   }

   // load fwdesc
   if(prtkimgdesc->flash_type == MTD_NANDFLASH)
   {
      ret = load_fwdesc(&fw, prtkimgdesc->reserved_boot_size);
      sprintf(lzma_path, "%s", filename);
   }
   else if(prtkimgdesc->flash_type == MTD_NORFLASH || prtkimgdesc->flash_type == MTD_DATAFLASH)
   {
      ret = load_fwdesc_nor(&fw);
      sprintf(lzma_path, "%s.lzma", filename);
   }

   if(ret < 0)
   {
      install_debug("load_fwdesc() fail\r\n");
      return -1;
   }

   if(fw.fw_tab.version == FIRMWARE_DESCRIPTION_TABLE_VERSION_21)
   {
      install_debug("Not support extract secure video firmware\r\n");
      return -1;
   }
   install_log("fw_type:%d\r\n", efwtype);
   if(0 == FW_IS_SET(&fw, efwtype))
   {
      install_debug("FW(%d) don't exist\r\n", efwtype);
      return -1;
   }


   // open stotredfile
   fd = open(lzma_path, O_CREAT|O_WRONLY|O_TRUNC, 0644);
   if(fd < 0)
   {
      install_debug("Can't open filename:%s\r\n", filename);
      return -1;
   }

   // open source device
   be32_to_cpu(fw.fw_desc[efwtype].v1.offset);
   be32_to_cpu(fw.fw_desc[efwtype].v1.length);
   install_log("We will seek 0x%08x\r\n", fw.fw_desc[efwtype].v1.offset);
   //dev_fd = rtk_open_mtd_block();
   dev_fd = rtk_open_mtd_block_with_offset(fw.fw_desc[efwtype].v1.offset);
   if(dev_fd < 0)
   {
      install_debug("Can't open mtd_block\r\n");
      return -1;
   }

   len = fd_to_fd(dev_fd, fd, fw.fw_desc[efwtype].v1.length);
   close(dev_fd);
   close(fd);
   if(len != fw.fw_desc[efwtype].v1.length)
   {
      install_debug("fd_to_fd fail\r\n");
      return -1;
   }

   if(prtkimgdesc->flash_type == MTD_NORFLASH || prtkimgdesc->flash_type == MTD_DATAFLASH)
   {
      sprintf(command, "unlzma %s", lzma_path);
      ret = rtk_command(command, __LINE__, __FILE__);
      if(ret < 0)
      {
         install_debug("Can't extract lzma file\r\n");
         return -1;
      }

   }

   return 0;
}

int copy_videofw_from_local_to_file(struct t_rtkimgdesc* prtkimgdesc, const char* filename)
{
   if(NULL == filename)
   {
      filename = "video_firmware.install.bin";
   }
   return copy_fw_from_local_to_file(prtkimgdesc, FW_TYPE_VIDEO, filename);
}

// not used
int findNandReservedSpaceStartEndAddress(unsigned int* startAddress, unsigned int* endAddress)
{
   int ret;
   struct rtk_fw_header fw;

   ret = load_fwdesc(&fw);
   if(ret < 0)
   {
      install_debug("load_fwdesc fail\r\n");
      return -1;
   }

   if(FW_IS_SET(&fw, FW_TYPE_VIDEO))
   {
      //printf("write 0x%02x\r\n", i);
      *startAddress = get_be32_to_cpu(fw.fw_desc[FW_TYPE_VIDEO].v1.offset) + get_be32_to_cpu(fw.fw_desc[FW_TYPE_VIDEO].v1.paddings);
   }

   if(FW_IS_SET(&fw, FW_TYPE_AUDIO_FILE))
   {
      //printf("write 0x%02x\r\n", i);
      *startAddress = get_be32_to_cpu(fw.fw_desc[FW_TYPE_AUDIO_FILE].v1.offset) + get_be32_to_cpu(fw.fw_desc[FW_TYPE_AUDIO_FILE].v1.paddings);
   }

   if(FW_IS_SET(&fw, FW_TYPE_VIDEO_FILE))
   {
      //printf("write 0x%02x\r\n", i);
      *startAddress = get_be32_to_cpu(fw.fw_desc[FW_TYPE_VIDEO_FILE].v1.offset) + get_be32_to_cpu(fw.fw_desc[FW_TYPE_VIDEO_FILE].v1.paddings);
   }

   *startAddress = *startAddress;// + rtk_get_erasesize();

   if(FW_IS_SET(&fw, FW_TYPE_SQUASH))
   {
      //printf("write 0x%02x\r\n", i);
      *endAddress = get_be32_to_cpu(fw.fw_desc[FW_TYPE_SQUASH].v1.offset);
   }
   install_log("startAddress=0x%08x, endAddress=0x%08x\r\n", *startAddress, *endAddress);
   return 0;
}

int findNandBackupAreaStartEndAddress(unsigned int* startAddress, unsigned int* endAddress, unsigned int _NAND_BOOTCODE_RESERVED_SIZE)
{
   int ret;
   int dev_fd;
   int i;
   unsigned int offset, paddings, fw_len;
   fw_desc_table_v1_t fw_tab;
   part_desc_entry_v1_t part_desc;
   fw_desc_entry_v1_t fw_entry;

   dev_fd = rtk_open_mtd_block_with_offset(_NAND_BOOTCODE_RESERVED_SIZE);
   if(dev_fd < 0)
   {
      install_debug("rtk_open_mtd_block_with_offset() fail\r\n");
      return -1;
   }

   // read fw_tab
   ret = read(dev_fd, &fw_tab, sizeof(fw_desc_table_v1_t));
   if(ret < 0)
   {
      install_debug("read() fail\r\n");
      return -1;
   }
   install_log("version:0x%02x #part:%u #fw:%u checksum:0x%08x\r\n"
   , fw_tab.version
   , get_be32_to_cpu(fw_tab.part_list_len)/sizeof(part_desc_entry_v1_t)
   , get_be32_to_cpu(fw_tab.fw_list_len)/sizeof(fw_desc_entry_v1_t)
   , get_be32_to_cpu(fw_tab.checksum));

   // check if secure
   if(fw_tab.version == 0x21)
   {
      // secure
      fw_len = sizeof(fw_desc_entry_v21_t);
   }
   else if(fw_tab.version == 0x01)
   {
      // non-secure
      fw_len = sizeof(fw_desc_entry_v1_t);
   }
   else
   {
      install_debug("Wrong boottable version\r\n");
      return -1;

   }

   //sanity check
   if(get_be32_to_cpu(fw_tab.part_list_len)/sizeof(part_desc_entry_v1_t) > 10)
   {
      install_debug("part_list_len > 5\r\n");
      return -1;
   }

   // read part_desc
   for(i=0;i<(int)(get_be32_to_cpu(fw_tab.part_list_len)/sizeof(part_desc_entry_v1_t));i++)
   {
      ret = read(dev_fd, &part_desc, sizeof(part_desc_entry_v1_t));
      install_log("part type:0x%02x, length=0x%08x, fw_count=0x%02x, fw_type=0x%02x, mount=%s\r\n"   \
         , part_desc.type
         , get_be32_to_cpu(part_desc.length)
         , part_desc.fw_count
         , part_desc.fw_type
         , part_desc.mount_point);
   }

   // read fw_desc
   ret = read(dev_fd, &fw_entry, fw_len);
   offset = get_be32_to_cpu(fw_entry.offset);
   paddings = get_be32_to_cpu(fw_entry.paddings);
   for(i=1;i<(int)(get_be32_to_cpu(fw_tab.fw_list_len)/fw_len);i++)
   {
      ret = read(dev_fd, &fw_entry, sizeof(fw_desc_entry_v1_t));
      install_log("fwtype:0x%02x, target_addr=0x%08x, offset=0x%08x, length=0x%08x, paddings=0x%08x\r\n"\
         , fw_entry.type
         , get_be32_to_cpu(fw_entry.target_addr)
         , get_be32_to_cpu(fw_entry.offset)
         , get_be32_to_cpu(fw_entry.length)
         , get_be32_to_cpu(fw_entry.paddings));

      if((offset+paddings) == get_be32_to_cpu(fw_entry.offset))
      {
         offset = get_be32_to_cpu(fw_entry.offset);
         paddings = get_be32_to_cpu(fw_entry.paddings);
      }
      else if((offset+paddings) < get_be32_to_cpu(fw_entry.offset))
      {
         *startAddress = offset+paddings;
         *endAddress = get_be32_to_cpu(fw_entry.offset);
         break;
      }
      else
      {
         install_debug("Can't find backup area\r\n");
         close(dev_fd);
         return -1;
      }
   }
   close(dev_fd);
   if(i == (int)(get_be32_to_cpu(fw_tab.fw_list_len)/fw_len))
   {
      install_debug("Can't find backup area\r\n");
      return -1;
   }
   install_log("startAddress=0x%08x, endAddress=0x%08x, size= %u Bytes = %u KB = %u MB\r\n", *startAddress, *endAddress, (*endAddress-*startAddress), (*endAddress-*startAddress)>>10, (*endAddress-*startAddress)>>20);
   return 0;
}

int check_boot_code_size(unsigned int *nand_boot_size, unsigned int *factory_size)
{
   int fd = 0;
   int ret;
   char sys_parameters[256];
   char *ptr, *ptr1;
   const char *sep=": \t\n";

   *nand_boot_size = *factory_size = 0;
   memset(sys_parameters, 0 , sizeof(sys_parameters));

   fd = open(SYSTEM_PARAMETERS, O_RDONLY);

   if(fd < 0)
   {
      install_debug("ERROR: open %s error.\n", SYSTEM_PARAMETERS);
      return -1;
   }
   ret = read(fd, sys_parameters, sizeof(sys_parameters));
   if(ret < 0)
   {
      install_debug("ERROR: can not read system parameters.\n");
      return -1;
   }

   // find nand_boot_size
   ptr = strstr(sys_parameters, "nand_boot_size");
   if(!ptr) {
      close(fd);
      return -1;
   }

   ptr = strtok(sys_parameters, sep);
   while(ptr){
      ptr = strtok(NULL, sep);
      if(!ptr)
         continue;
      if(strstr(ptr, "nand_boot_size")) {
         ptr1 = strstr(ptr, "=");
         ptr1++;
         *nand_boot_size = strtol(ptr1, &ptr, 16);
         install_log("nand_boot_size 0x%x\r\n", *nand_boot_size);
      }
      if(strstr(ptr, "factory_size")) {
         ptr1 = strstr(ptr, "=");
         ptr1++;
         *factory_size = strtol(ptr1, &ptr, 16);
         install_log("factory_size 0x%x\n", *factory_size);
      }
   }

   close(fd);
   return 0;
}

