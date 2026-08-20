![Meshtbank1](https://github.com/user-attachments/assets/23c43278-313a-4adc-8904-275a67178925)

# 📡💸 Meshtbank

**A tiny off-grid payment node for Meshtastic, running a lightweight ledger for balances and transfers on low-power hardware.**

Meshtbank is an experimental payment system built on top of **Meshtastic**.

It maintains a central ledger containing users, balances, and transaction histories while using a **LoRa mesh network instead of Internet connectivity**.

Users interact with the system by sending direct Meshtastic messages such as:

```text
balance
pay !27e52039 50
history
```

The central node runs on a **DFRobot FireBeetle 2 ESP32-C6**, connected to a **Seeed Studio XIAO nRF52840 + SX1262 Meshtastic radio**.

A 2-inch TFT display provides a local activity log, while account data is stored persistently in the ESP32's LittleFS filesystem.

> ⚠️ **Meshtbank is an experimental maker project and proof of concept.**
>
> It is not a banking product, cryptocurrency, secure payment processor, or financial service. Do not use it to store or transfer real monetary value.

---

# ✨ Features

* 📡 Works over a Meshtastic LoRa mesh
* 🌐 No Internet connection required
* 🔋 Low-power hardware
* ☀️ Can potentially operate from battery + solar power
* 💸 Peer-to-peer balance transfers
* 👤 Accounts identified by Meshtastic node ID
* 📊 Persistent balances
* 📜 Per-user transaction history
* 📝 System event log
* 🔐 Password-protected administrator commands
* 💬 User interaction through Meshtastic direct messages
* 📢 Startup broadcast announcing the node
* 🖥️ 2-inch TFT activity display
* 💾 LittleFS persistent storage
* 🛡️ Input sanitization and transaction validation
* 📜 MIT licensed

---

# 🧠 Concept

Meshtastic is normally used for low-power, long-range text communication without cellular networks or Internet access.

Meshtbank asks a different question:

> If the communication infrastructure disappears, could the same mesh network support simple local applications such as a community ledger?

Traditional digital payment infrastructure depends on systems such as:

```text
Internet
    ↓
Bank servers
    ↓
Clearing networks
    ↓
Payment processors
    ↓
ATMs / POS terminals
```

Meshtbank replaces that infrastructure with something deliberately much smaller:

```text
Meshtastic nodes
       ↓
LoRa mesh
       ↓
Meshtbank node
       ↓
Local ledger
```

It is intentionally a **centralized ledger running over a decentralized communication network**.

---

# ⚙️ Architecture

```text
               Meshtastic LoRa Mesh
                        │
          ┌─────────────┼─────────────┐
          │             │             │
          ▼             ▼             ▼
     ┌─────────┐   ┌─────────┐   ┌─────────┐
     │ User A  │   │ User B  │   │ User C  │
     │ !nodeID │   │ !nodeID │   │ !nodeID │
     └────┬────┘   └────┬────┘   └────┬────┘
          │             │             │
          └─────────────┴─────────────┘
                        │
                   Direct Messages
                        │
                        ▼
            ┌──────────────────────┐
            │ XIAO nRF52840        │
            │ + SX1262 LoRa        │
            │                      │
            │ Meshtastic firmware  │
            └──────────┬───────────┘
                       │
                       │ Serial / Proto
                       ▼
            ┌──────────────────────┐
            │ FireBeetle 2 ESP32-C6│
            │                      │
            │ Meshtbank firmware   │
            │                      │
            │ • Accounts           │
            │ • Balances           │
            │ • Transfers          │
            │ • History            │
            │ • Admin commands     │
            │ • LittleFS ledger    │
            └──────────┬───────────┘
                       │
                       ▼
              ┌────────────────┐
              │ 2" TFT display │
              │ Activity log   │
              └────────────────┘
```

The **XIAO + SX1262** handles Meshtastic communication.

The **FireBeetle ESP32-C6** runs the banking logic.

---

# 📡 Meshtastic communication

The two boards communicate through the Meshtastic Arduino serial interface.

The current firmware defines:

```cpp
#define SERIAL_RX_PIN 17
#define SERIAL_TX_PIN 16
#define BAUD_RATE 115200
```

The XIAO Meshtastic node is configured with:

```text
Serial module: Enabled
Mode: Proto
TX: Pin 7
RX: Pin 6
```

This creates the bridge:

```text
Meshtastic radio
      │
      ▼
XIAO nRF52840
      │
      │ Serial Proto
      ▼
ESP32-C6
      │
      ▼
Meshtbank
```

---

# 🧰 Hardware

| Component                         | Purpose                           |
| --------------------------------- | --------------------------------- |
| **Seeed Studio XIAO nRF52840**    | Meshtastic node                   |
| **Wio-SX1262 LoRa shield**        | Long-range radio                  |
| **DFRobot FireBeetle 2 ESP32-C6** | Ledger and application logic      |
| **DFRobot 2-inch 320×240 TFT**    | Local status and activity display |
| **3.7 V battery**                 | Portable power                    |
| Jumper wires                      | Serial and power connections      |
| Optional solar panel              | Off-grid charging                 |

### XIAO Meshtastic kit

**[Seeed Studio XIAO nRF52840 + Wio-SX1262 Kit](https://www.seeedstudio.com/XIAO-nRF52840-Wio-SX1262-Kit-for-Meshtastic-p-6400.html)**

### FireBeetle 2 ESP32-C6

**[DFRobot FireBeetle 2 ESP32-C6](https://www.dfrobot.com/product-2771.html)**

---

# 🔌 Wiring

Mount the SX1262 shield on the XIAO nRF52840.

Then connect the XIAO and FireBeetle.

| FireBeetle ESP32-C6 | XIAO  |
| ------------------- | ----- |
| 3.3 V               | 3.3 V |
| GND                 | GND   |
| GPIO 17 / RX        | Pin 7 |
| GPIO 16 / TX        | Pin 6 |

The TFT display connects to the FireBeetle using the DFRobot GDI interface.

The original build therefore requires very little external wiring.

---

# 🖥️ TFT activity display

The firmware uses:

```cpp
DFRobot_ST7789_240x320_HW_SPI
```

through the **DFRobot_GDL** graphics library.

The display shows recent incoming commands and system activity.

A scrolling buffer stores up to:

```cpp
const int MAX_LOGS = 6;
```

recent entries.

Messages are visually distinguished using different colors for commands and system events.

The display is optional: disconnecting it reduces power consumption.

---

# 💾 Ledger storage

Meshtbank stores its ledger using the ESP32's **LittleFS** filesystem.

Each account has a balance file:

```text
/!27e52039.txt
```

and a separate history file:

```text
/!27e52039_hist.txt
```

A system-wide event log is stored as:

```text
/sys.log
```

Conceptually:

```text
LittleFS
│
├── !27e52039.txt
├── !27e52039_hist.txt
│
├── !12ab34cd.txt
├── !12ab34cd_hist.txt
│
└── sys.log
```

The user ID corresponds to the Meshtastic node ID.

---

# 👤 Accounts

Accounts are created by an administrator.

Example:

```text
setup 7777 !27e52039 100
```

This creates or overwrites:

```text
User:
!27e52039

Balance:
100
```

The firmware then records an initial history entry.

> 🔐 `7777` is the default password shown in the current source. **Change it before uploading the firmware.**

---

# 💬 User commands

Commands must be sent to the Meshtbank node using a **private Meshtastic direct message**.

Broadcast commands are intentionally ignored.

---

## `help`

Show the available user commands:

```text
help
```

Response:

```text
CMDS: balance, history <#>, pay <ID> <$>
```

---

## `balance`

Check your current account balance:

```text
balance
```

Example response:

```text
[$] Balance: $100.00
```

Accounts are tied to the Meshtastic node ID of the sender.

---

## `pay`

Transfer funds:

```text
pay <TargetID> <Amount>
```

Example:

```text
pay !27e52039 50
```

Before processing a payment, Meshtbank verifies:

* sender account exists
* destination account exists
* sender and receiver are different
* amount is greater than zero
* sender has enough balance

The system then updates the destination account, checks that the write succeeded, updates the sender account, and creates history records for both users.

The receiver also gets a Meshtastic notification.

Example:

```text
[$] Received $50.00 from !12ab34cd
```

---

## `history`

Display transaction history:

```text
history
```

Example:

```text
HIST:
1. Admin Setup: Initial $100.00
2. TX1234: Sent $20.00 to !27e52039
3. TX1801: Recv $10.00 from !55dd8800
```

---

## `history <N>`

Retrieve a specific transaction record:

```text
history 2
```

Example response:

```text
Rec #2: TX1234: Sent $20.00 to !27e52039
```

This is useful because Meshtastic messages have limited practical payload sizes.

---

# 🔐 Administrator commands

Administrative operations require the configured password.

The password is defined in:

```cpp
const String ADMIN_PASS = "7777";
```

Change this before deploying the node.

---

## Create or overwrite an account

```text
setup <Pass> <User_ID> <Amount>
```

Example:

```text
setup 7777 !27e52039 100
```

---

## Delete an account

```text
delete <Pass> <User_ID>
```

Example:

```text
delete 7777 !27e52039
```

The associated balance and transaction-history files are removed.

---

## List accounts

```text
listusers <Pass>
```

Example:

```text
listusers 7777
```

---

## Check another account

```text
checkbal <Pass> <User_ID>
```

Example:

```text
checkbal 7777 !27e52039
```

---

## Check another user's history

```text
checkhist <Pass> <User_ID>
```

---

## Factory reset

```text
reset <Pass>
```

This deletes all ledger files.

> ⚠️ This operation is destructive.

---

# 📢 Broadcast behavior

Normal banking commands are only accepted through direct messages.

If someone sends a command as a public channel message, Meshtbank ignores it and announces that commands must use private DMs.

After connecting successfully to the Meshtastic node, the system also broadcasts a startup message containing the current number of registered users.

---

# 🛡️ Input validation

Incoming messages are sanitized before processing.

The firmware limits commands to:

```cpp
#define MAX_MSG_LEN 100
```

and removes non-printable characters.

Payments also reject:

```text
self-payments
negative amounts
zero amounts
missing users
insufficient funds
```

These checks improve robustness but should **not** be interpreted as financial-grade security.

---

# 🚀 Installation

## 1. Clone the repository

```bash
git clone https://github.com/ronibandini/Meshtbank.git
cd Meshtbank
```

Repository structure:

```text
Meshtbank/
├── libraries/
│   ├── DFRobot_GDL-master.zip
│   ├── Meshtastic-arduino-master.zip
│   └── readme.txt
├── LICENSE
├── Meshtbank.stl
├── README.md
└── meshtbank1.ino
```

---

# 📻 Configure the Meshtastic node

## 2. Flash the XIAO

Install Meshtastic firmware using:

**[Meshtastic Web Flasher](https://flasher.meshtastic.org/)**

If necessary, put the XIAO into bootloader mode before flashing.

After flashing, connect it to:

**[Meshtastic Web Client](https://client.meshtastic.org/)**

---

## 3. Configure Meshtastic

Configure:

```text
Region: your local region
Primary channel: Meshtbank
```

Then configure the Serial module:

```text
Mode: Proto
TX: 7
RX: 6
```

During initial testing, if using default channel settings, consider:

```text
Hop limit: 1
MQTT: Disabled
```

This reduces unnecessary propagation while testing.

> 📡 Always configure Meshtastic according to the radio regulations applicable to your region.

---

# 🔥 Configure the FireBeetle

## 4. Install libraries

The required Arduino libraries are included under:

```text
libraries/
```

Install:

```text
DFRobot_GDL-master.zip
Meshtastic-arduino-master.zip
```

through the Arduino IDE.

---

## 5. Change the administrator password

Open:

```text
meshtbank1.ino
```

Find:

```cpp
const String ADMIN_PASS = "7777";
```

and replace it:

```cpp
const String ADMIN_PASS = "YOUR_PASSWORD";
```

---

## 6. Upload

Compile and upload the sketch to the **FireBeetle 2 ESP32-C6**.

The serial console runs at:

```text
115200 baud
```

On startup the system:

1. initializes the TFT
2. mounts LittleFS
3. writes a system boot event
4. initializes Meshtastic serial communication
5. requests the Meshtastic node report
6. registers the text-message callback

The display should show:

```text
Meshtbank v1.0
```

followed by:

```text
Initializing...
```

---

# ☀️ Off-grid operation

Meshtbank is designed around low-power hardware.

The original build uses a:

```text
3.7 V battery
```

and the FireBeetle can also be combined with a small solar panel.

For reduced consumption, the TFT can be disconnected.

This makes configurations possible where the payment node operates independently from:

```text
Internet
cellular infrastructure
mains electricity
```

provided the Meshtastic mesh itself remains operational.

---

# 🖨️ 3D stand

The repository includes:

```text
Meshtbank.stl
```

A downloadable version is also available on Cults3D:

**[Meshtbank experimental Meshtastic Banking Node — Cults3D](https://cults3d.com/en/3d-model/gadget/meshtbank-experimental-meshtastic-baking-node)**

The original stand is intentionally simple and designed primarily to hold the prototype components.

---

# 🎥 Demo

**[▶️ Meshtbank demo on YouTube](https://www.youtube.com/shorts/2lq43zoeuWo)**

---

# 🔬 Ideas for extending the project

1. **🔏 Signed transactions** — add cryptographic signatures or another stronger identity mechanism instead of relying primarily on Meshtastic node IDs.

2. **🕒 Reliable timestamps** — add real transaction timestamps using RTC, GPS time, or another time source instead of deriving transaction IDs from ESP32 uptime.

3. **🌐 Distributed ledger experiments** — explore replication between several Meshtbank nodes instead of depending on one central ledger.

---

# 📰 External references

Meshtbank has received independent editorial coverage as well as documentation on several maker platforms.

---

# 🗞️ Independent editorial coverage

## Hackaday

**[Off-Grid, Small-Scale Payment System](https://hackaday.com/2025/12/05/off-grid-small-scale-payment-system/)**

Hackaday writer **Bryan Cockfield** featured Meshtbank on December 5, 2025.

The article describes the project as a small-scale off-grid digital payment system built around Meshtastic and discusses potential applications such as:

* community credits
* festival credits
* local exchange systems
* infrastructure-independent experiments

The article also highlights two important limitations of the prototype: the Meshtastic transport is not designed to provide banking-grade security, and the centralized ledger requires users to trust the administrator.

---

## Hackster News

**[A Meshtastic Banking System for the Apocalypse](https://www.hackster.io/news/a-meshtastic-banking-system-for-the-apocalypse-bce0c1d7bdc1)**

Hackster News writer **Cameron Coward** independently featured Meshtbank as an experimental payment system designed to operate without Internet access or reliable grid power.

The article covers:

* Meshtastic as the communication backbone
* the central ledger
* account balances
* transfers
* the XIAO nRF52840 + SX1262
* FireBeetle ESP32-C6
* TFT display
* battery-powered operation

---

# 🛠️ Project documentation

## Hackaday.io

**[Meshtbank — Hackaday.io](https://hackaday.io/project/204555-meshtbank)**

The complete project page includes:

* concept
* hardware
* wiring
* Meshtastic configuration
* Arduino setup
* command reference
* off-grid operation
* source-code link
* 3D-model link

---

## DFRobot Maker Community

**[Meshtastic payment ledger with FireBeetle 2: Meshtbank](https://community.dfrobot.com/makelog-318289.html)**

DFRobot Maker Community hosts the hardware tutorial for the project.

It documents:

* FireBeetle 2 ESP32-C6
* XIAO Meshtastic node
* 2-inch TFT
* battery operation
* Meshtastic configuration
* user commands
* administrator commands

The article explicitly links to this GitHub repository.

---

## Medium

**[Meshtbank, un sistema de pagos sobre Meshtastic](https://bandini.medium.com/meshtbank-un-sistema-de-pagos-sobre-meshtastic-8ea756e4d9af)**

Spanish-language article by Roni Bandini explaining the motivation, architecture, hardware, setup, and operation of Meshtbank.

The article also links directly to:

* this repository
* Hackaday.io tutorial
* Cults3D model
* Hackaday coverage
* Hackster coverage

---

# 🖨️ 3D model

## Cults3D

**[Meshtbank experimental Meshtastic Banking Node](https://cults3d.com/en/3d-model/gadget/meshtbank-experimental-meshtastic-baking-node)**

Downloadable STL for the original prototype stand.

The STL is also included directly in this repository as:

```text
Meshtbank.stl
```

---

# 📚 Useful references

* **[Meshtastic](https://meshtastic.org/)**
* **[Meshtastic Web Flasher](https://flasher.meshtastic.org/)**
* **[Meshtastic Web Client](https://client.meshtastic.org/)**
* **[Meshtastic GitHub](https://github.com/meshtastic)**
* **[Meshtastic Arduino](https://github.com/meshtastic/Meshtastic-arduino)**
* **[DFRobot GDL](https://github.com/DFRobot/DFRobot_GDL)**

---

# 🔗 You may also be interested in...

Other projects by **Roni Bandini** exploring off-grid systems, wireless communication, and unconventional embedded applications.

## 🗳️📡 Meshtvote

**Decentralized decision-making over Meshtastic for scenarios without conventional communications infrastructure.**

Meshtvote is the closest companion project to Meshtbank: instead of moving balances over Meshtastic, it uses the same network concept to coordinate voting and collective decisions.

**[github.com/ronibandini/Meshtvote](https://github.com/ronibandini/Meshtvote)**

---

## 📡🎯 Router Attack

**An ESP8266 game that transforms nearby Wi-Fi networks into physical game targets.**

Another experiment that repurposes wireless-network metadata as the foundation of an unconventional embedded application.

**[github.com/ronibandini/routerattack](https://github.com/ronibandini/routerattack)**

---

## 📡🚨 mmWave Alarm

**Human-presence detection using mmWave radar, ESP32, and remote notifications.**

Another compact embedded project exploring radio-frequency technologies outside their usual applications.

**[github.com/ronibandini/mmWaveAlarm](https://github.com/ronibandini/mmWaveAlarm)**

---

# 🔐 Security and trust model

Meshtbank should be treated as an **experimental protocol demonstrator**, not as a secure financial platform.

The current design has several intentional simplifications.

### Central authority

One Meshtbank node owns the complete ledger.

The administrator can:

```text
create accounts
overwrite balances
delete accounts
inspect balances
inspect histories
wipe the ledger
```

Users therefore need to trust the operator of that node.

### Administrator password

The administrator password is embedded directly in the firmware:

```cpp
const String ADMIN_PASS = "7777";
```

It is sent as part of administrator commands and is not a substitute for modern authentication.

### Node identity

Accounts are associated with Meshtastic node IDs.

The project does not implement independent cryptographic user identities or transaction signatures.

---

# 📜 License

Meshtbank is released under the **MIT License**.

See [`LICENSE`](LICENSE) for details.

---

# 👤 Author

**Roni Bandini**

Maker, AI developer, electronic artist and writer.

* 🐙 GitHub: **[@ronibandini](https://github.com/ronibandini)**
* 📸 Instagram: **[@ronibandini](https://www.instagram.com/ronibandini/)**
* 🐦 X: **[@RoniBandini](https://x.com/RoniBandini)**
* ✍️ Medium: **[bandini.medium.com](https://bandini.medium.com/)**
* 🛠️ Hackaday.io: **[Roni Bandini](https://hackaday.io/ronibandini)**

Contributions, forks, alternative ledger models, Meshtastic experiments, and improved enclosures are welcome.
