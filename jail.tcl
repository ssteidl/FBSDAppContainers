# -*- mode: tcl; indent-tabs-mode: nil; tab-width: 4; -*-

package require appc::native
package require appc::env
package require textutil::expander

namespace eval appc::jail {

    namespace eval _ {

        proc build_network_parameters {network_param} {

            set network_params_dict [dict create]
            if {$network_param eq "inherit"} {
                return [dict set network_params_dict "ip4" "inherit"]
            } elseif {$network_param ne {}} {
                return [dict set network_params_dict "vnet" "1"]
            }

            return $network_params_dict
        }
        
        #Build the jail command which can be exec'd
        proc build_jail_conf {name mountpoint volume_datasets network args} {

            set network_params_dict [build_network_parameters $network]
            
            #Add single quotes around any multi-word parts of the command
            set quoted_cmd [join [lmap word $args {expr {[llength $word] > 1 ?  "\'[join $word]\'" : $word }}]]
            
            #TODO: Run mustache in a safe interpreter
            #TODO: Host and hostname should be different.
            #TODO: Allow setting any jail parameter
            #can contain different values then name
            #NOTE: Everything in the jailconf jail lifecycle after exec.start is pretty much
            #useless because there is no guarantee the jail command is still running.  Most
            #of the cleanup needs to be reimplemented.

            textutil::expander jail_file_expander
            try {
            jail_file_expander evalcmd [list uplevel "#[info level]"]
            
            set jail_conf [jail_file_expander expand {
                [set name] {
                    path="[set mountpoint]";
                    host.hostname="[set name]";
                    sysvshm=new;
                    allow.mount;
                    allow.mount.devfs;
                    mount.devfs;
                    allow.mount.zfs;
                    enforce_statfs=1;
                    [set network_string {}
                     dict for {parameter value} ${network_params_dict} {
                         append network_string "${parameter}=${value};"
                     }
                     set network_string]
                    
                    
                    [set volume_string {}
                     foreach volume $volume_datasets {
                         set jail_string [subst {exec.created="zfs jail $name $volume";\n}]
                         append volume_string $jail_string
                         set mount_string [subst {exec.start+="zfs mount $volume";\n}]
                         append volume_string $mount_string
                     }
                     set volume_string]
                    exec.start+="[set quoted_cmd]";
                }}]
            } finally {
                rename jail_file_expander {}
            }
            return $jail_conf
        }
    }

    proc kill {jid {signal TERM} {output_chan stderr}} {
        # Send signal to all processes in the jail.
        # Allow errors to propagate

        return [exec -ignorestderr jexec $jid kill "-${signal}" -1 >&@ $output_chan]
    }

    proc shutdown {jid {output_chan stderr}} {
        
    }

    proc remove {jid {output_chan stderr}} {

        return [exec -ignorestderr jail -r $jid >&@ $output_chan]
    }
    
    proc run_jail {name mountpoint volume_datasets chan_dict network callback args} {

        #Create the conf file
        set jail_conf [_::build_jail_conf $name $mountpoint $volume_datasets $network {*}$args]
        set jail_conf_file {}
        set jail_conf_file_chan [file tempfile jail_conf_file]

        puts $jail_conf_file_chan $jail_conf
        close $jail_conf_file_chan
        
        set jail_command [list jail -f $jail_conf_file -c $name]
        
        puts stderr "JAIL CONF: $jail_conf"

        #If callback is empty, that implies blocking mode.  non-empty callback implies
        #async mode.  If async, make sure the callback has the jail conf file to cleanup
        set _callback {}
        if {$callback ne {}} {
            set _callback [list $callback $jail_conf_file] 
        }
        appc::exec $chan_dict ${_callback} {*}$jail_command
    }
}

package provide appc::jail 1.0.0
