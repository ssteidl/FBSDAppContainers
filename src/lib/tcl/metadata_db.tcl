package require vessel::env
package require vessel::zfs
package require defer
package require fileutil
package require json
package require json::write
package require struct::matrix

#NOTE: The metadata db was going to be an sqlite database that
# held metadata for images and containers.  That may still happen but
# initially it makes more sense to just have a directory of json files
# that can be easily transformed into jail files.


namespace eval vessel::metadata_db {
    #The matadata_db module is an interface to get all metadata from
    #images and containers.  The actual store for the metadata varies.
    #We try to not duplicate data.  For instance, if metadata can be found
    #via the filesystem or zfs command, we aren't going to duplicate that
    #data in a database or json file.  

    namespace eval _ {
	
	proc dict_get_value {dict key default_value} {

	    if {[dict exists $dict $key]} {
		return [dict get $dict $key]
	    }

	    return $default_value
	}
	
	proc create_metadata_json {metadata_dict} {
	    # metadata_dict params:
	    #    name: Name of the image.  Required
	    #    tag: Tag of the image.  Defaults to latest
	    #    command: command to be ran /etc/rc by default
	    #    cwd: Initial working directory
	    #    parent_images: Currently will always be some version of FreeBSD defaults
	    #                  to FreeBSD:12.1.  In the future we can make it a sorted list
	    #                  of images. This is expected to be a list with a single element

	    set name [dict_get_value $metadata_dict {name} {}]
	    if {$name eq {}} {
		return -code error -errorcode {METADATA REQUIRED NAME} \
		    "name field must exist in dictionary"
	    }

	    set tag [dict_get_value $metadata_dict {tag} {latest}]
	    set command [dict_get_value $metadata_dict {command} {/etc/rc}]
	    set cwd [dict_get_value $metadata_dict {cwd} {/}]
	    set parent_images [dict_get_value $metadata_dict {parent_images} {FreeBSD:12.1}]

	    set json_str [json::write object \
			      "name" [json::write string $name] \
			      "tag" [json::write string $tag] \
			      "command" [json::write array \
					     {*}[lmap l $command {json::write string $l}]] \
			      "cwd" [json::write string $cwd] \
			      "parent_images" [json::write array \
					       {*}[lmap l $parent_images {json::write string $l}]]]

	    return $json_str
	}

	proc read_metadata_json {dataset} {

	    # params:
	    #
	    # dataset: The complete name of the dataset.  zroot/vessel-test/FreeBSD:12.1
	    
	    set components [split $dataset /]
	    set child_name [lrange $components end end]
	    set filename [file join [vessel::env::metadata_db_dir] "${child_name}.json"]

	    set json [fileutil::cat $filename]

	    set metadata_dict [json::json2dict $json]
	    return $metadata_dict
	}
	
	proc list_images {output_chan} {

	    #Output script consumable fields for each image to output_chan.
	    
	    set headers [list ID PARENT COMMAND]
	    set vessel_dataset [vessel::env::get_dataset]

	    set dataset_list [vessel::zfs::dataset_children $vessel_dataset]

	    #We don't care about the parent dataset
	    set children_list [lrange $dataset_list 1 end]

	    set matrix_name {image_output_matrix}
	    struct::matrix $matrix_name
	    $matrix_name add columns 3

	    set image_data [list]
	    foreach child_dict $children_list {
		set dataset_name [dict get $child_dict {name}]

		#We don't want to list container images here. So we only use
		#images with 3 parts when we split on /.  An example
		#container image is:
		#zroot/jails/minimal:hackathon/0bdefddd-a753-48a8-bd85-750d5031464d
		set component_count [llength [split $dataset_name /]]
		if {$component_count == 4} {
		    continue
		}
		
		set dataset_metadata [read_metadata_json $dataset_name]

		set command [dict get $dataset_metadata command]
		set parent_image [lindex [dict get $dataset_metadata parent_images] 0]
		set dataset_output_list [list $dataset_name \
					     $command \
					     $parent_image]

		set image_data [lappend image_data $dataset_output_list]
	    }

	    set matrix_cmd {output_matrix}
	    struct::matrix $matrix_cmd
	    defer::defer [list matrix_cmd] {
		$matrix_cmd destroy
	    }
	    
	    $matrix_cmd add columns 3
	    $matrix_cmd add row [list DATASET CMD PARENT]
	    foreach row $image_data {
		$matrix_cmd add row $row
	    }

	    
	    puts $output_chan [$matrix_cmd format 2chan]
	}
    }

    proc metadata_file_path {image_name tag} {
	set metadata_dir [vessel::env::metadata_db_dir]
	set metadata_file [file join $metadata_dir "${image_name}:${tag}.json"]
	return $metadata_file
    }
    
    proc write_metadata_file {image_name tag cwd cmd parent_images} {

	#params:
	#
	# image_name: the image name without the tag appended
	# tag: The tag for the image
	# cwd: current working directory when the container is started
	# cmd: Command to run by default
	# parent_image: The one parent image
	

	set json_content [_::create_metadata_json [dict create name $image_name \
						       tag $tag \
						       command $cmd \
						       cwd $cwd \
						       parent_images $parent_images]]

        try {
	    #Tempfile rename to avoid corruption
	    #TODO: If I'm going to be doing this stuff, I might as well just use sqlite
	    set tmp_file_chan [file tempfile file_path "${image_name}.json"]
	    puts $tmp_file_chan $json_content
	    flush $tmp_file_chan
	    set metadata_file [metadata_file_path $image_name $tag]
	    file rename -force $file_path $metadata_file
	    
	} finally {

	    catch {close $file_chan}
	}

	return $metadata_file
    }
    
    proc image_command {args_dict} {

	puts stderr "args_dict: $args_dict"
	_::list_images stdout

    }
}

package provide vessel::metadata_db 1.0.0
