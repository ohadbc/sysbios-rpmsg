/*
 * Copyright (c) 2011, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 *  ======== updateFileArray ========
 *  Scan the input array looking for source files and config scripts.
 *  Add these (only once) to the output array.
 *
 *  The arguments can be references to the following types:
 *
 *  JavaScript.Array                outputArray
 *  xdc.String[]                    outputArray
 *  PackageBuildHelp.LibAttrs[]     inputArray
 *  PackageBuildHelp.ProgAttrs[]    inputArray
 */
function updateFileArray(outputArray, inputArray)
{
    var tmpAry = new Array();

    /* function to add filename only once to array */
    tmpAry.addOnce = function(filename)
    {
        for (var f = 0; f < this.length; f++) {
            if (this[f] == filename) return;
        }
        this.push(filename);
    }

    /* scan input array for source files and config scripts */
    for (var i = 0; i < inputArray.length; i++) {
        for (var f = 0; f < inputArray[i].sources.length; f++) {
            tmpAry.addOnce(inputArray[i].sources[f]);
        }
        if (("execAttrs" in inputArray[i]) &&
            ("cfgScript" in inputArray[i].execAttrs) &&
            inputArray[i].execAttrs.cfgScript != undefined) {
            tmpAry.addOnce(inputArray[i].execAttrs.cfgScript);
        }
    }

    /* sort the array */
    tmpAry.sort();

    /* append files to output array */
    for (var f = 0; f < tmpAry.length; f++) {
        if ("$add" in outputArray) {
            outputArray.$add(tmpAry[f]);
        }
        else {
            outputArray.push(tmpAry[f]);
        }
    }
}


/*
 *  ======== makeExecutables ========
 */
function makeExecutables(progArray, xdcArgs, profileAry)
{
    var Pkg = xdc.useModule('xdc.bld.PackageContents');

    // check if profile specified in XDCARGS
    // XDCARGS="... profile=debug ..."
    var cmdlProf = (" " + xdcArgs.join(" ") + " ").match(/ profile=([^ ]+) /);
    cmdlProf = cmdlProf != null ? cmdlProf[1] : null;

    // check if platform specified in XDCARGS
    // XDCARGS="... platform=ti.platforms.evm3530 ..."
    var cmdlPlat = (" " + xdcArgs.join(" ") + " ").match(/ platform=([^ ]+) /);
    cmdlPlat = cmdlPlat != null ? cmdlPlat[1] : null;

    // check if instance specified in XDCARGS
    // XDCARGS="... instance=ti.platforms.evm3530:myInstance ..."
    var cmdlInst = (" " + xdcArgs.join(" ") + " ").match(/ instance=([^ ]+) /);
    cmdlInst = cmdlInst != null ? cmdlInst[1] : null;

    if ((cmdlPlat != null) && (cmdlInst != null)) {
        throw Error("cannot specify both platform and instance in XDCARGS, "
            + "use only one at a time.");
    }

    // if fileArray has been set, add source files and config scripts
    if (this.fileArray != undefined) {
        updateFileArray(this.fileArray, progArray);
    }

    // ==== loop over array of programs ====
    for (var i = 0; i < progArray.length; i++) {
        var prog = progArray[i];

        // ==== loop over all targets in build array ====
        for (var j = 0; j < Build.targets.length; j++) {
            var targ = Build.targets[j];

            // invoke user supplied target predicate function
            if ((this.skipTarget !== undefined) && this.skipTarget(targ)) {
                continue;
            }

            // skip target if not compatible with source code
            if (prog.icw.length > 0) {
                var skipTarget = true;
                var targIsaChain = "/" + targ.getISAChain().join("/") + "/";
                for (var k = 0; k < prog.icw.length; k++) {
                    if (targIsaChain.match("/" + prog.icw[k] + "/")) {
                        skipTarget = false;
                        break;
                    }
                }
                if (skipTarget) continue;
            }

            // skip target if it does not generate code for the given isa
            if (prog.isas.length > 0) {
                var skipTarget = true;
                var list = "/";
                for (var t = 0; t < prog.isas.length; t++) {
                    list += prog.isas[t] + "/";
                }
                if (list.match("/" + targ.isa + "/")) {
                    skipTarget = false;
                }
                if (skipTarget) continue;
            }

            // use platforms from the program or the target
            var platAry = (prog.platforms.length > 0)
                ? prog.platforms : targ.platforms;

            // ==== loop over all platform ====
            for (var k = 0; k < platAry.length; k++) {
                var platName = platAry[k];

                // invoke user supplied platform predicate function
                if ((this.skipPlatform !== undefined) &&
                    this.skipPlatform(platName, targ)) {
                    continue;
                }

                // skip platform if different from that specified on cmd line
                if ((cmdlPlat != null) &&
                    (cmdlPlat != platName.match(/^[^:]+/)[0])) {
                    continue;
                }

                // skip instance if different from that specified on cmd line
                if ((cmdlInst != null) && (cmdlInst != platName)) {
                    continue;
                }

                // the program specified platform must be in the target array
                if (prog.platforms.length > 0) {
                    var skipPlatform = true;
                    var progPlat = platName.match(/^[^:]+/)[0];
                    for (var tpi = 0; tpi < targ.platforms.length; tpi++) {
                        var targPlat = targ.platforms[tpi].match(/^[^:]+/)[0];
                        if (targPlat == progPlat) {
                            skipPlatform = false;
                            break;
                        }
                    }
                    if (skipPlatform) continue;
                }

                // ==== loop over all profiles in target ====
                for (var profile in targ.profiles) {

                    // skip profile if different than specified on command line
                    if (cmdlProf != null) {
                        if (cmdlProf != profile)  continue;
                    }

                    // skip target profile if not found in profile array
                    else if (profileAry != undefined) {
                        var skipProfile = true;
                        var profileList = "/" + profileAry.join("/") + "/";
                        if (profileList.match("/" + profile + "/")) {
                            skipProfile = false;
                        }
                        if (skipProfile) continue;
                    }

                    // invoke user supplied hook function
                    var bldProg = prog.$copy();
                    if ((this.skipExec !== undefined) &&
                        this.skipExec(bldProg, targ, platName)) {
                        continue;
                    }

                    // compute platform directory name
                    var platDir = "";
                    if (this.usePlatformInstanceName) {
                        // replace all ':' and '.' with '_' in platform name
                        platDir = platName.replace(/\:|\./g, "_");
                    }
                    else {
                        // match only before ':', replace '.' with '_'
                        platDir =
                            platName.match(/^[^:]+/)[0].replace(/\./g, "_");
                    }

                    // name = bin/platform/extra/profile/name
                    var name = "bin/" + platDir + "/"
                        // insert profile as last directory (just before name)
                        + bldProg.name.replace(/([^\/]+)$/, profile+"/"+"$1");

                    // must set profile explicitly
                    bldProg.execAttrs.profile = profile;

                    // build the program
                    var pgm = Pkg.addExecutable(name, targ, platName,
                        bldProg.execAttrs);

                    /* add the source files */
                    pgm.addObjects(bldProg.sources);
                }
            }
        }
    }
}

