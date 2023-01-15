#include "byte_stream.hh"

#include <cassert>
// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`


#include <iostream>

using std::cout;
using std::endl;


template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : _stream(), _capacity(capacity), _bytes_popped(0), _bytes_pushed(0), _end(false) {
}

size_t ByteStream::write(const string &data) {
    
    assert(!input_ended());     //  如果写端被关闭，则外界不应当对stream进行write。

    size_t bytes_to_write = min(data.size(), _capacity - _stream.size());   //  最多写多少bytes
    _bytes_pushed += bytes_to_write;
    for (size_t i = 0; i < bytes_to_write; ++i) {
        _stream.push_back(data[i]);
    }

    return bytes_to_write;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    assert(_stream.size() <= _capacity);

    size_t bytes_to_read = min(len, _stream.size());
    // string res;
    // for (deque<char>::const_iterator iter = _stream.begin(); (bytes_to_read-- > 0) && iter != _stream.end(); ++iter) {
    //     // cout<<*iter;
    //     res.push_back(*iter);
    // }
    // return res;
    return string(_stream.begin(),_stream.begin()+bytes_to_read);
}


//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    assert(_stream.size() <= _capacity);
    
    size_t bytes_to_pop = min(len, _stream.size());  //  最多全部弹出
    _bytes_popped += bytes_to_pop;
    while (bytes_to_pop--) {
        _stream.pop_front();
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    // DUMMY_CODE(len);
    assert(_stream.size() <= _capacity);
    string res(peek_output(len));
    pop_output(len);
    return res;
}

//  关闭input端
void ByteStream::end_input() { _end = true; }

//  input写端是否被关闭
bool ByteStream::input_ended() const { return _end; }
//  当前_stream中还有多少bytes未读出
size_t ByteStream::buffer_size() const { return _stream.size(); }

bool ByteStream::buffer_empty() const { return _stream.empty(); }

//  遇见eof : input关闭，且_stream中无数据
bool ByteStream::eof() const { return _end && _stream.empty(); }
//  总共有多少bytes压入到_stream中过
size_t ByteStream::bytes_written() const { return _bytes_pushed; }
//  _stream中总共有过多少bytes流出
size_t ByteStream::bytes_read() const { return _bytes_popped; }

size_t ByteStream::remaining_capacity() const { return _capacity - _stream.size(); }

