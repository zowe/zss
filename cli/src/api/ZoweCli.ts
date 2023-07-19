/**
 * This program and the accompanying materials are made available and may be used, at your option, under either:
 * * Eclipse Public License v2.0, available at https://www.eclipse.org/legal/epl-v20.html, OR
 * * Apache License, version 2.0, available at http://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *
 * Copyright Contributors to the Zowe Project.
 */

import { ZssRestClient } from "./ZssRestClient";
import { AbstractSession, Logger, ImperativeExpect, Headers, RestClientError, ImperativeError } from "@zowe/imperative";
export class ZoweCli {
    static LoginPath = "/login";

    public static async login(session: AbstractSession) {
        Logger.getAppLogger().info("ZoweCli.login()");
        ImperativeExpect.toNotBeNullOrUndefined(session, "Required session must be defined");
        session.ISession.protocol = "http";
        const credentials = {"username": session.ISession.user, "password": session.ISession.password};
        let resp;
        try {
            resp = await ZssRestClient.postExpectString(
                session,
                this.LoginPath,
                [Headers.APPLICATION_JSON],
                credentials,
            );
        } catch (err) {
            if (err instanceof RestClientError && err.mDetails && err.mDetails.httpStatus) {
                throw new ImperativeError({
                    msg: `HTTP(S) error status ${err.mDetails.httpStatus} received`,
                    additionalDetails: err.mDetails.additionalDetails,
                    causeErrors: err
                });
            } else {
                throw err;
            }
        }
        Logger.getAppLogger().info("Session", JSON.stringify(session));
        return resp;
    }
}