/*
 *  ======== makeLibraries ========
 */
function makeLibraries(libArray, xdcArgs, profileAry)
{
    var Pkg = xdc.useModule('xdc.bld.PackageContents');

    // check if profile specified in XDCARGS
    // XDCARGS="... profile=debug ..."
    var cmdlProf = (" " + xdcArgs.join(" ") + " ").match(/ profile=([^ ]+) /);
    cmdlProf = cmdlProf != null ? cmdlProf[1] : null;

    // if fileArray has been set, add source files
    if (this.fileArray != undefined) {
        updateFileArray(this.fileArray, libArray);
    }

    // ==== loop over array of libraries ====
    for (var i = 0; i < libArray.length; i++) {
        var lib = libArray[i];

        // ==== loop over all targets in build array ====
        for (var j = 0; j < Build.targets.length; j++) {
            var targ = Build.targets[j];

            // invoke user supplied target predicate function
            if ((this.skipTarget !== undefined) && this.skipTarget(targ)) {
                continue;
            }

            // skip target if not compatible with source code
            if (lib.icw.length > 0) {
                var skipTarget = true;
                var targIsaChain = "/" + targ.getISAChain().join("/") + "/";
                for (var k = 0; k < lib.icw.length; k++) {
                    if (targIsaChain.match("/" + lib.icw[k] + "/")) {
                        skipTarget = false;
                        break;
                    }
                }
                if (skipTarget) continue;
            }

            // ==== loop over all profiles in target ====
            for (var profile in targ.profiles) {

                // skip profile if different than specified on command line
                if (cmdlProf != null) {
                    if (cmdlProf != profile)  continue;
                }

                // skip target profile if not found in profile array
                else if (profileAry != undefined) {
                    var skipProfile = true;
                    var profileList = "/" + profileAry.join("/") + "/";
                    if (profileList.match("/" + profile + "/")) {
                        skipProfile = false;
                    }
                    if (skipProfile) continue;
                }

                // invoke user supplied hook function
                var bldLib = lib.$copy();
                if ((this.skipLib !== undefined) &&
                    this.skipLib(bldLib, targ)) {
                    continue;
                }

                // name = lib/profile/name.a+suffix
                var name = "lib/" + profile + "/" + bldLib.name;

                // pass along library attributes specified in library array
                var libAttrs = "libAttrs" in bldLib ? bldLib.libAttrs : {};

                // must set profile explicitly
                libAttrs.profile = profile;

                // build the library
                var library = Pkg.addLibrary(name, targ, libAttrs);

                /* add the source files */
                library.addObjects(bldLib.sources);
            }
        }
    }
}
/*
 *  @(#) ti.tools.build; 1, 0, 0,23; 1-14-2011 17:49:40; /db/atree/library/trees/osal/osal.git/src/ osal-d05
 */
