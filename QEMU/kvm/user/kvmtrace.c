/*
 * kvm tracing application
 *
 * This tool is used for collecting trace buffer data
 * for kvm trace.
 *
 * Based on blktrace 0.99.3
 *
 * Copyright (C) 2005 Jens Axboe <axboe@suse.de>
 * Copyright (C) 2006 Jens Axboe <axboe@kernel.dk>
 * Copyright (C) 2008 Eric Liu <eric.e.liu@intel.com>
 *
 * This work is licensed under the GNU LGPL license, version 2.
 */

#define _GNU_SOURCE

#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/statfs.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <getopt.h>
#include <errno.h>
#include <sched.h>

#ifndef __user
#define __user
#endif
#include <linux/kvm.h>

static char kvmtrace_version[] = "0.1";

/*
 * You may want to increase this even more, if you are logging at a high
 * rate and see skipped/missed events
 */
#define BUF_SIZE	(512 * 1024)
#define BUF_NR		(8)

#define OFILE_BUF	(128 * 1024)

#define DEBUGFS_TYPE	0x64626720

#define max(a, b)	((a) > (b) ? (a) : (b))

#define S_OPTS	"r:o:w:?Vb:n:D:"
static struct option l_opts[] = {
	{
		.name = "relay",
		.has_arg = required_argument,
		.flag = NULL,
		.val = 'r'
	},
	{
		.name = "output",
		.has_arg = required_argument,
		.flag = NULL,
		.val = 'o'
	},
	{
		.name = "stopwatch",
		.has_arg = required_argument,
		.flag = NULL,
		.val = 'w'
	},
	{
		.name = "version",
		.has_arg = no_argument,
		.flag = NULL,
		.val = 'V'
	},
	{
		.name = "buffer-size",
		.has_arg = required_argument,
		.flag = NULL,
		.val = 'b'
	},
	{
		.name = "num-sub-buffers",
		.has_arg = required_argument,
		.flag = NULL,
		.val = 'n'
	},
	{
		.name = "output-dir",
		.has_arg = required_argument,
		.flag = NULL,
		.val = 'D'
	},
	{
		.name = NULL,
	}
};

struct thread_information {
	int cpu;
	pthread_t thread;

	int fd;
	char fn[MAXPATHLEN + 64];

	FILE *ofile;
	char *ofile_buffer;

	int (*get_subbuf)(struct thread_information *, unsigned int);
	int (*read_data)(struct thread_information *, void *, unsigned int);

	unsigned long long data_read;

	struct kvm_trace_information *trace_info;

	int exited;

	/*
	 * mmap controlled output files
	 */
	unsigned long long fs_size;
	unsigned long long fs_max_size;
	unsigned long fs_off;
	void *fs_buf;
	unsigned long fs_buf_len;

};

struct kvm_trace_information {
	int fd;
	volatile int trace_started;
	unsigned long lost_records;
	struct thread_information *threads;
	unsigned long buf_size;
	unsigned long buf_nr;
};

static struct kvm_trace_information trace_information;

static int ncpus;
static char default_debugfs_path[] = "/sys/kernel/debug";

/* command line option globals */
static char *debugfs_path;
static char *output_name;
static char *output_dir;
static int stop_watch;
static unsigned long buf_size = BUF_SIZE;
static unsigned long buf_nr = BUF_NR;
static unsigned int page_size;

#define for_each_cpu_online(cpu) \
	for (cpu = 0; cpu < ncpus; cpu++)
#define for_each_tip(tip, i) \
	for (i = 0, tip = trace_information.threads; i < ncpus; i++, tip++)

#define is_done()	(*(volatile int *)(&done))
static volatile int done;

#define is_trace_stopped()	(*(volatile int *)(&trace_stopped))
static volatile int trace_stopped;

static void exit_trace(int status);

static void handle_sigint(__attribute__((__unused__)) int sig)
{
	ioctl(trace_information.fd, KVM_TRACE_PAUSE);
	done = 1;
}

static int get_lost_records()
{
	int fd;
	char tmp[MAXPATHLEN + 64];

	snprintf(tmp, sizeof(tmp), "%s/kvm/lost_records", debugfs_path);
	fd = open(tmp, O_RDONLY);
	if (fd < 0) {
		/*
		 * this may be ok, if the kernel doesn't support dropped counts
		 */
		if (errno == ENOENT)
			return 0;

		fprintf(stderr, "Couldn't open dropped file %s\n", tmp);
		return -1;
	}

	if (read(fd, tmp, sizeof(tmp)) < 0) {
		perror(tmp);
		close(fd);
		return -1;
	}
	close(fd);

	return atoi(tmp);
}

