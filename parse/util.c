#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "parse.h"

/* malloc wrappers */
void *zalloc(size_t sz)
{
	void *mem;

	mem = calloc(1, sz);
	if (!mem && sz)
		die("Out of memory");
	return mem;
}

void *xalloc(size_t sz)
{
	void *mem;

	mem = malloc(sz);
	if (!mem && sz)
		die("Out of memory");
	return mem;
}

void *zrealloc(void *mem, size_t oldsz, size_t sz)
{
	char *p;

	p = xrealloc(mem, sz);
	if (sz > oldsz)
		memset(&p[oldsz], 0, sz - oldsz);
	return p;
}

void *xrealloc(void *mem, size_t sz)
{
	mem = realloc(mem, sz);
	if (!mem && sz)
		die("Out of memory");
	return mem;
}

/* errors */
void die(char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	abort();
}

void fatal(Node *n, char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	lfatalv(n->loc, msg, ap);
	va_end(ap);
}

void lfatal(Srcloc l, char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	lfatalv(l, msg, ap);
	va_end(ap);
}

void lfatalv(Srcloc l, char *msg, va_list ap)
{
	fprintf(stdout, "%s:%d: ", fname(l), lnum(l));
	vfprintf(stdout, msg, ap);
	fprintf(stdout, "\n");
	exit(1);
}

/* Some systems don't have strndup. */
char *strdupn(char *s, size_t len)
{
	char *ret;

	ret = xalloc(len + 1);
	memcpy(ret, s, len);
	ret[len] = '\0';
	return ret;
}

char *strjoin(char *u, char *v)
{
	size_t n;
	char *s;

	n = strlen(u) + strlen(v) + 1;
	s = xalloc(n);
	bprintf(s, n + 1, "%s%s", u, v);
	return s;
}

void *memdup(void *mem, size_t len)
{
	void *ret;

        if (!mem)
            return NULL;
	ret = xalloc(len);
	return memcpy(ret, mem, len);
}

/* lists */
void lappend(void *l, size_t *len, void *n)
{
	void ***pl;

	pl = l;
	*pl = xrealloc(*pl, (*len + 1) * sizeof(void *));
	(*pl)[*len] = n;
	(*len)++;
}

void *lpop(void *l, size_t *len)
{
	void ***pl;
	void *v;

	pl = l;
	(*len)--;
	v = (*pl)[*len];
	*pl = xrealloc(*pl, *len * sizeof(void *));
	return v;
}

void linsert(void *p, size_t *len, size_t idx, void *v)
{
	void ***pl, **l;

	pl = p;
	*pl = xrealloc(*pl, (*len + 1) * sizeof(void *));
	l = *pl;

	memmove(&l[idx + 1], &l[idx], (*len - idx) * sizeof(void *));
	l[idx] = v;
	(*len)++;
}

void ldel(void *p, size_t *len, size_t idx)
{
	void ***pl, **l;

	assert(p != NULL);
	assert(idx < *len);
	pl = p;
	l = *pl;
	memmove(&l[idx], &l[idx + 1], (*len - idx - 1) * sizeof(void *));
	(*len)--;
	*pl = xrealloc(l, *len * sizeof(void *));
}

void lcat(void *dst, size_t *ndst, void *src, size_t nsrc)
{
	size_t i;
	void ***d, **s;

	d = dst;
	s = src;
	for (i = 0; i < nsrc; i++)
		lappend(d, ndst, s[i]);
}

void lfree(void *l, size_t *len)
{
	void ***pl;

	assert(l != NULL);
	pl = l;
	free(*pl);
	*pl = NULL;
	*len = 0;
}

/* endian packing */
void be64(vlong v, byte buf[8])
{
	buf[0] = (v >> 56) & 0xff;
	buf[1] = (v >> 48) & 0xff;
	buf[2] = (v >> 40) & 0xff;
	buf[3] = (v >> 32) & 0xff;
	buf[4] = (v >> 24) & 0xff;
	buf[5] = (v >> 16) & 0xff;
	buf[6] = (v >> 8) & 0xff;
	buf[7] = (v >> 0) & 0xff;
}

