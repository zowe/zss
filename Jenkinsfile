#!groovy

/**
 * This program and the accompanying materials are
 * made available under the terms of the Eclipse Public License v2.0 which accompanies
 * this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Copyright Contributors to the Zowe Project.
 */


properties([
    buildDiscarder(logRotator(numToKeepStr: "25")),
    disableConcurrentBuilds(),
    parameters([
            string(
                    name: "BUILD_SERVER_HOST",
                    description: "z/OS server host name to build",
                    defaultValue: "river.zowe.org",
                    trim: true,
                    required: true
            ),
            string(
                    name: "BUILD_SERVER_SSH_PORT",
                    description: "SSH port of build server.",
                    defaultValue: "2022",
                    trim: true,
                    required: true
            ),
            credentials(
                    name: "BUILD_SERVER_SSH_CREDENTIAL",
                    description: "The SSH credential used to connect to build server",
                    credentialType: "com.cloudbees.plugins.credentials.impl.UsernamePasswordCredentialsImpl",
                    defaultValue: "ssh-zdt-test-image-guest",
                    required: true
            ),
            string(
                    name: "JAVA_HOME",
                    description: "Java installation directory",
                    defaultValue: "/usr/lpp/java/J8.0_64",
                    trim: true,
                    required: true
            ),
            string(
                    name: "ANT_HOME",
                    description: "Ant installation directory",
                    defaultValue: "/zaas1/ant/apache-ant-1.10.5",
                    trim: true,
                    required: true
            ),
            string(
                    name: "XLC_DATASET",
                    description: "z/OS XL C/C++ installation dataset",
                    defaultValue: "CBC.SCCNCMP",
                    trim: true,
                    required: true
            ),
            string(
                    name: "ZOWE_COMMON_C_REPO",
                    description: "Zowe common C repository",
                    defaultValue: "https://github.com/zowe/zowe-common-c.git",
                    trim: true,
                    required: true
            ),
            string(
                    name: "ARTIFACTORY_SERVER",
                    description: "Artifactory server",
                    defaultValue: "gizaArtifactory",
                    trim: true,
                    required: true
            ),
            string(
                    name: "ARTIFACTORY_REPO",
                    description: "Artifactory repository to deploy ZSS",
                    defaultValue: "libs-snapshot-local/org/zowe/zss",
                    trim: true,
                    required: true
            ),
            string(
                    name: "ZOWE_MANIFEST_URL",
                    description: "Zowe template manifest URL",
                    defaultValue: "https://raw.githubusercontent.com/zowe/zowe-install-packaging/master/manifest.json.template",
                    trim: true,
                    required: true
            )
    ])
])


def getZoweVersion() {
    def response = httpRequest url: params.ZOWE_MANIFEST_URL
    echo "Zowe manifest template:\n${response.content}"
    return (response.content =~ /"version"\s*:\s*"(.*)"/)[0][1]
}


