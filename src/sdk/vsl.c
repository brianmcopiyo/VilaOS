#include "vsl.h"
#include "bridge.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define VSL_LINE_MAX 512
#define VSL_MAX_ARGS 12
#define VSL_BLOCK_DEPTH 64
#define EXT_NONE 2

static const char *const k_int_ops[] = {"add", "sub", "mul", "div", "mod", NULL};
static const char *const k_flt_ops[] = {"fadd", "fsub", "fmul", "fdiv", NULL};
static const char *const k_cmp_ops[] = {"eq", "ne", "lt", "le", "gt", "ge", NULL};

static int run_range(VslRuntime *rt, int start, int end);

void vsl_reset(VslRuntime *rt)
{
    if (!rt)
        return;
    memset(rt, 0, sizeof(*rt));
}

static char *trim(char *s)
{
    while (*s && isspace((unsigned char)*s))
        s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1]))
        *--e = 0;
    return s;
}

static void unquote(char *s)
{
    size_t n = strlen(s);
    if (n >= 2 && ((s[0] == '"' && s[n - 1] == '"') || (s[0] == '\'' && s[n - 1] == '\''))) {
        s[n - 1] = 0;
        memmove(s, s + 1, n - 1);
    }
}

static int idx_of(const char *op, const char *const *names)
{
    for (int i = 0; names[i]; i++) {
        if (str_eq(op, names[i]))
            return i;
    }
    return -1;
}

static int word_is(const char *t, const char *w)
{
    size_t n = strlen(w);
    return strncmp(t, w, n) == 0 && (t[n] == 0 || isspace((unsigned char)t[n]));
}

static int tokenize(char *s, char **argv, int max)
{
    int n = 0;
    while (*s && n < max) {
        while (*s && isspace((unsigned char)*s))
            s++;
        if (!*s)
            break;
        argv[n++] = s;
        char q = 0;
        while (*s) {
            if (q) {
                if (*s == q)
                    q = 0;
            } else if (*s == '"' || *s == '\'') {
                q = *s;
            } else if (isspace((unsigned char)*s)) {
                break;
            }
            s++;
        }
        if (*s)
            *s++ = 0;
    }
    return n;
}

static VslVar *var_get(VslRuntime *rt, const char *name, int create)
{
    for (int i = 0; i < rt->var_count; i++) {
        if (str_eq(rt->vars[i].name, name))
            return &rt->vars[i];
    }
    if (!create || rt->var_count >= VSL_MAX_VARS)
        return NULL;
    VslVar *v = &rt->vars[rt->var_count++];
    memset(v, 0, sizeof(*v));
    str_copy(v->name, sizeof(v->name), name);
    return v;
}

static void set_var(VslRuntime *rt, const char *name, const char *val)
{
    if (!name)
        return;
    if (name[0] == '$')
        name++;
    if (!name[0] || str_eq(name, "_"))
        return;
    VslVar *v = var_get(rt, name, 1);
    if (v)
        str_copy(v->value, sizeof(v->value), val ? val : "");
}

static void resolve_token(VslRuntime *rt, const char *tok, char *out, size_t n)
{
    out[0] = 0;
    if (!tok || !tok[0])
        return;
    if (tok[0] == '$') {
        VslVar *v = var_get(rt, tok + 1, 0);
        if (v)
            str_copy(out, n, v->value);
        return;
    }
    if (str_starts(tok, "text:")) {
        bridge_get_text(rt, tok + 5, out, n);
        return;
    }
    str_copy(out, n, tok);
    unquote(out);
}

static void rarg(VslRuntime *rt, char **argv, int argc, int i, char *out, size_t n)
{
    if (i < argc)
        resolve_token(rt, argv[i], out, n);
    else
        out[0] = 0;
}

static int is_num(const char *s)
{
    while (*s && isspace((unsigned char)*s))
        s++;
    if (!(isdigit((unsigned char)*s) || *s == '-' || *s == '+' || *s == '.'))
        return 0;
    char *e;
    strtod(s, &e);
    if (e == s)
        return 0;
    while (*e && isspace((unsigned char)*e))
        e++;
    return *e == 0;
}

static long to_long(const char *s)
{
    return s && s[0] ? strtol(s, NULL, 10) : 0;
}

static double to_dbl(const char *s)
{
    return s && s[0] ? strtod(s, NULL) : 0.0;
}

