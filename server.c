#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <signal.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "auction.h"

/
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define DIM     "\033[2m"



typedef struct {
    int       fd;
    int       id;
    char      name[MAX_NAME];
    int       active;
    pthread_t thread;
} Client;

typedef struct {
    char      item[64];
    int       start_price;
    int       current_price;
    char      best_bidder[MAX_NAME];
    int       time_left;
    int       open;
    int       round;
    pthread_mutex_t lock;
} AuctionState;

static AuctionState g_auction;
static Client       g_clients[MAX_CLIENTS];
static int          g_client_count = 0;
static pthread_mutex_t g_clients_lock = PTHREAD_MUTEX_INITIALIZER;
static int          g_next_id = 1;
static int          g_server_fd = -1;



void send_msg(int fd, const char *fmt, ...) {
    char buf[MSG_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf) - 2, fmt, args);
    va_end(args);
    strcat(buf, "\r\n");
    if (send(fd, buf, strlen(buf), MSG_NOSIGNAL) < 0) {
       
    }
}

static void broadcast(const char *fmt, ...) {
    char buf[MSG_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    pthread_mutex_lock(&g_clients_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].active) {
            send_msg(g_clients[i].fd, "%s", buf);
        }
    }
    pthread_mutex_unlock(&g_clients_lock);
}

