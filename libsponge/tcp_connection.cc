#include "tcp_connection.hh"

#include <iostream>

//  关于 rst
//  什么时候发送rst
// When should I send a segment with the rst flag set ?
// There are two situations where you’ll want to abort the entire connection:
// 1. If the sender has sent too many consecutive retransmissions without success (more than TCPConfig::MAX RETX
// ATTEMPTS, i.e., 8).
// 2. If the TCPConnection destructor is called while the connection is still active (active() returns true).
//  rst作用(接收和发送rst)
//  Sending a segment with rst set has a similar effect to receiving one:
//  the connection is dead and no longer active(), and both ByteStreams should be set to the error state
//  如何发送rst
//  Wait, but how do I even make a segment that I can set the rst flag on? What’s the sequence number?
//  Any outgoing segment needs to have the proper sequence number.
//  You can force the TCPSender to generate an empty segment with the proper sequence number by calling its send empty
//  segment() method. Or you can make it fill the window (generating segments if it has outstanding information to send,
//  e.g. bytes from the stream or SYN/FIN) by calling its fill window() method.

//  关于ack
//  What’s the purpose of the ack flag? Isn’t there always an ackno?
//  Almost always
//  Almost every TCPSegment has an ackno, and has the ack flag set.
//  The exceptions are just at the very beginning of the connection, before the receiver has anything to acknowledge.
//  Purpose : send to peer sender
//  On outgoing segments, you’ll want to set the ackno and the ack flag whenever possible. That is, whenever the
//  TCPReceiver’s ackno() method returns a std::optional<WrappingInt32> that has a value, which you can test with has
//  value(). On incoming segments, you’ll want to look at the ackno only if the ack field is set. If so, give that ackno
//  (and window size) to the TCPSender.

//  关于State
//  How do I decipher these “state” names (like “stream started” or “stream ongoing”)?
//  Please see the diagrams in the Lab 2 and Lab 3 handouts.
//  We want to emphasize again that the “states” are useful for testing and debugging,
//  but we’re not asking you to materialize these states in your code. You don’t need to make more state variables to
//  keep track of this. The “state” is just a function of the public interface your modules are already exposing.

//  TCPConnection结束 unclean + clean
//  One important function of the TCPConnection is to decide when the TCP connection is fully “done.”
//  When TCP connection is fully “done.” ,
//  the implementation releases its exclusive claim to a local port number,
//  stops sending acknowledgments in reply to incoming segments,
//  considers the connection to be history,
//  and has its active() method return false

//  In an unclean shutdown, the TCPConnection either sends or receives a segment with the rst flag set.
//  In this case, the outbound and inbound ByteStreams should both be in the error state, and active() can return false
//  immediately.

//  A clean shutdown is how we get to “done” (active() = false) without an error.
//  This is more complicated, but it’s a beautiful thing because it ensures as much as possible that each of the two
//  ByteStreams has been reliably delivered completely to the receiving peer. In the next section (§§5.1), we give the
//  practical upshot for when a clean shutdown happens, so feel free to skip ahead if you like Cool, you’re still here.
//  Because of the Two Generals Problem, it’s impossible to guarantee that both peers can achieve a clean shutdown, but
//  TCP gets pretty close. Here’s how. From the perspective of one peer (one TCPConnection, which we’ll call the “local”
//  peer), there are four prerequisites to having a clean shutdown in its connection with the “remote” peer:

// Prereq #1 The inbound stream has been fully assembled and has ended.             (I think)TCPReceiver :: FIN_RECV
// Prereq #2 The outbound stream has been ended by the local application and fully sent (including the fact that it
// ended, i.e. a segment with fin ) to the remote peer.    (I think)TCPSender :: FIN_SENT Prereq #3 The outbound stream
// has been fully acknowledged by the remote peer.    (I think)TCPSender :: FIN_ACKED Prereq #4 The local TCPConnection
// is confident that the remote peer can satisfy prerequisite #3.
//  This is the brain-bending part. There are two alternative ways this can happen

