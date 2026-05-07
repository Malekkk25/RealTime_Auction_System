#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <termios.h>
#include <time.h>
#include "auction.h"


#define RESET       "\033[0m"
#define BOLD        "\033[1m"
#define DIM         "\033[2m"
#define RED         "\033[31m"
#define GREEN       "\033[32m"
#define YELLOW      "\033[33m"
#define BLUE        "\033[34m"
#define MAGENTA     "\033[35m"
#define CYAN        "\033[36m"
#define WHITE       "\033[37m"
#define CLEAR_LINE  "\033[2K\r"
#define CLEAR_SCREEN "\033[2J\033[H"


static int    g_fd        = -1;
static char   g_name[MAX_NAME];
static int    g_running   = 1;
static int    g_my_id     = 0;


static struct {
    char      item[64];
    int       price;
    char      leader[MAX_NAME];
    int       time_left;
    int       open;
    int       players;
    pthread_mutex_t lock;
} g_state = {
    .item      = "En attente...",
    .price     = 0,
    .leader    = "—",
    .time_left = 0,
    .open      = 0,
    .players   = 0,
};



static void clear_screen(void) {
    printf(CLEAR_SCREEN);
    fflush(stdout);
}

static void draw_header(void) {
    printf(BOLD YELLOW);
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║         🔨  ENCHÈRES EN DIRECT - TERMINAL             ║\n");
    printf("║════════════════════════════════════════════════════════║\n");
    printf(RESET);
    
    printf(CYAN "  Votre pseudo    : " RESET BOLD "%s\n" RESET, g_name);
    printf(CYAN "  En ligne        : " RESET BOLD "%d joueur(s)\n" RESET, g_state.players);
    printf("\n");
}

static void draw_auction(void) {
    pthread_mutex_lock(&g_state.lock);
    
    if (!g_state.open) {
        pthread_mutex_unlock(&g_state.lock);
        printf(DIM "  ⏸️  Enchère terminée. Prochaine bientôt...\n" RESET);
        return;
    }

    char item[64];
    int price = g_state.price;
    char leader[MAX_NAME];
    int time_left = g_state.time_left;
    strcpy(item, g_state.item);
    strcpy(leader, g_state.leader);
    
    pthread_mutex_unlock(&g_state.lock);


    printf(BOLD "  🏷️  Objet" RESET);
    printf(DIM " .................... " RESET YELLOW BOLD "%s\n" RESET, item);


    printf(BOLD "  💰 Prix actuel" RESET);
    printf(DIM " ............... " RESET YELLOW BOLD "%d€\n" RESET, price);

  
    printf(BOLD "  👑 Meilleure offre" RESET);
    printf(DIM " ................. " RESET);
    if (strcmp(leader, "—") == 0) {
        printf(DIM "aucune\n" RESET);
    } else if (strcmp(leader, g_name) == 0) {
        printf(GREEN BOLD "C'EST VOUS ! 🎉\n" RESET);
    } else {
        printf(MAGENTA "%s\n" RESET, leader);
    }
    printf("\n");

    /* Barre de temps */
    int total = AUCTION_DURATION;
    int filled = (time_left * 30) / (total > 0 ? total : 1);
    if (filled < 0) filled = 0;
    if (filled > 30) filled = 30;

    const char *color = (time_left <= 10) ? RED : (time_left <= 20) ? YELLOW : GREEN;

    printf(BOLD "  ⏱️  Temps" RESET);
    printf(DIM " ........................ " RESET "%s%d s" RESET "\n", color, time_left);
    printf("  [");
    for (int i = 0; i < 30; i++) {
        if (i < filled) printf("%s█" RESET, color);
        else            printf(DIM "░" RESET);
    }
    printf(DIM "]\n\n" RESET);
}

static void draw_input_area(void) {
    printf(BOLD "╔════════════════════════════════════════════════════════╗\n" RESET);
    printf(BOLD "║" RESET DIM " Commandes : " RESET CYAN "bid <montant>" RESET
           DIM "  |  " RESET CYAN "quit" RESET DIM " " RESET BOLD "║\n" RESET);
    printf(BOLD "╚════════════════════════════════════════════════════════╝\n\n" RESET);
    printf(BOLD BLUE "$ " RESET);
    fflush(stdout);
}

static void refresh_display(void) {
    clear_screen();
    draw_header();
    draw_auction();
    draw_input_area();
}

static void notify(const char *color, const char *msg) {
    printf(CLEAR_LINE);
    printf("%s  ► %s%s\n", color, msg, RESET);
    printf(BOLD BLUE "$ " RESET);
    fflush(stdout);
}

