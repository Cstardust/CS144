#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
//  next_hop : 将要发送到的下一跳的IP地址. 一般是路由器或者网关.
//  但也有可能是目标主机(如果目标主机和本主机位于同一局域网的话)

// This method is called when the caller (e.g., your TCPConnection or a router) wants to send an outbound Internet (IP)
// datagram to the next hop.1 It’s your interface’s job to translate this datagram into an Ethernet frame and
// (eventually) send it.
//  If the destination Ethernet address is already known,
//  send it right away.
//  Create an Ethernet frame (with type = EthernetHeader::TYPE IPv4), set the payload to be the serialized datagram, and
//  set the source and destination addresses
//  If the destination Ethernet address is unknown,
//  broadcast an ARP request for the next hop’s Ethernet address, and queue the IP datagram so it can be sent after the
//  ARP reply is received.
// You don’t want to flood the network with ARP requests. If the network interface already sent an ARP request about the
// same IP address in the last five seconds, don’t send a second request—just wait for a reply to the first one. Again,
// queue the datagram until you learn the destination Ethernet address
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    std::unordered_map<uint32_t, MacAddrInfo>::iterator iter = _arp_table.find(next_hop_ip);
    if (iter != _arp_table.end()) {
        const MacAddrInfo &mac = iter->second;
        //  mac addr is expired ; abort sending this segment.
        if (mac.second <= 0) {
            cerr << "never happened" << endl;
            return;
        }

        EthernetFrame ethernet_frame =
            buildEthernetFrame(mac.first, _ethernet_address, EthernetHeader::TYPE_IPv4, dgram.serialize());
        _frames_out.push(std::move(ethernet_frame));

    } else {
        //   If the network interface already sent an ARP request about the same IP address in the last five seconds,
        //   don’t send a second request—just wait for a reply to the first one
        if (_wait_for_req[next_hop_ip] <= 0) {
            //  build an ARP request into EthernetFrame
            ARPMessage arp = buildArpRequest(next_hop_ip);
            EthernetFrame ethernet_frame =
                buildEthernetFrame(ETHERNET_BROADCAST, _ethernet_address, EthernetHeader::TYPE_ARP, arp.serialize());
            //  send arp req
            _frames_out.push(std::move(ethernet_frame));
            //  waiting reply for 5 s
            _wait_for_req[next_hop_ip] = WAITING_TIME;
        }

        //  queue the dgram
        _data_buffer[next_hop_ip].push_back(dgram);
    }
}

bool NetworkInterface::checkInValidFrame(const EthernetFrame &frame) {
    return frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST;
}

// This method is called when an Ethernet frame arrives from the network.
// The code should ignore any frames not destined for the network interface
//  (meaning, the Ethernet destination is either the broadcast address or the interface’s own Ethernet address stored in
//  the ethernet address member variable).
// If the inbound frame is IPv4, parse the payload as an InternetDatagram and,
//  if successful (meaning the parse() method returned ParseResult::NoError),
//  return the resulting InternetDatagram to the caller.
//  If the inbound frame is ARP, parse the payload as an ARPMessage and,
//  if successful,
//  remember the mapping between the sender’s IP address and Ethernet address for 30 seconds. (Learn mappings from both
//  requests and replies.)
//  In addition, if it’s an ARP request asking for our IP address, send an appropriate ARP reply
//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    // The code should ignore any frames not destined for the network interface
    //  the Ethernet destination is either the broadcast address or the interface’s own Ethernet address
    if (checkInValidFrame(frame)) {
        // cerr<<"not belong to us"<<endl;
        return {};
    }

    InternetDatagram ipv4_data;
    //  recv IPv4
    if (ipv4_data.parse(frame.payload()) == ParseResult::NoError) {
        return ipv4_data;
    }
    //  recv ARP
    else {
        ARPMessage arp;
        if (arp.parse(frame.payload()) != ParseResult::NoError) {
            cerr << " Not ipv4 or Arp " << endl;
            return {};
        }
        //  remember the mapping between the sender’s IP address and Ethernet address for 30 seconds. (Learn mappings
        //  from both requests and replies.)
        _arp_table[arp.sender_ip_address] = make_pair(arp.sender_ethernet_address, TTL);
        //  send waiting datagrams
        updateDataBuffer(arp.sender_ip_address);

        //  In addition, if it’s an ARP request asking for our IP address, send an appropriate ARP reply
        if (arp.opcode == ARPMessage::OPCODE_REQUEST) {
            //  ARP_REQ广播并非指向本节点
            if (_ip_address.ipv4_numeric() != arp.target_ip_address)
                return {};
            //  build an ARP reply into EthernetFrame
            ARPMessage reply = buildArpReply(arp.sender_ethernet_address, arp.sender_ip_address);
            EthernetFrame ethernet_frame = buildEthernetFrame(
                arp.sender_ethernet_address, _ethernet_address, EthernetHeader::TYPE_ARP, reply.serialize());
            //  send an ARP reply
            _frames_out.push(std::move(ethernet_frame));
        }
    }

    return {};
}