//  Pratical Summary for Clean Shutdown
//  Practically what all this means is that your TCPConnection has a member variable called linger_after_streams_finish,
//  exposed to the testing apparatus through the state() method. The variable starts out true. If the inbound stream
//  ends before the TCPConnection has reached EOF on its outbound stream, this variable needs to be set to false.
//  如果本端是被动关闭tcp连接的一方(发送起fin的一方),那么 linger_after_streams_finish = false
//  At any point where prerequisites #1 through #3 are satisfied,
//  当本端满足先决条件1 && 2 && 3(TCPReceiver :: FIN_RECV && TCPSender :: FIN_ACKED)
//  if linger_after_streams_finish is false , the connection is “done” (and active() should return false) .
//  如果linger_after_streams_finish = false , 那么 本连接立刻结束
//  Otherwise you need to linger: the connection is only done after enough time (10 × cfg.rt timeout) has elapsed since
//  the last segment was received 如果linger_after_streams_finish = true , 那么 本连接不结束，而是进入TIME_WAIT

//  关于linger_after_streams_finish（感觉也可以叫 whether_time_wait）

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

//  bytes sent but not acked
size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

//  Ques : 是否应该将segment_out中的报文全部清空 ? 然后直接发送 rst segment ?
//  还是先将segment_out中的报文发送出去 然后再 rst ?
void TCPConnection::unclean_shutdown(bool rst_to_send /* = false */) {
    queue<TCPSegment> empty;
    _sender.segments_out().swap(empty);

    //  是否是主动发送rst的一方
    if (rst_to_send) {
        _sender.send_empty_segment(true);
        this->send_segments();
        //  rst seg之前还会有一些未发送出去的普通segment. 目前是需要先将那些segment发送出去之后再发送rst segment
    }
    _active = false;
    _linger_after_streams_finish = false;
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    _time_since_last_segment_received = 0;
    bool ack_to_send{false};

    //  if the rst (reset) flag is set,
    //  sets both the inbound and outbound streams to the error state
    //  and kills the connection permanently
    if (seg.header().rst) {
        unclean_shutdown();
        return;
    }

    //  TCP keep-alive
    //  receive a keep-alive segment (ack + invalid seq)
    //  There is one extra special case that you will have to handle in the TCPConnection’s segment received() method:
    //  responding to a “keep-alive” segment.
    //  The peer may choose to send a segment with an invalid sequence number to see if your TCP implementation is still
    //  alive (and if so, what your current window is). Your TCPConnection should reply to these “keep-alives” even
    //  though they do not occupy any sequence numbers. Code to implement this can look like this
    if (_receiver.ackno().has_value() && (seg.length_in_sequence_space() == 0) &&
        seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
        ack_to_send = true;
    }

    //  gives the segment to the TCPReceiver
    //  so it can inspect the fields it cares about on incoming segments
    //  : seqno, syn , payload, and fin .
    //  _receiver.segment_received足够健壮, 可处理非法seg
    _receiver.segment_received(seg);

    //  if the incoming segment occupied any sequence numbers,
    //  the TCPConnection makes sure that at least one segment is sent in reply,
    //  to reflect an update in the ackno and window size.
    //  可处理发送空ack segment的情况
    if (seg.length_in_sequence_space() > 0)  //  receiver recv syn , payload , fin
        ack_to_send = true;

    //  if the ack flag is set,
    //  tells the TCPSender about the fields it cares about on incoming segments:
    //  ackno and window size.
    //  如果本端的TCPConnection处于LISTEN状态，则本端收到的任何ack都被忽略
    //  即如果TCPsender处于CLOSED,那么其接收到的任何ack报文都是无用的。因为ack报文反映的是本端Sender发起的连接，而本端Sender仍处于CLOSED状态，故不可能.
    if (seg.header().ack && _sender.state() != TCPSender::State::CLOSED) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _sender.fill_window();  //  有可能ack_received中没fill到
    }

    //  如果本端TCPConnection刚接收到SYN 那么易知其要发送syn作为返回TCPConnection CLOSED -> SYN_SENT : TCPSender send
    //  syn local send syn as a passive peer
    if (_receiver.state() == TCPReceiver::State::SYN_RECV && _sender.state() == TCPSender::State::CLOSED) {
        connect();
        // ack_to_send = false;    //  ack已经和该syn segment一起在connect中发送,不必再发送一个空ack segment
        return;
    }

    //  我目前令当TCPConnection处于ESTABLISHED状态时 active = true;
    //  根据 TCPState来看 TCPConnection从一开始就视为active
    // if((_sender.state() == TCPSender::State::SYN_ACKED_1 || _sender.state() == TCPSender::State::SYN_ACKED_2)
    //     && _receiver.state() == TCPReceiver::State::SYN_RECV)
    // {
    //     // _active = true;
    // }
    //  断开连接
    //  clean shutdown 之 本端 被动断开连接 : 必然会经历关闭连接的第一个状态 : CLOSE_WAIT
    //  TCPConnection should be CLOSE_WAIT now
    if (_receiver.state() == TCPReceiver::State::FIN_RECV &&
        (_sender.state() == TCPSender::State::SYN_ACKED_2 || _sender.state() == TCPSender::State::SYN_ACKED_1))
        _linger_after_streams_finish = false;

    //  unclean shutdown 之 本端 主动断开连接 . 虽然不用linger , 但也不发送rst报文 , 因为这是正常clean
    //  shutdown的一部分。是clean_shutdown中被动关闭的一方
    // _linger_after_streams_finish都有什么时候会改变 ?
    if (_receiver.state() == TCPReceiver::State::FIN_RECV && _sender.state() == TCPSender::State::FIN_ACKED &&
        !_linger_after_streams_finish) {
        clean_shutdown();
        return;
    }

    //  clean shutdown 之 本端 主动关闭连接 进入 TIME_WAIT
    //  come into TIME_WAIT!
    if (_receiver.state() == TCPReceiver::State::FIN_RECV && _sender.state() == TCPSender::State::FIN_ACKED &&
        _linger_after_streams_finish) {
    }

    //  如果TCPReceiver 需要发送ack segment 且无法被捎带(TCPSender没有发送segment),那么单独发送一个空的ack segment
    if (ack_to_send && _sender.segments_out().empty())
        _sender.send_empty_segment();

    send_segments();
}

