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
 *  ======== PackageBuildHelp.xdc ========
 */
package ti.tools.build;

import xdc.bld.Executable;
import xdc.bld.Library;


/*!
 *  ======== PackageBuildHelp ========
 *  Common package build support
 *
 *  This module is a helper module for your package build script
 *  (`package.bld`). It provides a high-level interface for defining
 *  your libraries and executables, and provides functions to generate
 *  your package interface (makefiles).
 */

metaonly module PackageBuildHelp {

    /*!
     *  ======== LibAttrs ========
     *  Attributes for a library instances
     *
     *  An array of this type is passed to `PackageBuildHelp.makeLibraries`.
     *  Each element in the array describes the build properties for the
     *  library. Multiple library instances may be produced from a single
     *  `LibAttrs` element. For example, a library instance may be build for
     *  different profiles and/or different targets.
     *
     *  The `icw` array is used to limit which targets will be use for
     *  building the library. For example, if you have a library which
     *  should only be built for ARM, but you know the build model has a
     *  target for DSP, then set the `icw` array as follows to build only
     *  for the ARM targets and exclude the DSP targets.
     *
     *  @p(code)
     *  icw: [ "v5T" ]
     *  @p(text)
     *  Note that all ARM targets which accept v5T as an input isa will
     *  build the library using their output isa. For example, if the build
     *  model contains both GCArmv5T (isa=v5T) and GCArmv6 (isa=v6) targets,
     *  then both targets will build this library using their respective
     *  isa. However, if the build model contains the C64P (isa=64P) target,
     *  it will not be used to build this library because it cannot accept
     *  the v5T isa.
     *
     *  @a(See Also)
     *  {@link #makeLibraries} -- for additional examples
     *
     *  @field(name) The name of the library with an optional
     *  directory prefix.
     *
     *  @field(sources) An array of source files to be compiled
     *  for the library.
     *
     *  @field(icw) An array of strings listing isa's that a given target
     *  must be able to accept. Use this property to limit which targets
     *  will be used to build this library. See Details below.
     *
     *  @field(libAttrs) An xdc.bld.Library.Attrs object used to specify
     *  optional attributes of the library instance.
     */
    struct LibAttrs {
        String          name;           /*! library name */
        String          sources[];      /*! source files to build */
        String          icw[];          /*! array of input isa's */
        Library.Attrs   libAttrs;       /*! library attributes */
    };

    /*!
     *  ======== ProgAttrs ========
     *  Attributes for an executable instance
     *
     *  An array of this type is passed to `PackageBuildHelp.makeExecutables`.
     *  Each element in the array describes the build properties for the
     *  executable. Multiple executable instances may be produced from a
     *  single `ProgAttrs` element. For example, an executable instance may
     *  be build for different profiles, targets, and platforms.
     *
     *  The `icw` array is used to limit which targets will be use for
     *  building the executable. For example, if you have an executable
     *  which should only be built for ARM, but you know the build model
     *  has a target for DSP, then set the `icw` array as follows to build
     *  only for the ARM targets and exclude the DSP targets.
     *
     *  @p(code)
     *  icw: [ "v5T" ]
     *  @p(text)
     *  Note that all ARM targets which accept v5T as an input isa will
     *  build the executable using their output isa. For example, if the build
     *  model contains both GCArmv5T (isa=v5T) and GCArmv6 (isa=v6) targets,
     *  then both targets will build this executable using their respective
     *  isa. However, if the build model contains the C64P (isa=64P) target,
     *  it will not be used to build this executable because it cannot accept
     *  the v5T isa.
     *
     *  @p(text)
     *  Note: this property is more useful to building libraries than
     *  executables. The `isas` property is usually used when building
     *  executables.
     *
     *  @p(text)
     *  The `isas` array is used to control the executable's isa. Only those
     *  targets which have an output isa that matches any entry in this array
     *  will be used to build the executable. For example, if you have a
     *  program which should be built for ARM v5T or ARM v6 but not for ARM
     *  v7A, but you know the build model has targets for all three, then
     *  set the `isas` array as follows to get the desired output.
     *
     *  @p(code)
     *  isas: [ "v5T", "v6" ]
     *  @p(text)
     *  Note: although the GCArmv7A target accepts v5T and v6 isa as input,
     *  it will not participate in the build because it outputs v7A isa which
     *  not specified in the `isas` array.
     *
     *  @a(See Also)
     *  {@link #makeExecutables} -- for additional examples
     *
     *  @field(name) The name of the executable with an optional
     *  directory prefix.
     *
     *  @field(sources) An array of strings listing the source files
     *  to be compiled for the executables.
     *
     *  @field(platforms) An array of platform (and/or platform instance)
     *  names which are supported by this program. When this property is
     *  specified, the executable is built only for those platforms listed
     *  by this property, otherwise, the executable is built for all
     *  platforms specified in the target's platforms array property.
     *
     *  @field(icw) An array of strings listing isa's which are compatible
     *  for this executable. Use this property to limit which isa's the
     *  program will be built for.
     *
     *  @field(isas) An array of isa's the program should be built for.
     *
     *  @field(execAttrs) An xdc.bld.Executable.Attrs object used to specify
     *  optional attributes of the executable instance.
     */
    struct ProgAttrs {
        String              name;           /*! executable name */
        String              sources[];      /*! source files to build */
        String              platforms[];    /*! array of platform names */
        String              icw[];          /*! array of input isa's */
        String              isas[];         /*! array of output isa's */
        Executable.Attrs    execAttrs;      /*! executble attributes */
    };

    /*!
     *  ======== fileArray ========
     *  Record all source files and config scripts in given array
     *
     *  When this config param is assigned to a string array, each call
     *  to `PackageBuildHelp.makeExecutables` and
     *  `PackageBuildHelp.makeLibraries` will append all given source files
     *  and config scripts to the array.
     */
    config Any fileArray;

    /*!
     *  ======== skipExec ========
     *  Skip given program if function returns true
     *
     *  If you don't want your package to build a given executable based
     *  on certain build attributes, set this config param to a function
     *  defined in your package.bld script which returns true for any
     *  executable you do not want to build.
     *
     *  This function can also be used to modify the program object just
     *  before building it. The following example show a function which
     *  adds a source file depending on the target.os property. It always
     *  returns false, because we don't want to skip the executable.
     *
     *  @p(code)
     *  PackageBuildHelp.skipExec = function(prog, targ, platName)
     *  {
     *      if (targ.os == undefined) {
     *          prog.sources.push("main_BIOS.c");
     *      }
     *      else {
     *          prog.sources.push("main_native.c");
     *      }
     *      return false;
     *  }
     *
     *  @b(ARGUMENTS)
     *  @p(dlist)
     *    - prog
     *      (`PackageBuildHelp.ProgAttrs`) An element from the `progAry`
     *      argument passed to the `PackageBuildHelp.makeExecutables`
     *      function.
     *    - targ
     *      (`xdc.bld.ITarget.Module`) The target used for building the
     *      current executable.
     *    - platName
     *      (`String`) The name of the platform used for the current
     *      executable.
     *  @p(text)
     */
    config Bool skipExec(ProgAttrs, xdc.bld.ITarget.Module, String);

    /*!
     *  ======== skipLib ========
     *  Skip given library if function returns true
     *
     *  If you don't want your package to build a given library based
     *  on certain build attributes, set this config param to a function
     *  defined in your package.bld script which returns true for any
     *  library you do not want to build.
     *
     *  This function can also be used to modify the library object just
     *  before building it. The following example show a function which
     *  adds a source file depending on the target.os property. It always
     *  returns false, because we don't want to skip the library.
     *
     *  @p(code)
     *  PackageBuildHelp.skipLib = function(lib, targ)
     *  {
     *      if (targ.os == undefined) {
     *          lib.sources.push("util_BIOS.c");
     *      }
     *      else {
     *          lib.sources.push("util_native.c");
     *      }
     *      return false;
     *  }
     *
     *  @p(text)
     *  This example show a function which builds the library only if the
     *  library name contains the string "syslink" and the target's
     *  os (Operating System) property matches "Linux".
     *
     *  @p(code)
     *  PackageBuildHelp.skipLib = function(lib, targ)
     *  {
     *      if (lib.name.match(/syslink/)) {
     *          return targ.os != "Linux" ? true : false;
     *      }
     *      else {
     *          return false;
     *      }
     *  }
     *
     *  @b(ARGUMENTS)
     *  @p(dlist)
     *    - lib
     *      (`PackageBuildHelp.LibAttrs`) An element from the `libAry`
     *      argument passed to the `PackageBuildHelp.makeLibraries` function.
     *    - targ
     *      (`xdc.bld.ITarget.Module`) The target used for building the
     *      current library.
     *  @p(text)
     */
    config Bool skipLib(LibAttrs, xdc.bld.ITarget.Module);

    /*!
     *  ======== usePlatformInstanceName ========
     *  Use platform instance name when constructing executable pathname
     *
     *  The executable pathname is constructed using the following
     *  elements: 'bin', platform name, directory prefix used in
     *  program name, profile, and the executable basename. These
     *  elements are combined in the following pattern:
     *
     *  bin/platform/prefix/profile/name
     *
     *  This config param controls the platform part of the pathname
     *  when using platform instances. When set to true, the platform
     *  instance name is used, otherwise, the platform base name is used.
     *  When using the default platform instance the platform base name
     *  is always used.
     */
    config Bool usePlatformInstanceName = false;

    /*!
     *  ======== skipPlatform ========
     *  Skip given platform if function returns true
     *
     *  If you do not want your package to build for a particular
     *  platform, set this config param to a function defined in your
     *  `package.bld` script which returns true for any platform you wish
     *  to skip.
     *
     *  The following example shows a function which skips any platform
     *  named `ti.platforms.evm3530`.
     *
     *  @p(code)
     *  PackageBuildHelp.skipPlatform = function(platName, targ)
     *  {
     *      return platName.match(/ti.platforms.evm3530/) ? true : false;
     *  }
     *
     *  @b(ARGUMENTS)
     *  @p(dlist)
     *    - platName
     *      (`String`) The name of the platform used for the current
     *      executable.
     *    - targ
     *      (`xdc.bld.ITarget.Module`) The target used for building the
     *      current executable.
     *  @p(text)
     */
    config Bool skipPlatform(String, xdc.bld.ITarget.Module);

    /*!
     *  ======== skipTarget ========
     *  Skip given target if function returns true
     *
     *  If you do not want your package to build for a particular
     *  target, set this config param to a function defined in your
     *  package.bld script which returns true for any target you wish
     *  to skip.
     *
     *  The following example shows a function which skips any target
     *  named `UCArm9`.
     *
     *  @p(code)
     *  PackageBuildHelp.skipTarget = function(targ)
     *  {
     *      return targ.name.match(/UCArm9/) ? true : false;
     *  }
     *
     *  @b(ARGUMENTS)
     *  @p(dlist)
     *    - targ
     *      (`xdc.bld.ITarget.Module`) The target used for building the
     *      current library or executable.
     *  @p(text)
     */
    config Bool skipTarget(xdc.bld.ITarget.Module);

    /*!
     *  ======== makeExecutables ========
     *  Add executables to the build model
     *
     *  This function iterates over the given `progAry` and adds each element
     *  to the build model, possibly multiple times to cover all combinations
     *  of platforms, targets, and profiles.
     *
     *  The following code adds a program called `program` to the build model
     *  for all combinations of platforms, targets, and profiles in the build
     *  model.
     *
     *  @p(code)
     *  var PackageBuildHelp = xdc.useModule('ti.tools.build.PackageBuildHelp');
     *  var progArray = new Array();
     *
     *  progArray.push(
     *      new PackageBuildHelp.ProgAttrs({
     *          name: "program",
     *          sources: [ "main.c", "core.c" ],
     *          execAttrs: {
     *              cfgScript: "server.cfg"
     *          }
     *      })
     *  );
     *
     *  PackageBuildHelp.makeExecutables(progArray, arguments);
     *
     *  @p(text)
     *  You can pass an array of profiles as the third (optional)
     *  argument to `PackageBuildHelp.makeExecutables`. This is useful
     *  for limiting the number of executables to build when the target
     *  defines additional profiles.
     *
     *  @p(code)
     *  PackageBuildHelp.makeExecutables(
     *      progArray, arguments, [ "debug", "release" ]);
     *
     *  @p(text)
     *  You may limit the build to a single profile using the XDCARGS
     *  command line option, provided the target supports the given profile.
     *
     *  @p(code)
     *  xdc -r XDCARGS="profile=coverage" all
     *
     *  @p(text)
     *  You may limit the build to a single platform using the XDCARGS
     *  command line option, provided the target supports the given platform.
     *
     *  @p(code)
     *  xdc -r XDCARGS="platform=ti.platforms.evm3530" all
     *
     *  @p(text)
     *  The profile and platform arguments may be combined in XDCARGS by
     *  separating them with white space.
     *
     *  @p(code)
     *  XDCARGS="profile=coverage platform=ti.platforms.evm3530"
     *
     *  @p(text)
     *  When specifying platforms, isas, or both it is important to ensure
     *  that all possible combinations are valid. In other words, each
     *  platform specified in the `platforms` array must have a core which
     *  is either capable of executing every isa specified in the `isas`
     *  array or, if `isas` is not specified, capable of executing code
     *  generated by every target in the build model. To avoid invalid
     *  combinations, specify either or both. Use multiple `ProgAttrs`
     *  instances to cover all desired combinations.
     *
     *  The following example builds a program for the DSP on an
     *  OMAP3530 device and the same program for the VIDEO-M3 on a DM8168
     *  device.
     *
     *  @p(code)
     *  var BuildHelp = xdc.useModule('ti.tools.build.PackageBuildHelp');
     *  var progArray = new Array();
     *
     *  progArray.push(
     *      new BuildHelp.ProgAttrs({
     *          name: "server",
     *          platforms: [ "ti.platforms.evm3530" ],
     *          isas: [ "64P" ],
     *          sources: [ "server.c", "utils.c" ]
     *      })
     *  );
     *
     *  progArray.push(
     *      new BuildHelp.ProgAttrs({
     *          name: "server",
     *          platforms: [ "ti.platforms.evmDM8168" ],
     *          isas: [ "v7M" ],
     *          sources: [ "server.c", "utils.c" ]
     *      })
     *  );
     *
     *  BuildHelp.makeExecutables(progArray, arguments);
     *  @p(text)
     *  Note that combining these two entries into one would generate an
     *  invalid combination. For example, there is no core on the OMAP3530
     *  capable of running the v7M isa.
     *
     *  @param(progAry) An array of program attributes, one for each
     *  executable to build.
     *
     *  @param(arguments) The global arguments object passed to the build
     *  script which contains the arguments assigned to the XDCARGS
     *  environment variable used by the xdc command.
     *
     *  @param(profiles) Optional. (`String[]`) Array of profile names to
     *  build provided the target supports the profile. If not supported
     *  by the target, the profile is ignored. When omitted, all profiles
     *  defined in the target will be used. If a profile is specified in
     *  XDCARGS as described above, this parameter is ignored.
     */
    Void makeExecutables(
            ProgAttrs   progAry[],
            Any         arguments,
            Any         profiles = undefined
        );

    /*!
     *  ======== makeLibraries ========
     *  Add libraries to the build model
     *
     *  This function iterates over the given `libAry` and adds each element
     *  to the build model, possibly multiple times to cover all combinations
     *  of targets and profiles.
     *
     *  The following code adds a library called `rcm` to the build model
     *  for all combinations of targets and profiles in the build model.
     *
     *  @p(code)
     *  var PackageBuildHelp = xdc.useModule('ti.tools.build.PackageBuildHelp');
     *  var libArray = new Array();
     *
     *  libArray.push(
     *      new PackageBuildHelp.LibAttrs({
     *          name: "rcm",
     *          sources: [ "rcm.c" ],
     *      })
     *  );
     *
     *  PackageBuildHelp.makeLibraries(libArray, arguments);
     *
     *  @p(text)
     *  You can pass an array of profiles as the third (optional)
     *  argument to `PackageBuildHelp.makelibraries`. This is useful
     *  for limiting the number of libraries to build when the target
     *  defines additional profiles.
     *
     *  @p(code)
     *  PackageBuildHelp.makeLibraries(
     *      libArray, arguments, [ "debug", "release" ]);
     *
     *  @p(text)
     *  You may limit the build to a single profile using the XDCARGS
     *  command line option, provided the target supports the given profile.
     *
     *  @p(code)
     *  xdc -r XDCARGS="profile=debug" all
     *  @p(text)
     *
     *  @a(Examples)
     *  The following example builds the library only for ARM targets and
     *  only for the debug and release profiles.
     *
     *  @p(code)
     *  var BuildHelp = xdc.useModule('ti.tools.build.PackageBuildHelp');
     *  var libArray = new Array();
     *
     *  libArray.push(
     *      new BuildHelp.LibAttrs({
     *          name: "host",
     *          sources: [ "config_host.c" ],
     *          icw: [ "v5T" ]
     *      })
     *   );
     *
     *  BuildHelp.makeLibraries(libArray, arguments, ["debug","release"]);
     *  @p(text)
     *  If the build model contained the GCArmv5T, GCArmv6, and C64P targets,
     *  the following libraries would be produced. Note, the C64P target is
     *  excluded.
     *
     *  @p(code)
     *  lib/debug/host.av5T
     *  lib/debug/host.av6
     *  lib/release/host.av5T
     *  lib/release/host.av6
     *  @p(text)
     *
     *  @param(libAry) An array of library attributes, one for each
     *  library to build.
     *
     *  @param(arguments) The global arguments object passed to the build
     *  script which contains the arguments assigned to the XDCARGS
     *  environment variable used by the xdc command.
     *
     *  @param(profiles) Optional. (`String[]`) Array of profile names to
     *  build provided the target supports the profile. If not supported
     *  by the target, the profile is ignored. When omitted, all profiles
     *  defined in the target will be used. If a profile is specified in
     *  XDCARGS as described above, this parameter is ignored.
     */
    Void makeLibraries(
            LibAttrs    libAry[],
            Any         arguments,
            Any         profiles = undefined
        );
}
/*
 *  @(#) ti.tools.build; 1, 0, 0,23; 1-14-2011 17:49:40; /db/atree/library/trees/osal/osal.git/src/ osal-d05
 */
