#ifndef MQTT_H
#define MQTT_H

#include <stdio.h>

#define MQTT_HEADER_LEN 2
#define MQTT_ACK_LEN    4

// gotowe wartosci pierwszego bajtu nagłowka MQTT(Fixed Header) dla odp brokera
#define CONNECT_ACK_BYTE        0x20 // 0010 0000 - Odpowiedź brokera na pakiet CONNECT od klienta.
#define PUBLISH_BYTE            0x30 // 0011 0000 - Payload
#define PUBLISH_ACK_BYTE        0x40 // 0100 0000 - Broker wysyła po poprawnym przetworzeniu wiadomości
#define PUBLISH_RECEIVED_BYTE   0x50 // 0101 0000 - Pierwszy etap potwierdzenia dla QoS 2, Po odebraniu PUBLISH, Qos 2 to DOSTARCZENIE DOKLADNIE JEDEN RAZ
#define PUBLISH_RELEASE_BYTE    0x60 // 0110 0000 - Drugi etap QoS 2 – potwierdzenie gotowości zwolnienia wiadomości
#define PUBLISH_COMPLETE_BYTE   0x70 // 0111 0000 - Ostateczne zakończenie transmisji QoS 2, Obie strony mogą usunąć wiadomość z pamięci
#define SUBSCRIBE_ACK_BYTE      0x90 // 1001 0000 - Odpowiedź brokera na SUBSCRIBE, zawartosc to: packetID, przyznany QoS lub bład subskrypcji
#define UNSUBSCRIBE_BYTE        0xB0 // 1011 0000 - Potwierdzenie usunięcia subskrypcji, Po poprawnym przetworzeniu UNSUBSCRIBE
#define PING_RESPONSE_BYTE      0xD0 // 1100 0000 - Odpowiedź brokera na PING_REQUEST, w celu: utrzymania polaczenia TCP, wykrycie zerwanego klienta
/*
Pierwszy bajt nagłówka MQTT wygląda tak:
    bits 7–4 → typ pakietu
    bits 3–0 → flagi
    A wiec flagi jakie chcemy ustawiamy potem, pamietac ze w PUBLISH_RELEASE_BYTE nalezy miec 0x62= 0110 0010
*/

enum packet_type 
{
    CONNECT          = 1,  // Klient → broker, inicjalizacja połączenia (ID, keepalive, autoryzacja)
    CONNECT_ACK      = 2,  // Broker → klient, potwierdzenie lub odrzucenie połączenia
    PUBLISH          = 3,  // Przesyłanie danych (payload) na topic, QoS 0/1/2
    PUBLISH_ACK      = 4,  // Potwierdzenie PUBLISH dla QoS 1 (dostarczenie co najmniej raz)
    PUBLISH_RECEIVE  = 5,  // Pierwszy etap potwierdzenia QoS 2, po odebraniu PUBLISH
    PUBLISH_RELEASE  = 6,  // Drugi etap QoS 2, zwolnienie wiadomości
    PUBLISH_COMPLETE = 7,  // Ostateczne potwierdzenie QoS 2, dostarczenie dokładnie jeden raz
    SUBSCRIBE        = 8,  // Klient → broker, żądanie subskrypcji topiców
    SUBSCRIBE_ACK    = 9,  // Broker → klient, potwierdzenie subskrypcji
    UNSUBSCRIBE      = 10, // Klient → broker, usunięcie subskrypcji
    UNSUBSCRIBE_ACK  = 11, // Broker → klient, potwierdzenie usunięcia subskrypcji
    PING_REQUEST     = 12, // Klient → broker, sprawdzenie czy połączenie jest aktywne
    PING_RESPONSE    = 13, // Broker → klient, odpowiedź keepalive
    DISCONNECT       = 14  // Klient → broker, kontrolowane zakończenie połączenia
};

enum qos_level // Używane przy obsłudze PUBLISH
{
    AT_MOST_ONCE,  // (QoS 0) – bez potwierdzeń, możliwa utrata
    AT_LEAST_ONCE, // (QoS 1) – z potwierdzeniem PUBACK 
    EXACTLY_ONCE   // (QoS 2) – sekwencja PUBREC -> PUBREL-> PUBCOMP
};

union mqtt_header // Reprezentuje pierwszy bajt nagłówka MQTT (Fixed Header).
{
    unsigned char byte; // caly naglowek jako jeden BAJT
    struct              // pozwala zmodyfikowac poszczegolne elementy pierwszego bajta naglowka
    {
        unsigned retain : 1; // czy ostatnia wiadomość ma być zapamiętana dla nowych subskrybentów
        unsigned qos : 2;    // poziom QoS (0, 1, 2)
        unsigned dup : 1;    // flaga ponownej transmisji
        unsigned type : 4;   // typ pakietu (CONNECT, PUBLISH, itd.)
    } bits; // potrzebne jak np. bedziemy chciec sprawdzic konkretna jedna wartosc tego BAJTU headera, zamiast masek bitowych, fair enough :) 
};


