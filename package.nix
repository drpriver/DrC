{ stdenv, buildPackages, lib, libffi, }:
let
    host = stdenv.hostPlatform;
    hostOS = if host.isDarwin then "macos"
             else if host.isLinux then "linux"
             else if host.isWindows then "windows"
             else throw "Unsupported host OS: ${host.system}";
    hostArch = if host.isx86_64 then "x86"
               else if host.isAarch64 then "arm"
               else throw "Unsupported host architecture: ${host.system}";
    hostBits = if host.is64bit then "64bit"
               else if host.is32bit then "32bit"
               else throw "Unsupported host bitness: ${host.system}";
in stdenv.mkDerivation (self: {
    passthru = {
        inherit hostOS hostArch hostBits;
    };
    name = "drc";
    configurePhase = ''
        runHook preConfigure
        $CC_FOR_BUILD build.c -o build -DDEFAULT_BUILD_COMPILER="\"$CC_FOR_BUILD\""
        ./build nothing \
            --builddir=bin \
            --prefix=$out \
            --build-cc=$CC_FOR_BUILD \
            --host-cc=$CC \
            --cc-handles-target \
            --os=${hostOS} \
            --arch=${hostArch} \
            --bits=${hostBits} \
            --optimize=${lib.boolToString self.optimize} \
            --debug-symbols=${lib.boolToString self.debugInfo} \
            --sanitize=${lib.boolToString self.debugInfo}
        runHook postConfigure
    '';
    buildPhase = ''
        runHook preBuild
        ./build drc drcpp
        runHook postBuild
    '';
    checkPhase = ''
        runHook preCheck
        ./build tests
        runHook postCheck
    '';
    doCheck = false; # stdenv.buildPlatform.canExecute stdenv.hostPlatform;
    installPhase = ''
        runHook preInstall
        ./build install
        runHook postInstall
    '';
    strictDeps = true;
    env.NIX_CFLAGS_COMPILE = lib.optionalString host.isx86_64 "-march=x86-64-v2";
    src = lib.fileset.toSource {
        root = ./.;
        fileset = lib.fileset.unions [
            ./build.c
            ./Drp
            ./C
            ./Vendored
            ./cc.c
            ./cpp.c
            ./drc_test.c
            ./cc_repl_completion.h
            ./cpp_args.h
        ];
    };
    nativeCheckInputs = lib.optional self.doCheck buildPackages.libffi;
    depsBuildBuild = [ buildPackages.stdenv.cc ];
    buildInputs = [libffi];
    optimize = true;
    debugInfo = false;
    dontStrip = self.debugInfo;
    sanitize = false;
})
