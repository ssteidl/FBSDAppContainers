# -*- mode: tcl; indent-tabs-mode: nil; tab-width: 4; -*-
package require TclOO
package require json::write

oo::class create definition_file {

    variable m_whiteout_list

    #Each command is a string of values that can
    # be expanded.
    variable m_commands
    

    constructor {} {

        set m_whiteout_list [list]
        set m_commands [list]
    }

    method add_whiteout_path {path} {

        lappend m_whiteout_list $path
    }

    method add_command {command} {
        lappend m_commands $command
    }

    method serialize {} {

        json::write indented false
        json::write aligned false

        return [json::write object \
                    "whiteout" [json::write array {*}[lmap whiteout $m_whiteout_list {json::write string $whiteout}]] \
                    "commands" [json::write array {*}[lmap command $m_commands {json::write string $command}]]]
    }

    method write {path} {

        set container_chan [open $path "w" 0600]

        puts $container_chan [my serialize]
        close $container_chan
    }
}

package provide vessel::definition_file 1.0.0
# definition_file create o
# o add_whiteout_path {/yippity/doo/dah}
# o add_whiteout_path {/yippity/doo/dac}
# o add_command {echo 'hello world'}
# puts [o serialize]
# o write ./test.config
