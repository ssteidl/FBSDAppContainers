package require vessel::native
package require vessel::bsd
package require vessel::env
package require vessel::jail
package require vessel::metadata_db
package require vessel::zfs
package require fileutil
package require uuid

set current_dataset {}
set cmdline_options {}
set mountpoint {}
set from_called [expr false]
set name {}
set guid {}
set parent_image {}
set cwd {}
set cmd {}
set status_channel stderr

set command_queue [list]

namespace eval vessel::file_commands::_ {
    proc fetch_image {image_dataset name version status_channel} {

	#Download an image.  Ideally we just pull from a repo like
	#any other image.  For now we have a special function because
	#it's not an actual vessel image.  Just a tarball
	
	if {$name ne "FreeBSD" } {
	    
	    return -code error -errorcode {BUILD IMAGE FETCH} \
		"Only FreeBSD images are currently allowed"
	}

	set mountpoint [vessel::zfs::get_mountpoint $image_dataset]
	
	#TODO: Support a registry.  For now only support
	#freebsd base.txz

	#We require the image to be the same as the
	#host architecture
	set arch [vessel::bsd::host_architecture]

	set url "https://ftp.freebsd.org/pub/FreeBSD/releases/$arch/$version/base.txz"

	set old_pwd [pwd]
	set download_dir [vessel::env::image_download_dir]
	try {
	    #We used to pipe these two command together but that would cause issues
	    #with slower internet.
	    cd $download_dir
	    exec -ignorestderr curl -L -O $url
	    exec tar -C $mountpoint -xvf base.txz >&@ stderr
	} finally {
	    cd $old_pwd
	}
    }
}

proc FROM {image} {

    global current_dataset
    global cmdline_options
    global mountpoint
    global from_called
    global name
    global guid
    global parent_image
    global env
    global status_channel

    if {$from_called} {
	return -code error -errorcode {BUILD INVALIDSTATE EFROMALRDY} \
	    "Multiple FROM commands is not supported"
    }

    set parent_image $image
    puts stderr "FROM: $image"

    set pool [vessel::env::get_pool]

    set vessel_parent_dataset [vessel::env::get_dataset]
    
    #Image is in the form <image name>:<version>
    #So image name is the dataset and version is the snapshot name

    set image_tuple [split $image ":"]
    if {[llength $image_tuple] ne 2} {

	return -code error -errorcode {BUILD IMAGE FORMAT} \
	    "Image must be in the format <name:version>"
    }

    set image_name [lindex $image_tuple 0]
    set image_version [lindex $image_tuple 1]
    set snapshot_name "${image_name}:${image_version}@${image_version}"
    set snapshot_path "${vessel_parent_dataset}/${snapshot_name}"

    set snapshot_exists [vessel::zfs::snapshot_exists $snapshot_path]

    if {!$snapshot_exists} {

	set image_dataset "${vessel_parent_dataset}/${image_name}:${image_version}"
	vessel::zfs::create_dataset $image_dataset

	#TODO: This fetch_image should really be just another image.  For now it's a
	#special case.
	vessel::file_commands::_::fetch_image $image_dataset $image_name $image_version $status_channel
	vessel::metadata_db::write_metadata_file $image_name $image_version {/} {/etc/rc} \
	    [list]
	vessel::zfs::create_snapshot $image_dataset $image_version
    }

    # Clone base jail and name new dataset with guid
    set name [dict get $cmdline_options {name}]
    set guid [uuid::uuid generate]

    if {$name eq {}} {
	set name $guid
    }

    set tag [dict get $cmdline_options {tag}]

    #TODO: This default tag to latest is scattered around.  I should put it in
    #a common place.  Probably an image class.
    if {$tag eq {}} {
	set tag latest
    }
    
    #This needs to have the name:tag instead of a cloned dataset of
    #name/tag.  I don't think the latter makes sense because having a parent
    #dataset with different versions doesn't really make sense as the data in
    #the parent is not necessarily relevant to the data in all children. One could be
    #FBSD 11 and the other FBSD 12.  Anyway, handle the tag here.
    set new_dataset "${vessel_parent_dataset}/${name}:${tag}"
    if {![vessel::zfs::dataset_exists $new_dataset]} {
	vessel::zfs::clone_snapshot $snapshot_path $new_dataset
    }
    
    if {![vessel::zfs::snapshot_exists "${new_dataset}@a"]} {
	#Snapshot with version a is used for zfs diff
	vessel::zfs::create_snapshot $new_dataset a
    }

    #Set the current_dataset global var
    set current_dataset $new_dataset

    set mountpoint [vessel::zfs::get_mountpoint $new_dataset]

    set from_called true
    return
}

proc CWD {path} {
    global cwd
    global from_called

    if {!$from_called} {

        return -code error -errorcode {BUILD CWD} \
            "CWD called before FROM.  FROM must be the first command called."
    }
    
    #Verify cwd exists in the dataset
    #set cwd variable
    puts stderr "CWD: $path"

    set cwd $path
}

proc COPY {source dest} {
    global cwd
    global mountpoint
    global from_called
    global status_channel

    if {!$from_called} {

        return -code error -errorcode {BUILD COPY} \
            "COPY called before FROM.  FROM must be the first command called."
    }
    
    #dest is relative to cwd in the jail
    #source is on the host system and is relative to current pwd
    puts $status_channel "COPY: $source -> $dest"

    set absolute_path [fileutil::jail $mountpoint [file join $cwd $dest]]

    file copy -force $source $absolute_path
}

proc EXPOSE {port} {

    #This will need to integrate with pf.
    puts stderr "EXPOSE $port"
}

proc RUN {args} {
    global mountpoint
    global name
    global from_called
    global status_channel

    if {!$from_called} {

        return -code error -errorcode {BUILD CWD} \
            "RUN called before FROM.  FROM must be the first command called."
    }

    if {[llength $args] == 0} {

        return -code error -errorcode {BUILD RUN ARGS} \
            "RUN invoked without arguments" 
    }

    puts $status_channel "RUN mountpoint: $mountpoint"
    set channel_dict [dict create stdin stdin stdout $status_channel stderr $status_channel]
    set network "inherit"

    
    try {

	#Copy resolv conf so we can access the internet
	vessel::env::copy_resolv_conf $mountpoint
	
	#Empty callback signifies blocking mode
	set callback {}

	set volumes [list]
        vessel::jail::run_jail "${name}-buildcmd" $mountpoint $volumes $channel_dict $network $callback {*}$args
    } trap {CHILDSTATUS} {results options} {

        puts stderr "Run failed: $results"
        return -code error -errorcode {BUILD RUN}
    } finally {
	
	vessel::env::remove_resolv_conf $mountpoint
    }

    return
}

proc CMD {args} {

    variable cmd

    if {$cmd ne {}} {
        return -code error -errorcode {BUILD CMD EEXISTS} "Only one CMD is allowed"
    }

    #Set the global cmd output var.
    set cmd $args
}

package provide VesselFileCommands 1.0.0
