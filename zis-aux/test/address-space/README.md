This program and the accompanying materials are
made available under the terms of the Eclipse Public License v2.0 which accompanies
this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

SPDX-License-Identifier: EPL-2.0

Copyright Contributors to the Zowe Project.

# ASCRE, ASDES and ASEXT tests

* Build:
  * Go to the zis-aux/test/address-space/build directory
  * Run build.sh
  * The zis-aux/test/address-space/bin directory will contain a program called master
  * userID.DEV.LOADLIB will have load lib ASTARGET

* Target program deployment:
  * Copy the samplib/ASTARGET STC JCL to your PROCLIB
  * Change the STEPLIB DD to point to userID.DEV.LOADLIB
  * Make sure the load lib is APF-authorized

* Running the ASCRE/ASEXT (creation/parameter extraction) test:
  * Go to zis-aux/test/address-space/bin
  * Run master with parameters "start \<your-target-proclib-member-name\> \<your-parm-sting\>"

  For example, ```  > ./master start ASTARGET test-parm-string```
  * If the request was successfull your PROCLIB should start and print the passed parameter value to SYSPRINT (you may have to scroll to the right to see the full output)

* Running the ASDES (termination) test:
  * Go to zis-aux/test/address-space/bin
  * Run master with parameters "start \<your-target-proclib-name\> \<your-parm-sting\>"
  * If the request was successfull the STOKEN value should be printed to the console and your PROCLIB should be started
  * Run master with parameters "stop \<stoken-value\>"
    
  For example,```> ./master stop 0000025C00000001```
  * Your running target should terminate

* Running the termination exit test
  * Go to zis-aux/test/address-space/bin
  * Run termexit
  * In case of success you should see ```ASCRE TERMINATION STATUS = SUCCESS``` in the SYSLOG
  * If you run this test using a JCL, you will also see this message in your SYSPRINT

This program and the accompanying materials are
made available under the terms of the Eclipse Public License v2.0 which accompanies
this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

SPDX-License-Identifier: EPL-2.0

Copyright Contributors to the Zowe Project.

