/*
 Copyright 2001 Crispin Perdue <cris@perdues.com>

 This is free software, and comes with ABSOLUTELY NO WARRANTY.
 You may distribute it under the terms of the GNU Public License.
*/

#include <unistd.h>
// #include <pwd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>


/*
  Opens the given file for appending, and replaces this executable
  with the executable for the given arguments.
 */
int main(int argc, char **argv) {
    int fd;
    int fd_save;
    int keep_stderr = 0;
    if (argc>=1 && strcmp("-e", argv[1])==0) {
      keep_stderr = 1;
      argv++;
      argc--;
    }
    if (argc<2) {
        fprintf(stderr, "Usage: logto [ -e ] file program [ arg ... ]\n");
        exit(1);
    }
    fd = open(argv[1],O_WRONLY|O_CREAT|O_APPEND, 0666);
    if (fd<0) {
        perror("open");
        exit(1);
    }
    if (dup2(fd, 1) == -1) {
      perror("redirecting stdout");
      exit(1);
    }
    /* Save the old stderr */
    fd_save = dup(2);
    
    /* Redirect stderr unless -e */
    if (!keep_stderr && dup2(fd, 2) == -1) {
      perror("redirecting stderr");
      exit(1);
    }

    execvp(argv[2], argv+2);

    /* Restore stderr */
    if (fd_save>=0)
      dup2(fd_save, 2);
    perror("exec");
    exit(1);
}
