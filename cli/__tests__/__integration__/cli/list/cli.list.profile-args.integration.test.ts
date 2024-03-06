/**
 * This program and the accompanying materials are made available and may be used, at your option, under either:
 * * Eclipse Public License v2.0, available at https://www.eclipse.org/legal/epl-v20.html, OR
 * * Apache License, version 2.0, available at http://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *
 * Copyright Contributors to the Zowe Project.
 */

import * as fs from "fs";
import * as path from "path";
import { ITestEnvironment, TestEnvironment, runCliScript, isStderrEmptyForProfilesCommand } from "@zowe/cli-test-utils";
import { ITestPropertiesSchema } from "../../../__src__/environment/doc/ITestPropertiesSchema";

// Test environment will be populated in the "beforeAll"
let TEST_ENVIRONMENT: ITestEnvironment<ITestPropertiesSchema>;

const configJson = "zowe.config.json";
const configUserJson = "zowe.config.user.json";

describe("zowe-cli-sample list profile-args command", () => {
    // Create the unique test environment
    beforeEach(async () => {
        TEST_ENVIRONMENT = await TestEnvironment.setUp({
            installPlugin: true,
            testName: "list_profile_args_command",
            skipProperties: true
        });
    });

    afterEach(async () => {
        await TestEnvironment.cleanUp(TEST_ENVIRONMENT);
    });

    it("should list profile args from team config profile and other sources", () => {
        fs.copyFileSync(path.join(__dirname, "__resources__", configJson), path.join(TEST_ENVIRONMENT.workingDir, configJson));
        fs.copyFileSync(path.join(__dirname, "__resources__", configUserJson), path.join(TEST_ENVIRONMENT.workingDir, configUserJson));

        const response = runCliScript(__dirname + "/__scripts__/list_profile_args_team_config.sh", TEST_ENVIRONMENT);
        expect(response.stderr.toString()).toBe("");
        expect(response.status).toBe(0);
        const output = response.stdout.toString();
        expect(output).toMatch(/host:\s+new.host.com/);
        expect(output).toMatch(/port:\s+1337/);
        expect(output).toMatch(/user:\s+user1/);
        expect(output).toMatch(/password:\s+123456/);
        expect(output).toMatch(/usingTeamConfig:\s+true/);
        expect(output).toMatch(/sampleProfileName:\s+my_sample/);
        expect(output).toMatch(/baseProfileName:\s+my_base/);
        expect(output).toMatchSnapshot();
    });

    it("should list profile args from old school profile and other sources", () => {
        const response = runCliScript(__dirname + "/__scripts__/list_profile_args_old_profiles.sh", TEST_ENVIRONMENT);
        expect(isStderrEmptyForProfilesCommand(response.stderr)).toBe(true);
        expect(response.status).toBe(0);
        const output = response.stdout.toString();
        expect(output).toMatch(/host:\s+new.host.com/);
        expect(output).toMatch(/port:\s+1337/);
        expect(output).toMatch(/user:\s+user1/);
        expect(output).toMatch(/password:\s+123456/);
        expect(output).toMatch(/usingTeamConfig:\s+false/);
        expect(output).toMatch(/sampleProfileName:\s+my_sample/);
        expect(output).toMatch(/baseProfileName:\s+my_base/);
        expect(output).toMatchSnapshot();
    });
});
