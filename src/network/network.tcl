package require TclOO
package require vessel::name-gen
namespace eval vessel::network {

    namespace eval _ {

	#Private variables for network ensemble
	variable networks_dict [dict create]
	variable main_bridge {}
	
	#Represents a jail that can join a network.
	#Note this method does not create or destroy
	#a jail.  It's just used to reference and operate
	#on a running jail.
	# TODO: Handle jail resource cleanup after a jail
	# exits.
	oo::class create network_jail {

	    variable _jid
	    variable _ip 
	    variable _epair_interface_obj
	    
	    constructor {network_name jid ip} {
		set _jid $jid
		set _epair_interface_obj [vessel::network::epair new "${network_name}${jid}"]
		set _ip $ip
	    }

	    method jail_id {} {
		return $_jid
	    }

	    method connect_to_network {bridge_obj_ref} {
	        $_epair_interface_obj add_to_bridge $bridge_obj_ref
		set jail_nic [$_epair_interface_obj get_aside]
		exec ifconfig $jail_nic vnet $_jid
		exec jexec $_jid ifconfig $jail_nic $_ip
	    }
	    
	    destructor {
		catch {$_epair_interface destroy}
	    }
	}
    }
    
    oo::class create bridge {

	self export createWithNamespace
	
	variable _name

	method Create {name} {

	    set _name $name
	    if {! [my exists]} {
		puts stderr "Bridge doesn't exist, creating"
		exec ifconfig bridge create name $name
		exec ifconfig $name up
		set _name $name
	    }
	}
	
	constructor {name} {

	    my Create $name
	}
	
	method exists {} {

	    set exists true
	    if {[catch {exec ifconfig $_name} msg]} {
		puts stderr "$msg"
		set exists false
	    }

	    return $exists
	}

	method add_member {member_interface} {

	    if {[catch {exec ifconfig $_name addm $member_interface} msg]} {
		
		set exists_error_pattern {ifconfig: BRDGADD *: File exists}
		
		if {[string match $exists_error_pattern $msg]} {
		    return -code error -errorcode {BRIDGE ADDM EXISTS} $msg
		} else {
		    return -code error -errorcode {BRIDGE ADDM ERROR} $msg
		}
	    }
	}

	method delete_member {member_interface} {

	    exec ifconfig $_name deletem $member_interface
	}

	destructor {
	    puts stderr "destroy bridge"
	    exec ifconfig $_name destroy
	}
    }

    oo::class create epair {

	self export createWithNamespace
	
	variable _name

	#Saves whether or not the interface already existed when this
	#object is created.  We use this to know if the interface should
	#be deleted or not.
	variable _existed
	
	method Create {name} {
	    set _name $name
	    
	    if {![my exists]} {
		set epair_iface [exec ifconfig epair create]

		#ifconfig always returns the 'a' side of the epair
		#We strip the a off so we can easily work with both
		#sides
		set epair_iface [string range $epair_iface 0 end-1]
		exec ifconfig "${epair_iface}a" name "${name}a"
		exec ifconfig "${epair_iface}b" name "${name}b"
	    }
	}

	method Destroy {} {

	    exec ifconfig [my get_aside] destroy
	}
	
	constructor {name} {
	    
	    my Create $name
	}

	method exists {} {

	    set exists true
	    set aside [my get_aside]
	    if {[catch {exec ifconfig $aside} msg]} {
		puts stderr "$aside Exists check: $msg"
		set exists false
	    }

	    return $exists
	}
	
	method get_aside {} {
	    return "${_name}a"
	}
	
	method get_bside {} {
	    return "${_name}b"
	}

	method up {} {
	    exec ifconfig [my get_bside] up
	}
	
	method add_to_bridge {bridge_obj} {

	    set bside [my get_bside]
	    try {

		$bridge_obj add_member $bside
	    } trap {BRIDGE ADDM EXISTS} vlist {

		puts stderr "$bside already a member of bridge"
	    }

	    my up
	}

	method set_ip_addr {ip} {
	    #ip can be with or without the netmask
	    #192.168.3.2 or 192.168.3.2/24
	    exec ifconfig [my get_aside] $ip
	}
	
	destructor {
	    puts stderr "destroy epair"
	    my Destroy
	}
    }

    #NOTE: I'm not using vlans for now.  As an MVP, a single
    #non-vlan network would be better.
    oo::class create vlan {

	self export createWithNamespace
	
	variable _name
	variable _tag_number

	method Create {tag_number vlandev} {
	    set _name "${vlandev}.${tag_number}"
	    set _tag_number $tag_number
	    exec ifconfig $_name create vlan $_tag_number vlandev $vlandev  
	}
	
	constructor {tag_number vlandev} {

	    my Create $tag_number $vlandev
	}

	method set_ip_addr {ip} {
	    #ip can be with or without the netmask
	    #192.168.3.2 or 192.168.3.2/24
	    exec ifconfig $_name $ip
	}
	
	destructor {

	    exec ifconfig $_name destroy
	}
    }
    
    oo::class create internal_network {

	self export createWithNamespace
	
	variable _network_name
	variable _bridge_obj_ref
	variable _ip
	variable _epair_obj

	#dict of network_jail objs.
	variable _jail_dict 
	variable _dns_dict
	
	constructor {network_name bridge_obj {ip {}} {dns_dict {}}} {

	    set _network_name $network_name
	    set _bridge_obj_ref $bridge_obj
	    set _epair_obj [vessel::network::epair new "${network_name}"]
	    $_epair_obj add_to_bridge $_bridge_obj_ref

	    if {$ip ne {} } {
		set _ip $ip
		$_epair_obj set_ip_addr $ip
	    } else {
		#TODO: Query host ip from system
	    }
	    set _jail_dict [dict create]
	    set _dns_list $dns_dict
	}

	method add_jail {jid ip} {

	    if {![dict exists $_jail_dict $jid]} {

		set netjail_obj [vessel::network::_::network_jail new $_network_name $jid $ip]
		dict set $_jail_dict $jid $netjail_obj
		$netjail_obj connect_to_network $_bridge_obj_ref
		
		
	    } else {
		return -code error -errorcode {NETWORK JAIL EXISTS} \
		    "Attempted to add jail to network which has already been added"
	    }
	}
	
	destructor {
	    catch {$_epair_obj destroy}

	    catch {
		dict for {jid network} $_jail_dict {
		    $network destroy
		}
	    }
	}
    }

    proc create  {network_name} {
	variable _::networks_dict
	variable _::main_bridge

	if {[exists $network_name]} {
	    return [dict get $networks_dict $network_name]
	}

	set bridge $main_bridge
	if {$main_bridge eq {}} {
	    set main_bridge [bridge new {vesselbridge}]
	}
	
	set network {}

	#For now always use a class C network between 100 and 200
	set subnet [expr int(rand() * 100) + 100]
	set ip "192.168.${subnet}.1"
	
	set network [internal_network new $network_name $main_bridge $ip]

	dict set networks_dict $network_name $network
	
	return $network
	
    }

    proc exists {name} {
	variable _::networks_dict

	return [dict exists $networks_dict $name]
    }

    proc create_network_cmd {args_dict} {
	#The top level command that is used to create networks
	
	variable _::networks_dict

	set network_name {}
	if {[dict exists $args_dict name]} {
	    set network_name [dict get $args_dict name]
	} else {
	    set network_name [vessel::name-gen::generate-name 2 {} {} {network}]
	}

	return [create $network_name]
    }
}
package provide vessel::network 1.0.0