static int truthy(const char *s)
{
    return s && s[0] && !str_eq(s, "0") && !str_eq(s, "false");
}

static int cmp_values(const char *a, const char *b)
{
    if (is_num(a) && is_num(b)) {
        double x = strtod(a, NULL);
        double y = strtod(b, NULL);
        return x < y ? -1 : (x > y ? 1 : 0);
    }
    int c = strcmp(a, b);
    return c < 0 ? -1 : (c > 0 ? 1 : 0);
}

static int cmp_apply(int k, int c)
{
    switch (k) {
    case 0:
        return c == 0;
    case 1:
        return c != 0;
    case 2:
        return c < 0;
    case 3:
        return c <= 0;
    case 4:
        return c > 0;
    default:
        return c >= 0;
    }
}

static int eval_cond(VslRuntime *rt, char **argv, int argc)
{
    char a[MAX_TEXT];
    char b[MAX_TEXT];
    if (argc < 2)
        return 0;
    int k = idx_of(argv[1], k_cmp_ops);
    if (k >= 0) {
        rarg(rt, argv, argc, 2, a, sizeof(a));
        rarg(rt, argv, argc, 3, b, sizeof(b));
        return cmp_apply(k, cmp_values(a, b));
    }
    if (str_eq(argv[1], "truthy")) {
        rarg(rt, argv, argc, 2, a, sizeof(a));
        return truthy(a);
    }
    if (str_eq(argv[1], "falsy")) {
        rarg(rt, argv, argc, 2, a, sizeof(a));
        return !truthy(a);
    }
    rarg(rt, argv, argc, 1, a, sizeof(a));
    return truthy(a);
}

static int list_ref(VslRuntime *rt, const char *tok)
{
    if (!tok)
        return -1;
    const char *name = tok[0] == '$' ? tok + 1 : tok;
    VslVar *v = var_get(rt, name, 0);
    const char *val = v ? v->value : name;
    if (!str_starts(val, "#list:"))
        return -1;
    int i = atoi(val + 6);
    if (i < 0 || i >= VSL_MAX_LISTS || !rt->lists[i].used)
        return -1;
    return i;
}

static int exec_list(VslRuntime *rt, const char *op, char **argv, int argc)
{
    char a[MAX_TEXT];
    char r[MAX_TEXT];
    if (str_eq(op, "list.new")) {
        if (argc < 2)
            return -1;
        int li = list_ref(rt, argv[1]);
        if (li < 0) {
            for (int i = 0; i < VSL_MAX_LISTS; i++) {
                if (!rt->lists[i].used) {
                    li = i;
                    break;
                }
            }
        }
        if (li < 0) {
            set_var(rt, argv[1], "");
            return -1;
        }
        memset(&rt->lists[li], 0, sizeof(rt->lists[li]));
        rt->lists[li].used = 1;
        snprintf(r, sizeof(r), "#list:%d", li);
        set_var(rt, argv[1], r);
        return 0;
    }
    if (str_eq(op, "list.push")) {
        if (argc < 2)
            return -1;
        int li = list_ref(rt, argv[1]);
        if (li < 0 || rt->lists[li].count >= VSL_MAX_LIST_ITEMS)
            return -1;
        rarg(rt, argv, argc, 2, a, sizeof(a));
        str_copy(rt->lists[li].items[rt->lists[li].count++], MAX_TEXT, a);
        return 0;
    }
    if (str_eq(op, "list.get")) {
        if (argc < 4)
            return -1;
        int li = list_ref(rt, argv[2]);
        rarg(rt, argv, argc, 3, a, sizeof(a));
        long i = to_long(a);
        if (li < 0 || i < 0 || i >= rt->lists[li].count) {
            set_var(rt, argv[1], "");
            return -1;
        }
        set_var(rt, argv[1], rt->lists[li].items[i]);
        return 0;
    }
    if (str_eq(op, "list.set")) {
        if (argc < 4)
            return -1;
        int li = list_ref(rt, argv[1]);
        if (li < 0)
            return -1;
        rarg(rt, argv, argc, 2, a, sizeof(a));
        long i = to_long(a);
        rarg(rt, argv, argc, 3, r, sizeof(r));
        if (i < 0 || i > rt->lists[li].count || i >= VSL_MAX_LIST_ITEMS)
            return -1;
        if (i == rt->lists[li].count)
            rt->lists[li].count++;
        str_copy(rt->lists[li].items[i], MAX_TEXT, r);
        return 0;
    }
    if (str_eq(op, "list.len")) {
        if (argc < 3)
            return -1;
        int li = list_ref(rt, argv[2]);
        snprintf(r, sizeof(r), "%d", li < 0 ? 0 : rt->lists[li].count);
        set_var(rt, argv[1], r);
        return 0;
    }
    if (str_eq(op, "list.clear")) {
        if (argc < 2)
            return -1;
        int li = list_ref(rt, argv[1]);
        if (li < 0)
            return -1;
        rt->lists[li].count = 0;
        return 0;
    }
    return EXT_NONE;
}

