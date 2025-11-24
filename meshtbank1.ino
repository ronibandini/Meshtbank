/*
 * Meshtbank v1.0 - because if things go downhill, this could be a bank...
 * Developed by Roni Bandini https://x.com/RoniBandini Nov 2025
 * License: MIT
 * 
 * Firebeetle 2 ESP32C6
 * 
 * User commands (via DM):
 * -----------------------
 * balance               -> Shows current balance.
 * pay <User_ID> <Amt>   -> Transfers funds (e.g., pay !27e52039 50).
 * history               -> Shows transaction history.
 * history <N>           -> Shows specific line N of history.
 * help                  -> Shows available commands.
 * 
 * Admin commands (via DM with Password):
 * ----------------------------------
 * setup <Pass> <User_ID> <Amt>    -> Create/Overwrite user with initial balance.
 * delete <Pass> <User_ID>         -> Delete specific user.
 * reset <Pass>                    -> FACTORY RESET (Wipes all data).
 * listusers <Pass>                -> List all users and balances.
 * checkbal <Pass> <User_ID>       -> Spy: View user balance.
 * checkhist <Pass> <User_ID>      -> Spy: View user history.

 */

#include <Arduino.h>
#include <Meshtastic.h>
#include "DFRobot_GDL.h"
#include "FS.h"
#include <LittleFS.h>
#include <vector>

#define SERIAL_RX_PIN 17  // to Xiao 7
#define SERIAL_TX_PIN 16  // to Xiao 6
#define BAUD_RATE 115200 // setup in Meshtastic, modules, serial
#define MAX_MSG_LEN 100 // L Bobbitt rule

// Change before uploading
const String ADMIN_PASS = "7777"; 

// --- Screen Config ---
#define TFT_DC  D2
#define TFT_CS  D6
#define TFT_RST D3

DFRobot_ST7789_240x320_HW_SPI screen(TFT_DC, TFT_CS, TFT_RST);

// --- Scrolling Buffer ---
struct LogMessage {
  String sender;
  String text;
  bool isCommand;
};

std::vector<LogMessage> messageBuffer;
const int MAX_LOGS = 6; 
int messageCount = 0;

// --- State Variables ---
bool nodeConnected = false;
bool startupBroadcastSent = false;
unsigned long connectionTime = 0;

// ==========================================
// SECURITY & HELPER FUNCTIONS
// ==========================================

String sanitizeInput(String input) {
  if (input.length() > MAX_MSG_LEN) {
    input = input.substring(0, MAX_MSG_LEN);
  }
  String clean = "";
  for (unsigned int i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    if (isPrintable(c)) {
      clean += c;
    }
  }
  return clean;
}

String formatNodeId(uint32_t id) {
  String hexId = String(id, HEX);
  hexId.toLowerCase();
  return "!" + hexId;
}

uint32_t parseNodeId(String hexId) {
  if (hexId.startsWith("!")) hexId = hexId.substring(1);
  return (uint32_t) strtoul(hexId.c_str(), NULL, 16);
}

// ==========================================
// FILESYSTEM
// ==========================================

void initFileSystem() {
  if (!LittleFS.begin(true)) {
    Serial.println("FS Error");
  } else {
    Serial.println("LittleFS Mounted");
  }
}

String getUserFile(String user) { return "/" + user + ".txt"; }
String getHistoryFile(String user) { return "/" + user + "_hist.txt"; }
String getSystemLogFile() { return "/sys.log"; }

void logSystem(String entry) {
  File file = LittleFS.open(getSystemLogFile(), "a");
  if (file) {
    String logLine = String(millis()/1000) + "s: " + entry;
    file.println(logLine);
    file.close();
    Serial.println("[SYSLOG] " + logLine);
  }
}

int countUsers() {
  int count = 0;
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  while (file) {
    String fname = file.name();
    if (fname.endsWith(".txt") && fname.indexOf("_hist") == -1 && fname != "sys.log") {
      count++;
    }
    file = root.openNextFile();
  }
  return count;
}

float getBalance(String user) {
  String path = getUserFile(user);
  if (!LittleFS.exists(path)) return -1.0;
  File file = LittleFS.open(path, "r");
  if (!file) return -1.0;
  String balStr = file.readStringUntil('\n');
  file.close();
  return balStr.toFloat();
}

