/**
 * This program and the accompanying materials are made available and may be used, at your option, under either:
 * * Eclipse Public License v2.0, available at https://www.eclipse.org/legal/epl-v20.html, OR
 * * Apache License, version 2.0, available at http://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *
 * Copyright Contributors to the Zowe Project.
 */

import { ICommandDefinition } from "@zowe/imperative";

export const LoginDefinition: ICommandDefinition = {
    name: "login",
    aliases: ["login"],
    summary: "login",
    description: "n/a",
    type: "command",
    handler: __dirname + "/Login.handler",
    positionals: [
        {
            name: "username",
            description: "Username",
            type: "string"
        },
        {
            name: "password",
            description: "Password",
            type: "string"
        }
    ]
};