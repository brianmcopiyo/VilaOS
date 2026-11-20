#include "native.h"

extern "C" {
#include "builtins.h"
#include "exec.h"
#include "parser.h"
#include "util.h"
}

#include <stdio.h>
#include <string.h>

#include "webview.h"

#include <string>

static webview_t g_w = nullptr;
static const wchar_t *k_ui_host = L"app.local";

/* Absolute path to bin/ui next to the executable. */
static std::wstring ui_folder(void)
{
    wchar_t module[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, module, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return L"";
    }
    wchar_t *slash = wcsrchr(module, L'\\');
    if (slash != nullptr) {
        *(slash + 1) = L'\0';
    }
    std::wstring dir(module);
    dir += L"ui";
    return dir;
}

/*
 * Serve bin/ui as https://app.local/... so ES modules work.
 * file:// blocks type=module scripts in WebView2 (blank white page).
 */
static int mount_ui(void)
{
    void *raw = webview_get_native_handle(
        g_w, WEBVIEW_NATIVE_HANDLE_KIND_BROWSER_CONTROLLER);
    if (raw == nullptr) {
        return 0;
    }

    auto *controller = static_cast<ICoreWebView2Controller *>(raw);
    ICoreWebView2 *core = nullptr;
    if (FAILED(controller->get_CoreWebView2(&core)) || core == nullptr) {
        return 0;
    }

    ICoreWebView2_3 *core3 = nullptr;
    HRESULT hr = core->QueryInterface(IID_ICoreWebView2_3,
                                      reinterpret_cast<void **>(&core3));
    core->Release();
    if (FAILED(hr) || core3 == nullptr) {
        return 0;
    }

    std::wstring folder = ui_folder();
    if (folder.empty() || GetFileAttributesW(folder.c_str()) == INVALID_FILE_ATTRIBUTES) {
        core3->Release();
        return 0;
    }

    hr = core3->SetVirtualHostNameToFolderMapping(
        k_ui_host, folder.c_str(),
        COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
    core3->Release();
    return SUCCEEDED(hr) ? 1 : 0;
}

static void json_escape(const char *in, char *out, size_t outsz)
{
    size_t j = 0;
    if (in == nullptr || out == nullptr || outsz == 0) {
        return;
    }
    for (size_t i = 0; in[i] != '\0' && j + 2 < outsz; i++) {
        char c = in[i];
        if (c == '\\' || c == '"' || c == '\n' || c == '\r') {
            if (j + 3 >= outsz) {
                break;
            }
            out[j++] = '\\';
            if (c == '\n') {
                out[j++] = 'n';
                continue;
            }
            if (c == '\r') {
                out[j++] = 'r';
                continue;
            }
        }
        out[j++] = c;
    }
    out[j] = '\0';
}

/* Extract first JSON string argument from a JSON array request. */
static int parse_arg0(const char *req, char *out, size_t n)
{
    const char *p;
    size_t j = 0;
    int esc = 0;

    out[0] = '\0';
    if (req == nullptr || n == 0) {
        return 0;
    }
    p = strchr(req, '"');
    if (p == nullptr) {
        return 0;
    }
    p++;
    while (*p != '\0' && j + 1 < n) {
        if (esc) {
            out[j++] = *p;
            esc = 0;
            p++;
            continue;
        }
        if (*p == '\\') {
            esc = 1;
            p++;
            continue;
        }
        if (*p == '"') {
            break;
        }
        out[j++] = *p++;
    }
    out[j] = '\0';
    return j > 0;
}

static std::string ui_url(void)
{
    return "https://app.local/index.html";
}

static void on_pwd(const char *id, const char *req, void *arg)
{
    char cwd[PATH_MAX_LEN];
    char esc[PATH_MAX_LEN * 2];
    char json[PATH_MAX_LEN * 2 + 64];
    (void)req;
    (void)arg;

    if (get_cwd(cwd, sizeof(cwd)) != 0) {
        webview_return(g_w, id, 0, "{\"ok\":false,\"error\":\"unavailable\"}");
        return;
    }
    json_escape(cwd, esc, sizeof(esc));
    snprintf(json, sizeof(json), "{\"ok\":true,\"path\":\"%s\"}", esc);
    webview_return(g_w, id, 0, json);
}

static void on_cd(const char *id, const char *req, void *arg)
{
    char path[PATH_MAX_LEN];
    char *argv[3];
    char cwd[PATH_MAX_LEN];
    char esc[PATH_MAX_LEN * 2];
    char json[PATH_MAX_LEN * 2 + 64];
    (void)arg;

    parse_arg0(req, path, sizeof(path));
    argv[0] = (char *)"cd";
    if (path[0] == '\0') {
        argv[1] = nullptr;
        try_builtin(1, argv, nullptr);
    } else {
        argv[1] = path;
        argv[2] = nullptr;
        try_builtin(2, argv, nullptr);
    }

    if (get_cwd(cwd, sizeof(cwd)) != 0) {
        webview_return(g_w, id, 0, "{\"ok\":false,\"error\":\"cd failed\"}");
        return;
    }
    json_escape(cwd, esc, sizeof(esc));
    snprintf(json, sizeof(json), "{\"ok\":true,\"path\":\"%s\"}", esc);
    webview_return(g_w, id, 0, json);
}

static void on_run(const char *id, const char *req, void *arg)
{
    char cmd[LINE_MAX];
    char esc[512];
    char json[640];
    int argc = 0;
    char **argv = nullptr;
    int should_exit = 0;
    int code = 0;
    const char *status = "ok";
    (void)arg;

    parse_arg0(req, cmd, sizeof(cmd));
    if (cmd[0] == '\0') {
        webview_return(g_w, id, 0, "{\"ok\":false,\"code\":-1,\"status\":\"Empty command.\"}");
        return;
    }

    argv = parse_line(cmd, &argc);
    if (argv == nullptr || argc == 0) {
        webview_return(g_w, id, 0, "{\"ok\":false,\"code\":-1,\"status\":\"Parse error.\"}");
        return;
    }

    if (try_builtin(argc, argv, &should_exit)) {
        free_argv(argv);
        status = should_exit ? "Exit requested." : "Built-in finished.";
        code = 0;
    } else {
        code = run_external(argc, argv);
        free_argv(argv);
        status = code < 0 ? "Failed to start." : "Finished.";
    }

    json_escape(status, esc, sizeof(esc));
    snprintf(json, sizeof(json),
             "{\"ok\":%s,\"code\":%d,\"status\":\"%s\"}",
             code < 0 ? "false" : "true", code, esc);
    webview_return(g_w, id, 0, json);
}

static void on_quit(const char *id, const char *req, void *arg)
{
    (void)req;
    (void)arg;
    webview_return(g_w, id, 0, "{\"ok\":true}");
    webview_terminate(g_w);
}

extern "C" int native_run(void)
{
    std::string url;

    g_w = webview_create(0, nullptr);
    if (g_w == nullptr) {
        MessageBoxA(nullptr,
                    "Failed to create WebView2. Install the Evergreen WebView2 Runtime.",
                    "Terminal", MB_OK | MB_ICONERROR);
        return 1;
    }

    webview_set_title(g_w, "Terminal");
    webview_set_size(g_w, 960, 640, WEBVIEW_HINT_NONE);

    {
        void *hwnd = webview_get_native_handle(
            g_w, WEBVIEW_NATIVE_HANDLE_KIND_UI_WINDOW);
        if (hwnd != nullptr) {
            ShowWindow(static_cast<HWND>(hwnd), SW_SHOWMAXIMIZED);
        }
    }

    webview_bind(g_w, "nativePwd", on_pwd, nullptr);
    webview_bind(g_w, "nativeCd", on_cd, nullptr);
    webview_bind(g_w, "nativeRun", on_run, nullptr);
    webview_bind(g_w, "nativeQuit", on_quit, nullptr);

    if (!mount_ui()) {
        MessageBoxA(nullptr,
                    "Could not mount UI folder (bin\\ui next to shell.exe).",
                    "Terminal", MB_OK | MB_ICONERROR);
        webview_destroy(g_w);
        g_w = nullptr;
        return 1;
    }

    url = ui_url();
    webview_navigate(g_w, url.c_str());
    webview_run(g_w);
    webview_destroy(g_w);
    g_w = nullptr;
    return 0;
}
