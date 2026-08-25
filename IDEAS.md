# Идеи за бъдещо развитие

Място за пазене на идеи, преди да сме готови да ги реализираме.
Добавяй нови идеи под "Неразработени", а когато нещо бъде
имплементирано, премести го в `README.md` / commit history и го
изтрий оттук (или го маркирай като done, ако искаш история).

Как се добавя идея:
- Отвори този файл във VS Code (или помоли Copilot да го направи).
- Добави нов ред/точка под "Неразработени" с кратко описание.
- Commit-ни промяната (`git add IDEAS.md && git commit -m "..."`),
  за да е записана в историята и да не се загуби.

## Неразработени

### 🏠 Multi-device Smart Home Hub

1. **Централен ESP32 "hub" сървър**
   - Събира данни от множество ESP32 възли, разпръснати из къщата
     (не само 1 сензор).
   - Всеки възел праща измерванията си (темп/влажност/налягане,
     по-късно и други) към hub-а.
   - Hub-ът е единствената точка, която говори със сайта/desktop
     приложението - те не общуват директно с всеки сензор поотделно.
   - **Отворен въпрос - мрежова топология:**
     - (а) всички ESP32 възли + hub-ът на един и същ Wi-Fi (рутер),
       hub-ът периодично ги "пита" по HTTP (най-лесно, но изисква
       статичен IP/hostname на всеки възел); или
     - (б) възлите комуникират директно с hub-а (напр. ESP-NOW),
       без нужда всеки да пази Wi-Fi парола - по-сложно, но по-добро
       за малки, батерийни сензори.

2. **Разширяеми типове измервания**
   - Освен темп/влажност/налягане - ток, светлина и бъдещи сензори.
   - Нов тип измерване → автоматично получава свой gauge на
     сайта/приложението, без да се пренаписва интерфейсът всеки път.
   - **Механизъм:** всеки node праща стойностите си към hub-а в JSON,
     като всяка стойност носи "таг" - какво измерва (напр. `"type":
     "temperature"`), мерна единица (`"unit": "C"`) и самата стойност.
     Сайтът/приложението интерпретират тага и решават как да го
     покажат (gauge, единица, име), без hub-ът да има твърдо
     закодиран списък от типове.
   - Възможност за **добавяне на нови мерни единици и граници**
     (min/max за gauge-а, "нормални" прагове за оцветяване) през
     конфигурация, вместо да се преправя кода при всеки нов сензор.

3. **Лесно добавяне на нов дивайс**
   - Plug-and-play усещане - качваш firmware на нов ESP32, той се
     появява в системата без ръчна конфигурация на hub-а.
   - **Отворен въпрос:** auto-discovery в локалната мрежа
     (mDNS/broadcast), или ръчно въвеждане на IP/име през сайта или
     desktop приложението?

4. **Нисковолтов дизайн на всеки node**
   - Всеки ESP32 node да е проектиран да консумира минимално, с идея
     за евентуално батерийно захранване (не само кабел към контакт).
   - На практика: **deep sleep** между измерванията (ESP32 спи
     повечето време, буди се само колкото да измери + прати данните
     + пак заспива), вместо постоянно будно Wi-Fi устройство.
   - Node-ът да докладва и **ниво на батерията** (напр. чрез делител
     на напрежение към ADC pin), за да се вижда в dashboard-а кога
     трябва смяна/зареждане.

5. **Сайтът да поддържа много дивайси**
   - Показва списък с всички регистрирани дивайси + техните
     измервания, вероятно с етикет "стая/локация" на всеки дивайс.

6. **Desktop приложението → контролен панел за умен дом**
   - Разширява се от "1 tray widget" в dashboard за всички дивайси.
   - Добавяне/премахване на дивайси директно от приложението.
   - Лесно наблюдение, докато си на компютъра.

7. **Здраве на дивайсите + прости автоматизации**
   - **Device health**: индикатор "онлайн/офлайн", "последно видян
     преди X минути", ниво на батерия директно на всеки dashboard
     елемент - особено полезно с батерийни nodes, за да знаеш кой е
     "умрял".
   - **Прости автоматизации**: правила от типа "ако влажността в
     дадена стая мине над X% → изпрати известие" (по-нататък -
     задейства реле/смарт контакт). Превръща проекта от чисто
     мониторинг в истински "умен дом"; не е задължително за първата
     версия.

8. **SD карта на hub-а за детайлно логване**
   - Hub-ът да има SD карта модул и да записва локално всичко, което
     получава от нодовете: измерванията, статус (онлайн/офлайн) и
     ниво на батерия - с времеви печат, подробно.
   - Причина: ThingSpeak/сайтът пазят само обобщени/по-редки данни;
     SD картата е пълен, детайлен локален архив (полезен за дебъг,
     дългосрочен анализ, или ако интернет връзката прекъсне).