static void wait_for_data(struct thread_information *tip, int timeout)
{
	struct pollfd pfd = { .fd = tip->fd, .events = POLLIN };

	while (!is_done()) {
		if (poll(&pfd, 1, timeout) < 0) {
			perror("poll");
			break;
		}
		if (pfd.revents & POLLIN)
			break;
	}
}

static int read_data(struct thread_information *tip, void *buf,
			  unsigned int len)
{
	int ret = 0;

	do {
		wait_for_data(tip, 100);

		ret = read(tip->fd, buf, len);

		if (!ret)
			continue;
		else if (ret > 0)
			return ret;
		else {
			if (errno != EAGAIN) {
				perror(tip->fn);
				fprintf(stderr, "Thread %d failed read of %s\n",
					tip->cpu, tip->fn);
				break;
			}
			continue;
		}
	} while (!is_done());

	return ret;

}

/*
 * For file output, truncate and mmap the file appropriately
 */
static int mmap_subbuf(struct thread_information *tip, unsigned int maxlen)
{
	int ofd = fileno(tip->ofile);
	int ret;
	unsigned long nr;
	unsigned long size;

	/*
	 * extend file, if we have to. use chunks of 16 subbuffers.
	 */
	if (tip->fs_off + maxlen > tip->fs_buf_len) {
		if (tip->fs_buf) {
			munlock(tip->fs_buf, tip->fs_buf_len);
			munmap(tip->fs_buf, tip->fs_buf_len);
			tip->fs_buf = NULL;
		}

		tip->fs_off = tip->fs_size & (page_size - 1);
		nr = max(16, tip->trace_info->buf_nr);
		size = tip->trace_info->buf_size;
		tip->fs_buf_len = (nr * size) - tip->fs_off;
		tip->fs_max_size += tip->fs_buf_len;

		if (ftruncate(ofd, tip->fs_max_size) < 0) {
			perror("ftruncate");
			return -1;
		}

		tip->fs_buf = mmap(NULL, tip->fs_buf_len, PROT_WRITE,
				   MAP_SHARED, ofd, tip->fs_size - tip->fs_off);
		if (tip->fs_buf == MAP_FAILED) {
			perror("mmap");
			return -1;
		}
		mlock(tip->fs_buf, tip->fs_buf_len);
	}

	ret = tip->read_data(tip, tip->fs_buf + tip->fs_off, maxlen);
	if (ret >= 0) {
		tip->data_read += ret;
		tip->fs_size += ret;
		tip->fs_off += ret;
		return 0;
	}

	return -1;
}

static void tip_ftrunc_final(struct thread_information *tip)
{
	/*
	 * truncate to right size and cleanup mmap
	 */
	if (tip->ofile) {
		int ofd = fileno(tip->ofile);

		if (tip->fs_buf)
			munmap(tip->fs_buf, tip->fs_buf_len);

		ftruncate(ofd, tip->fs_size);
	}
}

static void *thread_main(void *arg)
{
	struct thread_information *tip = arg;
	pid_t pid = getpid();
	cpu_set_t cpu_mask;

	CPU_ZERO(&cpu_mask);
	CPU_SET((tip->cpu), &cpu_mask);

	if (sched_setaffinity(pid, sizeof(cpu_mask), &cpu_mask) == -1) {
		perror("sched_setaffinity");
		exit_trace(1);
	}

	snprintf(tip->fn, sizeof(tip->fn), "%s/kvm/trace%d",
			debugfs_path, tip->cpu);
	tip->fd = open(tip->fn, O_RDONLY);
	if (tip->fd < 0) {
		perror(tip->fn);
		fprintf(stderr, "Thread %d failed open of %s\n", tip->cpu,
			tip->fn);
		exit_trace(1);
	}
	while (!is_done()) {
		if (tip->get_subbuf(tip, tip->trace_info->buf_size) < 0)
			break;
	}

	/*
	 * trace is stopped, pull data until we get a short read
	 */
	while (tip->get_subbuf(tip, tip->trace_info->buf_size) > 0)
		;

	tip_ftrunc_final(tip);
	tip->exited = 1;
	return NULL;
}

