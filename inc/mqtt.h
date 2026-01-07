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





#endif // MQTT_H