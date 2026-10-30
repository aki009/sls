/*
 * sls - remote directory listing via SSH (scp-style)
 *
 * Copyright (C) 2026  Aki Korhonen <spammagnet@eibrak.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 * Portions of the interface design and remote-execution model
 * are inspired by OpenSSH's scp (BSD-licensed). Where linking
 * against LGPL-2.0 libraries occurs in derivative works, the
 * LGPL-2.0 terms apply to those portions.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/wait.h>

#define MAX_ARGS 64
#define BUF_SIZE 4096

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [ssh-options] [ls-options] [user@]host:path\n"
        "       %s [ssh-options] [ls-options] [user@]host\n"
        "\n"
        "List remote directory contents over SSH using the same\n"
        "user@host:path syntax as scp.\n"
        "\n"
        "SSH options (passed to ssh, same as scp):\n"
        "  -P port           Port to connect to\n"
        "  -i identity_file  Identity (private key) file\n"
        "  -o ssh_option     Extra ssh_config option (repeatable)\n"
        "  -F ssh_config     Alternate SSH config file\n"
        "  -J destination    ProxyJump\n"
        "  -4 / -6           Force IPv4 / IPv6\n"
        "  -C                Compression\n"
        "  -v                Verbose\n"
        "  -q                Quiet\n"
        "\n"
        "All other options are passed through to the remote ls(1).\n"
        "Connection settings are also read from ~/.ssh/config by ssh.\n"
        "\n"
        "Examples:\n"
        "  %s -l user@example.com:/var/log\n"
        "  %s -P 2222 -i ~/.ssh/id_ed25519 -la host:.\n"
        "  %s -o StrictHostKeyChecking=no -h user@host\n",
        prog, prog, prog, prog, prog);
    exit(EXIT_FAILURE);
}

/* Parse user@host:path  (path may be empty => ".") */
static int parse_target(const char *arg,
                        char *user, size_t userlen,
                        char *host, size_t hostlen,
                        char *path, size_t pathlen) {
    const char *at = strchr(arg, '@');
    const char *colon = strchr(arg, ':');
    const char *hoststart;
    size_t n;

    if (!colon) {
        /* no colon => treat whole string as host, path="." */
        colon = arg + strlen(arg);
    }

    if (at && at < colon) {
        n = (size_t)(at - arg);
        if (n >= userlen) return -1;
        memcpy(user, arg, n);
        user[n] = '\0';
        hoststart = at + 1;
    } else {
        user[0] = '\0';
        hoststart = arg;
    }

    n = (size_t)(colon - hoststart);
    if (n == 0 || n >= hostlen) return -1;
    memcpy(host, hoststart, n);
    host[n] = '\0';

    if (*colon == ':') {
        const char *p = colon + 1;
        if (*p == '\0')
            strncpy(path, ".", pathlen - 1);
        else
            strncpy(path, p, pathlen - 1);
        path[pathlen - 1] = '\0';
    } else {
        strncpy(path, ".", pathlen - 1);
        path[pathlen - 1] = '\0';
    }
    return 0;
}

/* Quote a string for safe inclusion in a remote shell command.
 * Minimal POSIX sh quoting. */
static char *shell_quote(const char *s) {
    size_t len = strlen(s);
    char *q = malloc(len * 2 + 3); /* worst case */
    char *p;
    if (!q) return NULL;
    p = q;
    *p++ = '\'';
    for (; *s; s++) {
        if (*s == '\'') {
            /* end quote, escaped quote, start quote */
            memcpy(p, "'\\''", 4);
            p += 4;
        } else {
            *p++ = *s;
        }
    }
    *p++ = '\'';
    *p = '\0';
    return q;
}