static int fill_ofname(struct thread_information *tip, char *dst)
{
	struct stat sb;
	int len = 0;

	if (output_dir)
		len = sprintf(dst, "%s/", output_dir);
	else
		len = sprintf(dst, "./");

	if (stat(dst, &sb) < 0) {
		if (errno != ENOENT) {
			perror("stat");
			return 1;
		}
		if (mkdir(dst, 0755) < 0) {
			perror(dst);
			fprintf(stderr, "Can't make output dir\n");
			return 1;
		}
	}

	sprintf(dst + len, "%s.kvmtrace.%d", output_name, tip->cpu);

	return 0;
}

static void fill_ops(struct thread_information *tip)
{
	tip->get_subbuf = mmap_subbuf;
	tip->read_data = read_data;
}

static void close_thread(struct thread_information *tip)
{
	if (tip->fd != -1)
		close(tip->fd);
	if (tip->ofile)
		fclose(tip->ofile);
	if (tip->ofile_buffer)
		free(tip->ofile_buffer);

	tip->fd = -1;
	tip->ofile = NULL;
	tip->ofile_buffer = NULL;
}

static int tip_open_output(struct thread_information *tip)
{
	int mode, vbuf_size;
	char op[NAME_MAX];

	if (fill_ofname(tip, op))
		return 1;

	tip->ofile = fopen(op, "w+");
	mode = _IOFBF;
	vbuf_size = OFILE_BUF;

	if (tip->ofile == NULL) {
		perror(op);
		return 1;
	}

	tip->ofile_buffer = malloc(vbuf_size);
	if (setvbuf(tip->ofile, tip->ofile_buffer, mode, vbuf_size)) {
		perror("setvbuf");
		close_thread(tip);
		return 1;
	}

	fill_ops(tip);
	return 0;
}

static int start_threads(int cpu)
{
	struct thread_information *tip;

	tip = trace_information.threads + cpu;
	tip->cpu = cpu;
	tip->trace_info = &trace_information;
	tip->fd = -1;

	if (tip_open_output(tip))
	    return 1;

	if (pthread_create(&tip->thread, NULL, thread_main, tip)) {
		perror("pthread_create");
		close_thread(tip);
		return 1;
	}

	return 0;
}

static void stop_threads()
{
	struct thread_information *tip;
	unsigned long ret;
	int i;

	for_each_tip(tip, i) {
		if (tip->thread)
			(void) pthread_join(tip->thread, (void *) &ret);
		close_thread(tip);
	}
}

static int start_trace(void)
{
	int fd;
	struct kvm_user_trace_setup kuts;

	fd = trace_information.fd = open("/dev/kvm", O_RDWR);
	if (fd == -1) {
		perror("/dev/kvm");
		return 1;
	}

	memset(&kuts, 0, sizeof(kuts));
	kuts.buf_size = trace_information.buf_size = buf_size;
	kuts.buf_nr = trace_information.buf_nr = buf_nr;

	if (ioctl(trace_information.fd , KVM_TRACE_ENABLE, &kuts) < 0) {
		perror("KVM_TRACE_ENABLE");
		close(fd);
		return 1;
	}
	trace_information.trace_started = 1;

	return 0;
}

static void cleanup_trace(void)
{
	if (trace_information.fd == -1)
		return;

	trace_information.lost_records = get_lost_records();

	if (trace_information.trace_started) {
		trace_information.trace_started = 0;
		if (ioctl(trace_information.fd, KVM_TRACE_DISABLE) < 0)
			perror("KVM_TRACE_DISABLE");
	}

	close(trace_information.fd);
	trace_information.fd  = -1;
}

static void stop_all_traces(void)
{
	if (!is_trace_stopped()) {
		trace_stopped = 1;
		stop_threads();
		cleanup_trace();
	}
}

static void exit_trace(int status)
{
	stop_all_traces();
	exit(status);
}

static int start_kvm_trace(void)
{
	int i, size;
	struct thread_information *tip;

	size = ncpus * sizeof(struct thread_information);
	tip = malloc(size);
	if (!tip) {
		fprintf(stderr, "Out of memory, threads (%d)\n", size);
		return 1;
	}
	memset(tip, 0, size);
	trace_information.threads = tip;

	if (start_trace())
		return 1;

	for_each_cpu_online(i) {
		if (start_threads(i)) {
			fprintf(stderr, "Failed to start worker threads\n");
			break;
		}
	}

	if (i != ncpus) {
		stop_threads();
		cleanup_trace();
		return 1;
	}

	return 0;
}