void updateBalance(String user, float newBalance) {
  String path = getUserFile(user);
  File file = LittleFS.open(path, "w");
  if (file) {
    file.println(newBalance);
    file.close();
  }
}

void addToHistory(String user, String logEntry) {
  String path = getHistoryFile(user);
  File file = LittleFS.open(path, "a");
  if (file) {
    file.println(logEntry);
    file.close();
  }
}

String readHistory(String user) {
  String path = getHistoryFile(user);
  if (!LittleFS.exists(path)) return "No history.";
  File file = LittleFS.open(path, "r");
  String history = "";
  int lineCount = 1;
  while(file.available()) {
    String line = file.readStringUntil('\n');
    if(line.length() > 0) {
      history += String(lineCount) + ". " + line + "\n";
      lineCount++;
    }
  }
  file.close();
  return history;
}

String getHistoryRecord(String user, int targetLine) {
  String path = getHistoryFile(user);
  if (!LittleFS.exists(path)) return "No history.";
  File file = LittleFS.open(path, "r");
  int currentLine = 1;
  while(file.available()) {
    String line = file.readStringUntil('\n');
    if (currentLine == targetLine) {
      file.close();
      return "Rec #" + String(targetLine) + ": " + line;
    }
    currentLine++;
  }
  file.close();
  return "Record #" + String(targetLine) + " not found.";
}

// --- FIXED: DELETE & RESET HELPERS ---

void deleteUserFiles(String user) {
  String uPath = getUserFile(user);
  String hPath = getHistoryFile(user);
  
  bool d1 = false, d2 = false;
  
  if (LittleFS.exists(uPath)) d1 = LittleFS.remove(uPath);
  if (LittleFS.exists(hPath)) d2 = LittleFS.remove(hPath);
  
  if (d1 || d2) {
    logSystem("Admin deleted user: " + user);
    Serial.println("Manual Delete Success: " + user);
  } else {
    Serial.println("Manual Delete Failed (File not found?): " + user);
  }
}

// Now it works and all
void wipeSystem() {
  std::vector<String> filesToDelete;
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  
  // 1. Collect all files
  while (file) {
    String name = file.name();
    // Ensure path starts with /
    if (!name.startsWith("/")) name = "/" + name;
    filesToDelete.push_back(name);
    file = root.openNextFile();
  }
  
  // 2. Delete all 
  for (const auto& path : filesToDelete) {
    LittleFS.remove(path);
    Serial.println("Deleted: " + path);
  }
  
  logSystem("SYSTEM FACTORY RESET PERFORMED");
}

String listAllUsers() {
  String output = "USERS:\n";
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  bool found = false;
  while (file) {
    String fname = file.name();
    if (fname.endsWith(".txt") && fname.indexOf("_hist") == -1 && fname != "sys.log") {
      if(fname.startsWith("/")) fname = fname.substring(1);
      String user = fname.substring(0, fname.length() - 4);
      File f = LittleFS.open("/" + fname, "r");
      String bal = f.readStringUntil('\n');
      f.close();
      output += user + ": $" + bal + "\n";
      found = true;
    }
    file = root.openNextFile();
  }
  if (!found) return "No users found.";
  return output;
}

// ==========================================
// SCREEN & SCROLL LOGIC
// ==========================================

void redrawBuffer() {
  screen.fillRect(0, 35, 240, 285, COLOR_RGB565_BLACK);
  int currentY = 40;
  
  for (const auto& log : messageBuffer) {
    screen.setTextSize(1);
    if (log.isCommand) screen.setTextColor(COLOR_RGB565_YELLOW);
    else screen.setTextColor(COLOR_RGB565_ORANGE);
    
    screen.setCursor(10, currentY);
    screen.print(log.sender);
    currentY += 10;
    
    screen.setTextColor(COLOR_RGB565_WHITE);
    screen.setCursor(10, currentY);
    
    if (log.text.length() > 38) {
      screen.print(log.text.substring(0, 38));
      currentY += 10;
      screen.setCursor(10, currentY);
      screen.print(log.text.substring(38));
    } else {
      screen.print(log.text);
    }
    currentY += 25; 
  }
}

