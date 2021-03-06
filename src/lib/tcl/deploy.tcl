package require inifile
package require defer

namespace eval vessel::deploy {

    namespace eval ini {

	namespace eval _ {
	    proc validate_section_name {name} {
			if {$name eq {vessel-supervisor}} {
				return
			}

			if {[string first {dataset:} $name] != -1} {
				return
			}

			if {$name eq {jail}} {
				return
			}

			if {[string first {resource:} $name] != -1} {
				return
			}

			return -code error -errorcode {VESSEL INI SECTION UNEXPECTED} \
				"Unexpected section $name"
			}
		}
	
	proc get_deployment_dict {ini_file} {
	    set fh [ini::open $ini_file]
	    defer::with [list fh] {
		ini::close $fh
	    }
	    set sections [ini::sections $fh]

	    set deployment_dict [dict create]
	    foreach section $sections {
		dict set deployment_dict $section [ini::get $fh $section]
		_::validate_section_name $section
	    }
	    
	    return $deployment_dict
	}
    }
}

package provide vessel::deploy 1.0.0
