/*
 * The install tool at the other end of the USB cable (card files, D-138): a host process given by --installer
 * "command" that reads every line the device writes to its console on stdin and answers each with exactly one line
 * on stdout. The answer reaches the device a line time plus its bytes at 1 MB/s later, in virtual time, so a run is
 * as deterministic as without it, however fast the host is. Answers starting with "#SILENT" or "#NONE" are not sent
 * (a lost line, nothing to say); "#UNPLUG" pulls the cable the way a script's "usb unplug" does. Once the process
 * exits or closes its output it never answers again. Only while a computer is plugged in (hal_usb_sim.c).
 */
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "sim.h"

extern char **environ;

enum { SLOTS = 4, REPLY_MAX = 4096 };
static struct {
    bool started, dead;
    int to;                         /* its stdin */
    FILE *from;                     /* its stdout */
    const char *line;
    size_t n;
    char reply[SLOTS][REPLY_MAX];   /* answers on their way to the device */
    unsigned next;
} I;

static void start(void) { /* host work */
    int in[2], out[2];
    I.started = true;
    if (pipe(in) || pipe(out)) {
        I.dead = true;
        return;
    }
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, in[0], 0);
    posix_spawn_file_actions_adddup2(&fa, out[1], 1);
    posix_spawn_file_actions_addclose(&fa, in[1]);
    posix_spawn_file_actions_addclose(&fa, out[0]);
    char *argv[] = {"/bin/sh", "-c", (char *)g_sim.installer, NULL};
    pid_t pid;
    if (posix_spawn(&pid, "/bin/sh", &fa, NULL, argv, environ)) I.dead = true;
    posix_spawn_file_actions_destroy(&fa);
    close(in[0]);
    close(out[1]);
    I.to = in[1];
    I.from = fdopen(out[0], "r");
    if (!I.from) I.dead = true;
    signal(SIGPIPE, SIG_IGN); /* a tool that went away: the write fails instead */
}

static void exchange(void *arg) { /* host work: one line out, one line back */
    char *reply = arg;
    reply[0] = 0;
    if (!I.started) start();
    for (size_t done = 0; !I.dead && done < I.n;) {
        ssize_t w = write(I.to, I.line + done, I.n - done);
        if (w <= 0) I.dead = true;
        else done += (size_t)w;
    }
    if (!I.dead && !fgets(reply, REPLY_MAX, I.from)) {
        I.dead = true;
        reply[0] = 0;
    }
}

static void deliver(void *arg) { /* ISR */
    const char *reply = arg;
    sim_usb_console_rx(reply, strlen(reply));
}

static void unplug(void *arg) { /* ISR, as the script does it */
    (void)arg;
    sim_usb_plug(false);
}

void sim_installer_line(const char *line, size_t n) { /* task context: the device wrote a line to its console */
    if (!g_sim.installer || I.dead) return;
    char *reply = I.reply[I.next++ % SLOTS];
    I.line = line;
    I.n = n;
    vtk_host_call(exchange, reply);
    if (!reply[0] || !strncmp(reply, "#SILENT", 7) || !strncmp(reply, "#NONE", 5)) return;
    uint64_t at = SIM_MS(2) + SIM_US(n + strlen(reply)); /* a line time, then both lines at 1 MB/s */
    vtk_lock();
    vtk_event_at(vtk_now() + at, strncmp(reply, "#UNPLUG", 7) ? deliver : unplug, reply);
    vtk_unlock();
}