vlong host64(byte buf[8])
{
	vlong v = 0;

	v |= ((vlong)buf[0] << 56LL);
	v |= ((vlong)buf[1] << 48LL);
	v |= ((vlong)buf[2] << 40LL);
	v |= ((vlong)buf[3] << 32LL);
	v |= ((vlong)buf[4] << 24LL);
	v |= ((vlong)buf[5] << 16LL);
	v |= ((vlong)buf[6] << 8LL);
	v |= ((vlong)buf[7] << 0LL);
	return v;
}

void be32(long v, byte buf[4])
{
	buf[0] = (v >> 24) & 0xff;
	buf[1] = (v >> 16) & 0xff;
	buf[2] = (v >> 8) & 0xff;
	buf[3] = (v >> 0) & 0xff;
}

long host32(byte buf[4])
{
	int32_t v = 0;
	v |= ((long)buf[0] << 24);
	v |= ((long)buf[1] << 16);
	v |= ((long)buf[2] << 8);
	v |= ((long)buf[3] << 0);
	return v;
}

void wrbuf(FILE *fd, void *p, size_t sz)
{
	size_t n;
	char *buf;

	n = 0;
	buf = p;
	while (n < sz) {
		n += fwrite(buf + n, 1, sz - n, fd);
		if (feof(fd))
			die("Unexpected EOF");
		if (ferror(fd))
			die("Error writing");
	}
}

void rdbuf(FILE *fd, void *buf, size_t sz)
{
	size_t n;

	n = sz;
	while (n > 0) {
		n -= fread(buf, 1, n, fd);
		if (feof(fd))
			die("Unexpected EOF");
		if (ferror(fd))
			die("Error writing");
	}
}

void wrbyte(FILE *fd, char val)
{
	if (fputc(val, fd) == EOF)
		die("Unexpected EOF");
}

char rdbyte(FILE *fd)
{
	int c;
	c = fgetc(fd);
	if (c == EOF)
		die("Unexpected EOF");
	return c;
}

void wrint(FILE *fd, long val)
{
	byte buf[4];
	be32(val, buf);
	wrbuf(fd, buf, 4);
}

long rdint(FILE *fd)
{
	byte buf[4];
	rdbuf(fd, buf, 4);
	return host32(buf);
}

void wrstr(FILE *fd, char *val)
{
	size_t len;

	if (!val) {
		wrint(fd, -1);
	} else {
		wrint(fd, strlen(val));
		len = strlen(val);
		wrbuf(fd, val, len);
	}
}

char *rdstr(FILE *fd)
{
	ssize_t len;
	char *s;

	len = rdint(fd);
	if (len == -1) {
		return NULL;
	} else {
		s = xalloc(len + 1);
		rdbuf(fd, s, len);
		s[len] = '\0';
		return s;
	}
}

void wrlenstr(FILE *fd, Str str)
{
	wrint(fd, str.len);
	wrbuf(fd, str.buf, str.len);
}

void rdlenstr(FILE *fd, Str *str)
{
	str->len = rdint(fd);
	str->buf = xalloc(str->len + 1);
	rdbuf(fd, str->buf, str->len);
	str->buf[str->len] = '\0';
}

void wrflt(FILE *fd, double val)
{
	byte buf[8];
	/* Assumption: We have 'val' in 64 bit IEEE format */
	union {
		uvlong ival;
		double fval;
	} u;

	u.fval = val;
	be64(u.ival, buf);
	wrbuf(fd, buf, 8);
}

double rdflt(FILE *fd)
{
	byte buf[8];
	union {
		uvlong ival;
		double fval;
	} u;

	if (fread(buf, 8, 1, fd) < 8)
		die("Unexpected EOF");
	u.ival = host64(buf);
	return u.fval;
}

size_t bprintf(char *buf, size_t sz, char *fmt, ...)
{
	va_list ap;
	size_t n;

	va_start(ap, fmt);
	n = vsnprintf(buf, sz, fmt, ap);
	if (n >= sz)
		n = sz;
	va_end(ap);

	return n;
}