9. **(много по-късно) Контролиране на дивайси, не само логване**
   - Пример: няколко сензора в стая измерват температура и чрез
     PID логика решават дали да увеличат/намалят парното (дивайс на
     парното, който управлява потока вода).
   - Общо взето - възможност за дефиниране на **причинно-следствени
     връзки и действия** ("ако X → направи Y"), не просто
     наблюдение. Това е естествено продължение на идея №7
     (автоматизации), но по-напреднало ниво (управление на
     изпълнителни устройства, PID регулация).

10. **Групиране на дивайси по стаи/зони**
    - Възможност дивайсите да се групират логически - "хол",
      "кухня", "спалня" и т.н. - вместо плосък списък.
    - Улеснява навигацията в сайта/приложението при много дивайси и
      е основа за автоматизации "по зона" (напр. правило само за
      "хол").

11. **Динамичен период на измерване на всеки node**
    - Периодът, на който всеки node мери и праща данни, да се
      настройва отдалечено (от hub-а/сайта/приложението), вместо да
      е фиксиран в кода.
    - Полезно за баланс точност/консумация - напр. по-рядко мерене
      през нощта или при нисък заряд на батерията.

12. **Бъдеща архитектура на комуникация с много node-ове (мащаб, discovery, low-power radio)**
    - Текущият модел (hub праща HTTP заявка, node отговаря) работи
      добре за 1-2 node-а, но при десетки node-а трябват: time slots
      за да не се "сблъскват" при отговор, ID базиран на hardware
      адреса (MAC) вместо ръчно закодиран, auto-discovery/join при
      пускане на нов node без preflash на конфигурация, генеричен
      протокол за различни типове сензори (не само BME280), конфигур.
      без препрограмиране, версии на конфигурацията, времева
      синхронизация, отдалечено сменяем период на измерване, и
      преминаване към нискоенергиен радио протокол (Zigbee/Thread/
      802.15.4) вместо Wi-Fi за батерийни node-ове.
    - Пълният, подробен анализ (с диаграми и стъпки на развитие
      Version 1 → Version 7) е записан по-долу в
      "Бъдеща архитектура на комуникация с node-ове (детайлно)".

## Реализирани

_(преместени тук след завършване, за справка)_

## Бъдеща архитектура на комуникация с node-ове (детайлно)

_Записано на английски, както е получено - подробен технически анализ
за идея №12 по-горе. Не е спешно за имплементация, а насока за когато
броят на node-овете нарасне значително._

# Future Node Communication Architecture

The current system uses a simple **Hub → Node request/response** model over Wi-Fi. This works well for a small number of nodes, but a larger system introduces several problems that should be considered for future versions.

The long-term goal is to support **many heterogeneous sensor nodes**, potentially battery-powered and capable of operating for long periods without user intervention.

---

## 1. Multiple Nodes and Collision Avoidance

With a small number of nodes, the Hub can communicate with nodes individually. With dozens of nodes, however, broadcasting a request and allowing every node to respond immediately can cause multiple nodes to transmit at the same time.

### Possible solution: time slots

Each node can have a unique logical ID.

After receiving a broadcast from the Hub, the node waits for a delay based on its ID:

```text
Hub → Broadcast: "Send measurements"

Node 1 → wait 100 ms → transmit
Node 2 → wait 200 ms → transmit
Node 3 → wait 300 ms → transmit
...
```

This is a simple form of **time-slotted communication**.

The delay could also be dynamically assigned by the Hub rather than being permanently derived from the node ID.

For example:

```text
Node 1 → slot 3
Node 2 → slot 7
Node 3 → slot 1
```

This allows the Hub to reorganize communication if nodes are added or removed.

### Problems to solve

* A missing node should not cause the entire cycle to stall.
* The system needs collision protection.
* Nodes should be able to retry failed transmissions.
* The Hub should know whether a node is offline or simply missed its slot.
* The number and duration of slots should scale with the number of nodes.

Possible mechanisms include:

* ACKs
* timeouts
* retransmissions
* randomized backoff
* dynamically assigned time slots

---

# 2. Node Identity

Node IDs should not be manually hard-coded into every firmware image.

Each ESP32 already has a unique hardware identity, such as its MAC/IEEE address.

A possible architecture is:

```text
Hardware ID
     ↓
Hub assigns logical Node ID
     ↓
Node stores configuration in non-volatile memory
```