static int exec_invoke(VslRuntime *rt, char **argv, int argc)
{
    if (argc < 3)
        return -1;
    int fi = -1;
    for (int i = 0; i < rt->fun_count; i++) {
        if (str_eq(rt->funs[i].name, argv[1])) {
            fi = i;
            break;
        }
    }
    if (fi < 0 || rt->depth >= VSL_MAX_CALL_DEPTH) {
        set_var(rt, argv[2], "");
        return 0;
    }
    VslFun *f = &rt->funs[fi];
    int np = f->param_count;
    char vals[VSL_MAX_PARAMS][MAX_TEXT];
    char saved[VSL_MAX_PARAMS][MAX_TEXT];
    int had[VSL_MAX_PARAMS];
    for (int i = 0; i < np; i++)
        rarg(rt, argv, argc, 3 + i, vals[i], sizeof(vals[i]));
    for (int i = 0; i < np; i++) {
        VslVar *v = var_get(rt, f->params[i], 0);
        had[i] = v != NULL;
        str_copy(saved[i], sizeof(saved[i]), v ? v->value : "");
    }
    for (int i = 0; i < np; i++)
        set_var(rt, f->params[i], vals[i]);
    rt->depth++;
    rt->ret_val[0] = 0;
    rt->returning = 0;
    int rc = run_range(rt, f->line_start, f->line_end);
    rt->depth--;
    char result[MAX_TEXT];
    str_copy(result, sizeof(result), rt->ret_val);
    rt->ret_val[0] = 0;
    rt->returning = 0;
    for (int i = 0; i < np; i++) {
        VslVar *v = var_get(rt, f->params[i], 0);
        if (v)
            str_copy(v->value, sizeof(v->value), had[i] ? saved[i] : "");
    }
    set_var(rt, argv[2], result);
    return rc < 0 ? -1 : 0;
}

