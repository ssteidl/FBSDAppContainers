package require debug
package require vessel::env
package require vessel::metadata_db
package require vessel::native
package require defer
package require fileutil
package require json
package require uri
package require TclOO

namespace eval vessel::import {
    #vessel::import is responsible for unpacking and "importing"
    #vessel images into the zfs pool

    debug define import
    debug on import 1 stderr

    namespace eval _ {

	proc create_layer {image tag extracted_path status_channel} {

	    # We don't know the uuid so we glob in the image_dir and
	    # use what should be the only thing there
	    set layer_file_glob [glob "${extracted_path}/*-layer.tgz"]
	    if {[llength $layer_file_glob] != 1} {
		return -code error -errorcode {VESSEL IMPORT LAYER} \
		    "Unexpected number of layer files in image: $layer_file_glob"
	    }
	    set layer_file [lindex $layer_file_glob 0]

	    #Read parent image from metadata file
	    set extracted_metadata_file [file join ${extracted_path} "${image}:${tag}.json"]
	    set metadata_json [fileutil::cat $extracted_metadata_file]
	    set metadata_dict [json::json2dict $metadata_json]

	    #NOTE: The metadata file allows a list of parent images.
	    #We current only support one layer of parent images.  In
	    #the future we can use arbitrarily long chain of parent
	    #images.
	    set parent_image [lindex [dict get $metadata_dict parent_images] 0]
	    set parent_image_components [split $parent_image :]
	    set parent_image_name [lindex $parent_image_components 0]
	    set parent_image_version [lindex $parent_image_components 1]
	    set parent_image_snapshot [vessel::env::get_dataset_from_image_name $parent_image_name $parent_image_version]
	    set parent_image_snapshot "${parent_image_snapshot}@${parent_image_version}"
	    
	    #TODO: fetch the parent image if it doesn't exist.  We can
	    #move the fetch_image command from the vessel_file_commands
	    if {![vessel::zfs::snapshot_exists $parent_image_snapshot]} {
		return -code error -errorcode {NYI} \
		    "Pulling parent image is not yet implemented: '$parent_image_snapshot'"
	    }

	    #Clone parent filesystem
	    set new_dataset [vessel::env::get_dataset_from_image_name $image $tag]
	    if {![vessel::zfs::dataset_exists $new_dataset]} {
		debug.import "Cloning parent image snapshot: '$parent_image_snapshot' to '$new_dataset'"
		vessel::zfs::clone_snapshot $parent_image_snapshot $new_dataset
	    }

	    if {![vessel::zfs::snapshot_exists ${new_dataset}@a]} {
		vessel::zfs::create_snapshot ${new_dataset} a
	    }
	    
	    #Now clone the new dataset into a version dataset.  This means
	    #that the 'new_dataset' will always be the same as its parent
	    #dataset.  We have to do this because we can't have a dataset
	    #without a parent.  So for example, the vesseldevel image (named
	    #devel) can't have a dataset called <vessel_pool>/jails/devel/0
	    #without first having <vessel_pool>/jails/devel
	    #NOTE: I think all of this can be deleted.
	    # set versioned_new_dataset [vessel::env::get_dataset_from_image_name $image_name $version]
	    # if {![vessel::zfs::dataset_exists $versioned_new_dataset]} {
	    # 	vessel::zfs::clone_snapshot ${new_dataset}@a $versioned_new_dataset
	    # }

	    # if {![vessel::zfs::snapshot_exists ${versioned_new_dataset}@a]} {
	    # 	vessel::zfs::create_snapshot $versioned_new_dataset a
	    # }
	    
	    #Untar layer on top of parent file system
	    set mountpoint [vessel::zfs::get_mountpoint $new_dataset]
	    exec tar -C $mountpoint -xvf $layer_file >&@ $status_channel
	    flush $status_channel

	    #TODO: delete files from whitelist
	    if {[vessel::zfs::snapshot_exists ${new_dataset}@b]} {
		#If the b snapshot already exists then we need to delete it and
		#make a new one.
		vessel::zfs::destroy ${new_dataset}@b
	    }
	    vessel::zfs::create_snapshot $new_dataset b

	    #Very last thing is importing the image metadata.  We import
	    #instead of copying the file to safeguard against mismatching versions
	    vessel::import::import_image_metadata_dict $metadata_dict
	}
    }

    proc import_image_metadata {name tag cwd cmd parent_images} {
	#Used when the image already exists (maybe it was built) and
	# we just need to store the metadata.

        vessel::metadata_db::write_metadata_file $name $tag $cwd $cmd $parent_images
    }

    proc import_image_metadata_dict {metadata_dict} {
	set name [dict get $metadata_dict name]
	set tag [dict get $metadata_dict tag]
	set command [dict get $metadata_dict command]
	set cwd [dict get $metadata_dict cwd]
	set parent_images [dict get $metadata_dict parent_images]

	import_image_metadata $name $tag $cwd $command $parent_images
    }
    
    proc import {image tag image_dir status_channel} {
	# Import an image into the vessel environment.
	#
	# params:
	#
	# image_w_tag: The name of the image without tag appended
	# tag: The tag of the image
	# image_dir: The directory where the image file resides
	debug.import "import{}: ${image},${tag}"

	set extracted_path [file join $image_dir "${image}:${tag}"]

	#Extract files into extracted_path overriding files that already exist.
	exec unzip -o -d $extracted_path [file join $image_dir "${image}:${tag}.zip"] >&@ $status_channel
	_::create_layer $image $tag $extracted_path $status_channel

    }
}

package provide vessel::import 1.0.0
