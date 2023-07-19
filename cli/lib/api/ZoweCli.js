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
    static login(session) {
        return __awaiter(this, void 0, void 0, function* () {
            imperative_1.Logger.getAppLogger().info("ZoweCli.login()");
            imperative_1.ImperativeExpect.toNotBeNullOrUndefined(session, "Required session must be defined");
            session.ISession.protocol = "http";
            const credentials = { "username": session.ISession.user, "password": session.ISession.password };
            let resp;
            try {
                resp = yield ZssRestClient_1.ZssRestClient.postExpectString(session, this.LoginPath, [imperative_1.Headers.APPLICATION_JSON], credentials);
            }
            catch (err) {
                if (err instanceof imperative_1.RestClientError && err.mDetails && err.mDetails.httpStatus) {
                    throw new imperative_1.ImperativeError({
                        msg: `HTTP(S) error status ${err.mDetails.httpStatus} received`,
                        additionalDetails: err.mDetails.additionalDetails,
                        causeErrors: err
                    });
                }
                else {
                    throw err;
                }
            }
            imperative_1.Logger.getAppLogger().info("Test");
            imperative_1.Logger.getAppLogger().info("Session", session);
            return resp;
        });
    }
}
exports.ZoweCli = ZoweCli;
ZoweCli.LoginPath = "/login";
//# sourceMappingURL=ZoweCli.js.map