// Pierwszy pakiet ktory musi zostac wyslany gdy nowy klient estabilish connection (estabilish to za dobre slowo zeby je tlumaczyc na polski :P)
// To musi byc wyslane raz i tylko raz, wiecej powoduje blad !!!
// dla kazdego CONNECT musimy otrzymac CONNECT_ACK
struct mqtt_connect 
{
    union mqtt_header header; // to co jest wyzej zdefiniowane
    union {
        unsigned char byte; // taka sama sytuacja jak wczesniejsza unia, jeden byte lub struct z bitefields
        int reserved : 1;           // zawsze 0 (wymóg protokołu)
        unsigned clean_session : 1; // czy broker ma czyścić poprzednią sesję
        unsigned will : 1;          // czy klient definiuje Last Will
        unsigned will_qos : 2;      // QoS wiadomości Last Will
        unsigned will_retain : 1;   // czy Last Will ma być retain, tzn ze kazdy nowy subskrybent tego topicu od razu ją dostanie (wiadomosc LastWill)
        unsigned password : 1;      // czy pole hasła jest obecne
        unsigned username : 1;      // czy pole użytkownika jest obecne
    } bits;

    struct // dane do konfiguracji sesji
    {
        unsigned short keepalive;   // maksymalny czas bez komunikacji
        unsigned char *client_id;   // unikalny identyfikator klienta
        unsigned char *username;    // dane uwierzytelniania 
        unsigned char *password;
        unsigned char *will_topic;  // Last Will Message - to wiadomość, którą broker wysyła automatycznie, gdy klient rozłączy się nieprawidłowo
        unsigned char *will_message;
    } payload; 
}; 

struct mqtt_connect_ack
{
    union mqtt_header header;
    union{
        unsigned char byte;
        struct 
        {
            unsigned session_present : 1; // czy istnieje poprzednia sesja klienta
            unsigned reserved : 7;        // zawsze 0
        } bits;
    };

    unsigned char return_code; // 0-polaczenie zaakceptowane, <1-5> - rozne bledy
};

struct mqtt_subscribe {
    union mqtt_header header;
    unsigned short pkt_id;          // Packet Identifier (wymagany dla SUBSCRIBE).
    unsigned short tuples_len;      // liczba wpisów (topiców) w żądaniu subskrypcji.
    struct {                        
        unsigned short topic_len;   // długość nazwy topicu,
        unsigned char *topic;       // topic jako string/bufor,
        unsigned qos;               // żądany QoS dla tego topicu.
    } *tuples;
};

struct mqtt_unsubscribe {
    union mqtt_header header;
    unsigned short pkt_id;
    unsigned short tuples_len;
    struct {
        unsigned short topic_len;
        unsigned char *topic;
    } *tuples;
};

struct mqtt_suback {
    union mqtt_header header;
    unsigned short pkt_id;      // musi pasować do pkt_id z SUBSCRIBE.
    unsigned short rcslen;      // liczba kodów zwrotnych (po jednym na każdy topic).
    unsigned char *rcs;         // zwykle „przyznany QoS” albo informacja o błędzie dla danego topicu.
};

struct mqtt_publish {
    union mqtt_header header;
    unsigned short pkt_id;
    unsigned short topiclen;
    unsigned char *topic;       // topic wiadomosci
    unsigned short payloadlen;  
    unsigned char *payload;     // wlasciwa wiadomosc
};

struct mqtt_ack {               // Uniwersalny pakiet potwierdzeń oparty o sam pkt_id 
    union mqtt_header header;   
    unsigned short pkt_id;
};

// remaining ACK packets
typedef struct mqtt_ack mqtt_puback;
typedef struct mqtt_ack mqtt_pubrec;
typedef struct mqtt_ack mqtt_pubrel;
typedef struct mqtt_ack mqtt_pubcomp;
typedef struct mqtt_ack mqtt_unsuback;
typedef union mqtt_header mqtt_pingreq;
typedef union mqtt_header mqtt_pingresp;
typedef union mqtt_header mqtt_disconnect;

// FINALLY, definiujemy generyczny MQTT packet jako unia 
union mqtt_packet
{
    struct mqtt_ack ack;
    union mqtt_header header;
    struct mqtt_connect connect;
    struct mqtt_connect_ack connect_ack;
    struct mqtt_suback suback;
    struct mqtt_publish publish;
    struct mqtt_subscribe subscribe;
    struct mqtt_unsubscribe unsubscribe;
};

// deklaracje funkcji z mqtt.c ktore beda w przyszlosci
/* 
int mqtt_encode_length(unsigned char *, size_t);
unsigned long long mqtt_decode_length(const unsigned char **);
int unpack_mqtt_packet(const unsigned char *, union mqtt_packet *);
unsigned char *pack_mqtt_packet(const union mqtt_packet *, unsigned);

union mqtt_header *mqtt_packet_header(unsigned char);
struct mqtt_ack *mqtt_packet_ack(unsigned char , unsigned short);
struct mqtt_connack *mqtt_packet_connack(unsigned char, unsigned char, unsigned char);
struct mqtt_suback *mqtt_packet_suback(unsigned char, unsigned short,
                                       unsigned char *, unsigned short);
struct mqtt_publish *mqtt_packet_publish(unsigned char, unsigned short, size_t,
                                         unsigned char *, size_t, unsigned char *);
void mqtt_packet_release(union mqtt_packet *, unsigned);


*/

#endif // MQTT_H