For example:

```text
Hardware ID: AA:BB:CC:12:34:56
Node ID:     17
Name:        Living Room
```

The hardware identity remains permanent, while the logical ID can be assigned or changed by the Hub.

This also allows a node to be replaced without changing the rest of the system.

---

# 3. Automatic Node Registration / Discovery

A new node should be able to join the network without modifying and reflashing its firmware.

Possible sequence:

```text
New Node
   ↓
Discovery / Join request
   ↓
Hub
   ↓
Assign Node ID
   ↓
Send configuration
   ↓
Node stores configuration
   ↓
Normal operation
```

The node could also report its capabilities during registration.

Example:

```json
{
    "hardware_id": "AA:BB:CC:12:34:56",
    "sensors": [
        "temperature",
        "humidity",
        "pressure"
    ]
}
```

Another node might report:

```json
{
    "hardware_id": "AA:BB:CC:98:21:44",
    "sensors": [
        "illuminance",
        "uv"
    ]
}
```

This allows the Hub to automatically determine what measurements are available from each node.

---

# 4. Different Types of Nodes

The system should not assume that every node contains the same sensors.

Possible nodes:

```text
Node 1 → BME280
          temperature
          humidity
          pressure

Node 2 → Outdoor light sensor
          illuminance

Node 3 → UV sensor
          UV index

Node 4 → Gas sensors
          CO
          CO₂
          VOC

Node 5 → Relay / actuator
```

The communication protocol should therefore be **generic**.

Instead of defining messages such as:

```text
GET_BME280_DATA
```

the protocol should use more generic concepts:

```text
MEASURE
DATA
CONFIG
STATUS
PING
JOIN
ACK
ERROR
```

The node reports which capabilities it supports.

---

# 5. Configuration Without Reflashing

Network credentials and node configuration should not be compiled permanently into the firmware.

For example, the firmware should not require:

```cpp
const char* ssid = "...";
const char* password = "...";
```

for every individual node.

Instead, the node should support a provisioning process.

Possible first-boot flow:

```text
Node starts
   ↓
No configuration found
   ↓
Provisioning mode
   ↓
User provides network credentials
   ↓
Node stores credentials in NVS
   ↓
Normal operation
```

The Hub could also be responsible for distributing configuration to nodes.

This would make changing the Wi-Fi network or other configuration possible without physically reflashing every node.

---

# 6. Configuration Versions

Node configuration should have a version number.

Example:

```text
config_version = 42
```

The Hub can send:

```text
CONFIG_UPDATE
version = 43
```

The node stores the new configuration and acknowledges it.

This prevents ambiguity when a node reconnects after being offline.

---

# 7. Time Synchronization

For battery-powered nodes, synchronized time can be useful.

The Hub can periodically send:

```text
current_time
next_wakeup
sleep_period
communication_slot
configuration_version
```

For example:

```text
TIME = 18:00:00
PERIOD = 1800 s
SLOT = 7
CONFIG_VERSION = 42
```

The node can then calculate its future wake-up times locally.

It does not need to receive a command every time it should wake up.

Periodic synchronization can compensate for clock drift.

---

# 8. Dynamic Sleep Schedule

The Hub should ideally be able to change the measurement interval remotely.

For example:

```text
Current:
measurement interval = 60 minutes
```

The Hub changes it to:

```text
measurement interval = 30 minutes
```

The new configuration is sent to the node:

```text
CONFIG_UPDATE
period = 1800
```

The node then changes its sleep cycle without requiring a firmware update.

Possible future configuration:

```text
sleep_period
wake_time
measurement_interval
communication_slot
```

---

# 9. Battery-Powered Nodes

Battery-powered nodes introduce an important requirement:

**The radio should not remain active continuously.**

A possible cycle is:

```text
        ┌──────────────┐
        │     SLEEP    │
        └──────┬───────┘
               │
               ▼
             WAKE
               │
               ▼
        Read sensors
               │
               ▼
        Start radio
               │
               ▼
       Send measurements
               │
               ▼
       Receive commands
               │
               ▼
             SLEEP
```

The goal is to keep the radio active only when necessary.

Wi-Fi can technically be used with deep sleep, but reconnecting to Wi-Fi consumes significantly more energy than keeping the system asleep.

For long-term battery operation, alternative radio technologies should therefore be considered.

---

# 10. Possible Communication Technologies

Several technologies could be considered for future versions.

### Wi-Fi

Advantages:

* already supported by ESP32
* high bandwidth
* simple IP networking
* easy integration with existing infrastructure

Disadvantages:

