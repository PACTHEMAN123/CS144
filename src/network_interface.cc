#include <iostream>

#include "arp_message.hh"
#include "exception.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  auto it = ip_map_.find(next_hop.ipv4_numeric());
  if(it != ip_map_.end()) 
  {
    EthernetHeader header;
    header.type = EthernetHeader::TYPE_IPv4;
    header.dst = (it->second).eaddr;
    header.src = ethernet_address_;

    EthernetFrame frame;
    frame.header = header;
    frame.payload = serialize(dgram);
    transmit(frame);
  } else {
    // boardcast an ARP request
    if(ip_request_.find(next_hop.ipv4_numeric()) != ip_request_.end())
      return;

    ARPMessage arp;
    arp.opcode = ARPMessage::OPCODE_REQUEST;
    arp.sender_ip_address = ip_address_.ipv4_numeric();
    arp.sender_ethernet_address = ethernet_address_;
    arp.target_ip_address = next_hop.ipv4_numeric();
    
    EthernetHeader header;
    header.type = EthernetHeader::TYPE_ARP;
    header.dst = ETHERNET_BROADCAST;
    header.src = ethernet_address_;

    EthernetFrame frame;
    frame.header = header;
    frame.payload = serialize(arp);
    transmit(frame);

    dgram_buffered d;
    d.dgram = dgram;
    d.ip = next_hop.ipv4_numeric();
    datagrams_buffered_.push_back(d);
    ip_request_.insert({next_hop.ipv4_numeric(), 0});
  }
}

void NetworkInterface::datagrams_clear( uint32_t dst)
{
  auto it = ip_map_.find(dst);
  for(auto i = datagrams_buffered_.begin(); i != datagrams_buffered_.end();) {
    dgram_buffered d = *i;
    if(d.ip == dst) {
      EthernetHeader header;
      header.type = EthernetHeader::TYPE_IPv4;
      header.dst = (it->second).eaddr;
      header.src = ethernet_address_;

      EthernetFrame frame;
      frame.header = header;
      frame.payload = serialize(d.dgram);
      transmit(frame);
      i = datagrams_buffered_.erase(i);
    } else {
      i ++;
    }
  }
}


//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if(frame.header.dst != ETHERNET_BROADCAST && frame.header.dst != ethernet_address_)
    return;

  if(frame.header.type == EthernetHeader::TYPE_IPv4) {
    InternetDatagram dgram;
    if(parse(dgram, frame.payload))
      datagrams_received_.push(dgram);
  }

  if(frame.header.type == EthernetHeader::TYPE_ARP) {
    ARPMessage arp;
    if(parse(arp, frame.payload) == false)
      return;
    
    if(arp.opcode == ARPMessage::OPCODE_REPLY) {
      ip_map_.insert({arp.sender_ip_address, {arp.sender_ethernet_address, 0}});
      datagrams_clear(arp.sender_ip_address);
      
      auto it = ip_request_.find(arp.sender_ip_address);
      if(it != ip_request_.end())
        ip_request_.erase(it);
    }

    if(arp.opcode == ARPMessage::OPCODE_REQUEST) {
      ip_map_.insert({arp.sender_ip_address, {arp.sender_ethernet_address, 0}});

      auto it = ip_request_.find(arp.sender_ip_address);
      if(it != ip_request_.end())
        ip_request_.erase(it);

      // send an ARP reply if ask for our address
      if(arp.target_ip_address == ip_address_.ipv4_numeric()){
        ARPMessage ar;
        ar.opcode = ARPMessage::OPCODE_REPLY;
        ar.sender_ip_address = ip_address_.ipv4_numeric();
        ar.sender_ethernet_address = ethernet_address_;
        ar.target_ip_address = arp.sender_ip_address;
        ar.target_ethernet_address = arp.sender_ethernet_address;
        
        EthernetHeader header;
        header.type = EthernetHeader::TYPE_ARP;
        header.dst = arp.sender_ethernet_address;
        header.src = ethernet_address_;

        EthernetFrame f;
        f.header = header;
        f.payload = serialize(ar);
        transmit(f);
      }
      
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // check the time of the request ARP
  for(auto it = ip_request_.begin(); it != ip_request_.end();) {
    it->second += ms_since_last_tick;
    if(it->second >= 5000)
      it = ip_request_.erase(it);
    else
      it++;
  }

  // check the time of the mapping between IP address and Ethernet address
  for(auto it = ip_map_.begin(); it != ip_map_.end();) {
    (it->second).time += ms_since_last_tick;
    if((it->second).time >= 30000)
      it = ip_map_.erase(it);
    else
      it++;
  }
}
