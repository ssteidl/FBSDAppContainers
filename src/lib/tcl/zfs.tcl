# -*- mode: tcl; indent-tabs-mode: nil; tab-width: 4; -*-

namespace eval vessel::zfs {
    #TODO: Make ensemble
    
    proc get_pools {} {

        set pools [exec zpool list -H]
        set lines [split $pools \n]

        set pools_dict [dict create]
        set headers [list name size alloc free ckpoint expandsz frag cap dedup health altroot]
        foreach pool_line $lines {

            set dict_list [list]
            lmap pool $pool_line header $headers {lappend dict_list $header $pool}

            set name [lindex $pool_line 0]
            dict set pools_dict $name $dict_list
        }

        return $pools_dict
    }

    proc get_snapshots {} {

        set snapshots [exec zfs list -H -t snap]
        set lines [split $snapshots \n]

        set snapshots_dict [dict create]
        set headers [list name used avail refer mountpoint]
        foreach snapshot_line $lines {

            set dict_list [list]
            lmap snapshot $snapshot_line header $headers {lappend dict_list $header $snapshot}

            set name [lindex $snapshot_line 0]
            dict set snapshots_dict $name $dict_list
        }

        return $snapshots_dict
    }

    proc clone_snapshot {snapshot new_dataset} {

        variable snapshots_dict
        if {![snapshot_exists $snapshot]} {

            puts stderr "Attempted to clone a non existent snapshot: $snapshot"
            puts stderr "${snapshots_dict}"
            #TODO: Raise error instead of exit
            exit 1
        }

        exec zfs clone $snapshot $new_dataset >&@ stderr
    }

    proc get_mountpoints {} {

        set mountpoints [exec zfs get -H mountpoint]
        set lines [split $mountpoints \n]
        
        set mp_dict [dict create]
        set headers [list name property value source]
        foreach mp_line $lines {

            set dict_list [list]
            lmap mountpoint [split $mp_line \t] header $headers {lappend dict_list $header $mountpoint}

            set name [lindex $mp_line 0]
            dict set mp_dict $name $dict_list
        }

        return $mp_dict
    }

    proc dataset_exists {dataset} {
        variable mountpoints_dict

        return [dict exists $mountpoints_dict $dataset]
    }

    proc is_mounted {dataset} {
        set mounted_line [exec zfs get -H mounted $dataset]

        return [expr [lindex $mounted_line 2] eq {yes}]
    }

    proc mount {dataset} {
        exec zfs mount $dataset
    }
    
    proc snapshot_exists {snapshot_path} {
        variable snapshots_dict

        return [dict exists $snapshots_dict $snapshot_path]
    }

    proc get_mountpoint {dataset} {
        # NOTE: It's probably better to just get the mountpoint
        # attribute.
        
        variable mountpoints_dict
        set mountpoints_dict [get_mountpoints]
        if {![dict exists $mountpoints_dict $dataset value]} {

            return -code error -errorcode {ZFS MOUNTPOINT EEXIST} \
                "mountpoint does not exist for dataset: $dataset"
        }

        return [dict get $mountpoints_dict $dataset value]
    }

    proc update_mountpoints {} {
        variable mountpoints_dict

        set mountpoints_dict [get_mountpoints]
    }

    proc update_snapshots {} {
        variable snapshots_dict

        set snapshots_dict [get_snapshots]
    }
    
    proc create_dataset {new_dataset_name} {

        exec zfs create -p $new_dataset_name >&@ stderr

        update_mountpoints
        return
    }

    proc create_snapshot {image_dataset image_version} {

        exec zfs snapshot "${image_dataset}@${image_version}"

        update_snapshots
        update_mountpoints

        return
    }

    proc diff {snapshot dataset} {
        #NOTE: 'zfs diff' sucks.  It only shows a single file path per inode.  So
        # a directory will show modified but if two paths in the directory share
        # an inode.  Only one will show up.  This means we have to do tons
        # of extra processing.
        #
        # It's probably not the most efficient thing but the solution I'm taking
        # is to:
        #
        # 1. Create a dict of lists where key is inode and paths are a value list.
        # 2. Get the inode for each path returned by zfs diff
        # 3. Use the inode as the key into the inode,paths dict
        # 4. Output each path to the tar file.

        
        set diff_output [exec -keepnewline zfs diff -H $snapshot $dataset]
        
        set diff_dict [dict create]
        foreach line [split $diff_output "\n" ] {

            if {$line eq {}} {

                continue
            }
            
            set change_type [lindex $line 0]
            set path [lindex $line 1]

            dict lappend diff_dict $change_type $path
        }

        return $diff_dict
    }

    proc diff_hardlinks {diff_dict dataset_mountpoint} {
        # Creates a dict with inodes as keys and a list of
        # paths for those inodes.  This is necessary because zfs diff doesn't
        # work with hardlinks.
        #
        # diff_dict: A dictionary as returned from the zfs::diff proc
        #
        # dataset_mountpoint: is the mountpoint for the dataset used to generate
        # diff_dict
        #
        # NOTE: This operation is super expensive.

        #Create a line delimited set of inode path pairs for all inodes under
        #the dataset_mountpoint
        set inode_path_pairs [exec find $dataset_mountpoint -exec stat -f "%i%t%N" \{\} \;]

        set inode_dict [dict create]
        foreach {inode_path_pair} [split $inode_path_pairs \n] {

            foreach {inode path}  [split $inode_path_pair \t] {
                dict lappend inode_dict $inode $path
            }
        }

        set modified_file_list [list {*}[dict get $diff_dict {M}] \
                                    {*}[dict get $diff_dict {+}]]

        set output_dict [dict create]
        foreach path $modified_file_list {

            array set stat_buf {}
            file lstat $path stat_buf

            #Add the path to the output_dict with inode as key
            foreach inode_path [dict get $inode_dict $stat_buf(ino)] {
                dict lappend output_dict $stat_buf(ino) $inode_path
            }
        }

        return $output_dict
    }
    
    proc destroy {dataset} {
        # Force destroy. We assume that if we are
        # destroying a dataset, we really want to destroy it.
        exec zfs destroy -f $dataset
    }

    proc destroy_recursive {dataset} {
        exec zfs destroy -r $dataset
    }

    proc set_jailed_attr {dataset} {
        exec zfs set jailed=on $dataset
    }

    proc set_mountpoint_attr {dataset mountpoint} {
        exec zfs set mountpoint=$mountpoint $dataset
    }
    
    proc dataset_children {parent_dataset} {

        #Return a list of dicts that describe the datasets.  The first element in the
        #list is the parent_dataset
        
        set output [exec zfs list -H -r $parent_dataset]
        set dataset_dict_list [list]
        foreach dataset_line [split $output \n] {
            set dataset_dict [dict create \
                                  name [lindex $dataset_line 0] \
                                  used [lindex $dataset_line 1] \
                                  avail [lindex $dataset_line 2] \
                                  refer [lindex $dataset_line 3] \
                                  mountpoint [lindex $dataset_line 4]]

            set dataset_dict_list [lappend dataset_dict_list $dataset_dict]
        }

        return $dataset_dict_list
    }
    
    variable snapshots_dict [get_snapshots]
    variable mountpoints_dict [get_mountpoints]
}

package provide vessel::zfs 1.0.0
