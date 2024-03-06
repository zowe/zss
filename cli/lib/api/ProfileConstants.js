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
Object.defineProperty(exports, "__esModule", { value: true });
exports.ProfileConstants = void 0;
class ProfileConstants {
}
exports.ProfileConstants = ProfileConstants;
ProfileConstants.SAMPLE_CONNECTION_OPTION_GROUP = "Sample Connection Options";
ProfileConstants.SAMPLE_OPTION_HOST = {
    name: "host",
    aliases: ["H"],
    description: "The sample service host name",
    type: "string",
    group: ProfileConstants.SAMPLE_CONNECTION_OPTION_GROUP
};
ProfileConstants.SAMPLE_OPTION_PORT = {
    name: "port",
    aliases: ["P"],
    description: "The sample service port",
    type: "number",
    group: ProfileConstants.SAMPLE_CONNECTION_OPTION_GROUP
};
ProfileConstants.SAMPLE_OPTION_USER = {
    name: "user",
    aliases: ["u"],
    description: "The sample service user name",
    type: "string",
    group: ProfileConstants.SAMPLE_CONNECTION_OPTION_GROUP
};
ProfileConstants.SAMPLE_OPTION_PASSWORD = {
    name: "password",
    aliases: ["p"],
    description: "The sample service password",
    type: "string",
    group: ProfileConstants.SAMPLE_CONNECTION_OPTION_GROUP
};
ProfileConstants.SAMPLE_OPTION_REJECT_UNAUTHORIZED = {
    name: "reject-unauthorized",
    aliases: ["ru"],
    description: "Reject self-signed certificates",
    type: "boolean",
    defaultValue: true,
    group: ProfileConstants.SAMPLE_CONNECTION_OPTION_GROUP
};
ProfileConstants.SAMPLE_OPTION_TSHIRT_SIZE = {
    name: "tshirt-size",
    aliases: ["ts"],
    description: "Sample option for your T-shirt size",
    type: "string",
    allowableValues: { values: ["XS", "S", "M", "L", "XL"] },
    defaultValue: "M",
    group: ProfileConstants.SAMPLE_CONNECTION_OPTION_GROUP
};
ProfileConstants.SAMPLE_CONNECTION_OPTIONS = [
    ProfileConstants.SAMPLE_OPTION_HOST,
    ProfileConstants.SAMPLE_OPTION_PORT,
    ProfileConstants.SAMPLE_OPTION_USER,
    ProfileConstants.SAMPLE_OPTION_PASSWORD,
    ProfileConstants.SAMPLE_OPTION_REJECT_UNAUTHORIZED,
    ProfileConstants.SAMPLE_OPTION_TSHIRT_SIZE
];
//# sourceMappingURL=ProfileConstants.js.map