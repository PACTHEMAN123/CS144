#include "reassembler.hh"
using namespace std;


void Reassembler::update(substring &st)
{
  // since we will handler overlap every time,
  // we assume overlaping substrings dont exist.

  // 1.find the pos and insert.
  list<substring>::iterator it = substrings.begin();
  list<substring>::iterator backit = substrings.begin();
  list<substring>::iterator fowdit = substrings.begin();

  for(; it != substrings.end(); it++)
    if((*it).first_byte >= st.first_byte)
      break;
  it = substrings.insert(it, st);

  // 2.check the substring before pos.
  if(it != substrings.begin()){
    backit = it;
    backit --;

    substring t = (*backit);
    // case 1: st totally inside the substring. delete the st.
    if((*backit).first_byte + (*backit).data.size() >= st.first_byte + st.data.size()){
      substrings.erase(it);
      goto push;
    }

    // case 2: st overlap part of it. edit the substring.
    if((*backit).first_byte + (*backit).data.size() < st.first_byte + st.data.size()){
      (*backit).data = (*backit).data.substr(0, st.first_byte - (*backit).first_byte);
    }

    // case 3: no overlap. do nothing.
  }


  // 3.check the substring after pos.
  if(it == substrings.end())
    goto push;

  fowdit = it;
  fowdit++;
  for(; fowdit != substrings.end();){
    // case 1: no overlap. 
    if((*fowdit).first_byte >= st.first_byte + st.data.size())
      break;

    // case 2: st totally inside the substring. delete the st.
    if((*fowdit).first_byte == st.first_byte && (*fowdit).data.size() >= st.data.size()){
      substrings.erase(it);
      goto push;
    }

    // case 3: substring totally inside the st. delete the substring.
    if((*fowdit).first_byte + (*fowdit).data.size() <= st.first_byte + st.data.size()){
      fowdit = substrings.erase(fowdit);
      continue;
    }

    // case 4: st overlap part of it. edit the substring.
    if((*fowdit).first_byte + (*fowdit).data.size() > st.first_byte + st.data.size()){
      (*fowdit).data = (*fowdit).data.substr(st.first_byte + st.data.size() - (*fowdit).first_byte);
      (*fowdit).first_byte = st.first_byte + st.data.size();
    }
    fowdit++;
  }

push:
  // 4.scan the list and push possible substrings
  substring t;
  while(!substrings.empty()){
    t = substrings.front();

    if(t.first_byte == next_byte){
      output_.writer().push(t.data);
      next_byte += t.data.size();
      substrings.pop_front();
    } else 
      break;
  }

  if(t.first_byte + t.data.size() == finish_index && substrings.empty()){
    output_.writer().close();
  }

  return;
}

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // pack it up.
  substring st;
  st.first_byte = first_index;
  st.is_last_substring = is_last_substring;
  st.data = data;

  // get the finish byte index
  if(st.is_last_substring){
    finish_index = first_index + data.size();
  }
  // if the substring totally outside the available area, discarded.
  if(first_index + data.size() < next_byte || first_index >= unaccept_index()){
    return;
  }

  // if no space for substring, discarded.
  if(next_byte == unaccept_index())
    return;

  // if the substring have bytes outside the available area, cut it.
  // case 1: head outbound.
  if(st.first_byte < next_byte){
    st.data = st.data.substr(next_byte - st.first_byte);
    st.first_byte = next_byte;
  }

  // case 2: tail outside.
  if(st.first_byte + st.data.size() > unaccept_index()){
    st.data = st.data.substr(0, unaccept_index() - st.first_byte);
    st.is_last_substring = false;
  }

  // finish process, push into the list.
  update(st);

  return;
}

uint64_t Reassembler::bytes_pending() const
{
  uint64_t used_bytes = 0;
  for(substring i: substrings){
    used_bytes += i.data.size();
  }
  return used_bytes;
}


uint64_t Reassembler::unpopped_index()
{
  return output_.writer().bytes_pushed();
}

uint64_t Reassembler::unaccept_index()
{
  return next_byte + output_.writer().available_capacity();
}
