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

//  什么时候变成fin_recevied ? 
//  segment_received收到了fin flag就变了吗？
//  不用考虑只放在window但是没加入byte_stream的情况吗？
    //  不是
    //  需要考虑

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // DUMMY_CODE(seg);
    // A byte with invalid stream index should be ignored
        //  报文起始字节和syn起始空位置字节相同 , 即data byte at isn
    if(_state == SYN_RECV && seg.header().seqno == _isn)
    {
        cout<<"data byte can't at isn"<<endl;
        return ;
    }
    //  首次确立isn
    if(!_isn.has_value() && seg.header().syn)
    {
        _isn = seg.header().seqno;
        //  根据我目前所学的 tcp三次握手的第一次 只有syn建立isn，而不携带应用层数据 
            //  t_recv_special中打破了该认知。对于第一个SYN报文，其也可能携带应用层数据
        assert(seg.payload().size() == 0);
        // update_state();     //  更新状态
        // return ;             不能return 因为可能同时有fin    recv_connect中有该case
    }
    //  如果还没初始化isn，也即还没接收第一个syn报文,且该报文不是syn报文
    if(!_isn.has_value())   
        return ;

    //  seq -> abs_seq
    size_t abs_seq = unwrap(seg.header().seqno,_isn.value(),_reassembler.first_unassembled());   //  选用_reassembler.first_unassembled()作为check_point
    //  abs_seq -> stream_idx
    size_t stream_idx = 0;   //  对于最一开始的syn报文 其携带的seq 不是 正式应用数据报文的seq 那么如果携带了数据 对于该写动作，应用报文的stream_idx也不能通过abs_seq_to_streamidx计算 应当人为规定为0
    if(abs_seq != 0)
        stream_idx = abs_seq_to_stream_idx(abs_seq);

    cout<<seg.header().seqno<<" "<<abs_seq<<" "<<stream_idx<<" "<<seg.payload().str()<<endl;

    _reassembler.push_substring(string(seg.payload().str()),stream_idx,seg.header().fin);   //  seg.payload().str().data() 不可以！因为data返回的是个char*指针。会导致string中的'\0'被截断

    update_state();         //  更新状态
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    //  还没初始化isn，也即还没建立连接，还没接收到第一个SYN报文 
    // if(! _isn.has_value())
    //     return std::nullopt;
    cout<<"ackno state "<<_state<<endl;
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
        cout<<"_state " <<_state<< "state may be established"<<endl;
        return wrap(_reassembler.first_unassembled() + 1,_isn.value());
    }
}



size_t TCPReceiver::window_size() const { 
    return _reassembler.window_size();
}



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