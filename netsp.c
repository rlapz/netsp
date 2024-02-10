/* netsp - A simple bandwidth monitor
 *
 * Copyright (c) 2022 Arthur Lapz (rLapz) <rlapz@gnuweeb.org>
 *
 * See LICENSE file for license details
 */

#define _XOPEN_SOURCE   600
#define _POSIX_C_SOURCE 200112L

#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "config.h"

#define LEN(X)         (sizeof(X) / sizeof(*X))
#define unlikely(X)    __builtin_expect(!!(X), 0)
#define likely(X)      __builtin_expect(!!(X), 1)
#define __hot          __attribute__((__hot__))

#define FMT_SIZE       (256 + 64)
#define PATH_SIZE      (sizeof(NET_DIR) + sizeof(RX_BYTES) + 255)


struct traf {
	FILE   *file;
	size_t  bytes;
};

struct interface {
	struct traf rx;
	struct traf tx;
	char        name[256];
};

struct netsp {
	int              fmt_pad;
	unsigned         infs_count;
	struct interface infs[INTERFACES_MAX];
};


static int interface_open(struct interface *inf, const char *name,
			  size_t name_len);
static int netsp_interfaces_load(struct netsp *net, const char *pfx[],
				 int pfx_len);
static int netsp_show(struct netsp *net);
static void netsp_cleanup(struct netsp *net);
static int netsp_run(const char *pfx[], int pfx_len);
static size_t traf_read(struct traf *traf);
static const char *bytes_fmt(char *buf, size_t buf_size, size_t bytes);
static void netsp_show_interfaces(void);


static int
interface_open(struct interface *inf, const char *name, size_t name_len)
{
	int ret;
	char path[PATH_SIZE];
	const char *err_ctx;

	/* RX */
	err_ctx = name;
	if (snprintf(path, PATH_SIZE, "%s%s%s", NET_DIR, name, RX_BYTES) < 0)
		goto err;

	err_ctx = path;
	if ((inf->rx.file = fopen(path, "r")) == NULL)
		goto err;

	/* TX */
	err_ctx = name;
	if (snprintf(path, PATH_SIZE, "%s%s%s", NET_DIR, name, TX_BYTES) < 0)
		goto err;

	err_ctx = path;
	if ((inf->tx.file = fopen(path, "r")) == NULL)
		goto err;

	memcpy(inf->name, name, name_len);
	inf->name[name_len] = '\0';
	return 0;

err:
	ret = -errno;
	if (inf->rx.file != NULL)
		fclose(inf->rx.file);

	fprintf(stderr, "interface_open: %s: %s\n", err_ctx, strerror(-ret));
	return ret;
}


static int
netsp_interfaces_load(struct netsp *net, const char *pfx[], int pfx_len)
{
	int ret = 0;
	int fmt_pad = 0;
	struct interface *infs = net->infs;
	struct dirent *dirent;
	unsigned count = 0;
	DIR *dir;

	if ((dir = opendir(NET_DIR)) == NULL) {
		ret = -errno;
		goto err;
	}

	while (count < INTERFACES_MAX) {
		errno = 0;
		if ((dirent = readdir(dir)) == NULL) {
			if (errno != 0)
				ret = -errno;
			break;
		}

		const char *const fname = dirent->d_name;
		if (fname[0] == '.')
			continue;

		const size_t flen = strlen(fname);

		/* try to load all interfaces */
		if (pfx == NULL || pfx_len == 0) {
			if (flen > (size_t)fmt_pad)
				fmt_pad = flen;

			if (interface_open(&infs[count], fname, flen) == 0)
				count++;

			continue;
		}

		for (int j = 0; j < pfx_len; j++) {
			if (strncmp(fname, pfx[j], strlen(pfx[j])) != 0)
				continue;

			if (flen > (size_t)fmt_pad)
				fmt_pad = flen;

			if (interface_open(&infs[count], fname, flen) == 0)
				count++;
		}
	}

	net->infs_count = count;
	net->fmt_pad    = fmt_pad;
	closedir(dir);

	if (ret < 0)
		goto err;

	return 0;

err:
	netsp_cleanup(net);
	fprintf(stderr, "netsp_interfaces_load: %s: %s\n", NET_DIR,
		strerror(-ret));
	return ret;
}