static int exec_ext(VslRuntime *rt, char **argv, int argc)
{
    const char *op = argv[0];
    char a[MAX_TEXT];
    char b[MAX_TEXT];
    char r[MAX_TEXT];
    int k;

    if (str_starts(op, "list."))
        return exec_list(rt, op, argv, argc);

    if (str_eq(op, "invoke"))
        return exec_invoke(rt, argv, argc);

    if (str_eq(op, "vapi")) {
        if (argc < 3)
            return -1;
        char method[MAX_TEXT];
        char out[MAX_TEXT];
        rarg(rt, argv, argc, 2, method, sizeof(method));
        if (argc > 3)
            rarg(rt, argv, argc, 3, a, sizeof(a));
        if (argc > 4)
            rarg(rt, argv, argc, 4, b, sizeof(b));
        out[0] = 0;
        int st = bridge_call_out(rt, method, argc > 3 ? a : NULL, argc > 4 ? b : NULL, out, sizeof(out));
        snprintf(r, sizeof(r), "%d", st);
        set_var(rt, "status", r);
        set_var(rt, argv[1], out);
        return st == 0 ? 0 : -1;
    }

    k = idx_of(op, k_int_ops);
    if (k >= 0) {
        if (argc < 4)
            return -1;
        rarg(rt, argv, argc, 2, a, sizeof(a));
        rarg(rt, argv, argc, 3, b, sizeof(b));
        long x = to_long(a);
        long y = to_long(b);
        long z = 0;
        switch (k) {
        case 0:
            z = (long)((unsigned long)x + (unsigned long)y);
            break;
        case 1:
            z = (long)((unsigned long)x - (unsigned long)y);
            break;
        case 2:
            z = (long)((unsigned long)x * (unsigned long)y);
            break;
        case 3:
            if (y == -1)
                z = (long)(0UL - (unsigned long)x);
            else if (y != 0)
                z = x / y;
            break;
        default:
            if (y != -1 && y != 0)
                z = x % y;
            break;
        }
        snprintf(r, sizeof(r), "%ld", z);
        set_var(rt, argv[1], r);
        return 0;
    }

    k = idx_of(op, k_flt_ops);
    if (k >= 0) {
        if (argc < 4)
            return -1;
        rarg(rt, argv, argc, 2, a, sizeof(a));
        rarg(rt, argv, argc, 3, b, sizeof(b));
        double x = to_dbl(a);
        double y = to_dbl(b);
        double z = 0.0;
        switch (k) {
        case 0:
            z = x + y;
            break;
        case 1:
            z = x - y;
            break;
        case 2:
            z = x * y;
            break;
        default:
            if (y != 0.0)
                z = x / y;
            break;
        }
        snprintf(r, sizeof(r), "%g", z);
        set_var(rt, argv[1], r);
        return 0;
    }

    k = idx_of(op, k_cmp_ops);
    if (k >= 0) {
        if (argc < 4)
            return -1;
        rarg(rt, argv, argc, 2, a, sizeof(a));
        rarg(rt, argv, argc, 3, b, sizeof(b));
        set_var(rt, argv[1], cmp_apply(k, cmp_values(a, b)) ? "1" : "0");
        return 0;
    }

    if (str_eq(op, "neg")) {
        if (argc < 3)
            return -1;
        rarg(rt, argv, argc, 2, a, sizeof(a));
        snprintf(r, sizeof(r), "%ld", (long)(0UL - (unsigned long)to_long(a)));
        set_var(rt, argv[1], r);
        return 0;
    }
    if (str_eq(op, "not")) {
        if (argc < 3)
            return -1;
        rarg(rt, argv, argc, 2, a, sizeof(a));
        set_var(rt, argv[1], truthy(a) ? "0" : "1");
        return 0;
    }
    if (str_eq(op, "concat")) {
        if (argc < 4)
            return -1;
        rarg(rt, argv, argc, 2, a, sizeof(a));
        rarg(rt, argv, argc, 3, b, sizeof(b));
        str_copy(r, sizeof(r), a);
        str_cat(r, sizeof(r), b);
        set_var(rt, argv[1], r);
        return 0;
    }
    if (str_eq(op, "len")) {
        if (argc < 3)
            return -1;
        rarg(rt, argv, argc, 2, a, sizeof(a));
        snprintf(r, sizeof(r), "%zu", strlen(a));
        set_var(rt, argv[1], r);
        return 0;
    }
    if (str_eq(op, "substr")) {
        if (argc < 4)
            return -1;
        rarg(rt, argv, argc, 2, a, sizeof(a));
        rarg(rt, argv, argc, 3, b, sizeof(b));
        long len = (long)strlen(a);
        long start = to_long(b);
        long n = len;
        if (argc > 4) {
            rarg(rt, argv, argc, 4, r, sizeof(r));
            n = to_long(r);
        }
        if (start < 0)
            start = 0;
        if (start > len)
            start = len;
        if (n < 0)
            n = 0;
        if (n > len - start)
            n = len - start;
        char out[MAX_TEXT];
        str_ncopy(out, sizeof(out), a + start, (size_t)n);
        set_var(rt, argv[1], out);
        return 0;
    }
    if (str_eq(op, "tostr")) {
        if (argc < 3)
            return -1;
        rarg(rt, argv, argc, 2, a, sizeof(a));
        if (is_num(a) && !strpbrk(a, ".eE"))
            snprintf(r, sizeof(r), "%ld", to_long(a));
        else
            str_copy(r, sizeof(r), a);
        set_var(rt, argv[1], r);
        return 0;
    }
    if (str_eq(op, "toint")) {
        if (argc < 3)
            return -1;
        rarg(rt, argv, argc, 2, a, sizeof(a));
        double d = to_dbl(a);
        long v = 0;
        if (d >= -9.0e18 && d <= 9.0e18)
            v = (long)d;
        snprintf(r, sizeof(r), "%ld", v);
        set_var(rt, argv[1], r);
        return 0;
    }
    return EXT_NONE;
}

