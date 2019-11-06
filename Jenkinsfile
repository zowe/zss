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

node("ibm-jenkins-slave-nvm") {

    def lib = library("jenkins-library").org.zowe.jenkins_shared_library
    def pipeline = lib.pipelines.generic.GenericPipeline.new(this)

    pipeline.admins.add("dnikolaev", "sgrady")

    pipeline.setup(
        packageName: 'org.zowe.zss',
        extraInit: {
            def version = [
                sh(script: "cat build/version.properties | grep PRODUCT_MAJOR_VERSION | awk -F= '{print \$2};'", returnStdout: true).trim(),
                sh(script: "cat build/version.properties | grep PRODUCT_MINOR_VERSION | awk -F= '{print \$2};'", returnStdout: true).trim(),
                sh(script: "cat build/version.properties | grep PRODUCT_REVISION | awk -F= '{print \$2};'", returnStdout: true).trim(),
            ].join('.')
            pipeline.setVersion(version)
            echo "Current version is v${version}"
        }
    )

    pipeline.build(
        operation: {
            echo "Build will happen in pre-packaging"
        }
    )

    // define we need packaging stage, which processed in .pax folder
    pipeline.packaging(name: 'zss', paxOptions: '-x os390')

    // define we need publish stage
    pipeline.publish(
        allowPublishWithoutTest: true,
        artifacts: [
            '.pax/zss.pax',
        ]
    )

    pipeline.end()

}
