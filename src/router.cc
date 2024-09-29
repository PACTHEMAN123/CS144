#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  route_entry re;
  re.route_prefix = route_prefix;
  re.prefix_length = prefix_length;
  re.next_hop = next_hop;
  re.interface_num = interface_num;
  route_table.push_back(re);
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  for(size_t i = 0; i < _interfaces.size(); i++) {
    // for each datagram
    while(!((interface(i)->datagrams_received()).empty())) {

      InternetDatagram dgram = interface(i)->datagrams_received().front();
      
      // search the routing table and match
      uint32_t dst = dgram.header.dst;
      uint32_t longest_length = 0;
      route_entry *longest_prefix_match = nullptr;
      auto it = route_table.begin();
      for(; it != route_table.end(); it++) {
        route_entry e = *it;

        // search the match
        if(e.prefix_length == 0) {
          if(longest_length == 0)
            longest_prefix_match = &(*it);
          continue;
        }

        uint32_t prefix1 = (dst >> (32 - e.prefix_length)) << (32 - e.prefix_length);
        uint32_t prefix2 = (e.route_prefix >> (32 - e.prefix_length)) << (32 - e.prefix_length);
        
        if(prefix1 == prefix2) {
          // among the match, choose the longest-prefix-match route
          if(e.prefix_length > longest_length) {
            longest_length = e.prefix_length;
            longest_prefix_match = &(*it);
          }
        }
      }

      // if not found, drop the datagram
      if(longest_prefix_match == nullptr) {
        interface(i)->datagrams_received().pop();
        continue;
      }
        

      // decrement TTL
      if(dgram.header.ttl == 0 || dgram.header.ttl == 1) {
        interface(i)->datagrams_received().pop();
        continue;
      }  
      dgram.header.ttl -= 1;
      dgram.header.compute_checksum();

      // send modified datagram to appropriate interface
      if(longest_prefix_match->next_hop.has_value())
        interface(longest_prefix_match->interface_num)->send_datagram(dgram, (longest_prefix_match->next_hop).value());
      else 
        interface(longest_prefix_match->interface_num)->send_datagram(dgram, Address::from_ipv4_numeric(dgram.header.dst));
      interface(i)->datagrams_received().pop();
    }
    
  }
}
