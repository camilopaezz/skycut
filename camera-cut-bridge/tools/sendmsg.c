#include "CameraCutBridge.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "user32.lib")

static void usage(void)
{
    fprintf(stderr,
            "Usage:\n"
            "  sendmsg.exe find\n"
            "  sendmsg.exe ping\n"
            "  sendmsg.exe go\n"
            "  sendmsg.exe path [wParam]\n"
            "  sendmsg.exe cfg [lParam]\n"
            "  sendmsg.exe raw <msg> <wParam> <lParam>\n");
}

static int parse_ll(const char *s, long long *out)
{
    char *end = NULL;
    long long v;

    if (!s || !s[0])
        return 0;
    errno = 0;
    v = strtoll(s, &end, 0);
    if (end == s || *end != '\0' || errno == ERANGE)
        return 0;
    *out = v;
    return 1;
}

static HWND find_cameracut(DWORD *pid_out)
{
    HWND hwnd;
    DWORD pid = 0;

    hwnd = FindWindowA("#32770", "CameraCut");
    if (!hwnd) {
        fprintf(stderr, "error: CameraCut window not found\n");
        return NULL;
    }
    GetWindowThreadProcessId(hwnd, &pid);
    printf("hwnd=%p pid=%lu\n", (void *)hwnd, (unsigned long)pid);
    if (pid_out)
        *pid_out = pid;
    return hwnd;
}

static int send_timeout(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DWORD_PTR dwResult = 0;

    if (!SendMessageTimeout(hwnd, msg, wParam, lParam, SMTO_ABORTIFHUNG, 5000,
                            &dwResult)) {
        DWORD err = GetLastError();
        printf("LRESULT=%lld\n", (long long)(LONG_PTR)dwResult);
        fprintf(stderr, "error: SendMessageTimeout failed gle=%lu\n",
                (unsigned long)err);
        return 1;
    }
    printf("LRESULT=%lld\n", (long long)(LONG_PTR)dwResult);
    return 0;
}

int main(int argc, char **argv)
{
    HWND hwnd;
    const char *cmd;
    UINT msg = 0;
    WPARAM wParam = 0;
    LPARAM lParam = 0;
    long long n_msg = 0, n_wp = 0, n_lp = 0;

    if (argc < 2) {
        usage();
        return 1;
    }
    cmd = argv[1];

    if (strcmp(cmd, "find") == 0 || strcmp(cmd, "ping") == 0 ||
        strcmp(cmd, "go") == 0) {
        if (argc != 2) {
            usage();
            return 1;
        }
    } else if (strcmp(cmd, "path") == 0) {
        n_wp = 4;
        if (argc > 3) {
            usage();
            return 1;
        }
        if (argc == 3 && !parse_ll(argv[2], &n_wp)) {
            fprintf(stderr, "error: bad wParam\n");
            return 1;
        }
        wParam = (WPARAM)n_wp;
    } else if (strcmp(cmd, "cfg") == 0) {
        if (argc > 3) {
            usage();
            return 1;
        }
        n_lp = 0;
        if (argc == 3 && !parse_ll(argv[2], &n_lp)) {
            fprintf(stderr, "error: bad lParam\n");
            return 1;
        }
        lParam = (LPARAM)n_lp;
    } else if (strcmp(cmd, "raw") == 0) {
        if (argc != 5) {
            usage();
            return 1;
        }
        if (!parse_ll(argv[2], &n_msg) || !parse_ll(argv[3], &n_wp) ||
            !parse_ll(argv[4], &n_lp)) {
            fprintf(stderr, "error: bad msg/wParam/lParam\n");
            return 1;
        }
        msg = (UINT)n_msg;
        wParam = (WPARAM)n_wp;
        lParam = (LPARAM)n_lp;
    } else {
        usage();
        return 1;
    }

    hwnd = find_cameracut(NULL);
    if (!hwnd)
        return 1;
    if (strcmp(cmd, "find") == 0)
        return 0;
    if (strcmp(cmd, "ping") == 0)
        return send_timeout(hwnd, WM_CC_PING, 0, 0);
    if (strcmp(cmd, "go") == 0)
        return send_timeout(hwnd, WM_CC_GO, 0, 0);
    if (strcmp(cmd, "path") == 0)
        return send_timeout(hwnd, WM_CC_PATH, wParam, 0);
    if (strcmp(cmd, "cfg") == 0)
        return send_timeout(hwnd, WM_CC_CFG, 0, lParam);
    return send_timeout(hwnd, msg, wParam, lParam);
}
