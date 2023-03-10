#include "arduino.h"

void updateState(int nextState) {
  state = nextState;
  lastStateChange = millis();
}

void updateRangingPhase(boolean next) {
  if (next) idx_anchor++;
  updateState(STATE_RANGING);
}

void noteActivity() {
  lastActivity = millis();
}

void handleReceived() {
  receivedAck = true;
}

void handleSent() {
  sentAck = true;
}

void setAddress() {
    String macAddr = WiFi.macAddress();
    if (macAddr == "70:B8:F6:D8:F6:48") {
        myID = 1;
    } else if (macAddr == "70:B8:F6:D8:F6:24") {
        myID = 2;
    } else if (macAddr == "70:B8:F6:D8:F6:60") {
        myID = 3;
    } else {
        // PRINTLN(F("Wrong tag MAC address."));
    }
}

void connectToWiFi() {
   WiFi.mode(WIFI_MODE_STA);
   WiFi.setSleep(false);
   WiFi.begin(ssid, password);
   while (WiFi.status() != WL_CONNECTED)
   {
       delay(300);
       // PRINT(F("."));
   }
   // PRINTLN(F("Connected"));
  
}

void connectToServer() {
   
   if (client.connect(host, 30001))
   {
       // PRINTLN(F("Success"));
   } else {
       PRINTLN(F("Unsuccessfull connection"));
   }
}

void checkForReset() {
    if(!sentAck && !receivedAck) {
    if(currentTime - lastActivity > DEFAULT_RESET_TIMEOUT) {
      initReceiver();
      PRINTLN(F("Reinit...."));
    }
    return;
  }
}

boolean isAnchorAddress(uint16_t addr) {
    return (100 < addr && addr < 199) ? true : false;
}

void initReceiver() {
  DW1000.newReceive();
  DW1000.setDefaults();
  DW1000.receivePermanently(true);
  DW1000.startReceive();
  //delay(1000);
  noteActivity();
}

void prepareTx() {
  DW1000.newTransmit();
  DW1000.setDefaults();
}

void startTx() {
  DW1000.setData(txFrame, FRAME_SIZE);
  DW1000.startTransmit();
  // timeout will be asserted after tx interrupt
  lastSent = 0;
}

void transmitBlink() {
  prepareTx();
  txFrame[0] = BLINK;
  setSenderAddr(txFrame, myID);
  startTx();
}

void transmitPoll() {
  prepareTx();
  txFrame[0] = POLL;
  setSenderAddr(txFrame, myID);
  setReceiverAddr(txFrame, anchors[idx_anchor]);
  //for (auto i = 0; i < 20; i++) Serial.print(txFrame[i]);
  //Serial.println();
  startTx();
}

void transmitRange() {
  prepareTx();
  txFrame[0] = RANGE;
  setSenderAddr(txFrame, myID);
  setReceiverAddr(txFrame, anchors[idx_anchor]);
  DW1000.setDelay(rangeReplyDelay);
  startTx();
}

void computeRangeAsymmetric() {
    // asymmetric two-way ranging (more computation intense, less error prone)
    // See APS013: The Implementation of Two-Way Ranging with he DW1000
    // https://decawave.com/sites/default/files/aps013_dw1000_and_two_way_ranging_v2.2.pdf pp. 4
    //Serial.println(String("timePollSent: ") + String(timePollSent.getAsMicroSeconds()));
    //Serial.println(String("timePollReceived: ") + String(timePollReceived.getAsMicroSeconds()));
    //Serial.println(String("timePollAckSent: ") + String(timePollAckSent.getAsMicroSeconds()));
    //Serial.println(String("timePollAckReceived: ") + String(timePollAckReceived.getAsMicroSeconds()));
    //Serial.println(String("timeRangeSent: ") + String(timeRangeSent.getAsMicroSeconds()));
   // Serial.println(String("timeRangeReceived: ") + String(timeRangeReceived.getAsMicroSeconds()));
    DW1000Time round1 = (timePollAckReceived - timePollSent).wrap();
    //PRINT(F("round1: ")); PRINTLN(round1);
    DW1000Time reply1 = (timePollAckSent - timePollReceived).wrap();
    //PRINT(F("reply1: ")); PRINTLN(reply1);
    DW1000Time round2 = (timeRangeReceived - timePollAckSent).wrap();
    //PRINT(F("round2: ")); PRINTLN(round2);
    DW1000Time reply2 = (timeRangeSent - timePollAckReceived).wrap();
    //PRINT(F("reply2: ")); PRINTLN(reply2);
    DW1000Time tof = (round1 * round2 - reply1 * reply2)
                      / (round1 + round2 + reply1 + reply2);
    //PRINT(F("tof: ")); PRINTLN(tof);
    //PRINT(F("tof as meters: ")); PRINTLN(tof.getAsMeters());
    distances[idx_anchor] = tof.getAsMeters();
}