static int exec_line(VslRuntime *rt, char *line)
{
    line = trim(line);
    if (!line[0] || line[0] == '#')
        return 0;

    char tb[VSL_LINE_MAX];
    char *argv[VSL_MAX_ARGS];
    str_copy(tb, sizeof(tb), line);
    int argc = tokenize(tb, argv, VSL_MAX_ARGS);
    if (argc > 0) {
        int er = exec_ext(rt, argv, argc);
        if (er != EXT_NONE)
            return er;
    }

    char buf[VSL_LINE_MAX];
    str_copy(buf, sizeof(buf), line);
    char *cmd = strtok(buf, " \t");
    if (!cmd)
        return 0;
    char *a1 = strtok(NULL, " \t");
    char *a2 = strtok(NULL, "");
    if (a1)
        a1 = trim(a1);
    if (a2)
        a2 = trim(a2);
    if (a1)
        unquote(a1);
    if (a2)
        unquote(a2);

    if (str_eq(cmd, "let") && a1 && a2) {
        char val[MAX_TEXT];
        resolve_token(rt, a2, val, sizeof(val));
        VslVar *v = var_get(rt, a1, 1);
        if (v)
            str_copy(v->value, sizeof(v->value), val);
        return 0;
    }
    if (strchr(cmd, '.')) {
        char val1[MAX_TEXT];
        char val2[MAX_TEXT];
        const char *arg1 = a1;
        const char *arg2 = a2;
        if (a1) {
            resolve_token(rt, a1, val1, sizeof(val1));
            arg1 = val1;
        }
        if (a2) {
            resolve_token(rt, a2, val2, sizeof(val2));
            arg2 = val2;
        }
        return bridge_exec_cmd(rt, cmd, arg1, arg2);
    }
    if (str_eq(cmd, "set_text") && a1 && a2) {
        char val[MAX_TEXT];
        resolve_token(rt, a2, val, sizeof(val));
        return bridge_exec_cmd(rt, "ui.set_text", a1, val);
    }
    if (str_eq(cmd, "append_text") && a1 && a2) {
        char val[MAX_TEXT];
        resolve_token(rt, a2, val, sizeof(val));
        return bridge_exec_cmd(rt, "ui.append_text", a1, val);
    }
    if (str_eq(cmd, "clear_text") && a1)
        return bridge_exec_cmd(rt, "ui.clear_text", a1, NULL);
    if (str_eq(cmd, "nav") && a1)
        return bridge_nav(rt, a1);
    if (str_eq(cmd, "push_route") && a1)
        return bridge_exec_cmd(rt, "ui.push", a1, NULL);
    if (str_eq(cmd, "replace_route") && a1)
        return bridge_exec_cmd(rt, "ui.replace", a1, NULL);
    if (str_eq(cmd, "pop_route") || str_eq(cmd, "back"))
        return bridge_exec_cmd(rt, "ui.pop", NULL, NULL);
    if (str_eq(cmd, "go_home") || str_eq(cmd, "home"))
        return bridge_exec_cmd(rt, "ui.home", NULL, NULL);
    if (str_eq(cmd, "call"))
        return bridge_exec_cmd(rt, "telephony.call", NULL, NULL);
    if (str_eq(cmd, "require") && a1)
        return bridge_exec_cmd(rt, "require", a1, NULL);
    if (str_eq(cmd, "storage_set") && a1 && a2) {
        char val[MAX_TEXT];
        resolve_token(rt, a2, val, sizeof(val));
        return bridge_exec_cmd(rt, "storage.app.set", a1, val);
    }
    if (str_eq(cmd, "storage_get") && a1 && a2)
        return bridge_exec_cmd(rt, "storage.app.get", a1, a2);
    if (str_eq(cmd, "digit") && a1)
        return bridge_exec_cmd(rt, "telephony.digit", a1, NULL);
    if (str_eq(cmd, "action") && a1) {
        static int dispatching;
        char target[MAX_TEXT];
        char nav[MAX_PATH];
        nav[0] = 0;
        resolve_token(rt, a1, target, sizeof(target));
        if (!dispatching) {
            static const char *const ctx_vars[] = {"key", "target", "event"};
            char saved[3][MAX_TEXT];
            int had[3];
            for (int i = 0; i < 3; i++) {
                VslVar *v = var_get(rt, ctx_vars[i], 0);
                had[i] = v != NULL;
                str_copy(saved[i], sizeof(saved[i]), v ? v->value : "");
            }
            dispatching = 1;
            int matched = vsl_fire(rt, "action", target);
            dispatching = 0;
            for (int i = 0; i < 3; i++) {
                if (had[i])
                    set_var(rt, ctx_vars[i], saved[i]);
            }
            if (matched > 0)
                return 0;
        }
        int rc = screen_handle_action(rt->screen, target, nav, sizeof(nav));
        if (rc == 1 && nav[0])
            str_copy(rt->nav, sizeof(rt->nav), nav);
        return rc;
    }
    return bridge_exec_cmd(rt, cmd, a1, a2);
}

