#!/usr/bin/env tclsh8.6
# -*- mode: tcl; indent-tabs-mode: nil; tab-width: 4; -*-

package require TclOO
package require dicttool
package require vessel::native

namespace eval vessel::dns {

    namespace export create_server
    namespace ensemble create
    
    namespace eval _ {

        variable domain_store [dict create]
        
        oo::class create transform {

            constructor {} {

            }

            method initialize {handle mode} {

                return "initialize clear finalize read write"
            }

            method clear {} {
                
            }

            method finalize {handle} {

            }

            method limit {handle} {
                return 0
            }

            method read {handle buffer} {

                set query [vessel::dns::parse_query $buffer]
                return $query
            }

            method write {handle buffer} {

                if {![dict exists $buffer raw_query] ||
                    ![dict exists $buffer address] ||
                    ![dict exists $buffer ttl]} {
                    return -code error -errorcode {DNS RESPONSE INVALID} \
                        "writing response to dns channel must be a dict with 'raw_query', 'address' and 'ttl' keys"
                }

                set addr [dict get $buffer address]
                set ttl [dict get $buffer ttl]
                set raw_query [dict get $buffer raw_query]
                # buffer should be an ipv4 address string or an empty string
                set response [vessel::dns::generate_A_response $addr $ttl $raw_query]
                return $response
            }

            destructor {

                puts "Transform destructor"
            }
        }
        
        oo::class create DNSServer {

            variable store [dict create]
            variable udp_channel
            variable message_channel

            constructor {port ip} {

                #Add support for listening on a specific ip
                #address
                set udp_channel [vessel::udp_open -myaddr $ip $port]
                fconfigure $udp_channel -translation binary -buffering none -blocking false

                #TODO: Proper way to delete this
                set dns_transform [vessel::dns::_::transform new]
                set message_channel [chan push $udp_channel $dns_transform]
                fileevent $udp_channel readable [list [self] pkt_ready]

            }

            method pkt_ready {} {

                #Callback method invoked when the udp channel has a packet ready

                #Read the packet
                set pkt [read $message_channel]

                #Configure the channel for response
                fconfigure $udp_channel -remote [fconfigure $udp_channel -peer]

                #Create the response from the stored entries
                set address {}
                set qname [dict getnull $pkt "qname"]
                if {$qname ne {}} {
                    set entry [dict getnull $store $qname]
                    set address [lindex $entry 0]
                    set ttl [lindex $entry 1]
                }

                if {$ttl eq {}} {
                    set ttl 0
                }
                
                set response [dict create]
                dict set response address $address
                dict set response raw_query [dict get $pkt raw_query]
                dict set response ttl $ttl

                #Respond
                puts -nonewline $message_channel $response
                flush $message_channel       
            }

            method add_lookup_mapping {name ip {ttl 0}} {
                set store [dict set store $name [list $ip $ttl]]
            }
            
            destructor {
                close $udp_channel
            }
            
        }
    }

    proc create_server {{port 53} {ip {0.0.0.0}}} {

        return [_::DNSServer new $port $ip]
    }
}

package provide vessel::dns 1.0.0
