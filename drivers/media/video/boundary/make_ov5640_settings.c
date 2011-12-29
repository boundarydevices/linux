#include <stdio.h>
#include "ov5640.h"
#include <dirent.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <zlib.h>
#include <sys/stat.h>

#define MAX_REGS 1024
#define MIN_REGNUM 0x3000
#define MAX_REGNUM 0x6FFF

static int valid_width(unsigned dataw) {
	return ((4 == dataw)
		||
                (8 == dataw)
                ||
                (10 == dataw)
                ||
                (16 == dataw));
}

static int parse_description(char const *line,struct ov5640_setting *s) {
	char 	 name[MAX_OV_NAME];
	char 	 vfourcc[5];
	char 	 ipuinfourcc[5];
	char 	 ipuoutfourcc[5];
	float 	 fps ;
	unsigned xres ;
	unsigned yres ;
	unsigned stride ;
	unsigned sizeimg ;
	unsigned mclk ;			// clock rate
	char	 mclkm ;		// 'M' or 'K'
	unsigned hoffs ;
	unsigned voffs ;
	unsigned hmarg ;
	unsigned vmarg ;
	unsigned data_width ;
	unsigned clk_mode ;
	unsigned ext_vsync ;
	unsigned Vsync_pol ;
	unsigned Hsync_pol ;
	unsigned pixclk_pol ;
	unsigned data_pol ;
	unsigned pack_tight ;
	unsigned force_eof ;
	unsigned data_en_pol ;
	if (25 == sscanf(line,"%15s %4s %4s %4s %f %u %u %u %u %u%c %u %u %u %u %u %u %u %u %u %u %u %u %u %u",
			name,
			vfourcc,
			ipuinfourcc,
			ipuoutfourcc,
			&fps,
			&xres,
			&yres,
			&stride,
			&sizeimg,
			&mclk,
			&mclkm,
			&hoffs,
			&voffs,
			&hmarg,
			&vmarg,
			&data_width,
			&clk_mode,
			&ext_vsync,
			&Vsync_pol,
			&Hsync_pol,
			&pixclk_pol,
			&data_pol,
			&pack_tight,
			&force_eof,
			&data_en_pol
			)) {
		if ((1 <= strlen(name))
		    &&
		    (4 == strlen(vfourcc))
		    &&
		    (4 == strlen(ipuinfourcc))
		    &&
		    (4 == strlen(ipuoutfourcc))
		    &&
		    (0 < xres)
		    &&
		    (0 < yres)
		    &&
		    (0.0 < fps)
		    &&
		    (0 <mclk)
		    &&
		    (('M' == mclkm)||('K'==mclkm))
		    &&
		    (hoffs<=hmarg)
		    &&
		    (voffs<=vmarg)
		    &&
		    valid_width(data_width)
		    &&
		    (8 > clk_mode)	// IPU_CSI_CLK_MODE_...
		    &&
		    (1 >= ext_vsync)
		    &&
		    (1 >= Vsync_pol)
		    &&
		    (1 >= Hsync_pol)
		    &&
		    (1 >= pixclk_pol)
		    &&
		    (1 >= data_pol)
		    &&
		    (1 >= pack_tight)
		    &&
		    (1 >= force_eof)
		    &&
		    (1 >= data_en_pol)
		    ){
			strcpy(s->name,name);
			memcpy(&s->v4l_fourcc,vfourcc,sizeof(s->v4l_fourcc));
			memcpy(&s->ipuin_fourcc,ipuinfourcc,sizeof(s->ipuout_fourcc));
			memcpy(&s->ipuout_fourcc,ipuoutfourcc,sizeof(s->ipuout_fourcc));
			s->fps = fps ;
			s->xres = xres ;
			s->yres = yres ;
			s->stride = stride ;
			s->sizeimage = sizeimg ;
			if ('M'==mclkm)
				s->mclk = mclk * 1000000 ;
			else
				s->mclk = mclk * 1000 ;
			s->hoffs = hoffs ;
			s->voffs = voffs ;
			s->hmarg = hmarg ;
			s->vmarg = vmarg ;
			s->data_width = data_width ;
			s->clk_mode = clk_mode ;
			s->ext_vsync = ext_vsync ;
			s->Vsync_pol = Vsync_pol ;
			s->Hsync_pol = Hsync_pol ;
			s->pixclk_pol = pixclk_pol ;
			s->data_pol = data_pol ;
			s->pack_tight = pack_tight ;
			s->force_eof = force_eof ;
			s->data_en_pol = data_en_pol ;
			return 1 ;
		}
	}
	return 0 ;
}