withCredentials([usernamePassword(
    credentialsId: params.BUILD_SERVER_SSH_CREDENTIAL,
    usernameVariable: "BUILD_SERVER_USERNAME",
    passwordVariable: "BUILD_SERVER_PASSWORD"
)]) {

    node("ibm-jenkins-slave-nvm") {
        try {
            currentBuild.result = "SUCCESS"

            def buildDir = "/tmp/~${env.BUILD_TAG}"
            def buildServer = [
                name         : params.BUILD_SERVER_HOST,
                host         : params.BUILD_SERVER_HOST,
                port         : params.BUILD_SERVER_SSH_PORT.toInteger(),
                user         : BUILD_SERVER_USERNAME,
                password     : BUILD_SERVER_PASSWORD,
                allowAnyHosts: true
            ]
            def zoweVersion = getZoweVersion()

            stage("Checkout") {
                sh "git clone ${params.ZOWE_COMMON_C_REPO}"
                dir("zss") {
                    checkout scm
                }
            }

            stage("Prepare") {
                def (majorVersion, minorVersion, microVersion) = zoweVersion.tokenize(".")
                sh \
                    """
                    cd zss/build
                    sed -i "s/MAJOR_VERSION=0/MAJOR_VERSION=${majorVersion}/" version.properties
                    sed -i "s/MINOR_VERSION=0/MINOR_VERSION=${minorVersion}/" version.properties
                    sed -i "s/REVISION=0/REVISION=${microVersion}/" version.properties
                    """
                sh "tar cf source.tar zowe-common-c zss"
                sshCommand remote: buildServer, command: "rm -rf ${buildDir} && mkdir -p ${buildDir}"
                sshPut remote: buildServer, from: "source.tar", into: buildDir
                sshCommand remote: buildServer, command: \
                    """
                    cd ${buildDir} &&
                    pax -r -x tar -o to=IBM-1047 -f source.tar
                    """
            }

            stage("Build") {
                sshCommand remote: buildServer, command: \
                    """
                    export JAVA_HOME=${params.JAVA_HOME} &&
                    export ANT_HOME=${params.ANT_HOME} &&
                    export STEPLIB=${params.XLC_DATASET} &&
                    cd ${buildDir}/zss/build &&
                    ${params.ANT_HOME}/bin/ant
                    """
            }

            stage("Package") {
                sshCommand remote: buildServer, command: \
                    """
                    cd ${buildDir} &&
                    mkdir LOADLIB &&
                    mkdir SAMPLIB &&
                    cp "//DEV.LOADLIB(ZWESIS01)" LOADLIB/ZWESIS01  &&
                    cp zss/samplib/zis/* SAMPLIB/  &&
                    cp zss/bin/zssServer ./  &&
                    extattr +p zssServer &&
                    pax -x os390 -w -f zss.pax SAMPLIB LOADLIB zssServer
                    """
                sshGet remote: buildServer, from: "${buildDir}/zss.pax", into: "zss.pax"
                sshCommand remote: buildServer, command:  "rm -rf ${buildDir}"
            }

            stage("Publish") {
                def target = "${params.ARTIFACTORY_REPO}/" +
                             "${zoweVersion}-${getReleaseIdentifier()}/" +
                             "zss-${zoweVersion}-${getBuildIdentifier(true, '__EXCLUDE__', true)}.pax"
                def uploadSpec = \
                    """
                    {"files":
                        [
                            {
                                "pattern": "zss.pax",
                                "target": "${target}"
                            }
                        ]
                    }
                    """
                echo "Artifactory upload spec:"
                echo uploadSpec

                def artifactoryServer = Artifactory.server params.ARTIFACTORY_SERVER
                def buildInfo = Artifactory.newBuildInfo()
                artifactoryServer.upload spec: uploadSpec, buildInfo: buildInfo
                artifactoryServer.publishBuildInfo buildInfo
            }

        } catch (e) {
            currentBuild.result = "FAILURE"
            throw e
        } finally {

            stage("Report") {
                def prettyParams = ""
                params.each{ key, value -> prettyParams += "<br/>&nbsp;&nbsp;&nbsp;${key} = '${value}'"}
                emailext \
                    subject: """${env.JOB_NAME} [${env.BUILD_NUMBER}]: ${currentBuild.result}""",
                    attachLog: true,
                    mimeType: "text/html",
                    recipientProviders: [
                        [$class: "RequesterRecipientProvider"],
                        [$class: "CulpritsRecipientProvider"],
                        [$class: "DevelopersRecipientProvider"],
                        [$class: "UpstreamComitterRecipientProvider"]
                    ],
                    body: \
                        """
                        <p><b>${currentBuild.result}</b></p>
                        <hr/>
                        <ul>
                            <li>Duration: ${currentBuild.durationString[0..-14]}</li>
                            <li>Build link: <a href="${env.RUN_DISPLAY_URL}">
                                ${env.JOB_NAME} [${env.BUILD_NUMBER}]</a></li>
                            <li>Parameters: ${prettyParams}</li>
                        </ul>
                        <hr/>
                        """
            }
        }
    }
}