bool TCPConnection::active() const { return _active; }

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

// When time passes. The TCPConnection has a tick method that will be called periodically by the operating system. When
// this happens, the TCPConnection needs to • tell the TCPSender about the passage of time. • if the number of
// consecutive retransmissions is more than an upper limit TCPConfig::MAX RETX ATTEMPTS
//  abort the connection, and send a reset segment to the peer (an empty segment with the rst flag set)
// • end the connection cleanly if necessary (please see Section 5).
//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    // tell the TCPSender about the passage of time
    _sender.tick(ms_since_last_tick);

    // if the number of consecutive retransmissions is more than an upper limit TCPConfig::MAX RETX ATTEMPTS
    //  abort the connection, and send a reset segment to the peer (an empty segment with the rst flag set)
    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
        //  how to abort the connection ? : 目前 send rst seg and set local stream error
        unclean_shutdown(true);  //  rst 1
        return;
    }

    //  因为tick可能会造成sender重传 故tcpconnection需要及时将segment从sender中取出发送
    send_segments();

    _time_since_last_segment_received += ms_since_last_tick;

    //  end the connection cleanly if necessary (please see Section 5).
    _receiver.state();
    _sender.state();

    if (_receiver.state() == TCPReceiver::State::FIN_RECV && _sender.state() == TCPSender::State::FIN_ACKED &&
        _linger_after_streams_finish) {
        if (_time_since_last_segment_received >= 10 * _cfg.rt_timeout)
            clean_shutdown();
    }
}

//  shutdown TCPSender's outbound_stream
void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    //  send_segment 令TCPSender及时发送fin报文
    _sender.fill_window();
    send_segments();
    //  我目前猜测 关闭己方_stream 可能会tcp状态造成影响
    //  不过具体例子还没找出
    //  差强人意的例子 : TCPSender SYN_ACKED_1 ---local stream eof--> SYN_ACKED_2
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
            unclean_shutdown(true);  //  rst 2
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

// Sending segments. The TCPConnection will send TCPSegments over the Internet:
// • any time the TCPSender has pushed a segment onto its outgoing queue,
//  having set the fields it’s responsible for on outgoing segments:
//  (seqno, syn , payload, and fin ).
// • Before sending the segment,
//  the TCPConnection will ask the TCPReceiver for the fields it’s responsible for on outgoing segments:
//  ackno and window size.
//  If there is an ackno, it will set the ack flag and the fields in the TCPSegment

//  TCPConnection发送segment
//  将_sender的segment从其queue中全部放入connection的queue
//  注意该segment是否可以捎带_receiver的ack
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
        //  可能会出现多个segment捎带同一ack.不过应该不影响正确性. ack已经ack过的报文，在receiver看来就是直接忽略即可
        _segments_out.push(seg);
    }
}