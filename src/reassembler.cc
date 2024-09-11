#include "reassembler.hh"
#include <iostream>
using namespace std;

//#define D

void Reassembler::update()
{
  // call after insert get the correct next byte
  while(next_byte >= substrings.front().first_byte) {
    // clean the useless substrings
    if(substrings.front().first_byte < next_byte 
    && substrings.front().first_byte + substrings.front().data.size() <= next_byte){
      substrings.pop_front();
      if(substrings.empty())
        break;
      continue;
    }
    // allow overlap
    if(next_byte > substrings.front().first_byte
    && substrings.front().first_byte + substrings.front().data.size() - next_byte > 0){
      substrings.front().data = substrings.front().data.substr(next_byte - substrings.front().first_byte, 
      substrings.front().first_byte + substrings.front().data.size() - next_byte);
    }

#ifdef D
    char lb = substrings.front().data.back();
    cout << "update push ..." << static_cast<int>(lb) << endl;
#endif
    
    output_.writer().push(substrings.front().data);
    next_byte += substrings.front().data.size();
    //cout << "damn" << endl;
    if(substrings.front().is_last_substring)
      output_.writer().close();

    substrings.pop_front();
    if(substrings.empty())
      break;
  }
  return;
}

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  substring st;
  st.data = data;
  st.first_byte = first_index;
  st.is_last_substring = is_last_substring;

// debug info
  #ifdef D
  cout << "---------------------------"<< endl;
  cout << "insert: " << first_index << " size: " << data.size() << endl;
  cout << "| current next_bytes: " << next_byte << endl;
  cout << "| current unaccepted index: " << unaccept_index() << endl;
  cout << "| pushed_bytes" << output_.writer().bytes_pushed() << endl;
  char lastchar = data.back();
  cout << "| data's last byte"  << static_cast<int>(lastchar) << endl;
  #endif

  if(output_.writer().is_closed() || output_.writer().available_capacity() == 0)
    return;

  if(first_index == next_byte && first_index < unaccept_index()) {
    if(unaccept_index() - first_index < data.size())
      st.data = st.data.substr(0, unaccept_index() - first_index);
    substrings.push_front(st);
    
    update();
    return;
  } else if(first_index > next_byte && first_index < unaccept_index()) {
    // find and insert
    std::list<substring>::iterator it = substrings.begin();
    for(; it != substrings.end(); it++) {
      if((*it).first_byte >= first_index)
        break;
    }
    if(unaccept_index() - first_index < data.size()){
      st.data = st.data.substr(0, unaccept_index() - first_index);
      st.is_last_substring = false;
    }
    substrings.insert(it, st);
    update();
    return;
  } else if(first_index < next_byte && first_index + data.size() > next_byte && next_byte < unaccept_index()) {
    // overlap insert
    st.first_byte = next_byte;
    st.data = st.data.substr(next_byte - first_index, first_index + data.size() - next_byte);
    substrings.push_front(st);
    update();
    return;
  } else {
    // discard the substring that beyond available capacity
    return;
  }
  
  
}

uint64_t Reassembler::bytes_pending() const
{
  uint64_t used_sized = 0;
  uint64_t read_index = next_byte;
  //cout << "bytes_pending:" << endl;
  //cout << "next_byte: " << next_byte << endl;
  for(substring i : substrings){
    // handler overlap
    if(read_index > i.first_byte && i.first_byte + i.data.size() > read_index)
      used_sized -= read_index - i.first_byte;

    if(read_index > i.first_byte && i.first_byte + i.data.size() <= read_index)
      continue;

    used_sized += i.data.size();
    if(read_index < i.first_byte + i.data.size())
      read_index = i.first_byte + i.data.size();
    //cout << "data: "<< i.data << endl;
    //cout << "used_sized: "<< used_sized << endl;
  }
    
  return used_sized;
}


uint64_t Reassembler::unpopped_index()
{
  return output_.writer().bytes_pushed();
}

uint64_t Reassembler::unaccept_index()
{
  return next_byte + output_.writer().available_capacity();
}
