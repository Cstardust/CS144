#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";
    //  next_hop是位于route_prefix + prefix_length中的ip 应该是
    _forwarding_table.emplace_back(route_prefix, prefix_length, next_hop, interface_num);
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {

    uint32_t dgram_dst_ip = dgram.header().dst;
    size_t matched_entry_idx = _forwarding_table.size();
    for (size_t i = 0; i < _forwarding_table.size(); ++i) {
        auto &entry = _forwarding_table[i];
        //  entry.route_prefix ^ dgram_dst_ip : 尝试匹配.
        //  >> (32 - entry.prefix_length) : 将非前缀去掉
        //  >> 时高位补0 , 因为unsigned
        if ((((entry.route_prefix ^ dgram_dst_ip) >> (32 - entry.prefix_length)) == 0) || entry.route_prefix == 0) {
            if (matched_entry_idx == _forwarding_table.size() ||
                _forwarding_table[matched_entry_idx].prefix_length < entry.prefix_length) {
                matched_entry_idx = i;
            }
        }
    }

    // If no routes matched, the router drops the datagram
    if (matched_entry_idx == _forwarding_table.size())
        return;

    //  --ttl
    //  If the TTL was zero already, or hits zero after the decrement, the router should drop the datagrams
    if (dgram.header().ttl == 0 || --dgram.header().ttl == 0)
        return;

    //  send dgram
    auto &entry = _forwarding_table[matched_entry_idx];

    //  entry.next_hop != nullopt , 则将entry记录的下一跳作为下一跳告知interface card
    //  if the router is connected to the network in question through some other router,
    //  the next hop will contain the IP address of the next router along the path.
    //  entry.next_hop == nullopt ,
    //  if the router is directly attached to the network in question,
    //  the next hop will be an empty optional.
    //  In that case, the next hop is the datagram’s destination address
    interface(entry.interface_num)
        .send_datagram(dgram, entry.next_hop.value_or(Address::from_ipv4_numeric(dgram.header().dst)));
}

//  route :
//  linker-layer -> ip -> linker-layer
void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
