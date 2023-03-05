#include "stream_reassembler.hh"

#include <cassert>
#include <iostream>

using std::cout;
using std::endl;


# if 1

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
    , _eof_idx(0)
    , _eof(false) 
    {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
//  合法data : empty || not empty
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {

//   约定 data的前串prev_data 为 prev_data的起始idx比data的起始idx小 ; 同理 data的 后串 next_data为 next_data的起始idx比data的起始idx大
//   1. 获取当前 index 在 receive_window 中的前串
//      a. 如果存在前串，则判断是否存在重复区域。如果存在部分重复则截断，存在全部重复则直接丢弃。
//      b. 如果不存在前串, 则判断当前数据是否已经和已经被 receive_window 的字符串重复。如果存在部分重复则截断，存在全部重复则直接丢弃。
//   2. 获取当前 index 在 receive_window 中的后串。 如果存在后串
//     a. 如果后串 没有被当前数据完全包含，则判断是否存在重复区域。如果存在则截断。
//     b. 如果后串 被当前数据包含，则将后串 从 receive_window 中丢弃，之后重新取出一个新的后串
//     重复第二步 这时候我们可以获得与 receive_window 中没有任何重复的字符串
//     我们没有将不同的串合并，只是让串之间不能重叠即可
    //  我们没有将不同的串合并，只是保证了不能重叠。合并省去了，感觉可能影响效率。
    //  寻找 新串index 的 前串(其idx <= index)
    map<size_t,string>::iterator prev_iter = _receving_window.upper_bound(index);       //  >
    if(prev_iter!=_receving_window.begin())         //  <=
        --prev_iter;
    //  1. prev_iter == begin_iter == end_iter，表示_receive_window不存在任何数据
    //  2. prev_iter != end_iter. prev_iter 指向前串 prev_iter指向后串
    //  new_idx 至少要从 first_unassembled()开始
    size_t new_idx = max(index,first_unassembled());
    //  _receving_window.empty()
    if(prev_iter == _receving_window.end())
    {
        //  _receving_window.empty()
    }
    //  如果是 前串(其idx < index) 
    else if(prev_iter->first <= index)
    {
        new_idx = max(prev_iter->first + prev_iter->second.size(),index);
    }
    
    //  在考虑new data前面可能有老串重叠的情况下，得出data非重叠部分的起始偏移量(相对)
    size_t data_start_offset = new_idx - index;
    //  去除重叠部分的data长度
        //  出去重叠前缀的data长度
    if(data.size() < (new_idx - index))    //  如果已经全部都在bytestream中     不写是因为可能有空的eof
        return ;
    if(data.size() == new_idx - index)
    {
        if(eof)
        {
            _eof = true;
            _eof_idx = index + data.size();
            // cout<<"EOF IDX "<<_eof_idx<<endl;
        }
        if(_eof && _eof_idx <= first_unassembled())
        {
            _output.end_input();
        }
    }
    size_t data_size = data.size() - (new_idx - index);
    

    // cout<<"new_idx "<<new_idx<<"data_size "<<data_size<<" "<<data_start_offset<<endl;

    //  寻找后串(且idx >= new_idx) :去掉全部覆盖的以及重叠后缀
    map<size_t,string>::iterator ne_iter = _receving_window.lower_bound(new_idx);           // >=    
    //  如果和后面的重叠
    while(ne_iter!=_receving_window.end() && new_idx <= ne_iter->first && new_idx + data_size > ne_iter->first)
    {
        //  new data 全部覆盖 ne_iter
        if(new_idx + data_size >= ne_iter->first + ne_iter->second.size())
        {
            _receiving_window_size -= ne_iter->second.size();
            ne_iter = _receving_window.erase(ne_iter);
        }
        //  new data 部分覆盖 ne_iter，去除后缀
        else
        {
                //  去除后缀的data长度
            data_size = ne_iter->first - new_idx;
        }
    }

    //  至此，我们得到的 data.substr(data_start_offset,data_size) 
    //  是一个不和receive_window中其他字段重叠的data
    // 检测是否存在数据超出了窗口容量
    if (first_unacceptable() <= new_idx)
        return;
    
    // cout<<"start offset "<<data_start_offset<<" data_size "<<data_size<<endl;
    if(data_size > 0)
    {
        string isolated_data = data.substr(data_start_offset,data_size);
        // cout<<"isolated data "<<isolated_data<<endl;
        //  新串未构成顺序，不可直接写入stream
        if(first_unassembled() < new_idx)
        {
            _receiving_window_size += isolated_data.size();
            _receving_window.insert({new_idx,std::move(isolated_data)});
        }
        //  新串和stream构成顺序，可以直接写入stream
        else if(first_unassembled() == new_idx)
        {
            // cout<<"try to write "<<isolated_data<<endl;
            size_t written = _output.write(isolated_data);
            //  没写完 -> 将剩下的str存入receive_window  维护_receive_window_size
            if(written < data_size)
            {
                string left_data = isolated_data.substr(written,data_size - written);
                _receiving_window_size += left_data.size();
                _receving_window.insert({new_idx + written , left_data});
            }
        }
        else
        {
            cerr<<"never reach!"<<endl;
        }
    }

    //  本次来的新串写入bytestream之后 , 有可能造成别的串也可以压入bytestream了。
    for(auto iter = _receving_window.begin();iter!=_receving_window.end();)
    {
        assert(first_unassembled() <= iter->first);
        auto idx = iter->first;
        auto &str = iter->second;
        //  已经不可写入, break即可
        if(first_unassembled() != idx)
            break;
        if(idx == first_unassembled())
        {
            // cout<<"try to write "<<str<<endl;
            size_t written = _output.write(str);
            //  维护 receive_window_size
            _receiving_window_size -= written;
            //  全部写入
            if(written == str.size())
            {
                iter = _receving_window.erase(iter);
            }
            //  没全部写入 剩下的重新存入 break;
            else
            {
                int nidx = idx + written;
                _receving_window.insert({nidx,std::move(str.substr(written))}); 
                iter = _receving_window.erase(iter);
                break;
            }
        }
    }
    if(eof)
    {
        _eof = true;
        _eof_idx = index + data.size();
    }
    if(_eof && _eof_idx <= first_unassembled())
    {
        _output.end_input();
    }

}

size_t StreamReassembler::unassembled_bytes() const { 
    return _receiving_window_size;
}

bool StreamReassembler::empty() const { 
    return unassembled_bytes() == 0 && _output.buffer_empty(); 
}


#endif


// cout<<"show window start"<<endl;
// for(auto &item : _receving_window)
// {
//     cout<<item.first<<" "<<item.second<<endl;
// }
// cout<<"show window end"<<endl;