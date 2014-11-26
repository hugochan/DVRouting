=====================================================================
                                                                       
                              ======                                   
                              README                                   
                              ======                                   
                                                                       
                             DVRouting 1.0
                             26 Nov. 2014
                             author: Hugo Chan
                          
                      C++ Programs for Socket Programming 
                 																			                                              
=====================================================================

Contents:
-----------
1. Description
2. Implementation
3. Operation Instruction
4. Notice



------------------------
1. Description:
------------------------

(1) A distance vector routing protocol that implements the distributed Bellman-Ford algorithm.
(2) Triggered update and poison reverse features are incorporated.
(3) The program runs on Windows.
(4) Each process simulates a router.

------------------------
2. Implementation:
------------------------
(1) An interactive console is implemented for interaction.
(2) Sub-thread is responsible to run a distance vector protocol.
(3) Router information (e.g., ID, hostname, portnum, sock, life_state and link_info) is maintained by corresponding router.
(4) Routing table is maintained by corresponding router.
(5) Timeout information for each link is maintained by corresponding router.

----------------------------
3. Operation Instruction
----------------------------
(1) Using cmd to run the exe file, and router ID & port number should be passed to it. (format: DVRouting RID portnum)
(2) For each router, using command: add RID IP portnum distance to add a link between current router and the specific router whose ID is RID.
(3) For each router, using command: change RID distance to change the link between current router and the specific router whose ID is RID.
(4) For each router, using command: display to print out its routing table.
(5) For each router, using command: kill to kill the router.
(6) Comment the #define _POISONREVERSE in source code DVRouting.cpp to cancel the poison reverse feature.

----------------------------
4. Notice
----------------------------
(1) Current version only supports localhost (loopback) IP address.