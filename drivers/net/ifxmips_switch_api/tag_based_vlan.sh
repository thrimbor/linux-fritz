#!/bin/sh

#ifndef CONFIG_PPA

##Enable the switch
switch_utility Enable
 
LAN0_VID=10
LAN1_VID=11
LAN2_VID=12
LAN3_VID=13
LAN4_VID=14
LAN0_PORT=0
LAN1_PORT=1
LAN2_PORT=2
LAN3_PORT=4
LAN4_PORT=5
CPU_PORT=6

switch_utility RegisterSet 0xCCD 0x00

## Enable Vlan Capability
switch_utility RegisterSet 0x456 0x4008

## Remove Special Header for Ingress Only (Bit3)
##switch_utility RegisterSet $((0xc40 + 0x82)) 0x744

## Remove Special Header for Ingress and Egress (Bit3 u. Bit6)
switch_utility RegisterSet $((0xc40 + 0x82)) 0x704

for argument in 0 1 2 3 4 5 6;do
  switch_utility PortCfgSet $argument 1 0 0 0 0 0 1 255 0 0
done
switch_utility VLAN_IdCreate ${LAN0_VID} 1
switch_utility VLAN_IdCreate ${LAN1_VID} 2 
switch_utility VLAN_IdCreate ${LAN2_VID} 3
switch_utility VLAN_IdCreate ${LAN3_VID} 4
switch_utility VLAN_IdCreate ${LAN4_VID} 5



## Port VLAN Configuration 
## - argument: Port ID
## - 50: port VID
## - 0: VLAN Unknow Drop
## - 0: VLAN ReAssign
## - 3: Violation Mode Both
## - 0: Admit All
## - 1: TVM enabled
switch_utility VLAN_PortCfgSet ${LAN0_PORT} ${LAN0_VID} 1 1 3 0 1
switch_utility VLAN_PortCfgSet ${LAN1_PORT} ${LAN1_VID} 1 1 3 0 1
switch_utility VLAN_PortCfgSet ${LAN2_PORT} ${LAN2_VID} 1 1 3 0 1
switch_utility VLAN_PortCfgSet ${LAN3_PORT} ${LAN3_VID} 1 1 3 0 1
switch_utility VLAN_PortCfgSet ${LAN4_PORT} ${LAN4_VID} 1 1 3 0 1
switch_utility VLAN_PortCfgSet ${CPU_PORT}  ${LAN0_VID} 0 0 0 0 0

## PortMemberAdd
## - VLAN ID
## - argument: Port ID
## - 0: Tag base Number Egress disabled
switch_utility VLAN_PortMemberAdd ${LAN0_VID} ${LAN0_PORT} 0
switch_utility VLAN_PortMemberAdd ${LAN1_VID} ${LAN1_PORT} 0
switch_utility VLAN_PortMemberAdd ${LAN2_VID} ${LAN2_PORT} 0
switch_utility VLAN_PortMemberAdd ${LAN3_VID} ${LAN3_PORT} 0
switch_utility VLAN_PortMemberAdd ${LAN4_VID} ${LAN4_PORT} 0

switch_utility VLAN_PortMemberAdd ${LAN0_VID} ${CPU_PORT} 1
switch_utility VLAN_PortMemberAdd ${LAN1_VID} ${CPU_PORT} 1
switch_utility VLAN_PortMemberAdd ${LAN2_VID} ${CPU_PORT} 1
switch_utility VLAN_PortMemberAdd ${LAN3_VID} ${CPU_PORT} 1
switch_utility VLAN_PortMemberAdd ${LAN4_VID} ${CPU_PORT} 1


#############################################
##ifconfig eth0 0.0.0.0 up
##ifconfig eth1 0.0.0.0 up
##
##vconfig add eth0 ${LAN0_VID}
##vconfig add eth0 ${LAN1_VID}
##vconfig add eth0 ${LAN2_VID}
##vconfig add eth0 ${LAN3_VID}
##
##ifconfig eth0.${LAN0_VID} 192.168.178.1 up
##ifconfig eth0.${LAN1_VID} 192.168.1.1 up
#############################################

##ifconfig eth0 192.168.178.1 up
##ifconfig eth1 192.168.1.1 up

#endif