static int
netsp_show(struct netsp *net)
{
	char fmt[FMT_SIZE];
	char buffer[IO_BUF_SIZE];
	const unsigned count = net->infs_count;
	struct interface *infs = net->infs;
	const int pad = net->fmt_pad;
	const char *pf;

	if (count == 0) {
		fprintf(stderr, "netsp_show: No interface\n");
		return -EINVAL;
	}

	if (setvbuf(stdout, buffer, _IOFBF, IO_BUF_SIZE) != 0) {
		const int ret = -errno;
		fprintf(stderr, "netsp_show: setvbuf: %s\n", strerror(-ret));
		return ret;
	}

	// save cursor position
	printf("\x1b[s");

show_again:
	// restore cursor position
	printf("\x1b[u");

	for (unsigned i = 0; likely(i < count); i++) {
		struct interface *const inf = &infs[i];

		pf = bytes_fmt(fmt, FMT_SIZE, inf->tx.bytes + inf->rx.bytes);
		printf("%-*s [%*s] ", pad, inf->name, FMT_PAD, pf);

		pf = bytes_fmt(fmt, FMT_SIZE, traf_read(&inf->tx));
		printf(FMT_UP_STR ": %*s", FMT_PAD, pf);

		printf(" - ");

		pf = bytes_fmt(fmt, FMT_SIZE, traf_read(&inf->rx));
		printf(FMT_DW_STR ": %*s\n", FMT_PAD, pf);
	}

	fflush(stdout);
	usleep(DELAY);
	goto show_again;

	return 0;
}


static void
netsp_show_interfaces(void)
{
	DIR *const dir = opendir(NET_DIR);
	if (dir == NULL) {
		perror("netsp_show_interfaces: opendir");
		return;
	}

	while (1) {
		errno = 0;
		struct dirent *const dirent = readdir(dir);
		if (dirent == NULL) {
			if (errno != 0)
				perror("netsp_show_interfaces: readdir");

			break;
		}

		const char *const fname = dirent->d_name;
		if (fname[0] == '.')
			continue;

		puts(fname);
	}

	closedir(dir);
}


static void
netsp_cleanup(struct netsp *net)
{
	unsigned count = net->infs_count;
	while (count--) {
		fclose(net->infs[count].rx.file);
		fclose(net->infs[count].tx.file);
	}
}


static int
netsp_run(const char *pfx[], int pfx_len)
{
	int ret = 0;
	struct netsp net;

	memset(&net, 0, sizeof(net));
	if ((ret = netsp_interfaces_load(&net, pfx, pfx_len)) < 0)
		return ret;

	ret = netsp_show(&net);

	/* Maybe not reached */
	netsp_cleanup(&net);

	return ret;
}


static __hot size_t
traf_read(struct traf *traf)
{
	struct traf *const tf = traf;
	const size_t old_traf = tf->bytes;

	while (likely(fscanf(tf->file, "%zu", &tf->bytes) != EOF));
	rewind(tf->file);

	return tf->bytes - old_traf;
}


/* slstatus: util.c: fmt_human() */
static __hot const char *
bytes_fmt(char *buf, size_t buf_size, size_t bytes)
{
	const char prefix[] = { 'b', 'k', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y' };
	double scaled = bytes;
	size_t i;

	for (i = 0; likely((i < sizeof(prefix)) && (scaled >= FMT_BASE)); i++)
		scaled /= FMT_BASE;

	const int sp = snprintf(buf, buf_size, "%." FMT_PREC "f%c", scaled, prefix[i]);
	if (unlikely(sp < 0))
		return "-";

	return buf;
}


static void
netsp_help(const char *app_name)
{
	printf("netsp - A simple bandwidth monitor\n\n"
		"Usage: \n"
		" Load interface by its prefix name\n"
		" %s [NET_PREFIX_1] [NET_PREFIX_2] ...\n\n"
		" Load all interfaces\n"
		" %s --all\n\n"
		" Show interfaces\n"
		" %s --show\n\n"
		"Example:\n"
		" %s --all\n"
		" %s w e\n"
		" %s wlan eth\n",
		app_name, app_name, app_name, app_name, app_name, app_name);
}


int
main(int argc, const char *argv[])
{
	if (argc < 2)
		goto err;

	if (strcmp(argv[1], "--show") == 0) {
		netsp_show_interfaces();
		return 0;
	}

	if (strcmp(argv[1], "--all") == 0) {
		if (argc != 2)
			goto err;

		return -netsp_run(NULL, 0);
	}

	if (argv[1][0] == '-')
		goto err;

	return -netsp_run(argv + 1, argc - 1);

err:
	netsp_help(argv[0]);
	return EINVAL;
}
