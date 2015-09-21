/*
 Copyright 2001-2004 Crispin Perdue <cris@perdues.com>

 This is free software, and comes with ABSOLUTELY NO WARRANTY.
 You may distribute it under the terms of the GNU Public License.
*/

#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <libiberty.h>
#include <errno.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

/*

  Usage: monitor [ -e ] [ -n service-name ] command [ arg ... ]

  This utility helps protect long-running services from
  undesired termination.  First, it starts and automatically
  restarts a service in case it exits.

  Second, it provides a straightforward way to kill the service
  when desired.  Send SIGTERM or SIGINT to the monitor process.
  The monitor will also forward SIGHUP to the child.

  Finally, it embeds naturally into SystemV service start/stop
  scripts, as used by systems such as Solaris and RedHat Linux.

  If killed with SIGTERM or SIGINT, the parent process does not exit
  until it has confirmation that the child has terminated.  Thus if
  another processes kills the mother process and waits for it to
  terminate, there is assurance that the underlying service has also
  terminated.

  This kills a service with SIGTERM.  If sent a second SIGTERM after
  commencing a kill of its child process, it issues another kill
  to the child, this time using SIGKILL.

  Automatic restarts include a 2-second sleep after termination
  of the service, to limit the rate of process spawning.  If the child
  dies within one minute of initial startup, without being sent a signal
  by the monitor, the monitor treats the startup as unsuccessful and
  does not restart the child.

  This logs its own and child process startups and terminations to
  syslog at the LOG_NOTICE level.  Other log messages are at the
  LOG_ERR level.

  Writes child status to /tmp/<service-name>.<mypid>.status, currently
  just a caution message during startup.

  The -n flag names the service.  Default is the basename of the command.

  The -e flag causes monitor to dup stderr same as stdout in the
  child, after opportunity to emit error messages.
*/

int verbose = 0;

int nargs;
char **args;

/* Base name of program to run */
char *progname;

/* Name of service as passed in, defaults to progname */
char *servicename;

/* If set true, dup stderr same as stdout in child process. */
/* This allows our error messages to go to the tty via stderr. */
int dup_stdout = 0;

/* PID of child, or 0 if no live child. */
int childpid = 0;

/* This process PID */
int mypid = 0;

/* Buffer to store the name of this monitor's status file. */
char statusfilename[PATH_MAX];

/* Saved value for log message. */
int savepid = 0;

/* Start time of this program */
time_t start_time;

/* Set to 1 in the interval from the first child start until
   one minute elapses or the monitor sends it a signal, whichever
   comes first. */
int starting = 0;

/* kill in progress */
int killing = 0;


/*
  Send SIGTERM or SIGKILL to the child to kill it, or SIGHUP
  to ask it to reload config files.
  If a kill is already in progress, use SIGKILL.
  If there is no child to kill, just exit normally.
 */
void killchild(int signo) {
  /* Mark the end of the startup period. */
  starting = 0;

  int signal = SIGTERM;
  if (signo==SIGHUP)
    signal = signo;
  
  /* Child died and not yet restarted. */
  if (childpid==0)
    exit(0);

  /* Already tried to kill this child; try harder. */
  if (killing)
    signal = SIGKILL;
  if (verbose)
    printf("killing child\n");
  if (kill(childpid, signal)) {
    if (errno!=ESRCH) {
      perror("kill");
      exit(1);
    }
  }
  if (signal!=SIGHUP)
    killing = 1;
}


void end_startup(int signo) {
  starting = 0;
  if (unlink(statusfilename) && errno!=ENOENT) {
    syslog(LOG_ERR, "Remove status file failed: %s", strerror(errno));
  }
}


/*
  Start a child process and set "pid" to its pid.
 */
void startchild() {
  /* struct sigaction childaction; */
  FILE* status;

  switch(childpid = fork()) {
    case -1:
      perror("fork failed");
      return;
    case 0:
      /*
      childaction.sa_handler = SIG_IGN;
      if (sigaction(SIGHUP, &childaction, NULL)) {
	perror("child signal handling");
	exit(1);
      }
      */

      if (dup_stdout) {
	/* Put stderr on fd3, equate stderr to stdout. */
	dup2(2, 3);
	dup2(1, 2);
      }
      execvp(args[0],args);
      if (dup_stdout) {
	/* Recover stderr */
	dup2(3, 2);
      }
      perror("exec failed");
      syslog(LOG_CRIT, "Exec failed: %s", strerror(errno));
      exit(1);
  }
  starting = 1;
  syslog(LOG_NOTICE, "%s[%d] %sstarted", progname, childpid,
	 savepid!=0 ? "re" : "");
  if (verbose)
    printf("started child %d\n", childpid);

  status = fopen(statusfilename, "w");
  if (status==NULL) {
    syslog(LOG_ERR, "Writing child status: %s", strerror(errno));
  }
  fprintf(status, "CAUTION: %s uptime < 1 minute.\n", servicename);
  if (fclose(status)) {
    syslog(LOG_ERR, "Writing child status: %s", strerror(errno));
  }

  /* Prepare to remove the "startup" status in 60 seconds. */
  struct sigaction alarm_action;
  alarm_action.sa_flags = 0;
  sigfillset(&alarm_action.sa_mask);
  sigdelset(&alarm_action.sa_mask, SIGTSTP);
  alarm_action.sa_handler = &end_startup;
  if (sigaction(SIGALRM, &alarm_action, NULL)) {
    perror("alarm handling");
  }
  if (alarm(60)) {
    perror("child startup - alarm");
  }
}


