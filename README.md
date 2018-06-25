# FBSDAppContainers
Application containers for FreeBSD.

## Goals
* Provide a simple and powerful interface to the plethora of underutilized FreeBSD features, including:
    * Jails (Heirarchical)
    * pf
    * zfs
    * capsicum
    * rctl
    * mandatory access control
    * Security Level
* Deeply integrate with FreeBSD system, preferring system libraries to fork/exec.  This project has 0 concern with being portable.

## Vocabulary
* **Image**: A directory or mounted filesystem that the container will use as a root directory.
* **Image Archive**: A single file (usually a compressed image) that suitable for storing in a registry.
* **Registry**: A store for image archives (REST, local filesystem, ftp, etc)
* **Volume**: A filesystem that is mounted to a directory inside of the image.  Bind mount, memory back file mount, disk etc.
* **Container**: The combination of image, network stack, volumes and jail.

## Environment

* **APPC_IMAGE_DIR**: Directory for unpacked images
* **APPC_CONTAINER_DIR**: Directory for containers.  Images are mounted (nullfs mount for directories, mount for vnode fs's) here and
  containers are run in those images.
* **APPC_ARCHIVE_DIR**: Directory for archives to be saved (during creation or download).
* **APPC_REGISTRY_URL**: URL to the appc registry

## Example cli

**NOTE:** These commands are the direction for implementation.  They are not yet necessarily implemented.

* appc start --name=mycontainer --image=couchdb [--mount=<some nullfs directory>:<abolute path in container>] [-- command to run]
    * Run a container with the given command.  Anything falling '--' is considered part of the command.  If -- is not given, then look
      for an appc file.
* appc save --name=mycontainer
* appc publish --image=couchdb

## Features

* **Image registry**: Pull multiple formats of images that can be used as filesystems for jails.  There could be multiple registry types:
    * ftp(s)
    * ssh
    * zfs: via zsend and zrecv
    * fs: local fs registry
    * http(s)
* **Layered images**: Use unionfs to layer images, similar to docker
* **Multiple Image Formats**
    * iso
    * tarball
* **Auto Networking**: 
    * expose ports: Automatically create pf redirect rule.
    * VIMAGE when supported by kernel
    * Auto create subnets based on network names
* **Task Scheduling**:
    * Support a scheduler passing tasks to different hosts.  Similar to borg or nomad
    * Perhaps implement a nomad backend.
