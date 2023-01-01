#include "stream_reassembler.hh"

#include <cassert>
#include <iostream>

using std::cout;
using std::endl;

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity)
    , _capacity(capacity)
    , _receving_window()
    , _first_unread(0)
    , _first_unassembled(0)
    , _eof_idx(0)
    , _eof(false) {
        cout<<"capacity "<<_capacity<<endl;
    }



//  实际做了所谓的reassembler重组工作
size_t StreamReassembler::cached_into_receiving_window(const string &data, const size_t index,bool &non)
{
    size_t last_idx = 0;        //  for eof_idx。本轮操作结束后 如果有eof的话，则其坐标应当在last_idx
    //  A. 将data的相应部分 正确的放入receving window   O(n)
    //  1. data全部是已经排好序的老数据，即data全部是已经加入bytestream _output的数据,
    //  则不必加入bytestream，也不必有其他操作
    if (index + data.size() < _first_unassembled) {
        non = true;
        return 0;  //  nothiing
    }
    //  2. data全部是还没加入bytestream output的数据
    //  则将其加入_receving_window、这里可能会重复加入，效率低，不过先无所谓了，保证正确性再说别的。
    else if (index >= _first_unassembled && index < first_unacceptable()) {
        size_t len = 0;
        if (index + data.size() - 1 >= first_unacceptable()) {
            len = first_unacceptable() - index;
        } else {
            len = data.size();
        }
        // cout<<"len "<<len<<endl;
        // string data_to_accept(data.substr(0,len));
        for (size_t i = 0; i < len; ++i) {
            _receving_window[index + i] = data[i];
        }
        last_idx = index + len;
    }
    //  3.  data全部位于receving_window范围之外
    else if (index >= first_unacceptable()) {
        non = true;
        return 0;  //  nothing
    }
    //  4. data一部分加入 一部分没加入bytestream
    //  则截取相应部分落入recv_window
    else if (index < _first_unassembled && index + data.size() >= _first_unassembled) {
        size_t len = min(index + data.size() - _first_unassembled,
                         first_unacceptable() - _first_unassembled);  //  data中有多少bytes落入recv_window
        assert(len < data.size());
        size_t start_idx = _first_unassembled - index;
        for (size_t i = 0; i < len; ++i) {
            _receving_window[_first_unassembled + i] = data[i + start_idx];
        }
        last_idx = _first_unassembled + len;
    } else {
        cout<<"sth unknown happened!!"<<endl;
        non = true;
        return 0;
    }

    return last_idx;
}


void StreamReassembler::whether_eof(const string &data,const int index,const bool eof,const int last_idx)
{
    if (eof && index + data.size() <= first_unacceptable()) {      //  不能用last_index<=first_unacc来判断，因为last_index会自动截断到first_unacc         貌似eof不占据byte位置 ?
        // cout<<"eof "<<last_idx<<" "<<first_unacceptable()<<endl;
        _eof = true;
        _eof_idx = last_idx;
    }
}

// 将recv_window中已经顺序的字节加入bytestream 
void StreamReassembler::move_receiving_window()
{
    size_t old_first_unacceptable = first_unacceptable();
    for (size_t i = _first_unassembled; i <= old_first_unacceptable; ++i) {
        if(i == _eof_idx && _eof)
        {
            _output.end_input();
            break;   
        }
        if(i == old_first_unacceptable)
            break;

        if (_receving_window.find(i) == _receving_window.end())
            break;
        // cout<<_receving_window[i];
        
        //  从recving_window进入bytestream
        _output.write(string(1,_receving_window[i]));
        //  从recving_window中移除
        _receving_window.erase(i);
    }
}

//  return true : 
bool StreamReassembler::corner(const string &data, const size_t index, const bool eof)
{
    //  corner 1 : 如果eof已经加入了bytestream
        //  逻辑“如果已经eof，那么不再接受data。 可是加上这个就不对了，原因如下。
        //  答：因为可能只是将eof加入到了recv_window。但是recv_window里面的字节还并没有排成顺序，也即recv_window里的bytes还没全部接收，也就没有加入到bytestream里，更不必说eof也只是在recv_window中而没有加入到bytestream中
        //  改进：加上判断unassembled_bytes() == 0 即可。(即 // if(_eof && unassembled_bytes() == 0))
    if(_eof && unassembled_bytes() == 0)
    {
        cout<<"eof already!"<<endl;
        cout<<data<<" "<<index<<" "<<eof<<endl;
        cout<<data.size()<<" "<<index<<" "<<eof<<endl;
        cout<<_first_unread<<" "<<_first_unassembled<<" "<<first_unacceptable()<<" "<<_receving_window.size()<<" "<<_output.bytes_read()<<" "<<_output.bytes_written()<<endl;
        return true;
    }
    //  corner 2 : data empty , then nothing todo
    if(data.empty() && (!eof))
        return true;
    //  corner 3 : data empty 
    if(data.empty() && eof)
    {
        _eof = true;
        _eof_idx = index;
        _output.end_input();
        return true;
    }
    return false;
}

// cout<<_first_unread<<" "<<_first_unassembled<<" "<<first_unacceptable()<<" "<<_receving_window.size()<<" "<<_output.bytes_read()<<" "<<_output.bytes_written()<<endl;

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // cout<<"================push_substring start=============="<<endl;

    if(corner(data,index,eof))
        return ;

    _first_unread = _output.bytes_read();
    _first_unassembled = _output.bytes_written();

    // cout<<_first_unread<<" "<<_first_unassembled<<" "<<first_unacceptable()<<" "<<_receving_window.size()<<" "<<_output.bytes_read()<<" "<<_output.bytes_written()<<endl;
    
    //  1. 将data的相应部分cached into receiving_window
    bool non = false;   //  nothing_to_do ? 
    size_t last_idx = cached_into_receiving_window(data,index,non);
    if(non) return ;

    //  2. 计算eof下标
    whether_eof(data,index,eof,last_idx);
    //  3. 将receiving_window中的顺序字节加入bytestream，宏观来看就是移动接收窗口
    move_receiving_window();

    _first_unread = _output.bytes_read();
    _first_unassembled = _output.bytes_written();

    // cout<<_first_unread<<" "<<_first_unassembled<<" "<<first_unacceptable()<<" "<<_receving_window.size()<<" "<<_output.bytes_read()<<" "<<_output.bytes_written()<<endl;
    // cout<<"================push_substring end=============="<<endl;
}

size_t StreamReassembler::unassembled_bytes() const { 
    cout<<"get unassembled_bytes "<<_receving_window.size()<<endl;
    return _receving_window.size();
}

bool StreamReassembler::empty() const { 
    return unassembled_bytes() == 0 && _output.buffer_empty(); 
}
