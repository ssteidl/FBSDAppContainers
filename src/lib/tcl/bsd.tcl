# -*- mode: tcl; indent-tabs-mode: nil; tab-width: 4; -*-

namespace eval vessel::bsd {

    proc host_architecture {} {

        return [exec uname -m]
    }

    proc host_version {} {

        return [exec uname -r]
    }

    proc null_mount {source_dir dest_dir} {

        if {$source_dir eq {}} {

            return -code error -errorcode {BSD MOUNT PARAM} "source_dir parameter is empty for bind mount"
        }

        if {$dest_dir eq {}} {

            return -code error -errorcode {BSD MOUNT PARAM} "dest_dir parameter is empty for bind mount"
        }
        file mkdir $dest_dir

        exec mount -t nullfs $source_dir $dest_dir
    }

    proc umount {path} {

        if {$path ne {} && [file exists $path]} {
            exec umount $path
        }
    }
}

package provide vessel::bsd 1.0.0
