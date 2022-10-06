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
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "config.h"

#define LEN(X)         (sizeof(X) / sizeof(*X))
#define unlikely(X)    __builtin_expect((bool)(X), 0)
#define likely(X)      __builtin_expect((bool)(X), 1)

#define BUFFER_SIZE    1024u


struct traf {
	FILE   *file;
	size_t  bytes;
};

struct interface {
	struct traf rx;
	struct traf tx;
	char        name[255];
};

struct netsp {
	int              fmt_pad;
	unsigned         infs_count;
	struct interface infs[INTERFACES_MAX];
};


static int interface_open(struct interface *inf, const char *name,
			  size_t name_len);
static int netsp_interfaces_load(struct netsp *net, const char *pfx[],
				 unsigned pfx_len);
static void netsp_show(struct netsp *net);
static void netsp_cleanup(struct netsp *net);
static int netsp_run(const char *pfx[], unsigned pfx_len);
static size_t traf_read(struct traf *traf);
static const char *bytes_fmt(char *buffer, size_t b_size, size_t bytes);


static int
interface_open(struct interface *inf, const char *name, size_t name_len)
{
	int ret;
	char path[1024];
	const char *err_ctx;

	/* RX */
	err_ctx = name;
	if (snprintf(path, sizeof(path), "%s%s%s", NET_DIR, name, RX_BYTES) < 0)
		goto err0;

	err_ctx = path;
	if ((inf->rx.file = fopen(path, "r")) == NULL)
		goto err0;

	/* TX */
	err_ctx = name;
	if (snprintf(path, sizeof(path), "%s%s%s", NET_DIR, name, TX_BYTES) < 0)
		goto err0;

	err_ctx = path;
	if ((inf->tx.file = fopen(path, "r")) == NULL)
		goto err0;

	memcpy(inf->name, name, name_len);
	inf->name[name_len] = '\0';
	return 0;

err0:
	ret = -errno;
	if (inf->rx.file != NULL)
		fclose(inf->rx.file);

	fprintf(stderr, "interface_open: %s: %s\n", err_ctx, strerror(-ret));
	return ret;
}


static int
netsp_interfaces_load(struct netsp *net, const char *pfx[], unsigned pfx_len) 
{
	int ret = 0;
	int fmt_pad = 0;
	struct interface *infs = net->infs;
	struct dirent *dirent;
	unsigned count = 0;
	DIR *dir;

	if ((dir = opendir(NET_DIR)) == NULL) {
		ret = -errno;
		goto err0;
	}

	for (unsigned i = 0; i < INTERFACES_MAX; i++) {
readdir_again:
		errno = 0;
		if ((dirent = readdir(dir)) == NULL) {
			if (errno != 0)
				ret = -errno;
			break;
		}

		const char *d_name = dirent->d_name;
		if (d_name[0] == '.')
			goto readdir_again;

		for (unsigned j = 0; j < pfx_len; j++) {
			if (strncmp(d_name, pfx[j], strlen(pfx[j])) != 0)
				continue;

			const size_t d_name_len = strlen(d_name);
			if (d_name_len > (size_t)fmt_pad)
				fmt_pad = d_name_len;

			if (interface_open(&infs[count], d_name, d_name_len) < 0)
				continue;

			count++;
		}
	}

	net->infs_count = count;
	net->fmt_pad    = fmt_pad;
	closedir(dir);

	if (ret < 0)
		goto err0;

	return 0;

err0:
	netsp_cleanup(net);
	fprintf(stderr, "netsp_interfaces_load: %s: %s\n", NET_DIR,
		strerror(-ret));
	return ret;
}


static void
netsp_show(struct netsp *net)
{
	char buffer[BUFFER_SIZE];
	const unsigned count = net->infs_count;
	struct interface *infs = net->infs;
	const int pad = net->fmt_pad;
	const char *fmt;

	if (count == 0)
		return;

show_again:
	printf("\x1b[2J\x1b[H");
	for (unsigned i = 0; likely(i < count); i++) {
		struct interface *inf = &infs[i];

		fmt  = bytes_fmt(buffer, BUFFER_SIZE,
				 inf->tx.bytes + inf->rx.bytes);
		printf("%-*s [%*s] ", pad, inf->name, FMT_PAD, fmt);

		fmt  = bytes_fmt(buffer, BUFFER_SIZE, traf_read(&inf->tx));
		printf(FMT_UP_STR": %*s ", FMT_PAD, fmt);

		fmt  = bytes_fmt(buffer, BUFFER_SIZE, traf_read(&inf->rx));
		printf(FMT_DW_STR": %*s\n", FMT_PAD, fmt);
	}
	usleep(DELAY);

	goto show_again;
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
netsp_run(const char *pfx[], unsigned pfx_len)
{
	int ret = 0;
	struct netsp net;

	memset(&net, 0, sizeof(net));
	if ((ret = netsp_interfaces_load(&net, pfx, pfx_len)) < 0)
		return ret;

	netsp_show(&net);

	/* Maybe not reached */
	netsp_cleanup(&net);

	return ret;
}


static size_t
traf_read(struct traf *traf)
{
	struct traf *tf = traf;
	const size_t old_traf = tf->bytes;

	while (likely(fscanf(tf->file, "%zu", &tf->bytes) != EOF));
	rewind(tf->file);

	return tf->bytes - old_traf;
}


/* slstatus: util.c: fmt_human() */
static const char *
bytes_fmt(char *buffer, size_t b_size, size_t bytes)
{
	const char prefix[] = { 'b', 'k', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y' };
	size_t scaled = bytes;
	size_t i;

	for (i = 0; likely((i < sizeof(prefix)) && (scaled >= 1000)); i++)
		scaled >>= 10;

	if (unlikely(snprintf(buffer, b_size, "%zu%c", scaled, prefix[i]) < 0))
		return "-";

	return buffer;
}


static void
netsp_help(const char *app_name)
{
	printf("netsp - A simple bandwidth monitor\n\n"
		"Usage: %s [NET_PREFIX_1] [NET_PREFIX_2] ...\n"
		"Example:\n"
		" %s w e\n"
		" %s wlan eth\n",
		app_name, app_name, app_name);
}


int
main(int argc, const char *argv[])
{
	if (argc < 2) {
		netsp_help(argv[0]);
		return EINVAL;
	}

	return -netsp_run(argv + 1, (unsigned)argc - 1);
}
