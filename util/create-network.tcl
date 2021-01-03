#! /usr/bin/env tclsh8.6

package require cmdline
package require vessel::network

proc create_network {netname} {

    puts stderr "netname: $netname"
    
    #Intentionally don't destroy these objects so the
    #OS objects (interfaces) are not destroyed.
    set args_dict [dict create name $netname]
    set internal_network_obj [vessel::network::create_network_cmd $args_dict]
}

set options {
    {network.arg "" "Name of the network to create"}
}

set usage "create-network --network=<name>"

try {
    array set params [::cmdline::getoptions argv $options $usage]
} trap {CMDLINE USAGE} {msg o} {
    # Trap the usage signal, print the message, and exit the application.
    # Note: Other errors are not caught and passed through to higher levels!
    puts stderr $msg
    exit 1
}

parray params

set netname $params(network)

create_network $netname
