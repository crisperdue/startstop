/*
 Copyright 2001 Crispin Perdue <cris@perdues.com>

 This is free software, and comes with ABSOLUTELY NO WARRANTY.
 You may distribute it under the terms of the GNU Public License.
*/

#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <stdio.h>

/*
   Sets the uid, group id, and supplementary groups from the
   given user name, then execs the given program with the
   given command line arguments.  The only return is an
   error return, but the new program replaces the program
   running in this process, and it can exit in its own way.
 */
int main(int argc, char **argv) {
    struct passwd* p;
    if (argc<3) {
        fprintf(stderr, "Usage: setuser user program [ arg ... ]\n");
        exit(1);
    }
    p = getpwnam(argv[1]);
    if (p==NULL) {
        fprintf(stderr, "Can't find user %s\n", argv[1]);
        exit(1);
    }
    if (geteuid()==p->pw_uid) {
        /* Exit successfully if we are already the right user. */
        exit(0);
    }
    if (initgroups(argv[1], p->pw_gid)) {
        perror("initgroups");
	exit(1);
    }
    if (setgid(p->pw_gid)) {
        perror("setgid");
	exit(1);
    }
    if (setuid(p->pw_uid)) {
        perror("setuid");
        exit(1);
    }
    execvp(argv[2], argv+2);
    perror("exec");
    exit(1);
}
