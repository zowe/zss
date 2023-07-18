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
import { AbstractSession, Logger, ImperativeExpect, Headers } from "@zowe/imperative";
export class ZoweCli {
    static LoginPath = "/login";

    public static async login(session: AbstractSession) {
        Logger.getAppLogger().info("ZoweCli.login()");
        ImperativeExpect.toNotBeNullOrUndefined(session, "Required session must be defined");
        session.ISession.protocol = "http";
        // const client = new ZssRestClient(session);
        const resp = await ZssRestClient.postExpectString(
            session,
            this.LoginPath,
            [Headers.APPLICATION_JSON],
            JSON.stringify({"username": session.ISession.user, "password": session.ISession.password}),
        );
        Logger.getAppLogger().info(resp);
        Logger.getAppLogger().info("Session", session);
        return "";
        // if (client.response.statusCode !== RestConstants.HTTP_STATUS_204) {
        //     throw new ImperativeError((client as any).populateError({
        //         msg: `REST API Failure with HTTP(S) status ${client.response.statusCode}`,
        //         causeErrors: client.dataString,
        //         source: SessConstants.HTTP_PROTOCOL
        //     }));
        // }

        // // return token to the caller
        // return session.ISession.tokenValue;
    }
}