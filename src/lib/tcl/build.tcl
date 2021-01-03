# -*- mode: tcl; indent-tabs-mode: nil; tab-width: 4; -*-
package require uuid
package require fileutil
package require vessel::definition_file
package require vessel::env
package require vessel::import
package require vessel::jail
package require vessel::zfs

namespace eval vessel::build {

    namespace eval _ {

        proc build_context_get {build_context build_interp key} {
            set existing_val [dict get $build_context $key]
            set val [$build_interp eval [list set $key]]
            if {$val eq {}} {
                set val $existing_val
            }

            return $val
        }
        
        proc transfer_build_context_from_interp {build_context build_interp} {

            dict set build_context current_dataset \
                [$build_interp eval {set current_dataset}]

            dict set build_context guid [build_context_get $build_context $build_interp {guid}]

            dict set build_context cmd [build_context_get $build_context $build_interp {cmd}]

            dict set build_context cwd [build_context_get $build_context $build_interp {cwd}]

            dict set build_context parent_image [build_context_get $build_context $build_interp {parent_image}]

            return $build_context
        }

        proc execute_vessel_file {build_context vessel_file status_channel} {

            #Sourcing the vessel file calls the functions like RUN
            # and copy at a global level.  We definitely need to do
            # that in a jail.
            set interp_name {build_interp}

            #Create the interp and setup the necessary global options
            interp create $interp_name
            defer::with [list interp_name] {
                interp delete $interp_name
            }
            interp share {} $status_channel $interp_name
            $interp_name eval {
                package require AppcFileCommands
            }
            $interp_name eval [list set ::status_channel $status_channel]
            $interp_name eval [list set ::cmdline_options [dict get $build_context cmdline_options]]
            $interp_name eval {source [dict get $::cmdline_options file]}
            set build_context [transfer_build_context_from_interp $build_context $interp_name]

            return [transfer_build_context_from_interp $build_context $interp_name]
        }
    }
    
    proc build_command {args_dict status_channel}  {

        #build_context maintains the state for this build.  All
        #of the global variables set by sourcing the vessel file
        #are copied into this dict.
        set build_context [dict create \
                               cmdline_options $args_dict \
                               from_called false \
                               parent_image {} \
                               current_dataset {} \
                               cwd {/} \
                               mountpoint {} \
                               name {} \
                               guid {} \
                               cmd {sh /etc/rc} \
                               status_channel $status_channel]
        
        set vessel_file [dict get $build_context cmdline_options {file}]

        #Clones the new dataset, runs commands and creates the '@a' snapshot
        set build_context [_::execute_vessel_file $build_context $vessel_file $status_channel]
        
        #current_dataset is set by the FROM command
        set current_dataset [dict get $build_context current_dataset]

        #Create the b snapshot that will be used to clone children containers
	    set bsnapshot "${current_dataset}@b"
	    if {[vessel::zfs::snapshot_exists $bsnapshot]} {
            vessel::zfs::destroy $bsnapshot
	    }
	    vessel::zfs::create_snapshot $current_dataset {b}
        
        set guid [dict get $build_context guid]
        set name $guid
        if {[dict exists $build_context cmdline_options name]} {
            set name [dict get $build_context cmdline_options name]
        }

        set tag {latest}
        if {[dict exists $build_context cmdline_options tag]} {
            set tag [dict get $build_context cmdline_options tag]
        }

        set cmd [dict get $build_context cmd]
        set cwd [dict get $build_context cwd]
        set parent_image [dict get $build_context parent_image]

        vessel::import::import_image_metadata $name $tag $cwd $cmd $parent_image
    }
}

package provide vessel::build 1.0.0
