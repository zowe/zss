/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
 */

package org.zowe.zis;

import java.io.IOException;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.FileInputStream;
import java.io.PrintStream;
import java.util.HashSet;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 The purpose of the stubs is to be able to build ZIS plugins that do NOT
 statically link zowe-common-c and ZIS base code into the plugin.
 */
public class ZISStubGenerator {

    private final String hFileName;

    private enum DispatchMode {R12, ZVTE}

    public ZISStubGenerator(String hFileName) {
        this.hFileName = hFileName;
    }

    private static final String[] hlasmProlog =
            {
                    "         TITLE 'ZISSTUBS'",
                    "         ACONTROL AFPR",
                    "ZISSTUBS CSECT",
                    "ZISSTUBS AMODE 64",
                    "ZISSTUBS RMODE ANY",
                    "         SYSSTATE ARCHLVL=2,AMODE64=YES",
                    "         IEABRCX DEFINE",
                    ".* The HLASM GOFF option is needed to assemble this program"
            };

    private static final String[] hlasmEpilog =
            {
                    "         EJECT",
                    "ZISSTUBS CSECT ,",
                    "         END"
            };

    private static final String[] initCopyright =
            {
                    "/*",
                    "  This program and the accompanying materials are",
                    "  made available under the terms of the Eclipse Public License v2.0 which accompanies",
                    "  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html",
                    "",
                    "  SPDX-License-Identifier: EPL-2.0",
                    "",
                    "  Copyright Contributors to the Zowe Project.",
                    "*/",
            };

    private void writeLines(PrintStream out, String[] lines) {
        for (String line : lines) {
            out.printf("%s\n", line);
        }
    }

    private BufferedReader openEbcdic(String filename) throws IOException {
        return new BufferedReader(new InputStreamReader(new FileInputStream(filename)));
    }

    private void generateCode(PrintStream out, boolean generateASM, DispatchMode dispatchMode) throws IOException {

        BufferedReader reader = openEbcdic(hFileName);
        if (generateASM) {
            writeLines(out, hlasmProlog);
        } else {
            writeLines(out, initCopyright);
        }

        Set<Integer> indices = new HashSet<>();
        Set<String> symbols = new HashSet<>();
        Set<String> functions = new HashSet<>();
        int maxStubNum = 0;
        Pattern stubPattern = Pattern.compile("^#define\\s+ZIS_STUB_(\\S+)\\s+([0-9]{1,8})\\s*/\\*\\s*(\\S+)(\\s+mapped)?\\s*\\*/\\s*");
        Pattern maxStubCountPattern = Pattern.compile("^#define\\s+MAX_ZIS_STUBS\\s+([0-9]+)\\s*");

        String line;
        while ((line = reader.readLine()) != null) {

            Matcher stubMatcher = stubPattern.matcher(line);
            if (stubMatcher.matches()) {
                String symbol = stubMatcher.group(1);
                int index = Integer.parseInt(stubMatcher.group(2));
                String functionName = stubMatcher.group(3);
                boolean isMapped = stubMatcher.group(4) != null;

                if (!symbols.add(symbol)) {
                    throw new RuntimeException("Error: duplicate symbol " + symbol);
                }
                if (!functions.add(functionName)) {
                    throw new RuntimeException("Error: duplicate function name " + functionName);
                }
                if (index + 1 > maxStubNum) {
                    throw new RuntimeException("Error: MAX_ZIS_STUBS " + maxStubNum + " is too low for index " + index);
                }
                if (!indices.add(index)) {
                    throw new RuntimeException("Error: duplicate index " + index);
                }

                if (generateASM) {
                    out.printf("         ENTRY %s\n", symbol);
                    if (!isMapped) {
                        out.printf("%-8.8s ALIAS C'%s'\n", symbol, functionName);
                    }
                    switch (dispatchMode) {
                        case ZVTE: {
                            out.printf("%-8.8s LLGT 15,16(0,0)       CVT\n", symbol);
                            out.print("         LLGT 15,X'8C'(,15)    ECVT\n");
                            out.print("         LLGT 15,X'CC'(,15)    CSRCTABL\n");
                            out.print("         LLGT 15,X'23C'(,15)   ZVT\n");
                            out.print("         LLGT 15,X'9C'(,15)    FIRST ZVTE (the ZIS)\n");
                            out.print("         LG   15,X'80'(,15)    ZIS STUB VECTOR\n");
                            out.print("         LA   15,X'28'(,15)    Slots address\n");
                            break;
                        }
                        case R12: {
                            out.printf("%-8.8s LLGT 15,X'2A8'(,12)   Get the (R)LE CAA's RLETask\n", symbol);
                            out.print("         LLGT 15,X'38'(,15)   Get the RLETasks RLEAnchor\n");
                            out.print("         LG   15,X'18'(,15)   Get the Stub Vector \n");
                            out.print("         LA   15,X'28'(,15)   Slots address\n");
                            break;
                        }
                        default: {
                            throw new IllegalStateException("unknown dispatch mode " + dispatchMode);
                        }
                    }
                    out.printf("         LG   15,X'%02X'(,15)    %s\n", index * 8, symbol);
                    out.print("         BR   15\n");
                } else {
                    out.printf("    stubVector[ZIS_STUB_%-8.8s] = (void*)%s;\n", symbol, functionName);
                }
                continue;
            }

            Matcher maxStubCountMatcher = maxStubCountPattern.matcher(line);
            if (maxStubCountMatcher.matches()) {
                maxStubNum = Integer.parseInt(maxStubCountMatcher.group(1));
            }

        }
        if (generateASM) {
            writeLines(out, hlasmEpilog);
        } else {
            writeLines(out, initCopyright);
        }

        reader.close();
    }

    private static void printHelp(String reason) {
        if (reason != null) {
            System.out.println(reason);
        }
        System.out.println();
        System.out.println("Usage: java com.zossteam.zis.ZISStubGenerator <command> <stub_header> [dispatch_mode]");
        System.out.println();
        System.out.println("Options:");
        System.out.println("  command - the utility command (init, asm)");
        System.out.println("    asm - generate a stub file");
        System.out.println("    init - generate the initialization code for the base plugin");
        System.out.println("  sub_header - the header with your stub definitions (usually zisstubs.h)");
        System.out.println("  dispatch_mode - the dispatch mode of the stub routines (only used with the asm command)");
        System.out.println("    r12 - the stub vector is based off of GPR12 (default)");
        System.out.println("    zvte - the stub vector is based off of the ZVTE");
        System.out.println();
    }

    public static void main(String[] args) throws Exception {

        if (args.length == 0 || args[0].equals("-h") || args[0].equals("--help")) {
            printHelp(null);
            return;
        }

        if (args.length < 2) {
            printHelp("Error: too few arguments.");
            return;
        }

        String command = args[0];
        String hFileName = args[1];
        ZISStubGenerator generator = new ZISStubGenerator(hFileName);
        if (command.equalsIgnoreCase("asm")) {
            DispatchMode dispatchMode = DispatchMode.R12;
            if (args.length >= 3) {
                String modeString = args[2];
                if (modeString.equalsIgnoreCase("zvte")) {
                    dispatchMode = DispatchMode.ZVTE;
                } else if (!modeString.equalsIgnoreCase("r12")) {
                    printHelp("Error: unknown dispatch mode " + modeString + ".");
                    return;
                }
            }
            generator.generateCode(System.out, true, dispatchMode);
        } else if (command.equalsIgnoreCase("init")) {
            generator.generateCode(System.out, false, DispatchMode.R12);
        } else {
            printHelp("Error: unknown command " + command + ".");
        }

    }

}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
 */
