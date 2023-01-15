#include "tcp_receiver.hh"

#include <iostream>

using std::cout;
using std::endl;

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

bool TCPReceiver::corner(const TCPSegment &seg) const {
    // A byte with invalid stream index should be ignored
    // data byte can't at isn
    if (state() == SYN_RECV && seg.header().seqno == _isn)
        return true;

    return false;
}

//  合法seg : seg.header().syn || seg.header().fin || (seg.payload().size()!=0)
//  可处理非法seg
void TCPReceiver::segment_received(const TCPSegment &seg) {
    //  0.  corner case such as invalid idx
    if (corner(seg))
        return;
    //  1.  首次确立isn
    if (state() == LISTEN && seg.header().syn)
        _isn = seg.header().seqno;
    //  2.  如果还没初始化isn，且该报文不是syn报文
    if (state() == LISTEN)
        return;
    //  3.  seq -> abs_seq -> stream_idx
    size_t abs_seq = unwrap(seg.header().seqno, _isn.value(), _reassembler.first_unassembled());
    size_t stream_idx = abs_seq_to_stream_idx(abs_seq);

    //  4.  push_substring(segment)
    //  这里可以看出 reassembler之中 payload占据空间 而不flag不占据空间
    _reassembler.push_substring(string(seg.payload().str()), stream_idx, seg.header().fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    State st = state();
    //  还没初始化isn，也即还没建立连接，还没接收到第一个SYN报文
    if (st == LISTEN) {
        return std::nullopt;
    }
    //  +1 是为了第一个初始化syn报文 规定其占据一个字节的位置
    else if (st == SYN_RECV) {
        return wrap(_reassembler.first_unassembled() + 1, _isn.value());
    }
    //  +2 : syn + fin
    else if (st == FIN_RECV) {
        return wrap(_reassembler.first_unassembled() + 2, _isn.value());
    } else {
        //  receiver unknown _state
        return wrap(_reassembler.first_unassembled() + 1, _isn.value());
    }
}

//  接收窗口大小,也即可以缓存的乱序字节大小
size_t TCPReceiver::window_size() const { return _reassembler.window_size(); }

TCPReceiver::State TCPReceiver::state() const {
    if (stream_out().error()) {
        return ERROR;
    } else if (listen()) {
        return LISTEN;
    } else if (syn_recv()) {
        return SYN_RECV;
    } else if (fin_recv()) {
        return FIN_RECV;
    } else {
        //  sth unknown happened
        return State::ERROR;
    }
}