static int find_label(VslRuntime *rt, int start, int end, const char *name)
{
    for (int i = start; i <= end && i < rt->line_count; i++) {
        const char *t = rt->lines[i];
        if (!word_is(t, "label"))
            continue;
        const char *p = t + 5;
        while (*p && isspace((unsigned char)*p))
            p++;
        if (str_eq(p, name))
            return i;
    }
    return -1;
}

static int run_range(VslRuntime *rt, int start, int end)
{
    int pc = start;
    if (end >= rt->line_count)
        end = rt->line_count - 1;
    while (pc <= end) {
        if (rt->steps++ >= VSL_MAX_STEPS)
            return -1;
        char line[VSL_LINE_MAX];
        str_copy(line, sizeof(line), rt->lines[pc] ? rt->lines[pc] : "");
        char *t = trim(line);
        if (!t[0] || t[0] == '#') {
            pc++;
            continue;
        }
        if (str_eq(t, "end") || str_starts(t, "on_"))
            break;

        char *argv[VSL_MAX_ARGS];
        char tb[VSL_LINE_MAX];
        int argc = 0;

        if (word_is(t, "if") || word_is(t, "while")) {
            str_copy(tb, sizeof(tb), t);
            argc = tokenize(tb, argv, VSL_MAX_ARGS);
            int is_while = word_is(t, "while");
            int ok = eval_cond(rt, argv, argc);
            if (ok) {
                pc++;
            } else if (is_while) {
                pc = rt->jump[pc] >= 0 ? rt->jump[pc] + 1 : end + 1;
            } else {
                pc = rt->jump[pc] >= 0 ? rt->jump[pc] : end + 1;
            }
            continue;
        }
        if (str_eq(t, "else")) {
            pc = rt->jump[pc] >= 0 ? rt->jump[pc] : end + 1;
            continue;
        }
        if (str_eq(t, "endif")) {
            pc++;
            continue;
        }
        if (str_eq(t, "endwhile")) {
            pc = rt->jump[pc] >= 0 ? rt->jump[pc] : pc + 1;
            continue;
        }
        if (word_is(t, "fn")) {
            pc = rt->jump[pc] >= 0 ? rt->jump[pc] + 1 : end + 1;
            continue;
        }
        if (str_eq(t, "endfn") || word_is(t, "label")) {
            pc++;
            continue;
        }
        if (word_is(t, "ret")) {
            str_copy(tb, sizeof(tb), t);
            argc = tokenize(tb, argv, VSL_MAX_ARGS);
            rarg(rt, argv, argc, 1, rt->ret_val, sizeof(rt->ret_val));
            rt->returning = 1;
            return 0;
        }
        if (word_is(t, "jmp") || word_is(t, "jz") || word_is(t, "jnz")) {
            str_copy(tb, sizeof(tb), t);
            argc = tokenize(tb, argv, VSL_MAX_ARGS);
            int is_jmp = word_is(t, "jmp");
            int want = word_is(t, "jnz");
            int target_idx = is_jmp ? 1 : 2;
            int go = 1;
            if (!is_jmp) {
                char a[MAX_TEXT];
                rarg(rt, argv, argc, 1, a, sizeof(a));
                go = truthy(a) == want;
            }
            int dest = -1;
            if (go && target_idx < argc)
                dest = find_label(rt, start, end, argv[target_idx]);
            pc = dest >= 0 ? dest + 1 : pc + 1;
            continue;
        }

        int rc = exec_line(rt, t);
        if (rc < 0 && (str_starts(t, "require") || str_starts(t, "invoke")))
            return -1;
        pc++;
    }
    return 0;
}

static int ends_with(const char *s, const char *suf)
{
    size_t n, m;
    if (!s || !suf)
        return 0;
    n = strlen(s);
    m = strlen(suf);
    if (m > n)
        return 0;
    return str_eq(s + (n - m), suf);
}