/*
  Respond to death of child: if a kill is in progress,
  exit normally, otherwise sleep for 2 seconds and restart
  the child.
 */
void handledeath(int status) {
  int code;
  int signal;
  const char *signame;

  /* TODO: Maybe provide the child a way to quit voluntarily. */

  if (WIFEXITED(status)) {
    code = WEXITSTATUS(status);
    syslog(LOG_ERR, "%s[%d] exited status %d",
           progname, savepid, code);
  } else if (WIFSIGNALED(status)) {
    signal = WTERMSIG(status);
    signame = strsigno(signal);
    if (killing) {
      syslog(LOG_NOTICE, "%s[%d] terminated by monitor %s",
	     progname, savepid, signame);
    } else {
      syslog(LOG_ERR, "%s[%d] terminated by %s",
             progname, savepid, signame);
    }
  } else {
    /* Who knows what to do, I guess this is impossible. */
    code = -1;
  }
  if (killing) {
    exit(0);
  }

  /* Check for infant death syndrome (less than one minute lifetime).
     Abandon only on very first start, for example do not abandon a
   */
  if (starting) {
    syslog(LOG_CRIT, "%s[%d] died in startup, abandoning",
	   progname, savepid);
    exit(1);
  }
  sleep(2);
  startchild();
}


/**
   Send a SIGHUP to the child.  By convention this asks it to reload
   its config files, but unprepared children will die.  In that case
   the child will get restarted.
*/
void interruptchild(int signo) {
  killchild(signo);
  killing = 0;
}


void logexit(int status, void* ptr) {
  end_startup(0);
  syslog(LOG_NOTICE, "exiting status %d", status);
}


int main(int argc, char **argv) {
  struct sigaction action;
  int code;
  int childstat;
  int waitpid;
  int c;
  int i;

  progname = argv[0];

  while (1) {
    /* Plus-sign to disable GNU-style permuting of cmd-line arguments. */
    c = getopt(argc, argv, "+en:");
    if (c == -1)
      break;
  
    switch (c) {
    case 'n':
      servicename = optarg;
      break;

    case 'e':
      dup_stdout=1;
      break;
  
    case '?':
      printf("Illegal option: %c\n", optopt);
      break;
    
    default:
      printf("Internal error, option -%c\n", c);
    }
  }

  nargs = argc - optind;
  args = argv + optind;
  if (argc<2) {
    fprintf
     (stderr, "Usage: monitor [ -e ] [ -n name ] program [ arg ... ]\n");
    exit(1);
  }

  progname = strrchr(args[0], '/');
  if (progname==NULL) {
    /* No slash found: use the whole string. */
    progname = args[0];
  } else {
    /* Move forward one char. */
    progname++;
  }

  if (servicename==NULL) {
    servicename = progname;
  }

  /* Define the name of the status file. */
  mypid = getpid();
  snprintf(statusfilename, PATH_MAX, "/tmp/%s.%d.status",
	   servicename, mypid);

  openlog(servicename, LOG_PID, LOG_DAEMON);
  on_exit(logexit, NULL);
  syslog(LOG_NOTICE, "starting");

  action.sa_flags = 0;
  sigfillset(&action.sa_mask);
  sigdelset(&action.sa_mask, SIGTSTP);
  /*
  sigdelset(&action.sa_mask, SIGALRM);
  */

  action.sa_handler = &killchild;
  if (sigaction(SIGTERM, &action, NULL)) {
    perror("signal handling");
    exit(1);
  }
  
  action.sa_handler = &killchild;
  if (sigaction(SIGINT, &action, NULL)) {
    perror("signal handling");
    exit(1);
  }
  
  action.sa_handler = &interruptchild;
  if (sigaction(SIGHUP, &action, NULL)) {
    perror("signal handling");
    exit(1);
  }

  /* Get the monitor's start time in seconds since the epoch */
  start_time = time(NULL);

  startchild();

  if (dup2(1, 2) == -1) {
    perror("dup");
    /* A bit late to exit. */
  }

  /* printf("PID: %d\n", pid); */
  while (1) {
    waitpid = wait(&childstat);
    if (waitpid==-1) {
      if (errno==EINTR) {
	continue;
      }
      perror("wait");
      exit(1);
    }
    savepid = childpid;
    childpid = 0;
    if (verbose)
      printf("child %d died\n", waitpid);
    handledeath(childstat);
  }

}
