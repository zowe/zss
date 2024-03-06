/**
 * This program and the accompanying materials are made available and may be used, at your option, under either:
 * * Eclipse Public License v2.0, available at https://www.eclipse.org/legal/epl-v20.html, OR
 * * Apache License, version 2.0, available at http://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *
 * Copyright Contributors to the Zowe Project.
 */

import { ITestEnvironment, TestEnvironment, runCliScript } from "@zowe/cli-test-utils";
import { ITestPropertiesSchema } from "../../../__src__/environment/doc/ITestPropertiesSchema";

// Test environment will be populated in the "beforeAll"
let TEST_ENVIRONMENT: ITestEnvironment<ITestPropertiesSchema>;

describe("zowe-cli-sample fail error-handler command", () => {

    // Create the unique test environment
    beforeAll(async () => {
        TEST_ENVIRONMENT = await TestEnvironment.setUp({
            installPlugin: true,
            testName: "fail_error_handler_command",
            skipProperties: true
        });
    });
    afterAll(async () => {
        await TestEnvironment.cleanUp(TEST_ENVIRONMENT);
    });
    it("should fail the handler", () => {
        const response = runCliScript(__dirname + "/__scripts__/fail_error_handler.sh", TEST_ENVIRONMENT);
        expect(response.stderr.toString()).toMatchSnapshot();
        expect(response.status).toBe(1);
        expect(response.stdout.toString()).toMatchSnapshot();
    });
});
