#include "tuntap_adapter.hh"

using namespace std;

//! \param[in] tap Raw network device that will be owned by the adapter
//! \param[in] eth_address Ethernet address (local address) of the adapter
//! \param[in] ip_address IP address (local address) of the adapter
//! \param[in] next_hop IP address of the next hop (typically a router or default gateway)
TCPOverIPv4OverEthernetAdapter::TCPOverIPv4OverEthernetAdapter(TapFD &&tap,
                                                               const EthernetAddress &eth_address,
                                                               const Address &ip_address,
                                                               const Address &next_hop)
    : _tap(move(tap)), _interface(eth_address, ip_address), _next_hop(next_hop) {
    // Linux seems to ignore the first frame sent on a TAP device, so send a dummy frame to prime the pump :-(
    EthernetFrame dummy_frame;
    _tap.write(dummy_frame.serialize());
}

optional<TCPSegment> TCPOverIPv4OverEthernetAdapter::read() {
    // Read Ethernet frame from the raw device
    EthernetFrame frame;
    //  网卡读数据的! : _tap.read()
    if (frame.parse(_tap.read()) != ParseResult::NoError) {
        return {};
    }

    // Give the frame to the NetworkInterface. Get back an Internet datagram if frame was carrying one.
    optional<InternetDatagram> ip_dgram = _interface.recv_frame(frame);

    // The incoming frame may have caused the NetworkInterface to send a frame.
    send_pending();

    // Try to interpret IPv4 datagram as TCP
    if (ip_dgram) {
        return unwrap_tcp_in_ip(ip_dgram.value());
    }
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPOverIPv4OverEthernetAdapter::tick(const size_t ms_since_last_tick) {
    _interface.tick(ms_since_last_tick);
    send_pending();
}

//! \param[in] seg the TCPSegment to send
void TCPOverIPv4OverEthernetAdapter::write(TCPSegment &seg) {
    //  wrap_tcp_in_ip : tcp seg 封装成 ip datagram
    //  send_datagram : ip datagram 封装成 frame
    _interface.send_datagram(wrap_tcp_in_ip(seg), _next_hop);
    send_pending();
}

void TCPOverIPv4OverEthernetAdapter::send_pending() {
    //  Frame逐个从TapFD发送出去
        //  tap设备特点 : TAP device接收上层构造好的链路层帧(link-layer frames)并直接发送出去
    while (not _interface.frames_out().empty()) {
        //  这个write最终还是从FileDescriptor封装的write发送出去
        _tap.write(_interface.frames_out().front().serialize());
        _interface.frames_out().pop();
    }
}

//! Specialize LossyFdAdapter to TCPOverIPv4OverTunFdAdapter
template class LossyFdAdapter<TCPOverIPv4OverTunFdAdapter>;
