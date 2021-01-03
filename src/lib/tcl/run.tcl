# -*- mode: tcl; indent-tabs-mode: nil; tab-width: 4; -*-

package require debug
package require fileutil
package require uuid

package require vessel::bsd
package require vessel::env
package require vessel::jail
package require vessel::zfs

namespace eval vessel::run {

    debug define run
    debug on run 1 stderr
    
    namespace eval _ {

        proc handle_dataset_argument {dataset_arg} {

            set arg_list [split $dataset_arg ":"]
            if {[llength $arg_list] != 2} {
                return -code error -errorcode {DATASET ARGS} "dataset requires a value in the format <source>:<dest>"
            }

            set dataset [lindex $arg_list 0]
            set jail_mntpoint [lindex $arg_list 1]

            if {![vessel::zfs::dataset_exists $dataset]} {
                vessel::zfs::create_dataset $dataset
            }

            if {[catch {vessel::zfs::set_mountpoint_attr $dataset $jail_mntpoint} error_msg]} {
                debug.run "Failed to set mountpoint attribute on $dataset.  Ignoring: $error_msg"
            }
            vessel::zfs::set_jailed_attr $dataset

            return $dataset
        }
        
        proc handle_volume_argument {mountpoint volume_arg} {

            set arg_list [split $volume_arg ":"]
            if {[llength $arg_list] != 2} {
                return -code error -errorcode {MOUNT ARGS} "volume requires a value in the format <source>:<dest>"
            }

            set sourcedir [lindex $arg_list 0]
            set destdir [lindex $arg_list 1]
            set jailed_dest_path [fileutil::jail $mountpoint $destdir]
            
            if {[file exists $sourcedir] && [file isdirectory $sourcedir]} {

                vessel::bsd::null_mount $sourcedir $jailed_dest_path
            } else {

                return -code error -errorcode {MOUNT DEST EEXIST} "Source path for volume argument does not exist"
            }

            return $jailed_dest_path
        }
    }

    proc run_command {chan_dict args_dict cb_coro} {

        defer::with [list cb_coro] {
            after idle $cb_coro
        }

        #Initialization yield
        yield

        #TODO: All parameters should be validated before any cloning is done so proper
        # cleanup can be done.  right now we clone and then fail on poorly formatted
        # parameters.
        
        #TODO: IMAGE should be it's own class
        set image [dict get $args_dict "image"]
        set image_components [split $image :]
        set image_name [lindex $image_components 0]
        set network [dict get $args_dict "network"]
        set tag {latest}
        if {[llength $image_components] > 1} {
            set tag [lindex $image_components 1]
        } elseif {[dict exists $args_dict tag]} {
            set tag [dict get $args_dict tag]
        }

        set mountpoints_dict [vessel::zfs::get_mountpoints]

        if {$tag eq {local}} {
            #If tag is the special name "local" then don't clone the dataset.  This can
            #can be useful for development and testing.
            set tag {local}
        }
        set image_dataset [vessel::env::get_dataset_from_image_name $image_name $tag]
        debug.run "RUN COMMAND image dataset: $image_dataset"

        if {![dict exists $mountpoints_dict $image_dataset]} {
            #TODO: retrieve and unpack layer
            return -code error -errorcode {VESSEL RUN NYI} \
                "Automatically pulling images is NYI"
        }

        set b_snapshot_exists [vessel::zfs::snapshot_exists "${image_dataset}@b"]
        debug.run "RUN COMMAND b snapshot exists: $b_snapshot_exists"
        if {$b_snapshot_exists && $tag ne {local}} {

            set uuid [uuid::uuid generate]
            set container_dataset [vessel::env::get_dataset_from_image_name $image_name ${tag}/${uuid}]

            puts stderr "Cloning b snapshot: ${image_dataset}@b $container_dataset"
            vessel::zfs::clone_snapshot "${image_dataset}@b" $container_dataset
        } else {

            #Support manually created or pulled datasets
            set container_dataset "${image_dataset}"
        }
        
        set mountpoint [vessel::zfs::get_mountpoint $container_dataset]

        #nullfs volume mounts
        set jailed_mount_paths [list]
        foreach volume [dict get $args_dict volumes] {
            lappend jailed_mount_paths [_::handle_volume_argument $mountpoint $volume]
        }

        set volume_datasets [list]
        foreach volume_dataset_arg [dict get $args_dict datasets] {
            set dataset [_::handle_dataset_argument $volume_dataset_arg]
            lappend volume_datasets $dataset
        }

        vessel::env::copy_resolv_conf $mountpoint
        
        set command [dict get $args_dict "command"]
        set jail_name [uuid::uuid generate]
        if {[dict exists $args_dict "name" ]} {
            set jail_name [dict get $args_dict "name"]
        }

        set coro_name [info coroutine]
        set error [catch {
            vessel::jail::run_jail $jail_name $mountpoint $volume_datasets $chan_dict $network $coro_name {*}$command
        } error_msg info_dict]
        if {$error} {
            return -code error -errorcode {VESSEL JAIL EXEC} "Error running jail: $error_msg"
        }
        
        #Wait for the command to finish
        set tmp_jail_conf [yield]
        debug.run "Container exited. Cleaning up..."
        file delete $tmp_jail_conf

        debug.run "Removing jail: $jail_name"
        if {[catch {exec jail -r $jail_name >&@ stderr} error_msg]} {
            puts stderr "Unable to remove jail.  Attempting to cleanup filesystem: $error_msg"
        }

        debug.run "Unmounting jail dev filesystem"
        if {[catch {vessel::bsd::umount "${mountpoint}/dev"} error_msg]} {
            puts stderr "Unable to umount dev filesystem, continuing on...: $error_msg"
        }

        debug.run "Unmounting nullfs volumes"
        foreach volume_mount $jailed_mount_paths {
            set error [catch {vessel::bsd::umount $volume_mount} error_msg]
            if {$error} {
                puts stderr "Error unmounting $volume_mount: $error_msg"
            }
        }

        debug.run "Unjailing jailed datasets"
        foreach volume_dataset $volume_datasets {
            catch {exec zfs set jailed=off $volume_dataset >&@ stderr}
            catch {exec zfs umount $volume_dataset >&@ stderr}
        }
        
        if {[dict get $args_dict "remove"]} {
            debug.run "Destroying container dataset: $container_dataset"
            vessel::zfs::destroy $container_dataset
        }

        debug.run "Finished running container: $jail_name"
    }
}

package provide vessel::run 1.0.0
