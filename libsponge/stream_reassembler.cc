#include "stream_reassembler.hh"

#include <cassert>
#include <iostream>

using std::cout;
using std::endl;


# if 0

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
    , _eof(false) 
    {}



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
        for (size_t i = 0; i < len; ++i) {
            _receving_window[index + i] = data[i];
        }
        last_idx = index + len;
    }
    //  3.  data全部位于receving_window范围之外
    //  这种情况即所谓的 [接收缓存溢出] 因为bytestream可能已经满了达到capacity 导致 receive_window_size == 0，向其中写入byte失败
    //  因为bytestream长时间不读，导致bytestream_size + recv_window_sz == capacity(最终会变成bytestream_sz == capacity) , 致使data落在recv_window之外被discard
    //  此即[接收缓存溢出]
    //  与自顶向下P164描述不同 , 我的处理方案是 写失败后 并不将其从receive_window中移除
    //  而是既不写入bytestream , 也不将其从receive_window中移除
    //  也即不做操作，从bytestream到receive_window都不变
    //  而之后如果有新的数据到来receiver , receiver 不会将其缓存进receive_window 而是丢弃。
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
        //  sth unknown happened;
        non = true;
        return 0;
    }

    return last_idx;
}


void StreamReassembler::whether_eof(const string &data,const int index,const bool eof,const int last_idx)
{
    if (eof && index + data.size() <= first_unacceptable()) {      //  不能用last_index<=first_unacc来判断，因为last_index会自动截断到first_unacc         貌似eof不占据byte位置 ?
        _eof = true;
        _eof_idx = last_idx;
    }
}

// 将recv_window中已经顺序的字节加入bytestream 
void StreamReassembler::move_receiving_window()
{
    size_t old_first_unacceptable = first_unacceptable();
    string data;
    bool ed = false;
    for (size_t i = _first_unassembled; i <= old_first_unacceptable; ++i) {
        if(i == _eof_idx && _eof)
        {
            ed = true;
            // _output.end_input();
            break;   
        }
        if(i == old_first_unacceptable)
            break;

        if (_receving_window.find(i) == _receving_window.end())
            break;
        
        //  字节从recving_window进入bytestream
        // _output.write(string(1,_receving_window[i]));
        //  copy
        data.push_back(_receving_window[i]);

        //  从recving_window中移除
        _receving_window.erase(i);
    }

    _output.write(std::move(data));        //  引用 &
    if(ed)
    {
        _output.end_input();
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
        return true;
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


//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
//  合法data : empty || not empty
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    
    //  0. corner case
    if(corner(data,index,eof))
        return ;

    _first_unread = _output.bytes_read();
    _first_unassembled = _output.bytes_written();

    //  1. 将data的相应部分cached into receiving_window
    bool non = false;   //  nothing_to_do 
    size_t last_idx = cached_into_receiving_window(data,index,non);
    if(non) return ;

    _first_unread = _output.bytes_read();
    _first_unassembled = _output.bytes_written();

    //  2. 计算eof下标
    whether_eof(data,index,eof,last_idx);
    //  3. 将receiving_window中的顺序字节加入bytestream，宏观来看就是移动接收窗口
    move_receiving_window();

    _first_unread = _output.bytes_read();
    _first_unassembled = _output.bytes_written();

}

size_t StreamReassembler::unassembled_bytes() const { 
    return _receving_window.size();
}

bool StreamReassembler::empty() const { 
    return unassembled_bytes() == 0 && _output.buffer_empty(); 
}


#endif


StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _first_unacceptable(capacity) {}

void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (index >= _first_unacceptable) {  // 如果该字符串索引越界，则直接return
        return;
    } else if (data.length() == 0) {
        if (eof)
            _eof_flag = true;
        assembled();
        return;
    }

    bool all_store = true;  // 该字符串是否完全存入
    size_t len = data.length();
    size_t _index = index;
    size_t start = 0;

    if (_index + len - 1 < _first_unassembled) {  // 如果这个字符串已经完全被重组，则return
        if (eof && all_store)
            _eof_flag = true;
        assembled();
        return;
    }

    // step1. 去掉越界的部分
    if (_index + len > _first_unacceptable) {
        len = _first_unacceptable - _index;
        all_store = false;
    }

    // step2. 去掉已经被重组的部分
    if (_index < _first_unassembled) {
        len -= _first_unassembled - _index;
        _index = _first_unassembled;
        start = _index - index;
    }

    if (len == 0) {
        if (eof && all_store)
            _eof_flag = true;
        assembled();
        return;
    }

    // step3. 去掉已经存储的前缀
    auto iter = storage.lower_bound(_index);
    // 如果存储中第一个索引不小于_index的元素不是第一个元素 或者
    // 不存在（即storage.end())，则前一个元素就是最后一个索引小于_index的元素
    if (iter != storage.begin()) {
        iter--;
        // 如果该子串已经完全重叠，也就该子串最后一个字符索引<=查询到的子串的最后一个字符索引, 返回
        if (_index + len <= iter->first + iter->second.length()) {
            if (eof && all_store)
                _eof_flag = true;
            assembled();
            return;
        } else if (iter->first + iter->second.length() > _index) {  // 如果有重叠，去掉前缀
            len -= iter->first + iter->second.length() - _index;
            _index = iter->first + iter->second.length();
            start = _index - index;
        }
    }

    // step4. 去掉已经存储的完全属于该串的子串或者该串已经被完全存储过则不存储该串直接写入
    iter = storage.lower_bound(_index);
    while (iter != storage.end()) {
        if (iter->first + iter->second.length() <= _index + len) {
            _unassembled_bytes -= iter->second.length();
            storage.erase(iter);
            iter = storage.lower_bound(_index);
        } else if (iter->first == _index && iter->first + iter->second.length() >= _index + len) {
            if (eof && all_store)
                _eof_flag = true;
            assembled();
            return;
        } else {
            break;
        }
    }

    // setp5. 去掉已经存储的后缀
    // 找到第一个已经存储的索引大于_index的串
    iter = storage.upper_bound(_index);
    if (iter != storage.end()) {
        if (iter->first <= _index + len - 1) {  // 有重叠
            len = iter->first - _index;
            all_store = false;
        }
    }

    // 存储
    storage[_index] = data.substr(start, len);
    _unassembled_bytes += len;

    if (all_store && eof)
        _eof_flag = true;

    // 写入字节流
    assembled();
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

void StreamReassembler::assembled() {
    while (storage.count(_first_unassembled)) {
        string data = storage[_first_unassembled];
        size_t len = _output.write(data);  // 实际重组了len个字节
        if (len == 0)
            break;
        storage.erase(_first_unassembled);
        _first_unassembled += len;
        _first_unacceptable += len;
        _unassembled_bytes -= len;
        if (len < data.length()) {  // 如果没有完全写入，再将剩下的再次插入map
            storage[_first_unassembled] = data.substr(len);
            break;
        }
    }

    if (_eof_flag && empty())
        _output.end_input();
}

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }