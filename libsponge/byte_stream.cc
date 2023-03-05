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


//  如果外界传参传的是个临时量或者char，则传参还会有一次拷贝. 
//  原先StreamReassembler(unordered_map<size_t,char>)调用write时只能传递char，故每个char在传参的时候都会拷贝一次，这个const &就跟直接传值一样。那么对于一个大字符串，每个字符都要拷贝一次。
//      为什么原先只能传递char 不能传递string呢? 因为原先是以char为单元来对窗口进行维护的，也没有维护哪些字符连续，只能边遍历边传unordered_map里的字符char拷贝给write.
//  而我们将StreamReassembler 底层容器改为 map<size_t,string>时,可以以块(string)的形式维护字节连续的信息，这样当我们调用write的时候，直接传入给write const string &的就是map里的string，所以是以引用的方式传递的，故减少了拷贝的时间和内存上的消耗.
//      这就是改变StreamReassembler提升性能的原因之1

//  原先StreamReassembler(unordered_map<size_t,char>)调用write时只能传递char,一个一个传递，故调用write的次数会极多，write开辟和回退栈帧的消耗会变得很大
//  故这也是StreamReassembler提升性能的原因之2

//  继续优化，我们将ByteStream内部的底层容器改成BufferList.(deque<string>)
//  在write里面，使用vector<char>时，每次只能push_back一个char，也会push很多很多次，push_back开辟和回退栈帧的开销也会很大.
//  使用deque<string> 时，只需要一次，就能拷贝到所有char. 开辟回退栈帧开销小.

//  但是可以看到，在ByteStream内部，底层容器采用BufferList还是deque<char>，对于在write中，拷贝char次数是相同的.
//  采用deque<char>,每次push_back(char)的时候会有拷贝一个字符，最终会拷贝data的所有字符，将其放入deque中
//  采用BufferList(deque<Buffer>, Buffer即shared_ptr拥有着一个string;可以看作是个deque<string>)，则虽然string不同于char，是个内部有移动构造函数的对象
//  但是，由于data是个const & , 我们为了获取它的字符，还是需要substr一次。而substr是一个拷贝. 故 还是会拷贝一次string里所有的char

//  StreamReassembler 提升性能原因之3
    //  原先的策略，仅仅是为了保证正确性，对于到来的char，哪怕这些char之前已经缓存过，还是重新缓存一遍(也即，每个char会加入到receive_window中多次），对于重复的大量字符串，效率极差.
    //  现在的策略，采用map<i,string>，对于缓存的char，会以块的方式维护（因为底层用的是string），可以维护哪些字符是连在一起的，这样调用write的时候不必1个char1个char的遍历、write。
    //  且不会重复缓存已经缓存过的char! 每个char只会缓存一次. 也即 每个char只会加到 receive_window中一次!

size_t ByteStream::write(const string &data) {
    
    assert(!input_ended());     //  如果写端被关闭，则外界不应当对stream进行write。

    size_t bytes_to_write = min(data.size(), _capacity - _stream.size());   //  最多写多少bytes
    _bytes_pushed += bytes_to_write;
    // for (size_t i = 0; i < bytes_to_write; ++i) {
        // _stream.push_back(data[i]);
    // }
    //  substr copy 一次 ; 然后 move到_stream中
    _stream.append(std::move(data.substr(0,bytes_to_write)));

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
    string s(_stream.concatenate());
    return s.substr(0,bytes_to_read);
    // return string(_stream.begin(),_stream.begin()+bytes_to_read);
}


//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    assert(_stream.size() <= _capacity);
    
    size_t bytes_to_pop = min(len, _stream.size());  //  最多全部弹出
    _bytes_popped += bytes_to_pop;
    // while (bytes_to_pop--) {
    //     _stream.pop_front();
    // }
    _stream.remove_prefix(bytes_to_pop);
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

// bool ByteStream::buffer_empty() const { return _stream.empty(); }
bool ByteStream::buffer_empty() const { return _stream.size() == 0; }


//  遇见eof : input关闭，且_stream中无数据
// bool ByteStream::eof() const { return _end && _stream.empty(); }
bool ByteStream::eof() const { return _end && _stream.size() == 0; }

//  总共有多少bytes压入到_stream中过
size_t ByteStream::bytes_written() const { return _bytes_pushed; }
//  _stream中总共有过多少bytes流出
size_t ByteStream::bytes_read() const { return _bytes_popped; }

size_t ByteStream::remaining_capacity() const { return _capacity - _stream.size(); }

