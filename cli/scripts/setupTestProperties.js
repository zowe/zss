/**
 * This program and the accompanying materials are made available and may be used, at your option, under either:
 * * Eclipse Public License v2.0, available at https://www.eclipse.org/legal/epl-v20.html, OR
 * * Apache License, version 2.0, available at http://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *
 * Copyright Contributors to the Zowe Project.
 */

const fs = require("fs");
const jsYaml = require("js-yaml");

const properties = jsYaml.load(fs.readFileSync(__dirname + "/../__tests__/__resources__/properties/example_properties.yaml", "utf-8"));
properties.zosmf = {
    ...properties.zosmf,
    protocol: "http",
    host: "localhost",
    port: 3000
};
fs.writeFileSync(__dirname + "/../__tests__/__resources__/properties/custom_properties.yaml", jsYaml.dump(properties));
