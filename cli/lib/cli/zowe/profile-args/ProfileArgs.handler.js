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
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", { value: true });
const imperative_1 = require("@zowe/imperative");
class ProfileArgsHandler {
    process(params) {
        var _a, _b, _c;
        return __awaiter(this, void 0, void 0, function* () {
            // Create session config with connection info to access service
            const sessCfg = {
                hostname: params.arguments.host,
                port: params.arguments.port,
                basePath: params.arguments.basePath,
                rejectUnauthorized: params.arguments.rejectUnauthorized,
                protocol: params.arguments.protocol || "https"
            };
            const sessCfgWithCreds = yield imperative_1.ConnectionPropsForSessCfg.addPropsOrPrompt(sessCfg, params.arguments, { doPrompting: true, parms: params });
            const session = new imperative_1.Session(sessCfgWithCreds);
            // Build an output object for command response
            const usingTeamConfig = ((_a = imperative_1.ImperativeConfig.instance.config) === null || _a === void 0 ? void 0 : _a.exists) || false;
            const output = {
                arguments: {
                    // Load connection info from session object
                    host: session.ISession.hostname,
                    port: session.ISession.port,
                    user: session.ISession.user,
                    password: session.ISession.password,
                    rejectUnauthorized: session.ISession.rejectUnauthorized,
                    // Example of how to load other profile properties
                    tshirtSize: params.arguments.tshirtSize
                },
                environment: {
                    usingTeamConfig
                }
            };
            // Show names of base and sample profiles if they exist
            if (usingTeamConfig) {
                output.environment.sampleProfileName = imperative_1.ImperativeConfig.instance.config.properties.defaults.sample;
                output.environment.baseProfileName = imperative_1.ImperativeConfig.instance.config.properties.defaults.base;
            }
            else {
                output.environment.sampleProfileName = (_b = params.profiles.getMeta("sample", false)) === null || _b === void 0 ? void 0 : _b.name;
                output.environment.baseProfileName = (_c = params.profiles.getMeta("base", false)) === null || _c === void 0 ? void 0 : _c.name;
            }
            // Set output for --rfj response and print it to console
            params.response.data.setObj(output);
            params.response.format.output({
                output,
                format: "object"
            });
        });
    }
}
exports.default = ProfileArgsHandler;
//# sourceMappingURL=ProfileArgs.handler.js.map