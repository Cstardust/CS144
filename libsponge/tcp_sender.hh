#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <queue>
#include <deque>

using std::deque;
using std::cout;
using std::endl;
using std::unordered_map;
using std::vector;


// the retransmission timer
// an alarm that can be started at a certain time, 
// and the alarm goes off (or “expires”) once the RTO has elapsed. 
// We emphasize that this notion of time passing comes from the tick method being called 
// --- not by getting the actual time of day
class Timer
{
public:
    Timer():_alarm(0),_initial_alarm(0),_active(false){}
    void start(uint64_t initial_alarm);   //  milliseconds  设置超时时间。
    bool elapse(uint64_t elapsed);//  milliseconds  pass了多少时间。降至0则返回true，代表Timer超时。
    void reset();
    bool active() const {return _active;}
    uint64_t initial_alarm() const {return _initial_alarm;}
    uint64_t alarm() const {return _alarm;}
private:
    uint64_t _alarm;
    uint64_t _initial_alarm;
    bool _active;
};



//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    //  这并不是我们所说的send_window。
    //  sender的发送动作就是将segment放入 queue _segment_out中。然后外界调用_segment进行发送
    //  所以这并不是我们所说的send window
    //  sender只需要_segment_out.push(seg)即可发送
    std::queue<TCPSegment> _segments_out{};


    //  这才是 我们所说的 sending_window
    //  sent but not acked
    //  <seq num , segment>
    // unordered_map<size_t,TCPSegment> _send_window;
    //  由于我们fill_window的顺序 也即push的顺序 故里面的seg也都是按照seqno增序排序的
    deque<TCPSegment> _send_window;

    // size_t _send_window_size;     //  就是 size_t outstanding_bytes; 就是已经发送 但是未确认的字节数量（占据seq空间）。就是_segment_out payload + SYN + FIN
    //  我不理解 为什么send_window的初始值是1
    // current window size, updated when ack_received() called.
    // the initial and minimum value is 1 so that the sender won't wait endlessly.
    size_t _receive_window_size;

    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout;
    // retransmission timer for the left edge of the sending window
    Timer _timer;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    //  sender将要发送的下一个字节的abs seq
    uint64_t _next_seqno{0};
    //  原先以为：接收方回复的ack是累计确认，那么sender要发送的下一个字节的序号自然就是ack。那么_next_seq就是ack。不过很可惜，似乎想错了，_next_seq并非ack。
    //  很遗憾。似乎和我预想的不一样。这个_next_seq 不是ack.也不是在ack_received中更新，而是sender自己维护。发送了seg就自己更新_next_seq，也不用等到该seg被ack，也不用管这个seg到底是否被receiver接收
    enum State {
        ERROR = 0,
        CLOSED ,
        SYN_SENT,
        SYN_ACKED_1,
        SYN_ACKED_2,
        FIN_SENT,
        FIN_ACKED,
    };

    //  对于同一分组的 重传次数
    uint64_t _consecutive_retransmissions_cnt;
    //  放弃该tcp连接
        //  1. 对同一segment 重传次数过多
    bool _aborted;
  private:
    bool closed() const { return next_seqno_absolute() == 0; }
    //  为什么要这么比较 next_seq_abs 和 bytes_in_flight ?
    bool syn_sent() const { return next_seqno_absolute() > 0 && next_seqno_absolute() == bytes_in_flight(); }
    bool syn_acked_1() const { return next_seqno_absolute() > bytes_in_flight() && !stream_in().eof(); }
    bool syn_acked_2() const { return stream_in().eof() && next_seqno_absolute() < stream_in().bytes_written() + 2; }
    bool fin_sent() const {
        return stream_in().eof() && next_seqno_absolute() == stream_in().bytes_written() + 2 && bytes_in_flight() > 0;
    }
    bool fin_acked() const {
        return stream_in().eof() && next_seqno_absolute() == stream_in().bytes_written() + 2 && bytes_in_flight() == 0;
    }
    State state() const {
        cout<<next_seqno_absolute()<<" "<<stream_in().bytes_written()<<endl;
        if (closed())
        {
            cout<<"state CLOSED"<<endl;
            return State::CLOSED;
        }
        else if (syn_sent())
        {
            cout<<"state SYN_SENT"<<endl;
            return State::SYN_SENT;
        }
        else if (syn_acked_1())
        {
            cout<<"state SYN_ACKED_1"<<endl;
            return State::SYN_ACKED_1;
        }
        else if (syn_acked_2())
        {
            cout<<"state SYN_ACKED_2"<<endl;
            return State::SYN_ACKED_2;
        }
        else if (fin_sent())
        {
            cout<<"state FIN_SENT"<<endl;
            return State::FIN_SENT;
        }
        else if (fin_acked())
        {
            cout<<"state FIN_ACKED"<<endl;
            return State::FIN_ACKED;
        }
        else {
            cout << "unknown sender state" << endl;
            return State::CLOSED;
        }
    }

  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    //  已经发送但是未被确认的字节数量。似乎就是_segments_out的所有payload的字节数量 + SYN + FIN 的字节数量? 
    //  就是outstanding_bytes的数量
    // count is in "sequence space,
    // _send_window_size
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    //  对于同一分组的 重传次数（目前我认为是这样的）
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
