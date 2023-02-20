#include "tcp_connection.hh"

#include <iostream>
// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::unclean_shutdown(bool rst_to_send /* = false */) {
    //  clear segments to send
    queue<TCPSegment> empty_1;
    queue<TCPSegment> empty_2;
    _sender.segments_out().swap(empty_1);
    _segments_out.swap(empty_2);

    //  whether active to send rst
    if (rst_to_send) {
        _sender.send_empty_segment(true);
        this->send_segments();
    }

    //  shutdown
    _active = false;
    _linger_after_streams_finish = false;
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
}

//  本端接收seg 并根据自身receiver以及sender状态 发送相应seg给peer
void TCPConnection::segment_received(const TCPSegment &seg) {
    _time_since_last_segment_received = 0;
    bool ack_to_send{false};

    if (seg.header().rst) {
        unclean_shutdown();
        return;
    }

    //  TCP keep-alive
    if (_receiver.ackno().has_value() && (seg.length_in_sequence_space() == 0) &&
        seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
        ack_to_send = true;
    }

    //  _receiver.segment_received is robust enough to deal with invalid seg
    //  receiver care about seqno, syn , payload, and fin .
    _receiver.segment_received(seg);

    //  if the incoming segment occupied any sequence numbers, the TCPConnection makes sure that at least one segment is
    //  sent in reply, to reflect an update in the ackno and window size.
    if (seg.length_in_sequence_space() > 0)  //  receiver recv syn , payload , fin
        ack_to_send = true;                  //  send empty segment with ack if can not shaodai

    //  if the ack flag is set, tells the TCPSender about the fields it cares about on incoming segments: ackno and
    //  window size. if TCPsender is CLOSED , then any ack is invalid, because that ack reflect the connection that
    //  local Sender发起. However , if local Sender is still CLOSED when ack received , it's illegal
    if (seg.header().ack && _sender.state() != TCPSender::State::CLOSED) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _sender.fill_window();  //  有可能ack_received中没fill到
    }

    //  syn local send syn as a passive peer
    if (_receiver.state() == TCPReceiver::State::SYN_RECV && _sender.state() == TCPSender::State::CLOSED) {
        connect();
        // ack_to_send = false;    //  ack已经和该syn segment一起在connect中发送,不必再发送一个空ack segment
        return;
    }

    //  clean shutdown 之 local passive close : CLOSE_WAIT
    if (_receiver.state() == TCPReceiver::State::FIN_RECV &&
        (_sender.state() == TCPSender::State::SYN_ACKED_2 || _sender.state() == TCPSender::State::SYN_ACKED_1))
        _linger_after_streams_finish = false;  //  false 标记 passive close

    //  unclean shutdown 之 local passive close .
    if (_receiver.state() == TCPReceiver::State::FIN_RECV && _sender.state() == TCPSender::State::FIN_ACKED &&
        !_linger_after_streams_finish) {
        clean_shutdown();
        return;
    }

    //  clean shutdown 之 local active close : TIME_WAIT
    // if (_receiver.state() == TCPReceiver::State::FIN_RECV && _sender.state() == TCPSender::State::FIN_ACKED &&
    //     _linger_after_streams_finish) {}

    //  if we need to send ack but it can't be 捎带
    if (ack_to_send && _sender.segments_out().empty())
        _sender.send_empty_segment();

    send_segments();
}

bool TCPConnection::active() const { return _active; }

//  upper user use the write() function to send the data from application layer
size_t TCPConnection::write(const string &data) {
    //  write into sender's outbound_stream
    size_t bytes_written = _sender.stream_in().write(data);
    //  send it over TCP if possible
    _sender.fill_window();
    send_segments();

    return bytes_written;
}

void TCPConnection::clean_shutdown() {
    _active = false;
    _linger_after_streams_finish = false;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    // tell the TCPSender about the passage of time
    _sender.tick(ms_since_last_tick);

    // if the number of consecutive retransmissions is more than an upper limit TCPConfig::MAX RETX ATTEMPTS , abort the
    // connection, and send a reset segment to the peer (an empty segment with the rst flag set)
    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
        //  how to abort the connection ? : 目前 send rst seg and set local stream error
        unclean_shutdown(true);  //  rst 1
        return;
    }

    //  因为tick可能会造成sender重传 故tcpconnection需要及时将segment从sender中取出发送
    send_segments();

    _time_since_last_segment_received += ms_since_last_tick;

    //  end the connection cleanly if passes the TIME_WAIT
    if (_receiver.state() == TCPReceiver::State::FIN_RECV && _sender.state() == TCPSender::State::FIN_ACKED &&
        _linger_after_streams_finish) {
        if (_time_since_last_segment_received >= 10 * _cfg.rt_timeout)
            clean_shutdown();
    }
}

//  shutdown TCPSender's outbound_stream
void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    //  send fin segment in time
    _sender.fill_window();
    send_segments();
}

void TCPConnection::connect() {
    //  send syn . TCPSender : CLOSED -> SYN_SENT
    _sender.fill_window();
    send_segments();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            // Your code here: need to send a RST segment to the peer
            unclean_shutdown(true);  //  rst 2
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

//  TCPConnection发送segment
//  move segment from _sender to tcpconnection
void TCPConnection::send_segments() {
    while (!_sender.segments_out().empty()) {
        //  segment
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        std::optional<WrappingInt32> ackno = _receiver.ackno();
        //  捎带ack
        if (ackno.has_value()) {
            seg.header().ack = true;
            seg.header().ackno = ackno.value();
            seg.header().win = _receiver.window_size();
        }
        //  捎带window_size
        seg.header().win = min(_receiver.window_size(), static_cast<size_t>(std::numeric_limits<uint16_t>::max()));
        //  会出现多个segment捎带同一ack.不过应该不影响正确性. ack已经ack过的报文，在receiver看来就是直接忽略即可
        _segments_out.push(seg);
    }
}