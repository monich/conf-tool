/*
 * Copyright (C) 2020 Jolla Ltd.
 * Copyright (C) 2020 Slava Monich <slava@monich.com>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *   3. Neither the names of the copyright holders nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * any official policies, either expressed or implied.
 */

#include <glib.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if !GLIB_CHECK_VERSION(2,42,0)
#  define G_OPTION_FLAG_NONE 0
#endif

#define STANDARD_INPUT "-"

typedef enum ret_val {
    RET_FILE_UPDATED = -1,
    RET_OK,
    RET_CMDLINE,
    RET_FILE_NOT_FOUND,
    RET_FILE_LOAD_ERROR,
    RET_FILE_UPDATE_ERROR,
    RET_SECTION_NOT_FOUND,
    RET_KEY_NOT_FOUND
} RET_VAL;

static gboolean quiet = FALSE;

static
void
app_print_error(
    const char* format,
    ...) G_GNUC_PRINTF(1, 2);

static
void
app_print_error(
    const char* format,
    ...)
{
    if (!quiet) {
        va_list args;

        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }
}

static
void
app_error(
    GError* error)
{
    if (error) {
        app_print_error("%s\n", error->message);
        g_error_free(error);
    }
}

static
void
app_print_strv(
    char** strv)
{
    guint i;

    for (i = 0; strv[i]; i++) {
        printf("%s\n", strv[i]);
    }
}

static
RET_VAL
app_handle_key(
    GKeyFile* keyfile,
    const char* section,
    const char* key,
    const char* value,
    gboolean remove)
{
    GError* error = NULL;

    if (remove) {
        if (value) {
            app_print_error("You can't set and remove key at the same time\n");
            return RET_CMDLINE;
        } else if (g_key_file_remove_key(keyfile, section, key, &error)) {
            return RET_FILE_UPDATED;
        } else {
            app_error(error);
            return RET_KEY_NOT_FOUND;
        }
    } else if (value) {
        g_key_file_set_string(keyfile, section, key, value);
        return RET_FILE_UPDATED;
    } else {
        char* str = g_key_file_get_string(keyfile, section, key, &error);

        if (str) {
            printf("%s", str);
            g_free(str);
            return RET_OK;
        } else {
            app_error(error);
            return RET_KEY_NOT_FOUND;
        }
    }
}

static
RET_VAL
app_handle_keyfile(
    GKeyFile* keyfile,
    const char* section,
    const char* key,
    const char* value,
    gboolean remove)
{
    GError* error = NULL;
    char** strv = NULL;
    RET_VAL ret;

    if (section) {
        if (key) {
            ret = app_handle_key(keyfile, section, key, value, remove);
        } else if (remove) {
            if (g_key_file_remove_group(keyfile, section, &error)) {
                ret = RET_FILE_UPDATED;
            } else {
                app_error(error);
                ret = RET_SECTION_NOT_FOUND;
            }
        } else {
            strv = g_key_file_get_keys(keyfile, section, NULL, &error);
            if (strv) {
                /* Print keys */
                app_print_strv(strv);
                ret = RET_OK;
            } else {
                app_error(error);
                ret = RET_SECTION_NOT_FOUND;
            }
        }
    } else if (key) {
        section = (strv = g_key_file_get_groups(keyfile, NULL))[0];
        if (section) {
            ret = app_handle_key(keyfile, section, key, value, remove);
        } else {
            app_print_error("Config file has no sections\n");
            ret = RET_SECTION_NOT_FOUND;
        }
    } else if (remove) {
        app_print_error("You have to specify key or section to remove\n");
        ret = RET_CMDLINE;
    } else {
        /* Print sections */
        app_print_strv(strv = g_key_file_get_groups(keyfile, NULL));
        ret = RET_OK;
    }
    g_strfreev(strv);
    return ret;
}

static
RET_VAL
app_run(
    const char* file,
    const char* section,
    const char* key,
    const char* value,
    gboolean remove)
{
    /* Keep the comments if we are going to modify the file */
    const GKeyFileFlags flags = (value || remove) ?
        G_KEY_FILE_KEEP_COMMENTS : G_KEY_FILE_NONE;

    if (!strcmp(file, STANDARD_INPUT)) {
        GKeyFile* keyfile = g_key_file_new();
        GByteArray* buf = g_byte_array_new();
        GError* error = NULL;
        guint8 chunk[256];
        gssize nread;
        RET_VAL ret;

        while ((nread = read(STDIN_FILENO, chunk, sizeof(chunk))) > 0) {
            g_byte_array_append(buf, chunk, nread);
        }
        if (g_key_file_load_from_data(keyfile, (void*)buf->data, buf->len,
            flags, &error)) {
            ret = app_handle_keyfile(keyfile, section, key, value, remove);
            if (ret == RET_FILE_UPDATED) {
                gchar* data = g_key_file_to_data(keyfile, NULL, NULL);

                /* Dump updated file to stdout */
                printf("%s", data);
                g_free(data);
                ret = RET_OK;
            }
        } else {
            app_error(error);
            ret = RET_FILE_LOAD_ERROR;
        }
        g_byte_array_free(buf, TRUE);
        g_key_file_free(keyfile);
        return ret;
    } else {
        if (g_file_test(file, G_FILE_TEST_EXISTS)) {
            GKeyFile* keyfile = g_key_file_new();
            GError* error = NULL;
            RET_VAL ret;

            if (g_key_file_load_from_file(keyfile, file, flags, &error)) {
                ret = app_handle_keyfile(keyfile, section, key, value, remove);
                if (ret == RET_FILE_UPDATED) {
                    gsize size;
                    gchar* data = g_key_file_to_data(keyfile, &size, NULL);

                    if (g_file_set_contents(file, data, size, &error)) {
                        ret = RET_OK;
                    } else {
                        app_error(error);
                        ret = RET_FILE_UPDATE_ERROR;
                    }
                    g_free(data);
                }
            } else {
                app_error(error);
                ret = RET_FILE_LOAD_ERROR;
            }
            g_key_file_free(keyfile);
            return ret;
        } else {
            app_print_error("No such file: %s\n", file);
            return RET_FILE_NOT_FOUND;
        }
    }
}
    
int main(int argc, char* argv[])
{
    char* section = NULL;
    gboolean remove = FALSE;
    GOptionEntry entries[] = {
        { "section", 's',  G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &section,
          "Config section", NULL },
        { "remove", 'r',  G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &remove,
          "Remove the specified key or section", NULL },
        { "quiet", 'q',  G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &quiet,
          "Don't print errors", NULL },
        { NULL }
    };
    GOptionContext* options = g_option_context_new("FILE [KEY [VALUE]]");
    GError* error = NULL;
    gboolean ok;
    RET_VAL ret = RET_CMDLINE;

    g_option_context_set_summary(options, "Tool for parsing config files.");
    g_option_context_add_main_entries(options, entries, NULL);
    ok = g_option_context_parse(options, &argc, &argv, &error);
    if (ok) {
        if (argc < 2 || argc > 4) {
            char* help = g_option_context_get_help(options, TRUE, NULL);

            app_print_error("%s", help);
            g_free(help);
        } else {
            const char* key = (argc > 2) ? argv[2] : NULL;
            const char* value = (argc > 3) ? argv[3] : NULL;

            ret = app_run(argv[1], section, key, value, remove);
        }
    } else {
        app_error(error);
    }
    g_option_context_free(options);
    g_free(section);
    return ret;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
