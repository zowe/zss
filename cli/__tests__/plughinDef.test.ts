/**
 * This program and the accompanying materials are made available and may be used, at your option, under either:
 * * Eclipse Public License v2.0, available at https://www.eclipse.org/legal/epl-v20.html, OR
 * * Apache License, version 2.0, available at http://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *
 * Copyright Contributors to the Zowe Project.
 */

describe("plugin definition", () => {

    // Will fail if imperative config object is changed. This is a sanity/protection check to ensure that any
    // changes to the configuration document are intended.
    it("pluginDef should match expected values", () => {
        const pluginDef = require("../src/pluginDef");
        expect(pluginDef.name).toBe("zowe-cli-sample");
        expect(pluginDef.pluginLifeCycle).toContain("LifeCycleForSample");
        expect(pluginDef.pluginSummary).toBe("Zowe CLI sample plug-in");
        expect(pluginDef.productDisplayName).toBe("Zowe CLI Sample Plug-in");
        expect(pluginDef.rootCommandDescription).toContain("Welcome to the sample plug-in");
    });

});
