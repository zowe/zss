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


BUILD_SERVER_TMP_DIR = "/ZOWE/tmp"
JAVA_HOME = "/usr/lpp/java/J8.0_64"
ANT_HOME = "/ZOWE/apache-ant-1.10.5"
XLC_DATASET = "CBC.SCCNCMP"


node("ibm-jenkins-slave-nvm") {

    def lib = library("jenkins-library").org.zowe.jenkins_shared_library
    def pipeline = lib.pipelines.generic.GenericPipeline.new(this)

    def buildDir = "${BUILD_SERVER_TMP_DIR}/~${env.BUILD_TAG}"
    def buildServer = null

    withCredentials([usernamePassword(
        credentialsId: lib.Constants.DEFAULT_PAX_PACKAGING_SSH_CREDENTIAL,
        usernameVariable: "BUILD_SERVER_USERNAME",
        passwordVariable: "BUILD_SERVER_PASSWORD"
    )]) {
        buildServer = [
            name         : lib.Constants.DEFAULT_PAX_PACKAGING_SSH_HOST,
            host         : lib.Constants.DEFAULT_PAX_PACKAGING_SSH_HOST,
            port         : lib.Constants.DEFAULT_PAX_PACKAGING_SSH_PORT.toInteger(),
            user         : BUILD_SERVER_USERNAME,
            password     : BUILD_SERVER_PASSWORD,
            allowAnyHosts: true
        ]
    }

    pipeline.admins.add("dnikolaev", "sgrady")

    pipeline.setup()

    pipeline.createStage(
        name: 'Deploy source',
        stage: {
            sh "tar --exclude='./source.tar' -cf source.tar *"
            sshCommand remote: buildServer, command: "rm -rf ${buildDir} && mkdir -p \$_"
            sshPut remote: buildServer, from: "source.tar", into: buildDir
            sshCommand remote: buildServer, command: \
                """
                cd ${buildDir} &&
                pax -r -x tar -o to=IBM-1047 -f source.tar
                """
        }
    )

    pipeline.build(
        operation: {
            sshCommand remote: buildServer, command: \
                """
                export JAVA_HOME=${JAVA_HOME} &&
                export ANT_HOME=${ANT_HOME} &&
                export STEPLIB=${XLC_DATASET} &&
                cd ${buildDir}/build &&
                ${ANT_HOME}/bin/ant
                """
        }
    )

    pipeline.packaging(
        name: 'zss',
        operation: {
            sshCommand remote: buildServer, command: \
                """
                cd ${buildDir} &&
                mkdir LOADLIB SAMPLIB &&
                cp -X "//DEV.LOADLIB(ZWESIS01)" LOADLIB/ZWESIS01  &&
                cp -X "//DEV.LOADLIB(ZWESAUX)" LOADLIB/ZWESAUX  &&
                cp samplib/zis/* SAMPLIB/  &&
                cp bin/zssServer ./  &&
                extattr +p zssServer &&
                pax -x os390 -w -f zss.pax SAMPLIB LOADLIB zssServer
                """
            sshGet remote: buildServer, from: "${buildDir}/zss.pax", into: "zss.pax"
            sshCommand remote: buildServer, command:  "rm -rf ${buildDir}"
        }
    )

    pipeline.end()

}
