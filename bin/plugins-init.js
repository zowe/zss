/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

import * as os from 'os';
import * as zos from 'zos';
import * as std from 'std';
import * as xplatform from 'xplatform';
import * as fs from '../../../bin/libs/fs';
import * as componentlib from '../../../bin/libs/component';

console.log(`Started plugins-init.js, platform=${os.platform}`);

const runtimeDirectory=std.getenv('ZWE_zowe_runtimeDirectory');
const extensionDirectory=std.getenv('ZWE_zowe_extensionDirectory');
const workspaceDirectory=std.getenv('ZWE_zowe_workspaceDirectory');

const installedComponentsEnv=std.getenv('ZWE_INSTALLED_COMPONENTS');
const installedComponents = installedComponentsEnv ? installedComponentsEnv.split(',') : null;

const enabledComponentsEnv=std.getenv('ZWE_ENABLED_COMPONENTS');
const enabledComponents = enabledComponentsEnv ? enabledComponentsEnv.split(',') : null;

const pluginPointerDirectory = `${workspaceDirectory}/app-server/plugins`;

function deleteFile(path) {
  return os.remove(path);
}

function registerPlugin(pluginPath, pluginDefinition) {
  const pointerPath = `${pluginPointerDirectory}/${pluginDefinition.identifier}.json`;
  if (fs.fileExists(pointerPath)) {
    return true;
  } else {
    let location, relativeTo;
    if (pluginPath.startsWith(runtimeDirectory)) {
      relativeTo = "$ZWE_zowe_runtimeDirectory";
      location = pluginPath.substring(runtimeDirectory.length);
      if (location.startsWith('/')) {
        location = location.substring(1);
      }

      return xplatform.storeFileUTF8(pointerPath, xplatform.AUTO_DETECT, JSON.stringify({
        "identifier": pluginDefinition.identifier,
        "pluginLocation": location,
        "relativeTo": relativeTo
      }, null, 2));
    } else {
      return xplatform.storeFileUTF8(pointerPath, xplatform.AUTO_DETECT, JSON.stringify({
        "identifier": pluginDefinition.identifier,
        "pluginLocation": pluginPath
      }, null, 2));
    }
  }
}

function deregisterPlugin(pluginDefinition) {
  const filePath = `${pluginPointerDirectory}/${pluginDefinition.identifier}.json`;
  if (fs.fileExists(filePath, true)) {
    const rc = deleteFile(filePath);
    if (rc !== 0) {
      console.log(`Could not deregister plugin ${pluginDefinition.identifier}, delete ${filePath} failed, error=${rc}`);
    }
    return rc !== 0;
  } else {
    return true;
  }
}

if (!fs.directoryExists(pluginPointerDirectory, true)) {
  const rc = fs.mkdirp(pluginPointerDirectory);
  if (rc < 0) {
    console.log(`Could not create pluginsDir=${pluginPointerDirectory}, err=${rc}`);
    std.exit(2);
  }
}

console.log("Start iteration");

//A port of https://github.com/zowe/zlux-app-server/blob/v2.x/staging/bin/init/plugins-init.sh

installedComponents.forEach(function(installedComponent) {
  const componentDirectory = componentlib.findComponentDirectory(installedComponent);
  if (componentDirectory) {
    const enabled = enabledComponents.includes(installedComponent);
    console.log(`Checking plugins for component=${installedComponent}, enabled=${enabled}`);

    const manifest = componentlib.getManifest(componentDirectory);
    if (manifest.appfwPlugins) {
      manifest.appfwPlugins.forEach(function (manifestPluginRef) {
        const path = manifestPluginRef.path;
        const fullPath = `${componentDirectory}/${path}`
        const pluginDefinition = componentlib.getPluginDefinition(fullPath);
        if (pluginDefinition) {
          if (enabled) {
            console.log(`Registering plugin ${fullPath}`);
            registerPlugin(fullPath, pluginDefinition);
          } else {
            console.log(`Deregistering plugin ${fullPath}`);
            deregisterPlugin(pluginDefinition);
          }
        } else {
          console.log(`Skipping plugin at ${fullPath} due to pluginDefinition missing or invalid`);
        }
      });
    }
  } else {
    console.log(`Warning: Could not remove app framework plugins for extension ${installedComponent} because its directory could not be found within ${extensionDirectory}`);
  }
});

