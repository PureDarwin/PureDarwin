// pdwifi-nm.c - pdwifi.h for Argon on Linux, over NetworkManager's nmcli
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// NetworkManager owns the radio and the networks, with no NetworkManager or Wi-Fi every call reports
// PDWIFI_NO_HARDWARE

#define _GNU_SOURCE
#include "pdwifi.h"
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;
static char interface[32];
static int looked;

// runs nmcli with argv, its standard output into out, the exit status returned
static int nmcli(char *out, size_t size, const char *const argv[])
{
    int pipefd[2], status = -1;
    posix_spawn_file_actions_t actions;
    pid_t pid;
    size_t used = 0;

    if (out && size)
        out[0] = '\0';
    if (pipe(pipefd) != 0)
        return -1;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDOUT_FILENO);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", 1, 0);
    posix_spawn_file_actions_addclose(&actions, pipefd[0]);
    if (posix_spawnp(&pid, "nmcli", &actions, NULL, (char *const *)argv, environ) != 0) {
        posix_spawn_file_actions_destroy(&actions);
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }
    posix_spawn_file_actions_destroy(&actions);
    close(pipefd[1]);
    for (;;) {
        char scratch[512];
        ssize_t n = read(pipefd[0], out && used + 1 < size ? out + used : scratch,
                         out && used + 1 < size ? size - used - 1 : sizeof(scratch));
        if (n <= 0)
            break;
        if (out && used + 1 < size)
            used += (size_t)n;
    }
    if (out && size)
        out[used] = '\0';
    close(pipefd[0]);
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// splits nmcli's terse output field, where a colon inside a value is written \:
static char *field(char **line)
{
    char *start = *line, *w = start;

    if (start == NULL)
        return NULL;
    for (char *r = start; *r; r++) {
        if (r[0] == '\\' && r[1]) {
            *w++ = *++r;
            continue;
        }
        if (*r == ':') {
            *w = '\0';
            *line = r + 1;
            return start;
        }
        *w++ = *r;
    }
    *w = '\0';
    *line = NULL;
    return start;
}

