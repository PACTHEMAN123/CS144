#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

bool Writer::is_closed() const
{
  return writer().closed_;
}

void Writer::push( string data )
{
  uint64_t cap = writer().available_capacity();
  if(data.length() > cap)
  {
    data = data.substr(0, cap);
  } 
  
  writer().buffer_ += data;
  writer().bytes_pushed_ += data.size();
  return;
}

void Writer::close()
{
  writer().closed_ = true;
}

uint64_t Writer::available_capacity() const
{
  return (writer().capacity_ - writer().buffer_.size());
}

uint64_t Writer::bytes_pushed() const
{
  return writer().bytes_pushed_;
}

bool Reader::is_finished() const
{
  bool b = (reader().buffer_.size() == 0? true: false) 
    && (reader().closed_);
  return b;
}

uint64_t Reader::bytes_popped() const
{
  return reader().bytes_popped_;
}

string_view Reader::peek() const
{
  string_view view = reader().buffer_;
  return view;
}

void Reader::pop( uint64_t len )
{
  // Your code here.
  reader().buffer_ = reader().buffer_.substr(len);
  reader().bytes_popped_ += len;
}

uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return reader().buffer_.size();
}
