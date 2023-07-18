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
import { AbstractSession, Logger, ImperativeExpect, ImperativeError, RestConstants, SessConstants } from "@zowe/imperative";
export class ZoweCli {
    public static LoginPath = "";
    public static async testLogin(profile: string, username: string, password: string) {
        return "Password: " + profile + "; username: " + username + "; password: " + password;
    }

    public static async login(session: AbstractSession) {
        Logger.getAppLogger().trace("Login");
        ImperativeExpect.toNotBeNullOrUndefined(session, "Required session must be defined");
        ImperativeExpect.toNotBeNullOrUndefined(session.ISession.user, "User name for basic login must be defined.");
        ImperativeExpect.toNotBeNullOrUndefined(session.ISession.password, "Password for basic login must be defined.");

        const client = new ZssRestClient(session);
        await client.request({
            request: "POST",
            resource: this.LoginPath,
        });

        if (client.response.statusCode !== RestConstants.HTTP_STATUS_204) {
            throw new ImperativeError((client as any).populateError({
                msg: `REST API Failure with HTTP(S) status ${client.response.statusCode}`,
                causeErrors: client.dataString,
                source: SessConstants.HTTP_PROTOCOL
            }));
        }

        // return token to the caller
        return session.ISession.tokenValue;
    }
}