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

console.log(`Started plugins-init.js, platform=${os.platform}`);

const runtimeDirectory=std.getenv('ZWE_zowe_runtimeDirectory');
const extensionDirectory=std.getenv('ZWE_zowe_extensionDirectory');
const workspaceDirectory=std.getenv('ZWE_zowe_workspaceDirectory');


// This can be used to load useful js libraries present within zowe
// But it's not recommended for extenders because this is internal, undocumented code
// Which could have a change in behavior between versions of zowe
// It is being used here because app-server is shipped within zowe, so behavior is kept in sync
let fs, componentlib;
async function importModule() {
  try {
    fs = await import(`${runtimeDirectory}/bin/libs/fs`);
    componentlib = await import(`${runtimeDirectory}/bin/libs/component`);
  } catch (error) {
    console.error('import failed');
    std.exit(1);
  }
}

importModule().then(()=> {
  const installedComponentsEnv=std.getenv('ZWE_INSTALLED_COMPONENTS');
  const installedComponents = installedComponentsEnv ? installedComponentsEnv.split(',') : null;

  const enabledComponentsEnv=std.getenv('ZWE_ENABLED_COMPONENTS');
  const enabledComponents = enabledComponentsEnv ? enabledComponentsEnv.split(',') : null;

  const pluginPointerDirectory = `${workspaceDirectory}/app-server/plugins`;

  function deleteFile(path) {
    return os.remove(path);
  }

  function registerPlugin(path, pluginDefinition) {
    const filePath = `${pluginPointerDirectory}/${pluginDefinition.identifier}.json`;
    if (fs.fileExists(filePath)) {
      return true;
    } else {
      let location, relativeTo;
      const index = path.indexOf(runtimeDirectory);
      if (index != -1) {
        relativeTo = "$ZWE_zowe_runtimeDirectory";
        location = filePath.substring(index);


        return xplatform.storeFileUTF8(filePath, xplatform.AUTO_DETECT, JSON.stringify({
          "identifier": pluginDefinition.identifier,
          "pluginLocation": location,
          "relativeTo": relativeTo
        }, null, 2));
      } else {
        return xplatform.storeFileUTF8(filePath, xplatform.AUTO_DETECT, JSON.stringify({
          "identifier": pluginDefinition.identifier,
          "pluginLocation": filePath
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
    const rc = os.mkdir(pluginPointerDirectory, 0o770);
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
});
