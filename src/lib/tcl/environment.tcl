# -*- mode: tcl; indent-tabs-mode: nil; tab-width: 4; -*-
package require uri

namespace eval vessel::env {

    proc get_from_env {key {_default {}}} {

        global env
        set value $_default
        if {[info exists env($key)]} {

            set value $env($key)
        }

        return $value
    }
    
    proc get_pool {} {

        return [get_from_env VESSEL_POOL "zroot"]
    }
    
    proc get_dataset {} {
        #Get the zfs path including pool
        
        set pool [get_pool]
        set dataset [get_from_env VESSEL_DATASET "jails"]

        return "${pool}/${dataset}"
    }
    
    proc get_workdir {} {
        return [get_from_env VESSEL_WORKDIR \
                    [file join [get_from_env HOME [pwd]] .vessel-workdir]]
    }
    
    proc get_repo_url {} {

        #Defaults to a directory in the workdir
        return [get_from_env VESSEL_REPO_URL [uri::join path [get_workdir]/local_repo scheme file]]
    }
    
    proc get_dataset_from_image_name {image_name {tag {}}} {

        set dataset [get_dataset]
        set container_path "${dataset}/${image_name}"
        if {$tag ne {}} {
            set container_path "${container_path}:${tag}"
        }
        return $container_path
    }

    proc copy_resolv_conf {mountpoint} {
        # copy resolv.conf
        set resolv_file {/etc/resolv.conf}
        file copy -force $resolv_file [fileutil::jail $mountpoint $resolv_file]
    }

    proc remove_resolv_conf {mountpoint} {
        set resolve_file {/etc/resolv.conf}
        #TODO: Commented this out because it was getting deleted multiple times
        #due to multiple run commands.  Need to fix the vessel_file_command
        #file delete $resolve_file [fileutil::jail $mountpoint $resolve_file]
    }

    proc s3cmd_config_file {} {

        return [get_from_env VESSEL_S3CMD_CONFIG [file normalize ~/.s3cfg]]
    }

    proc image_download_dir {} {
        set workdir [get_workdir]
        return [get_from_env VESSEL_DOWNLOAD_DIR [file join $workdir {downloaded_images}]]
    }

    proc metadata_db_dir {} {

        set workdir [get_workdir]
        return [get_from_env VESSEL_METADATA_DB_DIR [file join $workdir {db}]]
    }
}
package provide vessel::env 1.0.0
