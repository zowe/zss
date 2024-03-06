# Changelog

All notable changes to the Sample Plug-in for Zowe CLI will be documented in this file.

## `3.0.0`

- Enhancement: Added plugin lifecycle example.
- Enhancement: Removed deprecated pluginHealthCheck.

## `3.0.0`

- Major: Updated for V2 compatibility. See the prerelease items below for more details.

## `3.0.0-next.202203241806`

- Fixed core and zosmf SDKs not included as plug-in dependencies. [#59](https://github.com/zowe/zowe-cli-sample-plugin/issues/59)

## `3.0.0-next.202104201456`

- Publish `@next` tag that is compatible with team config profiles.
- Added "sample" profile type and `list profile-args` command as example of loading arguments from profiles.
- Replaced `TestEnvironment` class with @zowe/cli-test-utils package.
- Added a local server that mocks the z/OSMF info endpoint for system tests.

## `2.0.4`

- BugFix: Update License headers. [#44](https://github.com/zowe/zowe-cli-sample-plugin/issues/44)

## `2.0.3`

- Update dependencies to conform with other plugins

## `2.0.2`

- Fix broken links
- Cut lts-incremental
- Update documentation
- Move CLI and Imperative into dev dependencies
- Update test snapshots
- Move to Zowe artifactory
- Update dependencies and check for circular dependencies

## `2.0.1`

- Update remaining instances of @brightside to @zowe
- Fix links
- Update snapshots

## `2.0.0`

- Change @brightside to @zowe