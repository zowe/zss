# ZSS Changelog

All notable changes to the ZSS package will be documented in this file.

## Recent Changes

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