//  已经获取到ip对应mac，故将之前等待的datagram全部发送
void NetworkInterface::updateDataBuffer(uint32_t ip) {
    //  no datagram to ip is waiting
    if (_data_buffer[ip].size() == 0)
        return;
    vector<InternetDatagram> dgrams;
    dgrams.swap(_data_buffer[ip]);
    for (auto &dgram : dgrams) {
        send_datagram(dgram, Address(std::to_string(ip)));
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    DUMMY_CODE(ms_since_last_tick);
    for (std::unordered_map<uint32_t, MacAddrInfo>::iterator iter = _arp_table.begin(); iter != _arp_table.end();) {
        MacAddrInfo &mac = iter->second;
        mac.second -= ms_since_last_tick;
        if (mac.second <= 0)
        //  erase if this ip-mac entry expired
        {
            _arp_table.erase(iter++);
        } else {
            ++iter;
        }
    }

    for (auto &waiting_item : _wait_for_req) {
        if (waiting_item.second <= 0)
            continue;
        waiting_item.second -= ms_since_last_tick;
    }
}

//  构造ARP查询分组
ARPMessage NetworkInterface::buildArpRequest(uint32_t target_ip_address) {
    ARPMessage req;                           //  arp查询分组
    req.opcode = ARPMessage::OPCODE_REQUEST;  //  arp request
    req.sender_ethernet_address = _ethernet_address;
    req.sender_ip_address = _ip_address.ipv4_numeric();
    req.target_ethernet_address = EthernetAddress{};  //  00:00:00:00:00:00why ?
    req.target_ip_address = target_ip_address;
    return req;
}

//  构造ARP响应分组
ARPMessage NetworkInterface::buildArpReply(EthernetAddress target_ethernet_address, uint32_t target_ip_address) {
    ARPMessage reply;                         //  arp响应分组
    reply.opcode = ARPMessage::OPCODE_REPLY;  //  arp reply
    reply.sender_ethernet_address = _ethernet_address;
    reply.sender_ip_address = _ip_address.ipv4_numeric();
    reply.target_ethernet_address = target_ethernet_address;
    reply.target_ip_address = target_ip_address;
    return reply;
}

//  build an IPv4Datagram into EthernetFrame
EthernetFrame NetworkInterface::buildEthernetFrame(EthernetAddress dst_mac_addr,
                                                   EthernetAddress src_mac_addr,
                                                   uint16_t type,
                                                   const BufferList &load) {
    EthernetFrame ethernet_frame;
    ethernet_frame.header().dst = dst_mac_addr;  //  dst mac addr
    ethernet_frame.header().src = src_mac_addr;  //  src mac addr
    ethernet_frame.header().type = type;         //  IPv4 / ARP
    ethernet_frame.payload().append(load);
    return ethernet_frame;
}