void displayOnScreen(String sender, String msg, bool isCommand) {
  messageCount++;
  String senderStr = "#" + String(messageCount) + " " + sender;
  LogMessage newLog = {senderStr, msg, isCommand};
  messageBuffer.push_back(newLog);
  
  if (messageBuffer.size() > MAX_LOGS) {
    messageBuffer.erase(messageBuffer.begin());
  }
  redrawBuffer();
}

// ==========================================
// BANKING - sort of - LOGIC
// ==========================================

String processBankingCommand(String senderID, String cmdLine) {
  cmdLine.trim();
  int firstSpace = cmdLine.indexOf(' ');
  String cmd = (firstSpace == -1) ? cmdLine : cmdLine.substring(0, firstSpace);
  String args = (firstSpace == -1) ? "" : cmdLine.substring(firstSpace + 1);
  
  cmd.toLowerCase(); 

  if (cmd == "help") {
    return "CMDS: balance, history <#>, pay <ID> <$>";
  }
  else if (cmd == "balance") {
    float bal = getBalance(senderID);
    if (bal == -1.0) return "Err: No account.";
    return "[$] Balance: $" + String(bal);
  }
  else if (cmd == "history") {
    args.trim();
    if (args.length() > 0) {
      int recordNum = args.toInt();
      if (recordNum > 0) {
        return getHistoryRecord(senderID, recordNum);
      }
    }
    return "HIST:\n" + readHistory(senderID);
  }
  else if (cmd == "pay") {
    int split = args.indexOf(' ');
    if (split > 0) {
      String targetUser = args.substring(0, split);
      float amount = args.substring(split + 1).toFloat();
      targetUser.toLowerCase();
      
      if (senderID == targetUser) return "Err: Cannot pay self.";
      
      float senderBal = getBalance(senderID);
      float targetBal = getBalance(targetUser);
      
      if (senderBal == -1.0) return "Err: No account.";
      if (targetBal == -1.0) return "Err: Target not found.";
      if (amount <= 0) return "Err: Invalid amount.";
      if (senderBal < amount) return "Err: Low funds.";
      
      float newSenderBal = senderBal - amount;
      float newTargetBal = targetBal + amount;
      
      updateBalance(targetUser, newTargetBal);
      float checkTargetBal = getBalance(targetUser);
      if (abs(checkTargetBal - newTargetBal) > 0.01) {
        logSystem("CRITICAL: Write failed for " + targetUser);
        return "Err: System Error. Funds NOT transferred.";
      }
      updateBalance(senderID, newSenderBal);
      
      long txID = millis() / 1000;
      addToHistory(senderID, "TX" + String(txID) + ": Sent $" + String(amount) + " to " + targetUser);
      addToHistory(targetUser, "TX" + String(txID) + ": Recv $" + String(amount) + " from " + senderID);
      logSystem("TX: " + senderID + " sent " + String(amount) + " to " + targetUser);
      
      uint32_t targetNodeAddr = parseNodeId(targetUser);
      if (targetNodeAddr > 0) {
        String notifyMsg = "[$] Received $" + String(amount) + " from " + senderID;
        mt_send_text(notifyMsg.c_str(), targetNodeAddr, 0);
      }
      
      return "[$] Sent $" + String(amount) + ". New Bal: $" + String(newSenderBal);
    }
    return "Usage: pay <TargetID> <Amount>";
  }
  else if (cmd == "setup") {
    int s1 = args.indexOf(' ');
    if (s1 > 0) {
      String pass = args.substring(0, s1);
      String rest = args.substring(s1 + 1);
      int s2 = rest.indexOf(' ');
      if (s2 > 0) {
        String targetUser = rest.substring(0, s2);
        float amount = rest.substring(s2 + 1).toFloat();
        targetUser.toLowerCase();
        
        if (pass == ADMIN_PASS) {
          updateBalance(targetUser, amount);
          addToHistory(targetUser, "Admin Setup: Initial $" + String(amount));
          logSystem("Setup user " + targetUser + " with $" + String(amount));
          return "Setup OK for " + targetUser;
        } else return "Err: Wrong Password";
      }
    }
    return "Usage: setup <Pass> <TargetID> <Amt>";
  }
  else if (cmd == "delete") {
    int s1 = args.indexOf(' ');
    if (s1 > 0) {
      String pass = args.substring(0, s1);
      String target = args.substring(s1 + 1);
      target.toLowerCase();
      
      if (pass == ADMIN_PASS) {
         deleteUserFiles(target); // Try delete regardless of getBalance check to clean ghosts
         return "Delete command executed for " + target;
      } else return "Err: Wrong Password";
    }
    return "Usage: delete <Pass> <TargetID>";
  }
  else if (cmd == "reset") {
    if (args == ADMIN_PASS) {
      wipeSystem();
      return "SYSTEM RESET: All data wiped.";
    }
    return "Err: Wrong Password. Usage: reset <Pass>";
  }
  else if (cmd == "listusers") {
    if (args == ADMIN_PASS) return listAllUsers();
    return "Err: Wrong Password";
  }
  else if (cmd == "checkbal") {
    int s1 = args.indexOf(' ');
    if (s1 > 0) {
      String pass = args.substring(0, s1);
      String target = args.substring(s1 + 1);
      target.toLowerCase();
      if (pass == ADMIN_PASS) {
        float bal = getBalance(target);
        if (bal == -1.0) return "User not found";
        return target + " Bal: $" + String(bal);
      }
    }
    return "Usage: checkbal <Pass> <TargetID>";
  }
  else if (cmd == "checkhist") {
    int s1 = args.indexOf(' ');
    if (s1 > 0) {
      String pass = args.substring(0, s1);
      String target = args.substring(s1 + 1);
      target.toLowerCase();
      if (pass == ADMIN_PASS) {
        return "HIST " + target + ":\n" + readHistory(target);
      }
    }
    return "Usage: checkhist <Pass> <TargetID>";
  }

  return "Unknown Command. Try 'help'";
}

