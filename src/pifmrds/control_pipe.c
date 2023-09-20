/*
    PiFmAdv - Advanced FM transmitter for the Raspberry Pi
    Copyright (C) 2017 Miegl

    See https://github.com/Miegl/PiFmAdv
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "rds.h"
#include "control_pipe.h"

#define CTL_BUFFER_SIZE 70
#define GPIO_PAD_0_27                   (0x2C/4)
#define GPIO_PAD_28_45                  (0x30/4)

static volatile uint32_t *pad_reg;

int fd;
FILE *f_ctl;

/*
 * Opens a file (pipe) to be used to control the RDS coder, in non-blocking mode.
 */
int open_control_pipe(char *filename, volatile uint32_t *padreg)
{
	pad_reg = padreg;
	fd = open(filename, O_RDWR | O_NONBLOCK);
	if(fd == -1) return -1;

	int flags;
	flags = fcntl(fd, F_GETFL, 0);
	flags |= O_NONBLOCK;
	if( fcntl(fd, F_SETFL, flags) == -1 ) return -1;

	f_ctl = fdopen(fd, "r");
	if(f_ctl == NULL) return -1;

	return 0;
}
//for bad rds, as most rds decoders won't decode these
void removeDiacritics(char *str) {
    char diacritics[] = "ąćęłńóśźżĄĆĘŁŃÓŚŹŻ";
    char replacements[] = "acelnoszzACELNOSZZ";
    
    for (size_t i = 0; i < strlen(str); i++) {
        for (size_t j = 0; j < strlen(diacritics); j++) {
            if (str[i] == diacritics[j]) {
                str[i] = replacements[j];
                break;
            }
        }
    }
}

/*
 * Polls the control file (pipe), non-blockingly, and if a command is received,
 * processes it and updates the RDS data.
 */
int poll_control_pipe() {
	int res = -1;
	static char buf[CTL_BUFFER_SIZE];

	char *fifo = fgets(buf, CTL_BUFFER_SIZE, f_ctl);

	if(fifo == NULL) {
		return res;
	}

	if(strlen(fifo) > 3 && fifo[2] == ' ') {
		char *arg = fifo+3;
		if(arg[strlen(arg)-1] == '\n') arg[strlen(arg)-1] = 0;
		if(fifo[0] == 'P' && fifo[1] == 'S') {
			arg[8] = 0;
			set_rds_ps(arg);
			printf("PS set to: \"%s\"\n", arg);
			res =  CONTROL_PIPE_PS_SET;
		}
		else if(fifo[0] == 'R' && fifo[1] == 'T') {
			arg[64] = 0;
			set_rds_rt(arg);
			printf("RT set to: \"%s\"\n", arg);
			res = CONTROL_PIPE_RT_SET;
		}
        else if(fifo[0] == 'P' && fifo[1] == 'I') {
			arg[4] = 0;
			set_rds_pi((uint16_t) strtol(arg, NULL, 16));
			printf("PI set to: \"%s\"\n", arg);
			res = CONTROL_PIPE_PI_SET;
		}
		else if(fifo[0] == 'T' && fifo[1] == 'A') {
			int ta = ( strcmp(arg, "ON") == 0 );
			set_rds_ta(ta);
			printf("Set TA to ");
			if(ta) printf("ON\n"); else printf("OFF\n");
			res = CONTROL_PIPE_TA_SET;
		}
		else if(fifo[0] == 'T' && fifo[1] == 'P') {
			int tp = ( strcmp(arg, "ON") == 0 );
			set_rds_tp(tp);
			printf("Set TP to ");
			if(tp) printf("ON\n"); else printf("OFF\n");
			res = CONTROL_PIPE_TP_SET;
		}
		else if(fifo[0] == 'M' && fifo[1] == 'S') {
			int ms = ( strcmp(arg, "ON") == 0 );
			set_rds_ms(ms);
			printf("Set MS to ");
			if(ms) printf("ON\n"); else printf("OFF\n");
			res = CONTROL_PIPE_MS_SET;
		}
		else if(fifo[0] == 'A' && fifo[1] == 'B') {
			int ab = ( strcmp(arg, "ON") == 0 );
			set_rds_ab(ab);
			printf("Set AB to ");
			if(ab) printf("ON\n"); else printf("OFF\n");
			res = CONTROL_PIPE_AB_SET;
		}
	} else if(strlen(fifo) > 4 && fifo[3] == ' ') {
		char *arg = fifo+4;
		if(arg[strlen(arg)-1] == '\n') arg[strlen(arg)-1] = 0;
		if(fifo[0] == 'P' && fifo[1] == 'T' && fifo[2] == 'Y') {
			int pty = atoi(arg);
			if (pty >= 0 && pty <= 31) {
				set_rds_pty(pty);
				if (!pty) {
					printf("PTY disabled\n");
				} else {
					printf("PTY set to: %i\n", pty);
				}
			}
			else {
				printf("Wrong PTY identifier! The PTY range is 0 - 31.\n");
			}
			res = CONTROL_PIPE_PTY_SET;
		} else if(fifo[0] == 'U' && fifo[1] == 'R' && fifo[2] == 'T') {
			arg[64] = 0;
			removeDiacritics(arg);
			set_rds_rt(arg);
			printf("RT set to: \"%s\"\n", arg);
			res = CONTROL_PIPE_RT_SET;
		} else if(fifo[0] == 'P' && fifo[1] == 'W' && fifo[2] == 'R') {
			arg[1] = 0;
			pad_reg[GPIO_PAD_0_27] = 0x5a000018 + atoi(arg);
    		pad_reg[GPIO_PAD_28_45] = 0x5a000018 + atoi(arg);
			printf("POWER set to: \"%s\"\n", arg);
			res = CONTROL_PIPE_PWR_SET;
		}
	}

	return res;
}

int close_control_pipe() {
	if(f_ctl) fclose(f_ctl);
	if(fd) return close(fd);
	else return 0;
}