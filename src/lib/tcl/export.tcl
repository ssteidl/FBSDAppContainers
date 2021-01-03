package require vessel::env
package require vessel::metadata_db
package require vessel::zfs
package require debug
package require uuid

namespace eval vessel::export {

    debug define export
    debug on run 1 stderr
    
    namespace eval _ {

	proc create_layer {dataset guid status_channel} {

	    #Create the layer by zfs diff'ing the 'a' snapshot with the
	    # dataset filesystem.  A layer is a zip archive of
	    # the differences in the snapshots.

            set mountpoint [vessel::zfs::get_mountpoint $dataset]
            set diff_dict [vessel::zfs::diff ${dataset}@a ${dataset}]

            set workdir [vessel::env::get_workdir]
            set build_dir [file join $workdir $guid]
            
            file mkdir $build_dir
            set old_dir [pwd]
            cd $build_dir
            set whiteout_file [open {whiteouts.txt} {w}]

            if {[dict exists $diff_dict {-}]} {
                foreach deleted_file [dict get $diff_dict {-}] {
                    puts $whiteout_file $deleted_file
                }
            }

	    set modified_links_dict [vessel::zfs::diff_hardlinks $diff_dict $mountpoint]
            
            set tar_list_file [open {files.txt} {w}]

            #If the first line of the file is a '-C' then tar will chdir to the directory
            # specified in the second line of the file.
            puts $tar_list_file {-C}
            puts $tar_list_file "$mountpoint"

            dict for {inode paths} $modified_links_dict {

		#Put all the paths in the file to be tar'd up.
		foreach path $paths {
		    puts $tar_list_file [fileutil::stripPath "${mountpoint}" $path]
		}
            }
	    
            flush $tar_list_file
            close $tar_list_file
            exec -ignorestderr tar -cavf ${guid}-layer.tgz -n -T {files.txt} >&@ $status_channel

            #TODO: Use a trace to return to the old directory. Or better
            # yet, don't change directory
            cd $old_dir
            return $build_dir
        }

        proc create_image {image_name image_tag output_dir status_channel} {

	    #Create the image metadata and zip it with the layer.  An image
	    # is the layer with the metadata zipped together.
	    
	    set output_image_path [file join $output_dir "${image_name}:${image_tag}.zip"]
	    if {[file exists $output_image_path]} {
		#Short circuit if the image already exists.
		return $output_image_path
	    }
	    #Create the image layer.
	    set dataset [vessel::env::get_dataset_from_image_name $image_name $image_tag]
	    set guid [uuid::uuid generate]
	    set image_dir [create_layer $dataset $guid $status_channel]

	    set metadata_file [vessel::metadata_db::metadata_file_path $image_name $image_tag]
	    file copy $metadata_file $image_dir
            set workdir [vessel::env::get_workdir]

            #Ensure we cd back to the initial directory. Should probably just
            # use tcllib::defer
            trace add variable workdir unset [list apply {{old_pwd _1 _2 _3} {
                cd $old_pwd
            } } [pwd]]
	    cd $image_dir
	    
            exec zip -v -r -m $output_image_path . >&@ $status_channel
	    return $output_image_path
        }
    }
    
    proc export_image {image tag output_dir} {

	return [_::create_image $image $tag $output_dir stderr]
    }

    proc export_command {args_dict} {

	#TODO: Audit the code base and move all of this code into an
	# image class
	set image_w_tag [dict get $args_dict image]
	set image_components [split $image_w_tag :]
	set image_name [lindex $image_components 0]
	set image_tag {latest}
	if {[llength $image_components] > 1} {
	    set image_tag [lindex $image_components 1]
	}

	set output_dir [dict get $args_dict dir]
	return [export_image $image_name $image_tag $output_dir]

	#TODO: We should get rid of the export command.  It should just be part of
	#the publish command with a file:// scheme
    }
}

package provide vessel::export 1.0.0
