"use strict";
/**
 * This program and the accompanying materials are made available and may be used, at your option, under either:
 * * Eclipse Public License v2.0, available at https://www.eclipse.org/legal/epl-v20.html, OR
 * * Apache License, version 2.0, available at http://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *
 * Copyright Contributors to the Zowe Project.
 */
const Login_definition_1 = require("./login/Login.definition");
const ZoweCliDefinition = {
    name: "zss",
    summary: "Zowe CLI ZSS plugin",
    description: "n/a",
    type: "group",
    children: [Login_definition_1.LoginDefinition]
};
module.exports = ZoweCliDefinition;
//# sourceMappingURL=ZoweCli.definition.js.map