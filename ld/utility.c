#include "ld.h"

_Noreturn void error(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    char *str = vformat(msg, args);
    va_end(args);

    // fprintf(stderr, "[ERROR] %s\n", str);
    printf("[ERROR] %s\n", str);
    // fprintf(stderr, "[DEBUG] %s, %d\n", __FILE__, __LINE__);
    exit(EXIT_FAILURE);
}

void warn(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    char *str = vformat(msg, args);
    va_end(args);

    // fprintf(stderr, "[WARN]  %s\n", str);
    printf("[WARN]  %s\n", str);
    // fprintf(stderr, "[DEBUG] %s, %d\n", __FILE__, __LINE__);
}

void *safe_malloc(int size)
{
    void *ptr;

    ptr = malloc(size);
    if (ptr == NULL) error("malloc failed.");
    return ptr;
}

char *new_str(const char *src)
{
    char *ret = safe_malloc(strlen(src) + 1);
    strcpy(ret, src);
    return ret;
}

int *new_int(int src)
{
    int *ret = safe_malloc(sizeof(int));
    *ret = src;
    return ret;
}

char *vformat(const char *src, va_list ap)
{
    char buf[512];  // TODO: enough length?
    vsprintf(buf, src, ap);

    char *ret = safe_malloc(strlen(buf) + 1);
    strcpy(ret, buf);
    return ret;
}

char *format(const char *src, ...)
{
    va_list args;
    va_start(args, src);
    char *ret = vformat(src, args);
    va_end(args);
    return ret;
}

int unescape_char(int src)
{
    static int table[128];
    if (table[0] == 0) {
        memset(table, 255, sizeof(table));

        table['n'] = '\n';
        table['r'] = '\r';
        table['t'] = '\t';
        table['0'] = '\0';
        table['a'] = '\a';
        table['b'] = '\b';
        table['v'] = '\v';
        table['f'] = '\f';
    }

    int ch = table[src];
    return ch == -1 ? src : ch;
}

char *escape_string(char *str, int size)
{
    StringBuilder *sb = new_string_builder();
    for (int i = 0; i < size; i++) {
        char ch = str[i];

        switch (ch) {
            case '\n':
                string_builder_append(sb, '\\');
                string_builder_append(sb, 'n');
                break;

            case '\r':
                string_builder_append(sb, '\\');
                string_builder_append(sb, 'n');
                break;

            case '\t':
                string_builder_append(sb, '\\');
                string_builder_append(sb, 't');
                break;

            case '\0':
                string_builder_append(sb, '\\');
                string_builder_append(sb, '0');
                break;

            case '\a':
                string_builder_append(sb, '\\');
                string_builder_append(sb, 'a');
                break;

            case '\b':
                string_builder_append(sb, '\\');
                string_builder_append(sb, 'b');
                break;

            case '\v':
                string_builder_append(sb, '\\');
                string_builder_append(sb, 'v');
                break;

            case '\f':
                string_builder_append(sb, '\\');
                string_builder_append(sb, 'f');
                break;

            case '"':
                string_builder_append(sb, '\\');
                string_builder_append(sb, '"');
                break;

            default:
                string_builder_append(sb, ch);
                break;
        }
    }

    return string_builder_get(sb);
}

int min(int a, int b) { return a < b ? a : b; }

int max(int a, int b) { return a < b ? b : a; }

int roundup(int n, int b) { return (n + b - 1) & ~(b - 1); }
