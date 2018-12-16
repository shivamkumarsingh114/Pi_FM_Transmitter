#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <math.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

volatile unsigned char *allof7e;

#define CM_GP0CTL (0x7e101070) //Clock control
#define GPFSEL0 (0x7E200000) // GPIO select 0
#define CM_GP0DIV (0x7e101074) //clock diviser to select a desired frequency
#define GPIO_BASE (0x7E200000)
// addresses of all I/O peripheral start at 7E0000000 and we need to access them
#define ACCESS(offset, type) (*(volatile type*)(offset+(int)allof7e-0x7e000000))
#define SETBIT(base, bit) ACCESS(base,int) |= 1<<bit
#define CLRBIT(base, bit) ACCESS(base,int) &= ~(1<<bit)

// declaring all the bits like mash by taking reference from datasheet
struct GPCTL {
        char SRC         : 4;
        char ENAB        : 1;
        char KILL        : 1;
        char             : 1;
        char BUSY        : 1;
        char FLIP        : 1;
        char MASH        : 2;
        unsigned int     : 13;
        char PASSWD      : 8;
};

void setup_fm() {
        // access of RD WR and SYNC by using O_RDWR and O_SYNC flags
        int mem_fd = open("/dev/mem", O_RDWR|O_SYNC);
        if (mem_fd < 0) {
                printf("can't open /dev/mem\n");
                exit(-1);
        }
        allof7e = (unsigned char *)mmap(
                        NULL,
                        0x01000000,
                        PROT_READ|PROT_WRITE,
                        MAP_SHARED,
                        mem_fd,
                        0x3f000000
                      );  // we have reserved this part of memory for our use

        if (allof7e == (unsigned char *)-1) {
                exit(-1);
        }

        close(mem_fd);

        SETBIT(GPFSEL0 , 14);
        CLRBIT(GPFSEL0 , 13);
        CLRBIT(GPFSEL0 , 12);

        ACCESS(CM_GP0CTL, struct GPCTL) = (struct GPCTL) {6, 1, 0, 0, 0, 1, 0x5a };
}

void close_fm() {
        static int close = 0;
        if (!close) {
                close = 1;
                printf("Closing Fm\n");
                ACCESS(CM_GP0CTL, struct GPCTL) = (struct GPCTL) {6, 0, 0, 0, 0, 0, 0x5a };
                exit(0);
        }
}

void modulate(int div){
         ACCESS(CM_GP0DIV,int) = (0x5a << 24) + div; // will give as desired freq
                                                    // by dividing the timer clk
         printf("%d\n",div);
}

void delay(int n) {
        int clock = 0;
        int i=0;
        for (i = 0; i < n; ++i) {
                ++clock;
        }
}
// we have taken page size of 1KB. This can be varied accordingly
void playWav(char *filename, int div, float bandwidth,int del) {
        int fp = open(filename, 'r');
        lseek(fp, 22, SEEK_SET); //Skip header
        short *data = (short *)malloc(1024);
        printf("Now broadcasting: %s\n", filename);

        while (read(fp, data,1024)) {
				int j=0;
                for (j = 0; j<1024/2; j++) {
            // DIVF  is  12-bit that is  why we  use  16-bit
            // audio  file Normalize the audio file and scale it to the bandwidthrequired.
            // (Audio_value/ (2^16))*bandwidth
            // This step make sure that the audio file values are limited to a range,
            // i.e. if bandwidth is 44.1 kHz then audio_values are normalized as 44.1 to -44.1
						float divif = (int)((float)(data[j])/65536.0*bandwidth);
                        modulate(divif+div);
                        delay(del);


                }
        }
}

int main(int argc, char **argv) {
        signal(SIGTERM, &close_fm);
        signal(SIGINT, &close_fm);
        atexit(&close_fm);
        printf("Test");
        setup_fm();

        float freq = atof(argv[2]); //getting the transmitted frequency from user
        float bandwidth;
        int div = (500/freq)*4096;
        modulate(div);
        int del;

        if (argc==3) {
                del = 3850; // delay of 3850ms was most suitable
                bandwidth = 44.1; //can be varied. 44.1 is optimum as wav file is
                                  //sampled at 22.05KHz
                printf("Setting up modulation: %f Mhz / %d @ %f\n",freq,div,bandwidth);
                playWav(argv[1], div, bandwidth,del);
        }
        else if (argc==5) {
          //if user want to control every paramter including bandwidth
          del = atoi(argv[4]);
          bandwidth = atof(argv[3]);
          printf("Setting up modulation: %f Mhz / %d @ %f\n",freq,div,bandwidth,delay);


          playWav(argv[1], div, bandwidth,del);
          }
        else {
                printf("provide proper input");
        }
        return 0;
}