static void log_msg(const char *color, const char *prefix, const char *fmt, ...) {
    char buf[MSG_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char ts[16];
    strftime(ts, sizeof(ts), "%H:%M:%S", tm);
    
    printf("%s[%s]%s %s%-6s%s %s\n", DIM, ts, RESET, color, prefix, RESET, buf);
    fflush(stdout);
}

#define LOG_OK(fmt, ...)   log_msg(GREEN, "✓", fmt, ##__VA_ARGS__)
#define LOG_BID(fmt, ...)  log_msg(YELLOW, "BID", fmt, ##__VA_ARGS__)
#define LOG_ERR(fmt, ...)  log_msg(RED, "✗", fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) log_msg(CYAN, "→", fmt, ##__VA_ARGS__)



static void start_new_auction(void) {
    pthread_mutex_lock(&g_auction.lock);

    int idx = g_auction.round % CATALOG_SIZE;
    const Item *item = &CATALOG[idx];

    strncpy(g_auction.item, item->item, sizeof(g_auction.item) - 1);
    g_auction.start_price   = item->start_price;
    g_auction.current_price = item->start_price;
    g_auction.best_bidder[0] = '\0';
    g_auction.time_left     = AUCTION_DURATION;
    g_auction.open          = 1;
    g_auction.round++;

    int round = g_auction.round;
    char item_copy[64];
    strcpy(item_copy, g_auction.item);
    int price = g_auction.start_price;

    pthread_mutex_unlock(&g_auction.lock);

    printf("\n");
    LOG_OK("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    LOG_OK("Enchère #%d : %s (départ %d€)", round, item_copy, price);
    LOG_OK("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    broadcast("NEW_AUCTION %s|%d", item_copy, price);
}

static void close_auction(void) {
    pthread_mutex_lock(&g_auction.lock);
    g_auction.open = 0;

    char winner[MAX_NAME];
    int  final_price = g_auction.current_price;
    int  has_winner  = (g_auction.best_bidder[0] != '\0');
    strcpy(winner, has_winner ? g_auction.best_bidder : "NONE");

    pthread_mutex_unlock(&g_auction.lock);

    printf("\n");
    if (has_winner) {
        LOG_OK("🏆 GAGNANT : %s avec %d€", winner, final_price);
        broadcast("END %s|%d", winner, final_price);
    } else {
        LOG_INFO("Aucune offre, enchère annulée");
        broadcast("END NONE|0");
    }
    printf("\n");
}


static void *timer_thread(void *arg) {
    (void)arg;
    int n;

    while (1) {
        pthread_mutex_lock(&g_clients_lock);
        n = g_client_count;
        pthread_mutex_unlock(&g_clients_lock);

        if (n > 0) break;
        sleep(1);
    }

    start_new_auction();

    while (1) {
        sleep(1);

        pthread_mutex_lock(&g_auction.lock);
        int open = g_auction.open;

        if (open && g_auction.time_left > 0) {
            g_auction.time_left--;
        }

        int time_left = g_auction.time_left;
        pthread_mutex_unlock(&g_auction.lock);

        if (!open) continue;

        
if (time_left % 10 == 0 || time_left <= 5) {
    broadcast("TIME %d", time_left);
}

        if (time_left == 0) {
            close_auction();
            sleep(8);

            pthread_mutex_lock(&g_clients_lock);
            n = g_client_count;
            pthread_mutex_unlock(&g_clients_lock);

            if (n > 0) start_new_auction();
        }
    }
    return NULL;
}


typedef struct { int fd; int slot; } ClientArgs;

static void *client_thread(void *arg) {
    ClientArgs *ca = (ClientArgs *)arg;
    int fd = ca->fd, slot = ca->slot;
    free(ca);

    char buf[MSG_SIZE * 2];
    char line[MSG_SIZE];
    int  buf_len = 0;
    int  named = 0;


    pthread_mutex_lock(&g_auction.lock);
    if (g_auction.open) {
        send_msg(fd, "NEW_AUCTION %s|%d", g_auction.item, g_auction.current_price);
        send_msg(fd, "TIME %d", g_auction.time_left);
    }
    pthread_mutex_unlock(&g_auction.lock);

 
    while (1) {
        int n = recv(fd, buf + buf_len, sizeof(buf) - buf_len - 1, 0);
        if (n <= 0) break;

        buf_len += n;
        buf[buf_len] = '\0';

        char *start = buf;
        char *nl;
        while ((nl = strchr(start, '\n')) != NULL) {
            int len = (int)(nl - start);
            if (len > 0 && start[len-1] == '\r') len--;
            strncpy(line, start, len);
            line[len] = '\0';
            start = nl + 1;

            if (strlen(line) == 0) continue;

            /* Parser : "JOIN Alice" ou "BID 150" */
            char cmd[32], arg[128];
            cmd[0] = arg[0] = '\0';
            sscanf(line, "%31s %127s", cmd, arg);

          
            if (strcmp(cmd, "JOIN") == 0) {
                arg[MAX_NAME - 1] = '\0';
                for (int i = 0; arg[i]; i++)
                    if (arg[i] == '_') arg[i] = ' ';

                pthread_mutex_lock(&g_clients_lock);
                strncpy(g_clients[slot].name, arg, MAX_NAME);
                pthread_mutex_unlock(&g_clients_lock);

                named = 1;
                LOG_OK("Joueur « %s » connecté", arg);
                send_msg(fd, "WELCOME %d", g_clients[slot].id);
                broadcast("PLAYERS %d", g_client_count);
                continue;
            }

         
            if (strcmp(cmd, "BID") == 0) {
                if (!named) {
                    send_msg(fd, "REJECT Vous_devez_envoyer_JOIN");
                    continue;
                }

                int amount = atoi(arg);
                char bidder[MAX_NAME];

                pthread_mutex_lock(&g_clients_lock);
                strcpy(bidder, g_clients[slot].name);
                pthread_mutex_unlock(&g_clients_lock);


                pthread_mutex_lock(&g_auction.lock);

                if (!g_auction.open) {
                    pthread_mutex_unlock(&g_auction.lock);
                    send_msg(fd, "REJECT Enchere_fermee");
                    continue;
                }

                if (amount <= g_auction.current_price) {
                    int cur = g_auction.current_price;
                    pthread_mutex_unlock(&g_auction.lock);
                    send_msg(fd, "REJECT Offre_trop_basse|%d", cur);
                    LOG_INFO("  ✗ %s propose %d€ (actuel %d€)", bidder, amount, cur);
                    continue;
                }

            
                int old_price = g_auction.current_price;
                g_auction.current_price = amount;
                strcpy(g_auction.best_bidder, bidder);

                if (g_auction.time_left <= 5) {
                    g_auction.time_left += 5;
                }
                int time_left = g_auction.time_left;

                pthread_mutex_unlock(&g_auction.lock);
           

                LOG_BID("%s : %d€ (était %d€)  [%ds]", bidder, amount, old_price, time_left);
                broadcast("NEW_BID %s|%d|%d", bidder, amount, time_left);
                continue;
            }
        }

        buf_len = (int)strlen(start);
        if (buf_len > 0) memmove(buf, start, buf_len);
        else buf_len = 0;
    }


    pthread_mutex_lock(&g_clients_lock);
    LOG_INFO("Client « %s » déconnecté", named ? g_clients[slot].name : "?");
    g_clients[slot].active = 0;
    g_client_count--;
    int remaining = g_client_count;
    pthread_mutex_unlock(&g_clients_lock);

    close(fd);
    broadcast("PLAYERS %d", remaining);
    return NULL;
}


int main(void) {
    signal(SIGPIPE, SIG_IGN);

    memset(g_clients, 0, sizeof(g_clients));
    memset(&g_auction, 0, sizeof(g_auction));
    pthread_mutex_init(&g_auction.lock, NULL);

    g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_fd < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(g_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(PORT),
        .sin_addr.s_addr = INADDR_ANY,
    };

    if (bind(g_server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); exit(1);
    }
    if (listen(g_server_fd, 16) < 0) { perror("listen"); exit(1); }

    /* Header */
    printf("\n");
    printf("%s╔════════════════════════════════════════════════════════╗%s\n", BOLD YELLOW, RESET);
    printf("%s║         🔨  SERVEUR D'ENCHÈRES EN TEMPS RÉEL         ║%s\n", BOLD YELLOW, RESET);
    printf("%s╠════════════════════════════════════════════════════════╣%s\n", BOLD YELLOW, RESET);
    printf("%s║  Port        : %d%-42s║%s\n", BOLD YELLOW, PORT, "", RESET);
    printf("%s║  Max clients : %d%-42s║%s\n", BOLD YELLOW, MAX_CLIENTS, "", RESET);
    printf("%s║  Durée       : %d secondes%-37s║%s\n", BOLD YELLOW, AUCTION_DURATION, "", RESET);
    printf("%s╚════════════════════════════════════════════════════════╝%s\n\n", BOLD YELLOW, RESET);

    /* Thread timer */
    pthread_t timer_tid;
    pthread_create(&timer_tid, NULL, timer_thread, NULL);
    pthread_detach(timer_tid);
    LOG_OK("Thread timer démarré");
    LOG_INFO("En attente de connexions...\n");

  
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = accept(g_server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));

        pthread_mutex_lock(&g_clients_lock);
        int slot = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!g_clients[i].active) { slot = i; break; }
        }

        if (slot == -1) {
            pthread_mutex_unlock(&g_clients_lock);
            LOG_ERR("Salle pleine ! Connexion refusée");
            send_msg(client_fd, "REJECT Salle_pleine");
            close(client_fd);
            continue;
        }

        g_clients[slot].fd     = client_fd;
        g_clients[slot].id     = g_next_id++;
        g_clients[slot].active = 1;
        g_clients[slot].name[0] = '\0';
        g_client_count++;

        pthread_mutex_unlock(&g_clients_lock);

        LOG_INFO("Connexion depuis %s [slot %d]", ip, slot);

        ClientArgs *ca = malloc(sizeof(ClientArgs));
        ca->fd   = client_fd;
        ca->slot = slot;
        pthread_create(&g_clients[slot].thread, NULL, client_thread, ca);
        pthread_detach(g_clients[slot].thread);
    }

    close(g_server_fd);
    pthread_mutex_destroy(&g_auction.lock);
    return 0;
}
