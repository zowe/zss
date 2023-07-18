/**
 * This program and the accompanying materials are made available and may be used, at your option, under either:
 * * Eclipse Public License v2.0, available at https://www.eclipse.org/legal/epl-v20.html, OR
 * * Apache License, version 2.0, available at http://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *
 * Copyright Contributors to the Zowe Project.
 */

import { ICommandHandler, IHandlerParameters } from "@zowe/imperative";
import { ZoweCli } from "../../../api/ZoweCli";

export default class LoginHandler implements ICommandHandler {
    public async process(params: IHandlerParameters): Promise<void> {
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

        const resp = await ZoweCli.testLogin(params.arguments.profile, params.arguments.username, params.arguments.password);
        params.response.console.log(resp);
    }
}