void wrbool(FILE *fd, int val) { wrbyte(fd, val); }

int rdbool(FILE *fd) { return rdbyte(fd); }

char *swapsuffix(char *buf, size_t sz, char *s, char *suf, char *swap)
{
	size_t slen, suflen, swaplen;

	slen = strlen(s);
	suflen = strlen(suf);
	swaplen = strlen(swap);

	if (slen < suflen)
		return NULL;
	if (slen + swaplen >= sz)
		die("swapsuffix: buf too small");

	buf[0] = '\0';
	/* if we have matching suffixes */
	if (suflen < slen && !strcmp(suf, &s[slen - suflen])) {
		strncat(buf, s, slen - suflen);
		strncat(buf, swap, swaplen);
	} else {
		bprintf(buf, sz, "%s%s", s, swap);
	}

	return buf;
}

size_t max(size_t a, size_t b)
{
	if (a > b)
		return a;
	else
		return b;
}

size_t min(size_t a, size_t b)
{
	if (a < b)
		return a;
	else
		return b;
}

size_t align(size_t sz, size_t a)
{
	/* align to 0 just returns sz */
	if (a == 0)
		return sz;
	return (sz + a - 1) & ~(a - 1);
}

void indentf(int depth, char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfindentf(stdout, depth, fmt, ap);
	va_end(ap);
}

void findentf(FILE *fd, int depth, char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfindentf(fd, depth, fmt, ap);
	va_end(ap);
}

void vfindentf(FILE *fd, int depth, char *fmt, va_list ap)
{
	ssize_t i;

	for (i = 0; i < depth; i++)
		fprintf(fd, "\t");
	vfprintf(fd, fmt, ap);
}

static int optinfo(Optctx *ctx, char arg, int *take, int *mand)
{
	char *s;

	for (s = ctx->optstr; *s != '\0'; s++) {
		if (*s == arg) {
			s++;
			if (*s == ':') {
				*take = 1;
				*mand = 1;
				return 1;
			} else if (*s == '?') {
				*take = 1;
				*mand = 0;
				return 1;
			} else {
				*take = 0;
				*mand = 0;
				return 1;
			}
		}
	}
	return 0;
}

static int findnextopt(Optctx *ctx)
{
	size_t i;

	for (i = ctx->argidx + 1; i < ctx->noptargs; i++) {
		if (ctx->optargs[i][0] == '-')
			goto foundopt;
		else
			lappend(&ctx->args, &ctx->nargs, ctx->optargs[i]);
	}
	ctx->finished = 1;
	return 0;
foundopt:
	ctx->argidx = i;
	ctx->curarg = ctx->optargs[i] + 1; /* skip initial '-' */
	return 1;
}

void optinit(Optctx *ctx, char *optstr, char **optargs, size_t noptargs)
{
	ctx->args = NULL;
	ctx->nargs = 0;

	ctx->optstr = optstr;
	ctx->optargs = optargs;
	ctx->noptargs = noptargs;
	ctx->optdone = 0;
	ctx->finished = 0;
	ctx->argidx = 0;
	ctx->curarg = "";
	findnextopt(ctx);
}

int optnext(Optctx *ctx)
{
	int take, mand;
	int c;

	c = *ctx->curarg;
	ctx->curarg++;
	if (!optinfo(ctx, c, &take, &mand)) {
		printf("Unexpected argument %c\n", *ctx->curarg);
		exit(1);
	}

	ctx->optarg = NULL;
	if (take) {
		if (*ctx->curarg) {
			ctx->optarg = ctx->curarg;
			ctx->curarg += strlen(ctx->optarg);
		} else if (ctx->argidx < ctx->noptargs - 1) {
			ctx->optarg = ctx->optargs[ctx->argidx + 1];
			ctx->argidx++;
		} else if (mand) {
			fprintf(stderr, "expected argument for %c\n", *ctx->curarg);
		}
		findnextopt(ctx);
	} else {
		if (*ctx->curarg == '\0')
			findnextopt(ctx);
	}
	return c;
}

int optdone(Optctx *ctx) { return *ctx->curarg == '\0' && ctx->finished; }
