#include "arduino.h"

void updateState(int nextState) {
  state = nextState;
  lastStateChange = millis();
}

void noteActivity() {
  lastActivity = millis();
}

void setAddress() {
    String macAddr = WiFi.macAddress();
    Serial.println(macAddr);
    if (macAddr == "70:B8:F6:D8:F8:B8") {
        myID = 101;
    } else if (macAddr == "70:B8:F6:D8:F8:28") {
        myID = 102;
    } else {
        Serial.println("Wrong tag MAC address.");
    }
}

boolean isTagAddress(uint16_t addr) {
    return (0 < addr && addr < 99) ? true : false;
}

void checkForReset() {
    if(!sentAck && !receivedAck) {
      if(currentTime - lastActivity > DEFAULT_RESET_TIMEOUT) {
        initReceiver();
        Serial.println("Reinit....");
      }
      return;
    }
}

bool isExpectedReceivedMessageType (byte messageType) { 
    return (messageType == MSG_TYPE_BLINK || messageType == MSG_TYPE_POLL || messageType == MSG_TYPE_RANGE) ? true : false; 
} 
 
void prepareMessageToSend(byte messageType, byte source, byte destination) { 
    memcpy(message, &messageType, sizeof(messageType)); 
    memcpy(message + 1, &source, sizeof(source)); 
    memcpy(message + 2, &destination, sizeof(destination)); 
} 
 
void sendMessage(byte messageType) { 
  Serial.println("---Send Message---");
  DW1000.newTransmit(); 
  DW1000.setDefaults(); 
  if (messageType == MSG_TYPE_ANCHOR_ADDR) {
    Serial.println("  MSG_TYPE_ANCHOR_ADDR");
    prepareMessageToSend(MSG_TYPE_ANCHOR_ADDR, myID, tagID);
  }  
  else if (messageType == MSG_TYPE_POLL_ACK) { 
    Serial.println("  MSG_TYPE_POLL_ACK");
    prepareMessageToSend(MSG_TYPE_POLL_ACK, myID, tagID); 
    DW1000.setDelay(pollackReplyDelay); 
  }  
  else if (messageType == MSG_TYPE_RANGE_REPORT) { 
    Serial.println("  MSG_TYPE_RANGE_REPORT");
    prepareMessageToSend(MSG_TYPE_RANGE_REPORT, myID, tagID); 
    timePollReceived.getTimestamp(message + 3); 
    timePollAckSent.getTimestamp(message + 8); 
    timeRangeReceived.getTimestamp(message + 13); 
  }  
 
  DW1000.setData(message, sizeof(message)); 
  Serial.print("  Message content: ");
  for (size_t i = 0; i < sizeof(message); i++) {
    Serial.print(message[i]);
    Serial.print(" ");
  }
  Serial.println(" ");
  
  DW1000.startTransmit(); 
  while (!DW1000.isTransmitDone()) continue; 
  Serial.println("    message is sent");
  // if (messageType == MSG_TYPE_POLL_ACK) { 
  //   DW1000.getTransmitTimestamp(timePollAckSent); 
  // } 
 
 // DW1000.clearTransmitStatus(); 
} 
 