* relatively high power consumption
* not ideal for frequently sleeping battery nodes
* network credentials need to be provisioned
* Wi-Fi infrastructure becomes a dependency

Good for:

```text
Hub
Mains-powered nodes
High-bandwidth devices
```

---

### BLE

Advantages:

* very low power
* supported by ESP32
* excellent for configuration and short-range communication

Disadvantages:

* less suitable as the main infrastructure for a large home sensor network
* topology and communication model may be less convenient for this architecture

Potential use:

```text
Phone ↔ Node
Configuration / provisioning
```

---

### IEEE 802.15.4

802.15.4 provides a low-power wireless MAC/PHY layer.

It is used as the radio foundation for technologies such as:

* Zigbee
* Thread

It is therefore an interesting foundation for a dedicated low-power sensor network.

---

### Zigbee

Zigbee is a complete protocol stack built on IEEE 802.15.4.

Important features include:

* low power operation
* mesh networking
* device addressing
* joining/rejoining
* security
* routers
* sleepy end devices
* device capabilities / clusters

A possible architecture:

```text
             HUB
          Coordinator
               │
       ┌───────┼───────┐
       │       │       │
     Node 1  Node 2  Node 3
       │
     Node 4
```

Nodes can communicate through other nodes when necessary, creating a mesh network.

Zigbee is particularly interesting for battery-powered sensors.

---

### Thread

Thread is another low-power mesh technology based on IEEE 802.15.4.

A major difference is that Thread uses IPv6 networking.

This makes the architecture more IP-oriented than Zigbee.

Thread is also used as a networking layer by Matter-based smart-home devices.

Thread should be considered as a possible long-term alternative to a custom radio protocol.

---

# 11. Offline / Rejoining Nodes

A node may be switched off for charging, maintenance, or battery replacement.

When it comes back online, it should not require reflashing.

Possible sequence:

```text
Node powers on
     ↓
Load stored configuration
     ↓
Search for Hub
     ↓
Rejoin network
     ↓
Synchronize time
     ↓
Check configuration version
     ↓
Update configuration if necessary
     ↓
Resume normal operation
```

The Hub should be able to recognize the node using its permanent hardware identity.

---

# 12. Manual Discovery / Recovery Mode

A manual discovery mode could be provided by the Hub.

For example:

```text
[ Search for new nodes ]
```

The Hub temporarily broadcasts discovery packets.

This can be useful when:

* installing a new node
* replacing a node
* recovering a node
* adding a node whose configuration has been lost

Nodes without valid configuration can listen for these broadcasts and enter the registration process.

---

# 13. Recommended Long-Term Architecture

The communication system should ideally be separated from the rest of the application.

Instead of the Web Server directly communicating with individual nodes:

```text
Web Server
    ↓
HTTP
    ↓
Node
```

use an abstraction layer:

```text
             Home Server
                  │
        ┌─────────┴─────────┐
        │                   │
    Node Manager        Web Server
        │
 Communication Layer
        │
 ┌──────┼───────────┐
 │      │           │
Wi-Fi  Zigbee     Thread
```

The rest of the application should not need to know which physical communication technology is being used.

This allows the communication mechanism to be replaced in the future without redesigning the entire Home Server.

---

# 14. Long-Term Goal

The eventual system could look like:

```text
                         HOME HUB
                    ┌────────────────┐
                    │ Node Manager   │
                    │ Web Interface  │
                    │ Database       │
                    │ Configuration  │
                    └───────┬────────┘
                            │
                     Low-power radio
                            │
              ┌─────────────┼─────────────┐
              │             │             │
           Node 1        Node 2        Node 3
           BME280       Outdoor UV     Gas sensor
              │             │             │
            sleep         sleep         sleep
```

The Hub should handle:

* node discovery
* identity
* configuration
* time synchronization
* scheduling
* communication
* acknowledgements
* retries
* node health/status
* firmware updates

while each node should primarily handle:

* sensor acquisition
* local processing
* low-power operation
* communication with the Hub

---

## Possible Development Path

The current Wi-Fi implementation can remain as the first prototype.

A possible evolution is:

```text
Version 1
Wi-Fi + HTTP
single/few nodes
        ↓
Version 2
Node IDs + discovery + ACK/retry
        ↓
Version 3
Generic node/capability system
        ↓
Version 4
Configuration provisioning
        ↓
Version 5
Time synchronization + scheduled communication
        ↓
Version 6
Deep-sleep battery nodes
        ↓
Version 7
Low-power radio
(Zigbee / Thread / custom 802.15.4)
```

The goal is not to implement all of this immediately. These are possible directions for the future architecture as the number of nodes and power requirements increase.

