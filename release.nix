let pkgs = import <nixpkgs> {
    overlays = [
        (final: prev: {
            drc = final.lib.makeScope final.newScope (self: let
                host = self.drc.stdenv.hostPlatform;
                buildPackages = final.buildPackages;
                libc = self.drc.stdenv.cc.libc;
                ccLib = final.lib.getLib self.drc.stdenv.cc.cc;
                includeSep = if host.isWindows then ";" else ":";
                includePath = final.lib.concatStringsSep includeSep [
                    "${final.lib.getDev libc}/include"
                    "${final.lib.getDev final.libffi}/include"
                ];
                winePath = final.lib.concatStringsSep ";" [
                    "${final.lib.getLib final.libffi}/bin"
                    "${ccLib}/${host.config}/lib"
                    "${final.threads.package}/bin"
                ];
                libraryPath = final.lib.makeLibraryPath [ final.libffi ccLib ];
            in {
                drc = self.callPackage ./package.nix {};
                tests =
                    let
                        canExecute = self.drc.stdenv.buildPlatform.canExecute host;
                        emulator = host.emulator buildPackages;
                    in self.drc.overrideAttrs (old: {
                        doCheck = true;
                        # Schedule an explicit emulatedCheckPhase for foreign targets
                        # as checkPhase is disabled for foreign targets.
                        preInstallPhases = (old.preInstallPhases or []) ++ final.lib.optional (!canExecute) "emulatedCheckPhase";
                        # use DRC_INCLUDE_PATH to tell it about the cross include headers.
                        emulatedCheckPhase = ''
                            runHook preCheck
                            export DRC_INCLUDE_PATH="${includePath}"
                            ${final.lib.optionalString host.isWindows ''
                                export WINEPREFIX="$TMPDIR/wine"
                                export WINEDEBUG=-all
                                export WINEPATH="${winePath};${libc}/bin;${libc}/lib"
                            ''}
                            ${final.lib.optionalString host.isLinux ''
                                export DRC_INCLUDE_PATH="$DRC_INCLUDE_PATH:${final.linuxHeaders}/include"
                                export QEMU_LD_PREFIX="${libc}"
                                export QEMU_SET_ENV="LD_LIBRARY_PATH=${libraryPath}"
                            ''}
                            ./build tests --runner="${emulator}"
                            runHook postCheck
                        '';
                    });
                devShell = let
                    cross = !self.drc.stdenv.buildPlatform.canExecute host;
                    canRun = host.emulatorAvailable buildPackages;
                    runner = if cross && canRun then host.emulator buildPackages else "";
                in final.mkShell {
                    inputsFrom = [ self.drc ];
                    depsBuildBuild = [ final.buildPackages.stdenv.cc ];
                    nativeBuildInputs =
                        final.lib.optional (cross && host.isWindows)
                            (buildPackages.winePackagesFor "wine${toString host.parsed.cpu.bits}").minimal
                        ++ final.lib.optional (cross && host.isLinux && canRun)
                            buildPackages.qemu-user;
                    NIX_ENFORCE_PURITY = 0; # Allow the build helper to rebuild itself inside the checkout.
                    shellHook =
                        let
                            config = host.config;
                        in ''
                            export BUILD="${config}"-build
                            (
                                set -ex
                                if [ ! -e "$BUILD" ]; then
                                    "$CC_FOR_BUILD" build.c -o "$BUILD" -DDEFAULT_BUILD_COMPILER="\"$CC_FOR_BUILD\""
                                fi
                                ./"$BUILD" nothing \
                                    --host-cc="$CC" \
                                    --build-cc="$CC_FOR_BUILD" \
                                    --builddir=${config}-builddir \
                                    --cc-handles-target \
                                    --os=${self.drc.hostOS} \
                                    --arch=${self.drc.hostArch} \
                                    --bits=${self.drc.hostBits} \
                                    --runner="${runner}"
                            )
                            alias mk=./"$BUILD"
                            ${final.lib.optionalString (cross && (host.isLinux || host.isWindows)) ''
                                export DRC_INCLUDE_PATH="${includePath}''${DRC_INCLUDE_PATH:+${includeSep}$DRC_INCLUDE_PATH}"
                            ''}
                            ${final.lib.optionalString (cross && host.isWindows) ''
                                export WINEPREFIX="''${WINEPREFIX:-$PWD/.cache/wine-${config}}"
                                mkdir -p "$WINEPREFIX"
                                export WINEDEBUG="''${WINEDEBUG:--all}"
                                export WINEPATH="${winePath}''${WINEPATH:+;$WINEPATH}"
                            ''}
                            ${final.lib.optionalString (cross && host.isLinux && canRun) ''
                                export DRC_INCLUDE_PATH="$DRC_INCLUDE_PATH:${final.linuxHeaders}/include"
                                export QEMU_LD_PREFIX="${libc}"
                                export QEMU_SET_ENV="LD_LIBRARY_PATH=${libraryPath}''${QEMU_SET_ENV:+,$QEMU_SET_ENV}"
                            ''}
                        '';
                };
            });
        })
    ];
};
in {
    inherit(pkgs.drc) drc devShell;
    aarch64-linux-gnu = pkgs.pkgsCross.aarch64-multiplatform.drc;
    x86_64-linux-gnu = pkgs.pkgsCross.gnu64.drc;
    mingw = pkgs.pkgsCross.mingw-ucrt-x86_64.drc;
    tests = {
        native = pkgs.drc.tests;
        aarch64-linux-gnu = pkgs.pkgsCross.aarch64-multiplatform.drc.tests;
        x86_64-linux-gnu = pkgs.pkgsCross.gnu64.drc.tests;
        mingw = pkgs.pkgsCross.mingw-ucrt-x86_64.drc.tests;
    };
}