bool messageReceived(byte expectedMessageType) { 
  Serial.println("---Receive Message---");
   
  // DW1000.newReceive(); 
  // DW1000.setDefaults();
  // DW1000.startReceive(); 

  // while (!DW1000.isReceiveDone() && !DW1000.isReceiveTimeout()); 
  // if (DW1000.isReceiveTimeout()) { 
  //   Serial.println("  Receive Timeout");
  //   //DW1000.clearReceiveTimeout(); 
  //   return false; 
  // } 

  DW1000.getData(message, sizeof(message)); 
  Serial.print("  Message content: ");
  for (size_t i = 0; i < sizeof(message); i++) {
    Serial.print(message[i]);
    Serial.print(" ");
  }
  Serial.println(" ");
  
  Serial.print("  Tag Address: ");
  Serial.println(message[1]);
  if (!isTagAddress(message[1])) {
    Serial.print("  Wrong Tag Address: ");
    Serial.println(message[1]);
    return false; 
  }
 
  if (message[0] == MSG_TYPE_BLINK && expectedMessageType == MSG_TYPE_BLINK) { 
    tagID = message[1]; 
  } 
   
  if (message[0] == expectedMessageType && message[1] == tagID) {
    Serial.print("  message: ");
    Serial.println(expectedMessageType);
    if (expectedMessageType == MSG_TYPE_POLL || expectedMessageType == MSG_TYPE_RANGE) { 
      Serial.print("Received time before clearReceiveStatus: "); 
      DW1000.getReceiveTimestamp(tempTime); 
      Serial.println(tempTime); 
    } 
    DW1000.clearReceiveStatus(); 
    if (expectedMessageType == MSG_TYPE_POLL || expectedMessageType == MSG_TYPE_RANGE) { 
      Serial.print("Received time after clearReceiveStatus: "); 
      DW1000.getReceiveTimestamp(tempTime); 
      Serial.println(tempTime); 
    } 
 
    return true; 
  } 
  
  Serial.println("!!!Received Wrong Message!!!");
  Serial.print("  Expected: ");
  Serial.println(expectedMessageType);
  Serial.print("  Received: ");
  Serial.println(message[0]);
  return false; 
 
} 

void handleReceived() {
  receivedAck = true;
}

void handleSent() {
  sentAck = true;
}

void initReceiver() {
  DW1000.newReceive();
  DW1000.setDefaults();
  DW1000.receivePermanently(true);
  DW1000.startReceive();
  noteActivity();
}

//void prepareTx() {
//  DW1000.newTransmit();
//  DW1000.setDefaults();
//}
//
//void startTx() {
//  DW1000.setData(txFrame, FRAME_SIZE);
//  DW1000.startTransmit();
//  lastSent = 0;
//}
//
//void transmitRangingInit() {
//  prepareTx();
//  txFrame[0] = RANGINGINIT;
//  setSenderAddr(txFrame, myID);
//  setReceiverAddr(txFrame, tagID);
//  startTx();
//}
//
//void transmitPollAck() {
//  prepareTx();
//  txFrame[0] = POLLACK;
//  setSenderAddr(txFrame, myID);
//  setReceiverAddr(txFrame, tagID);
//  DW1000.setDelay(pollackReplyDelay);
//  startTx();
//}
//
//void transmitRangeReport() {
//  prepareTx();
//  txFrame[0] = RANGEREPORT;
//  setSenderAddr(txFrame, myID);
//  setReceiverAddr(txFrame, tagID);
//  timePollReceived.getTimestamp(txFrame + 5);
//  timePollAckSent.getTimestamp(txFrame + 10);
//  timeRangeReceived.getTimestamp(txFrame + 15);
//  startTx();
//}

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
  
  //DW1000.setAntennaDelay(Adelay);

  initReceiver();

}

