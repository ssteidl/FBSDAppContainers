#! /usr/bin/env tclsh8.6
package require vessel::build
package require vessel::native

set usage "USAGE: build_vesselfile.tcl <name> <tag> <vessel_file>"
if {[llength $argv] != 3} {
    puts stderr $usage
    exit 1
}

set build_args [dict create \
		    name [lindex $argv 0] \
		    tag [lindex $argv 1] \
		    file [lindex $argv 2]]

vessel::build::build_command $build_args "util.build" stderr