void setup() {
  
  Serial.begin(115200);

  DW1000.begin(PIN_IRQ, PIN_RST);
  DW1000.select(PIN_SS);

  setAddress();

  DW1000.newConfiguration();
  DW1000.setDefaults();
  DW1000.setNetworkId(networkId);
  DW1000.setDeviceAddress(myID);
  DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_ACCURACY);
  DW1000.commitConfiguration();

  DW1000.attachSentHandler(handleSent);
  DW1000.attachReceivedHandler(handleReceived);

  initReceiver();
  connectToWiFi();
  connectToServer();
  // uwb_data = init_link(myID);
}

void loop() {
  currentTime = millis();

     /* Send data to server */
  if (state == STATE_IDLE) {
    PRINTLN(F("Server request"));
    if (client.connected()) { 
      client.print(String(myID) + "3");
      updateState(STATE_REQUEST_ACK_FROM_SERVER);
    } else {
      PRINTLN(F("Server is not available."));
      connectToServer();
      runtimeDelay = millis();
      while (millis() - runtimeDelay < 10) continue;
      updateState(STATE_IDLE);
      return;
    }
  }

  if (state == STATE_REQUEST_ACK_FROM_SERVER) {
    ack = "";
    //delay(20);
    runtimeDelay = millis();
    while (client.connected()) {
      if (client.available()) { 
        ack = client.read();
        break;
      } else if (millis() - runtimeDelay > 10) break;
    }
    
    if (ack == "53") {
      PRINTLN(F("Server response - ACCEPTED. Start SCAN state"));
      updateState(STATE_SCAN);
    }
    else {
      if (WiFi.status() == WL_CONNECTION_LOST) {
        PRINTLN(F("WiFi connection lost. Reconnecting..."));
        ack = "";
        connectToWiFi();
      }
      else if (!client.connected()) {
        PRINTLN(F("Server does't respond. Connection lost. Reconnecting..."));
        connectToServer();
        ack = "";
      } 
      else {
        PRINT(F("Received wrong ACK : ")); PRINTLN(ack);
      }
      runtimeDelay = millis();
      while (millis() - runtimeDelay < 10) continue;
      updateState(STATE_IDLE);
      return;
    }
  }

  if (state == STATE_SEND_DISTANCE_TO_SERVER) {
    //delay(100);
    //make_link_json(uwb_data, &all_json);
    client.print(msgToSend);
    ack = "";
    // delay(20);
    runtimeDelay = millis();
    while (client.connected()) {
      if (client.available()) { 
        ack = client.read();
        break;
      } else if (millis() - runtimeDelay > 10) break;
    }
    if (ack == "55") {
      toSend = false;
      PRINTLN(F("  UWB package sent"));
      ack = "";
      //delay(200);
    } else if (WiFi.status() == WL_CONNECTION_LOST) {
        PRINTLN(F("WiFi connection lost. Reconnecting..."));
        ack = "";
        connectToWiFi();
        updateState(STATE_SEND_DISTANCE_TO_SERVER);
        return;
    }
    else if (!client.connected()) {
      PRINTLN(F("Server does't respond. Connection lost. Reconnecting..."));
      ack = "";
      connectToServer();
      updateState(STATE_SEND_DISTANCE_TO_SERVER);
      return;
    } 
    else {
      PRINT(F("Received wrong ACK : ")); PRINTLN(ack);
    }
    runtimeDelay = millis();
    while (millis() - runtimeDelay < 10) continue;
    updateState(STATE_IDLE);
    return;
  }

  if (state == STATE_SCAN) {
    PRINTLN(F("SCANNING..."));
    for (idx_anchor = 0; idx_anchor < 2; idx_anchor++) {
      //delete_link(uwb_data, anchors[idx_anchor]);
      anchors[idx_anchor] = 0;
      distances[idx_anchor] = 0;
    }
    idx_anchor = 0;
    num_anchors = 0;
    toSend = false;
    transmitBlink();
    updateState(STATE_DISCOVERY);
    return;
  }

  if (state == STATE_DISCOVERY
      && ((lastSent && currentTime - lastSent > DISCOVERY_TIMEOUT)
          || currentTime - lastStateChange > 2 * DISCOVERY_TIMEOUT)) {

    if (num_anchors > 0) {
      PRINT(num_anchors); PRINTLN(F(" are detected. Go to RANGING state"));
      idx_anchor = 0;
      updateState(STATE_RANGING);
      return;

    } else {
      PRINTLN(F("No anchor is detected. STATE DISCOVERY..."));
      msgToSend = String(myID) + "No anchor is detected. STATE DISCOVERY."; // no anchor is detected
      //delay(SLEEP);
      //updateState(STATE_IDLE);
      updateState(STATE_SEND_DISTANCE_TO_SERVER);
      return;

    }
  }

  if (state == STATE_RANGING) {

    if (idx_anchor < num_anchors) {
      PRINTLN(F("  Sending POLL..."));
      transmitPoll();
      updateState(STATE_POLLACK);
    } else {
      PRINTLN(F("  RANGING is finished."));
      // PRINTLN();
      if (toSend) {
        msgToSend = "";
        msgToSend = String(myID) + "{\"Tag ID\": " + String(myID) + ", \"anchors\": [";
        for (idx_anchor = 0; idx_anchor < num_anchors; idx_anchor++) {
          if (distances[idx_anchor] > 0) { 
              if (idx_anchor == 0) msgToSend += "{ \"Anchor ID\": " + String(anchors[idx_anchor]) + ", \"distance\": " + String(distances[idx_anchor]) + " }"; 
              else msgToSend += ", { \"Anchor ID\": " + String(anchors[idx_anchor]) + ", \"distance\": " + String(distances[idx_anchor]) + " }";
              
            }
        }
        msgToSend += "]}";
        //updateState(STATE_SEND_DISTANCE_TO_SERVER);
      }
      else 
      {
        msgToSend = String(myID) + "No anchor is detected. STATE RANGING"; // no anchor is detected
        //delay(SLEEP);
        //updateState(STATE_IDLE);
      }
      updateState(STATE_SEND_DISTANCE_TO_SERVER);
    }
    return;
  }

  // Safety watchdog to avoid stuck in POLLACK state
  if (state == STATE_POLLACK
      && ((lastSent && currentTime - lastSent > POLLACK_TIMEOUT)
          || currentTime - lastStateChange > 2 * POLLACK_TIMEOUT)) {
    PRINTLN(F("POLLACK timeout.. No POLLACK were received..."));
    //toSend = false;
    if (counterPoll < 10) {
      counterPoll++;
      updateRangingPhase(false);
    }
    else {
      counterPoll = 0;
      updateRangingPhase(true);
    }
    return;
  }

  if (state == STATE_RANGEREPORT
      && ((lastSent && currentTime - lastSent > RANGEREPORT_TIMEOUT)
          || currentTime - lastStateChange > 2 * RANGEREPORT_TIMEOUT)) {
    //toSend = false;
    updateRangingPhase(true);
    return;
  }
    
  checkForReset();

  if (sentAck) {
    PRINTLN(F("Sent something"));
    sentAck = false;
    noteActivity();
    lastSent = lastActivity;

    sentMessageType = txFrame[0];

    if (sentMessageType != BLINK && sentMessageType != POLL && sentMessageType != RANGE) return;

    if (txFrame[0] == BLINK) {
      //PRINTLN(F("  PING sent"));
    }

    if (txFrame[0] == POLL) {
      PRINTLN(F("  POLL sent. Getting timestamp..."));
      DW1000.getTransmitTimestamp(timePollSent);
    }

    if (txFrame[0] == RANGE) {
      PRINTLN(F("  RANGE sent. Getting timestamp..."));
      DW1000.getTransmitTimestamp(timeRangeSent);
    }
  }

  if (receivedAck) {
    PRINTLN(F("received something..."));
    receivedAck = false;
    noteActivity();
    DW1000.getData(rxFrame, FRAME_SIZE);
    receivedMessageType = rxFrame[0];

    if (!isReceiverMatch(rxFrame, myID)) {
      PRINTLN(F("Not for me!"));
      return;
    }
    
    if (receivedMessageType != RANGINGINIT && receivedMessageType != POLLACK && receivedMessageType != RANGEREPORT) {
      PRINTLN(F("Wrong received message!"));
      return;
    }

    if (state == STATE_DISCOVERY && receivedMessageType == RANGINGINIT) {
      getSenderAddr(rxFrame, anchorID);

      uint16_t *anchor = std::find(std::begin(anchors), std::end(anchors), anchorID);
      if (anchor == std::end(anchors)) {
        memcpy(&anchors[idx_anchor], rxFrame + 1, 2);
        PRINT(F("    Anchor")); PRINT(idx_anchor); PRINT(F("(")); PRINT(anchors[idx_anchor]); PRINTLN(F(") found"));
        if (anchors[idx_anchor] > 0) {
          num_anchors++;
          idx_anchor++;
        }
      }
      // add_link(uwb_data, anchors[idx_anchor]);
      return;
    }


    if (isSenderMatch(rxFrame, anchors[idx_anchor])) {
      if (state == STATE_POLLACK && receivedMessageType == POLLACK) {
          PRINTLN(F("Received POLLACK"));
          DW1000.getReceiveTimestamp(timePollAckReceived);
          transmitRange();
          PRINTLN(F("RANGE request sent"));
          updateState(STATE_RANGEREPORT);
          return;
      }

      if (state == STATE_RANGEREPORT && receivedMessageType == RANGEREPORT) {
          timePollReceived.setTimestamp(rxFrame + 5);
          timePollAckSent.setTimestamp(rxFrame + 10);
          timeRangeReceived.setTimestamp(rxFrame + 15);
          computeRangeAsymmetric();
          PRINT(F("distance:")); PRINTLN(distances[idx_anchor]);
          if (distances[idx_anchor] > 0) toSend = true;
          updateRangingPhase(true);
          return;
      }
    }
  }
}