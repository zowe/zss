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
exports.ZoweCli = void 0;
const ZssRestClient_1 = require("./ZssRestClient");
const imperative_1 = require("@zowe/imperative");
class ZoweCli {
    static testLogin(profile, username, password) {
        return __awaiter(this, void 0, void 0, function* () {
            return "Password: " + profile + "; username: " + username + "; password: " + password;
        });
    }
    static login(session) {
        return __awaiter(this, void 0, void 0, function* () {
            imperative_1.Logger.getAppLogger().trace("Login");
            imperative_1.ImperativeExpect.toNotBeNullOrUndefined(session, "Required session must be defined");
            imperative_1.ImperativeExpect.toNotBeNullOrUndefined(session.ISession.user, "User name for basic login must be defined.");
            imperative_1.ImperativeExpect.toNotBeNullOrUndefined(session.ISession.password, "Password for basic login must be defined.");
            const client = new ZssRestClient_1.ZssRestClient(session);
            yield client.request({
                request: "POST",
                resource: this.LoginPath,
            });
            if (client.response.statusCode !== imperative_1.RestConstants.HTTP_STATUS_204) {
                throw new imperative_1.ImperativeError(client.populateError({
                    msg: `REST API Failure with HTTP(S) status ${client.response.statusCode}`,
                    causeErrors: client.dataString,
                    source: imperative_1.SessConstants.HTTP_PROTOCOL
                }));
            }
            // return token to the caller
            return session.ISession.tokenValue;
        });
    }
}
exports.ZoweCli = ZoweCli;
ZoweCli.LoginPath = "";
//# sourceMappingURL=ZoweCli.js.map