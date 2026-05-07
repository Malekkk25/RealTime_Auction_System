#ifndef AUCTION_H
#define AUCTION_H

#include<time.h>
#include<stdint.h>

#define PORT 5555
#define MAX_CLIENTS 32
#define MAX_NAME 32
#define MSG_SIZE        512
#define AUCTION_DURATION 150

#define MSG_JOIN        "JOIN"
#define MSG_BID         "BID"

#define MSG_WELCOME     "WELCOME"
#define MSG_NEW_AUCTION "NEW_AUCTION"
#define MSG_NEW_BID     "NEW_BID"
#define MSG_REJECT      "REJECT"
#define MSG_TIME        "TIME"
#define MSG_END         "END"
#define MSG_PLAYERS     "PLAYERS"

typedef struct {
    char     item[64];
    int      start_price;
} Item;


static const Item CATALOG[] = {
    { "Tableau peinture a l'huile (artiste local)", 500  },
    { "Vase antique en porcelaine",                1200 },
    { "Montre ancienne en or",                     2500 },
    { "Tapis persan fait main",                    1800 },
    { "Statue en bronze (sculpture)",              950  },
    { "Livre ancien edition rare",                 700  },
    { "Piece de monnaie historique",               400  },
    { "Bijou ancien en argent",                    600  },
    { "Horloge murale vintage",                    300  },
    { "Objet artisanal traditionnel",              200  }
};
#define CATALOG_SIZE 7
 
#endif