void loop() {
  currentTime = millis();
  
  checkForReset();

  // if (state == STATE_RANGING_INIT
  //     && currentTime - lastStateChange > RANGING_INIT_TIMEOUT) {
  //   PRINTLN(F("Seems Pending Pong lost. Return to IDLE"));
  //   updateState(STATE_IDLE);
  // }

  // if (state == STATE_RANGE
  //     && ((lastSent && currentTime - lastSent > RANGE_TIMEOUT)
  //         || currentTime - lastStateChange > 2 * RANGE_TIMEOUT)) {
  //   /*
  //    * Check RANGE message timeout when state is waiting for RANGE message
  //    */
  //   PRINTLN(F("RANGE timeout. Return to IDLE"));
  //   updateState(STATE_IDLE);
  //   return;
  // }

  if (sentAck) {
    // PRINTLN(F("Sent something"));
    sentAck = false;
    noteActivity();
    lastSent = lastActivity;

    Serial.println("Send something");
    if (message[0] == MSG_TYPE_POLL_ACK) {
      DW1000.getTransmitTimestamp(timePollAckSent);
    }
    // sentMessageType = txFrame[0];

    // PRINT(F("  MSG TYPE: ")); PRINTLN(sentMessageType);

    // if (sentMessageType != RANGINGINIT && sentMessageType != POLLACK && sentMessageType != RANGEREPORT) return;

    // if (state == STATE_RANGING_INIT && sentMessageType == RANGINGINIT) {
    //   PRINTLN(F("  Ranging phase initialized sent. Return to IDLE"));
    //   updateState(STATE_IDLE);
    //   return;
    // }

    // if (sentMessageType == POLLACK) {
    //   PRINTLN(F("  POLLACK sent. Getting timestamp..."));
    //   DW1000.getTransmitTimestamp(timePollAckSent);
    // }

    // if (sentMessageType == RANGEREPORT) {
    //   PRINTLN(F("  RANGEREPORT sent"));
    // }
  }
  
  if (receivedAck) {
    // PRINTLN(F("Received something"));
    receivedAck = false;
    noteActivity();

    Serial.println("Received something");
    if (messageReceived(MSG_TYPE_BLINK)) { 
      sendMessage(MSG_TYPE_ANCHOR_ADDR); 
    } else if (messageReceived(MSG_TYPE_POLL)) { 
      DW1000.getReceiveTimestamp(timePollReceived); 
      sendMessage(MSG_TYPE_POLL_ACK); 
    } else if (messageReceived(MSG_TYPE_RANGE)) { 
      DW1000.getReceiveTimestamp(timeRangeReceived); 
      sendMessage(MSG_TYPE_RANGE_REPORT); 
    }
    // DW1000.getData(rxFrame, FRAME_SIZE);
    // getSenderAddr(rxFrame, tagID);
    // if (!isTagAddress(tagID)) return; 

    // PRINT(F("Received something from tag: ")); PRINTLN(tagID);

    // receivedMessageType = rxFrame[0];
    // //PRINT(F("Received msg: ")); PRINTLN(receivedMessageType);
    // if (receivedMessageType != BLINK && receivedMessageType != POLL && receivedMessageType != RANGE) return;
    // PRINT(F("Current state: ")); PRINTLN(state);
    // if (state == STATE_IDLE) {
    //   PRINTLN(F("  State: IDLE"));
    //   if (receivedMessageType == BLINK) {
    //     PRINTLN(F("    Received BLINK. Reply with RANGING INIT"));
    //     rangingInitDelay = random(0, 50);
    //     //runtimeDelay = millis();
    //     //while (millis() - runtimeDelay < rangingInitDelay) continue;
    //     delay(rangingInitDelay);
    //     transmitRangingInit();
    //     updateState(STATE_RANGING_INIT);
    //     return;
    //   }

    //   if (receivedMessageType == POLL && isReceiverMatch(rxFrame, myID)) {
    //     PRINTLN(F("    Received POLL"));
    //     DW1000.getReceiveTimestamp(timePollReceived);
    //     transmitPollAck();
    //     PRINTLN(F("      Reply with POLLACK"));
    //     updateState(STATE_RANGE);
    //     return;
    //   }
    // }

    // if (state == STATE_RANGING_INIT) {
    //   PRINTLN(F("  State: STATE_RANGING_INIT"));
    //   PRINTLN(F("    Ignore all received frames"));
    //   /*
    //    * PONG message is pending to be transmitted
    //    * Anchor should ignore all other messages
    //    */
    //   return;
    // }

    // if (state == STATE_RANGE && receivedMessageType == RANGE && isSenderMatch(rxFrame, tagID)) {
    //   Serial.println("Received RANGE");
    //   DW1000.getReceiveTimestamp(timeRangeReceived);
    //   transmitRangeReport();
    //   updateState(STATE_IDLE);
    //   return;
    // }
  }
}
