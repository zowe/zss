/**
 * This program and the accompanying materials are made available and may be used, at your option, under either:
 * * Eclipse Public License v2.0, available at https://www.eclipse.org/legal/epl-v20.html, OR
 * * Apache License, version 2.0, available at http://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *
 * Copyright Contributors to the Zowe Project.
 */

import { ICommandOptionDefinition } from "@zowe/imperative";

export class ProfileConstants {
    public static SAMPLE_CONNECTION_OPTION_GROUP = "Sample Connection Options";

    public static SAMPLE_OPTION_HOST: ICommandOptionDefinition = {
        name: "host",
        aliases: ["H"],
        description: "The sample service host name",
        type: "string",
        group: ProfileConstants.SAMPLE_CONNECTION_OPTION_GROUP
    };

    public static SAMPLE_OPTION_PORT: ICommandOptionDefinition = {
        name: "port",
        aliases: ["P"],
        description: "The sample service port",
        type: "number",
        group: ProfileConstants.SAMPLE_CONNECTION_OPTION_GROUP
    };

    public static SAMPLE_OPTION_USER: ICommandOptionDefinition = {
        name: "user",
        aliases: ["u"],
        description: "The sample service user name",
        type: "string",
        group: ProfileConstants.SAMPLE_CONNECTION_OPTION_GROUP
    };

    public static SAMPLE_OPTION_PASSWORD: ICommandOptionDefinition = {
        name: "password",
        aliases: ["p"],
        description: "The sample service password",
        type: "string",
        group: ProfileConstants.SAMPLE_CONNECTION_OPTION_GROUP
    };

    public static SAMPLE_OPTION_REJECT_UNAUTHORIZED: ICommandOptionDefinition = {
        name: "reject-unauthorized",
        aliases: ["ru"],
        description: "Reject self-signed certificates",
        type: "boolean",
        defaultValue: true,
        group: ProfileConstants.SAMPLE_CONNECTION_OPTION_GROUP
    };

    public static SAMPLE_OPTION_TSHIRT_SIZE: ICommandOptionDefinition = {
        name: "tshirt-size",
        aliases: ["ts"],
        description: "Sample option for your T-shirt size",
        type: "string",
        allowableValues: { values: ["XS", "S", "M", "L", "XL"] },
        defaultValue: "M",
        group: ProfileConstants.SAMPLE_CONNECTION_OPTION_GROUP
    };

    public static SAMPLE_CONNECTION_OPTIONS: ICommandOptionDefinition[] = [
        ProfileConstants.SAMPLE_OPTION_HOST,
        ProfileConstants.SAMPLE_OPTION_PORT,
        ProfileConstants.SAMPLE_OPTION_USER,
        ProfileConstants.SAMPLE_OPTION_PASSWORD,
        ProfileConstants.SAMPLE_OPTION_REJECT_UNAUTHORIZED,
        ProfileConstants.SAMPLE_OPTION_TSHIRT_SIZE
    ];
}
