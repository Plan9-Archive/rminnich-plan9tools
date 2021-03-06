#include <u.h>
#include <libc.h>
#include <bio.h>
#include <thread.h>

Channel *out;
Channel *quit;
Channel *forkc;
int nread = 0;

typedef struct Str Str;
struct Str {
	char *buf;
	int len;
};

void
die(char *s)
{
	fprint(2, "%s\n", s);
	exits(s);
}
void
cwrite(int fd, char *path, char *cmd, int len)
{
	if (write(fd, cmd, len)  < len) {
		fprint(2, "cwrite: %s: failed %d bytes: %r\n", path, len);
		sendp(quit, nil);
		threadexits(nil);
	}
}
void
reader(void *v)
{
	char *ctl, *truss;
	int pid, newpid;
	int cfd, tfd;
	Str *s;
	int forking = 0;

	pid = (int)v;
	ctl = smprint("/proc/%d/ctl", pid);
	if ((cfd = open(ctl, OWRITE)) < 0)
		die(smprint("%s: %r", ctl));
	truss = smprint("/proc/%d/syscall", pid);
	if ((tfd = open(truss, OREAD)) < 0)
		die(smprint("%s: %r", truss));

	cwrite(cfd, ctl, "stop", 4);
	cwrite(cfd, truss, "startsyscall", 12);

	s = mallocz(sizeof(Str) + 8192, 1);
	s->buf = (char *)&s[1];
	/* 8191 is not a typo. It ensures a null-terminated string. The device currently limits to 4096 anyway */
	while((s->len = pread(tfd, s->buf, 8191, 0ULL)) > 0){
		if (forking && (s->buf[1] == '=') && (s->buf[3] != '-')) {
			forking = 0;
			newpid = strtol(&s->buf[3], 0, 0);
			sendp(forkc, (void*)newpid);
			procrfork(reader, (void*)newpid, 8192, 0);
		}

		/* There are three tests here and they (I hope) guarantee no false positives */
		if (strstr(s->buf, " Rfork") != nil) {
			char *a[8];
			char *rf;
			rf = strdup(s->buf);
         		if (tokenize(rf, a, 8) == 5) {
				unsigned long flags;
				flags = strtoul(a[4], 0, 16);
				if (flags & RFPROC)
					forking = 1;
			}
			free(rf);			
		}
		sendp(out, s);			
		cwrite(cfd, truss, "startsyscall", 12);
		s = mallocz(sizeof(Str) + 8192, 1);
		s->buf = (char *)&s[1];

	}
	sendp(quit, nil);
	threadexitsall(nil);
}


void
writer(void *)
{
	Alt a[4];
	Str *s;
	int newpid;

	a[0].op = CHANRCV;
	a[0].c = quit;
	a[0].v = nil;
	a[1].op = CHANRCV;
	a[1].c = out;
	a[1].v = &s;
	a[2].op = CHANRCV;
	a[2].c = forkc;
	a[2].v = &newpid;
	a[3].op = CHANEND;

	for(;;) { 
	switch(alt(a)){
	case 0:
		nread--;
		if(nread <= 0)
			goto done;
		break;
	case 1:
		/* it's a nice null terminated thing */
		fprint(2, "%s", s->buf);
		free(s);
		break;
	case 2:
//		procrfork(reader, (void*)newpid, 8192, 0);
		nread++;
		break;
	}
	}
done:
	exits(nil);
}

void
usage(void){
	fprint(2, "Usage: ratrace [-c cmd [args]] [pid] (one of these is required)\n");
	exits("usage");
}

void
threadmain(int argc, char **argv)
{
	int pid;
	char *cmd = nil;
	char **args = nil;

	/* don't bother with fancy arg processing, because it picks up switches for the 
	* argument you are starting. Just check for -c as argv[1] and then take it from there.
	*/
	if (argv[1][0] == '-') {
		switch(argv[1][1]) {
		case 'c':
			cmd = strdup(argv[2]);
			args = &argv[2];
			break;
		default:
			usage();
		}
	}

	/* run a command? */
	if(cmd) {
		pid = fork();
		if (pid < 0) {
			fprint(2, "No fork: %r\n");
			exits("fork failed");
		}
		if(pid == 0) {
			exec(cmd, args);
			fprint(2, "Bad exec: %s: %r\n", cmd);
			exits("Bad exec");
		}
	} else {
		if(argc != 1)
			usage();
		pid = atoi(argv[0]);
	}

	out = chancreate(sizeof(char*), 0);
	quit = chancreate(sizeof(char*), 0);
	forkc = chancreate(sizeof(ulong *), 0);
	nread++;
	procrfork(writer, nil, 8192, 0);
	reader((void*)pid);
}
