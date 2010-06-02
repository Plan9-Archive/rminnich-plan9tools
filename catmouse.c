/*
 * catmouse play games with a mouse
 */

#include <u.h>
#include <libc.h>
#include <bio.h>
#include <auth.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

Tree *catmousetree;
int verbose;
int mf;
int x0, y0, x1, y1, cx, cy;

int map(int x0, int x1, int delta, int where) {
	int half = (x1 - x0) / 2;
	int b = (where > half? delta : -delta);
	double f = (1.0/x1-x0);
	int mod = (where - x0) * f*b;
	int new = where + mod;
	return new;
}

static void
fsread(Req *r)
{
	static char m[72], m2[72];
	char *fields[6];
print("off %lld\n", r->ifcall.offset);
		read(mf, m, sizeof(m));
		tokenize(m, fields, 5);
		cx = strtol(fields[1], 0, 0);
		cy = strtol(fields[2], 0, 0);
		/* now remap */
		cx = map(x0, x1, 20, cx);
		sprint(m2, "m%11d%12d%12s%12s ", cx, cy, fields[3], fields[4]);
write(1, m2, sizeof(m2));
	write(mf, m2, sizeof(m2));

	readstr(r, m2);
/*
	r->ofcall.count = r->ifcall.count;
	strcpy(r->ofcall.data,  m);*/
	respond(r, nil);	
}

static void
fswrite(Req *r)
{
	int len;
	len = write(mf, r->ifcall.data, r->ifcall.count);
	r->ofcall.count = len;
	respond(r, nil);	
}

Srv fs = {
	.read=	fsread,
	.write=	fswrite,
};

static void
usage(void)
{
	fprint(2, "usage: catmouse\n");
	exits("usage");
}

void
main(int argc, char **argv)
{

	char screen[128];
	char *fields[6];
	int fd;
	ulong flag;
	char *mtpt;
	char err[ERRMAX];
	File *f;

	mtpt = "/dev";
	flag = MBEFORE;
	ARGBEGIN{
	case 'D':
		chatty9p++;
		break;
	case 'm':
		mtpt = EARGF(usage());
		break;
	default:
		usage();
		break;
	}ARGEND;

	if(argc)
		usage();

	catmousetree = fs.tree = alloctree("glenda", "glenda", DMDIR|0775, nil);
	f = createfile(catmousetree->root, "mouse", "glenda", 0664, nil);
	if(f == nil)
		sysfatal("creating %s: %r", "mouse");

	err[0] = '\0';
	errstr(err, sizeof err);

	fd = open("/dev/screen", OREAD);
	if (fd < 0)
		exits("no /dev/screen");
	if (read(fd, screen, sizeof screen) < 0)
		exits("no screen data");

	tokenize(screen, fields, 6);

	x0 = strtoul(fields[1], 0, 0);
	y0 = strtoul(fields[2], 0, 0);
	x1 = strtoul(fields[3], 0, 0);
	y1 = strtoul(fields[4], 0, 0);

	print("%d %d %d %d\n", x0, y0, x1, y1);

	mf = open("/dev/mouse", ORDWR);
	
	postmountsrv(&fs, nil, mtpt, flag);
	exits(0);
}
/* 8c -wF catmouse.c; 8l -o 8.catmouse catmouse.8 */