int main(int argc, char **argv) {
    char user[256] = {0};
    char host[256] = {0};
    char path[1024] = {0};
    char *ls_opts[MAX_ARGS];
    int nopts = 0;
    char *ssh_extra[MAX_ARGS];
    int nssh = 0;
    char *target = NULL;
    int i;

    if (argc < 2)
        usage(argv[0]);

    /* Parse arguments.
     * SSH options we recognize are collected for the ssh command.
     * Everything else starting with - goes to remote ls. */
    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];

        /* Options that take an argument */
        if (strcmp(arg, "-P") == 0 || strcmp(arg, "-i") == 0 ||
            strcmp(arg, "-o") == 0 || strcmp(arg, "-F") == 0 ||
            strcmp(arg, "-J") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s requires an argument\n", arg);
                return EXIT_FAILURE;
            }
            if (nssh >= MAX_ARGS - 2) {
                fprintf(stderr, "too many SSH options\n");
                return EXIT_FAILURE;
            }
            ssh_extra[nssh++] = (char *)arg;
            ssh_extra[nssh++] = argv[++i];
            continue;
        }

        /* Flag-only SSH options */
        if (strcmp(arg, "-4") == 0 || strcmp(arg, "-6") == 0 ||
            strcmp(arg, "-C") == 0 || strcmp(arg, "-v") == 0 ||
            strcmp(arg, "-q") == 0) {
            if (nssh >= MAX_ARGS - 1) {
                fprintf(stderr, "too many SSH options\n");
                return EXIT_FAILURE;
            }
            ssh_extra[nssh++] = (char *)arg;
            continue;
        }

        /* Everything else starting with - is an ls option */
        if (arg[0] == '-' && arg[1] != '\0') {
            if (nopts >= MAX_ARGS - 1) {
                fprintf(stderr, "too many ls options\n");
                return EXIT_FAILURE;
            }
            ls_opts[nopts++] = (char *)arg;
        } else if (!target) {
            target = (char *)arg;
        } else {
            fprintf(stderr, "unexpected argument: %s\n", arg);
            usage(argv[0]);
        }
    }
    ls_opts[nopts] = NULL;
    ssh_extra[nssh] = NULL;

    if (!target)
        usage(argv[0]);

    if (parse_target(target, user, sizeof user,
                     host, sizeof host,
                     path, sizeof path) != 0) {
        fprintf(stderr, "invalid target: %s\n", target);
        return EXIT_FAILURE;
    }

    /* Build remote command: ls [opts] path */
    char *quoted_path = shell_quote(path);
    if (!quoted_path) {
        perror("malloc");
        return EXIT_FAILURE;
    }

    size_t cmdlen = 16;
    for (i = 0; i < nopts; i++)
        cmdlen += strlen(ls_opts[i]) + 1;
    cmdlen += strlen(quoted_path) + 1;

    char *remote_cmd = malloc(cmdlen);
    if (!remote_cmd) {
        perror("malloc");
        free(quoted_path);
        return EXIT_FAILURE;
    }

    char *p = remote_cmd;
    p += sprintf(p, "ls");
    for (i = 0; i < nopts; i++)
        p += sprintf(p, " %s", ls_opts[i]);
    sprintf(p, " %s", quoted_path);
    free(quoted_path);

    /* Build ssh argv */
    char *ssh_argv[MAX_ARGS + 8];
    int n = 0;
    ssh_argv[n++] = "ssh";

    /* Add collected SSH options. Convert -P to -p for ssh. */
    for (i = 0; i < nssh; i++) {
        if (strcmp(ssh_extra[i], "-P") == 0) {
            ssh_argv[n++] = "-p";
            ssh_argv[n++] = ssh_extra[++i];
        } else {
            ssh_argv[n++] = ssh_extra[i];
        }
    }

    if (user[0]) {
        char *uh = malloc(strlen(user) + strlen(host) + 2);
        if (!uh) { perror("malloc"); free(remote_cmd); return EXIT_FAILURE; }
        sprintf(uh, "%s@%s", user, host);
        ssh_argv[n++] = uh;
    } else {
        ssh_argv[n++] = host;
    }
    ssh_argv[n++] = remote_cmd;
    ssh_argv[n] = NULL;

    /* Execute */
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        free(remote_cmd);
        return EXIT_FAILURE;
    }
    if (pid == 0) {
        execvp("ssh", ssh_argv);
        perror("execvp ssh");
        _exit(127);
    }

    int status;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        free(remote_cmd);
        return EXIT_FAILURE;
    }
    free(remote_cmd);

    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return EXIT_FAILURE;
}
