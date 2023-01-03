#ifndef SPONGE_LIBSPONGE_TCP_RECEIVER_HH
#define SPONGE_LIBSPONGE_TCP_RECEIVER_HH

#include "byte_stream.hh"
#include "stream_reassembler.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"
#include <iostream>

using std::cout;
using std::endl;
#include <cassert>
#include <optional>

//! \brief The "receiver" part of a TCP implementation.

//! Receives and reassembles segments into a ByteStream, and computes
//! the acknowledgment number and window size to advertise back to the
//! remote TCPSender.
class TCPReceiver {
    //! Our data structure for re-assembling bytes.
    StreamReassembler _reassembler;

    //! The maximum number of bytes we'll store.
    size_t _capacity;
    std::optional<WrappingInt32> _isn;

    enum State { LISTENING = 1, SYN_RECV,FIN_RECV };
    State _state;

  private:
    bool listening() { return !_isn.has_value(); }
    bool syn_recv() { return _isn.has_value() && !_reassembler.stream_out().input_ended(); }
    bool fin_recv() { return _reassembler.stream_out().input_ended(); }
    void update_state();
    bool corner(const TCPSegment &seg) const;
  public:

    //! \brief Construct a TCP receiver
    //!
    //! \param capacity the maximum number of bytes that the receiver will
    //!                 store in its buffers at any give time.
    TCPReceiver(const size_t capacity) : _reassembler(capacity), _capacity(capacity) , _isn(),_state(LISTENING) {}

    //! \name Accessors to provide feedback to the remote TCPSender
    //!@{

    //! \brief The ackno that should be sent to the peer
    //! \returns empty if no SYN has been received
    //!
    //! This is the beginning of the receiver's window, or in other words, the sequence number
    //! of the first byte in the stream that the receiver hasn't received.
    std::optional<WrappingInt32> ackno() const;

    //! \brief The window size that should be sent to the peer
    //!
    //! Operationally: the capacity minus the number of bytes that the
    //! TCPReceiver is holding in its byte stream (those that have been
    //! reassembled, but not consumed).
    //!
    //! Formally: the difference between (a) the sequence number of
    //! the first byte that falls after the window (and will not be
    //! accepted by the receiver) and (b) the sequence number of the
    //! beginning of the window (the ackno).
    size_t window_size() const;
    //!@}

    //! \brief number of bytes stored but not yet reassembled
    size_t unassembled_bytes() const { return _reassembler.unassembled_bytes(); }

    //! \brief handle an inbound segment
    void segment_received(const TCPSegment &seg);

    //! \name "Output" interface for the reader
    //!@{
    ByteStream &stream_out() { return _reassembler.stream_out(); }
    const ByteStream &stream_out() const { return _reassembler.stream_out(); }
    //!@}

  private:
    size_t abs_seq_to_stream_idx(size_t abs_seq) {
        //  特例abs_seq == 0 : 对于最一开始的syn报文 其携带的seq 不是 正式应用数据报文的seq 而是isn。
        //  那么如果携带了数据 对于该写动作，应用报文的stream_idx也不能通过abs_seq_to_streamidx计算 应当人为规定为0
        return abs_seq == 0 ? 0 : abs_seq - 1;
    }
};

#endif  // SPONGE_LIBSPONGE_TCP_RECEIVER_HH
