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
    if(state() == SYN_RECV && seg.header().seqno == _isn)
    {
        cout<<"data byte can't at isn"<<endl;
        return true;
    }
    return false;
}

//  合法seg : seg.header().syn || seg.header().fin || (seg.payload().size()!=0)
//  可处理非法seg
void TCPReceiver::segment_received(const TCPSegment &seg) {
    // cout<<"====================TCPReceiver segment_received start================"<<endl;
    //  0.  corner case such as invalid idx
    if(corner(seg))
        return ;
    //  1.  首次确立isn
    if(state() == LISTEN && seg.header().syn)
    {
        _isn = seg.header().seqno;
        // update_state();     
    }
    //  2.  如果还没初始化isn，且该报文不是syn报文
    if(state() == LISTEN)
        return ;
    //  3.  seq -> abs_seq -> stream_idx
    size_t abs_seq = unwrap(seg.header().seqno,_isn.value(),_reassembler.first_unassembled());   
    size_t stream_idx = abs_seq_to_stream_idx(abs_seq);

    // cout<<"seqno "<<seg.header().seqno<<" abs_seq "<<abs_seq<<" stream_idx "<<stream_idx<<" payload "<<seg.payload().str()<<" syn "<<seg.header().syn<<" fin "<<seg.header().fin<<endl;
    //  4.  push_substring(segment)
    //  这里可以看出 reassembler之中 payload占据空间 而不flag不占据空间
    _reassembler.push_substring(string(seg.payload().str()),stream_idx,seg.header().fin);   

    // update_state();         

    // cout<<"====================TCPReceiver segment_received end================"<<endl;

}

optional<WrappingInt32> TCPReceiver::ackno() const {
    //  还没初始化isn，也即还没建立连接，还没接收到第一个SYN报文 
    // cout<<"ackno state "<<_state<<endl;
    // cout<<"====================TCPReceiver ackno start================"<<endl;

    State st = state();
    if(st == LISTEN)
    {
        return std::nullopt;
    }
    //  +1 是为了第一个初始化syn报文 规定其占据一个字节的位置
    else if(st == SYN_RECV)
    {
        return wrap(_reassembler.first_unassembled() + 1,_isn.value());
    }
    //  +2 : syn + fin
    else if(st == FIN_RECV)
    {
        return wrap(_reassembler.first_unassembled() + 2,_isn.value());
    }
    else 
    {
        cout<<"receiver unknown _state" <<st<<endl;
        return wrap(_reassembler.first_unassembled() + 1,_isn.value());
    }

    // cout<<"====================TCPReceiver ackno end================"<<endl;

}


//  接收窗口大小,也即可以缓存的乱序字节大小
size_t TCPReceiver::window_size() const { 
    return _reassembler.window_size();
}


TCPReceiver::State TCPReceiver::state() const
{
    // cout<<"TCPReceiver State ";
    if(stream_out().error())
    {
        // cout<<states.at(ERROR)<<endl;
        return ERROR;
    }
    else if(listen())
    {
        // cout<<states.at(LISTEN)<<endl;
        return LISTEN;
    }
    else if(syn_recv())
    {
        // cout<<states.at(SYN_RECV)<<endl;
        return SYN_RECV;
    }
    else if(fin_recv())
    {
        // cout<<states.at(FIN_RECV)<<endl;
        return FIN_RECV;
    }
    else
    {
        // cout<<"keep state "<<" SYN_RECV"<<endl;
        return SYN_RECV;
    }
}

// //  注意及时调用update_state()
// void TCPReceiver::update_state()
// {
//     if(listen())
//     {
//         _state = LISTEN;
//     }
//     else if(syn_recv())
//     {
//         _state = SYN_RECV;
//     }
//     else if(fin_recv())
//     {
//         _state = FIN_RECV;
//     }
//     else 
//     {
//         cout<<"keep state " << _state<<" SYN_RECV"<<endl;
//     }
// }