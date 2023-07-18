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
const ZoweCli_1 = require("../../../api/ZoweCli");
class LoginHandler {
    process(params) {
        return __awaiter(this, void 0, void 0, function* () {
            // const sessCfg: ISession = {
            //     hostname: params.arguments.host,
            //     port: params.arguments.port,
            //     basePath: params.arguments.basePath,
            //     rejectUnauthorized: params.arguments.rejectUnauthorized,
            //     protocol: params.arguments.protocol || "https"
            // };
            // const sessCfgWithCreds = await ConnectionPropsForSessCfg.addPropsOrPrompt<ISession>(sessCfg, params.arguments,
            //     { doPrompting: true, parms: params });
            // const session = new Session(sessCfgWithCreds);
            const resp = yield ZoweCli_1.ZoweCli.testLogin(params.arguments.profile, params.arguments.username, params.arguments.password);
            params.response.console.log(resp);
        });
    }
}
exports.default = LoginHandler;
//# sourceMappingURL=Login.handler.js.map