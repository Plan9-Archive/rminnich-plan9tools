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
#include <draw.h>
Tree *catmousetree;
int verbose;
int mf;
int x0, y0, x1, y1, cx, cy;
int screenfd = -1;
char *fields[6];


/* The middle 1/8 is no mans land */
Rectangle
map(int *pcx, int *pcy) {
	int cx = *pcx;
	int cy = *pcy;
	Rectangle in;
	in = screen->r;
	in.min.x += 3*Dx(in)/8;
	in.max.x -= 5*Dx(in)/8;
	in.min.y += 3*Dy(in)/8;
	in.max.y -= 5*Dy(in)/8;
	in = canonrect(in);
	/* ok, now what do cx and cy look like? */
	if (cx >in.min.x && cx < in.max.x)
		cx = in.min.x;
	if (cy > in.min.y && cy < in.max.y)
		cy = in.min.y;
	*pcx = cx;
	*pcy = cy;
	return in;
	
}

/* the real mouse kind of ignores ofset */
static void
fsread(Req *r)
{
	Rectangle in;
	static char m[72], m2[72];
	char *fields[6];
	int amt;
		amt = read(mf, m, sizeof(m));
		tokenize(m, fields, 5);
		cx = strtol(fields[1], 0, 0);
		cy = strtol(fields[2], 0, 0);
		/* now remap */
		in = map(&cx, &cy);
		memset(m2, 0, sizeof(m2));
		sprint(m2, "m%11d%12d%12s%12s ", cx, cy, fields[3], fields[4]);
	write(mf, m2, amt);

	memmove(r->ofcall.data, m2, amt);
	r->ofcall.count = amt;
	r->ofcall.offset = 0;

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

	ulong flag;
	char *mtpt;
	char err[ERRMAX];
	File *f;
	char *uname;

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

	uname = getenv("user");
	catmousetree = fs.tree = alloctree(uname, uname, DMDIR|0775, nil);
	f = createfile(catmousetree->root, "mouse", uname, 0664, nil);
	if(f == nil)
		sysfatal("creating %s: %r", "mouse");

	err[0] = '\0';
	errstr(err, sizeof err);

	if (initdraw(0, 0, "catmouse") < 0)
		sysfatal("initdraw failed");

	mf = open("/dev/mouse", ORDWR);
	
	postmountsrv(&fs, nil, mtpt, flag);
	exits(0);
}
/* 8c -wF catmouse.c; 8l -o 8.catmouse catmouse.8 */
