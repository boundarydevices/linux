#include <stdio.h>
#include <ctype.h>
#include <string.h>

static void trimCtrl(char *buf){
        char *tail = buf+strlen(buf);
        // trim trailing <CR> if needed
        while ( tail > buf ) {
                --tail ;
                if ( iscntrl(*tail) ) {
                        *tail = '\0' ;
                }
                else
                        break;
        }
}

int main (int argc, char const *const argv[]) {
	if (2 <= argc) {
		FILE *fIn = fopen(argv[1],"rt");
		if (fIn) {
			char inBuf[256];
			unsigned numRegs = 0 ;
			while (0 != fgets(inBuf,sizeof(inBuf),fIn)) {
				trimCtrl(inBuf);
				unsigned reg, value ;
				if (2 == sscanf (inBuf,"78 %04x %x", &reg, &value)) {
                                        numRegs++ ;
					printf ("0x%04x,0x%02x\n", reg, value);
				} else
					fprintf (stderr, "ignore %s\n", inBuf);
			}
			fclose(fIn);
			fprintf (stderr, "read %u registers read from %s\n", numRegs, argv[1]);
		} else
			perror(argv[1]);
	} else
		fprintf (stderr,"Usage: %s inFile\n", argv[0]);
	return 0 ;
}
