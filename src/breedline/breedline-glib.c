#include <stdlib.h>
#include <string.h>

#include <poll.h>
#include <sys/poll.h>
#include <glib.h>

#include <breedline/breedline.h>

typedef struct {
    GIOChannel *ioc;
    guint       iow;
    void      (*cb)(int fd, int events, void *user_data);
    void       *user_data;
} watch_t;


static gboolean io_cb(GIOChannel *ioc, GIOCondition events, void *user_data)
{
    watch_t *w  = (watch_t *)user_data;
    int      e  = 0;
    int      fd = g_io_channel_unix_get_fd(ioc);

    if (events & G_IO_IN)
        e |= POLLIN;
    if (events & G_IO_HUP)
        e |= POLLHUP;

    w->cb(fd, e, w->user_data);

    return TRUE;
}


static void *add_watch(void *mlp, int fd,
                       void (*cb)(int fd, int events, void *user_data),
                       void *user_data)
{
    GIOCondition  events = G_IO_IN | G_IO_HUP;
    watch_t      *w;

    (void)mlp;

    w = malloc(sizeof(*w));

    if (w != NULL) {
        memset(w, 0, sizeof(*w));
        w->cb        = cb;
        w->user_data = user_data;
        w->ioc       = g_io_channel_unix_new(fd);

        if (w->ioc != NULL) {
            w->iow = g_io_add_watch(w->ioc, events, io_cb, w);

            if (w->iow != 0)
                return w;

            g_io_channel_unref(w->ioc);
        }

        free(w);
    }

    return NULL;
}


static void del_watch(void *wp)
{
    watch_t *w = (watch_t *)wp;

    if (w != NULL) {
        g_source_remove(w->iow);
        g_io_channel_unref(w->ioc);

        free(w);
    }
}


static brl_mainloop_ops_t ml_ops = {
    .add_watch = add_watch,
    .del_watch = del_watch
};


brl_t *brl_create_with_glib(int fd, const char *prompt, GMainLoop *ml,
                              brl_line_cb_t cb, void *user_data)
{
    brl_t *brl;

    brl = brl_create(fd, prompt);

    if (brl != NULL) {
        if (brl_use_mainloop(brl, ml, &ml_ops, cb, user_data) == 0)
            return brl;
        else
            brl_destroy(brl);
    }

    return NULL;
}
