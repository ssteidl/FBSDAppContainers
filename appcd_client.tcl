#Client support for communicating with appcd
package require debug
package require appc::native
package require appc::pty_shell

namespace eval appcd::client {
    debug define client
    debug on client 1 stderr
    
    namespace eval _ {

	variable conn {}
	variable vwait_var 0

	#Connects to appcd and returns a non-blocking channel
	proc connect {port} {

	    debug.client "Connecting..."
	    
	    #TODO: Use unix socket so we can check permissions
	    #TODO: Connect async as part of coroutine
	    set appcd_channel [socket {localhost} $port]
	    debug.client "Connected"
	    
	    fconfigure $appcd_channel -encoding {utf-8} \
		-buffering line \
		-translation crlf \
		-blocking 0

	    return $appcd_channel
	}
	
	proc run_coroutine {options} {
	    set args [dict get $options args]
	    set interactive [dict get $args interactive]
	    set appcd_chan [connect 6432]
	    set appcd_msg [dict create cli_options $options pty {}]

	    if {$interactive} {
		debug.client "Opening pty for interactive use"
		set ptys [pty::open]
		set master_chan [lindex $ptys 0]
		set slave_path [lindex $ptys 1]
		set appcd_msg [dict set appcd_msg pty $slave_path]
	    }
	    
	    puts $appcd_chan $appcd_msg
	    #TODO: I'm sure there will need to be more communication here.

	    if {$interactive} {

		debug.client "Starting pty shell [info coroutine]"
		appc::pty_shell::run $master_chan [info coroutine]

		debug.client "Yielding for shell to finish"
		yield
		
		debug.client "pty shell exited.  Exiting event loop."
		variable vwait_var
		exit 0
	    }
	}
    }

    proc main {options} {
	variable _::vwait_var
	
	set command [dict get $options command]
	switch -exact $command {
	    run {
		coroutine client_coro _::run_coroutine $options
	    }
	    default {
		puts stderr "Unknown command: $command"
	    }
	}

	vwait vwait_var
    }
}

package provide appcd::client 1.0.0