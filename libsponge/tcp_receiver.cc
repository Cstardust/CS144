#include "tcp_receiver.hh"
#include <iostream>

using std::cout;
using std::endl;

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;


bool TCPReceiver::corner(const TCPSegment & seg) const
{
    // A byte with invalid stream index should be ignored
    if(_state == SYN_RECV && seg.header().seqno == _isn)
    {
        cout<<"data byte can't at isn"<<endl;
        return true;
    }
    return false;
}

void TCPReceiver::segment_received(const TCPSegment &seg) {
    //  0.  corner case such as invalid idx
    if(corner(seg))
        return ;
    //  1.  首次确立isn
    if(_state == LISTENING && seg.header().syn)
    {
        _isn = seg.header().seqno;
        update_state();     
    }
    //  2.  如果还没初始化isn，且该报文不是syn报文
    if(_state == LISTENING)
        return ;
    //  3.  seq -> abs_seq -> stream_idx
    size_t abs_seq = unwrap(seg.header().seqno,_isn.value(),_reassembler.first_unassembled());   
    size_t stream_idx = abs_seq_to_stream_idx(abs_seq);

    cout<<seg.header().seqno<<" "<<abs_seq<<" "<<stream_idx<<" "<<seg.payload().str()<<endl;
    //  4.  push_substring(segment)
    _reassembler.push_substring(string(seg.payload().str()),stream_idx,seg.header().fin);   

    update_state();         
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    //  还没初始化isn，也即还没建立连接，还没接收到第一个SYN报文 
    // cout<<"ackno state "<<_state<<endl;
    if(_state == LISTENING)
    {
        return std::nullopt;
    }
    //  +1 是为了第一个初始化syn报文 规定其占据一个字节的位置
    else if(_state == SYN_RECV)
    {
        return wrap(_reassembler.first_unassembled() + 1,_isn.value());
    }
    //  +2 : syn + fin
    else if(_state == FIN_RECV)
    {
        return wrap(_reassembler.first_unassembled() + 2,_isn.value());
    }
    else 
    {
        cout<<"unknown _state" <<_state<<endl;
        return wrap(_reassembler.first_unassembled() + 1,_isn.value());
    }
}



size_t TCPReceiver::window_size() const { 
    return _reassembler.window_size();
}


//  注意及时调用update_state()
void TCPReceiver::update_state()
{
    if(listening())
    {
        _state = LISTENING;
    }
    else if(syn_recv())
    {
        _state = SYN_RECV;
    }
    else if(fin_recv())
    {
        _state = FIN_RECV;
    }
    else 
    {
        cout<<"unknown state , may be established"<<endl;
    }
}