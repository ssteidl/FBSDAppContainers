package require debug
package require vessel::env
package require vessel::native
package require defer
package require fileutil
package require uri
package require TclOO

namespace eval vessel::repo {

    debug define repo
    debug on repo 1 stderr
    
    oo::class create repo {

	variable _full_url
	variable _scheme
	variable _path
	
	constructor {url} {
	    set _full_url ${url}
	    
	    #Parse it to ensure it is valid
	    set url_dict [vessel::url::parse ${url}]

	    set _scheme [dict get $url_dict scheme]
	   
	    set _path [dict get $url_dict path]
	}
	
	method pull_image {image tag downloaddir} {
	    return -code error errorcode {INTERFACECALL} \
		"Subclass of repo must implement pull_image"
	}

	method put_image {image_path} {
	    return -code error errorcode {INTERFACECALL} \
		"Subclass of repo must implement put_image"
	}

	method reconfigure {} {
	    return -code error errorcode {INTERFACECALL} \
		"Subclass of repo must implement reconfigure"
	}

	method delete_image {image tag} {
	    return -code error errorcode {INTERFACECALL} \
		"Subclass of repo must implement delete"
	}

	method image_exists {image tag} {
	    return -code error errorcode {INTERFACECALL} \
		"Subclass of repo must implement image_exists"
	}
	
	method get_url {} {

	    return ${_full_url}
	}

	method get_scheme {} {

	    return $_scheme
	}

	method get_path {} {
	    return $_path
	}

	method get_status_channel {} {
	    #subclasses can override if they need to change
	    return stderr
	}
    }

    oo::class create file_repo {
	superclass repo

	constructor {url} {
	    next $url

	    if {[my get_scheme] ne {file}} {
		return -code error -errorcode {REPO SCHEME ENOTSUPPORTED} \
		    "Scheme: $scheme is not supported"
	    }

	    set path [my get_path]
	    if {![file exists $path] || ![file isdirectory $path]} {
		return -code error -errorcode {REPO PATH EEXISTS} \
		    "Path either doesn't exist or is not a directory: $path"
	    }
	}

	method pull_image {image tag downloaddir} {

	    set image_path [file join [my get_path] "${image}:${tag}.zip"]
	    if {![my image_exists $image $tag]} {
		return -code error -errorcode {REPO PULL ENOIMAGE} \
		    "Image path does not exist: $image_path"
	    }

	    if {![file exists $downloaddir]} {
		file mkdir $downloaddir
	    }

	    debug.repo "Copying image: ${image_path} -> ${downloaddir}"
	    try {
		file copy $image_path $downloaddir
	    } trap {POSIX EEXIST} {1 2} {
		debug.repo "Image already exists.  Continuing"
	    }
	}

	method put_image {image_path} {
	    set output_path [my get_path]

	    #TODO: Parse out image name from image path
	    set repo_url [vessel::env::get_repo_url]

	    set image_path_components [file split $image_path]
	    set image_zip_name [lrange $image_path_components end end]
	    puts "$image_path_components,$image_zip_name"
	    set extension [file extension $image_zip_name]
	    if {$extension ne ".zip"} {
		return -code error -errorcode {REPO PUT EFORMAT} \
		    "Unexpected image extension: $extension"
	    }
	    
	    exec curl file://localhost/$image_path -o \
		[file join $output_path $image_zip_name] \
		>&@ [my get_status_channel]
	}

	method image_exists {image tag} {
	    
	    set image_path [file join [my get_path] "${image}:${tag}.zip"]
	    return [file exists $image_path]
	}
	
	destructor {
	    debug.repo "destroying repo"
	}
    }

    oo::class create s3repo {
	superclass repo
	
	constructor {url} {
	    next $url

	    if {[my get_scheme] ne {s3}} {
		return -code error -errorcode {REPO SCHEME ENOTSUPPORTED} \
		    "Scheme: $scheme is not supported by s3repo"
	    }
	}

	method _get_s3_image_url {image tag} {
	    #TODO: this is everywhere
	    set image_name "${image}:${tag}.zip"

	    set image_url [join [list [my get_url] $image_name] /]	    
	}
	
	method pull_image {image tag downloaddir} {

	    set image_url [my _get_s3_image_url $image $tag]
	    puts stderr "$image_url,$downloaddir"
	    exec s3cmd get $image_url $downloaddir >&@ stderr
	}

	method put_image {image_path} {
	    exec s3cmd put $image_path [my get_url] >&@stderr
	}

	method reconfigure {} {
	    return -code error errorcode {INTERFACECALL} \
		"Subclass of repo must implement reconfigure"
	}

	method delete_image {image tag} {
	    return -code error errorcode {INTERFACECALL} \
		"Subclass of repo must implement delete"
	}

	method image_exists {image tag} {
	    set image_url [my _get_s3_image_url $image $tag]
	    set ls_data [exec s3cmd ls $image_url]
	    return [expr [string length $ls_data] > 0]
	}
    }
    
    proc repo_factory {repo_url} {
	set repo_url_dict [vessel::url::parse $repo_url]

	set scheme [dict get $repo_url_dict scheme]
	set path [dict get $repo_url_dict path]

	set repo {}
	switch -exact  $scheme {

	    file {
		
		return [vessel::repo::file_repo new $repo_url]
	    }
	    s3 {
		return [vessel::repo::s3repo new $repo_url]
	    }
	    default {
		return -code error -errorcode {PUBLISH SCHEME ENOTSUPPORTED } \
		    "Scheme not supported for publishing: $scheme"
	    }
	}

    }
}

package provide vessel::repo 1.0.0
