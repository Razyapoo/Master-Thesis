#define DEBUG false
#include <SPI.h>
#include <DW1000.h>

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiServer.h>
#include <WiFiClient.h>

#include "debug.h"

#define PIN_IRQ 34
#define PIN_RST 27
#define PIN_SS 4

// Macros in order to avoid hard coding
// #define STATE_IDLE 0
// #define STATE_SCAN 1
// #define STATE_DISCOVERY 2
// #define STATE_RANGING 3
// #define STATE_POLL 4
// #define STATE_POLLACK 5
// #define STATE_RANGE 6
// #define STATE_RANGEREPORT 7
// #define STATE_RANGING_INIT 8
// #define STATE_SEND_REQUEST_TO_SERVER 9
// #define STATE_REQUEST_ACK_FROM_SERVER 10
// #define STATE_SEND_DISTANCE_TO_SERVER 11

// #define BLINK 0
// #define RANGINGINIT 1
// #define POLL 2
// #define POLLACK 3
// #define RANGE 4
// #define RANGEREPORT 5

const byte MSG_TYPE_BLINK = 1;
const byte MSG_TYPE_ANCHOR_ADDR = 2;
const byte MSG_TYPE_POLL = 3;
const byte MSG_TYPE_POLL_ACK = 4;
const byte MSG_TYPE_RANGE = 5;
const byte MSG_TYPE_RANGE_REPORT = 6;

const size_t MAX_ANCHORS = 2;
const size_t MIN_ANCHORS = 2;
byte discoveredAnchors[MAX_ANCHORS] = {0};
size_t discoveredAnchorsCount = 0;
size_t anchorIndex = 0;
byte currentAnchorAddress, anchorAddress;
// const byte broadcastAddress = 255;
bool isTagBusy = false;

#define setSenderAddr(frame, addr) memcpy(frame + 1, &addr, 2)
#define setReceiverAddr(frame, addr) memcpy(frame + 3, &addr, 2)
#define getSenderAddr(frame, addr) memcpy(&addr, frame + 1, 2)
#define isSenderMatch(frame, addr) !memcmp(frame + 1, &addr, 2)
#define isReceiverMatch(frame, addr) !memcmp(frame + 3, &addr, 2)

// type (1), src (2), dst (2), time1 (5), time2 (5), time3 (5): total (20)
#define FRAME_SIZE 20

// Parameters are tunable, but anchors and tags must have the same values
/* Close setting */
// #define DISCOVERY_TIMEOUT           100
// #define POLLACK_TIMEOUT             10
// #define RANGE_TIMEOUT               10
// #define RANGEREPORT_TIMEOUT         10
// #define RANGING_INIT_TIMEOUT        10
// #define REPLY_DELAY                 3
// #define DEFAULT_RESET_TIMEOUT       500
// #define SERVER_TIMEOUT_MS           20
// #define SLEEP                       100//300

/* distance setting */
#define DISCOVERY_TIMEOUT 70
#define POLLACK_TIMEOUT 10
#define RANGE_TIMEOUT 10
#define RANGEREPORT_TIMEOUT 10
#define RANGING_INIT_TIMEOUT 10
// #define REPLY_DELAY                 10
#define DEFAULT_RESET_TIMEOUT 500
#define HARD_RESET_TIMEOUT 3000
#define SERVER_TIMEOUT_MS 20
#define SLEEP 100 // 300
#define BLINK_DELAY 80

/* WiFi settings */
WiFiClient client;
// const char *ssid = "ASUS";
// const char *password = "cuni3103&";
// const char *host = "192.168.1.55";
// const char *ssid = "iPhone Nyx";
// const char *password = "Nera1998&";
// const char *host = "172.20.10.3";
char ssid[] = "oskar-hotspot";
const char *password = "Nera1998";
const char *host = "10.42.0.1";

// UWB anchors and tags must have the same replay_delay
// DW1000Time rangeReplyDelay = DW1000Time(REPLY_DELAY, DW1000Time::MILLISECONDS);
DW1000Time rangeReplyDelay;
DW1000Time timePollSent;
DW1000Time timePollReceived;
DW1000Time timePollAckSent;
DW1000Time timePollAckReceived;
DW1000Time timeRangeSent;
DW1000Time timeRangeReceived;
// DW1000Time round1;
// DW1000Time reply1;
// DW1000Time round2;
// DW1000Time reply2;
// DW1000Time tof;

/* const */
const uint16_t networkId = 10;

// struct MyLink *uwb_data;
// int index_num = 0;
// String all_json = "";
// char ack[3] = { '\0' };
String ack = "";

// Current state of a UWB anchor state machine

// typedef struct {
//     uint16_t myID;
//     float distance;
// } Anchor;

uint8_t mac[6];

uint16_t num_anchors = 0;
uint16_t idx_anchor = 0;
uint16_t counterBlink = 0;
uint16_t anchors[MAX_ANCHORS];
uint16_t myID, anchorID; // range of tag ids: 0..99, anchor: 100..199
uint16_t replyDelay;
unsigned long lastSent, lastActivity, currentTime, lastStateChange, rangingInitDelay, runtimeDelay;
float distances[MAX_ANCHORS] = {0};

// byte state = STATE_IDLE;
byte message[FRAME_SIZE] = {0};
byte currentMessage[FRAME_SIZE] = {0};
byte receivedMessage[FRAME_SIZE] = {0};
byte expectedMessageType;
unsigned long blinkTimer, blinkCurrentMillis, discoveryTimeout, discoveryTimer;
// byte rxFrame[FRAME_SIZE];

// For rounds
// char msgToSend[150];
// char roundTimestamps[2][60];

char msgToSend[30];
String serverRequest;
bool toSend = false;
bool sentAck = false;
bool receivedAck = false;
bool debug = true;
// bool protocolFailed = false;
bool isRequestFromServerReceived = false;
bool isRequestToServerSent = false;