static void register_fn(VslRuntime *rt, const char *line, int line_no)
{
    if (rt->fun_count >= VSL_MAX_FUNS)
        return;
    char tb[VSL_LINE_MAX];
    char *argv[VSL_MAX_ARGS];
    str_copy(tb, sizeof(tb), line);
    int argc = tokenize(tb, argv, VSL_MAX_ARGS);
    if (argc < 2)
        return;
    VslFun *f = &rt->funs[rt->fun_count++];
    memset(f, 0, sizeof(*f));
    str_copy(f->name, sizeof(f->name), argv[1]);
    for (int i = 2; i < argc && f->param_count < VSL_MAX_PARAMS; i++) {
        const char *p = argv[i][0] == '$' ? argv[i] + 1 : argv[i];
        str_copy(f->params[f->param_count++], MAX_ID, p);
    }
    f->line_start = line_no + 1;
    f->line_end = rt->line_count - 1;
}

static void build_jumps(VslRuntime *rt)
{
    int sline[VSL_BLOCK_DEPTH];
    int skind[VSL_BLOCK_DEPTH];
    int sfun[VSL_BLOCK_DEPTH];
    int sp = 0;
    for (int i = 0; i < rt->line_count; i++)
        rt->jump[i] = -1;
    for (int i = 0; i < rt->line_count; i++) {
        const char *t = rt->lines[i];
        if (str_eq(t, "end") || str_starts(t, "on_")) {
            sp = 0;
            continue;
        }
        if (word_is(t, "if") || word_is(t, "while") || word_is(t, "fn")) {
            if (sp >= VSL_BLOCK_DEPTH)
                continue;
            sline[sp] = i;
            skind[sp] = word_is(t, "if") ? 1 : (word_is(t, "while") ? 3 : 4);
            sfun[sp] = -1;
            if (skind[sp] == 4) {
                int before = rt->fun_count;
                register_fn(rt, t, i);
                if (rt->fun_count > before)
                    sfun[sp] = rt->fun_count - 1;
            }
            sp++;
            continue;
        }
        if (str_eq(t, "else")) {
            if (sp > 0 && skind[sp - 1] == 1) {
                rt->jump[sline[sp - 1]] = i + 1;
                skind[sp - 1] = 2;
                sline[sp - 1] = i;
            }
            continue;
        }
        if (str_eq(t, "endif")) {
            if (sp > 0 && (skind[sp - 1] == 1 || skind[sp - 1] == 2)) {
                rt->jump[sline[sp - 1]] = i;
                sp--;
            }
            continue;
        }
        if (str_eq(t, "endwhile")) {
            if (sp > 0 && skind[sp - 1] == 3) {
                rt->jump[sline[sp - 1]] = i;
                rt->jump[i] = sline[sp - 1];
                sp--;
            }
            continue;
        }
        if (str_eq(t, "endfn")) {
            if (sp > 0 && skind[sp - 1] == 4) {
                rt->jump[sline[sp - 1]] = i;
                if (sfun[sp - 1] >= 0)
                    rt->funs[sfun[sp - 1]].line_end = i - 1;
                sp--;
            }
            continue;
        }
    }
}

