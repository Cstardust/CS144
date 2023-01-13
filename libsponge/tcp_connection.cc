#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//  sender还有多少bytes没发送过 
size_t TCPConnection::remaining_outbound_capacity() const {
    return _sender.stream_in().buffer_size();
}

//  bytes sent but not acked
size_t TCPConnection::bytes_in_flight() const { 
    return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const { 
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const { return {}; }


void TCPConnection::unclean_shutdown()
{
    _active = false;
    _linger_after_streams_finish = false;
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
}

static void Assert(bool msg)
{
    if(!msg)
    {
        exit(-1);
    }
}

void TCPConnection::segment_received(const TCPSegment &seg) { 


    //  if the rst (reset) flag is set, 
    //  sets both the inbound and outbound streams to the error state 
    //  and kills the connection permanently
    if(seg.header().rst)
    {
        Assert(seg.payload().str().empty());
        unclean_shutdown();
        return ;
    }

    //  TCP keep-alive
        //  receive a keep-alive segment (ack + invalid seq)
    if (_receiver.ackno().has_value() 
        && (seg.length_in_sequence_space() == 0)  
        && seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
    }

    //  gives the segment to the TCPReceiver 
    //  so it can inspect the fields it cares about on incoming segments
    //  : seqno, syn , payload, and fin .
    //  _receiver.segment_received足够健壮, 可处理非法seg
    _receiver.segment_received(seg);


    //  if the incoming segment occupied any sequence numbers, 
    //  the TCPConnection makes sure that at least one segment is sent in reply, 
    //  to reflect an update in the ackno and window size.


    //  if the ack flag is set, 
    //  tells the TCPSender about the fields it cares about on incoming segments: 
    //  ackno and window size.
    if(seg.header().ack)
    {
        _sender.ack_received(seg.header().ackno,seg.header().win);
        _sender.fill_window();
    }

}

bool TCPConnection::active() const { return {}; }

size_t TCPConnection::write(const string &data) {
    //  write into sender's outbound_stream
    size_t bytes_written = _sender.stream_in().write(data);
    //  send it over TCP if possible
    _sender.fill_window();
    send_segments();
    return bytes_written;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { DUMMY_CODE(ms_since_last_tick); }

//  shutdown TCPSender's outbound_stream
void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
}

void TCPConnection::connect() {
    //  send syn
    //  TCPSender : CLOSED -> SYN_SENT
    _sender.fill_window();
    send_segments();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

//  TCPConnection发送segment
    //  将_sender的segment从其queue中全部放入connection的queue
    //  注意该segment是否可以捎带_receiver的ack
void TCPConnection::send_segments()
{
    while(!_sender.segments_out().empty())
    {
        //  segment
        TCPSegment seg = _sender.segments_out().front();        
        _sender.segments_out().pop();
        //  捎带ack
        optional<WrappingInt32> ackno = _receiver.ackno();
        if(ackno.has_value())
        {
            seg.header().ack = true;
            seg.header().ackno = ackno.value();
        }
        //  捎带window_size
        seg.header().win = min(_receiver.window_size(),std::numeric_limits<size_t>::max());
        //  可能会出现多个segment捎带同一ack.不过应该不影响正确性. ack已经ack过的报文，在receiver看来就是直接忽略即可
        _segments_out.push(seg);
    }
}