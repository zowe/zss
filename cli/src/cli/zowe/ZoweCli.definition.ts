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
import { LoginDefinition } from "./login/Login.definition";
import { ProfileArgsDefinition } from "./profile-args/ProfileArgs.definition";
const ZoweCliDefinition: ICommandDefinition = {
    name: "zss",
    summary: "Zowe CLI ZSS plugin",
    description: "n/a",
    type: "group",
    children: [LoginDefinition, ProfileArgsDefinition]
};

export = ZoweCliDefinition;