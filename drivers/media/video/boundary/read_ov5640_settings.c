#include <stdio.h>
#include "ov5640.h"
#include <dirent.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <zlib.h>

static char const *fourcc_str(unsigned long fcc,char *buf)
{
	memcpy(buf,&fcc,sizeof(fcc));
	buf[sizeof(fcc)] = '\0' ;
	return buf ;
}

#define HTOTALH 0x380c
#define HTOTALL 0x380d
#define VTOTALH 0x380e
#define VTOTALL 0x380f

int main (int argc, char const **argv) {
	int retval = -1 ;
	FILE *fIn ;

	if (2 > argc) {
		fprintf (stderr, "Usage: %s infile\n", argv[0]);
		return -1 ;
	}

	fIn = fopen(argv[1],"rb");
	if (0 != fIn) {
		unsigned num_settings ;
		if (sizeof(num_settings) == fread(&num_settings,1,sizeof(num_settings),fIn)) {
			unsigned i ;
                        struct ov5640_setting *settings ;
			settings = (struct ov5640_setting *)malloc(num_settings*sizeof(*settings));
			for (i = 0; i < num_settings; i++) {
                                struct ov5640_setting *s = settings+i ;
				if (OV5640_SETTING_SIZE == fread(s,1,OV5640_SETTING_SIZE,fIn)) {
					unsigned file_crc = s->crc ;
					unsigned crc ;
					unsigned const regsize = s->reg_count*sizeof(s->registers[0]);
					s->registers = 0 ; // force NULL pointer for CRC
					s->crc = 0 ;
                                        crc = adler32(0,(Bytef *)s,OV5640_SETTING_SIZE);
					s->crc = file_crc ;
					s->registers = (struct ov5640_reg_value *)malloc(regsize);
					if (regsize == fread((void *)s->registers,1,regsize,fIn)) {
						crc = adler32(crc,(Bytef *)s->registers,regsize);
						if (s->crc == crc) {
							continue;
						} else {
							printf ("crc mismatch:%08x:%08x\n",crc,s->crc);
							return -1 ;
						}
					}
				}

				fprintf (stderr, "%s: short read\n", argv[1]);
				return -1 ;
			}
			for (i=0; i < num_settings; i++) {
				char buf0[5];
				char buf1[5];
				char buf2[5];
				unsigned regno ;
                                struct ov5640_setting *s = settings+i ;
				unsigned htotal = 0, vtotal = 0 ;
				printf("%s\t%s\t%s\t%s\t%.2f\t%u\t%u\t%u\t%u\t%u%c\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\n",
				       s->name,
				       fourcc_str(s->v4l_fourcc,buf0),
				       fourcc_str(s->ipuin_fourcc,buf1),
				       fourcc_str(s->ipuout_fourcc,buf2),
				       s->fps,
				       s->xres,
				       s->yres,
				       s->stride,
				       s->sizeimage,
				       s->mclk > 1000000 ? s->mclk/1000000 : s->mclk/1000,
				       s->mclk > 1000000 ? 'M' : 'K',
				       s->hoffs,
				       s->voffs,
				       s->hmarg,
				       s->vmarg,
				       s->data_width,
				       s->clk_mode,
				       s->ext_vsync,
				       s->Vsync_pol,
				       s->Hsync_pol,
				       s->pixclk_pol,
				       s->data_pol,
				       s->pack_tight,
				       s->force_eof,
				       s->data_en_pol
				       );
				for (regno=0; regno < s->reg_count; regno++) {
					struct ov5640_reg_value const *r = s->registers+regno;
					printf("0x%04x,0x%02x\n",r->addr,r->value);
					if (HTOTALH == r->addr) {
						htotal = (htotal&0xff)|((r->value&0x0f)<<8);
					} else if (HTOTALL == r->addr) {
						htotal = (htotal&0xff00)|r->value;
					} else if (VTOTALH == r->addr) {
						vtotal = (vtotal&0xff)|((r->value&0x0f)<<8);
					} else if (VTOTALL == r->addr) {
						vtotal = (vtotal&0xff00)|r->value;
					}
				}
	printf("sizeof(struct ov5640_setting) == %lu\n", sizeof(struct ov5640_setting));
	printf("offsetof(struct ov5640_setting,fps) == %lu\n", offsetof(struct ov5640_setting,fps));
	printf("offsetof(struct ov5640_setting,mclk) == %lu\n", offsetof(struct ov5640_setting,mclk));
	printf("offsetof(struct ov5640_setting,registers) == %lu\n", offsetof(struct ov5640_setting,registers));
	printf("htotal == %u, hmarg == %u\n", htotal, htotal-s->xres);
	printf("vtotal == %u, vmarg == %u\n", vtotal, vtotal-s->yres);
			}
		}
		fclose (fIn);
		retval = 0 ;
	}
	else
		perror (argv[1]);

	return retval ;
}
