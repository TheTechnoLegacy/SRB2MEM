// Android crash reporter: on a fatal signal, writes the signal and a native backtrace
// (module + offset, resolvable with addr2line) to <dir>/crash.txt.
#ifdef __ANDROID__
#include <signal.h>
#include <ucontext.h>
#include <unwind.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <dirent.h>
#include <pthread.h>
#include <time.h>
#include <sys/syscall.h>
#include <sys/prctl.h>

static char crashpath[600];
static char altstack_mem[64 * 1024];

typedef struct { int fd; int n; } btstate_t;

static _Unwind_Reason_Code unwind_cb(struct _Unwind_Context *ctx, void *arg)
{
	btstate_t *s = (btstate_t *)arg;
	uintptr_t pc = (uintptr_t)_Unwind_GetIP(ctx);
	char line[400];
	Dl_info info;
	if (pc)
	{
		if (dladdr((void *)pc, &info) && info.dli_fname)
			snprintf(line, sizeof line, "#%02d pc %p  %s +0x%lx  (%s)\n", s->n, (void *)pc,
				info.dli_fname, (unsigned long)(pc - (uintptr_t)info.dli_fbase),
				info.dli_sname ? info.dli_sname : "?");
		else
			snprintf(line, sizeof line, "#%02d pc %p\n", s->n, (void *)pc);
		if (write(s->fd, line, strlen(line)) < 0) return _URC_END_OF_STACK;
		s->n++;
	}
	return s->n >= 48 ? _URC_END_OF_STACK : _URC_NO_REASON;
}

static void crash_handler(int sig, siginfo_t *si, void *uc)
{
	char line[200];
	btstate_t st;
	st.fd = open(crashpath, O_WRONLY | O_CREAT | O_APPEND, 0644);
	st.n = 0;
	if (st.fd >= 0)
	{
		snprintf(line, sizeof line, "=== fatal signal %d (code %d), fault address %p ===\n",
			sig, si ? si->si_code : 0, si ? si->si_addr : NULL);
		if (write(st.fd, line, strlen(line)) < 0) {}
#if defined(__aarch64__)
		if (uc)
		{
			const ucontext_t *ctx = (const ucontext_t *)uc;
			int ri;
			snprintf(line, sizeof line, "pc=%llx sp=%llx\n", (unsigned long long)ctx->uc_mcontext.pc,
				(unsigned long long)ctx->uc_mcontext.sp);
			if (write(st.fd, line, strlen(line)) < 0) {}
			for (ri = 0; ri < 31; ri += 4)
			{
				int n = snprintf(line, sizeof line, "x%d=%llx x%d=%llx x%d=%llx x%d=%llx\n",
					ri, (unsigned long long)ctx->uc_mcontext.regs[ri],
					ri + 1, (unsigned long long)ctx->uc_mcontext.regs[ri + 1],
					ri + 2, (unsigned long long)ctx->uc_mcontext.regs[ri + 2],
					ri + 3 > 30 ? 30 : ri + 3, (unsigned long long)ctx->uc_mcontext.regs[ri + 3 > 30 ? 30 : ri + 3]);
				if (n > 0 && write(st.fd, line, (size_t)n) < 0) {}
			}
		}
#elif defined(__arm__)
		if (uc)
		{
			const ucontext_t *ctx = (const ucontext_t *)uc;
			snprintf(line, sizeof line, "pc=%lx lr=%lx sp=%lx r0=%lx r1=%lx r2=%lx r3=%lx\n",
				(unsigned long)ctx->uc_mcontext.arm_pc, (unsigned long)ctx->uc_mcontext.arm_lr,
				(unsigned long)ctx->uc_mcontext.arm_sp, (unsigned long)ctx->uc_mcontext.arm_r0,
				(unsigned long)ctx->uc_mcontext.arm_r1, (unsigned long)ctx->uc_mcontext.arm_r2,
				(unsigned long)ctx->uc_mcontext.arm_r3);
			if (write(st.fd, line, strlen(line)) < 0) {}
		}
#endif
		_Unwind_Backtrace(unwind_cb, &st);
		close(st.fd);
	}
	// SA_RESETHAND restored the default action; returning re-raises the fault
}


// ---------------------------------------------------------------------------
// Watchdog: if the game stops presenting frames for a few seconds (a freeze), write the
// backtrace of the game and audio threads to crash.txt so the hang can be located.
// ---------------------------------------------------------------------------
static volatile unsigned long heartbeat = 0;
static volatile int dump_done = 0;
static volatile int app_paused = 0;   // app is in the background: not a freeze

