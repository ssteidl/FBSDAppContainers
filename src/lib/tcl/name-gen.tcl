namespace eval vessel::name-gen {
    
    namespace eval _ {
	variable name_list [list]

	proc load_namelist {} {
	    variable name_list
	    set proper_names_file {/usr/share/dict/propernames}

	    if {[llength $name_list] > 0} {
		return -code ok
	    }
	    
	    if {! [file exists $proper_names_file]} {
		return -code error -errorcode {NAMEGEN ENOENT} \
		    "Proper names file does not exist"
	    }
	    set names_chan [open $proper_names_file]

	    set name {}
	    while {[gets $names_chan name] >= 0} {
		lappend name_list $name
	    }
	}

	proc generate_random_name {} {
	    variable name_list
	    set name_count [llength $name_list] 
	    set name_offset [expr {int(($name_count + 1) * rand())}]

	    set random_name [lindex $name_list $name_offset]
	    return $random_name
	}
    }

    proc generate-name {{num_components 2} {prefix {}} {separator -} {suffix {}} } {

	_::load_namelist

	set name_components [list $prefix]
	for {set i 0} {$i < $num_components} {incr i} {
	    lappend name_components [_::generate_random_name]
	}

	lappend name_components $suffix

	#Filter out the empty components
	set filtered [lmap component $name_components {expr {$component ne {} ? $component : [continue]}}]
	return [join $filtered $separator]
    }
}

package provide vessel::name-gen 1.0.0
