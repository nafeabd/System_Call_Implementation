/*
 *      file name : /hw1-nabdul/hw1/xhw1.c
 *
 *      author    : Nafees Ahmed Abdul
 *
 *      date      : 9th March 2014
 *
 *      File description : This file contains user module code which takes all the arguments user
 *			   has inputted through command prompt and acts as an interface 
 *			   to call actual system call implementation.In case of any errors ,respective
 *			   error no/descriptions are outputted to console.
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#define __NR_xconcat    349     /* our private syscall number */
#define __user
int main(int argc, char *argv[])
{
	struct myargs{
	 __user const char *outfile;
	 __user	const char **infiles;
		unsigned int infile_count;
		int oflags;
		mode_t mode ;
		unsigned int flags;
	};

	struct myargs user_input;
        /*Default mode if user specifies no mode */
	user_input.mode = 0755;
	extern char *optarg;
	extern int optind;
	int rc;
	int opt;
	int aFlag = 0;
	int cFlag = 0;
	int tFlag = 0;
	int eFlag = 0;
        int err = 0;
        int i = 0;
        char *str = malloc(sizeof(char *));
        char *mode_check = NULL;
	unsigned int Atomic_flag =0;
        unsigned int Nflag = 0;
        unsigned int Pflag = 0;
        unsigned int defaultflag = 0x00;
        static char usage[] = "usage: ./xhw1 [-acteANPmh] [output] [input.......]\n";
	while ((opt = getopt(argc, argv, "actehANPm:")) != -1)
		switch (opt) {
		case 'a':
			aFlag = O_APPEND;
			break;
		case 'c':
			cFlag = O_CREAT;
			break;
		case 't':
			tFlag = O_TRUNC;
			break;
		case 'e':
			eFlag = O_EXCL;
			break;
		case 'm':
                        user_input.mode = strtol(optarg, NULL, 8);
			mode_check = optarg;
			break;
		case 'A':
			Atomic_flag = 0x04;
			break;
		case 'P':
			Pflag = 0x02;
			break;
		case 'N':
			Nflag = 0x01;
			break;
		case 'h':
                        printf(usage);
			exit(1);
		case '?':
                        err = 1;
                        break;
		}

        /*
         * Mode checking if user passed invalid mode
         * Invalid mode is wrongly outputted octal combination
         *
         */
	if(mode_check != NULL){
		sprintf(str, "%o", user_input.mode);
		if(strcmp(str, mode_check) != 0){
			printf("Error:Invalid mode\n");
			free(str);
			exit(0);
		} 
        } 
	/*
	 * If any wrong combination of flags is inputted,
         * display the correct usage format.
         *
         */
        if(err){
             fprintf(stderr, usage, argv[0]);
             exit(1);
        }
        if(!aFlag && !cFlag && !tFlag && !eFlag){
                    user_input.oflags = O_APPEND | O_RDWR;

        }
        else {
                  user_input.oflags = aFlag | cFlag | tFlag | eFlag | O_RDWR;
        }
        
        user_input.flags = Atomic_flag | Pflag | Nflag | defaultflag;
	user_input.outfile = argv[optind];
	int p = (argc - optind - 1);
	user_input.infiles = malloc(sizeof(char*)*p);
	if (++optind < argc)
		for (;optind < argc; optind++)
		{
			user_input.infiles[i] = argv[optind];
			i++;
		}
	user_input.infile_count = p;
	free(str);
	rc = syscall(__NR_xconcat, (void *) &user_input,sizeof(user_input));
        if(rc >=  0)
           printf("%d\n", rc); 
        else
           printf("sys_xconcat failed. errno:%d (%s)\n",errno,strerror(errno));
       return 0;
}