// ==========================================
// MESHTASTIC CALLBACKS
// ==========================================

void connected_callback(mt_node_t* node, mt_nr_progress_t progress) {
  if (!nodeConnected) {
    Serial.println("Meshtastic Node Connected!");
    displayOnScreen("SYSTEM", "Node Connected", false);
    
    nodeConnected = true;
    connectionTime = millis();
    startupBroadcastSent = false;
  }
}

void text_message_callback(uint32_t from, uint32_t to, uint8_t channel, const char* text) {
  String senderID = formatNodeId(from);
  
  String rawMsg = String(text);
  String message = sanitizeInput(rawMsg);
  
  if (to == 0xFFFFFFFF) {
    displayOnScreen(senderID, "Broadcast Ignored", false);
    String helpMsg = "MESH BANK: Commands must be sent via Private DM.";
    mt_send_text(helpMsg.c_str(), 0xFFFFFFFF, channel);
    return;
  }

  displayOnScreen(senderID, message, true);
  String reply = processBankingCommand(senderID, message);

  if (reply != "") {
    Serial.println("Reply: " + reply);
    mt_send_text(reply.c_str(), from, channel);
  } 
}

// ==========================================
// MAIN
// ==========================================

void setup() {
  Serial.begin(115200);
  screen.begin();
  screen.fillScreen(COLOR_RGB565_BLACK);
  
  screen.setTextSize(2);
  screen.setTextColor(COLOR_RGB565_CYAN);
  screen.setCursor(10, 10);
  screen.print("Meshtbank v1.0");
  screen.drawFastHLine(0, 32, 240, COLOR_RGB565_WHITE); 
  
  initFileSystem();
  logSystem("System Boot");

  mt_serial_init(SERIAL_RX_PIN, SERIAL_TX_PIN, BAUD_RATE);
  mt_request_node_report(connected_callback);
  set_text_message_callback(text_message_callback);
  
  Serial.println("System Started.");
  displayOnScreen("SYSTEM", "Initializing...", false);
}

void loop() {
  mt_loop(millis());

  if (nodeConnected && !startupBroadcastSent) {
    if (millis() - connectionTime > 5000) { 
      String bCastMsg = "Meshtbank is an experimental payment system built on Meshtastic, developed by Roni Bandini under the MIT license in 2025. Users: " + String(countUsers());
      
      displayOnScreen("SYSTEM", "Sending Broadcast...", false);
      mt_send_text(bCastMsg.c_str(), 0xFFFFFFFF, 0);
      startupBroadcastSent = true;
    }
  }
}