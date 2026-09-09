let pkgs = import <nixpkgs> {
    overlays = [
        (final: prev: {
            drc = final.lib.makeScope final.newScope (self: {
                drc = self.callPackage ./package.nix {};
                devShell = final.mkShell {
                    inputsFrom = [ self.drc ];
                    depsBuildBuild = [ final.buildPackages.stdenv.cc ];
                    # Allow the build helper to rebuild itself inside the checkout.
                    NIX_ENFORCE_PURITY = 0;
                    shellHook = 
                        let 
                            config = self.drc.stdenv.hostPlatform.config; 
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
                                    --bits=${self.drc.hostBits}
                            )
                            alias mk=./"$BUILD"
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
}