static void notify_fmt(const char *color, const char *fmt, ...) {
    char msg[MSG_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    notify(color, msg);
}



static void handle_msg(const char *line) {
    char cmd[32];
    char *args = NULL;

    
    const char *space = strchr(line, ' ');
    if (space) {
        int cmd_len = (int)(space - line);
        if (cmd_len >= 32) cmd_len = 31;
        strncpy(cmd, line, cmd_len);
        cmd[cmd_len] = '\0';
        args = (char*)space + 1; // cast nécessaire car space est const
    } else {
        strncpy(cmd, line, 31);
        cmd[31] = '\0';
        args = ""; 
    }


    if (strcmp(cmd, "WELCOME") == 0) {
        g_my_id = atoi(args);
        notify(GREEN, "✅ Connecté au serveur !");
        return;
    }


    if (strcmp(cmd, "NEW_AUCTION") == 0) {
        char *pipe = strchr(args, '|'); 
        if (pipe) {
            *pipe = '\0';
            pthread_mutex_lock(&g_state.lock);
            strncpy(g_state.item, args, 63); 
            g_state.price = atoi(pipe + 1);
            strcpy(g_state.leader, "—");
            g_state.open = 1;
            g_state.time_left = AUCTION_DURATION;
            pthread_mutex_unlock(&g_state.lock);
        }
        refresh_display();
        notify_fmt(YELLOW, "🚀 Nouvelle enchère : %s (départ %d€)", args, atoi(pipe+1));
        return;
    }

  
    if (strcmp(cmd, "NEW_BID") == 0) {
        char name[MAX_NAME] = {0};
        int amount = 0, t = 0;
        
        char *p1 = strchr(args, '|');
        if (p1) {
            int nlen = (int)(p1 - args);
            if (nlen >= MAX_NAME) nlen = MAX_NAME - 1;
            strncpy(name, args, nlen);
            
            char *p2 = strchr(p1 + 1, '|');
            if (p2) {
                amount = atoi(p1 + 1);
                t = atoi(p2 + 1);
            }
        }

        pthread_mutex_lock(&g_state.lock);
        strcpy(g_state.leader, name);
        g_state.price = amount;
        g_state.time_left = t;
        pthread_mutex_unlock(&g_state.lock);

        if (strcmp(name, g_name) == 0) {
            notify_fmt(GREEN, "✅ Votre offre de %d€ est acceptée ! Vous menez.", amount);
        } else {
            notify_fmt(MAGENTA, "📢 %s propose %d€  (⏱ %ds)", name, amount, t);
        }
        return;
    }

 
    if (strcmp(cmd, "TIME") == 0) {
        int t = atoi(args);
        pthread_mutex_lock(&g_state.lock);
        g_state.time_left = t;
        pthread_mutex_unlock(&g_state.lock);
        
        if (t % 5 == 0 || t <= 10) refresh_display();
        if (t == 10) notify_fmt(YELLOW, "⚡ 10 secondes restantes !");
        if (t == 5)  notify_fmt(RED, "🔥 5 SECONDES !");
        return;
    }


    if (strcmp(cmd, "REJECT") == 0) {
        char reason[128];
        strcpy(reason, args);
        
        char *pipe = strchr(reason, '|');
        if (pipe) {
            *pipe = '\0';
            notify_fmt(RED, "❌ Offre refusée : %s (prix actuel: %s€)", reason, pipe+1);
        } else {
            notify_fmt(RED, "❌ Offre refusée : %s", reason);
        }
        return;
    }

    
    if (strcmp(cmd, "BONUS") == 0) {
        int extra = atoi(args);
        notify_fmt(CYAN, "⚡ Bonus ! +%d secondes ajoutées", extra);
        return;
    }

  
    if (strcmp(cmd, "END") == 0) {
        pthread_mutex_lock(&g_state.lock);
        g_state.open = 0;
        pthread_mutex_unlock(&g_state.lock);
        
        char winner[MAX_NAME] = {0};
        int price = 0;
        
        char *pipe = strchr(args, '|');
        if (pipe) {
            int nlen = (int)(pipe - args);
            if (nlen >= MAX_NAME) nlen = MAX_NAME - 1;
            strncpy(winner, args, nlen);
            price = atoi(pipe + 1);
        }

        refresh_display();
        
        if (strcmp(winner, "NONE") == 0) {
            notify(DIM, "🔔 Enchère terminée — Aucune offre.");
        } else if (strcmp(winner, g_name) == 0) {
            notify_fmt(GREEN BOLD, "🏆 FÉLICITATIONS ! Vous avez remporté pour %d€ !", price);
        } else {
            notify_fmt(YELLOW, "🏆 Gagnant : %s avec %d€", winner, price);
        }
        return;
    }

 
    if (strcmp(cmd, "PLAYERS") == 0) {
        int n = atoi(args);
        pthread_mutex_lock(&g_state.lock);
        g_state.players = n;
        pthread_mutex_unlock(&g_state.lock);
        return;
    }
}



static void *recv_thread(void *arg) {
    (void)arg;
    char buf[MSG_SIZE * 2];
    int buf_len = 0;

    while (g_running) {
        int n = recv(g_fd, buf + buf_len, sizeof(buf) - buf_len - 1, 0);
        if (n <= 0) {
            if (g_running) {
                notify(RED, "❌ Connexion au serveur perdue.");
                g_running = 0;
            }
            break;
        }

        buf_len += n;
        buf[buf_len] = '\0';

        char *start = buf;
        char *nl;
        while ((nl = strchr(start, '\n')) != NULL) {
            int len = (int)(nl - start);
            if (len > 0 && start[len-1] == '\r') len--;
            
            char line[MSG_SIZE];
            strncpy(line, start, len < MSG_SIZE ? len : MSG_SIZE - 1);
            line[len < MSG_SIZE ? len : MSG_SIZE - 1] = '\0';
            start = nl + 1;
            
            if (strlen(line) > 0) {
                handle_msg(line);
            }
        }

        buf_len = (int)strlen(start);
        if (buf_len > 0) memmove(buf, start, buf_len);
        else buf_len = 0;
    }
    return NULL;
}



void send_msg(int fd, const char *fmt, ...) {
    char buf[MSG_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf) - 2, fmt, args);
    va_end(args);
    strcat(buf, "\n");
    send(fd, buf, strlen(buf), MSG_NOSIGNAL);
}