const char *pdwifi_interface(void)
{
    char out[2048], *line, *save = NULL;

    if (looked)
        return interface[0] ? interface : NULL;
    looked = 1;
    if (nmcli(out, sizeof(out), (const char *const[]){ "nmcli", "-t", "-f", "DEVICE,TYPE", "device", NULL }) != 0)
        return NULL;
    for (line = strtok_r(out, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        char *rest = line, *dev = field(&rest), *type = field(&rest);
        if (type && strcmp(type, "wifi") == 0) {
            snprintf(interface, sizeof(interface), "%s", dev);
            return interface;
        }
    }
    return NULL;
}

void pdwifi_set_interface(const char *name)
{
    snprintf(interface, sizeof(interface), "%s", name ? name : "");
    looked = 1;
}

int pdwifi_power(void)
{
    char out[64];

    if (pdwifi_interface() == NULL || nmcli(out, sizeof(out), (const char *const[]){ "nmcli", "radio", "wifi", NULL }) != 0)
        return 0;
    return strncmp(out, "enabled", 7) == 0;
}

pdwifi_result pdwifi_set_power(int on)
{
    if (pdwifi_interface() == NULL)
        return PDWIFI_NO_HARDWARE;
    return nmcli(NULL, 0, (const char *const[]){ "nmcli", "radio", "wifi", on ? "on" : "off", NULL }) == 0 ? PDWIFI_OK : PDWIFI_FAILED;
}

// nmcli's signal is a percentage, pdwifi works in dBm
static int dbm(int percent)
{
    return percent / 2 - 100;
}

static int list(pdwifi_network *out, int max, int active_only, int rescan)
{
    static char buffer[32768];
    char *line, *save = NULL;
    int n = 0;

    if (pdwifi_interface() == NULL)
        return 0;
    if (nmcli(buffer, sizeof(buffer), (const char *const[]){ "nmcli", "-t", "-f", "ACTIVE,SSID,BSSID,SIGNAL,CHAN,SECURITY",
              "device", "wifi", "list", "--rescan", rescan ? "yes" : "no", NULL }) != 0)
        return 0;
    for (line = strtok_r(buffer, "\n", &save); line && n < max; line = strtok_r(NULL, "\n", &save)) {
        char *rest = line, *active = field(&rest), *ssid = field(&rest), *bssid = field(&rest);
        char *signal = field(&rest), *chan = field(&rest), *security = field(&rest);
        int duplicate = 0;

        if (ssid == NULL || *ssid == '\0' || (active_only && strcmp(active ? active : "", "yes") != 0))
            continue;
        // nmcli lists each access point, keep the strongest per network name
        for (int i = 0; i < n; i++)
            duplicate |= strcmp(out[i].ssid, ssid) == 0;
        if (duplicate)
            continue;
        memset(&out[n], 0, sizeof(out[n]));
        snprintf(out[n].ssid, sizeof(out[n].ssid), "%s", ssid);
        if (bssid)
            sscanf(bssid, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &out[n].bssid[0], &out[n].bssid[1], &out[n].bssid[2],
                   &out[n].bssid[3], &out[n].bssid[4], &out[n].bssid[5]);
        out[n].rssi = dbm(signal ? atoi(signal) : 0);
        out[n].channel = chan ? (unsigned)atoi(chan) : 0;
        out[n].secure = security && *security && strcmp(security, "--") != 0;
        out[n].security = out[n].secure ? (strstr(security, "WPA3") ? "WPA3 Personal" : "WPA2 Personal") : "None";
        n++;
    }
    return n;
}

int pdwifi_scan(pdwifi_network *out, int max)
{
    return list(out, max, 0, 1);
}

int pdwifi_current(pdwifi_network *out)
{
    return list(out, 1, 1, 0) == 1;
}

int pdwifi_state(void)
{
    pdwifi_network n;

    return pdwifi_current(&n);
}

int pdwifi_hwaddr(uint8_t mac[6])
{
    char path[96], text[32];
    FILE *f;
    int ok;

    if (pdwifi_interface() == NULL)
        return 0;
    snprintf(path, sizeof(path), "/sys/class/net/%s/address", interface);
    if ((f = fopen(path, "r")) == NULL)
        return 0;
    ok = fgets(text, sizeof(text), f) && sscanf(text, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) == 6;
    fclose(f);
    return ok;
}

pdwifi_result pdwifi_join(const char *ssid, const char *password, int remember)
{
    int status;

    (void)remember;
    if (pdwifi_interface() == NULL)
        return PDWIFI_NO_HARDWARE;
    if (!pdwifi_power())
        return PDWIFI_OFF;
    if (password && *password)
        status = nmcli(NULL, 0, (const char *const[]){ "nmcli", "device", "wifi", "connect", ssid, "password", password, NULL });
    else
        status = nmcli(NULL, 0, (const char *const[]){ "nmcli", "device", "wifi", "connect", ssid, NULL });
    // nmcli's exit statuses: 4 activation failed, 10 no such network
    return status == 0 ? PDWIFI_OK : status == 10 ? PDWIFI_NOT_FOUND : status == 4 && password ? PDWIFI_BAD_PASSWORD : PDWIFI_FAILED;
}

pdwifi_result pdwifi_join_known(const char *ssid)
{
    if (pdwifi_interface() == NULL)
        return PDWIFI_NO_HARDWARE;
    return nmcli(NULL, 0, (const char *const[]){ "nmcli", "connection", "up", "id", ssid, NULL }) == 0 ? PDWIFI_OK : PDWIFI_FAILED;
}

pdwifi_result pdwifi_leave(void)
{
    if (pdwifi_interface() == NULL)
        return PDWIFI_NO_HARDWARE;
    return nmcli(NULL, 0, (const char *const[]){ "nmcli", "device", "disconnect", interface, NULL }) == 0 ? PDWIFI_OK : PDWIFI_FAILED;
}

// NetworkManager reconnects to remembered networks itself
pdwifi_result pdwifi_autojoin(void)
{
    return pdwifi_interface() ? PDWIFI_OK : PDWIFI_NO_HARDWARE;
}

int pdwifi_known(char names[][33], int max)
{
    char out[8192], *line, *save = NULL;
    int n = 0;

    if (nmcli(out, sizeof(out), (const char *const[]){ "nmcli", "-t", "-f", "NAME,TYPE", "connection", "show", NULL }) != 0)
        return 0;
    for (line = strtok_r(out, "\n", &save); line && n < max; line = strtok_r(NULL, "\n", &save)) {
        char *rest = line, *name = field(&rest), *type = field(&rest);
        if (type && strcmp(type, "802-11-wireless") == 0)
            snprintf(names[n++], 33, "%s", name);
    }
    return n;
}

int pdwifi_is_known(const char *ssid)
{
    char names[64][33];
    int n = pdwifi_known(names, 64);

    for (int i = 0; i < n; i++) {
        if (strcmp(names[i], ssid) == 0)
            return 1;
    }
    return 0;
}

void pdwifi_forget(const char *ssid)
{
    nmcli(NULL, 0, (const char *const[]){ "nmcli", "connection", "delete", "id", ssid, NULL });
}

int pdwifi_bars(int rssi)
{
    if (rssi >= -60)
        return 3;
    if (rssi >= -72)
        return 2;
    return 1;
}

const char *pdwifi_message(pdwifi_result r)
{
    switch (r) {
    case PDWIFI_OK: return "Connected.";
    case PDWIFI_NO_HARDWARE: return "No Wi-Fi hardware was found.";
    case PDWIFI_OFF: return "Wi-Fi is turned off.";
    case PDWIFI_BAD_PASSWORD: return "Couldn't join. Check the password and try again.";
    case PDWIFI_TIMEOUT: return "The network didn't accept the connection.";
    case PDWIFI_NOT_FOUND: return "The network wasn't found. Move closer and try again.";
    default: return "The network couldn't be joined.";
    }
}