int vsl_load(VslRuntime *rt, const PkgApp *app, Screen *screen, const char *script_path)
{
    static VslVar keep[VSL_MAX_VARS];
    static VslList keep_lists[VSL_MAX_LISTS];
    static int keep_flag[VSL_MAX_LISTS];
    char resolved[MAX_PATH];
    int nkeep = 0;
    if (!rt || !app || !screen)
        return -1;
    memset(keep_flag, 0, sizeof(keep_flag));
    if (rt->loaded && app->id[0] && str_eq(rt->app.id, app->id)) {
        for (int i = 0; i < rt->var_count; i++) {
            if (!str_starts(rt->vars[i].name, "g_"))
                continue;
            keep[nkeep++] = rt->vars[i];
            if (str_starts(rt->vars[i].value, "#list:")) {
                int li = atoi(rt->vars[i].value + 6);
                if (li >= 0 && li < VSL_MAX_LISTS && rt->lists[li].used) {
                    keep_lists[li] = rt->lists[li];
                    keep_flag[li] = 1;
                }
            }
        }
    }
    vsl_reset(rt);
    rt->app = *app;
    rt->screen = screen;
    for (int i = 0; i < nkeep; i++)
        rt->vars[rt->var_count++] = keep[i];
    for (int i = 0; i < VSL_MAX_LISTS; i++) {
        if (keep_flag[i])
            rt->lists[i] = keep_lists[i];
    }
    if (!script_path || !script_path[0]) {
        rt->loaded = 1;
        return 0;
    }
    str_copy(resolved, sizeof(resolved), script_path);
    if (ends_with(resolved, ".kt")) {
        char alt[MAX_PATH];
        str_copy(alt, sizeof(alt), resolved);
        char *slash = strrchr(alt, '/');
        if (slash) {
            slash[1] = 0;
            str_cat(alt, sizeof(alt), "../bin/main.kbc");
        } else {
            str_copy(alt, sizeof(alt), "bin/main.kbc");
        }
        FILE *altf = fopen(alt, "r");
        if (altf) {
            fclose(altf);
            str_copy(resolved, sizeof(resolved), alt);
        }
    }
    FILE *f = fopen(resolved, "r");
    if (!f) {
        rt->loaded = 1;
        return 0;
    }
    size_t n = fread(rt->src, 1, sizeof(rt->src) - 1, f);
    fclose(f);
    rt->src[n] = 0;

    char *p = rt->src;
    while (*p && rt->line_count < VSL_MAX_LINES) {
        rt->lines[rt->line_count++] = p;
        char *nl = strchr(p, '\n');
        if (!nl)
            break;
        *nl = 0;
        if (nl > p && nl[-1] == '\r')
            nl[-1] = 0;
        p = nl + 1;
    }
    for (int i = 0; i < rt->line_count; i++)
        rt->lines[i] = trim(rt->lines[i]);

    for (int i = 0; i < rt->line_count; i++) {
        const char *t = rt->lines[i];
        if (!str_starts(t, "on_"))
            continue;
        if (rt->handler_count >= VSL_MAX_HANDLERS)
            break;
        VslHandler *h = &rt->handlers[rt->handler_count++];
        memset(h, 0, sizeof(*h));
        char ev[24] = {0};
        char tgt[MAX_ID] = {0};
        if (sscanf(t, "on_%23s %31s", ev, tgt) >= 1) {
            str_copy(h->event, sizeof(h->event), ev);
            if (tgt[0]) {
                unquote(tgt);
                str_copy(h->target, sizeof(h->target), tgt);
            }
        }
        h->line_start = i + 1;
        h->line_end = rt->line_count - 1;
        for (int j = i + 1; j < rt->line_count; j++) {
            const char *t2 = rt->lines[j];
            if (str_eq(t2, "end") || str_starts(t2, "on_")) {
                h->line_end = j - 1;
                break;
            }
        }
    }
    build_jumps(rt);
    rt->loaded = 1;
    vsl_fire(rt, "load", "");
    return 0;
}

int vsl_fire(VslRuntime *rt, const char *event, const char *target)
{
    if (!rt || !rt->loaded || !event)
        return 0;
    if (target) {
        VslVar *v = var_get(rt, "key", 1);
        if (v)
            str_copy(v->value, sizeof(v->value), target);
        v = var_get(rt, "target", 1);
        if (v)
            str_copy(v->value, sizeof(v->value), target);
        v = var_get(rt, "event", 1);
        if (v)
            str_copy(v->value, sizeof(v->value), event);
    }
    if (rt->depth == 0)
        rt->steps = 0;
    int matched = 0;
    for (int i = 0; i < rt->handler_count; i++) {
        VslHandler *h = &rt->handlers[i];
        if (!str_eq(h->event, event))
            continue;
        if (h->target[0] && target && target[0] && !str_eq(h->target, target) && !str_eq(h->target, "*"))
            continue;
        if (h->target[0] && (!target || !target[0]) && !str_eq(h->target, "*"))
            continue;
        run_range(rt, h->line_start, h->line_end);
        rt->returning = 0;
        matched = 1;
        if (rt->steps >= VSL_MAX_STEPS)
            break;
    }
    return matched;
}

int vsl_has_nav(const VslRuntime *rt)
{
    return rt && rt->nav[0] != 0;
}

const char *vsl_take_nav(VslRuntime *rt)
{
    if (!rt || !rt->nav[0])
        return "";
    return rt->nav;
}

int vsl_consume_nav(VslRuntime *rt, char *out, size_t out_sz)
{
    if (!out || out_sz == 0)
        return 0;
    out[0] = 0;
    if (!rt || !rt->nav[0])
        return 0;
    str_copy(out, out_sz, rt->nav);
    rt->nav[0] = 0;
    return 1;
}
