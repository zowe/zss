# ZSS Changelog

All notable changes to the ZSS package will be documented in this file.

## Recent Changes
- Bugfix:  Corrected build environment file's use of IP address to github.com (#660)

## `2.10.0`
- This action making a CHANGELOG note via special syntax from the GitHub PR commit message, like it could automatically update CHANGELOG.md with the message. First job checks if PR body has changelog note or not if it's not there then it asked them to add it and second job is to check if changelog note has been added in changelog.md file or not. (#636)
- Bugfix: Datasets with VOLSER set to an MVS Symbol would cause dataset read, write, and metadata API calls to fail for those datasets. (#603)
- Bugfix:  Preventing error code 0C9-09 caused by a block size of zero (#606)

## `2.9.0`
- Bugfix: expose the version of the ZIS dynamic linkage base plugin so it can be updated during a build
- Disable the ZIS dynamic linkage plugin as it's not needed by default

## `2.8.0`

- Feature: Added the API /datasetCopy to copy the datasets
- Enhancement: /datasetMetadata now returns prime, secnd, and space fields for showing the primary and secondary extent sizes and the unit type for them. (#582)
- Enhancement: ZSS dataset creation api now supports space values of "BYTE", "KB", and "MB" instead of just "CYL" and "TRK"
- Bugfix: change conflicting message IDs in the ZIS dynamic linkage base plugin

## `2.7.0`

- Enhancement: A new ZIS plugin, "ZISDYNAMIC" is available within the LOADLIB as ZWESISDL. This plugin allows for ZIS plugins to access utility functions of the zowe-common-c libraries without needing to statically build them into the plugin itself.
- Enhancement: New REST endpoint that maps distributed username to RACF user ID.
- Bugfix: Fixed /unixfile/metadata not working when URL encoded spaces were present in file names

## `2.5.0`

- Bugfix: In 2.3 and 2.4, 'safkeyring://' syntax stopped working, only allowing 'safkeyring:////'. Now, support for both is restored.
- Support ZIS runtime version check 
- Update the dynamic linkage stub vector to include new functions
- Add ZIS plugin development documentation and samples

## `2.4.0`

- Enhancement: ZSS /datasetContents now has a PUT API for creating datasets.
- Enhancement: ZIS dynamic linkage support

## `2.3.0`

- Enhancment: ZSS now utilizes the configuration parameters present in the zowe configuration file via the configmgr, simplifying the startup of ZSS and increasing the validation of its parameters. The file zss/defaults.yaml shows the default configuration parameters of zss, in combination with the schema of the parameters within zss/schemas, though some parameters are derived from zowe-wide parameters or from other components when they involve those other components.
- Enhancement: Improved startup time due to using the configmgr to process plugin registration, and only when the app-server is not enabled, as the app-server does the same thing.
- Bugfix: Fixed an 0C4 error within the /unixfile API in 31 bit mode. This was preventing files from being shown in the editor.
- Bugfix: 0C4 error messages from dataservices are now shown under the SEVERE log instead of the DEBUG log, so that issues can be spotted more easily.
- Bugfix: 0C4 when lht hashmap functions were called with negative key

## `2.0.0`

- Breaking change: Cookie name now has a suffix which includes the port or if in an HA instance, the HA ID.
- Enhancement: New configuration option that allows to run 64-bit ZSS
- Bugfix: Do not use "tee" when log destination is /dev/null

## `1.27.0`

- Enhancement: Get public key for JWT signature verification using APIML


## `1.26.0`

- Enhancement: Add support for continuations in the ZIS PARMLIB member

## `1.25.0`

- Enhancement: Add an endpoint for PassTicket generation
- Enhancement: Add an endpoint for user info
- Enhancement: Added method to read and set loglevel of dataservices
- Bugfix: Unixfile Copy and Rename API doesn't support distinct error status response
- Bugfix: Dataset contents API returns invalid error status for undefined length dataset update request
- Bugfix: Unixfile Copy service incorrectly processed files tagged as binary and mixed
- Bugfix: Unixfile Copy and Rename directory API doesn't support forceOverwrite query

## `1.24.0`

- Bugfix: Fix `zis-plugin-install.sh` script to properly exit on error with extended-install
- Bugfix: When builtin TLS was enabled, a small leak occurred when closing sockets.


## `1.23.0`

- Bugfix: `relativeTo` parsing may have failed depending upon path length and contents, leading to skipped plugin loading.
- Enhancement: Disable impersonation for OMVS Service.

## `1.22.0`

### New features and enhancements

- Bugfix: Dataset contents API doesn't skip empty records while reading a dataset 
- Enhancement: Plugins can push state out to the Caching Service for high availability storage via a storage API, available to dataservices as `remoteStorage`
- Enhancement: Plugins can push state out to the In-Memory Storage via a storage API, available to dataservices as `localStorage`
- Bugfix: ZSS now takes into account `relativeTo` attribute when loading plugin dlls

## `1.21.0`

- Set cookie path to root in order to avoid multiple cookies when browser tries to set path automatically

## `1.20.0`

### New features and enhancements
- Added method to read user & group server timeout info from a JSON file

## `1.17.0`

### New features and enhancements
- ZSS no longer requires NodeJS for its configure.sh script

- Bugfix: ZSS startup would issue warnings about failure to write yml files for APIML in the case APIML was not also being used

## `1.12.0`

### New features and enhancements
- Added scripts to allow ZSS to be its own component in the zowe install packaging