int main(int argc, char *argv[]) {
    pthread_mutex_init(&g_state.lock, NULL);

    const char *host = (argc >= 2) ? argv[1] : "127.0.0.1";
    int port = (argc >= 3) ? atoi(argv[2]) : PORT;

    clear_screen();
    printf(BOLD YELLOW "\n  🔨 Client Enchères\n" RESET);
    printf(CYAN "  Connexion à %s:%d\n\n" RESET, host, port);


    printf("  Votre pseudo : ");
    fflush(stdout);
    if (!fgets(g_name, sizeof(g_name), stdin)) return 1;
    g_name[strcspn(g_name, "\n")] = '\0';
    if (strlen(g_name) == 0) strcpy(g_name, "Joueur");

  
    g_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_fd < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(port),
    };
    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        fprintf(stderr, "Adresse invalide : %s\n", host);
        return 1;
    }

    printf("  Connexion en cours...\n");
    if (connect(g_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "  ❌ Impossible de se connecter\n");
        return 1;
    }


    char safe_name[MAX_NAME];
    strcpy(safe_name, g_name);
    for (int i = 0; safe_name[i]; i++)
        if (safe_name[i] == ' ') safe_name[i] = '_';
    send_msg(g_fd, "JOIN %s", safe_name);


    pthread_t rtid;
    pthread_create(&rtid, NULL, recv_thread, NULL);
    pthread_detach(rtid);

    sleep(1);
    refresh_display();

 
    char input[128];
    while (g_running) {
        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\n")] = '\0';

        if (strlen(input) == 0) { printf(BOLD BLUE "$ " RESET); fflush(stdout); continue; }

        if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0) break;

    
        if (strncmp(input, "bid ", 4) == 0) {
            int amount = atoi(input + 4);
            if (amount <= 0) {
                notify(RED, "❌ Montant invalide. Utilisez : bid 150");
            } else if (!g_state.open) {
                notify(RED, "❌ Aucune enchère en cours.");
            } else {
                send_msg(g_fd, "BID %d", amount);
                printf(BOLD BLUE "$ " RESET);
                fflush(stdout);
            }
            continue;
        }

  
        char *end;
        long val = strtol(input, &end, 10);
        if (*end == '\0' && val > 0) {
            if (!g_state.open) {
                notify(RED, "❌ Aucune enchère en cours.");
            } else {
                send_msg(g_fd, "BID %ld", val);
                printf(BOLD BLUE "$ " RESET);
                fflush(stdout);
            }
            continue;
        }

        notify(RED, "❌ Commande inconnue. Utilisez : bid <montant> | quit");
    }

    g_running = 0;
    close(g_fd);
    pthread_mutex_destroy(&g_state.lock);
    
    printf(RESET "\n  À bientôt !\n\n");
    return 0;
}
