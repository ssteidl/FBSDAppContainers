# -*- mode: tcl; indent-tabs-mode: nil; tab-width: 4; -*-

package require appc::native
package require appc::env

namespace eval appc::jail {

    namespace eval _ {

        #Build the jail command which can be exec'd
        proc build_command {name mountpoint args} {

            #TODO: Allow run jail command parameters to be overridden
            #TODO: Support ip4=inherit
            array set jail_parameters [list \
                                           "host.hostname" $name \
                                           "name" $name \
                                           "sysvshm" "new" \
                                           "allow.mount" "1" \
                                           "allow.mount.devfs" "1" \
                                           "mount.devfs" "1" \
                                           "vnet" "1"]

            #TODO: Allow user to configure shell parameter
            set shell {/bin/sh}

            set jail_cmd "jail -c path=$mountpoint"
            foreach {param value} [array get jail_parameters] {

                set jail_cmd [string cat $jail_cmd " " "${param}=${value}"]
            }

            #Add single quotes around any multi-word parts of the command
            set quoted_cmd [join [lmap word $args {expr {[llength $word] > 1 ?  "\'[join $word]\'" : $word }}]]
            
            set jailed_cmd "$shell -c \"$quoted_cmd\""
            set command_parameter "command=$jailed_cmd"
            return [string cat $jail_cmd " " $command_parameter]
        }
    }

    proc tmp_callback {} {

        puts stderr "Temp callback for jail complete"
    }
    
    proc run_jail {name mountpoint pty callback args} {

        set jail_command [_::build_command $name $mountpoint {*}$args]
        puts stderr "JAIL COMMAND: $jail_command"
        puts stderr "using pty: $pty"

        #Interactive
        set pty_chan [open $pty RDWR]
        set chan_dict [dict create stdin $pty_chan stdout $pty_chan stderr $pty_chan]
        appc::exec $chan_dict $callback {*}$jail_command
    }
}

package provide appc::jail 1.0.0
