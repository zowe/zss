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
const ZoweCli_1 = require("../../../api/ZoweCli");
class LoginHandler {
    process(params) {
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
            const resp = yield ZoweCli_1.ZoweCli.login(session);
            params.response.console.log(resp);
            // // Build an output object for command response
            // const usingTeamConfig = ImperativeConfig.instance.config?.exists || false;
            // const output: any = {
            //     arguments: {
            //         // Load connection info from session object
            //         host: session.ISession.hostname,
            //         port: session.ISession.port,
            //         user: session.ISession.user,
            //         password: session.ISession.password,
            //         rejectUnauthorized: session.ISession.rejectUnauthorized,
            //         // Example of how to load other profile properties
            //         tshirtSize: params.arguments.tshirtSize
            //     },
            //     environment: {
            //         usingTeamConfig
            //     }
            // };
            // // Show names of base and sample profiles if they exist
            // if (usingTeamConfig) {
            //     output.environment.sampleProfileName = ImperativeConfig.instance.config.properties.defaults.sample;
            //     output.environment.baseProfileName = ImperativeConfig.instance.config.properties.defaults.base;
            // } else {
            //     output.environment.sampleProfileName = params.profiles.getMeta("sample", false)?.name;
            //     output.environment.baseProfileName = params.profiles.getMeta("base", false)?.name;
            // }
            // // Set output for --rfj response and print it to console
            // params.response.data.setObj(output);
            // params.response.format.output({
            //     output,
            //     format: "object"
            // });
        });
    }
}
exports.default = LoginHandler;
//# sourceMappingURL=Login.handler.js.map