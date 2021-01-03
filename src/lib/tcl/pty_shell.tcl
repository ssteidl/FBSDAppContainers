#Shell implementation for attaching to a pty
#and passing through stdin/stdout to the current
#controlling terminal

package require vessel::native

namespace eval vessel::pty_shell {

    proc run {master done_script_prefix} {

	#Set the size of the new pty to the size of the current
	#terminal.  This is a good starting point and as long
	#as the size of the terminal (or X window terminal emulator)
	#isn't changed, then it should work very well.  When stdin
	#and pty are mismatched in size, it is very obvious when using
	#man or typing long commands.
	pty::copy_winsz stdin $master

	#Outside of setting blocking to false, the other configurations
	#probably aren't necessary.
	fconfigure stdin -blocking false -translation binary -buffering none
	fconfigure $master -blocking false -translation binary -buffering none
	fconfigure stdout -buffering none -translation binary -buffering none

	#Make stdin and stdout raw so that signals are passed though
	#to the process on the slave end of the pty
	set old_stdin_settings [pty::makeraw stdin]
	set old_stdout_settings [pty::makeraw stdout]

	#Async proxy input from stdin to the pty
	fileevent stdin readable [list apply {{input_chan output_chan} {
	    set data [read $input_chan]
	    puts -nonewline $output_chan $data
	}} stdin $master]

	#Async proxy input from the pty to stdout.  Closing when the pty closes.
	fileevent $master readable [list apply {{input_chan output_chan stdin_restore stdout_restore callback} {
	    set data [read $input_chan]
	    if {[eof $input_chan]} {
				
		pty::restore stdout $stdout_restore
		pty::restore stdin $stdin_restore
		close $input_chan
		{*}$callback
		
	    } else {
		puts -nonewline $output_chan $data
	    }
	}} $master stdout $old_stdin_settings $old_stdout_settings $done_script_prefix]
    }
}

package provide vessel::pty_shell 1.0.0
