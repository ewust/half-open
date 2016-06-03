
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>
#include <event2/buffer.h>
#include <event2/util.h>

#include <assert.h>

#define TIMEOUT_SECS 30


struct config {
    struct event_base   *base;
    char                *host;
    size_t              num_connections;
    size_t              last_connections;
    SSL_CTX             *ssl_ctx;
};

enum tls_state {
    INVALID=-1, CONNECTING, CONNECTED, CLOSED,
};

struct conn_state {
    struct config          *conf;
    struct bufferevent     *bev;
    enum tls_state          state;
};

void new_connection(struct config *conf);

void readcb(struct bufferevent *bev, void *ptr)
{

}

void writecb(struct bufferevent *bev, void *ptr)
{

}

void eventcb(struct bufferevent *bev, short events, void *ptr)
{
    struct conn_state *st = ptr;

    if (events & BEV_EVENT_CONNECTED) {
        // Yay?
        st->state = CONNECTED;
        st->conf->num_connections++;
    } else if (events & (BEV_EVENT_EOF | BEV_EVENT_TIMEOUT | BEV_EVENT_ERROR)) {
        // closed, cleanup
        if (st->state == CONNECTED) {
            st->conf->num_connections--;
        }
        st->state = CLOSED;
    } else {
        printf("weird event\n");
    }

}

void new_connection(struct config *conf)
{
    struct conn_state *st = calloc(1, sizeof(struct conn_state));
    st->state = CONNECTING;
    st->conf = conf;


    int rv;
    SSL *ssl;

    ssl = SSL_new(conf->ssl_ctx);
    if (!ssl) {
        printf("No ssl\n");
    }
    st->bev = bufferevent_openssl_socket_new(
      conf->base, -1, ssl, BUFFEREVENT_SSL_CONNECTING,
      BEV_OPT_DEFER_CALLBACKS | BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(st->bev, readcb, writecb, eventcb, st);
    rv = bufferevent_socket_connect_hostname(st->bev, NULL, AF_INET, conf->host, 443);
    if (rv < 0) {
        perror("socket connected failed: ");
        printf("Run `sudo sysctl -w net.ipv4.tcp_tw_recycle=1`\n");
        // TODO: free bev
        return;
    }


    char *msg = "GET / HTTP/1.1\r\nHost: test.com\r\nX-Ignore: ABCEDFGHIJKLMNOPQRSTUVWXYZ\r\n";

    evbuffer_add(bufferevent_get_output(st->bev), msg, strlen(msg));

    bufferevent_enable(st->bev, EV_READ | EV_WRITE);
}


// Initialize state and create #conns connections
void fetcher(struct config *conf, int conns)
{
    int i;
    for (i=0; i<conns; i++) {
        new_connection(conf);
        usleep(5000);
    }
}

void print_status(evutil_socket_t fd, short what, void *arg)
{
    struct config *conf = arg;

    printf("%lu connections open\n", conf->num_connections);
    conf->last_connections = conf->num_connections;
}

int main()
{
    struct config conf;
    memset(&conf, 0, sizeof(conf));

    // Lookup IP of target
    /*
    struct hostent *he = gethostbyname("128.138.200.81");
    memset(&conf.sin, 0, sizeof(conf.sin));
    conf.sin.sin_family  = he->h_addrtype;
    conf.sin.sin_port    = htons(443);
    conf.sin.sin_addr    = *(((struct in_addr **)he->h_addr_list)[0]);
    */

    // Dummy values

    conf.host = "128.138.200.81";

    conf.base = event_base_new();

    SSL_library_init();
    conf.ssl_ctx = SSL_CTX_new(TLSv1_2_method());
    if (!conf.ssl_ctx) {
        printf("No SSL_ctx\n");
    }


    struct timeval one_sec = {1,0};
    struct event *status_ev = event_new(conf.base, -1, EV_PERSIST, print_status, &conf);
    event_add(status_ev, &one_sec);

    fetcher(&conf, 1000);
    event_base_dispatch(conf.base);

    return 0;
}