void Android_SetPaused(int paused)
{
	app_paused = paused;
}

void Android_Heartbeat(void)
{
	heartbeat++;
}

static void dump_handler(int sig)
{
	char line[160], name[32];
	btstate_t st;
	(void)sig;
	name[0] = '\0';
	prctl(PR_GET_NAME, (unsigned long)name, 0, 0, 0);
	st.fd = open(crashpath, O_WRONLY | O_CREAT | O_APPEND, 0644);
	st.n = 0;
	if (st.fd >= 0)
	{
		snprintf(line, sizeof line, "--- thread %d (%s) ---\n", (int)syscall(__NR_gettid), name);
		if (write(st.fd, line, strlen(line)) < 0) {}
		_Unwind_Backtrace(unwind_cb, &st);
		close(st.fd);
	}
	dump_done = 1;
}

static void dump_threads(void)
{
	DIR *d;
	struct dirent *de;
	const int self = (int)syscall(__NR_gettid);
	char path[96], comm[64];
	int fd;
	char line[120];

	fd = open(crashpath, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (fd >= 0)
	{
		snprintf(line, sizeof line, "=== watchdog: no video frame for 6+ seconds (uptime %ld s) ===\n", (long)time(NULL) % 100000);
		if (write(fd, line, strlen(line)) < 0) {}
		close(fd);
	}

	d = opendir("/proc/self/task");
	if (!d) return;
	while ((de = readdir(d)) != NULL)
	{
		const int tid = atoi(de->d_name);
		FILE *f;
		int waited;
		if (tid <= 0 || tid == self) continue;
		snprintf(path, sizeof path, "/proc/self/task/%d/comm", tid);
		comm[0] = '\0';
		f = fopen(path, "r");
		if (f) { if (!fgets(comm, sizeof comm, f)) comm[0] = '\0'; fclose(f); }
		// only the game thread and audio threads; never poke the Java/ART runtime threads
		if (strncmp(comm, "SDL", 3) != 0 && !strstr(comm, "udio")) continue;
		dump_done = 0;
		syscall(__NR_tgkill, getpid(), tid, SIGUSR2);
		for (waited = 0; waited < 50 && !dump_done; waited++)
			usleep(10000);
	}
	closedir(d);
}

static void *watchdog_thread(void *arg)
{
	unsigned long last = 0;
	int stale = 0, dumped = 0, dumps = 0;
	(void)arg;
	prctl(PR_SET_NAME, (unsigned long)"srb2-watchdog", 0, 0, 0);
	for (;;)
	{
		const unsigned long hb = heartbeat;
		sleep(1);
		if (hb == 0 || heartbeat == 0) continue; // not rendering yet
		if (app_paused)                          // in the background: frames stop on purpose
		{
			stale = 0;
			last = heartbeat;
			continue;
		}
		if (heartbeat != last)
		{
			last = heartbeat;
			stale = 0;
			dumped = 0;
			continue;
		}
		if (++stale >= 6 && !dumped && dumps < 2)   // at most 2 dumps per run keeps crash.txt small
		{
			dumped = 1;
			dumps++;
			dump_threads();
		}
	}
	return NULL;
}

void Android_InstallCrashHandler(const char *dir)
{
	struct sigaction sa;
	stack_t ss;
	const int sigs[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT, SIGTRAP };
	size_t i;

	snprintf(crashpath, sizeof crashpath, "%s/crash.txt", dir);
	{
		// start every run with a fresh crash.txt; keep only the previous run's as crash_prev.txt
		char prevpath[600];
		snprintf(prevpath, sizeof prevpath, "%s/crash_prev.txt", dir);
		rename(crashpath, prevpath);
	}

	ss.ss_sp = altstack_mem;
	ss.ss_size = sizeof altstack_mem;
	ss.ss_flags = 0;
	sigaltstack(&ss, NULL);

	memset(&sa, 0, sizeof sa);
	sa.sa_sigaction = crash_handler;
	sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
	sigemptyset(&sa.sa_mask);
	for (i = 0; i < sizeof sigs / sizeof sigs[0]; i++)
		sigaction(sigs[i], &sa, NULL);

	// watchdog for freezes
	{
		struct sigaction wa;
		pthread_t th;
		memset(&wa, 0, sizeof wa);
		wa.sa_handler = dump_handler;
		sigemptyset(&wa.sa_mask);
		sigaction(SIGUSR2, &wa, NULL);
		if (pthread_create(&th, NULL, watchdog_thread, NULL) == 0)
			pthread_detach(th);
	}
}
#endif
