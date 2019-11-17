# gPTPd

IEEE 802.1 AS (gPTP) daemon

**Usage** `gPTPd [-n|-i<ifname>|-l<level>]`

* Normal mode (-n)
  
  * Runs the gPTP deamon in normal mode (used for testing and debugging)
  * By default the gPTPd runs as a deamon
  
* Interface name (-i<ifname>)

  * The name of the AVB enabled enthernet interface
  
* Log level (-l<level>)
  
  * Log level for debugging (default: LOG_NOTICE)
  
**Functionality**

* The gPTPd deamon is takes care of the following functionalites
   
   * Link delay measurement using Peer to peer delay measurement mechanism
   * Best Master Clock algorithm to select the best master in the network
   * Master and slave mode operation for time synchronization
   * Time synchronizartion using the PTP hardware clock

**License**

MIT License Copyright (c) [2018] [Indumathi Duraipandian]

