#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <string>
#include <unordered_map>
#include<memory>

using std::unordered_map;
using std::unique_ptr;
using std::string;
#if 0
//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.

    ByteStream _output;  //!< The reassembled in-order byte stream  有序的bytes
    size_t _capacity;    //!< The maximum number of bytes   // receiving window ?
    
    unordered_map<size_t , char> _receving_window;      //  乱序到达的，还没加入bytestream的bytes

    // size_t _nread;             //  已经读了多少bytes。（所谓读，即上层调用bytestream.read()读走) nread = _first_unread (起始索引为0)
    size_t _first_unread;         //  第一个没被读的byte的索引
    size_t _first_unassembled;    //  第一个没加入bytestream的byte的索引（第一个乱序的byte索引）。receiving_window左边界
    size_t first_unacceptable() const {return _first_unread + _capacity;} // 第一个不可接收的字节，即第一个超出接收范围的字节。receiving_window右边界
   
    size_t _eof_idx;
    bool _eof;

  private:
    size_t cached_into_receiving_window(const string &data, const size_t index , bool& non);
    void whether_eof(const string &data,const int index,const bool eof,const int last_idx);
    void move_receiving_window();
    bool corner(const string &data, const size_t index, const bool eof);
  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;

    size_t window_size() const{
      // _first_unassembled = _output.bytes_written();      
      // return first_unacceptable() - _first_unassembled;
      return _capacity + _output.bytes_read() - _output.bytes_written();
    }

    size_t first_unassembled() const{   
      //  比_first_unassembled更及时；不过由于整个类只能通过push_substring来改变_first_unassembled，因此first_unassembled() 调用时 应当== _first_unassembled
      // _first_unassembled = _output.bytes_written();
      // return _first_unassembled;
      return _output.bytes_written();
    }
};

#endif  

#include <map>

using std::map;

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.
    ByteStream _output;             //!< The reassembled in-order byte stream
    size_t _capacity;               //!< The maximum number of bytes
    size_t _first_unacceptable;     // the first unacceptable byte's index, default = _capacity
    size_t _first_unassembled = 0;  // the first unassembled byte's index, default = 0
    size_t _unassembled_bytes = 0;
    bool _eof_flag = false;                   // EOF Flag
    std::map<size_t, std::string> storage{};  // auxiliary storage
    void assembled();

  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;

    size_t window_size() const{
      // _first_unassembled = _output.bytes_written();      
      // return first_unacceptable() - _first_unassembled;
      return _capacity + _output.bytes_read() - _output.bytes_written();
    }

    size_t first_unassembled() const{   
      //  比_first_unassembled更及时；不过由于整个类只能通过push_substring来改变_first_unassembled，因此first_unassembled() 调用时 应当== _first_unassembled
      // _first_unassembled = _output.bytes_written();
      // return _first_unassembled;
      return _output.bytes_written();
    }
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
