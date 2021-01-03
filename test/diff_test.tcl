# -*- mode: tcl; indent-tabs-mode: nil; tab-width: 4; -*-

source [file join .. zfs.tcl]

package require fileutil

set pool "zpool"
if {[info exists env(VESSEL_POOL)]} {
    set pool $env(VESSEL_POOL)
}

set container "${pool}/jails/minimaltest"
set container_path "/${container}"
set diff_dict [appc::zfs::diff ${container}@0 ${container}]

file mkdir work
cd work
set whiteout_file [open {whiteouts.txt} {w}]

if {[dict exists $diff_dict {-}]} {
    foreach deleted_file [dict get $diff_dict {-}] {
        puts $whiteout_file $deleted_file
    }
}

set files [list {*}[dict get $diff_dict {M}] {*}[dict get $diff_dict {+}]]
set tar_list_file [open {files.txt} {w}]
puts $tar_list_file {-C}
puts $tar_list_file "$container_path"
foreach path $files {

    puts $tar_list_file [fileutil::stripPath "${container_path}" $path]
}
flush $tar_list_file
close $tar_list_file
exec tar -cavf layer.tgz -n -T {files.txt} >&@ stderr