static void wait_for_threads(void)
{
	struct thread_information *tip;
	int i, tips_running;

	do {
		tips_running = 0;
		usleep(100000);

		for_each_tip(tip, i)
			tips_running += !tip->exited;

	} while (tips_running);
}

static void show_stats(void)
{
	struct thread_information *tip;
	unsigned long long data_read;
	int i;

	data_read = 0;
	for_each_tip(tip, i) {
		printf("  CPU%3d: %8llu KiB data\n",
			tip->cpu, (tip->data_read + 1023) >> 10);
		data_read += tip->data_read;
	}

	printf("  Total:  lost %lu, %8llu KiB data\n",
		trace_information.lost_records, (data_read + 1023) >> 10);

	if (trace_information.lost_records)
		fprintf(stderr, "You have lost records, "
				"consider using a larger buffer size (-b)\n");
}

static char usage_str[] = \
	"[ -r debugfs path ] [ -D output dir ] [ -b buffer size ]\n" \
	"[ -n number of buffers] [ -o <output file> ] [ -w time  ] [ -V ]\n\n" \
	"\t-r Path to mounted debugfs, defaults to /sys/kernel/debug\n" \
	"\t-o File(s) to send output to\n" \
	"\t-D Directory to prepend to output file names\n" \
	"\t-w Stop after defined time, in seconds\n" \
	"\t-b Sub buffer size in KiB\n" \
	"\t-n Number of sub buffers\n" \
	"\t-V Print program version info\n\n";

static void show_usage(char *prog)
{
	fprintf(stderr, "Usage: %s %s %s", prog, kvmtrace_version, usage_str);
	exit(EXIT_FAILURE);
}

void parse_args(int argc, char **argv)
{
	int c;

	while ((c = getopt_long(argc, argv, S_OPTS, l_opts, NULL)) >= 0) {
		switch (c) {
		case 'r':
			debugfs_path = optarg;
			break;
		case 'o':
			output_name = optarg;
			break;
		case 'w':
			stop_watch = atoi(optarg);
			if (stop_watch <= 0) {
				fprintf(stderr,
					"Invalid stopwatch value (%d secs)\n",
					stop_watch);
				exit(EXIT_FAILURE);
			}
			break;
		case 'V':
			printf("%s version %s\n", argv[0], kvmtrace_version);
			exit(EXIT_SUCCESS);
		case 'b':
			buf_size = strtoul(optarg, NULL, 10);
			if (buf_size <= 0 || buf_size > 16*1024) {
				fprintf(stderr,
					"Invalid buffer size (%lu)\n",
					buf_size);
				exit(EXIT_FAILURE);
			}
			buf_size <<= 10;
			break;
		case 'n':
			buf_nr = strtoul(optarg, NULL, 10);
			if (buf_nr <= 0) {
				fprintf(stderr,
					"Invalid buffer nr (%lu)\n", buf_nr);
				exit(EXIT_FAILURE);
			}
			break;
		case 'D':
			output_dir = optarg;
			break;
		default:
			show_usage(argv[0]);
		}
	}

	if (optind < argc || output_name == NULL)
		show_usage(argv[0]);
}

int main(int argc, char *argv[])
{
	struct statfs st;

	parse_args(argc, argv);

	if (!debugfs_path)
		debugfs_path = default_debugfs_path;

	if (statfs(debugfs_path, &st) < 0) {
		perror("statfs");
		fprintf(stderr, "%s does not appear to be a valid path\n",
			debugfs_path);
		return 1;
	} else if (st.f_type != (long) DEBUGFS_TYPE) {
		fprintf(stderr, "%s does not appear to be a debug filesystem,"
			" please mount debugfs.\n",
			debugfs_path);
		return 1;
	}

	page_size = getpagesize();

	ncpus = sysconf(_SC_NPROCESSORS_ONLN);
	if (ncpus < 0) {
		fprintf(stderr, "sysconf(_SC_NPROCESSORS_ONLN) failed\n");
		return 1;
	}

	signal(SIGINT, handle_sigint);
	signal(SIGHUP, handle_sigint);
	signal(SIGTERM, handle_sigint);
	signal(SIGALRM, handle_sigint);
	signal(SIGPIPE, SIG_IGN);

	if (start_kvm_trace() != 0)
		return 1;

	if (stop_watch)
		alarm(stop_watch);

	wait_for_threads();
	stop_all_traces();
	show_stats();

	return 0;
}