static int parse_register(char const *line,struct ov5640_reg_value *reg)
{
	unsigned regnum ;
	unsigned regval ;
	memset(reg,0,sizeof(*reg)); // clear tail-end byte
	if (2 == sscanf(line,"%i,%i",&regnum,&regval)) {
		if ((MAX_REGNUM>=regnum)
		    &&
		    (MIN_REGNUM<=regnum)
		    &&
		    (0x100>regval)) {
			reg->addr = regnum ;
			reg->value = regval ;
			return 1 ;
		}
		else
			fprintf (stderr, "register out of range: %x->%x\n", regnum, regval);
	}
	if (line[0] && ('\r' != line[0]))
		fprintf (stderr, "not register? %s\n", line);
	return 0 ;
}

struct ov5640_setting *process_file (char const *path) {
	struct ov5640_setting *rval = 0 ;
	FILE *fIn ;

	if (0 != (fIn = fopen(path,"rt"))) {
		int have_desc = 0 ;
		char buf[256];
                struct ov5640_setting *s = (struct ov5640_setting *)malloc(sizeof(*s));
                struct ov5640_reg_value *regs = (struct ov5640_reg_value *)malloc(MAX_REGS*sizeof(*regs));
		memset(s,0,sizeof(*s));
		s->registers = 0 ;
		s->reg_count = 0 ;
		s->name[0] = 0;
		while (0 != fgets(buf,sizeof(buf),fIn)) {
			if ('#' != buf[0]) {
				if (!have_desc) {
					have_desc = parse_description(buf,s);
					if (!have_desc)
						printf( "not description? %s\n", buf);
				} else {
					s->reg_count += parse_register(buf,regs+s->reg_count);
					if (MAX_REGS <= s->reg_count) {
						fprintf(stderr,"%s: too many registers\n", path);
						free(regs);
						free(s);
						return 0 ;
					}
				} // try to parse register line
			}
		}
		if (0 < s->reg_count) {
			unsigned long early_crc;
			s->crc = 0 ;
			s->registers = 0 ; // until after CRC
			early_crc = adler32(0,(const Bytef *)s,OV5640_SETTING_SIZE);
			s->crc = adler32(early_crc,(const Bytef *)regs,s->reg_count*sizeof(regs[0]));
			printf ("%s:%u registers: %08lx/%08x\n",path, s->reg_count,early_crc,s->crc);
                        s->registers = regs ;
			rval = s ;
		} else {
			free(regs);
			free(s);
		}
		fclose (fIn);
	}
	else
		perror(path);
	return rval ;
}

static int is_file(char const *path)
{
	struct stat st ;
	if (0 == stat(path,&st)) {
		return S_ISREG(st.st_mode);
	}
	else
		return 0 ;
}

int main (int argc, char const **argv) {
	char path[FILENAME_MAX];
	int retval = -1 ;
	DIR *dir ;
	if (3 > argc) {
		fprintf (stderr, "Usage: %s in_dir outfile\n", argv[0]);
		return -1 ;
	}

	dir = opendir (argv[1]);
	if (dir) {
		FILE *fOut = fOut = fopen(argv[2],"wb");
		if (0 != fOut) {
                        struct ov5640_setting **settings = 0 ;
			int file_count = 0 ;
			int num_valid = 0 ;
                        struct dirent *dent ;
			char *pathend ;
			strcpy(path,argv[1]);

			pathend= path+strlen(path);
			if ('/' != pathend[-1])
				*pathend++ = '/' ;

			while (0 != (dent=readdir(dir))) {
				strcpy(pathend,dent->d_name);
				if (is_file(path))
                                        ++file_count ;
			}
			rewinddir(dir);
                        settings = (struct ov5640_setting **)malloc(file_count*sizeof(settings[0]));
			while (0 != (dent=readdir(dir))) {
				strcpy(pathend,dent->d_name);
				if (is_file(path)) {
					if (0 != (settings[num_valid] = process_file (path)))
						num_valid++ ;
				}
			}

			fwrite (&num_valid,1,sizeof(num_valid),fOut);
			for (file_count = 0 ; file_count < num_valid ; file_count++) {
                                struct ov5640_setting *s = settings[file_count];
				fwrite(s,OV5640_SETTING_SIZE,1,fOut);
				fwrite(s->registers,s->reg_count*sizeof(s->registers[0]),1,fOut);
			}
			fclose (fOut);
			printf ("%d valid files\n", num_valid);
			retval = 0 ;
		} else
			perror (argv[2]);
		closedir(dir);
	}
	else
		perror (argv[1]);

	return retval ;
}
